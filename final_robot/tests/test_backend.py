"""
Integration tests for FastAPI REST Endpoints on Control (8002) and Monitor (8003) servers.
Uses synchronous coroutine advancement to ensure zero event-loop lockups.
"""

import pytest

from final_robot.backend.camera_streamer import CameraStreamer
from final_robot.backend.motor_controller import MotorController
from final_robot.backend.safety_manager import SafetyManager
from final_robot.backend.serial_manager import SerialManager
from final_robot.backend.server_control import (
    create_control_app,
    CrawlerCommand,
    GripperCommand,
    ArmCommand,
    InversionCommand,
    ServoCommand
)
from final_robot.backend.server_monitor import create_monitor_app
from final_robot.backend.telemetry_parser import TelemetryStore
from final_robot.backend.websocket_manager import WebSocketManager

def run_sync(coro):
    """Executes a coroutine without requiring an IOCP OS event loop."""
    try:
        coro.send(None)
    except StopIteration as e:
        return e.value
    raise RuntimeError("Coroutine did not finish in one step")

@pytest.fixture
def robot_system():
    telemetry_store = TelemetryStore()
    serial_mgr = SerialManager(telemetry_store)
    motor_ctrl = MotorController(serial_mgr)
    safety_mgr = SafetyManager(motor_ctrl, telemetry_store)
    camera_streamer = CameraStreamer()
    ws_mgr = WebSocketManager()

    ctrl_app = create_control_app(serial_mgr, motor_ctrl, safety_mgr, telemetry_store, ws_mgr)
    mon_app = create_monitor_app(serial_mgr, telemetry_store, camera_streamer, ws_mgr)

    # Route lookup dictionaries
    ctrl_routes = {getattr(r, "path", None): getattr(r, "endpoint", None) for r in ctrl_app.routes}
    mon_routes = {getattr(r, "path", None): getattr(r, "endpoint", None) for r in mon_app.routes}

    return {
        "ctrl_routes": ctrl_routes,
        "mon_routes": mon_routes,
        "motor_ctrl": motor_ctrl,
        "telemetry_store": telemetry_store,
        "safety_mgr": safety_mgr,
        "camera_streamer": camera_streamer
    }

def test_control_app_status_endpoint(robot_system):
    endpoint = robot_system["ctrl_routes"]["/api/status"]
    data = run_sync(endpoint())
    assert "connected" in data
    assert "armed" in data
    assert "crawler_speed" in data
    assert "inversions" in data

def test_control_app_crawler_and_gripper(robot_system):
    ctrl_crawler = robot_system["ctrl_routes"]["/api/crawler"]
    res = run_sync(ctrl_crawler(CrawlerCommand(speed=65)))
    assert res["speed"] == 65

    ctrl_gripper = robot_system["ctrl_routes"]["/api/gripper"]
    res_a = run_sync(ctrl_gripper(GripperCommand(motor="A", speed=45)))
    assert res_a["speed"] == 45

    res_b = run_sync(ctrl_gripper(GripperCommand(motor="B", speed=-30)))
    assert res_b["speed"] == -30

def test_control_app_estop_and_reset(robot_system):
    ctrl_estop = robot_system["ctrl_routes"]["/api/estop"]
    res_estop = run_sync(ctrl_estop())
    assert res_estop["estop"] is True
    assert res_estop["state"] == "ESTOP_LATCHED"

    ctrl_reset = robot_system["ctrl_routes"]["/api/reset_estop"]
    res_reset = run_sync(ctrl_reset())
    assert res_reset["estop"] is False

def test_monitoring_app_telemetry_endpoint(robot_system):
    endpoint = robot_system["mon_routes"]["/api/telemetry"]
    data = run_sync(endpoint())
    assert "imu" in data
    assert "ina219_1" in data
    assert "encoders" in data
    assert "health" in data

def test_monitoring_app_camera_status(robot_system):
    endpoint = robot_system["mon_routes"]["/api/camera/status"]
    data = run_sync(endpoint())
    assert "backend" in data
    assert "resolution" in data
