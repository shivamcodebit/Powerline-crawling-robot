/**
 * ============================================================================
 * IIT Mandi Power Line Inspection Crawler Robot - Main Controller Firmware
 * ============================================================================
 * File: SafetyWatchdog.h
 * 
 * Safety Watchdog & Arming State Machine:
 * - 500ms command timeout (configurable)
 * - Hardware/Software E-Stop latch
 * - Neutral control interlock for re-arming
 * - Safe motor shutdown
 * ============================================================================
 */

#ifndef SAFETY_WATCHDOG_H
#define SAFETY_WATCHDOG_H

#include <Arduino.h>
#include "Config.h"

enum RobotSystemState {
    STATE_DISARMED = 0,
    STATE_ARMED = 1,
    STATE_ESTOP_LATCHED = 2,
    STATE_WATCHDOG_TIMEOUT = 3
};

class SafetyWatchdog {
public:
    SafetyWatchdog();

    void begin();

    // Periodic watchdog check called in main loop
    void update(bool isNeutralCommand);

    // Call on every valid motor control packet received from Raspberry Pi
    void kickWatchdog();

    // Emergency Stop trigger (latches in firmware)
    void triggerEstop();

    // Explicit manual reset of E-Stop latch
    bool resetEstop();

    // Arming command with neutral interlock
    bool setArmState(bool arm, bool isNeutralCommand);

    // State queries
    RobotSystemState getState() const { return _currentState; }
    const char* getStateString() const;
    bool isArmed() const { return _currentState == STATE_ARMED; }
    bool isEstopLatched() const { return _currentState == STATE_ESTOP_LATCHED; }
    bool isWatchdogTripped() const { return _watchdogTripped; }
    uint32_t getTimeSinceLastCommand() const;

    // Timeout duration configuration
    void setTimeoutMs(uint32_t timeoutMs) { _timeoutMs = timeoutMs; }
    uint32_t getTimeoutMs() const { return _timeoutMs; }

private:
    RobotSystemState _currentState;
    uint32_t _lastCommandTime;
    uint32_t _timeoutMs;
    bool _watchdogTripped;
    uint32_t _lastCheckTime;
};

#endif // SAFETY_WATCHDOG_H
