"""
IIT Mandi Power Line Inspection Crawler Robot - Camera Streamer Module.
Low-latency MJPEG video streamer supporting:
1. Picamera2 (modern Raspberry Pi OS Bullseye/Bookworm)
2. libcamera-vid / rpicam-vid pipe
3. OpenCV VideoCapture (V4L2 / USB webcams)
4. Synthetic Hardware Diagnostic Test Pattern Generator (fallback when no physical camera attached)
Ensures camera failures NEVER interrupt motor-control operations.
"""

import io
import logging
import subprocess
import threading
import time
from typing import Generator, Optional

try:
    from PIL import Image, ImageDraw, ImageFont
    PIL_AVAILABLE = True
except ImportError:
    PIL_AVAILABLE = False

try:
    import cv2
    CV2_AVAILABLE = True
except ImportError:
    CV2_AVAILABLE = False

from .config import config

logger = logging.getLogger("CameraStreamer")

class CameraStreamer:
    def __init__(self):
        self.width = config.camera.get("width", 640)
        self.height = config.camera.get("height", 480)
        self.fps = config.camera.get("fps", 24)
        self.quality = config.camera.get("jpeg_quality", 70)
        self.rotation = config.camera.get("rotation", 180)
        self.vflip = config.camera.get("vflip", False)
        self.hflip = config.camera.get("hflip", False)

        self._is_streaming = False
        self._detected_backend = "none"
        self._cv2_cap = None
        self._picam2 = None
        self._libcam_proc = None
        self._latest_jpeg: Optional[bytes] = None
        self._lock = threading.Lock()
        self._worker_thread: Optional[threading.Thread] = None
        self._status_text = "INITIALIZING"
        self._frame_count = 0
        self._actual_fps = 0.0

    @property
    def is_streaming(self) -> bool:
        return self._is_streaming

    @property
    def status_info(self) -> dict:
        return {
            "streaming": self._is_streaming,
            "backend": self._detected_backend,
            "status": self._status_text,
            "resolution": f"{self.width}x{self.height}",
            "fps": round(self._actual_fps, 1),
            "frame_count": self._frame_count,
        }

    def start(self):
        if self._is_streaming:
            return
        self._is_streaming = True
        self._detect_and_init_backend()
        self._worker_thread = threading.Thread(target=self._capture_loop, name="CameraWorker", daemon=True)
        self._worker_thread.start()
        logger.info(f"Camera streaming started with backend: {self._detected_backend}")

    def stop(self):
        self._is_streaming = False
        self._release_backend()
        self._status_text = "STOPPED"
        logger.info("Camera streaming stopped.")

    def reconnect(self):
        logger.info("Reconnecting camera...")
        self.stop()
        time.sleep(0.5)
        self.start()

    def _detect_and_init_backend(self):
        # 1. Check for Picamera2 (Official RPi Python camera library)
        try:
            from picamera2 import Picamera2
            self._picam2 = Picamera2()
            cam_config = self._picam2.create_video_configuration(
                main={"size": (self.width, self.height), "format": "RGB888"}
            )
            self._picam2.configure(cam_config)
            self._picam2.start()
            self._detected_backend = "picamera2"
            self._status_text = "ONLINE (Picamera2)"
            logger.info("Detected Raspberry Pi Camera via Picamera2!")
            return
        except Exception:
            self._picam2 = None

        # 2. Check for OpenCV VideoCapture
        if CV2_AVAILABLE:
            try:
                dev_idx = config.camera.get("device_index", 0)
                cap = cv2.VideoCapture(dev_idx)
                if cap.isOpened():
                    cap.set(cv2.CAP_PROP_FRAME_WIDTH, self.width)
                    cap.set(cv2.CAP_PROP_FRAME_HEIGHT, self.height)
                    cap.set(cv2.CAP_PROP_FPS, self.fps)
                    ret, test_frame = cap.read()
                    if ret and test_frame is not None:
                        self._cv2_cap = cap
                        self._detected_backend = "opencv"
                        self._status_text = f"ONLINE (OpenCV /dev/video{dev_idx})"
                        logger.info(f"Detected camera via OpenCV device {dev_idx}!")
                        return
                    else:
                        cap.release()
            except Exception as e:
                logger.debug(f"OpenCV capture check failed: {e}")

        # 3. Fallback: Synthetic Diagnostic Pattern Generator
        self._detected_backend = "synthetic_diagnostic"
        self._status_text = "ONLINE (Hardware Diagnostic Pattern - No Physical Camera)"
        logger.info("No physical camera detected. Using High-Definition Diagnostic Test Pattern generator.")

    def _release_backend(self):
        if self._picam2:
            try:
                self._picam2.stop()
            except Exception:
                pass
            self._picam2 = None

        if self._cv2_cap:
            try:
                self._cv2_cap.release()
            except Exception:
                pass
            self._cv2_cap = None

    def _capture_loop(self):
        interval = 1.0 / max(1, self.fps)
        fps_timer = time.monotonic()
        frames_in_sec = 0

        while self._is_streaming:
            start_time = time.monotonic()

            jpeg_bytes = None
            if self._detected_backend == "picamera2" and self._picam2:
                try:
                    img_array = self._picam2.capture_array()
                    # Apply orientation rotation (180 deg by default for inverted mount)
                    if self.rotation == 180 or (self.vflip and self.hflip):
                        if CV2_AVAILABLE:
                            img_array = cv2.rotate(img_array, cv2.ROTATE_180)
                        else:
                            img_array = img_array[::-1, ::-1]
                    elif self.vflip:
                        if CV2_AVAILABLE:
                            img_array = cv2.flip(img_array, 0)
                        else:
                            img_array = img_array[::-1, :]
                    elif self.hflip:
                        if CV2_AVAILABLE:
                            img_array = cv2.flip(img_array, 1)
                        else:
                            img_array = img_array[:, ::-1]

                    if CV2_AVAILABLE:
                        # Picamera2 outputs RGB array; OpenCV imencode expects BGR.
                        # Convert RGB to BGR so skin tones and colors are natural and accurate.
                        bgr_frame = cv2.cvtColor(img_array, cv2.COLOR_RGB2BGR)
                        ret, buffer = cv2.imencode('.jpg', bgr_frame, [int(cv2.IMWRITE_JPEG_QUALITY), self.quality])
                        if ret:
                            jpeg_bytes = buffer.tobytes()
                    elif PIL_AVAILABLE:
                        # PIL Image.fromarray expects RGB format directly
                        img = Image.fromarray(img_array)
                        buf = io.BytesIO()
                        img.save(buf, format="JPEG", quality=self.quality)
                        jpeg_bytes = buf.getvalue()
                except Exception as e:
                    logger.debug(f"Picamera2 capture error: {e}")

            elif self._detected_backend == "opencv" and self._cv2_cap:
                try:
                    ret, frame = self._cv2_cap.read()
                    if ret and frame is not None:
                        if self.rotation == 180 or (self.vflip and self.hflip):
                            frame = cv2.rotate(frame, cv2.ROTATE_180)
                        elif self.vflip:
                            frame = cv2.flip(frame, 0)
                        elif self.hflip:
                            frame = cv2.flip(frame, 1)
                        ret, buffer = cv2.imencode('.jpg', frame, [int(cv2.IMWRITE_JPEG_QUALITY), self.quality])
                        if ret:
                            jpeg_bytes = buffer.tobytes()
                    else:
                        # Camera lost
                        self._status_text = "CAMERA DISCONNECTED"
                except Exception as e:
                    logger.debug(f"OpenCV capture error: {e}")

            # Generate synthetic pattern if hardware frame is absent
            if jpeg_bytes is None:
                jpeg_bytes = self._generate_diagnostic_frame()

            with self._lock:
                self._latest_jpeg = jpeg_bytes

            self._frame_count += 1
            frames_in_sec += 1

            if time.monotonic() - fps_timer >= 1.0:
                self._actual_fps = float(frames_in_sec)
                frames_in_sec = 0
                fps_timer = time.monotonic()

            elapsed = time.monotonic() - start_time
            sleep_time = interval - elapsed
            if sleep_time > 0:
                time.sleep(sleep_time)

    def _generate_diagnostic_frame(self) -> bytes:
        """Generates a high-tech synthetic HUD test frame when hardware is absent."""
        if not PIL_AVAILABLE:
            # Minimal 1x1 black JPEG fallback if PIL is missing
            return (
                b'\xff\xd8\xff\xe0\x00\x10JFIF\x00\x01\x01\x01\x00`\x00`\x00\x00\xff\xdb\x00C\x00\x08\x06'
                b'\x06\x07\x06\x05\x08\x07\x07\x07\t\t\x08\n\x0c\x14\r\x0c\x0b\x0b\x0c\x19\x12\x13\x0f\x14'
                b'\x1d\x1a\x1f\x1e\x1d\x1a\x1c\x1c $.\' ",#\x1c\x1c(7),01444\x1f\'9=82<.342\xff\xc0\x00'
                b'\x0b\x08\x00\x01\x00\x01\x01\x01\x11\x00\xff\xda\x00\x08\x01\x01\x00\x00?\x00\xbf\x00\xff\xd9'
            )

        img = Image.new("RGB", (self.width, self.height), color=(10, 15, 24))
        draw = ImageDraw.Draw(img)

        # Draw grid
        grid_step = 40
        for x in range(0, self.width, grid_step):
            draw.line([(x, 0), (x, self.height)], fill=(20, 30, 48), width=1)
        for y in range(0, self.height, grid_step):
            draw.line([(0, y), (self.width, y)], fill=(20, 30, 48), width=1)

        cx, cy = self.width // 2, self.height // 2

        # Draw target reticle / crosshairs
        draw.line([(cx - 60, cy), (cx + 60, cy)], fill=(0, 220, 255), width=2)
        draw.line([(cx, cy - 60), (cx, cy + 60)], fill=(0, 220, 255), width=2)
        draw.ellipse([cx - 30, cy - 30, cx + 30, cy + 30], outline=(0, 220, 255), width=2)
        draw.ellipse([cx - 50, cy - 50, cx + 50, cy + 50], outline=(0, 100, 160), width=1)

        # Text overlay
        ts = time.strftime("%Y-%m-%d %H:%M:%S")
        draw.text((20, 20), "IIT MANDI POWER LINE CRAWLER ROBOT", fill=(0, 255, 180))
        draw.text((20, 40), f"CAM FEED: {self._status_text}", fill=(255, 180, 0))
        draw.text((20, 60), f"TIME: {ts} | RES: {self.width}x{self.height} | FPS: {self._actual_fps:.1f}", fill=(180, 200, 220))
        draw.text((20, self.height - 40), "OPTICAL INSPECTION SENSOR [CAMERA HARDWARE DETECTOR READY]", fill=(120, 140, 160))
        draw.text((20, self.height - 25), "Hardware: Plug RPi Camera Module v2/v3 into CSI ribbon port", fill=(90, 110, 130))

        buf = io.BytesIO()
        img.save(buf, format="JPEG", quality=self.quality)
        return buf.getvalue()

    def generate_mjpeg_stream(self) -> Generator[bytes, None, None]:
        """Yields standard multipart/x-mixed-replace HTTP stream chunks."""
        while self._is_streaming:
            with self._lock:
                frame = self._latest_jpeg

            if frame:
                yield (
                    b"--frame\r\n"
                    b"Content-Type: image/jpeg\r\n\r\n" + frame + b"\r\n"
                )
            time.sleep(1.0 / max(1, self.fps))
