/**
 * ============================================================================
 * IIT Mandi Power Line Inspection Crawler Robot - Main Controller Firmware
 * ============================================================================
 * File: ServoManager.h
 * 
 * Controls Servo 1 (PA0) and Servo 2 (PA6).
 * Provides safe angle clamping, configurable pulse-width limits,
 * enable/disable state management, and emergency safe positioning.
 * ============================================================================
 */

#ifndef SERVO_MANAGER_H
#define SERVO_MANAGER_H

#include <Arduino.h>
#include <Servo.h>
#include "PinDefinitions.h"
#include "Config.h"

class ServoManager {
public:
    ServoManager();

    // Initialize servos, attach pins with safe pulse width bounds
    void begin();

    // Set angle for Servo 1 or Servo 2 (clamped to safe min/max)
    bool setAngle(uint8_t servoId, int16_t angle);

    // Enable or disable (detach) servo
    void setEnabled(uint8_t servoId, bool enabled);

    // Getters for current state
    int16_t getAngle(uint8_t servoId) const;
    bool isEnabled(uint8_t servoId) const;

    // Put all servos into configured safe state (called on E-stop)
    void safeState();

    // Configure safe limits at runtime
    void setLimits(uint8_t servoId, int16_t minAngle, int16_t maxAngle);

private:
    Servo _servo1;
    Servo _servo2;

    int16_t _angle1;
    int16_t _angle2;

    bool _enabled1;
    bool _enabled2;

    int16_t _minAngle1;
    int16_t _maxAngle1;
    int16_t _minAngle2;
    int16_t _maxAngle2;
};

#endif // SERVO_MANAGER_H
