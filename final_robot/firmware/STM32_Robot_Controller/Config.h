/**
 * ============================================================================
 * IIT Mandi Power Line Inspection Crawler Robot - Main Controller Firmware
 * ============================================================================
 * File: Config.h
 * 
 * Configurable parameters, safety thresholds, and timing constants.
 * ============================================================================
 */

#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

// ----------------------------------------------------------------------------
// 1. SERIAL COMMUNICATION
// ----------------------------------------------------------------------------
#define SERIAL_BAUD_RATE            115200
#define SERIAL_RX_BUFFER_SIZE       256
#define TELEMETRY_INTERVAL_MS       50      // 20 Hz telemetry broadcast

// ----------------------------------------------------------------------------
// 2. WATCHDOG & SAFETY TIMINGS
// ----------------------------------------------------------------------------
#define WATCHDOG_TIMEOUT_MS         500     // Stop all motors if no command in 500ms
#define WATCHDOG_CHECK_INTERVAL_MS  20      // 50 Hz watchdog evaluation rate
#define ARMING_REQUIRE_NEUTRAL      true    // Must have 0 commanded speed to arm

// ----------------------------------------------------------------------------
// 3. MOTOR DRIVER PARAMETERS (Cytron MDD10A)
// ----------------------------------------------------------------------------
// In Sign-Magnitude mode:
// PWM pin frequency: 20kHz for silent operation without motor whine
#define MOTOR_PWM_FREQUENCY         20000   // 20 kHz
#define MOTOR_PWM_RESOLUTION        8       // 8-bit (0-255)
#define MAX_COMMANDED_SPEED         100     // Max input percentage (+/- 100%)
#define MOTOR_MAX_DUTY              255     // Max duty cycle output

// Direction Inversion Defaults:
// Crawler motors are mounted at opposite ends of the crawler chassis!
// Crawler B is mechanically flipped relative to Crawler A, so it requires inversion.
#define DEFAULT_CRAWLER_A_INVERT    false
#define DEFAULT_CRAWLER_B_INVERT    true    // Inverted for opposite-end mounting
#define DEFAULT_GRIPPER_A_INVERT    false
#define DEFAULT_GRIPPER_B_INVERT    false

// ----------------------------------------------------------------------------
// 4. SERVO MOTOR PARAMETERS
// ----------------------------------------------------------------------------
// Mechanical safe limits - prevent servo stalling or linkage jamming
#define SERVO_1_MIN_ANGLE           10      // Safe minimum limit (degrees)
#define SERVO_1_MAX_ANGLE           170     // Safe maximum limit (degrees)
#define SERVO_1_DEFAULT_ANGLE       90      // Safe neutral / E-stop angle

#define SERVO_2_MIN_ANGLE           10      // Safe minimum limit (degrees)
#define SERVO_2_MAX_ANGLE           170     // Safe maximum limit (degrees)
#define SERVO_2_DEFAULT_ANGLE       90      // Safe neutral / E-stop angle

// Safe pulse-width range (microseconds)
#define SERVO_MIN_PULSE_US          600
#define SERVO_MAX_PULSE_US          2400

// ----------------------------------------------------------------------------
// 5. ENCODER PARAMETERS & CALIBRATION
// ----------------------------------------------------------------------------
// Quadrature decoding multiplier: 4x decoding (both edges of Channel A and B)
#define ENCODER_QUAD_MULTIPLIER     4

// Counts Per Revolution (CPR) for each encoder channel.
// 1. Crawler / Wheel Motors A & B: Robokits Rhino 300RPM (60:1 planetary) + 3.6:1 external ratio = 216:1 total (11,232 counts/wheel rev)
#define DEFAULT_ENC1_CPR            13.0f   // Crawler Motor A Base PPR
#define DEFAULT_ENC2_CPR            13.0f   // Crawler Motor B Base PPR
#define DEFAULT_ENC1_GEAR_RATIO     216.0f  // 60:1 internal x 3.6 external = 216:1 total reduction
#define DEFAULT_ENC2_GEAR_RATIO     216.0f  // 60:1 internal x 3.6 external = 216:1 total reduction

// 2. Gripper Motors A & B: Robokits Rhino 100RPM 25kgcm (174:1 planetary reduction = 9,048 counts/rev)
#define DEFAULT_WENC1_CPR           13.0f   // Gripper Motor A Base PPR
#define DEFAULT_WENC2_CPR           13.0f   // Gripper Motor B Base PPR
#define DEFAULT_WENC1_GEAR_RATIO    174.0f  // 174:1 planetary gearbox ratio
#define DEFAULT_WENC2_GEAR_RATIO    174.0f  // 174:1 planetary gearbox ratio

#define RPM_CALCULATION_INTERVAL_MS 100     // Calculate RPM at 10 Hz

// ----------------------------------------------------------------------------
// 6. I2C SENSORS
// ----------------------------------------------------------------------------
#define I2C_CLOCK_SPEED             400000  // Fast Mode 400 kHz
#define SENSOR_READ_INTERVAL_MS     50      // Read sensors at 20 Hz

// Typical I2C Addresses
#define MPU9250_DEFAULT_ADDR        0x68
#define MPU9250_ALT_ADDR            0x69
#define INA219_ADDR_PRIMARY         0x40
#define INA219_ADDR_SECONDARY       0x41
#define INA219_ADDR_ALT1            0x44
#define INA219_ADDR_ALT2            0x45

#endif // CONFIG_H
