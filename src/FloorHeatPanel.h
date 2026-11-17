#pragma once

#include <stdint.h>
#include "MemoryCore.h"

class FloorHeatPanel
{
public:
    static constexpr uint8_t FHEAT_COUNT = 4;
    static constexpr uint8_t MODE_COUNT = 4;
    static constexpr uint16_t FHEAT_IMAGE_SIZE = 240; // 64x30 / 8

    enum Mode : uint8_t
    {
        Normal,
        Day,
        Night,
        Away
    };

    struct State
    {
        uint8_t version = 0;
        uint8_t valid = 0;
        uint8_t deviceSn = 0;
        uint8_t deviceId = 0;
        uint8_t channel = 0;
        uint8_t mode = Normal;
        bool power = false;

        float currentTemp = 24.0f;
        float setTemps[MODE_COUNT] = {24.0f, 24.0f, 24.0f, 24.0f}; // indexed by Mode
    };

    struct FlashHvac
    {
        uint8_t valid;
        uint8_t deviceSn;
        uint8_t deviceId;
        uint8_t channel;

        uint8_t power;
        float setTemp; // currently unused, see notes
        uint8_t mode;
    };

    explicit FloorHeatPanel(MemoryCore &flash);

    // Flash
    void load();
    bool update(); // true when the current unit's state changed since the last true

    // Unit selection
    void setCurrentUnit(uint8_t index);
    uint8_t currentUnit() const;
    bool changeUnit(bool forward);

    State &get(uint8_t index);
    const State &get(uint8_t index) const;
    State &currentState();
    const State &currentState() const;

    // Power
    void setPower(bool value);
    bool power() const;
    bool setPower(uint8_t index, bool power);
    void togglePower();

    // Temperature
    void setCurrentTemp(float value);
    float currentTemp() const;

    bool setSetTempNormal(uint8_t index, uint8_t temp);
    bool setSetTempDay(uint8_t index, uint8_t temp);
    bool setSetTempNight(uint8_t index, uint8_t temp);
    bool setSetTempAway(uint8_t index, uint8_t temp);

    bool setSetTemp(float temp);
    float setTemp() const;

    bool increaseTemp(float amount = 1.0f);
    bool decreaseTemp(float amount = 1.0f);

    float getSetTempNormal() const;
    float getSetTempDay() const;
    float getSetTempNight() const;
    float getSetTempAway() const;

    uint16_t address() const;

    // Mode
    void setMode(uint8_t mode);
    bool setMode(uint8_t index, uint8_t mode);
    uint8_t mode() const;
    bool nextMode();
    bool previousMode();

    // Images
    const uint8_t *currentImage() const;
    void fetchImage();
    void fetchImage(uint8_t index);

    // Membership / persistence
    void setFheatMember(uint8_t index, bool enabled, uint8_t deviceSn, uint8_t deviceId, uint8_t channel);
    void saveCurrent();
    void save(uint8_t index);

private:
    struct ControlSnapshot
    {
        bool valid;
        bool power;
        uint8_t mode;
        float setTemp;
    };

    State floorheat_[FHEAT_COUNT]{};
    ControlSnapshot lastSent_[FHEAT_COUNT]{};
    uint8_t images_[FHEAT_COUNT][FHEAT_IMAGE_SIZE];
    MemoryCore &flash_;
    uint8_t currentUnit_ = 0;

    uint32_t unitAddress(uint8_t index);
    void fetchUnit(uint8_t index);
    void saveUnit(uint8_t index);
};