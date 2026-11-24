/**
 * ============================================================================
 * IIT Mandi Power Line Inspection Crawler Robot - Main Controller Firmware
 * ============================================================================
 * File: PinDefinitions.h
 * 
 * Authoritative PCB Pin Mapping for STM32 Nucleo-F446RE.
 * Sourced directly from: STM32_Nucleo_F446RE_GPIO_Named_Connection_Sheet.xlsx
 * EasyEDA Netlist + Nucleo Pinout Reference.
 * 
 * DO NOT CHANGE THESE PIN ASSIGNMENTS.
 * ============================================================================
 */

#ifndef PIN_DEFINITIONS_H
#define PIN_DEFINITIONS_H

#include <Arduino.h>

// ----------------------------------------------------------------------------
// 1. CRAWLER MOTORS - Cytron MDD10A Driver 1 (Sign-Magnitude PWM + DIR)
// ----------------------------------------------------------------------------
// Crawler Motor A (Nucleo CN10-23 / CN7-38)
#define PIN_CRAWL_A_PWM       PA8     // Timer: TIM1_CH1
#define PIN_CRAWL_A_DIR       PC0     // GPIO Output

// Crawler Motor B (Nucleo CN10-21 / CN7-36)
#define PIN_CRAWL_B_PWM       PA9     // Timer: TIM1_CH2
#define PIN_CRAWL_B_DIR       PC1     // GPIO Output

// ----------------------------------------------------------------------------
// 2. GRIPPER MOTORS - Cytron MDD10A Driver 2 (Sign-Magnitude PWM + DIR)
// ----------------------------------------------------------------------------
// Gripper Motor A (Nucleo CN10-33 / CN7-35)
#define PIN_GRIP_A_PWM        PA10    // Timer: TIM1_CH3
#define PIN_GRIP_A_DIR        PC2     // GPIO Output

// Gripper Motor B (Nucleo CN10-14 / CN7-37)
#define PIN_GRIP_B_PWM        PA11    // Timer: TIM1_CH4
#define PIN_GRIP_B_DIR        PC3     // GPIO Output

// ----------------------------------------------------------------------------
// 3. SERVO MOTORS (50Hz PWM)
// ----------------------------------------------------------------------------
// Servo 1 (Nucleo CN7-28 -> JP1 pin 1)
#define PIN_SERVO_1_PWM       PA0     // Timer: TIM2_CH1 / TIM5_CH1

// Servo 2 (Nucleo CN10-13 -> JP1 pin 2)
#define PIN_SERVO_2_PWM       PA6     // Timer: TIM3_CH1 / TIM13_CH1

// Note: PCB netlist also has reserved JP1 headers:
// PA7  (CN10-15): SERVO_3_PWM
// PB14 (CN10-28): SERVO_4_PWM
// PB15 (CN10-26): SERVO_5_PWM

// ----------------------------------------------------------------------------
// 4. I2C SENSORS (I2C1 Bus)
// ----------------------------------------------------------------------------
// Shared I2C Bus for MPU9250 IMU, INA219 #1, INA219 #2 (10k pullups R1/R2 to +3V3)
#define PIN_I2C_SCL           PB8     // Nucleo CN10-3 (I2C1_SCL)
#define PIN_I2C_SDA           PB9     // Nucleo CN10-5 (I2C1_SDA)

// ----------------------------------------------------------------------------
// 5. QUADRATURE ENCODERS (4 Independent Channels)
// ----------------------------------------------------------------------------
// Encoder 1 (H1 connector)
#define PIN_ENC1_A            PB6     // Nucleo CN10-17 (TIM4_CH1)
#define PIN_ENC1_B            PB7     // Nucleo CN7-21  (TIM4_CH2)

// Encoder 2 (H2 connector)
#define PIN_ENC2_A            PC6     // Nucleo CN10-4  (EXTI6)
#define PIN_ENC2_B            PC7     // Nucleo CN10-19 (EXTI7)

// Wheel Encoder 1 (H7 connector)
#define PIN_WENC1_A           PB4     // Nucleo CN10-27 (EXTI4)
#define PIN_WENC1_B           PB5     // Nucleo CN10-29 (EXTI5)

// Wheel Encoder 2 (H6 connector)
#define PIN_WENC2_A           PA15    // Nucleo CN7-17  (EXTI15)
#define PIN_WENC2_B           PB3     // Nucleo CN10-31 (EXTI3)

// ----------------------------------------------------------------------------
// 6. COMMUNICATION & RESERVED PCB PINS (Documented for completeness)
// ----------------------------------------------------------------------------
// Raspberry Pi Hardware UART (Optional alternative to ST-LINK USB VCP)
#define PIN_PI_UART_RX        PA2     // Nucleo CN10-35 (USART2_TX on MCU side)
#define PIN_PI_UART_TX        PA3     // Nucleo CN10-37 (USART2_RX on MCU side)

// RC iBUS Input (Reserved)
#define PIN_RC_IBUS           PA1     // Nucleo CN7-30

// Time-of-Flight Sensors (Reserved)
#define PIN_TOF1_RX           PC10    // Nucleo CN7-1
#define PIN_TOF1_TX           PC11    // Nucleo CN7-2
#define PIN_TOF2_RX           PC12    // Nucleo CN7-3
#define PIN_TOF2_TX           PD2     // Nucleo CN7-4

// Limit Switch Inputs (Reserved - do not implement switch logic)
#define PIN_LIMIT_1           PC5     // Nucleo CN10-6
#define PIN_LIMIT_2           PB12    // Nucleo CN10-16
#define PIN_LIMIT_3           PB1     // Nucleo CN10-24
#define PIN_LIMIT_4           PB13    // Nucleo CN10-30

#endif // PIN_DEFINITIONS_H
