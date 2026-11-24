/**
 * ============================================================================
 * IIT Mandi Power Line Inspection Crawler Robot - Main Controller Firmware
 * ============================================================================
 * File: SafetyWatchdog.cpp
 * ============================================================================
 */

#include "SafetyWatchdog.h"

SafetyWatchdog::SafetyWatchdog()
    : _currentState(STATE_DISARMED),
      _lastCommandTime(0),
      _timeoutMs(WATCHDOG_TIMEOUT_MS),
      _watchdogTripped(false),
      _lastCheckTime(0)
{
}

void SafetyWatchdog::begin() {
    _currentState = STATE_DISARMED;
    _lastCommandTime = millis();
    _watchdogTripped = false;
    _lastCheckTime = millis();
}

void SafetyWatchdog::kickWatchdog() {
    _lastCommandTime = millis();
    // Do not clear timeout state solely on kick; deliberate re-arm is required
}

void SafetyWatchdog::triggerEstop() {
    _currentState = STATE_ESTOP_LATCHED;
}

bool SafetyWatchdog::resetEstop() {
    if (_currentState == STATE_ESTOP_LATCHED) {
        // Transition from E-Stop to Disarmed
        _currentState = STATE_DISARMED;
        _watchdogTripped = false;
        _lastCommandTime = millis();
        return true;
    }
    return false;
}

bool SafetyWatchdog::setArmState(bool arm, bool isNeutralCommand) {
    if (_currentState == STATE_ESTOP_LATCHED) {
        // Cannot arm while E-Stop is latched!
        return false;
    }

    if (arm) {
        // Safety Interlock: require neutral controls (0 speed) to arm
        if (ARMING_REQUIRE_NEUTRAL && !isNeutralCommand) {
            return false;
        }
        _currentState = STATE_ARMED;
        _watchdogTripped = false;
        _lastCommandTime = millis();
        return true;
    } else {
        _currentState = STATE_DISARMED;
        return true;
    }
}

void SafetyWatchdog::update(bool isNeutralCommand) {
    uint32_t now = millis();
    if (now - _lastCheckTime < WATCHDOG_CHECK_INTERVAL_MS) {
        return;
    }
    _lastCheckTime = now;

    // Check watchdog timeout only if currently ARMED
    if (_currentState == STATE_ARMED) {
        if (now - _lastCommandTime > _timeoutMs) {
            _currentState = STATE_WATCHDOG_TIMEOUT;
            _watchdogTripped = true;
        }
    }
}

uint32_t SafetyWatchdog::getTimeSinceLastCommand() const {
    return millis() - _lastCommandTime;
}

const char* SafetyWatchdog::getStateString() const {
    switch (_currentState) {
        case STATE_ARMED:            return "ARMED";
        case STATE_ESTOP_LATCHED:    return "ESTOP_LATCHED";
        case STATE_WATCHDOG_TIMEOUT: return "WATCHDOG_TIMEOUT";
        case STATE_DISARMED:
        default:                     return "DISARMED";
    }
}
