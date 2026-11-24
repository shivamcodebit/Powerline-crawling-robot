"""
IIT Mandi Power Line Inspection Crawler Robot - Mock STM32 Hardware Emulator.
Simulates Nucleo-F446RE firmware execution, physics, kinematics, and serial JSON protocol
for complete automated testing and verification without physical hardware.
"""

import json
import math
import random
import sys
import threading
import time
from typing import Optional

class MockSTM32Controller:
    def __init__(self, on_output_line=None):
        self.on_output_line = on_output_line
        self.is_running = False

        # System state
        self.state = "DISARMED" # DISARMED, ARMED, ESTOP_LATCHED, WATCHDOG_TIMEOUT
        self.estop = False
        self.watchdog_tripped = False
        self.last_cmd_time = time.monotonic()
        self.watchdog_timeout_sec = 0.5

        # Motor commanded speeds (-100 to +100)
        self.crawl_a = 0
        self.crawl_b = 0
        self.grip_a = 0
        self.grip_b = 0

        # Servos
        self.s1 = 90
        self.s2 = 90
        self.s1_en = True
        self.s2_en = True

        # Inversions
        self.invert_ca = False
        self.invert_cb = True
        self.invert_ga = False
        self.invert_gb = False

        # Simulated Encoders
        self.enc1_count = 0
        self.enc2_count = 0
        self.wenc1_count = 0
        self.wenc2_count = 0
        self.enc1_cpr = 0.0 # Uncalibrated by default
        self.enc2_cpr = 0.0
        self.wenc1_cpr = 0.0
        self.wenc2_cpr = 0.0

        # Simulated I2C Devices
        self.i2c_addresses = ["0x40", "0x41", "0x68"]
        self.boot_time = time.monotonic()
        self.seq = 0

        self._thread: Optional[threading.Thread] = None

    def start(self):
        self.is_running = True
        self.boot_time = time.monotonic()
        self.last_cmd_time = time.monotonic()
        self._thread = threading.Thread(target=self._telemetry_loop, name="MockSTM32Loop", daemon=True)
        self._thread.start()

    def stop(self):
        self.is_running = False

    def receive_command_line(self, line: str):
        line = line.strip()
        if not line:
            return

        try:
            msg = json.loads(line)
        except Exception:
            self._send_response(False, "invalid_json")
            return

        cmd_type = msg.get("type")
        self.last_cmd_time = time.monotonic()

        if cmd_type == "ping":
            seq = msg.get("seq", 0)
            self._send_line(json.dumps({"type": "pong", "seq": seq, "t": int((time.monotonic() - self.boot_time) * 1000)}))
            return

        if cmd_type == "estop":
            self.state = "ESTOP_LATCHED"
            self.estop = True
            self.crawl_a = 0
            self.crawl_b = 0
            self.grip_a = 0
            self.grip_b = 0
            self.s1 = 90
            self.s2 = 90
            self._send_response(True, "estop_latched")
            return

        if cmd_type == "reset_estop":
            if self.state == "ESTOP_LATCHED":
                self.state = "DISARMED"
                self.estop = False
                self.watchdog_tripped = False
                self.last_cmd_time = time.monotonic()
                self._send_response(True, "estop_cleared_disarmed")
            else:
                self._send_response(False, "not_in_estop")
            return

        if cmd_type == "arm":
            active = bool(msg.get("active", False))
            if self.state == "ESTOP_LATCHED":
                self._send_response(False, "arm_rejected_estop_latched")
                return

            if active:
                is_neutral = (self.crawl_a == 0 and self.grip_a == 0 and self.grip_b == 0)
                if not is_neutral:
                    self._send_response(False, "arm_rejected_controls_not_neutral")
                    return
                self.state = "ARMED"
                self.watchdog_tripped = False
                self._send_response(True, "system_armed")
            else:
                self.state = "DISARMED"
                self.crawl_a = 0
                self.crawl_b = 0
                self.grip_a = 0
                self.grip_b = 0
                self._send_response(True, "system_disarmed")
            return

        if self.state == "ESTOP_LATCHED":
            self._send_response(False, "estop_active")
            return

        if cmd_type == "crawler":
            spd = int(msg.get("speed", 0))
            if self.state != "ARMED" and spd != 0:
                self._send_response(False, "robot_disarmed")
                return
            self.crawl_a = spd
            self.crawl_b = spd
            self._send_response(True, "crawler_set")

        elif cmd_type == "gripper":
            motor = str(msg.get("motor", "A")).upper()
            spd = int(msg.get("speed", 0))
            if self.state != "ARMED" and spd != 0:
                self._send_response(False, "robot_disarmed")
                return
            if motor == "A":
                self.grip_a = spd
            elif motor == "B":
                self.grip_b = spd
            self._send_response(True, "gripper_set")

        elif cmd_type == "servo":
            sid = int(msg.get("id", 1))
            angle = max(10, min(170, int(msg.get("angle", 90))))
            if sid == 1:
                self.s1 = angle
            elif sid == 2:
                self.s2 = angle
            self._send_response(True, "servo_set")

        elif cmd_type == "servo_enable":
            sid = int(msg.get("id", 1))
            en = bool(msg.get("enabled", True))
            if sid == 1:
                self.s1_en = en
            elif sid == 2:
                self.s2_en = en
            self._send_response(True, "servo_enable_updated")

        elif cmd_type == "config_invert":
            self.invert_ca = bool(msg.get("crawler_a", self.invert_ca))
            self.invert_cb = bool(msg.get("crawler_b", self.invert_cb))
            self.invert_ga = bool(msg.get("gripper_a", self.invert_ga))
            self.invert_gb = bool(msg.get("gripper_b", self.invert_gb))
            self._send_response(True, "inversion_configured")

        elif cmd_type == "config_encoder":
            eid = int(msg.get("id", 1))
            cpr = float(msg.get("cpr", 0.0))
            if eid == 1: self.enc1_cpr = cpr
            elif eid == 2: self.enc2_cpr = cpr
            elif eid == 3: self.wenc1_cpr = cpr
            elif eid == 4: self.wenc2_cpr = cpr
            self._send_response(True, "encoder_calibrated")

        elif cmd_type == "reset_encoders":
            self.enc1_count = 0
            self.enc2_count = 0
            self.wenc1_count = 0
            self.wenc2_count = 0
            self._send_response(True, "encoders_reset")

    def _send_response(self, ok: bool, msg: str):
        res = {"type": "ack", "ok": ok, "msg": msg}
        self._send_line(json.dumps(res))

    def _send_line(self, line: str):
        if self.on_output_line:
            self.on_output_line(line)

    def _telemetry_loop(self):
        last_time = time.monotonic()
        while self.is_running:
            time.sleep(0.05) # 20 Hz
            now = time.monotonic()
            dt = now - last_time
            last_time = now

            # Watchdog evaluation
            if self.state == "ARMED":
                if (now - self.last_cmd_time) > self.watchdog_timeout_sec:
                    self.state = "WATCHDOG_TIMEOUT"
                    self.watchdog_tripped = True
                    self.crawl_a = 0
                    self.crawl_b = 0
                    self.grip_a = 0
                    self.grip_b = 0

            # Kinematic encoder simulation
            if self.state == "ARMED":
                eff_a = self.crawl_a * (-1 if self.invert_ca else 1)
                eff_b = self.crawl_b * (-1 if self.invert_cb else 1)
                self.enc1_count += int(eff_a * 50 * dt)
                self.enc2_count += int(eff_b * 50 * dt)
                self.wenc1_count += int(eff_a * 45 * dt)
                self.wenc2_count += int(eff_b * 45 * dt)

            self.seq += 1
            t_ms = int((now - self.boot_time) * 1000)

            # Simulated physics
            vibe_x = (random.random() - 0.5) * 0.05
            vibe_y = (random.random() - 0.5) * 0.05
            vibe_z = 9.81 + (random.random() - 0.5) * 0.04

            # Simulated power draw
            motor_current = abs(self.crawl_a) * 8.5 + abs(self.crawl_b) * 8.5 + abs(self.grip_a) * 4.0 + abs(self.grip_b) * 4.0
            total_current_1 = 350.0 + motor_current # Sensor 1 mA
            total_current_2 = 180.0 # Sensor 2 mA logic
            bus_voltage = 12.45 - (motor_current * 0.0003)

            pkt = {
                "type": "telemetry",
                "t": t_ms,
                "seq": self.seq,
                "state": self.state,
                "armed": (self.state == "ARMED"),
                "estop": self.estop,
                "wdog": self.watchdog_tripped,
                "motors": {
                    "ca": self.crawl_a,
                    "cb": self.crawl_b,
                    "ga": self.grip_a,
                    "gb": self.grip_b
                },
                "servos": {
                    "s1": self.s1,
                    "s2": self.s2,
                    "s1_en": self.s1_en,
                    "s2_en": self.s2_en
                },
                "encoders": {
                    "enc1": {"count": self.enc1_count, "rpm": 0.0, "cal": (self.enc1_cpr > 0)},
                    "enc2": {"count": self.enc2_count, "rpm": 0.0, "cal": (self.enc2_cpr > 0)},
                    "wenc1": {"count": self.wenc1_count, "rpm": 0.0, "cal": (self.wenc1_cpr > 0)},
                    "wenc2": {"count": self.wenc2_count, "rpm": 0.0, "cal": (self.wenc2_cpr > 0)}
                },
                "imu": {
                    "ok": True,
                    "addr": "0x68",
                    "ax": round(vibe_x, 2),
                    "ay": round(vibe_y, 2),
                    "az": round(vibe_z, 2),
                    "gx": round((random.random() - 0.5) * 0.2, 2),
                    "gy": round((random.random() - 0.5) * 0.2, 2),
                    "gz": 0.0,
                    "temp": 28.5,
                    "roll": 0.5,
                    "pitch": -0.2
                },
                "ina1": {
                    "ok": True,
                    "addr": "0x40",
                    "v": round(bus_voltage, 2),
                    "ma": round(total_current_1, 1),
                    "mw": round(bus_voltage * total_current_1, 1)
                },
                "ina2": {
                    "ok": True,
                    "addr": "0x41",
                    "v": round(bus_voltage - 0.05, 2),
                    "ma": round(total_current_2, 1),
                    "mw": round(bus_voltage * total_current_2, 1)
                },
                "i2c": self.i2c_addresses
            }

            self._send_line(json.dumps(pkt))

if __name__ == "__main__":
    def print_line(line):
        print(line)

    mock = MockSTM32Controller(print_line)
    mock.start()
    print("Mock STM32 running. Type commands as JSON. Press Ctrl+C to exit.")
    try:
        while True:
            cmd = sys.stdin.readline()
            if not cmd:
                break
            mock.receive_command_line(cmd)
    except KeyboardInterrupt:
        pass
    finally:
        mock.stop()
