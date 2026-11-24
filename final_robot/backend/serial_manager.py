"""
IIT Mandi Power Line Inspection Crawler Robot - Serial Communication Manager.
Handles threaded asynchronous USB serial comms with STM32 Nucleo-F446RE,
automatic port discovery, auto-reconnection, packet framing, and heartbeat dispatch.
"""

import glob
import json
import logging
import queue
import sys
import threading
import time
from typing import Any, Callable, Dict, List, Optional

try:
    import serial
    import serial.tools.list_ports
    SERIAL_AVAILABLE = True
except ImportError:
    SERIAL_AVAILABLE = False

from .config import config
from .telemetry_parser import TelemetryStore

logger = logging.getLogger("SerialManager")

class SerialManager:
    def __init__(self, telemetry_store: TelemetryStore):
        self.telemetry_store = telemetry_store
        self.serial_port: Optional[serial.Serial] = None
        self._is_running = False
        self._connected = False
        self._active_port_name: str = "None"
        
        # Thread synchronization
        self._tx_queue: queue.Queue = queue.Queue(maxsize=100)
        self._lock = threading.Lock()
        self._worker_thread: Optional[threading.Thread] = None
        self._heartbeat_thread: Optional[threading.Thread] = None

        # Callbacks for specific incoming messages
        self._ack_callbacks: Dict[str, Callable] = {}
        self._seq_counter = 0
        self._received_first_packet = False
        self._connected_time = 0.0
        self._warned_no_data = False

    @property
    def is_connected(self) -> bool:
        return self._connected and self.serial_port is not None and self.serial_port.is_open

    @property
    def port_name(self) -> str:
        return self._active_port_name

    def auto_detect_port(self) -> Optional[str]:
        """Scans system for STM32 ST-LINK Virtual COM Port or USB serial devices."""
        if not SERIAL_AVAILABLE:
            return None

        ports = list(serial.tools.list_ports.comports())
        if not ports:
            return None

        # 1. Prefer ST-LINK device
        for p in ports:
            desc = (p.description or "").lower()
            mfg = (p.manufacturer or "").lower()
            hwid = (p.hwid or "").lower()

            if "stlink" in desc or "st-link" in desc or "stmicroelectronics" in mfg:
                logger.info(f"Auto-detected ST-LINK port: {p.device} ({p.description})")
                return p.device
            if "0483:374b" in hwid or "0483:" in hwid:
                logger.info(f"Auto-detected ST-LINK by VID:PID: {p.device}")
                return p.device

        # 2. Prefer /dev/ttyACM* or /dev/ttyUSB* on Linux
        if sys.platform.startswith("linux"):
            acm_ports = glob.glob("/dev/ttyACM*")
            if acm_ports:
                return acm_ports[0]
            usb_ports = glob.glob("/dev/ttyUSB*")
            if usb_ports:
                return usb_ports[0]

        # 3. Fallback to first available port
        logger.info(f"Using first available serial port: {ports[0].device}")
        return ports[0].device

    def start(self):
        if self._is_running:
            return
        self._is_running = True
        self._worker_thread = threading.Thread(target=self._io_loop, name="SerialWorker", daemon=True)
        self._worker_thread.start()

        self._heartbeat_thread = threading.Thread(target=self._heartbeat_loop, name="SerialHeartbeat", daemon=True)
        self._heartbeat_thread.start()
        logger.info("Serial Manager background threads started.")

    def stop(self):
        self._is_running = False
        self._connected = False
        
        # Send emergency safe disarm before closing if port is open
        try:
            if self.serial_port and self.serial_port.is_open:
                self.serial_port.write(b'{"type":"crawler","speed":0}\n')
                self.serial_port.write(b'{"type":"gripper","motor":"A","speed":0}\n')
                self.serial_port.write(b'{"type":"gripper","motor":"B","speed":0}\n')
                self.serial_port.write(b'{"type":"arm","active":false}\n')
                self.serial_port.flush()
                self.serial_port.close()
        except Exception:
            pass

        self.telemetry_store.mark_disconnected()
        logger.info("Serial Manager stopped.")

    def send_command(self, cmd_dict: Dict[str, Any]) -> bool:
        """Enqueues a JSON command to be transmitted to the STM32."""
        if not self.is_connected:
            logger.debug(f"Cannot send command (disconnected): {cmd_dict}")
            return False

        try:
            line = json.dumps(cmd_dict, separators=(",", ":")) + "\n"
            self._tx_queue.put_nowait(line.encode("utf-8"))
            return True
        except queue.Full:
            logger.warning("Serial TX queue full, dropping command.")
            return False
        except Exception as e:
            logger.error(f"Error encoding command: {e}")
            return False

    def send_raw_line(self, line: str) -> bool:
        if not self.is_connected:
            return False
        if not line.endswith("\n"):
            line += "\n"
        try:
            self._tx_queue.put_nowait(line.encode("utf-8"))
            return True
        except queue.Full:
            return False

    def _attempt_connection(self) -> bool:
        if not SERIAL_AVAILABLE:
            logger.warning("pyserial is not installed in the environment.")
            return False

        target_port = config.serial.get("port", "auto")
        if target_port == "auto" or not target_port:
            detected = self.auto_detect_port()
            if not detected:
                return False
            target_port = detected

        baud = config.serial.get("baudrate", 115200)
        timeout = config.serial.get("timeout_sec", 0.1)

        try:
            logger.info(f"Opening serial connection to {target_port} at {baud} baud...")
            self.serial_port = serial.Serial(
                port=target_port,
                baudrate=baud,
                timeout=timeout,
                write_timeout=0.2
            )
            self._active_port_name = target_port
            self._connected = True
            self._connected_time = time.monotonic()
            self._received_first_packet = False
            self._warned_no_data = False
            time.sleep(0.1)
            # Flush existing buffers
            self.serial_port.reset_input_buffer()
            self.serial_port.reset_output_buffer()
            logger.info(f"Successfully connected to STM32 on {target_port}!")
            self._sync_initial_config()
            return True
        except Exception as e:
            logger.warning(f"Connection attempt to {target_port} failed: {e}. Note: If error is 'Access is denied', close the Arduino Serial Monitor or any other app using {target_port}!")
            self._connected = False
            self._active_port_name = "None"
            if self.serial_port:
                try:
                    self.serial_port.close()
                except Exception:
                    pass
                self.serial_port = None
            return False

    def _io_loop(self):
        reconnect_interval = config.serial.get("reconnect_interval_sec", 1.5)
        buffer = bytearray()

        while self._is_running:
            # 1. Handle connection & auto-reconnect
            if not self.is_connected:
                self.telemetry_store.mark_disconnected()
                if not self._attempt_connection():
                    time.sleep(reconnect_interval)
                    continue

            # 2. Write outgoing queued commands
            while not self._tx_queue.empty():
                try:
                    data = self._tx_queue.get_nowait()
                    with self._lock:
                        if self.serial_port and self.serial_port.is_open:
                            self.serial_port.write(data)
                            self.serial_port.flush()
                except queue.Empty:
                    break
                except Exception as e:
                    logger.warning(f"Serial write exception: {e}")
                    self._connected = False
                    break

            if not self._connected:
                continue

            # 3. Read incoming serial telemetry
            try:
                waiting = self.serial_port.in_waiting if (self.serial_port and self.serial_port.is_open) else 0
                if waiting > 0:
                    chunk = self.serial_port.read(waiting)
                    buffer.extend(chunk)

                    while b"\n" in buffer:
                        idx = buffer.find(b"\n")
                        line_bytes = buffer[:idx]
                        del buffer[:idx + 1]

                        try:
                            line_str = line_bytes.decode("utf-8", errors="replace").strip()
                            if line_str:
                                if not self._received_first_packet:
                                    self._received_first_packet = True
                                    logger.info(f"[SerialManager] Live telemetry streaming started! First packet: {line_str[:70]}...")
                                self.telemetry_store.parse_line(line_str)
                        except Exception as e:
                            logger.debug(f"Line parse error: {e}")
                else:
                    if not self._received_first_packet and (time.monotonic() - self._connected_time > 3.0) and not self._warned_no_data:
                        self._warned_no_data = True
                        logger.warning(
                            f"[SerialManager] Connected to {self._active_port_name}, but no telemetry received yet! "
                            "Please verify that STM32_Robot_Controller.ino has been uploaded to the Nucleo board in Arduino IDE."
                        )
                    time.sleep(0.005)  # 5ms rest when no bytes waiting
            except Exception as e:
                logger.warning(f"Serial read exception (disconnecting): {e}")
                self._connected = False
                try:
                    if self.serial_port:
                        self.serial_port.close()
                except Exception:
                    pass
                self.serial_port = None
                self.telemetry_store.mark_disconnected()
                time.sleep(1.0)

    def _heartbeat_loop(self):
        """Dispatches regular ping packets to keep firmware watchdog alive."""
        interval = config.serial.get("heartbeat_interval_sec", 0.2)
        while self._is_running:
            if self.is_connected:
                self._seq_counter += 1
                self.send_command({"type": "ping", "seq": self._seq_counter})
            time.sleep(interval)

    def _sync_initial_config(self):
        """Provisions STM32 with configured encoder CPR calibrations and motor inversions."""
        try:
            enc_map = {"enc1": 1, "enc2": 2, "wenc1": 3, "wenc2": 4}
            for key, enc_id in enc_map.items():
                enc_cfg = config.encoders.get(key, {})
                cpr = float(enc_cfg.get("cpr", 0.0))
                gear = float(enc_cfg.get("gear_ratio", 1.0))
                mult = int(enc_cfg.get("multiplier", 4))
                if cpr > 0:
                    self.send_command({
                        "type": "config_encoder",
                        "id": enc_id,
                        "cpr": cpr,
                        "gear": gear,
                        "mult": mult
                    })
            self.send_command({
                "type": "config_invert",
                "crawler_a": config.motors.get("crawler_a_inverted", False),
                "crawler_b": config.motors.get("crawler_b_inverted", True),
                "gripper_a": config.motors.get("gripper_a_inverted", False),
                "gripper_b": config.motors.get("gripper_b_inverted", False)
            })
            logger.info("Synchronized encoder calibrations and direction inversions with STM32.")
        except Exception as e:
            logger.debug(f"Failed to sync initial config: {e}")
