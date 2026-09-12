#pragma once

#include <stdint.h>

class FloorHeatPanel
{
public:
    static constexpr uint8_t ZONE_COUNT = 4;
    static constexpr uint8_t SPEED_COUNT = 4;

    struct State
    {
        bool power = false;

        float currentTemp = 24.0f;
        float setTemp = 24.0f;

        uint8_t mode = 0;
        uint8_t speed = 0;

        bool validSpeed[SPEED_COUNT] = {};
    };

public:
    FloorHeatPanel();

    // -------------------------------------------------
    // Zone selection
    // -------------------------------------------------

    void setCurrent(uint8_t index);
    uint8_t current() const;

    State &get(uint8_t index);
    const State &get(uint8_t index) const;

    State &currentState();
    const State &currentState() const;

    // -------------------------------------------------
    // Power
    // -------------------------------------------------

    void setPower(bool value);
    bool power() const;
    void togglePower();

    // -------------------------------------------------
    // Temperature
    // -------------------------------------------------

    void setCurrentTemp(float value);
    float currentTemp() const;

    void setSetTemp(float value);
    float setTemp() const;

    void increaseTemp(float amount = 1.0f);
    void decreaseTemp(float amount = 1.0f);

    // -------------------------------------------------
    // Mode
    // -------------------------------------------------

    void setMode(uint8_t mode);
    uint8_t mode() const;

    // -------------------------------------------------
    // Speed
    // -------------------------------------------------

    void setSpeed(uint8_t speed);
    uint8_t speed() const;

    void setSpeedValid(uint8_t speed, bool valid);
    bool isSpeedValid(uint8_t speed) const;

private:
    State zones_[ZONE_COUNT];

    uint8_t currentZone_;
};