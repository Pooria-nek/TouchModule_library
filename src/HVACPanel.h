#pragma once

#include <stddef.h>
#include <stdint.h>
#include "MemoryCore.h"

class HVACPanel
{
public:
    static constexpr uint8_t HVAC_COUNT = 8;
    static constexpr uint8_t MODE_COUNT = 4;
    static constexpr uint8_t FAN_COUNT = 4;
    static constexpr uint8_t TEMP_SENSOR_COUNT = 8;
    static constexpr uint16_t HVAC_IMAGE_SIZE = 240; // 64x30 / 8

    enum : uint8_t
    {
        MODE_COOL = 0,
        MODE_HEAT = 1,
        MODE_AUTO = 2,
        MODE_DEHUM = 3
    };

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
        bool valid = false;
        uint8_t deviceSn = 0;
        uint8_t deviceId = 0;

        bool power = false;
        bool irEnabled = false;

        float currentTemp = 24.0f;

        // UI-only setpoints (not persisted).
        float setTempCool = 24.0f;
        float setTempHeat = 24.0f;
        float setTempAuto = 24.0f;

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
        TempSensor tempSensors[TEMP_SENSOR_COUNT]{};
    };

    // On-flash record. Byte layout is identical to the previous version
    // (the old, never-written `float setTemp` is now `reserved`), so existing
    // stored records stay readable.
    struct FlashHvac
    {
        uint8_t valid; // 0/1, anything else = erased
        uint8_t deviceSn;
        uint8_t deviceId;
        uint8_t power;
        uint8_t irEnabled;
        uint8_t reserved[7];

        uint8_t mode;
        uint8_t fan;
        uint8_t autofan;

        uint8_t validModeMask;
        uint8_t validFanMask;

        uint8_t rangeCool[2];
        uint8_t rangeHeat[2];
        uint8_t rangeAuto[2];
        uint8_t rangeDehum[2];

        uint8_t tempSource;
        TempSensor tempSensors[TEMP_SENSOR_COUNT];
    };

    static_assert(sizeof(FlashHvac) <= 96, "FlashHvac exceeds its 96-byte flash slot");
    static_assert(offsetof(FlashHvac, mode) == 12, "FlashHvac layout changed - stored records would break");

    explicit HVACPanel(MemoryCore &flash);

    // Flash
    void load();
    bool update(); // true when the current unit's control state changed since last call

    // HVAC selection
    void setCurrentUnit(uint8_t index);
    uint8_t currentUnit() const;
    bool changeUnit(bool forward);

    State &currentState();
    const State &currentState() const;
    State &get(uint8_t index);
    const State &get(uint8_t index) const;

    // HVAC validity
    void setHvacValid(uint8_t index, bool valid);
    bool isHvacValid(uint8_t index) const;
    bool hasValidUnit() const;
    void setHvacMember(uint8_t index, bool enabled, uint8_t deviceSn, uint8_t deviceId);
    uint16_t address() const;

    // Power
    void setPower(bool power);
    bool power() const;
    void togglePower();
    bool setPower(uint8_t index, bool power);

    // Temperature
    void setCurrentTemp(float temp);
    float currentTemp() const;

    bool setSetTemp(float temp);
    float setTemp() const;
    bool increaseTemp(float amount = 1.0f);
    bool decreaseTemp(float amount = 1.0f);

    bool setSetTempCool(uint8_t index, uint8_t temp);
    bool setSetTempHeat(uint8_t index, uint8_t temp);
    bool setSetTempAuto(uint8_t index, uint8_t temp);

    uint8_t getSetTempCool() const;
    uint8_t getSetTempHeat() const;
    uint8_t getSetTempAuto() const;

    void setIrEnabled(bool enabled);
    bool irEnabled() const;
    bool setIrEnabled(uint8_t index, bool enabled);

    // Mode
    void setMode(uint8_t mode);
    bool setMode(uint8_t index, uint8_t mode);
    uint8_t mode() const;
    bool nextMode();
    bool previousMode();
    void setModeValid(uint8_t mode, bool valid);
    void setModeValid(uint8_t hvacIndex, uint8_t mode, bool valid);

    // Fan
    void setFan(uint8_t fan);
    bool setFan(uint8_t index, uint8_t fan);
    bool setAutoFan(uint8_t index, uint8_t fan);
    uint8_t fan() const;
    bool changeFan(bool forward);
    void setFanValid(uint8_t fan, bool valid);
    void setFanValid(uint8_t hvacIndex, uint8_t fan, bool valid);
    bool isFanValid(uint8_t fan) const;

    // Icon of the *current* unit only (one shared buffer).
    const uint8_t *currentImage() const;
    void fetchImage();              // reload current unit's icon
    void fetchImage(uint8_t index); // reloads only if index is the current unit

    void saveCurrent();
    void save(uint8_t index);

private:
    struct ControlSnapshot
    {
        bool power;
        float setTemp;
        uint8_t mode;
        uint8_t fan;
        uint8_t autofan;

        bool operator==(const ControlSnapshot &o) const
        {
            return power == o.power && setTemp == o.setTemp && mode == o.mode &&
                   fan == o.fan && autofan == o.autofan;
        }
    };

    State hvac_[HVAC_COUNT];
    uint8_t currentHvac_;
    MemoryCore &flash_;

    uint8_t image_[HVAC_IMAGE_SIZE]{};

    ControlSnapshot lastSent_[HVAC_COUNT]{};
    bool snapshotValid_[HVAC_COUNT]{};

    void fetchUnit(uint8_t index);
    void saveUnit(uint8_t index);
    void loadCurrentImage();
    void refreshSelection(uint8_t changedIndex);

    static void normalizeState(State &state);
    static bool applySetpoint(State &state, uint8_t mode, float temp);
};