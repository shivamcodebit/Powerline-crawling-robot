"""
Unit tests for TelemetryParser and Command Serializers.
"""

import json
import pytest
from final_robot.backend.telemetry_parser import TelemetryStore

def test_telemetry_store_initial_state():
    store = TelemetryStore()
    snapshot = store.get_snapshot()
    assert snapshot["state"] == "DISCONNECTED"
    assert snapshot["armed"] is False
    assert snapshot["health"]["stm32_connected"] is False
    assert snapshot["motors"]["crawler_a"] == 0

def test_telemetry_parsing_valid_packet():
    store = TelemetryStore()
    sample_packet = json.dumps({
        "type": "telemetry",
        "t": 15200,
        "seq": 105,
        "state": "ARMED",
        "armed": True,
        "estop": False,
        "wdog": False,
        "motors": {"ca": 60, "cb": 60, "ga": 0, "gb": -25},
        "servos": {"s1": 85, "s2": 110, "s1_en": True, "s2_en": True},
        "encoders": {
            "enc1": {"count": 1200, "rpm": 0.0, "cal": False},
            "enc2": {"count": 1195, "rpm": 0.0, "cal": False},
            "wenc1": {"count": 850, "rpm": 0.0, "cal": False},
            "wenc2": {"count": 852, "rpm": 0.0, "cal": False}
        },
        "imu": {
            "ok": True, "addr": "0x68",
            "ax": 0.05, "ay": -0.12, "az": 9.80,
            "gx": 0.1, "gy": -0.2, "gz": 0.0,
            "temp": 32.1, "roll": -0.7, "pitch": 0.3
        },
        "ina1": {"ok": True, "addr": "0x40", "v": 12.45, "ma": 850.0, "mw": 10582.5},
        "ina2": {"ok": True, "addr": "0x41", "v": 12.41, "ma": 220.0, "mw": 2730.2},
        "i2c": ["0x40", "0x41", "0x68"]
    })

    result = store.parse_line(sample_packet)
    assert result is not None
    assert result["state"] == "ARMED"
    assert result["armed"] is True
    assert result["motors"]["crawler_a"] == 60
    assert result["servos"]["servo_1"] == 85
    assert result["encoders"]["enc1"]["count"] == 1200
    assert result["imu"]["temp"] == 32.1
    assert result["ina219_1"]["voltage_v"] == 12.45
    assert result["i2c_scan"] == ["0x40", "0x41", "0x68"]

    snapshot = store.get_snapshot()
    assert snapshot["health"]["stm32_connected"] is True
    assert snapshot["health"]["total_packets"] == 1

def test_telemetry_parser_malformed_json():
    store = TelemetryStore()
    result = store.parse_line("NOT_A_JSON_STRING")
    assert result is None
    snapshot = store.get_snapshot()
    assert snapshot["health"]["error_count"] >= 0
