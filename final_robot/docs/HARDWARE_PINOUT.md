# Hardware Pinout & PCB Mapping Reference

**Project:** IIT Mandi Power Line Inspection Crawler Robot  
**Target Microcontroller:** STM32 Nucleo-F446RE  
**Authoritative Reference:** `STM32_Nucleo_F446RE_GPIO_Named_Connection_Sheet.xlsx` (EasyEDA netlist + Nucleo pinout)

---

## 1. Master Pin Assignment Table

| Nucleo Connector | Pin # | MCU GPIO | Signal / Net Name | Connected Hardware Component | Firmware Function / Timer Channel |
|---|---|---|---|---|---|
| **CN10** | 23 | **PA8** | `CRAWL_A_PWM` | Cytron MDD10A (Driver 1) Pin 2 | **TIM1_CH1** (20kHz PWM) |
| **CN7**  | 38 | **PC0** | `CRAWL_A_DIR` | Cytron MDD10A (Driver 1) Pin 1 | GPIO Output (Direction) |
| **CN10** | 21 | **PA9** | `CRAWL_B_PWM` | Cytron MDD10A (Driver 1) Pin 4 | **TIM1_CH2** (20kHz PWM) |
| **CN7**  | 36 | **PC1** | `CRAWL_B_DIR` | Cytron MDD10A (Driver 1) Pin 3 | GPIO Output (Direction) |
| **CN10** | 33 | **PA10**| `GRIP_A_PWM`  | Cytron MDD10A (Driver 2) Pin 2 | **TIM1_CH3** (20kHz PWM) |
| **CN7**  | 35 | **PC2** | `GRIP_A_DIR`  | Cytron MDD10A (Driver 2) Pin 1 | GPIO Output (Direction) |
| **CN10** | 14 | **PA11**| `GRIP_B_PWM`  | Cytron MDD10A (Driver 2) Pin 4 | **TIM1_CH4** (20kHz PWM) |
| **CN7**  | 37 | **PC3** | `GRIP_B_DIR`  | Cytron MDD10A (Driver 2) Pin 3 | GPIO Output (Direction) |
| **CN7**  | 28 | **PA0** | `SERVO_1_PWM` | JP1 Header Pin 1 | **TIM2_CH1 / TIM5_CH1** (50Hz Servo PWM) |
| **CN10** | 13 | **PA6** | `SERVO_2_PWM` | JP1 Header Pin 2 | **TIM3_CH1 / TIM13_CH1** (50Hz Servo PWM) |
| **CN10** | 3  | **PB8** | `I2C_SCL`     | MPU-9250, INA219 #1, INA219 #2 | **I2C1_SCL** (10k pullup R1 to +3V3) |
| **CN10** | 5  | **PB9** | `I2C_SDA`     | MPU-9250, INA219 #1, INA219 #2 | **I2C1_SDA** (10k pullup R2 to +3V3) |
| **CN10** | 17 | **PB6** | `ENC1_A`      | H1 JST-XH-4P Pin 1 (Encoder 1) | **TIM4_CH1** (Hardware Encoder Mode AF02) |
| **CN7**  | 21 | **PB7** | `ENC1_B`      | H1 JST-XH-4P Pin 2 (Encoder 1) | **TIM4_CH2** (Hardware Encoder Mode AF02) |
| **CN10** | 4  | **PC6** | `ENC2_A`      | H2 JST-XH-4P Pin 1 (Encoder 2) | **EXTI6** (Interrupt CHANGE) |
| **CN10** | 19 | **PC7** | `ENC2_B`      | H2 JST-XH-4P Pin 2 (Encoder 2) | **EXTI7** (Interrupt CHANGE) |
| **CN10** | 27 | **PB4** | `WENC1_A`     | H7 JST-XH-4P Pin 1 (Wheel Enc 1)| **EXTI4** (Interrupt CHANGE) |
| **CN10** | 29 | **PB5** | `WENC1_B`     | H7 JST-XH-4P Pin 2 (Wheel Enc 1)| **EXTI5** (Interrupt CHANGE) |
| **CN7**  | 17 | **PA15**| `WENC2_A`     | H6 JST-XH-4P Pin 1 (Wheel Enc 2)| **EXTI15** (Interrupt CHANGE) |
| **CN10** | 31 | **PB3** | `WENC2_B`     | H6 JST-XH-4P Pin 2 (Wheel Enc 2)| **EXTI3** (Interrupt CHANGE) |

---

## 2. Communication & Reserved Board Pins

| Nucleo Connector | Pin # | MCU GPIO | Signal / Net Name | Connected Component | Status / Notes |
|---|---|---|---|---|---|
| **CN10** | 35 | **PA2** | `PI_UART_RX` | H8 Header Pin 2 | MCU USART2_TX (Optional RPi hardware serial) |
| **CN10** | 37 | **PA3** | `PI_UART_TX` | H8 Header Pin 1 | MCU USART2_RX (Optional RPi hardware serial) |
| **CN7**  | 30 | **PA1** | `RC_IBUS`    | H5 Header Pin 1 | Reserved RC Receiver input |
| **CN7**  | 1  | **PC10**| `TOF1_RX`    | H3 Header Pin 4 | Reserved TOF 1 Serial |
| **CN7**  | 2  | **PC11**| `TOF1_TX`    | H3 Header Pin 3 | Reserved TOF 1 Serial |
| **CN7**  | 3  | **PC12**| `TOF2_RX`    | H4 Header Pin 4 | Reserved TOF 2 Serial |
| **CN7**  | 4  | **PD2** | `TOF2_TX`    | H4 Header Pin 3 | Reserved TOF 2 Serial |
| **CN10** | 6  | **PC5** | `LIMIT_1`    | J1 Header Pin 1 | Reserved Limit Switch 1 (Logic omitted per spec) |
| **CN10** | 16 | **PB12**| `LIMIT_2`    | J2 Header Pin 1 | Reserved Limit Switch 2 (Logic omitted per spec) |
| **CN10** | 24 | **PB1** | `LIMIT_3`    | J3 Header Pin 1 | Reserved Limit Switch 3 (Logic omitted per spec) |
| **CN10** | 30 | **PB13**| `LIMIT_4`    | J4 Header Pin 1 | Reserved Limit Switch 4 (Logic omitted per spec) |

---

## 3. Silicon Architecture: Timer & EXTI Allocation Analysis

### Why All Four DC Motors Use Timer 1 (TIM1)
- `PA8` = `TIM1_CH1`
- `PA9` = `TIM1_CH2`
- `PA10` = `TIM1_CH3`
- `PA11` = `TIM1_CH4`
All four Cytron MDD10A motor PWM pins map directly to the four channels of the **Advanced-control Timer TIM1**. This enables hardware-synchronized, high-frequency (20 kHz) silent PWM with zero timer prescaler contention.

### Resolution of Encoder EXTI Multiplexer Contention
In the STM32F446RE, external interrupt lines (EXTI 0-15) are multiplexed across ports through the `SYSCFG` register:
- **EXTI Line 6** can be routed to either `PA6`, `PB6`, or `PC6`.
- **EXTI Line 7** can be routed to either `PA7`, `PB7`, or `PC7`.

Notice that **Encoder 1 (`PB6/PB7`)** and **Encoder 2 (`PC6/PC7`)** share EXTI lines 6 and 7! They cannot both trigger EXTI interrupts simultaneously.

**The Solution:**
- `PB6` and `PB7` are configured as alternate function `AF02`, attaching them directly to **Hardware Timer 4 (TIM4) in Quadrature Encoder Mode (SMS=011)**.
- `TIM4` counts quadrature pulses autonomously in hardware inside the silicon without triggering CPU EXTI interrupts.
- This frees **EXTI Line 6** and **EXTI Line 7** completely for **Encoder 2 (`PC6/PC7`)**!
- Wheel Encoder 1 (`PB4/PB5`) uses **EXTI Line 4** and **EXTI Line 5**.
- Wheel Encoder 2 (`PA15/PB3`) uses **EXTI Line 15** and **EXTI Line 3**.
- Result: **All four encoders operate concurrently at maximum speed with zero pin collisions!**
