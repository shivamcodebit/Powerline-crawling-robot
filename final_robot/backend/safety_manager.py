"""
IIT Mandi Power Line Inspection Crawler Robot - Safety & Watchdog Supervisor.
Monitors browser WebSocket connection health, tracks heartbeat timeouts,
coordinates emergency stop latching, and ensures fail-safe motor shutdown.
"""

import logging
import threading
import time
from typing import Optional
from .config import config
from .motor_controller import MotorController
from .telemetry_parser import TelemetryStore

logger = logging.getLogger("SafetyManager")

class SafetyManager:
    def __init__(self, motor_ctrl: MotorController, telemetry_store: TelemetryStore):
        self.motor_ctrl = motor_ctrl
        self.telemetry_store = telemetry_store

        self._client_timeout_sec: float = config.safety.get("client_watchdog_timeout_sec", 0.7)
        self._last_client_activity: float = time.monotonic()
        self._is_running = False
        self._monitor_thread: Optional[threading.Thread] = None
        self._active_client_count = 0
        self._client_timeout_triggered = False

    def start(self):
        if self._is_running:
            return
        self._is_running = True
        self._last_client_activity = time.monotonic()
        self._monitor_thread = threading.Thread(target=self._watchdog_loop, name="SafetySupervisor", daemon=True)
        self._monitor_thread.start()
        logger.info("Safety supervisor loop started.")

    def stop(self):
        self._is_running = False
        logger.info("Safety supervisor stopped.")

    def notify_client_activity(self):
        """Called whenever a valid command or heartbeat is received from the control dashboard."""
        self._last_client_activity = time.monotonic()
        if self._client_timeout_triggered:
            self._client_timeout_triggered = False
            logger.info("Control client communication restored.")

    def register_client_connected(self):
        self._active_client_count += 1
        self._last_client_activity = time.monotonic()
        logger.info(f"Control client connected. Total clients: {self._active_client_count}")
        # Always enforce safe stop on new connection / page refresh
        self.motor_ctrl.stop_crawler()
        self.motor_ctrl.stop_gripper_a()
        self.motor_ctrl.stop_gripper_b()

    def register_client_disconnected(self):
        self._active_client_count = max(0, self._active_client_count - 1)
        logger.warning(f"Control client disconnected. Remaining clients: {self._active_client_count}")
        # Immediately stop all motors when client disconnects
        if self._active_client_count == 0:
            self.motor_ctrl.stop_crawler()
            self.motor_ctrl.stop_gripper_a()
            self.motor_ctrl.stop_gripper_b()

    def _watchdog_loop(self):
        while self._is_running:
            now = time.monotonic()
            elapsed = now - self._last_client_activity

            # 1. Check if control client has gone silent while motors might be moving
            if self._active_client_count > 0 and elapsed > self._client_timeout_sec:
                if not self._client_timeout_triggered:
                    self._client_timeout_triggered = True
                    if not self.motor_ctrl.is_neutral():
                        logger.warning(
                            f"SAFETY ALERT: Control client heartbeat timed out ({elapsed:.2f}s > {self._client_timeout_sec}s). "
                            "Zeroing all active motors immediately!"
                        )
                    self.motor_ctrl.stop_crawler()
                    self.motor_ctrl.stop_gripper_a()
                    self.motor_ctrl.stop_gripper_b()

            # 2. Check if STM32 telemetry has gone stale while we think we are armed
            snapshot = self.telemetry_store.get_snapshot()
            if snapshot["health"]["is_stale"] and snapshot["armed"]:
                logger.warning("SAFETY ALERT: STM32 telemetry is stale while armed. Commanding emergency stop.")
                self.motor_ctrl.stop_crawler()
                self.motor_ctrl.stop_gripper_a()
                self.motor_ctrl.stop_gripper_b()

            time.sleep(0.05)  # 20 Hz safety evaluation rate
