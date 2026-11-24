/**
 * ============================================================================
 * IIT Mandi Power Line Inspection Crawler Robot - Main Controller Firmware
 * ============================================================================
 * File: ServoManager.cpp
 * ============================================================================
 */

#include "ServoManager.h"

ServoManager::ServoManager()
    : _angle1(SERVO_1_DEFAULT_ANGLE),
      _angle2(SERVO_2_DEFAULT_ANGLE),
      _enabled1(false),
      _enabled2(false),
      _minAngle1(SERVO_1_MIN_ANGLE),
      _maxAngle1(SERVO_1_MAX_ANGLE),
      _minAngle2(SERVO_2_MIN_ANGLE),
      _maxAngle2(SERVO_2_MAX_ANGLE)
{
}

void ServoManager::begin() {
    // Attach Servo 1 on PA0 with safe pulse range
    _servo1.attach(PIN_SERVO_1_PWM, SERVO_MIN_PULSE_US, SERVO_MAX_PULSE_US);
    _servo1.write(_angle1);
    _enabled1 = true;

    // Attach Servo 2 on PA6 with safe pulse range
    _servo2.attach(PIN_SERVO_2_PWM, SERVO_MIN_PULSE_US, SERVO_MAX_PULSE_US);
    _servo2.write(_angle2);
    _enabled2 = true;
}

bool ServoManager::setAngle(uint8_t servoId, int16_t angle) {
    if (servoId == 1) {
        if (angle < _minAngle1) angle = _minAngle1;
        if (angle > _maxAngle1) angle = _maxAngle1;
        _angle1 = angle;
        if (_enabled1) {
            _servo1.write(_angle1);
        }
        return true;
    } else if (servoId == 2) {
        if (angle < _minAngle2) angle = _minAngle2;
        if (angle > _maxAngle2) angle = _maxAngle2;
        _angle2 = angle;
        if (_enabled2) {
            _servo2.write(_angle2);
        }
        return true;
    }
    return false;
}

void ServoManager::setEnabled(uint8_t servoId, bool enabled) {
    if (servoId == 1) {
        if (enabled && !_enabled1) {
            _servo1.attach(PIN_SERVO_1_PWM, SERVO_MIN_PULSE_US, SERVO_MAX_PULSE_US);
            _servo1.write(_angle1);
            _enabled1 = true;
        } else if (!enabled && _enabled1) {
            _servo1.detach();
            _enabled1 = false;
        }
    } else if (servoId == 2) {
        if (enabled && !_enabled2) {
            _servo2.attach(PIN_SERVO_2_PWM, SERVO_MIN_PULSE_US, SERVO_MAX_PULSE_US);
            _servo2.write(_angle2);
            _enabled2 = true;
        } else if (!enabled && _enabled2) {
            _servo2.detach();
            _enabled2 = false;
        }
    }
}

int16_t ServoManager::getAngle(uint8_t servoId) const {
    if (servoId == 1) return _angle1;
    if (servoId == 2) return _angle2;
    return 0;
}

bool ServoManager::isEnabled(uint8_t servoId) const {
    if (servoId == 1) return _enabled1;
    if (servoId == 2) return _enabled2;
    return false;
}

void ServoManager::safeState() {
    // Put both servos into neutral/safe positions
    setAngle(1, SERVO_1_DEFAULT_ANGLE);
    setAngle(2, SERVO_2_DEFAULT_ANGLE);
}

void ServoManager::setLimits(uint8_t servoId, int16_t minAngle, int16_t maxAngle) {
    if (minAngle < 0) minAngle = 0;
    if (maxAngle > 180) maxAngle = 180;
    if (minAngle > maxAngle) minAngle = maxAngle;

    if (servoId == 1) {
        _minAngle1 = minAngle;
        _maxAngle1 = maxAngle;
        setAngle(1, _angle1); // Re-clamp
    } else if (servoId == 2) {
        _minAngle2 = minAngle;
        _maxAngle2 = maxAngle;
        setAngle(2, _angle2); // Re-clamp
    }
}
