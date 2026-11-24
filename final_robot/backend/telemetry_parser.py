"""
IIT Mandi Power Line Inspection Crawler Robot - Telemetry Parser Module.
Validates, decodes, and tracks health metrics for telemetry streaming from STM32.
"""

import json
import logging
import time
from typing import Any, Dict, List, Optional

logger = logging.getLogger("TelemetryParser")

class TelemetryStore:
    def __init__(self):
        self._last_raw: Dict[str, Any] = {}
        self._last_update_monotonic: float = 0.0
        self._last_update_wall: float = 0.0
        self._packet_count: int = 0
        self._error_count: int = 0
        self._last_seq: int = 0
        self._dropped_packets: int = 0
        self._fps_window: List[float] = []

        # Default empty telemetry model
        self._current_telemetry: Dict[str, Any] = self._create_initial_telemetry()

    def _create_initial_telemetry(self) -> Dict[str, Any]:
        return {
            "timestamp": 0,
            "seq": 0,
            "state": "DISCONNECTED",
            "armed": False,
            "estop": False,
            "watchdog_tripped": False,
            "motors": {
                "crawler_a": 0,
                "crawler_b": 0,
                "gripper_a": 0,
                "gripper_b": 0,
            },
            "servos": {
                "servo_1": 90,
                "servo_2": 90,
                "servo_1_enabled": True,
                "servo_2_enabled": True,
            },
            "encoders": {
                "enc1": {"count": 0, "rpm": 0.0, "calibrated": False},
                "enc2": {"count": 0, "rpm": 0.0, "calibrated": False},
                "wenc1": {"count": 0, "rpm": 0.0, "calibrated": False},
                "wenc2": {"count": 0, "rpm": 0.0, "calibrated": False},
            },
            "imu": {
                "connected": False,
                "address": "None",
                "ax": 0.0, "ay": 0.0, "az": 0.0,
                "gx": 0.0, "gy": 0.0, "gz": 0.0,
                "temp": 0.0,
                "roll": 0.0, "pitch": 0.0,
            },
            "ina219_1": {
                "connected": False,
                "address": "None",
                "voltage_v": 0.0,
                "current_ma": 0.0,
                "power_mw": 0.0,
            },
            "ina219_2": {
                "connected": False,
                "address": "None",
                "voltage_v": 0.0,
                "current_ma": 0.0,
                "power_mw": 0.0,
            },
            "i2c_scan": [],
            "health": {
                "stm32_connected": False,
                "is_stale": True,
                "packet_rate_hz": 0.0,
                "total_packets": 0,
                "error_count": 0,
                "last_received_sec_ago": -1.0,
            }
        }

    def parse_line(self, line: str) -> Optional[Dict[str, Any]]:
        line = line.strip()
        if not line or not line.startswith("{") or not line.endswith("}"):
            return None

        try:
            data = json.loads(line)
        except json.JSONDecodeError as e:
            self._error_count += 1
            logger.debug(f"JSON decode error: {e} for line: {line}")
            return None

        msg_type = data.get("type")
        if msg_type != "telemetry":
            return data  # Return ACK, boot, or pong messages for command handlers

        # Process telemetry packet
        now_mono = time.monotonic()
        now_wall = time.time()
        self._last_update_monotonic = now_mono
        self._last_update_wall = now_wall
        self._packet_count += 1

        seq = data.get("seq", 0)
        if self._last_seq > 0 and seq > (self._last_seq + 1):
            self._dropped_packets += (seq - self._last_seq - 1)
        self._last_seq = seq

        # Rate calculation
        self._fps_window.append(now_mono)
        while self._fps_window and (now_mono - self._fps_window[0]) > 1.0:
            self._fps_window.pop(0)
        rate_hz = float(len(self._fps_window))

        # Normalized structure
        motors_raw = data.get("motors", {})
        servos_raw = data.get("servos", {})
        encs_raw = data.get("encoders", {})
        imu_raw = data.get("imu", {})
        ina1_raw = data.get("ina1", {})
        ina2_raw = data.get("ina2", {})

        normalized = {
            "timestamp": data.get("t", 0),
            "seq": seq,
            "state": data.get("state", "UNKNOWN"),
            "armed": bool(data.get("armed", False)),
            "estop": bool(data.get("estop", False)),
            "watchdog_tripped": bool(data.get("wdog", False)),
            "motors": {
                "crawler_a": motors_raw.get("ca", 0),
                "crawler_b": motors_raw.get("cb", 0),
                "gripper_a": motors_raw.get("ga", 0),
                "gripper_b": motors_raw.get("gb", 0),
            },
            "servos": {
                "servo_1": servos_raw.get("s1", 90),
                "servo_2": servos_raw.get("s2", 90),
                "servo_1_enabled": bool(servos_raw.get("s1_en", True)),
                "servo_2_enabled": bool(servos_raw.get("s2_en", True)),
            },
            "encoders": {
                "enc1": {
                    "count": encs_raw.get("enc1", {}).get("count", 0),
                    "rpm": encs_raw.get("enc1", {}).get("rpm", 0.0),
                    "calibrated": encs_raw.get("enc1", {}).get("cal", False),
                },
                "enc2": {
                    "count": encs_raw.get("enc2", {}).get("count", 0),
                    "rpm": encs_raw.get("enc2", {}).get("rpm", 0.0),
                    "calibrated": encs_raw.get("enc2", {}).get("cal", False),
                },
                "wenc1": {
                    "count": encs_raw.get("wenc1", {}).get("count", 0),
                    "rpm": encs_raw.get("wenc1", {}).get("rpm", 0.0),
                    "calibrated": encs_raw.get("wenc1", {}).get("cal", False),
                },
                "wenc2": {
                    "count": encs_raw.get("wenc2", {}).get("count", 0),
                    "rpm": encs_raw.get("wenc2", {}).get("rpm", 0.0),
                    "calibrated": encs_raw.get("wenc2", {}).get("cal", False),
                },
            },
            "imu": {
                "connected": bool(imu_raw.get("ok", False)),
                "address": imu_raw.get("addr", "None"),
                "ax": float(imu_raw.get("ax", 0.0)),
                "ay": float(imu_raw.get("ay", 0.0)),
                "az": float(imu_raw.get("az", 0.0)),
                "gx": float(imu_raw.get("gx", 0.0)),
                "gy": float(imu_raw.get("gy", 0.0)),
                "gz": float(imu_raw.get("gz", 0.0)),
                "temp": float(imu_raw.get("temp", 0.0)),
                "roll": float(imu_raw.get("roll", 0.0)),
                "pitch": float(imu_raw.get("pitch", 0.0)),
            },
            "ina219_1": {
                "connected": bool(ina1_raw.get("ok", False)),
                "address": ina1_raw.get("addr", "None"),
                "voltage_v": float(ina1_raw.get("v", 0.0)),
                "current_ma": float(ina1_raw.get("ma", 0.0)),
                "power_mw": float(ina1_raw.get("mw", 0.0)),
            },
            "ina219_2": {
                "connected": bool(ina2_raw.get("ok", False)),
                "address": ina2_raw.get("addr", "None"),
                "voltage_v": float(ina2_raw.get("v", 0.0)),
                "current_ma": float(ina2_raw.get("ma", 0.0)),
                "power_mw": float(ina2_raw.get("mw", 0.0)),
            },
            "i2c_scan": data.get("i2c", []),
            "health": {
                "stm32_connected": True,
                "is_stale": False,
                "packet_rate_hz": rate_hz,
                "total_packets": self._packet_count,
                "error_count": self._error_count,
                "dropped_packets": self._dropped_packets,
                "last_received_sec_ago": 0.0,
            }
        }

        self._current_telemetry = normalized
        return normalized

    def mark_disconnected(self):
        self._current_telemetry["health"]["stm32_connected"] = False
        self._current_telemetry["health"]["is_stale"] = True
        self._current_telemetry["health"]["packet_rate_hz"] = 0.0
        self._current_telemetry["state"] = "DISCONNECTED"
        self._current_telemetry["armed"] = False

    def get_snapshot(self) -> Dict[str, Any]:
        snapshot = dict(self._current_telemetry)
        now_mono = time.monotonic()
        dt = (now_mono - self._last_update_monotonic) if self._last_update_monotonic > 0 else 999.0
        
        # If no packet arrived in 1.5s, mark as stale (avoids USB buffer jitter on Windows)
        is_stale = dt > 1.5 or not snapshot["health"]["stm32_connected"]
        snapshot["health"] = dict(snapshot["health"])
        snapshot["health"]["is_stale"] = is_stale
        snapshot["health"]["last_received_sec_ago"] = round(dt, 2)
        if is_stale:
            snapshot["health"]["packet_rate_hz"] = 0.0

        return snapshot
