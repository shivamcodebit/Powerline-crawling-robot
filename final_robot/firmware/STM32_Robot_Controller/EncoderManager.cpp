/**
 * ============================================================================
 * IIT Mandi Power Line Inspection Crawler Robot - Main Controller Firmware
 * ============================================================================
 * File: EncoderManager.cpp
 * ============================================================================
 */

#include "EncoderManager.h"

// Quadrature decoding transition table (Gray code)
// Index: [oldA, oldB, newA, newB] -> transition direction (-1, 0, +1)
static const int8_t QUAD_TABLE[16] = {
     0, -1,  1,  0,
     1,  0,  0, -1,
    -1,  0,  0,  1,
     0,  1, -1,  0
};

// Global volatile storage for interrupt service routines
static volatile int32_t g_countEnc1 = 0;
static volatile int32_t g_countEnc2 = 0;
static volatile int32_t g_countWEnc1 = 0;
static volatile int32_t g_countWEnc2 = 0;

static volatile uint8_t g_stateEnc2 = 0;
static volatile uint8_t g_stateWEnc1 = 0;
static volatile uint8_t g_stateWEnc2 = 0;

#if defined(ARDUINO_ARCH_STM32)
static uint16_t g_tim4LastRaw = 0;
#endif

// ----------------------------------------------------------------------------
// INTERRUPT SERVICE ROUTINES
// ----------------------------------------------------------------------------

void EncoderManager::isrEnc2() {
    uint8_t a = (digitalRead(PIN_ENC2_A) == HIGH) ? 1 : 0;
    uint8_t b = (digitalRead(PIN_ENC2_B) == HIGH) ? 1 : 0;
    uint8_t state = (g_stateEnc2 & 0x03) << 2 | (a << 1) | b;
    g_countEnc2 += QUAD_TABLE[state];
    g_stateEnc2 = state;
}

void EncoderManager::isrWEnc1() {
    uint8_t a = (digitalRead(PIN_WENC1_A) == HIGH) ? 1 : 0;
    uint8_t b = (digitalRead(PIN_WENC1_B) == HIGH) ? 1 : 0;
    uint8_t state = (g_stateWEnc1 & 0x03) << 2 | (a << 1) | b;
    g_countWEnc1 += QUAD_TABLE[state];
    g_stateWEnc1 = state;
}

void EncoderManager::isrWEnc2() {
    uint8_t a = (digitalRead(PIN_WENC2_A) == HIGH) ? 1 : 0;
    uint8_t b = (digitalRead(PIN_WENC2_B) == HIGH) ? 1 : 0;
    uint8_t state = (g_stateWEnc2 & 0x03) << 2 | (a << 1) | b;
    g_countWEnc2 += QUAD_TABLE[state];
    g_stateWEnc2 = state;
}

// ----------------------------------------------------------------------------
// CLASS IMPLEMENTATION
// ----------------------------------------------------------------------------

EncoderManager::EncoderManager()
    : _lastRpmTime(0),
      _prevCount1(0),
      _prevCount2(0),
      _prevCountW1(0),
      _prevCountW2(0),
      _rpm1(0.0f),
      _rpm2(0.0f),
      _rpmW1(0.0f),
      _rpmW2(0.0f),
      _cpr1(DEFAULT_ENC1_CPR),
      _cpr2(DEFAULT_ENC2_CPR),
      _cprW1(DEFAULT_WENC1_CPR),
      _cprW2(DEFAULT_WENC2_CPR),
      _gearRatio1(DEFAULT_ENC1_GEAR_RATIO),
      _gearRatio2(DEFAULT_ENC2_GEAR_RATIO),
      _gearRatioW1(DEFAULT_WENC1_GEAR_RATIO),
      _gearRatioW2(DEFAULT_WENC2_GEAR_RATIO),
      _multiplier1(ENCODER_QUAD_MULTIPLIER),
      _multiplier2(ENCODER_QUAD_MULTIPLIER),
      _multiplierW1(ENCODER_QUAD_MULTIPLIER),
      _multiplierW2(ENCODER_QUAD_MULTIPLIER)
{
}

void EncoderManager::initHardwareTim4Encoder() {
#if defined(ARDUINO_ARCH_STM32)
    // Enable Clocks for TIM4 and GPIOB
    __HAL_RCC_TIM4_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();

    // Configure PB6 and PB7 as Alternate Function 2 (TIM4 CH1 & CH2) with pullups
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin = GPIO_PIN_6 | GPIO_PIN_7;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF2_TIM4;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

    // Initialize TIM4 in quadrature encoder mode 3 (both channels TI1 and TI2)
    TIM_Encoder_InitTypeDef sConfig = {0};
    TIM_HandleTypeDef htim4 = {0};
    htim4.Instance = TIM4;
    htim4.Init.Prescaler = 0;
    htim4.Init.CounterMode = TIM_COUNTERMODE_UP;
    htim4.Init.Period = 0xFFFF;
    htim4.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    htim4.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;

    sConfig.EncoderMode = TIM_ENCODERMODE_TI12;
    sConfig.IC1Polarity = TIM_ICPOLARITY_RISING;
    sConfig.IC1Selection = TIM_ICSELECTION_DIRECTTI;
    sConfig.IC1Prescaler = TIM_ICPSC_DIV1;
    sConfig.IC1Filter = 4;
    sConfig.IC2Polarity = TIM_ICPOLARITY_RISING;
    sConfig.IC2Selection = TIM_ICSELECTION_DIRECTTI;
    sConfig.IC2Prescaler = TIM_ICPSC_DIV1;
    sConfig.IC2Filter = 4;

    HAL_TIM_Encoder_Init(&htim4, &sConfig);
    HAL_TIM_Encoder_Start(&htim4, TIM_CHANNEL_ALL);

    TIM4->CNT = 0;
    g_tim4LastRaw = 0;
#else
    pinMode(PIN_ENC1_A, INPUT_PULLUP);
    pinMode(PIN_ENC1_B, INPUT_PULLUP);
#endif
}

int32_t EncoderManager::readTim4Encoder() {
#if defined(ARDUINO_ARCH_STM32)
    noInterrupts();
    uint16_t cur = (uint16_t)(TIM4->CNT);
    int16_t delta = (int16_t)(cur - g_tim4LastRaw);
    g_tim4LastRaw = cur;
    g_countEnc1 += delta;
    int32_t total = g_countEnc1;
    interrupts();
    return total;
#else
    int32_t count;
    noInterrupts();
    count = g_countEnc1;
    interrupts();
    return count;
#endif
}

void EncoderManager::begin() {
    // 1. Encoder 1: Hardware Timer TIM4
    initHardwareTim4Encoder();

    // 2. Encoder 2: PC6 (EXTI6) & PC7 (EXTI7)
    pinMode(PIN_ENC2_A, INPUT_PULLUP);
    pinMode(PIN_ENC2_B, INPUT_PULLUP);
    g_stateEnc2 = ((digitalRead(PIN_ENC2_A) == HIGH ? 1 : 0) << 1) | 
                  (digitalRead(PIN_ENC2_B) == HIGH ? 1 : 0);
    attachInterrupt(digitalPinToInterrupt(PIN_ENC2_A), isrEnc2, CHANGE);
    attachInterrupt(digitalPinToInterrupt(PIN_ENC2_B), isrEnc2, CHANGE);

    // 3. Wheel Encoder 1: PB4 (EXTI4) & PB5 (EXTI5)
    pinMode(PIN_WENC1_A, INPUT_PULLUP);
    pinMode(PIN_WENC1_B, INPUT_PULLUP);
    g_stateWEnc1 = ((digitalRead(PIN_WENC1_A) == HIGH ? 1 : 0) << 1) | 
                   (digitalRead(PIN_WENC1_B) == HIGH ? 1 : 0);
    attachInterrupt(digitalPinToInterrupt(PIN_WENC1_A), isrWEnc1, CHANGE);
    attachInterrupt(digitalPinToInterrupt(PIN_WENC1_B), isrWEnc1, CHANGE);

    // 4. Wheel Encoder 2: PA15 (EXTI15) & PB3 (EXTI3)
    pinMode(PIN_WENC2_A, INPUT_PULLUP);
    pinMode(PIN_WENC2_B, INPUT_PULLUP);
    g_stateWEnc2 = ((digitalRead(PIN_WENC2_A) == HIGH ? 1 : 0) << 1) | 
                   (digitalRead(PIN_WENC2_B) == HIGH ? 1 : 0);
    attachInterrupt(digitalPinToInterrupt(PIN_WENC2_A), isrWEnc2, CHANGE);
    attachInterrupt(digitalPinToInterrupt(PIN_WENC2_B), isrWEnc2, CHANGE);

    _lastRpmTime = millis();
}

int32_t EncoderManager::getCount1() {
    return readTim4Encoder();
}

int32_t EncoderManager::getCount2() {
    int32_t val;
    noInterrupts();
    val = g_countEnc2;
    interrupts();
    return val;
}

int32_t EncoderManager::getCountW1() {
    int32_t val;
    noInterrupts();
    val = g_countWEnc1;
    interrupts();
    return val;
}

int32_t EncoderManager::getCountW2() {
    int32_t val;
    noInterrupts();
    val = g_countWEnc2;
    interrupts();
    return val;
}

void EncoderManager::resetCounts() {
    noInterrupts();
#if defined(ARDUINO_ARCH_STM32)
    TIM4->CNT = 0;
    g_tim4LastRaw = 0;
#endif
    g_countEnc1 = 0;
    g_countEnc2 = 0;
    g_countWEnc1 = 0;
    g_countWEnc2 = 0;
    interrupts();

    _prevCount1 = 0;
    _prevCount2 = 0;
    _prevCountW1 = 0;
    _prevCountW2 = 0;
    _rpm1 = 0.0f;
    _rpm2 = 0.0f;
    _rpmW1 = 0.0f;
    _rpmW2 = 0.0f;
    _lastRpmTime = millis();
}

void EncoderManager::setCalibration(uint8_t encId, float cpr, float gearRatio, uint8_t multiplier) {
    if (gearRatio <= 0.0f) gearRatio = 1.0f;
    if (multiplier == 0) multiplier = 4;

    switch (encId) {
        case 1:
            _cpr1 = cpr;
            _gearRatio1 = gearRatio;
            _multiplier1 = multiplier;
            break;
        case 2:
            _cpr2 = cpr;
            _gearRatio2 = gearRatio;
            _multiplier2 = multiplier;
            break;
        case 3:
            _cprW1 = cpr;
            _gearRatioW1 = gearRatio;
            _multiplierW1 = multiplier;
            break;
        case 4:
            _cprW2 = cpr;
            _gearRatioW2 = gearRatio;
            _multiplierW2 = multiplier;
            break;
        default:
            break;
    }
}

void EncoderManager::update() {
    uint32_t now = millis();
    uint32_t dt = now - _lastRpmTime;
    if (dt < RPM_CALCULATION_INTERVAL_MS) {
        return;
    }
    _lastRpmTime = now;

    int32_t c1 = getCount1();
    int32_t c2 = getCount2();
    int32_t cw1 = getCountW1();
    int32_t cw2 = getCountW2();

    int32_t delta1 = c1 - _prevCount1;
    int32_t delta2 = c2 - _prevCount2;
    int32_t deltaW1 = cw1 - _prevCountW1;
    int32_t deltaW2 = cw2 - _prevCountW2;

    _prevCount1 = c1;
    _prevCount2 = c2;
    _prevCountW1 = cw1;
    _prevCountW2 = cw2;

    float dtMin = ((float)dt) / 60000.0f;

    // Encoder 1 RPM
    if (_cpr1 > 0.0f && dtMin > 0.0f) {
        float effectivePpr = _cpr1 * _multiplier1 * _gearRatio1;
        _rpm1 = ((float)delta1 / effectivePpr) / dtMin;
    } else {
        _rpm1 = 0.0f; // Uncalibrated
    }

    // Encoder 2 RPM
    if (_cpr2 > 0.0f && dtMin > 0.0f) {
        float effectivePpr = _cpr2 * _multiplier2 * _gearRatio2;
        _rpm2 = ((float)delta2 / effectivePpr) / dtMin;
    } else {
        _rpm2 = 0.0f; // Uncalibrated
    }

    // Wheel Encoder 1 RPM
    if (_cprW1 > 0.0f && dtMin > 0.0f) {
        float effectivePpr = _cprW1 * _multiplierW1 * _gearRatioW1;
        _rpmW1 = ((float)deltaW1 / effectivePpr) / dtMin;
    } else {
        _rpmW1 = 0.0f; // Uncalibrated
    }

    // Wheel Encoder 2 RPM
    if (_cprW2 > 0.0f && dtMin > 0.0f) {
        float effectivePpr = _cprW2 * _multiplierW2 * _gearRatioW2;
        _rpmW2 = ((float)deltaW2 / effectivePpr) / dtMin;
    } else {
        _rpmW2 = 0.0f; // Uncalibrated
    }
}

EncoderData EncoderManager::getEncoder1Data() {
    EncoderData data;
    data.count = getCount1();
    data.rpm = _rpm1;
    data.calibrated = (_cpr1 > 0.0f);
    data.cpr = _cpr1;
    data.gearRatio = _gearRatio1;
    data.multiplier = _multiplier1;
    return data;
}

EncoderData EncoderManager::getEncoder2Data() {
    EncoderData data;
    data.count = getCount2();
    data.rpm = _rpm2;
    data.calibrated = (_cpr2 > 0.0f);
    data.cpr = _cpr2;
    data.gearRatio = _gearRatio2;
    data.multiplier = _multiplier2;
    return data;
}

EncoderData EncoderManager::getWheelEncoder1Data() {
    EncoderData data;
    data.count = getCountW1();
    data.rpm = _rpmW1;
    data.calibrated = (_cprW1 > 0.0f);
    data.cpr = _cprW1;
    data.gearRatio = _gearRatioW1;
    data.multiplier = _multiplierW1;
    return data;
}

EncoderData EncoderManager::getWheelEncoder2Data() {
    EncoderData data;
    data.count = getCountW2();
    data.rpm = _rpmW2;
    data.calibrated = (_cprW2 > 0.0f);
    data.cpr = _cprW2;
    data.gearRatio = _gearRatioW2;
    data.multiplier = _multiplierW2;
    return data;
}
