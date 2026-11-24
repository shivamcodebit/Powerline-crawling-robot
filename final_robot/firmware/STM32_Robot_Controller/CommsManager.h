/**
 * ============================================================================
 * IIT Mandi Power Line Inspection Crawler Robot - Main Controller Firmware
 * ============================================================================
 * File: CommsManager.h
 * 
 * Non-blocking Newline-Delimited JSON Communication Protocol:
 * - Serial command receiver & zero-heap JSON parser
 * - Telemetry serializer & high-speed streaming engine
 * - Validation & acknowledgement
 * ============================================================================
 */

#ifndef COMMS_MANAGER_H
#define COMMS_MANAGER_H

#include <Arduino.h>
#include "Config.h"
#include "MotorManager.h"
#include "ServoManager.h"
#include "EncoderManager.h"
#include "SensorManager.h"
#include "SafetyWatchdog.h"

class CommsManager {
public:
    CommsManager(MotorManager &motorMgr,
                 ServoManager &servoMgr,
                 EncoderManager &encMgr,
                 SensorManager &sensorMgr,
                 SafetyWatchdog &safety);

    void begin(uint32_t baudRate = SERIAL_BAUD_RATE);

    // Read serial buffer, process incoming JSON commands
    void processIncoming();

    // Stream telemetry JSON packet at periodic interval
    void sendTelemetry();

private:
    MotorManager   &_motors;
    ServoManager   &_servos;
    EncoderManager &_encoders;
    SensorManager  &_sensors;
    SafetyWatchdog &_safety;

    char _rxBuffer[SERIAL_RX_BUFFER_SIZE];
    uint16_t _rxIndex;
    uint32_t _lastTelemetryTime;
    uint32_t _telemetrySequence;

    // Command dispatchers
    void handleCommand(const char* json);
    void handleCrawlerCommand(const char* json);
    void handleGripperCommand(const char* json);
    void handleServoCommand(const char* json);
    void handleServoEnableCommand(const char* json);
    void handleEstopCommand(const char* json);
    void handleResetEstopCommand();
    void handleArmCommand(const char* json);
    void handleConfigInvertCommand(const char* json);
    void handleConfigEncoderCommand(const char* json);
    void handlePingCommand(const char* json);

    // Lightweight zero-allocation JSON extraction helpers
    bool extractStringField(const char* json, const char* key, char* outVal, size_t maxLen);
    bool extractIntField(const char* json, const char* key, int32_t &outVal);
    bool extractFloatField(const char* json, const char* key, float &outVal);
    bool extractBoolField(const char* json, const char* key, bool &outVal);

    // Send formatted ACK / Error response
    void sendResponse(bool ok, const char* msg, uint32_t seq = 0);
};

#endif // COMMS_MANAGER_H
