"""
IIT Mandi Power Line Inspection Crawler Robot - WebSocket Connection Manager.
Manages bidirectional real-time WebSocket communication between frontend dashboards
and the robot backend. Broadcasts 20Hz telemetry and routes inbound control commands.
"""

import asyncio
import json
import logging
from typing import Any, Dict, List, Set
from fastapi import WebSocket, WebSocketDisconnect

logger = logging.getLogger("WebSocketManager")

class WebSocketManager:
    def __init__(self):
        self.control_connections: Set[WebSocket] = set()
        self.monitor_connections: Set[WebSocket] = set()
        self._lock = asyncio.Lock()

    async def connect_control(self, websocket: WebSocket):
        await websocket.accept()
        async with self._lock:
            self.control_connections.add(websocket)
        logger.info(f"Control client connected. Total active: {len(self.control_connections)}")

    async def disconnect_control(self, websocket: WebSocket):
        async with self._lock:
            self.control_connections.discard(websocket)
        logger.info(f"Control client disconnected. Remaining: {len(self.control_connections)}")

    async def connect_monitor(self, websocket: WebSocket):
        await websocket.accept()
        async with self._lock:
            self.monitor_connections.add(websocket)
        logger.info(f"Monitor client connected. Total active: {len(self.monitor_connections)}")

    async def disconnect_monitor(self, websocket: WebSocket):
        async with self._lock:
            self.monitor_connections.discard(websocket)
        logger.info(f"Monitor client disconnected. Remaining: {len(self.monitor_connections)}")

    async def broadcast_telemetry(self, telemetry: Dict[str, Any]):
        """Broadcasts live telemetry JSON to all connected control and monitoring dashboards."""
        msg = json.dumps(telemetry)
        
        # Gather all sockets
        async with self._lock:
            all_sockets = list(self.control_connections) + list(self.monitor_connections)

        if not all_sockets:
            return

        dead_sockets: List[WebSocket] = []
        for ws in all_sockets:
            try:
                await ws.send_text(msg)
            except Exception:
                dead_sockets.append(ws)

        if dead_sockets:
            async with self._lock:
                for ws in dead_sockets:
                    self.control_connections.discard(ws)
                    self.monitor_connections.discard(ws)

    @property
    def total_clients(self) -> int:
        return len(self.control_connections) + len(self.monitor_connections)
