"""
IIT Mandi Power Line Inspection Crawler Robot - Main Backend Entrypoint.
Concurrently orchestrates:
- Serial communication manager with STM32
- Motor control & direction inversion manager
- Telemetry ingestion & health supervisor
- Safety watchdog & emergency stop coordinator
- Low-latency MJPEG Camera streamer
- Port 8002: Robot Control Server (Dashboard 1)
- Port 8003: Live Monitoring Server (Dashboard 2)
"""

import asyncio
import logging
import signal
import sys
import uvicorn
from typing import List

from .camera_streamer import CameraStreamer
from .config import config
from .motor_controller import MotorController
from .notifier import send_ntfy_notification
from .safety_manager import SafetyManager
from .serial_manager import SerialManager
from .server_control import create_control_app
from .server_monitor import create_monitor_app
from .telemetry_parser import TelemetryStore
from .websocket_manager import WebSocketManager

# Configure logging
logging.basicConfig(
    level=logging.INFO,
    format="%(asctime)s [%(levelname)s] [%(name)s] %(message)s",
    handlers=[logging.StreamHandler(sys.stdout)]
)
logger = logging.getLogger("RobotMain")

async def telemetry_broadcast_loop(ws_mgr: WebSocketManager, telemetry_store: TelemetryStore):
    """Broadcasts live robot telemetry to all connected WebSocket clients at 20 Hz."""
    logger.info("Starting live WebSocket telemetry broadcast loop (20 Hz)...")
    while True:
        try:
            snapshot = telemetry_store.get_snapshot()
            await ws_mgr.broadcast_telemetry(snapshot)
            await asyncio.sleep(0.05)  # 50ms = 20 Hz
        except asyncio.CancelledError:
            break
        except Exception as e:
            logger.debug(f"Telemetry broadcast exception: {e}")
            await asyncio.sleep(0.1)

async def run_services():
    # 1. Instantiate core subsystems
    telemetry_store = TelemetryStore()
    serial_mgr = SerialManager(telemetry_store)
    motor_ctrl = MotorController(serial_mgr)
    safety_mgr = SafetyManager(motor_ctrl, telemetry_store)
    camera_streamer = CameraStreamer()
    ws_mgr = WebSocketManager()

    # 2. Start hardware & safety supervisor threads
    serial_mgr.start()
    safety_mgr.start()
    if config.camera.get("enabled", True):
        camera_streamer.start()

    # 3. Build FastAPI application instances
    app_control = create_control_app(serial_mgr, motor_ctrl, safety_mgr, telemetry_store, ws_mgr)
    app_monitor = create_monitor_app(serial_mgr, telemetry_store, camera_streamer, ws_mgr)

    # 4. Configure Uvicorn servers
    host = config.servers.get("host", "0.0.0.0")
    control_port = config.servers.get("control_port", 8002)
    monitor_port = config.servers.get("monitor_port", 8003)

    config_ctrl = uvicorn.Config(
        app=app_control,
        host=host,
        port=control_port,
        log_level="warning",
        access_log=False
    )
    server_ctrl = uvicorn.Server(config_ctrl)

    config_mon = uvicorn.Config(
        app=app_monitor,
        host=host,
        port=monitor_port,
        log_level="warning",
        access_log=False
    )
    server_mon = uvicorn.Server(config_mon)

    print("\n" + "=" * 76)
    print("  IIT MANDI POWER LINE INSPECTION CRAWLER ROBOT - SYSTEM ONLINE")
    print("=" * 76)
    print(f"  [+] Dashboard 1 (Robot Control):    http://localhost:{control_port}")
    print(f"  [+] Dashboard 2 (Live Monitoring):  http://localhost:{monitor_port}")
    print(f"  [+] Serial Target:                 {config.serial.get('port')} @ {config.serial.get('baudrate')} baud")
    print(f"  [+] STM32 Hardware Link:           {'CONNECTED' if serial_mgr.is_connected else 'CONNECTING / AUTO-DETECT'}")
    print("=" * 76 + "\n")

    # Send mobile push notification via ntfy.sh
    send_ntfy_notification()

    # 5. Launch all concurrent async tasks
    try:
        await asyncio.gather(
            server_ctrl.serve(),
            server_mon.serve(),
            telemetry_broadcast_loop(ws_mgr, telemetry_store)
        )
    finally:
        logger.info("Shutting down robot software subsystems...")
        safety_mgr.stop()
        camera_streamer.stop()
        serial_mgr.stop()
        logger.info("All subsystems terminated cleanly.")

def main():
    try:
        asyncio.run(run_services())
    except KeyboardInterrupt:
        print("\n[RobotMain] Interrupted by user. Exiting safely.")

if __name__ == "__main__":
    main()
