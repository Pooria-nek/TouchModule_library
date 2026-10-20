#pragma once

#include <stdint.h>
#include "MemoryCore.h"

class HVACPanel
{
public:
    static constexpr uint8_t HVAC_COUNT = 8;
    static constexpr uint8_t MODE_COUNT = 4;
    static constexpr uint8_t FAN_COUNT = 4;

    static constexpr uint8_t TEMP_SENSOR_COUNT = 8;

    enum class TempSource : uint8_t
    {
        Internal = 0,
        External = 1,
        Average = 2
    };

    struct TempSensor
    {
        uint8_t state = 0;
        uint8_t deviceSn = 0;
        uint8_t deviceId = 0;
        uint8_t channel = 0;
    };

    struct State
    {
        uint8_t version;
        uint8_t valid;
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

        TempSource tempSource = TempSource::Internal;
        TempSensor tempSensors[8]{};
    };

    struct FlashHvac
    {
        // HVAC settings
        uint8_t valid;
        uint8_t deviceSn;
        uint8_t deviceId;

        uint8_t power;
        float setTemp;

        uint8_t mode;
        uint8_t fan;
        uint8_t autofan;

        uint8_t validModeMask;
        uint8_t validFanMask;

        uint8_t rangeCool[2];
        uint8_t rangeHeat[2];
        uint8_t rangeAuto[2];
        uint8_t rangeDehum[2];

        // Temperature source: Internal, External, Average
        uint8_t tempSource;

        // Eight temperature sensors, 4 bytes each
        struct FlashTempSensor
        {
            uint8_t state;
            uint8_t deviceSn;
            uint8_t deviceId;
            uint8_t channel;
        };

        FlashTempSensor tempSensors[TEMP_SENSOR_COUNT];
    };

    static_assert(sizeof(FlashHvac) <= 96, "FlashHvac exceeds its 96-byte flash slot");

    explicit HVACPanel(MemoryCore &flash);

    // -------------------------------------------------
    // Flash
    // -------------------------------------------------

    void load();

    bool update();

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

    bool increaseTemp(float amount = 1.0f);
    bool decreaseTemp(float amount = 1.0f);

    // -------------------------------------------------
    // Mode
    // -------------------------------------------------

    void setMode(uint8_t mode);
    uint8_t mode() const;

    bool nextMode();
    void previousMode();

    void setModeValid(uint8_t mode, bool valid);
    // bool isModeValid(uint8_t mode) const;

    void setModeValid(uint8_t hvacIndex, uint8_t mode, bool valid);
    void setFanValid(uint8_t hvacIndex, uint8_t fan, bool valid);

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

    void setHvacMember(uint8_t index, bool enabled, uint8_t deviceSn, uint8_t deviceId);
    // void saveHvacSensors(uint8_t index);
    // void fetchHvacSensors(uint8_t index);

    void saveCurrent();
    void save(uint8_t index);

    // bool hasValidHvac() const;
private:
    State hvac_[HVAC_COUNT];

    uint8_t currentHvac_;

    MemoryCore &flash_;

    uint8_t images_[HVAC_COUNT][HVAC_IMAGE_SIZE];

    struct ControlSnapshot
    {
        bool power;
        float setTemp;
        uint8_t mode;
        uint8_t fan;
        uint8_t autofan;
    };

    ControlSnapshot lastSent_[HVAC_COUNT]{};
    bool snapshotValid_[HVAC_COUNT]{};

    void fetchHvacValid();
    void fetchHvacValid(uint8_t index);
    void fetchHvac(uint8_t index);

    bool hasValidHvac() const;
    void normalizeState(State &state);
    void saveHvac(uint8_t index);
};