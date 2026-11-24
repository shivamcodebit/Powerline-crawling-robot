/**
 * ============================================================================
 * IIT Mandi Power Line Inspection Crawler Robot - Main Controller Firmware
 * ============================================================================
 * Target Board: STM32 Nucleo-F446RE
 * Core:         STM32duino (STMicroelectronics STM32 core)
 * Framework:    Arduino IDE
 * 
 * Hardware Description:
 * - 2x Cytron MDD10A Motor Drivers (Crawler Motors A/B, Gripper Motors A/B)
 * - 2x Standard/High-Torque Servos (Servo 1 on PA0, Servo 2 on PA6)
 * - 4x Quadrature Encoders (ENC1 on PB6/PB7, ENC2 on PC6/PC7, WENC1 on PB4/PB5, WENC2 on PA15/PB3)
 * - I2C Sensors: MPU-9250 9-DoF IMU, 2x INA219 Voltage/Current/Power Monitors
 * - Communication: Serial over USB VCP (default 115200 baud) or PA2/PA3 UART
 * - Visual Heartbeat: On-board User LED LD2 (PA5)
 * ============================================================================
 */

#include <Arduino.h>
#include "PinDefinitions.h"
#include "Config.h"
#include "MotorManager.h"
#include "ServoManager.h"
#include "EncoderManager.h"
#include "SensorManager.h"
#include "SafetyWatchdog.h"
#include "CommsManager.h"

// Instantiate core subsystem managers
MotorManager   motorManager;
ServoManager   servoManager;
EncoderManager encoderManager;
SensorManager  sensorManager;
SafetyWatchdog safetyWatchdog;
CommsManager   commsManager(motorManager, servoManager, encoderManager, sensorManager, safetyWatchdog);

// LED blink timer
static uint32_t lastLedBlink = 0;

void setup() {
    // 0. Initialize User LED (PA5 / LD2 on Nucleo-F446RE)
    pinMode(LED_BUILTIN, OUTPUT);
    digitalWrite(LED_BUILTIN, HIGH); // Solid ON during boot sequence

    // 1. Initialize Serial Communication first for boot diagnostics
    commsManager.begin(SERIAL_BAUD_RATE);
    delay(100);

    // 2. Initialize Motor Driver GPIOs (ensures PWM=0 and DIR=LOW immediately)
    motorManager.begin();

    // 3. Initialize Servo outputs (attaches with safe pulse-widths and safe default angles)
    servoManager.begin();

    // 4. Initialize I2C Bus & scan attached sensors (fail-safe non-blocking)
    sensorManager.begin();

    // 5. Initialize 4-channel Quadrature Encoders
    encoderManager.begin();

    // 6. Initialize Safety & Watchdog subsystem (starts in DISARMED state)
    safetyWatchdog.begin();

    // Boot confirmation message
    Serial.println("{\"type\":\"boot\",\"msg\":\"IIT_MANDI_CRAWLER_FIRMWARE_READY\",\"board\":\"NUCLEO_F446RE\",\"state\":\"DISARMED\"}");

    digitalWrite(LED_BUILTIN, LOW);
}

void loop() {
    // 1. Ingest and execute incoming serial JSON commands from Raspberry Pi
    commsManager.processIncoming();

    // 2. Update quadrature encoders and calculate RPM
    encoderManager.update();

    // 3. Update I2C sensor telemetry (non-blocking)
    sensorManager.update();

    // 4. Evaluate Watchdog & Safety interlocks
    bool isNeutral = (motorManager.getCommandedCrawlerA() == 0 &&
                      motorManager.getCommandedGripperA() == 0 &&
                      motorManager.getCommandedGripperB() == 0);
    safetyWatchdog.update(isNeutral);

    // If watchdog tripped or robot is not armed, enforce hard motor stop
    if (!safetyWatchdog.isArmed()) {
        motorManager.emergencyStopAll();
    }

    // 5. Broadcast live telemetry packet (default 20 Hz)
    commsManager.sendTelemetry();

    // 6. Visual status heartbeat on Nucleo LD2 LED (PA5):
    // - DISARMED: Slow 1 Hz heartbeat blink
    // - ARMED: Fast 5 Hz active heartbeat blink
    // - ESTOP: Fast warning strobe
    uint32_t now = millis();
    uint32_t blinkInterval = 500;
    if (safetyWatchdog.isEstopLatched()) {
        blinkInterval = 50;
    } else if (safetyWatchdog.isArmed()) {
        blinkInterval = 100;
    }

    if (now - lastLedBlink >= blinkInterval) {
        lastLedBlink = now;
        digitalToggle(LED_BUILTIN);
    }
}
