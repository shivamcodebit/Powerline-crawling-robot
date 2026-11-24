/**
 * ============================================================================
 * IIT Mandi Power Line Inspection Crawler Robot - Main Controller Firmware
 * ============================================================================
 * File: SensorManager.h
 * 
 * Hardware I2C Sensor Manager for STM32F446RE:
 * - MPU9250 9-DoF IMU (Accelerometer, Gyroscope, Temperature, Roll, Pitch)
 * - Dual INA219 Current & Voltage Monitors (Bus Voltage, Current, Power)
 * 
 * Features:
 * - Startup I2C bus scanner dynamically discovers device addresses
 * - Auto-binding to detected INA219 addresses
 * - Non-blocking I2C error recovery (no infinite MCU lockups)
 * ============================================================================
 */

#ifndef SENSOR_MANAGER_H
#define SENSOR_MANAGER_H

#include <Arduino.h>
#include <Wire.h>
#include "PinDefinitions.h"
#include "Config.h"

struct IMUData {
    bool connected;
    uint8_t address;
    float ax;       // m/s^2
    float ay;       // m/s^2
    float az;       // m/s^2
    float gx;       // deg/s
    float gy;       // deg/s
    float gz;       // deg/s
    float temp;     // deg C
    float roll;     // deg
    float pitch;    // deg
};

struct INA219Data {
    bool connected;
    uint8_t address;
    float busVoltageV;  // Volts
    float shuntVoltageMv; // Millivolts
    float currentMa;    // Milliamps
    float powerMw;      // Milliwatts
};

class SensorManager {
public:
    SensorManager();

    // Initialize I2C bus, run address scan, configure devices
    void begin();

    // Non-blocking update called in main loop
    void update();

    // Get sensor telemetry snapshots
    const IMUData& getIMUData() const { return _imuData; }
    const INA219Data& getINA1Data() const { return _ina1Data; }
    const INA219Data& getINA2Data() const { return _ina2Data; }

    // I2C scan report
    uint8_t getDetectedCount() const { return _detectedCount; }
    uint8_t getDetectedAddress(uint8_t index) const {
        return (index < _detectedCount) ? _detectedAddresses[index] : 0;
    }

private:
    IMUData _imuData;
    INA219Data _ina1Data;
    INA219Data _ina2Data;

    uint8_t _detectedAddresses[16];
    uint8_t _detectedCount;

    uint32_t _lastReadTime;

    // Device addresses
    uint8_t _mpuAddress;
    uint8_t _ina1Address;
    uint8_t _ina2Address;

    // Internal drivers
    void scanI2CBus();
    bool initMPU9250(uint8_t addr);
    void readMPU9250();

    bool initINA219(uint8_t addr);
    void readINA219(uint8_t addr, INA219Data &data);

    // Complementary filter state for attitude
    float _rollFiltered;
    float _pitchFiltered;
    uint32_t _lastImuFilterTime;
};

#endif // SENSOR_MANAGER_H
