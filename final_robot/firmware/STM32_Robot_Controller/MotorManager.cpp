/**
 * ============================================================================
 * IIT Mandi Power Line Inspection Crawler Robot - Main Controller Firmware
 * ============================================================================
 * File: MotorManager.cpp
 * ============================================================================
 */

#include "MotorManager.h"

MotorManager::MotorManager()
    : _invertCrawlerA(DEFAULT_CRAWLER_A_INVERT),
      _invertCrawlerB(DEFAULT_CRAWLER_B_INVERT),
      _invertGripperA(DEFAULT_GRIPPER_A_INVERT),
      _invertGripperB(DEFAULT_GRIPPER_B_INVERT),
      _cmdCrawlerA(0),
      _cmdCrawlerB(0),
      _cmdGripperA(0),
      _cmdGripperB(0)
{
}

void MotorManager::begin() {
    // Configure Direction pins as standard digital outputs
    pinMode(PIN_CRAWL_A_DIR, OUTPUT);
    digitalWrite(PIN_CRAWL_A_DIR, LOW);

    pinMode(PIN_CRAWL_B_DIR, OUTPUT);
    digitalWrite(PIN_CRAWL_B_DIR, LOW);

    pinMode(PIN_GRIP_A_DIR, OUTPUT);
    digitalWrite(PIN_GRIP_A_DIR, LOW);

    pinMode(PIN_GRIP_B_DIR, OUTPUT);
    digitalWrite(PIN_GRIP_B_DIR, LOW);

    // Configure PWM pins as outputs
    pinMode(PIN_CRAWL_A_PWM, OUTPUT);
    pinMode(PIN_CRAWL_B_PWM, OUTPUT);
    pinMode(PIN_GRIP_A_PWM, OUTPUT);
    pinMode(PIN_GRIP_B_PWM, OUTPUT);

#if defined(ARDUINO_ARCH_STM32)
    // Configure TIM1 PWM frequency to 20kHz for quiet motor drive
    analogWriteFrequency(MOTOR_PWM_FREQUENCY);
    analogWriteResolution(MOTOR_PWM_RESOLUTION);
#endif

    // Ensure all motors are strictly stationary at startup
    emergencyStopAll();
}

void MotorManager::driveChannel(uint32_t pwmPin, uint32_t dirPin, int16_t speed, bool invert) {
    // Strictly clamp speed to [-100, +100]
    if (speed > MAX_COMMANDED_SPEED) speed = MAX_COMMANDED_SPEED;
    if (speed < -MAX_COMMANDED_SPEED) speed = -MAX_COMMANDED_SPEED;

    if (speed == 0) {
        // Zero speed: set PWM to 0 immediately (MDD10A enters braking state)
        analogWrite(pwmPin, 0);
        digitalWrite(dirPin, LOW);
        return;
    }

    bool forward = (speed > 0);
    if (invert) {
        forward = !forward;
    }

    // Set DIR pin polarity
    digitalWrite(dirPin, forward ? HIGH : LOW);

    // Compute duty cycle: map 1..100 to 1..MOTOR_MAX_DUTY
    uint32_t duty = map(abs(speed), 0, 100, 0, MOTOR_MAX_DUTY);
    analogWrite(pwmPin, duty);
}

void MotorManager::setCrawlerSpeed(int16_t speed) {
    // Command both crawler motors together from joystick
    _cmdCrawlerA = speed;
    _cmdCrawlerB = speed;
    driveChannel(PIN_CRAWL_A_PWM, PIN_CRAWL_A_DIR, _cmdCrawlerA, _invertCrawlerA);
    driveChannel(PIN_CRAWL_B_PWM, PIN_CRAWL_B_DIR, _cmdCrawlerB, _invertCrawlerB);
}

void MotorManager::setCrawlerMotorA(int16_t speed) {
    _cmdCrawlerA = speed;
    driveChannel(PIN_CRAWL_A_PWM, PIN_CRAWL_A_DIR, _cmdCrawlerA, _invertCrawlerA);
}

void MotorManager::setCrawlerMotorB(int16_t speed) {
    _cmdCrawlerB = speed;
    driveChannel(PIN_CRAWL_B_PWM, PIN_CRAWL_B_DIR, _cmdCrawlerB, _invertCrawlerB);
}

void MotorManager::setGripperMotorA(int16_t speed) {
    _cmdGripperA = speed;
    driveChannel(PIN_GRIP_A_PWM, PIN_GRIP_A_DIR, _cmdGripperA, _invertGripperA);
}

void MotorManager::setGripperMotorB(int16_t speed) {
    _cmdGripperB = speed;
    driveChannel(PIN_GRIP_B_PWM, PIN_GRIP_B_DIR, _cmdGripperB, _invertGripperB);
}

void MotorManager::emergencyStopAll() {
    _cmdCrawlerA = 0;
    _cmdCrawlerB = 0;
    _cmdGripperA = 0;
    _cmdGripperB = 0;

    analogWrite(PIN_CRAWL_A_PWM, 0);
    digitalWrite(PIN_CRAWL_A_DIR, LOW);

    analogWrite(PIN_CRAWL_B_PWM, 0);
    digitalWrite(PIN_CRAWL_B_DIR, LOW);

    analogWrite(PIN_GRIP_A_PWM, 0);
    digitalWrite(PIN_GRIP_A_DIR, LOW);

    analogWrite(PIN_GRIP_B_PWM, 0);
    digitalWrite(PIN_GRIP_B_DIR, LOW);
}

void MotorManager::setInversions(bool crawlA, bool crawlB, bool gripA, bool gripB) {
    _invertCrawlerA = crawlA;
    _invertCrawlerB = crawlB;
    _invertGripperA = gripA;
    _invertGripperB = gripB;

    // Re-apply current speeds with updated polarity
    driveChannel(PIN_CRAWL_A_PWM, PIN_CRAWL_A_DIR, _cmdCrawlerA, _invertCrawlerA);
    driveChannel(PIN_CRAWL_B_PWM, PIN_CRAWL_B_DIR, _cmdCrawlerB, _invertCrawlerB);
    driveChannel(PIN_GRIP_A_PWM, PIN_GRIP_A_DIR, _cmdGripperA, _invertGripperA);
    driveChannel(PIN_GRIP_B_PWM, PIN_GRIP_B_DIR, _cmdGripperB, _invertGripperB);
}
