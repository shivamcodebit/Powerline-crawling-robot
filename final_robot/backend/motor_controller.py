"""
IIT Mandi Power Line Inspection Crawler Robot - Motor & Actuator Controller.
Coordinates crawler locomotion, independent gripper actuation, and servo positioning.
Enforces deadband filtering, clamping, independent gripper safety, and direction inversions.
"""

import logging
from typing import Any, Dict
from .config import config
from .serial_manager import SerialManager

logger = logging.getLogger("MotorController")

class MotorController:
    def __init__(self, serial_mgr: SerialManager):
        self.serial_mgr = serial_mgr

        # Local command state cache
        self.crawler_speed: int = 0
        self.gripper_a_speed: int = 0
        self.gripper_b_speed: int = 0
        self.servo_1_angle: int = config.servos["servo_1"]["default_angle"]
        self.servo_2_angle: int = config.servos["servo_2"]["default_angle"]
        self.servo_1_enabled: bool = config.servos["servo_1"]["enabled"]
        self.servo_2_enabled: bool = config.servos["servo_2"]["enabled"]

        self.crawler_a_inverted: bool = config.motors["crawler_a_inverted"]
        self.crawler_b_inverted: bool = config.motors["crawler_b_inverted"]
        self.gripper_a_inverted: bool = config.motors["gripper_a_inverted"]
        self.gripper_b_inverted: bool = config.motors["gripper_b_inverted"]

        self.deadband: int = config.motors.get("deadband_percent", 2)
        self.max_speed: int = config.safety.get("max_motor_speed_percent", 100)

    def _apply_deadband(self, speed: int) -> int:
        if abs(speed) <= self.deadband:
            return 0
        if speed > self.max_speed:
            return self.max_speed
        if speed < -self.max_speed:
            return -self.max_speed
        return speed

    def set_crawler_speed(self, speed: int) -> bool:
        """Sets both crawler motors forward/reverse together from vertical joystick."""
        filtered_speed = self._apply_deadband(speed)
        self.crawler_speed = filtered_speed
        return self.serial_mgr.send_command({
            "type": "crawler",
            "speed": filtered_speed
        })

    def stop_crawler(self) -> bool:
        return self.set_crawler_speed(0)

    def set_gripper_motor_a(self, speed: int) -> bool:
        """Sets Gripper Motor A speed independently (-100 to +100)."""
        filtered_speed = self._apply_deadband(speed)
        self.gripper_a_speed = filtered_speed
        return self.serial_mgr.send_command({
            "type": "gripper",
            "motor": "A",
            "speed": filtered_speed
        })

    def stop_gripper_a(self) -> bool:
        return self.set_gripper_motor_a(0)

    def set_gripper_motor_b(self, speed: int) -> bool:
        """Sets Gripper Motor B speed independently (-100 to +100)."""
        filtered_speed = self._apply_deadband(speed)
        self.gripper_b_speed = filtered_speed
        return self.serial_mgr.send_command({
            "type": "gripper",
            "motor": "B",
            "speed": filtered_speed
        })

    def stop_gripper_b(self) -> bool:
        return self.set_gripper_motor_b(0)

    def set_servo_angle(self, servo_id: int, angle: int) -> bool:
        """Commands angle for Servo 1 or 2 with strict hardware safety limits."""
        if servo_id == 1:
            cfg = config.servos["servo_1"]
            clamped = max(cfg["min_angle"], min(cfg["max_angle"], angle))
            self.servo_1_angle = clamped
        elif servo_id == 2:
            cfg = config.servos["servo_2"]
            clamped = max(cfg["min_angle"], min(cfg["max_angle"], angle))
            self.servo_2_angle = clamped
        else:
            return False

        return self.serial_mgr.send_command({
            "type": "servo",
            "id": servo_id,
            "angle": clamped
        })

    def set_servo_enabled(self, servo_id: int, enabled: bool) -> bool:
        if servo_id == 1:
            self.servo_1_enabled = enabled
        elif servo_id == 2:
            self.servo_2_enabled = enabled
        else:
            return False

        return self.serial_mgr.send_command({
            "type": "servo_enable",
            "id": servo_id,
            "enabled": enabled
        })

    def configure_inversions(self, crawl_a: bool, crawl_b: bool, grip_a: bool, grip_b: bool) -> bool:
        self.crawler_a_inverted = crawl_a
        self.crawler_b_inverted = crawl_b
        self.gripper_a_inverted = grip_a
        self.gripper_b_inverted = grip_b

        return self.serial_mgr.send_command({
            "type": "config_invert",
            "crawler_a": crawl_a,
            "crawler_b": crawl_b,
            "gripper_a": grip_a,
            "gripper_b": grip_b
        })

    def emergency_stop_all(self) -> bool:
        """Immediately zeroes local caches and transmits latched emergency stop command."""
        self.crawler_speed = 0
        self.gripper_a_speed = 0
        self.gripper_b_speed = 0
        return self.serial_mgr.send_command({"type": "estop", "active": True})

    def reset_emergency_stop(self) -> bool:
        self.crawler_speed = 0
        self.gripper_a_speed = 0
        self.gripper_b_speed = 0
        return self.serial_mgr.send_command({"type": "reset_estop"})

    def set_armed_state(self, arm: bool) -> bool:
        return self.serial_mgr.send_command({"type": "arm", "active": arm})

    def is_neutral(self) -> bool:
        return self.crawler_speed == 0 and self.gripper_a_speed == 0 and self.gripper_b_speed == 0
