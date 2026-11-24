"""
IIT Mandi Power Line Inspection Crawler Robot - Phone Notification Module (ntfy.sh).
Sends instant push notifications to mobile phones when the robot boots or changes state.
Requires NO accounts, NO tokens, and NO external dependencies (pure Python urllib).
"""

import logging
import socket
import threading
import time
import urllib.request
from typing import Optional

from .config import config

logger = logging.getLogger("Notifier")

def get_local_ip() -> str:
    """Detects the primary local IP address of the robot on the network."""
    try:
        s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        s.settimeout(0.5)
        s.connect(("8.8.8.8", 80))
        ip = s.getsockname()[0]
        s.close()
        return ip
    except Exception:
        return "127.0.0.1"

def send_ntfy_notification(
    title: str = "🤖 IIT Mandi Crawler Robot Online",
    message: Optional[str] = None,
    priority: str = "high",
    tags: str = "robot,rocket"
) -> bool:
    """
    Sends an HTTP POST notification to ntfy.sh topic.
    Non-blocking / fails gracefully if network is unavailable.
    """
    notif_cfg = config.raw_config.get("notifications", {})
    if not notif_cfg.get("enabled", True):
        logger.info("Notifications are disabled in config.")
        return False

    server = notif_cfg.get("server", "https://ntfy.sh").rstrip("/")
    topic = notif_cfg.get("topic", "iitmandi_crawler_robot")
    ip = get_local_ip()
    ctrl_port = config.servers.get("control_port", 8002)
    mon_port = config.servers.get("monitor_port", 8003)

    if message is None:
        message = (
            f"Robot server is ONLINE & READY!\n"
            f"Local IP: {ip}\n"
            f"Monitoring: http://{ip}:{mon_port}\n"
            f"Control: http://{ip}:{ctrl_port}"
        )

    click_url = f"http://{ip}:{mon_port}"
    target_url = f"{server}/{topic}"

    def _send_worker():
        for attempt in range(5):
            try:
                req = urllib.request.Request(
                    target_url,
                    data=message.encode("utf-8"),
                    headers={
                        "Title": title,
                        "Priority": priority,
                        "Tags": tags,
                        "Click": click_url,
                        "User-Agent": "IITMandi-Robot-Crawler/1.0"
                    },
                    method="POST"
                )
                with urllib.request.urlopen(req, timeout=5) as response:
                    if response.status == 200:
                        logger.info(f"Notification sent successfully to {target_url}!")
                        return True
            except Exception as e:
                logger.debug(f"Notification attempt {attempt+1} failed: {e}")
                time.sleep(2.0)
        logger.warning(f"Could not deliver notification to {target_url} after retries.")
        return False

    t = threading.Thread(target=_send_worker, name="NtfyWorker", daemon=True)
    t.start()
    return True
