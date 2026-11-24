# STM32 Robot Controller Firmware — IIT Mandi Power Line Crawler Robot

Target Microcontroller: **STM32 Nucleo-F446RE**  
Core Framework: **STM32duino (Official STMicroelectronics Core)**  
Environment: **Arduino IDE 2.x**

---

## 1. Quick Start & Setup in Arduino IDE

### Step 1: Install STM32 Board Support in Arduino IDE
1. Open **Arduino IDE**.
2. Go to **File -> Preferences**.
3. In **Additional Boards Manager URLs**, paste:
   ```
   https://github.com/stm32duino/BoardManagerFiles/raw/main/package_stmicroelectronics_index.json
   ```
4. Click **OK**.
5. Go to **Tools -> Board -> Boards Manager...**, search for `STM32` and install:
   **STM32 MCU based boards by STMicroelectronics** (Version 2.7.0 or newer).

### Step 2: Select Board Configuration
Under the **Tools** menu, select the following exact settings:
- **Board:** `Nucleo-64`
- **Board part number:** `Nucleo F446RE`
- **U(S)ART support:** `Enabled (generic 'Serial')`
- **USB support (if available):** `None` (or `CDC` if using direct USB)
- **C Runtime Library:** `Newlib Nano + Float Printf` (enables floating-point formatting)
- **Port:** Select your Nucleo ST-LINK Virtual COM Port (e.g., `COM9` on Windows or `/dev/ttyACM0` on Linux/Raspberry Pi).

### Step 3: Compile and Flash
1. Open `final_robot/firmware/STM32_Robot_Controller/STM32_Robot_Controller.ino` in Arduino IDE.
2. Click **Verify** (checkmark).
3. Connect the Nucleo-F446RE via USB cable to your PC or Raspberry Pi.
4. Click **Upload** (arrow). The on-board ST-LINK will flash the firmware in seconds.

---

## 2. Timer & Channel Allocation / Conflict Analysis

A critical engineering aspect of this implementation is the timer, channel, and EXTI allocation across the STM32F446RE:

| Peripheral | Assigned Pin | Timer / Function | Architecture Analysis & Resolution |
|---|---|---|---|
| **Crawler A PWM** | `PA8` | `TIM1_CH1` | Shares Advanced Timer TIM1 with Crawler B and Grippers. Set to 20kHz for silent operation. |
| **Crawler B PWM** | `PA9` | `TIM1_CH2` | Shares TIM1. No collision. |
| **Gripper A PWM** | `PA10` | `TIM1_CH3` | Shares TIM1. No collision. |
| **Gripper B PWM** | `PA11` | `TIM1_CH4` | Shares TIM1. All 4 DC motor PWMs map perfectly to TIM1 Channels 1-4. |
| **Servo 1 PWM** | `PA0` | `TIM2_CH1` / `TIM5_CH1` | Managed by STM32 Servo library (50Hz pulse generation). |
| **Servo 2 PWM** | `PA6` | `TIM3_CH1` / `TIM13_CH1` | Managed by STM32 Servo library. |
| **Encoder 1 (ENC1)** | `PB6` / `PB7` | `TIM4_CH1` / `TIM4_CH2` | **Hardware Timer 4 Quadrature Encoder Mode (AF02).** Counts autonomously in hardware on both edges. Bypasses EXTI line conflicts! |
| **Encoder 2 (ENC2)** | `PC6` / `PC7` | `EXTI6` / `EXTI7` | Interrupt-based Gray code decoding. Because ENC1 uses TIM4 hardware, EXTI lines 6 & 7 are 100% available for PC6/PC7! |
| **Wheel Enc 1 (WENC1)**| `PB4` / `PB5` | `EXTI4` / `EXTI5` | Interrupt-based Gray code decoding on distinct EXTI lines 4 & 5. |
| **Wheel Enc 2 (WENC2)**| `PA15` / `PB3` | `EXTI15` / `EXTI3` | Interrupt-based Gray code decoding on distinct EXTI lines 15 & 3. |
| **I2C Bus (SCL/SDA)** | `PB8` / `PB9` | `I2C1` | Hardware I2C1 bus at 400kHz. |

---

## 3. Direction Inversion & Opposite Mounting

The two crawler motors are mounted at opposite ends of the crawler chassis facing opposite physical directions:
- In `Config.h`:
  ```cpp
  #define DEFAULT_CRAWLER_A_INVERT  false
  #define DEFAULT_CRAWLER_B_INVERT  true
  ```
- This ensures that a single vertical joystick command (Forward: +100%) drives both crawler tracks forward synchronously in the real physical world.
- Polarity can also be toggled dynamically at runtime from the dashboard without re-flashing!

---

## 4. Safety & Watchdog Architecture

1. **Command Timeout (500ms):**
   If control packets stop arriving from the Raspberry Pi for more than 500ms, the MCU automatically zeroes all motor outputs and transitions to `STATE_WATCHDOG_TIMEOUT`.
2. **Emergency Stop Latch:**
   An E-stop command triggers immediate shutdown and latches in firmware. It cannot be cleared by normal speed commands.
3. **Neutral Interlock for Arming:**
   The robot boots in `DISARMED`. Arming requires an explicit `arm` command, and will be **strictly rejected** if any motor speed command is non-zero.
