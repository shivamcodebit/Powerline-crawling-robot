/**
 * ============================================================================
 * IIT Mandi Power Line Inspection Crawler Robot - Main Controller Firmware
 * ============================================================================
 * File: CommsManager.cpp
 * ============================================================================
 */

#include "CommsManager.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

CommsManager::CommsManager(MotorManager &motorMgr,
                           ServoManager &servoMgr,
                           EncoderManager &encMgr,
                           SensorManager &sensorMgr,
                           SafetyWatchdog &safety)
    : _motors(motorMgr),
      _servos(servoMgr),
      _encoders(encMgr),
      _sensors(sensorMgr),
      _safety(safety),
      _rxIndex(0),
      _lastTelemetryTime(0),
      _telemetrySequence(0)
{
    memset(_rxBuffer, 0, sizeof(_rxBuffer));
}

void CommsManager::begin(uint32_t baudRate) {
    Serial.begin(baudRate);
    _lastTelemetryTime = millis();
}

// ----------------------------------------------------------------------------
// LIGHTWEIGHT ZERO-HEAP JSON EXTRACTION UTILITIES
// ----------------------------------------------------------------------------

bool CommsManager::extractStringField(const char* json, const char* key, char* outVal, size_t maxLen) {
    char searchKey[32];
    snprintf(searchKey, sizeof(searchKey), "\"%s\"", key);
    const char* pos = strstr(json, searchKey);
    if (!pos) return false;

    pos = strchr(pos + strlen(searchKey), ':');
    if (!pos) return false;

    // Advance past whitespace and opening quote
    pos++;
    while (*pos == ' ' || *pos == '\t') pos++;
    if (*pos != '"') return false;
    pos++; // Skip opening quote

    size_t i = 0;
    while (*pos && *pos != '"' && i < (maxLen - 1)) {
        outVal[i++] = *pos++;
    }
    outVal[i] = '\0';
    return true;
}

bool CommsManager::extractIntField(const char* json, const char* key, int32_t &outVal) {
    char searchKey[32];
    snprintf(searchKey, sizeof(searchKey), "\"%s\"", key);
    const char* pos = strstr(json, searchKey);
    if (!pos) return false;

    pos = strchr(pos + strlen(searchKey), ':');
    if (!pos) return false;

    pos++;
    while (*pos == ' ' || *pos == '\t') pos++;
    outVal = strtol(pos, NULL, 10);
    return true;
}

bool CommsManager::extractFloatField(const char* json, const char* key, float &outVal) {
    char searchKey[32];
    snprintf(searchKey, sizeof(searchKey), "\"%s\"", key);
    const char* pos = strstr(json, searchKey);
    if (!pos) return false;

    pos = strchr(pos + strlen(searchKey), ':');
    if (!pos) return false;

    pos++;
    while (*pos == ' ' || *pos == '\t') pos++;
    outVal = strtof(pos, NULL);
    return true;
}

bool CommsManager::extractBoolField(const char* json, const char* key, bool &outVal) {
    char searchKey[32];
    snprintf(searchKey, sizeof(searchKey), "\"%s\"", key);
    const char* pos = strstr(json, searchKey);
    if (!pos) return false;

    pos = strchr(pos + strlen(searchKey), ':');
    if (!pos) return false;

    pos++;
    while (*pos == ' ' || *pos == '\t') pos++;
    if (strncmp(pos, "true", 4) == 0 || strncmp(pos, "1", 1) == 0) {
        outVal = true;
        return true;
    } else if (strncmp(pos, "false", 5) == 0 || strncmp(pos, "0", 1) == 0) {
        outVal = false;
        return true;
    }
    return false;
}

void CommsManager::sendResponse(bool ok, const char* msg, uint32_t seq) {
    Serial.print("{\"type\":\"ack\",\"ok\":");
    Serial.print(ok ? "true" : "false");
    Serial.print(",\"msg\":\"");
    Serial.print(msg);
    Serial.print("\"");
    if (seq > 0) {
        Serial.print(",\"seq\":");
        Serial.print(seq);
    }
    Serial.println("}");
}

// ----------------------------------------------------------------------------
// INCOMING SERIAL STREAM PROCESSOR
// ----------------------------------------------------------------------------

void CommsManager::processIncoming() {
    while (Serial.available() > 0) {
        char c = (char)Serial.read();
        if (c == '\n' || c == '\r') {
            if (_rxIndex > 0) {
                _rxBuffer[_rxIndex] = '\0';
                handleCommand(_rxBuffer);
                _rxIndex = 0;
            }
        } else {
            if (_rxIndex < (sizeof(_rxBuffer) - 1)) {
                _rxBuffer[_rxIndex++] = c;
            } else {
                // Buffer overflow protection: reset buffer
                _rxIndex = 0;
            }
        }
    }
}

void CommsManager::handleCommand(const char* json) {
    char cmdType[32];
    if (!extractStringField(json, "type", cmdType, sizeof(cmdType))) {
        sendResponse(false, "missing_cmd_type");
        return;
    }

    if (strcmp(cmdType, "crawler") == 0) {
        handleCrawlerCommand(json);
    } else if (strcmp(cmdType, "gripper") == 0) {
        handleGripperCommand(json);
    } else if (strcmp(cmdType, "servo") == 0) {
        handleServoCommand(json);
    } else if (strcmp(cmdType, "servo_enable") == 0) {
        handleServoEnableCommand(json);
    } else if (strcmp(cmdType, "estop") == 0) {
        handleEstopCommand(json);
    } else if (strcmp(cmdType, "reset_estop") == 0) {
        handleResetEstopCommand();
    } else if (strcmp(cmdType, "arm") == 0) {
        handleArmCommand(json);
    } else if (strcmp(cmdType, "config_invert") == 0) {
        handleConfigInvertCommand(json);
    } else if (strcmp(cmdType, "config_encoder") == 0) {
        handleConfigEncoderCommand(json);
    } else if (strcmp(cmdType, "reset_encoders") == 0) {
        _encoders.resetCounts();
        sendResponse(true, "encoders_reset");
    } else if (strcmp(cmdType, "ping") == 0) {
        handlePingCommand(json);
    } else {
        sendResponse(false, "unknown_cmd");
    }
}

void CommsManager::handleCrawlerCommand(const char* json) {
    if (_safety.isEstopLatched()) {
        sendResponse(false, "estop_active");
        return;
    }

    int32_t speed = 0;
    if (!extractIntField(json, "speed", speed)) {
        sendResponse(false, "invalid_speed");
        return;
    }

    if (!_safety.isArmed()) {
        if (speed != 0) {
            sendResponse(false, "robot_disarmed");
            return;
        }
    }

    _motors.setCrawlerSpeed((int16_t)speed);
    _safety.kickWatchdog();
    sendResponse(true, "crawler_set");
}

void CommsManager::handleGripperCommand(const char* json) {
    if (_safety.isEstopLatched()) {
        sendResponse(false, "estop_active");
        return;
    }

    char motorStr[8];
    if (!extractStringField(json, "motor", motorStr, sizeof(motorStr))) {
        sendResponse(false, "missing_motor_id");
        return;
    }

    int32_t speed = 0;
    if (!extractIntField(json, "speed", speed)) {
        sendResponse(false, "invalid_speed");
        return;
    }

    if (!_safety.isArmed() && speed != 0) {
        sendResponse(false, "robot_disarmed");
        return;
    }

    if (motorStr[0] == 'A' || motorStr[0] == 'a') {
        _motors.setGripperMotorA((int16_t)speed);
    } else if (motorStr[0] == 'B' || motorStr[0] == 'b') {
        _motors.setGripperMotorB((int16_t)speed);
    } else {
        sendResponse(false, "unknown_motor");
        return;
    }

    _safety.kickWatchdog();
    sendResponse(true, "gripper_set");
}

void CommsManager::handleServoCommand(const char* json) {
    if (_safety.isEstopLatched()) {
        sendResponse(false, "estop_active");
        return;
    }

    int32_t id = 0;
    int32_t angle = 90;
    if (!extractIntField(json, "id", id) || !extractIntField(json, "angle", angle)) {
        sendResponse(false, "invalid_servo_params");
        return;
    }

    if (_servos.setAngle((uint8_t)id, (int16_t)angle)) {
        _safety.kickWatchdog();
        sendResponse(true, "servo_set");
    } else {
        sendResponse(false, "servo_error");
    }
}

void CommsManager::handleServoEnableCommand(const char* json) {
    int32_t id = 0;
    bool enabled = true;
    if (extractIntField(json, "id", id) && extractBoolField(json, "enabled", enabled)) {
        _servos.setEnabled((uint8_t)id, enabled);
        _safety.kickWatchdog();
        sendResponse(true, "servo_enable_updated");
    } else {
        sendResponse(false, "invalid_enable_params");
    }
}

void CommsManager::handleEstopCommand(const char* json) {
    // Immediate stop
    _safety.triggerEstop();
    _motors.emergencyStopAll();
    _servos.safeState();
    sendResponse(true, "estop_latched");
}

void CommsManager::handleResetEstopCommand() {
    if (_safety.resetEstop()) {
        _motors.emergencyStopAll();
        sendResponse(true, "estop_cleared_disarmed");
    } else {
        sendResponse(false, "estop_reset_failed");
    }
}

void CommsManager::handleArmCommand(const char* json) {
    bool active = false;
    if (!extractBoolField(json, "active", active)) {
        sendResponse(false, "missing_active_param");
        return;
    }

    bool isNeutral = (_motors.getCommandedCrawlerA() == 0 &&
                      _motors.getCommandedGripperA() == 0 &&
                      _motors.getCommandedGripperB() == 0);

    if (_safety.setArmState(active, isNeutral)) {
        if (active) {
            sendResponse(true, "system_armed");
        } else {
            _motors.emergencyStopAll();
            sendResponse(true, "system_disarmed");
        }
    } else {
        if (_safety.isEstopLatched()) {
            sendResponse(false, "arm_rejected_estop_latched");
        } else if (!isNeutral) {
            sendResponse(false, "arm_rejected_controls_not_neutral");
        } else {
            sendResponse(false, "arm_failed");
        }
    }
}

void CommsManager::handleConfigInvertCommand(const char* json) {
    bool ca = _motors.getInvertCrawlerA();
    bool cb = _motors.getInvertCrawlerB();
    bool ga = _motors.getInvertGripperA();
    bool gb = _motors.getInvertGripperB();

    extractBoolField(json, "crawler_a", ca);
    extractBoolField(json, "crawler_b", cb);
    extractBoolField(json, "gripper_a", ga);
    extractBoolField(json, "gripper_b", gb);

    _motors.setInversions(ca, cb, ga, gb);
    _safety.kickWatchdog();
    sendResponse(true, "inversion_configured");
}

void CommsManager::handleConfigEncoderCommand(const char* json) {
    int32_t id = 0;
    float cpr = 0.0f;
    float gear = 1.0f;
    int32_t mult = 4;

    if (extractIntField(json, "id", id)) {
        extractFloatField(json, "cpr", cpr);
        extractFloatField(json, "gear", gear);
        extractIntField(json, "mult", mult);
        _encoders.setCalibration((uint8_t)id, cpr, gear, (uint8_t)mult);
        _safety.kickWatchdog();
        sendResponse(true, "encoder_calibrated");
    } else {
        sendResponse(false, "invalid_encoder_id");
    }
}

void CommsManager::handlePingCommand(const char* json) {
    int32_t seq = 0;
    extractIntField(json, "seq", seq);
    _safety.kickWatchdog(); // Reset command timeout when alive ping arrives
    Serial.print("{\"type\":\"pong\",\"seq\":");
    Serial.print(seq);
    Serial.print(",\"t\":");
    Serial.print(millis());
    Serial.println("}");
}

// ----------------------------------------------------------------------------
// OUTGOING TELEMETRY STREAM
// ----------------------------------------------------------------------------

void CommsManager::sendTelemetry() {
    uint32_t now = millis();
    if (now - _lastTelemetryTime < TELEMETRY_INTERVAL_MS) {
        return;
    }
    _lastTelemetryTime = now;
    _telemetrySequence++;

    EncoderData e1 = _encoders.getEncoder1Data();
    EncoderData e2 = _encoders.getEncoder2Data();
    EncoderData ew1 = _encoders.getWheelEncoder1Data();
    EncoderData ew2 = _encoders.getWheelEncoder2Data();

    const IMUData &imu = _sensors.getIMUData();
    const INA219Data &ina1 = _sensors.getINA1Data();
    const INA219Data &ina2 = _sensors.getINA2Data();

    // Stream formatted single-line JSON
    Serial.print("{\"type\":\"telemetry\",\"t\":");
    Serial.print(now);
    Serial.print(",\"seq\":");
    Serial.print(_telemetrySequence);
    Serial.print(",\"state\":\"");
    Serial.print(_safety.getStateString());
    Serial.print("\",\"armed\":");
    Serial.print(_safety.isArmed() ? "true" : "false");
    Serial.print(",\"estop\":");
    Serial.print(_safety.isEstopLatched() ? "true" : "false");
    Serial.print(",\"wdog\":");
    Serial.print(_safety.isWatchdogTripped() ? "true" : "false");

    // Commanded motor speeds
    Serial.print(",\"motors\":{\"ca\":");
    Serial.print(_motors.getCommandedCrawlerA());
    Serial.print(",\"cb\":");
    Serial.print(_motors.getCommandedCrawlerB());
    Serial.print(",\"ga\":");
    Serial.print(_motors.getCommandedGripperA());
    Serial.print(",\"gb\":");
    Serial.print(_motors.getCommandedGripperB());
    Serial.print("}");

    // Servos
    Serial.print(",\"servos\":{\"s1\":");
    Serial.print(_servos.getAngle(1));
    Serial.print(",\"s2\":");
    Serial.print(_servos.getAngle(2));
    Serial.print(",\"s1_en\":");
    Serial.print(_servos.isEnabled(1) ? "true" : "false");
    Serial.print(",\"s2_en\":");
    Serial.print(_servos.isEnabled(2) ? "true" : "false");
    Serial.print("}");

    // Encoders (Counts & RPM)
    Serial.print(",\"encoders\":{");
    Serial.print("\"enc1\":{\"count\":");
    Serial.print(e1.count);
    Serial.print(",\"rpm\":");
    Serial.print(e1.rpm, 2);
    Serial.print(",\"cal\":");
    Serial.print(e1.calibrated ? "true" : "false");
    Serial.print("},\"enc2\":{\"count\":");
    Serial.print(e2.count);
    Serial.print(",\"rpm\":");
    Serial.print(e2.rpm, 2);
    Serial.print(",\"cal\":");
    Serial.print(e2.calibrated ? "true" : "false");
    Serial.print("},\"wenc1\":{\"count\":");
    Serial.print(ew1.count);
    Serial.print(",\"rpm\":");
    Serial.print(ew1.rpm, 2);
    Serial.print(",\"cal\":");
    Serial.print(ew1.calibrated ? "true" : "false");
    Serial.print("},\"wenc2\":{\"count\":");
    Serial.print(ew2.count);
    Serial.print(",\"rpm\":");
    Serial.print(ew2.rpm, 2);
    Serial.print(",\"cal\":");
    Serial.print(ew2.calibrated ? "true" : "false");
    Serial.print("}}");

    // IMU MPU9250
    Serial.print(",\"imu\":{\"ok\":");
    Serial.print(imu.connected ? "true" : "false");
    Serial.print(",\"addr\":\"0x");
    if (imu.address < 16) Serial.print("0");
    Serial.print(imu.address, HEX);
    Serial.print("\",\"ax\":");
    Serial.print(imu.ax, 2);
    Serial.print(",\"ay\":");
    Serial.print(imu.ay, 2);
    Serial.print(",\"az\":");
    Serial.print(imu.az, 2);
    Serial.print(",\"gx\":");
    Serial.print(imu.gx, 2);
    Serial.print(",\"gy\":");
    Serial.print(imu.gy, 2);
    Serial.print(",\"gz\":");
    Serial.print(imu.gz, 2);
    Serial.print(",\"temp\":");
    Serial.print(imu.temp, 1);
    Serial.print(",\"roll\":");
    Serial.print(imu.roll, 2);
    Serial.print(",\"pitch\":");
    Serial.print(imu.pitch, 2);
    Serial.print("}");

    // INA219 #1
    Serial.print(",\"ina1\":{\"ok\":");
    Serial.print(ina1.connected ? "true" : "false");
    Serial.print(",\"addr\":\"0x");
    if (ina1.address < 16) Serial.print("0");
    Serial.print(ina1.address, HEX);
    Serial.print("\",\"v\":");
    Serial.print(ina1.busVoltageV, 2);
    Serial.print(",\"ma\":");
    Serial.print(ina1.currentMa, 1);
    Serial.print(",\"mw\":");
    Serial.print(ina1.powerMw, 1);
    Serial.print("}");

    // INA219 #2
    Serial.print(",\"ina2\":{\"ok\":");
    Serial.print(ina2.connected ? "true" : "false");
    Serial.print(",\"addr\":\"0x");
    if (ina2.address < 16) Serial.print("0");
    Serial.print(ina2.address, HEX);
    Serial.print("\",\"v\":");
    Serial.print(ina2.busVoltageV, 2);
    Serial.print(",\"ma\":");
    Serial.print(ina2.currentMa, 1);
    Serial.print(",\"mw\":");
    Serial.print(ina2.powerMw, 1);
    Serial.print("}");

    // Detected I2C addresses array
    Serial.print(",\"i2c\":[");
    uint8_t count = _sensors.getDetectedCount();
    for (uint8_t i = 0; i < count; i++) {
        uint8_t a = _sensors.getDetectedAddress(i);
        Serial.print("\"0x");
        if (a < 16) Serial.print("0");
        Serial.print(a, HEX);
        Serial.print("\"");
        if (i < count - 1) Serial.print(",");
    }
    Serial.print("]");

    Serial.println("}");
}
