/**
 * ============================================================================
 * IIT Mandi Power Line Inspection Crawler Robot - Main Controller Firmware
 * ============================================================================
 * File: SensorManager.cpp
 * ============================================================================
 */

#include "SensorManager.h"
#include <math.h>

// MPU9250 Register Map
#define MPU_REG_SMPLRT_DIV     0x19
#define MPU_REG_CONFIG         0x1A
#define MPU_REG_GYRO_CONFIG    0x1B
#define MPU_REG_ACCEL_CONFIG   0x1C
#define MPU_REG_ACCEL_XOUT_H   0x3B
#define MPU_REG_TEMP_OUT_H     0x41
#define MPU_REG_GYRO_XOUT_H    0x43
#define MPU_REG_PWR_MGMT_1     0x6B
#define MPU_REG_WHO_AM_I       0x75

// INA219 Register Map
#define INA_REG_CONFIG         0x00
#define INA_REG_SHUNTVOLTAGE   0x01
#define INA_REG_BUSVOLTAGE     0x02
#define INA_REG_POWER          0x03
#define INA_REG_CURRENT        0x04
#define INA_REG_CALIBRATION    0x05

// Helper function to read 16-bit register over I2C
static int16_t readRegister16(uint8_t addr, uint8_t reg) {
    if (addr == 0) return 0;
    Wire.beginTransmission(addr);
    Wire.write(reg);
    if (Wire.endTransmission(false) != 0) {
        return 0;
    }
    if (Wire.requestFrom((uint8_t)addr, (uint8_t)2) != 2) {
        return 0;
    }
    uint8_t msb = Wire.read();
    uint8_t lsb = Wire.read();
    return (int16_t)((msb << 8) | lsb);
}

// Helper function to write 16-bit register over I2C
static bool writeRegister16(uint8_t addr, uint8_t reg, uint16_t val) {
    if (addr == 0) return false;
    Wire.beginTransmission(addr);
    Wire.write(reg);
    Wire.write((uint8_t)(val >> 8));
    Wire.write((uint8_t)(val & 0xFF));
    return (Wire.endTransmission() == 0);
}

SensorManager::SensorManager()
    : _detectedCount(0),
      _lastReadTime(0),
      _mpuAddress(0),
      _ina1Address(0),
      _ina2Address(0),
      _rollFiltered(0.0f),
      _pitchFiltered(0.0f),
      _lastImuFilterTime(0)
{
    memset(&_imuData, 0, sizeof(_imuData));
    memset(&_ina1Data, 0, sizeof(_ina1Data));
    memset(&_ina2Data, 0, sizeof(_ina2Data));
}

void SensorManager::scanI2CBus() {
    _detectedCount = 0;
    _mpuAddress = 0;
    _ina1Address = 0;
    _ina2Address = 0;

    for (uint8_t addr = 1; addr < 127; addr++) {
        Wire.beginTransmission(addr);
        uint8_t error = Wire.endTransmission();
        if (error == 0) {
            if (_detectedCount < 16) {
                _detectedAddresses[_detectedCount++] = addr;
            }

            // Identify MPU9250 (0x68 or 0x69)
            if ((addr == MPU9250_DEFAULT_ADDR || addr == MPU9250_ALT_ADDR) && _mpuAddress == 0) {
                _mpuAddress = addr;
            }
            // Identify INA219 sensors (0x40 to 0x4F range)
            else if (addr >= 0x40 && addr <= 0x4F) {
                if (_ina1Address == 0) {
                    _ina1Address = addr;
                } else if (_ina2Address == 0) {
                    _ina2Address = addr;
                }
            }
        }
    }
}

void SensorManager::begin() {
    // Configure internal pullups on PB8 (SCL) and PB9 (SDA)
    // Ensures bus is not stuck floating even if external resistors or sensors are disconnected
    pinMode(PIN_I2C_SCL, INPUT_PULLUP);
    pinMode(PIN_I2C_SDA, INPUT_PULLUP);
    delay(10);

    // Check if lines are held LOW before touching I2C hardware
    if (digitalRead(PIN_I2C_SCL) == LOW || digitalRead(PIN_I2C_SDA) == LOW) {
        _imuData.connected = false;
        _ina1Data.connected = false;
        _ina2Data.connected = false;
        return; // Safely skip I2C initialization if bus is grounded or unplugged
    }

#if defined(ARDUINO_ARCH_STM32)
    Wire.setSCL(PIN_I2C_SCL);
    Wire.setSDA(PIN_I2C_SDA);
#endif
    Wire.begin();
    Wire.setClock(100000); // 100 kHz standard mode for robust cable noise margins

    // Scan the bus dynamically
    scanI2CBus();

    // Initialize MPU9250 if detected
    if (_mpuAddress != 0) {
        _imuData.connected = initMPU9250(_mpuAddress);
        _imuData.address = _mpuAddress;
    } else {
        _imuData.connected = false;
        _imuData.address = 0;
    }

    // Initialize INA219 #1 if detected
    if (_ina1Address != 0) {
        _ina1Data.connected = initINA219(_ina1Address);
        _ina1Data.address = _ina1Address;
    } else {
        _ina1Data.connected = false;
        _ina1Data.address = 0;
    }

    // Initialize INA219 #2 if detected
    if (_ina2Address != 0) {
        _ina2Data.connected = initINA219(_ina2Address);
        _ina2Data.address = _ina2Address;
    } else {
        _ina2Data.connected = false;
        _ina2Data.address = 0;
    }

    _lastReadTime = millis();
    _lastImuFilterTime = millis();
}

bool SensorManager::initMPU9250(uint8_t addr) {
    if (addr == 0) return false;

    // Wake up MPU9250 (Clear sleep bit in PWR_MGMT_1, set auto-select clock)
    Wire.beginTransmission(addr);
    Wire.write(MPU_REG_PWR_MGMT_1);
    Wire.write(0x01); // Auto select clock source (PLL with X axis gyro reference)
    if (Wire.endTransmission() != 0) return false;
    delay(10);

    // Set sample rate divider (SMPLRT_DIV = 0 for 1kHz)
    Wire.beginTransmission(addr);
    Wire.write(MPU_REG_SMPLRT_DIV);
    Wire.write(0x00);
    Wire.endTransmission();

    // Configure DLPF: ~41 Hz
    Wire.beginTransmission(addr);
    Wire.write(MPU_REG_CONFIG);
    Wire.write(0x03);
    Wire.endTransmission();

    // Set Gyro full scale range to +/- 250 dps (FS_SEL = 0)
    Wire.beginTransmission(addr);
    Wire.write(MPU_REG_GYRO_CONFIG);
    Wire.write(0x00);
    Wire.endTransmission();

    // Set Accel full scale range to +/- 2g (AFS_SEL = 0)
    Wire.beginTransmission(addr);
    Wire.write(MPU_REG_ACCEL_CONFIG);
    Wire.write(0x00);
    Wire.endTransmission();

    return true;
}

bool SensorManager::initINA219(uint8_t addr) {
    if (addr == 0) return false;

    // Configure INA219 for 32V, +/- 320mV Range, 12-bit, continuous mode
    uint16_t config = 0x399F;
    if (!writeRegister16(addr, INA_REG_CONFIG, config)) {
        return false;
    }

    // Calibration register for 0.1 ohm shunt:
    writeRegister16(addr, INA_REG_CALIBRATION, 0x1000);
    return true;
}

void SensorManager::readMPU9250() {
    if (!_imuData.connected || _mpuAddress == 0) return;

    Wire.beginTransmission(_mpuAddress);
    Wire.write(MPU_REG_ACCEL_XOUT_H);
    if (Wire.endTransmission(false) != 0) {
        _imuData.connected = false;
        return;
    }

    // Burst read 14 bytes: 6 accel, 2 temp, 6 gyro
    if (Wire.requestFrom(_mpuAddress, (uint8_t)14) != 14) {
        _imuData.connected = false;
        return;
    }

    int16_t axRaw = (int16_t)((Wire.read() << 8) | Wire.read());
    int16_t ayRaw = (int16_t)((Wire.read() << 8) | Wire.read());
    int16_t azRaw = (int16_t)((Wire.read() << 8) | Wire.read());
    int16_t tempRaw = (int16_t)((Wire.read() << 8) | Wire.read());
    int16_t gxRaw = (int16_t)((Wire.read() << 8) | Wire.read());
    int16_t gyRaw = (int16_t)((Wire.read() << 8) | Wire.read());
    int16_t gzRaw = (int16_t)((Wire.read() << 8) | Wire.read());

    // Accel +/- 2g range: 16384 LSB/g -> m/s^2 (g * 9.80665)
    _imuData.ax = ((float)axRaw / 16384.0f) * 9.80665f;
    _imuData.ay = ((float)ayRaw / 16384.0f) * 9.80665f;
    _imuData.az = ((float)azRaw / 16384.0f) * 9.80665f;

    // Gyro +/- 250 dps range: 131.0 LSB/(deg/s)
    _imuData.gx = (float)gxRaw / 131.0f;
    _imuData.gy = (float)gyRaw / 131.0f;
    _imuData.gz = (float)gzRaw / 131.0f;

    // Temperature in deg C
    _imuData.temp = ((float)tempRaw / 333.87f) + 21.0f;

    // Attitude complementary filter
    uint32_t now = millis();
    float dt = (float)(now - _lastImuFilterTime) / 1000.0f;
    _lastImuFilterTime = now;
    if (dt <= 0.0f || dt > 0.5f) dt = 0.05f;

    float accRoll = atan2f(_imuData.ay, _imuData.az) * 57.2957795f;
    float accPitch = atan2f(-_imuData.ax, sqrtf(_imuData.ay * _imuData.ay + _imuData.az * _imuData.az)) * 57.2957795f;

    _rollFiltered = 0.96f * (_rollFiltered + _imuData.gx * dt) + 0.04f * accRoll;
    _pitchFiltered = 0.96f * (_pitchFiltered + _imuData.gy * dt) + 0.04f * accPitch;

    _imuData.roll = _rollFiltered;
    _imuData.pitch = _pitchFiltered;
}

void SensorManager::readINA219(uint8_t addr, INA219Data &data) {
    if (!data.connected || addr == 0) return;

    int16_t busRaw = readRegister16(addr, INA_REG_BUSVOLTAGE);
    data.busVoltageV = (float)((busRaw >> 3) * 4) / 1000.0f;

    int16_t shuntRaw = readRegister16(addr, INA_REG_SHUNTVOLTAGE);
    data.shuntVoltageMv = (float)shuntRaw * 0.01f;

    int16_t currentRaw = readRegister16(addr, INA_REG_CURRENT);
    if (currentRaw != 0) {
        data.currentMa = (float)currentRaw * 0.1f;
    } else {
        data.currentMa = data.shuntVoltageMv * 10.0f;
    }

    data.powerMw = data.busVoltageV * data.currentMa;
    if (data.powerMw < 0.0f) data.powerMw = 0.0f;
}

void SensorManager::update() {
    uint32_t now = millis();
    if (now - _lastReadTime < SENSOR_READ_INTERVAL_MS) {
        return;
    }
    _lastReadTime = now;

    readMPU9250();
    readINA219(_ina1Address, _ina1Data);
    readINA219(_ina2Address, _ina2Data);
}
