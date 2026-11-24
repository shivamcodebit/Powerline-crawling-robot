"""
IIT Mandi Power Line Inspection Crawler Robot - Live Monitoring Server (Port 8003).
FastAPI application serving the Monitoring & Diagnostics Dashboard,
live MJPEG camera stream, sensor telemetry endpoints, and 20Hz WebSocket feed.
"""

import asyncio
import json
import logging
from pathlib import Path
from typing import Any, Dict
from fastapi import FastAPI, WebSocket, WebSocketDisconnect
from fastapi.responses import FileResponse, JSONResponse, StreamingResponse
from fastapi.staticfiles import StaticFiles

from .camera_streamer import CameraStreamer
from .config import config
from .serial_manager import SerialManager
from .telemetry_parser import TelemetryStore
from .websocket_manager import WebSocketManager

logger = logging.getLogger("ServerMonitor")

STATIC_DIR = Path(__file__).parent.parent / "frontend" / "monitor"

def create_monitor_app(
    serial_mgr: SerialManager,
    telemetry_store: TelemetryStore,
    camera_streamer: CameraStreamer,
    ws_mgr: WebSocketManager
) -> FastAPI:
    app = FastAPI(title="IIT Mandi Crawler Robot - Live Telemetry & Monitoring", version="1.0.0")

    # Serve static assets
    if STATIC_DIR.exists():
        app.mount("/static", StaticFiles(directory=str(STATIC_DIR)), name="static")

    @app.get("/")
    async def index():
        index_file = STATIC_DIR / "index.html"
        if index_file.exists():
            return FileResponse(str(index_file))
        return JSONResponse({"status": "Monitoring Dashboard frontend not found."}, status_code=404)

    @app.get("/api/telemetry")
    async def get_telemetry():
        return telemetry_store.get_snapshot()

    @app.get("/api/camera/status")
    async def get_camera_status():
        return camera_streamer.status_info

    @app.post("/api/camera/start")
    async def start_camera():
        camera_streamer.start()
        return {"ok": True, "status": camera_streamer.status_info}

    @app.post("/api/camera/stop")
    async def stop_camera():
        camera_streamer.stop()
        return {"ok": True, "status": camera_streamer.status_info}

    @app.post("/api/camera/reconnect")
    async def reconnect_camera():
        camera_streamer.reconnect()
        return {"ok": True, "status": camera_streamer.status_info}

    @app.get("/video_feed")
    async def video_feed():
        """MJPEG video streaming endpoint."""
        if not camera_streamer.is_streaming:
            camera_streamer.start()

        return StreamingResponse(
            camera_streamer.generate_mjpeg_stream(),
            media_type="multipart/x-mixed-replace; boundary=frame"
        )

    @app.websocket("/ws/telemetry")
    async def websocket_telemetry_endpoint(websocket: WebSocket):
        await ws_mgr.connect_monitor(websocket)
        try:
            # Immediately send initial snapshot
            await websocket.send_text(json.dumps(telemetry_store.get_snapshot()))
            while True:
                # Keep socket alive and respond to client pings
                data = await websocket.receive_text()
                try:
                    msg = json.loads(data)
                    if msg.get("type") == "ping":
                        await websocket.send_text(json.dumps({"type": "pong"}))
                except Exception:
                    pass
        except WebSocketDisconnect:
            pass
        except Exception as e:
            logger.debug(f"Monitor WS error: {e}")
        finally:
            await ws_mgr.disconnect_monitor(websocket)

    return app
