"""
IIT Mandi Power Line Inspection Crawler Robot - Backend Configuration Module.
Loads system settings from config.yaml with full fallback defaults.
"""

import os
from pathlib import Path
from typing import Any, Dict
import yaml

CONFIG_PATH = Path(__file__).parent / "config.yaml"

DEFAULT_CONFIG: Dict[str, Any] = {
    "serial": {
        "port": "auto",
        "baudrate": 115200,
        "timeout_sec": 0.1,
        "reconnect_interval_sec": 1.5,
        "heartbeat_interval_sec": 0.2,
    },
    "safety": {
        "client_watchdog_timeout_sec": 0.7,
        "firmware_watchdog_timeout_ms": 500,
        "require_neutral_to_arm": True,
        "max_motor_speed_percent": 100,
    },
    "motors": {
        "crawler_a_inverted": False,
        "crawler_b_inverted": True,
        "gripper_a_inverted": False,
        "gripper_b_inverted": False,
        "deadband_percent": 2,
    },
    "servos": {
        "servo_1": {"min_angle": 10, "max_angle": 170, "default_angle": 90, "enabled": True},
        "servo_2": {"min_angle": 10, "max_angle": 170, "default_angle": 90, "enabled": True},
    },
    "encoders": {
        "enc1": {"name": "Crawler Motor Encoder 1", "cpr": 0.0, "gear_ratio": 1.0, "multiplier": 4},
        "enc2": {"name": "Crawler Motor Encoder 2", "cpr": 0.0, "gear_ratio": 1.0, "multiplier": 4},
        "wenc1": {"name": "Wheel Encoder 1", "cpr": 0.0, "gear_ratio": 1.0, "multiplier": 4},
        "wenc2": {"name": "Wheel Encoder 2", "cpr": 0.0, "gear_ratio": 1.0, "multiplier": 4},
    },
    "camera": {
        "enabled": True,
        "backend": "auto",
        "device_index": 0,
        "width": 640,
        "height": 480,
        "fps": 24,
        "jpeg_quality": 70,
    },
    "servers": {
        "host": "0.0.0.0",
        "control_port": 8002,
        "monitor_port": 8003,
    },
}


class RobotConfig:
    def __init__(self, config_dict: Dict[str, Any]):
        self._cfg = config_dict

    @classmethod
    def load(cls, path: Path = CONFIG_PATH) -> "RobotConfig":
        if path.exists():
            try:
                with open(path, "r", encoding="utf-8") as f:
                    loaded = yaml.safe_load(f) or {}
                # Merge loaded with defaults
                merged = cls._deep_merge(DEFAULT_CONFIG, loaded)
                return cls(merged)
            except Exception as e:
                print(f"[Config] Warning: Failed to parse {path} ({e}), using defaults.")
                return cls(DEFAULT_CONFIG)
        return cls(DEFAULT_CONFIG)

    @staticmethod
    def _deep_merge(default: Dict[str, Any], override: Dict[str, Any]) -> Dict[str, Any]:
        result = dict(default)
        for k, v in override.items():
            if k in result and isinstance(result[k], dict) and isinstance(v, dict):
                result[k] = RobotConfig._deep_merge(result[k], v)
            else:
                result[k] = v
        return result

    @property
    def serial(self) -> Dict[str, Any]:
        return self._cfg.get("serial", DEFAULT_CONFIG["serial"])

    @property
    def safety(self) -> Dict[str, Any]:
        return self._cfg.get("safety", DEFAULT_CONFIG["safety"])

    @property
    def motors(self) -> Dict[str, Any]:
        return self._cfg.get("motors", DEFAULT_CONFIG["motors"])

    @property
    def servos(self) -> Dict[str, Any]:
        return self._cfg.get("servos", DEFAULT_CONFIG["servos"])

    @property
    def encoders(self) -> Dict[str, Any]:
        return self._cfg.get("encoders", DEFAULT_CONFIG["encoders"])

    @property
    def camera(self) -> Dict[str, Any]:
        return self._cfg.get("camera", DEFAULT_CONFIG["camera"])

    @property
    def servers(self) -> Dict[str, Any]:
        return self._cfg.get("servers", DEFAULT_CONFIG["servers"])

    def to_dict(self) -> Dict[str, Any]:
        return self._cfg


# Global singleton instance
config = RobotConfig.load()
