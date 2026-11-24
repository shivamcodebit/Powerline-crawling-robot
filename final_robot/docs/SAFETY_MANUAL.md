# Safety Manual & Operational Safety Procedures

**Project:** IIT Mandi Power Line Inspection Crawler Robot  
**Author:** Robotics Engineering Team, IIT Mandi

---

## 1. Safety Philosophy

Operating a heavy crawler robot along live or de-energized high-voltage power transmission lines requires strict fail-safe software and electrical interlocks. A single unintended motor pulse or delayed communication response can cause mechanical runaway, line entanglement, or falling hazards.

This project implements a **Defense-in-Depth Safety Architecture** spanning:
1. Physical Hardware Emergency Stop (Electrical isolation).
2. Microcontroller Firmware Latch & Hardware Watchdog (500 ms).
3. Host Server Supervisor & Browser Connection Watchdog (700 ms).
4. Operator Interlock: Strict Neutral Arming Rule.

---

## 2. Emergency Stop (E-Stop) Architecture

### 2.1 Software E-Stop Latch (Firmware Layer)
- When the Emergency Stop button is clicked on Dashboard 1 (or triggered via REST API), the STM32 firmware enters `STATE_ESTOP_LATCHED`.
- **Immediate Action:**
  - `PWM` pins for all four DC motors (`PA8`, `PA9`, `PA10`, `PA11`) are driven to `0` duty cycle.
  - Cytron MDD10A drivers enter dynamic braking mode.
  - Servos (`PA0`, `PA6`) are commanded to their neutral safe angle (`90°`).
- **Latching Guarantee:**
  - Normal motion commands (`crawler`, `gripper`) are **completely ignored** and rejected by the firmware parser while E-Stop is latched.
  - E-Stop **cannot be cleared** simply by sending zero speed or a new speed command.
  - Clearing requires an explicit `{"type":"reset_estop"}` command.

### 2.2 Physical Emergency Stop Circuit (Recommendation)
> **CRITICAL HARDWARE RECOMMENDATION:**  
> In accordance with industrial robotic machinery standards (ISO 13849 / IEC 62061), software emergency stops should always be paired with a physical, hardwired normally-closed (NC) emergency stop switch or wireless safety relay that directly disconnects the **High-Current Battery Bus (+12V / +24V)** supplying the Cytron MDD10A motor driver terminal blocks. This guarantees motor cutoff even in the event of complete MCU silicon failure or shorted driver MOSFETs.

---

## 3. Dual-Layer Watchdogs

### Layer 1: STM32 Firmware Command Watchdog (500 ms)
- If the Raspberry Pi host server crashes, disconnects, or the USB cable is severed, valid control packets stop reaching the STM32.
- If no valid control packet is received within **500 ms**, the STM32 automatically transitions to `STATE_WATCHDOG_TIMEOUT`.
- Motors are immediately stopped (PWM = 0).
- Telemetry reports `"wdog": true`.
- **Anti-Runaway:** The MCU will NOT automatically resume motion when serial communications recover. A deliberate `arm` command with neutral controls is mandatory.

### Layer 2: Raspberry Pi Host Client Watchdog (700 ms)
- The operator's web browser on Dashboard 1 sends heartbeats and joystick commands via WebSocket.
- If the Wi-Fi link drops, the browser tab is closed, or the computer sleeps for more than **700 ms**, the Python `SafetyManager` thread immediately zeroes all motor commands in software.
- Page reloads or reconnections automatically start in the neutral state. Motion never resumes automatically on reconnection.

---

## 4. Operator Arming & Neutral Interlock

The robot boots into the safe `DISARMED` state.

### The Neutral Interlock Rule:
To transition from `DISARMED` to `ARMED`:
1. The operator must verify the emergency stop is cleared (`reset_estop`).
2. **All motion controls must be at zero (neutral):**
   - Crawler joystick = 0%
   - Gripper Motor A = 0%
   - Gripper Motor B = 0%
3. If an operator attempts to click "ARM ROBOT" while any joystick or slider is displaced, the command is **strictly rejected** by both the frontend UI and the STM32 firmware!

---

## 5. Independent Gripper Actuation Rule

- Gripper Motor A and Gripper Motor B have completely independent control bars.
- Both gripper bars automatically spring back to zero when the operator releases the mouse or lifts their finger from the touchscreen.
- The software **never commands dual grippers simultaneously in an automated sequence**, ensuring that at least one gripper can maintain mechanical clamping security during maintenance.

---

## 6. Pre-Operation Checklist

Before placing the crawler on an inspection line:
1. Verify both INA219 power monitors report valid bus voltages (~12V to 24V).
2. Confirm MPU-9250 IMU attitude horizon responds correctly to chassis tilt.
3. Verify all four quadrature encoder channels reflect mechanical shaft rotation.
4. Test the Emergency Stop button: verify that all four motors stop within < 20 ms.
5. Verify that releasing the crawler joystick immediately halts both crawler tracks.
