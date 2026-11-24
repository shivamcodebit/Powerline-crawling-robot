/**
 * ============================================================================
 * IIT Mandi Power Line Inspection Crawler Robot - Main Controller Firmware
 * ============================================================================
 * File: MotorManager.h
 * 
 * Cytron MDD10A Dual Motor Driver Controller (Sign-Magnitude PWM + DIR).
 * Manages Crawler Motors A & B and Gripper Motors A & B.
 * ============================================================================
 */

#ifndef MOTOR_MANAGER_H
#define MOTOR_MANAGER_H

#include <Arduino.h>
#include "PinDefinitions.h"
#include "Config.h"

class MotorManager {
public:
    MotorManager();

    // Initialize GPIO pins, set initial stop states
    void begin();

    // Set crawler motors speed (-100 to +100)
    // Both motors receive the commanded speed; inversion settings determine electrical DIR polarity
    void setCrawlerSpeed(int16_t speed);

    // Set individual crawler motor speeds directly if needed
    void setCrawlerMotorA(int16_t speed);
    void setCrawlerMotorB(int16_t speed);

    // Set gripper motors independently (-100 to +100)
    void setGripperMotorA(int16_t speed);
    void setGripperMotorB(int16_t speed);

    // Immediate emergency stop - sets all PWM to 0 and stops all 4 motors
    void emergencyStopAll();

    // Direction inversion getters and setters
    void setInversions(bool crawlA, bool crawlB, bool gripA, bool gripB);
    bool getInvertCrawlerA() const { return _invertCrawlerA; }
    bool getInvertCrawlerB() const { return _invertCrawlerB; }
    bool getInvertGripperA() const { return _invertGripperA; }
    bool getInvertGripperB() const { return _invertGripperB; }

    // Commanded speed getters (-100 to +100)
    int16_t getCommandedCrawlerA() const { return _cmdCrawlerA; }
    int16_t getCommandedCrawlerB() const { return _cmdCrawlerB; }
    int16_t getCommandedGripperA() const { return _cmdGripperA; }
    int16_t getCommandedGripperB() const { return _cmdGripperB; }

private:
    // Helper to drive a single Cytron channel (PWM pin + DIR pin)
    void driveChannel(uint32_t pwmPin, uint32_t dirPin, int16_t speed, bool invert);

    // Inversion configuration
    bool _invertCrawlerA;
    bool _invertCrawlerB;
    bool _invertGripperA;
    bool _invertGripperB;

    // Current commanded percentages
    int16_t _cmdCrawlerA;
    int16_t _cmdCrawlerB;
    int16_t _cmdGripperA;
    int16_t _cmdGripperB;
};

#endif // MOTOR_MANAGER_H
