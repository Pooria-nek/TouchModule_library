#pragma once

#include <stdint.h>
#include "MemoryCore.h"

class HVACPanel
{
public:
    static constexpr uint8_t HVAC_COUNT = 8;
    static constexpr uint8_t MODE_COUNT = 4;
    static constexpr uint8_t FAN_COUNT = 4;

    struct State
    {
        bool valid = true;
        // uint16_t deviceAddress = 0;
        uint8_t deviceSn = 0;
        uint8_t deviceId = 0;

        bool power = false;

        float currentTemp = 24.0f;
        float setTemp = 24.0f;

        uint8_t mode = 0;
        uint8_t fan = 0;
        uint8_t autofan = 0;

        bool validMode[MODE_COUNT] = {true, true, true, true};
        bool validFan[FAN_COUNT] = {true, true, true, true};

        uint8_t rangeCool[2] = {15, 30};
        uint8_t rangeHeat[2] = {15, 30};
        uint8_t rangeAuto[2] = {15, 30};
        uint8_t rangeDehum[2] = {15, 30};
    };

    struct FlashHvac
    {
        uint8_t power;
        float setTemp;
        uint8_t mode;
        uint8_t fan;
        uint8_t validModeMask;
        uint8_t validFanMask;
    };

    explicit HVACPanel(MemoryCore &flash);

    // -------------------------------------------------
    // Flash
    // -------------------------------------------------

    void load();

    // -------------------------------------------------
    // HVAC selection
    // -------------------------------------------------

    void setCurrentHvac(uint8_t index);
    uint8_t currentHvac() const;

    bool changeHvac(bool forward);

    State &currentState();
    const State &currentState() const;

    State &get(uint8_t index);
    const State &get(uint8_t index) const;

    // -------------------------------------------------
    // HVAC validity
    // -------------------------------------------------

    void setHvacValid(uint8_t index, bool valid);
    bool isHvacValid(uint8_t index) const;
    // uint8_t hvacValidMask() const;

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

    void nextMode();
    void previousMode();

    void setModeValid(uint8_t mode, bool valid);
    bool isModeValid(uint8_t mode) const;

    // -------------------------------------------------
    // Fan
    // -------------------------------------------------

    void setFan(uint8_t fan);
    uint8_t fan() const;

    bool changeFan(bool forward);
    // void nextFan();
    // void previousFan();

    void setFanValid(uint8_t fan, bool valid);
    bool isFanValid(uint8_t fan) const;

    static constexpr uint16_t HVAC_IMAGE_SIZE = 240; // 64x30 / 8

    const uint8_t *currentImage() const;
    void fetchImage();
    void fetchImage(uint8_t index);

private:
    State hvac_[HVAC_COUNT];

    uint8_t currentHvac_;

    MemoryCore &flash_;

    uint8_t images_[HVAC_COUNT][HVAC_IMAGE_SIZE];

    void fetchHvacValid();
    void fetchHvacValid(uint8_t index);
    void fetchHvac(uint8_t index);
};