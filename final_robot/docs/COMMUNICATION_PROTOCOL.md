# Communication Protocol Specification

**Project:** IIT Mandi Power Line Inspection Crawler Robot  
**Interface:** Newline-Delimited JSON (NDJSON) over USB Serial Virtual COM Port (or PA2/PA3 UART)  
**Baud Rate:** 115,200 baud, 8 data bits, no parity, 1 stop bit (8N1)  
**Telemetry Rate:** 20 Hz (every 50 ms)

---

## 1. Raspberry Pi -> STM32 Commands

All commands are single-line JSON objects terminated with `\n`.

### 1.1 Crawler Locomotion
Commands both crawler tracks forward or reverse from the single vertical joystick:
```json
{"type":"crawler","speed":50}
```
- `speed` *(integer)*: `-100` to `+100`. Center is `0`.
- Inversions configured in firmware ensure opposite-end mounted motors rotate in the same physical travel direction.

### 1.2 Independent Gripper Control
Controls each gripper motor independently:
```json
{"type":"gripper","motor":"A","speed":60}
{"type":"gripper","motor":"B","speed":-40}
```
- `motor` *(string)*: `"A"` or `"B"`.
- `speed` *(integer)*: `-100` to `+100`. Center is `0`.

### 1.3 Servo Positioning & Enable
Sets the angle for Servo 1 (PA0) or Servo 2 (PA6):
```json
{"type":"servo","id":1,"angle":90}
{"type":"servo","id":2,"angle":120}
{"type":"servo_enable","id":1,"enabled":false}
```
- `id` *(integer)*: `1` or `2`.
- `angle` *(integer)*: `0` to `180` (strictly clamped to configured safe software limits `10°` to `170°`).

### 1.4 Emergency Stop (Latching)
Immediately zeroes all 4 DC motor PWM outputs and moves servos to safe positions:
```json
{"type":"estop","active":true}
```
- Once received, the firmware latches into `ESTOP_LATCHED` state.
- All subsequent motion commands are rejected until explicitly cleared.

### 1.5 Emergency Stop Reset
Clears the latched emergency stop state, returning the robot to `DISARMED`:
```json
{"type":"reset_estop"}
```

### 1.6 Arm / Disarm System
Arms or disarms the robot motion controllers:
```json
{"type":"arm","active":true}
{"type":"arm","active":false}
```
- **Safety Interlock:** If `active: true` is commanded while crawler or gripper speeds are non-zero, the arm command is **strictly rejected**.

### 1.7 Direction Inversion Configuration
Dynamically updates electrical polarity for motor mounting:
```json
{"type":"config_invert","crawler_a":false,"crawler_b":true,"gripper_a":false,"gripper_b":false}
```

### 1.8 Encoder Calibration Configuration
Configures Counts Per Revolution (CPR) to calibrate RPM readings:
```json
{"type":"config_encoder","id":1,"cpr":1024.0,"gear":1.0,"mult":4}
```

### 1.9 Watchdog Heartbeat / Ping
```json
{"type":"ping","seq":42}
```
STM32 responds with:
```json
{"type":"pong","seq":42,"t":14520}
```

---

## 2. STM32 -> Raspberry Pi Telemetry Stream

Streamed continuously at 20 Hz (`TELEMETRY_INTERVAL_MS = 50`).

### Example Telemetry Frame:
```json
{
  "type": "telemetry",
  "t": 28450,
  "seq": 569,
  "state": "ARMED",
  "armed": true,
  "estop": false,
  "wdog": false,
  "motors": {
    "ca": 50,
    "cb": 50,
    "ga": 0,
    "gb": -25
  },
  "servos": {
    "s1": 90,
    "s2": 120,
    "s1_en": true,
    "s2_en": true
  },
  "encoders": {
    "enc1": {"count": 1420, "rpm": 0.0, "cal": false},
    "enc2": {"count": -1415, "rpm": 0.0, "cal": false},
    "wenc1": {"count": 920, "rpm": 0.0, "cal": false},
    "wenc2": {"count": 925, "rpm": 0.0, "cal": false}
  },
  "imu": {
    "ok": true,
    "addr": "0x68",
    "ax": 0.02, "ay": -0.15, "az": 9.81,
    "gx": 0.10, "gy": -0.20, "gz": 0.00,
    "temp": 31.4,
    "roll": -0.88, "pitch": 0.12
  },
  "ina1": {
    "ok": true,
    "addr": "0x40",
    "v": 12.45,
    "ma": 850.2,
    "mw": 10585.0
  },
  "ina2": {
    "ok": true,
    "addr": "0x41",
    "v": 12.41,
    "ma": 220.0,
    "mw": 2730.2
  },
  "i2c": ["0x40", "0x41", "0x68"]
}
```

---

## 3. WebSocket Channels

- **Control Socket (Port 8002):** `ws://<IP>:8002/ws/control`
  - Accepts real-time command JSON packets from joystick, sliders, and buttons.
  - Broadcasts 20Hz telemetry to update dashboard meters.
- **Monitoring Socket (Port 8003):** `ws://<IP>:8003/ws/telemetry`
  - Dedicated telemetry feed updating the attitude horizon, power sparklines, and encoder counts.

---

## 4. REST API Summary

### Port 8002 (Control):
- `GET /api/status` — Current system state, speeds, arming status.
- `POST /api/crawler` — `{"speed": int}`
- `POST /api/gripper` — `{"motor": "A"|"B", "speed": int}`
- `POST /api/servo` — `{"id": 1|2, "angle": int}`
- `POST /api/estop` — Activates Emergency Stop.
- `POST /api/reset_estop` — Resets Emergency Stop latch.
- `POST /api/arm` — `{"active": bool}`

### Port 8003 (Monitoring):
- `GET /api/telemetry` — Full snapshot of current sensor states.
- `GET /video_feed` — Low-latency MJPEG live camera stream.
- `GET /api/camera/status` — Resolution, backend, FPS.
- `POST /api/camera/start` / `stop` / `reconnect` — Stream controls.
