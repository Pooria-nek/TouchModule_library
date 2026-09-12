#pragma once

#include <stdint.h>

class HVACPanel
{
public:
    static constexpr uint8_t HVAC_COUNT = 8;
    static constexpr uint8_t MODE_COUNT = 4;
    static constexpr uint8_t FAN_COUNT  = 4;

    struct State
    {
        bool power = false;

        float currentTemp = 24.0f;
        float setTemp = 24.0f;

        uint8_t mode = 0;
        uint8_t fan = 0;

        bool validMode[MODE_COUNT] = {};
        bool validFan[FAN_COUNT] = {};
    };

public:
    HVACPanel();

    // -------------------------------------------------
    // HVAC selection
    // -------------------------------------------------

    void setCurrent(uint8_t index);
    uint8_t current() const;

    // Wrap-around zone navigation (e.g. "prev/next AC" buttons on a panel).
    void next();
    void previous();

    State &currentState();
    const State &currentState() const;

    State &get(uint8_t index);
    const State &get(uint8_t index) const;

    // -------------------------------------------------
    // Power
    // -------------------------------------------------

    void setPower(bool power);
    bool power() const;
    void togglePower();

    // -------------------------------------------------
    // Temperature
    // -------------------------------------------------

    void setCurrentTemp(float temp);
    float currentTemp() const;

    void setSetTemp(float temp);
    float setTemp() const;

    void increaseTemp(float amount = 1.0f);
    void decreaseTemp(float amount = 1.0f);

    // -------------------------------------------------
    // Mode
    // -------------------------------------------------

    void setMode(uint8_t mode);
    uint8_t mode() const;

    void setModeValid(uint8_t mode, bool valid);
    bool isModeValid(uint8_t mode) const;

    // -------------------------------------------------
    // Fan
    // -------------------------------------------------

    void setFan(uint8_t fan);
    uint8_t fan() const;

    void setFanValid(uint8_t fan, bool valid);
    bool isFanValid(uint8_t fan) const;

private:
    State hvac_[HVAC_COUNT];

    uint8_t currentHvac_;
};