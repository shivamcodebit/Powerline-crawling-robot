/**
 * ============================================================================
 * IIT Mandi Power Line Inspection Crawler Robot - Main Controller Firmware
 * ============================================================================
 * File: EncoderManager.h
 * 
 * Quadrature Encoder Reader for 4 Channels:
 * 1. Encoder 1:       PB6 (Ch A), PB7 (Ch B)
 * 2. Encoder 2:       PC6 (Ch A), PC7 (Ch B)
 * 3. Wheel Encoder 1: PB4 (Ch A), PB5 (Ch B)
 * 4. Wheel Encoder 2: PA15 (Ch A), PB3 (Ch B)
 * 
 * Hardware Timer & EXTI Conflict Analysis:
 * - PC6/PC7 and PB4/PB5 both map to TIM3_CH1 and TIM3_CH2 in STM32 hardware.
 *   Therefore, both CANNOT use hardware timer encoder mode simultaneously.
 * - PB6 and PC6 share EXTI line 6; PB7 and PC7 share EXTI line 7.
 *   Therefore, EXTI interrupts cannot be routed to both Port B and Port C simultaneously.
 * - Resolution:
 *   Encoder 1 (PB6/PB7) utilizes TIM4 Hardware Timer in Encoder Mode (AF02).
 *   Encoder 2 (PC6/PC7), Wheel Encoder 1 (PB4/PB5), and Wheel Encoder 2 (PA15/PB3)
 *   utilize dedicated, non-conflicting EXTI pin-change interrupts with 4x Gray-code decoding.
 * ============================================================================
 */

#ifndef ENCODER_MANAGER_H
#define ENCODER_MANAGER_H

#include <Arduino.h>
#include "PinDefinitions.h"
#include "Config.h"

struct EncoderData {
    int32_t count;
    float rpm;
    bool calibrated;
    float cpr;
    float gearRatio;
    uint8_t multiplier;
};

class EncoderManager {
public:
    EncoderManager();

    // Initialize all 4 encoder channels
    void begin();

    // Update RPM calculation at periodic intervals
    void update();

    // Reset all encoder counts to 0
    void resetCounts();

    // Set calibration parameters
    void setCalibration(uint8_t encId, float cpr, float gearRatio, uint8_t multiplier = 4);

    // Get telemetry data for each encoder
    EncoderData getEncoder1Data();
    EncoderData getEncoder2Data();
    EncoderData getWheelEncoder1Data();
    EncoderData getWheelEncoder2Data();

    // Raw count getters (thread/interrupt safe)
    int32_t getCount1();
    int32_t getCount2();
    int32_t getCountW1();
    int32_t getCountW2();

    // Static ISR handlers called by Arduino attachInterrupt
    static void isrEnc2();
    static void isrWEnc1();
    static void isrWEnc2();

private:
    uint32_t _lastRpmTime;

    // Previous count snapshots for delta-t RPM calculation
    int32_t _prevCount1;
    int32_t _prevCount2;
    int32_t _prevCountW1;
    int32_t _prevCountW2;

    // Calculated RPM values
    float _rpm1;
    float _rpm2;
    float _rpmW1;
    float _rpmW2;

    // Calibration settings
    float _cpr1;
    float _cpr2;
    float _cprW1;
    float _cprW2;

    float _gearRatio1;
    float _gearRatio2;
    float _gearRatioW1;
    float _gearRatioW2;

    uint8_t _multiplier1;
    uint8_t _multiplier2;
    uint8_t _multiplierW1;
    uint8_t _multiplierW2;

    // Hardware TIM4 encoder setup helper for PB6/PB7
    void initHardwareTim4Encoder();
    int32_t readTim4Encoder();
};

#endif // ENCODER_MANAGER_H
