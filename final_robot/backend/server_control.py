"""
IIT Mandi Power Line Inspection Crawler Robot - Robot Control Server (Port 8002).
FastAPI application serving the Control Dashboard, REST action endpoints,
and low-latency bidirectional WebSocket command channel.
"""

import asyncio
import json
import logging
from pathlib import Path
from typing import Any, Dict
from fastapi import FastAPI, WebSocket, WebSocketDisconnect, HTTPException
from fastapi.responses import FileResponse, JSONResponse
from fastapi.staticfiles import StaticFiles
from pydantic import BaseModel

from .config import config
from .motor_controller import MotorController
from .safety_manager import SafetyManager
from .serial_manager import SerialManager
from .telemetry_parser import TelemetryStore
from .websocket_manager import WebSocketManager

logger = logging.getLogger("ServerControl")

STATIC_DIR = Path(__file__).parent.parent / "frontend" / "control"

# Pydantic Request Models
class CrawlerCommand(BaseModel):
    speed: int

class GripperCommand(BaseModel):
    motor: str
    speed: int

class ServoCommand(BaseModel):
    id: int
    angle: int

class ServoEnableCommand(BaseModel):
    id: int
    enabled: bool

class ArmCommand(BaseModel):
    active: bool

class InversionCommand(BaseModel):
    crawler_a: bool
    crawler_b: bool
    gripper_a: bool
    gripper_b: bool

def create_control_app(
    serial_mgr: SerialManager,
    motor_ctrl: MotorController,
    safety_mgr: SafetyManager,
    telemetry_store: TelemetryStore,
    ws_mgr: WebSocketManager
) -> FastAPI:
    app = FastAPI(title="IIT Mandi Crawler Robot - Control Server", version="1.0.0")

    # Serve static assets (CSS, JS)
    if STATIC_DIR.exists():
        app.mount("/static", StaticFiles(directory=str(STATIC_DIR)), name="static")

    @app.get("/")
    async def index():
        index_file = STATIC_DIR / "index.html"
        if index_file.exists():
            return FileResponse(str(index_file))
        return JSONResponse({"status": "Control Dashboard frontend not found."}, status_code=404)

    @app.get("/api/status")
    async def get_status():
        snapshot = telemetry_store.get_snapshot()
        return {
            "connected": serial_mgr.is_connected,
            "port": serial_mgr.port_name,
            "armed": snapshot["armed"],
            "state": snapshot["state"],
            "estop": snapshot["estop"],
            "crawler_speed": motor_ctrl.crawler_speed,
            "gripper_a_speed": motor_ctrl.gripper_a_speed,
            "gripper_b_speed": motor_ctrl.gripper_b_speed,
            "inversions": {
                "crawler_a": motor_ctrl.crawler_a_inverted,
                "crawler_b": motor_ctrl.crawler_b_inverted,
                "gripper_a": motor_ctrl.gripper_a_inverted,
                "gripper_b": motor_ctrl.gripper_b_inverted,
            }
        }

    @app.post("/api/crawler")
    async def set_crawler(cmd: CrawlerCommand):
        safety_mgr.notify_client_activity()
        success = motor_ctrl.set_crawler_speed(cmd.speed)
        return {"ok": success, "speed": motor_ctrl.crawler_speed}

    @app.post("/api/gripper")
    async def set_gripper(cmd: GripperCommand):
        safety_mgr.notify_client_activity()
        m = cmd.motor.upper()
        if m == "A":
            success = motor_ctrl.set_gripper_motor_a(cmd.speed)
            return {"ok": success, "motor": "A", "speed": motor_ctrl.gripper_a_speed}
        elif m == "B":
            success = motor_ctrl.set_gripper_motor_b(cmd.speed)
            return {"ok": success, "motor": "B", "speed": motor_ctrl.gripper_b_speed}
        else:
            raise HTTPException(status_code=400, detail="Invalid gripper motor specified (must be 'A' or 'B').")

    @app.post("/api/servo")
    async def set_servo(cmd: ServoCommand):
        safety_mgr.notify_client_activity()
        success = motor_ctrl.set_servo_angle(cmd.id, cmd.angle)
        return {"ok": success, "id": cmd.id, "angle": cmd.angle}

    @app.post("/api/servo_enable")
    async def set_servo_enable(cmd: ServoEnableCommand):
        safety_mgr.notify_client_activity()
        success = motor_ctrl.set_servo_enabled(cmd.id, cmd.enabled)
        return {"ok": success, "id": cmd.id, "enabled": cmd.enabled}

    @app.post("/api/estop")
    async def trigger_estop():
        logger.warning("EMERGENCY STOP ACTIVATED VIA HTTP ENDPOINT!")
        success = motor_ctrl.emergency_stop_all()
        return {"ok": success, "estop": True, "state": "ESTOP_LATCHED"}

    @app.post("/api/reset_estop")
    async def reset_estop():
        logger.info("Manual Emergency Stop Reset requested.")
        success = motor_ctrl.reset_emergency_stop()
        return {"ok": success, "estop": False, "state": "DISARMED"}

    @app.post("/api/arm")
    async def set_arm(cmd: ArmCommand):
        safety_mgr.notify_client_activity()
        if cmd.active and not motor_ctrl.is_neutral():
            raise HTTPException(
                status_code=400,
                detail="Safety Interlock: Cannot arm while motor controls are not in neutral (0 speed)!"
            )
        success = motor_ctrl.set_armed_state(cmd.active)
        return {"ok": success, "armed": cmd.active}

    @app.post("/api/inversion")
    async def configure_inversion(cmd: InversionCommand):
        success = motor_ctrl.configure_inversions(
            cmd.crawler_a, cmd.crawler_b, cmd.gripper_a, cmd.gripper_b
        )
        return {"ok": success}

    @app.post("/api/heartbeat")
    async def client_heartbeat():
        safety_mgr.notify_client_activity()
        return {"ok": True}

    @app.websocket("/ws/control")
    async def websocket_control_endpoint(websocket: WebSocket):
        await ws_mgr.connect_control(websocket)
        safety_mgr.register_client_connected()

        try:
            while True:
                data_text = await websocket.receive_text()
                safety_mgr.notify_client_activity()

                try:
                    msg = json.loads(data_text)
                    msg_type = msg.get("type")

                    if msg_type == "crawler":
                        motor_ctrl.set_crawler_speed(int(msg.get("speed", 0)))
                    elif msg_type == "gripper":
                        motor = str(msg.get("motor", "A")).upper()
                        speed = int(msg.get("speed", 0))
                        if motor == "A":
                            motor_ctrl.set_gripper_motor_a(speed)
                        elif motor == "B":
                            motor_ctrl.set_gripper_motor_b(speed)
                    elif msg_type == "servo":
                        sid = int(msg.get("id", 1))
                        angle = int(msg.get("angle", 90))
                        motor_ctrl.set_servo_angle(sid, angle)
                    elif msg_type == "servo_enable":
                        sid = int(msg.get("id", 1))
                        en = bool(msg.get("enabled", True))
                        motor_ctrl.set_servo_enabled(sid, en)
                    elif msg_type == "estop":
                        motor_ctrl.emergency_stop_all()
                    elif msg_type == "reset_estop":
                        motor_ctrl.reset_emergency_stop()
                    elif msg_type == "arm":
                        active = bool(msg.get("active", False))
                        if active and not motor_ctrl.is_neutral():
                            await websocket.send_text(json.dumps({
                                "type": "error",
                                "msg": "Safety Interlock: Cannot arm while controls are non-zero!"
                            }))
                        else:
                            motor_ctrl.set_armed_state(active)
                    elif msg_type == "inversion":
                        motor_ctrl.configure_inversions(
                            bool(msg.get("crawler_a", False)),
                            bool(msg.get("crawler_b", True)),
                            bool(msg.get("gripper_a", False)),
                            bool(msg.get("gripper_b", False)),
                        )
                    elif msg_type == "ping":
                        await websocket.send_text(json.dumps({"type": "pong"}))
                except json.JSONDecodeError:
                    pass
        except WebSocketDisconnect:
            pass
        except Exception as e:
            logger.debug(f"Control WS error: {e}")
        finally:
            await ws_mgr.disconnect_control(websocket)
            safety_mgr.register_client_disconnected()

    return app
