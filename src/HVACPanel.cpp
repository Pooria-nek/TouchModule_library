#include "HVACPanel.h"

namespace
{
    // Select the first configured HVAC, falling back to index 0 when none are valid.
    void selectFirstValidHvac(const HVACPanel::State *states, uint8_t count, uint8_t &selected)
    {
        selected = 0;
        for (uint8_t i = 0; i < count; ++i)
        {
            if (states[i].valid)
            {
                selected = i;
                return;
            }
        }
    }

    // Return the configured setpoint range for the active operating mode.
    bool getTemperatureRange(const HVACPanel::State &state, uint8_t &minimum, uint8_t &maximum)
    {
        const uint8_t *range = nullptr;
        switch (state.mode)
        {
        case 0:
            range = state.rangeCool;
            break;
        case 1:
            range = state.rangeHeat;
            break;
        case 2:
            range = state.rangeAuto;
            break;
        case 3:
            range = state.rangeDehum;
            break;
        default:
            return false;
        }

        minimum = range[0];
        maximum = range[1];
        return minimum <= maximum;
    }
}

HVACPanel::HVACPanel(MemoryCore &flash)
    : currentHvac_(0),
      flash_(flash)
{
    for (uint8_t i = 0; i < HVAC_COUNT; ++i)
    {
        hvac_[i] = State{};
    }
}

// =================================================
// Flash
// =================================================

void HVACPanel::load()
{
    for (uint8_t i = 0; i < HVAC_COUNT; ++i)
    {
        hvac_[i] = State{};
        fetchHvac(i);
    }

    fetchImage();

    selectFirstValidHvac(hvac_, HVAC_COUNT, currentHvac_);
}

bool HVACPanel::update()
{
    if (currentHvac_ >= HVAC_COUNT)
        return false;

    const State &state = hvac_[currentHvac_];

    if (!state.valid)
        return false;

    ControlSnapshot current{
        state.power,
        state.setTemp,
        state.mode,
        state.fan,
        state.autofan};

    ControlSnapshot &last = lastSent_[currentHvac_];

    const bool changed =
        !snapshotValid_[currentHvac_] ||
        current.power != last.power ||
        current.setTemp != last.setTemp ||
        current.mode != last.mode ||
        current.fan != last.fan ||
        current.autofan != last.autofan;

    if (!changed)
        return false;

    last = current;
    snapshotValid_[currentHvac_] = true;

    return true;
}

void HVACPanel::fetchHvacValid(uint8_t index)
{
    if (index >= HVAC_COUNT)
        return;

    uint8_t valid = 0;
    const uint32_t address = flash_.findAdrress(MemoryAdress::Touch::SECTOR_INFO, MemoryAdress::Touch::HVAC_ADDRESSES[index]);
    flash_.read(address, &valid, sizeof(valid));

    hvac_[index].valid = valid;
}

void HVACPanel::fetchHvacValid()
{
    for (uint8_t i = 0; i < HVAC_COUNT; ++i)
    {
        fetchHvacValid(i);
    }
}

void HVACPanel::fetchHvac(uint8_t index)
{
    if (index >= HVAC_COUNT)
        return;

    FlashHvac data{};

    const uint32_t address = flash_.findAdrress(MemoryAdress::Touch::SECTOR_INFO, MemoryAdress::Touch::HVAC_ADDRESSES[index]);

    flash_.read(address, reinterpret_cast<uint8_t *>(&data), sizeof(data));

    State &state = hvac_[index];

    // Erased or invalid record.
    // Valid member values are strictly 0 or 1.
    if (data.valid > 1)
    {
        state = State{};
        state.valid = 0;
        return;
    }

    // Restore these even when the HVAC is disabled.
    state.valid = data.valid;
    state.deviceSn = data.deviceSn;
    state.deviceId = data.deviceId;

    state.power = data.power != 0;
    state.setTemp = data.setTemp;

    state.mode = data.mode;
    state.fan = data.fan;
    state.autofan = data.autofan;

    for (uint8_t i = 0; i < MODE_COUNT; ++i)
        state.validMode[i] =
            (data.validModeMask & (1u << i)) != 0;

    for (uint8_t i = 0; i < FAN_COUNT; ++i)
        state.validFan[i] =
            (data.validFanMask & (1u << i)) != 0;

    for (uint8_t i = 0; i < 2; ++i)
    {
        state.rangeCool[i] = data.rangeCool[i];
        state.rangeHeat[i] = data.rangeHeat[i];
        state.rangeAuto[i] = data.rangeAuto[i];
        state.rangeDehum[i] = data.rangeDehum[i];
    }

    if (data.tempSource <= static_cast<uint8_t>(TempSource::Average))
    {
        state.tempSource = static_cast<TempSource>(data.tempSource);
    }
    else
    {
        state.tempSource = TempSource::Internal;
    }

    for (uint8_t i = 0; i < TEMP_SENSOR_COUNT; ++i)
    {
        state.tempSensors[i].state = data.tempSensors[i].state;
        state.tempSensors[i].deviceSn = data.tempSensors[i].deviceSn;
        state.tempSensors[i].deviceId = data.tempSensors[i].deviceId;
        state.tempSensors[i].channel = data.tempSensors[i].channel;
    }

    // Old records may contain zeroed ranges because the previous
    // saveHvac() did not save them.
    auto fixRange = [](uint8_t range[2])
    {
        if (range[0] == 0 || range[1] == 0 ||
            range[0] > range[1])
        {
            range[0] = 15;
            range[1] = 30;
        }
    };

    fixRange(state.rangeCool);
    fixRange(state.rangeHeat);
    fixRange(state.rangeAuto);
    fixRange(state.rangeDehum);

    if (state.valid)
        normalizeState(state);
}

void HVACPanel::saveHvac(uint8_t index)
{
    if (index >= HVAC_COUNT)
        return;

    const State &state = hvac_[index];

    FlashHvac data{};

    data.valid = state.valid ? 1 : 0;
    data.deviceSn = state.deviceSn;
    data.deviceId = state.deviceId;

    data.power = state.power ? 1 : 0;
    data.setTemp = state.setTemp;

    data.mode = state.mode;
    data.fan = state.fan;
    data.autofan = state.autofan;

    for (uint8_t i = 0; i < MODE_COUNT; ++i)
        if (state.validMode[i])
            data.validModeMask |= (1u << i);

    for (uint8_t i = 0; i < FAN_COUNT; ++i)
        if (state.validFan[i])
            data.validFanMask |= (1u << i);

    for (uint8_t i = 0; i < 2; ++i)
    {
        data.rangeCool[i] = state.rangeCool[i];
        data.rangeHeat[i] = state.rangeHeat[i];
        data.rangeAuto[i] = state.rangeAuto[i];
        data.rangeDehum[i] = state.rangeDehum[i];
    }

    data.tempSource = static_cast<uint8_t>(state.tempSource);

    for (uint8_t i = 0; i < TEMP_SENSOR_COUNT; ++i)
    {
        data.tempSensors[i].state = state.tempSensors[i].state;
        data.tempSensors[i].deviceSn = state.tempSensors[i].deviceSn;
        data.tempSensors[i].deviceId = state.tempSensors[i].deviceId;
        data.tempSensors[i].channel = state.tempSensors[i].channel;
    }

    const uint32_t address = flash_.findAdrress(MemoryAdress::Touch::SECTOR_INFO, MemoryAdress::Touch::HVAC_ADDRESSES[index]);

    flash_.update(address, reinterpret_cast<uint8_t *>(&data), sizeof(data));
}

void HVACPanel::saveCurrent()
{
    saveHvac(currentHvac_);
}

void HVACPanel::save(uint8_t index)
{
    if (index >= HVAC_COUNT)
        return;

    saveHvac(index);
}

void HVACPanel::fetchImage(uint8_t index)
{
    if (index >= HVAC_COUNT)
        return;

    const uint32_t address = flash_.findAdrress(
        MemoryAdress::Touch::SECTOR_ICONS,
        MemoryAdress::Touch::HVAC_IMAGE[index]);
    flash_.read(address, images_[index], HVAC_IMAGE_SIZE);
}

void HVACPanel::fetchImage()
{
    for (uint8_t i = 0; i < HVAC_COUNT; ++i)
    {
        fetchImage(i);
    }
}

// =================================================
// HVAC selection
// =================================================

const uint8_t *HVACPanel::currentImage() const
{
    if (currentHvac_ >= HVAC_COUNT || !hvac_[currentHvac_].valid)
        return nullptr;

    return images_[currentHvac_];
}

void HVACPanel::setCurrentHvac(uint8_t index)
{
    if (index >= HVAC_COUNT)
        return;

    if (!hvac_[index].valid)
        return;

    currentHvac_ = index;
}

uint8_t HVACPanel::currentHvac() const
{
    return currentHvac_;
}

bool HVACPanel::changeHvac(bool forward)
{
    const uint8_t oldHvac = currentHvac_;
    uint8_t hvac = currentHvac_;

    for (uint8_t i = 1; i < HVAC_COUNT; ++i)
    {
        if (forward)
            hvac = (hvac >= HVAC_COUNT - 1) ? 0 : hvac + 1;
        else
            hvac = (hvac == 0) ? HVAC_COUNT - 1 : hvac - 1;

        if (hvac_[hvac].valid)
        {
            currentHvac_ = hvac;
            return currentHvac_ != oldHvac;
        }
    }

    return false;
}

HVACPanel::State &HVACPanel::currentState()
{
    return hvac_[currentHvac_];
}

const HVACPanel::State &HVACPanel::currentState() const
{
    return hvac_[currentHvac_];
}

HVACPanel::State &HVACPanel::get(uint8_t index)
{
    if (index < HVAC_COUNT)
        return hvac_[index];

    static State invalidState{};
    invalidState.valid = false;
    return invalidState;
}

const HVACPanel::State &HVACPanel::get(uint8_t index) const
{
    if (index < HVAC_COUNT)
        return hvac_[index];

    static const State invalidState = []
    {
        State state{};
        state.valid = false;
        return state;
    }();

    return invalidState;
}

// =================================================
// HVAC validity
// =================================================

void HVACPanel::setHvacValid(uint8_t index, bool valid)
{
    if (index >= HVAC_COUNT)
        return;

    hvac_[index].valid = valid;
    saveHvac(index);

    if (!valid && currentHvac_ == index)
    {
        selectFirstValidHvac(hvac_, HVAC_COUNT, currentHvac_);
    }
}

bool HVACPanel::hasValidHvac() const
{
    for (uint8_t i = 0; i < HVAC_COUNT; ++i)
    {
        if (hvac_[i].valid)
            return true;
    }

    return false;
}

bool HVACPanel::isHvacValid(uint8_t index) const
{
    if (index >= HVAC_COUNT)
        return false;

    return hvac_[index].valid;
}

// =================================================
// Power
// =================================================

void HVACPanel::setPower(bool power)
{
    currentState().power = power;
}

bool HVACPanel::power() const
{
    return currentState().power;
}

void HVACPanel::togglePower()
{
    currentState().power = !currentState().power;
}

// =================================================
// Temperature
// =================================================

void HVACPanel::setCurrentTemp(float temp)
{
    currentState().currentTemp = temp;
}

float HVACPanel::currentTemp() const
{
    return currentState().currentTemp;
}

void HVACPanel::normalizeState(State &state)
{
    // Restore a usable mode if the saved mode is invalid.
    if (state.mode >= MODE_COUNT || !state.validMode[state.mode])
    {
        bool found = false;

        for (uint8_t i = 0; i < MODE_COUNT; ++i)
        {
            if (state.validMode[i])
            {
                state.mode = i;
                found = true;
                break;
            }
        }

        if (!found)
            state.mode = 0;
    }

    // Restore a usable fan setting.
    if (state.fan >= FAN_COUNT || !state.validFan[state.fan])
    {
        bool found = false;

        for (uint8_t i = 0; i < FAN_COUNT; ++i)
        {
            if (state.validFan[i])
            {
                state.fan = i;
                found = true;
                break;
            }
        }

        if (!found)
            state.fan = 0;
    }

    // Prevent invalid or non-finite stored temperatures.
    if (!(state.setTemp >= 0.0f && state.setTemp <= 100.0f))
        state.setTemp = 24.0f;
}

void HVACPanel::setSetTemp(float temp)
{
    if (!(temp >= 0.0f && temp <= 100.0f))
        return;

    State &state = currentState();

    uint8_t minTemp;
    uint8_t maxTemp;
    if (!getTemperatureRange(state, minTemp, maxTemp))
        return;

    if (temp < minTemp)
        temp = static_cast<float>(minTemp);

    if (temp > maxTemp)
        temp = static_cast<float>(maxTemp);

    state.setTemp = temp;
}

float HVACPanel::setTemp() const
{
    return currentState().setTemp;
}

bool HVACPanel::increaseTemp(float amount)
{
    if (!(amount > 0.0f && amount <= 100.0f))
        return false;

    const float previous = currentState().setTemp;

    setSetTemp(previous + amount);

    return currentState().setTemp != previous;
}

bool HVACPanel::decreaseTemp(float amount)
{
    if (!(amount > 0.0f && amount <= 100.0f))
        return false;

    const float previous = currentState().setTemp;

    setSetTemp(previous - amount);

    return currentState().setTemp != previous;
}

// =================================================
// Mode
// =================================================

void HVACPanel::setMode(uint8_t mode)
{
    if (mode >= MODE_COUNT)
        return;

    if (!currentState().validMode[mode])
        return;

    currentState().mode = mode;
}

uint8_t HVACPanel::mode() const
{
    return currentState().mode;
}

bool HVACPanel::nextMode()
{
    State &state = currentState();

    for (uint8_t offset = 1; offset <= MODE_COUNT; ++offset)
    {
        const uint8_t index = (state.mode + offset) % MODE_COUNT;

        if (state.validMode[index])
        {
            if (state.mode == index)
                return false;

            state.mode = index;
            return true;
        }
    }

    return false;
}

void HVACPanel::previousMode()
{
    State &state = currentState();

    for (uint8_t offset = 1; offset <= MODE_COUNT; ++offset)
    {
        uint8_t index =
            (state.mode + MODE_COUNT - offset) % MODE_COUNT;

        if (state.validMode[index])
        {
            state.mode = index;
            return;
        }
    }
}

void HVACPanel::setModeValid(uint8_t mode, bool valid)
{
    setModeValid(currentHvac_, mode, valid);
}

void HVACPanel::setModeValid(
    uint8_t hvacIndex,
    uint8_t mode,
    bool valid)
{
    if (hvacIndex >= HVAC_COUNT || mode >= MODE_COUNT)
        return;

    State &state = hvac_[hvacIndex];

    state.validMode[mode] = valid;

    if (!valid && state.mode == mode)
    {
        for (uint8_t i = 0; i < MODE_COUNT; ++i)
        {
            if (state.validMode[i])
            {
                state.mode = i;
                saveHvac(hvacIndex);
                return;
            }
        }

        // No valid modes remain.
        state.mode = 0;
    }
    else if (valid &&
             (state.mode >= MODE_COUNT ||
              !state.validMode[state.mode]))
    {
        state.mode = mode;
    }

    saveHvac(hvacIndex);
}

void HVACPanel::setFanValid(uint8_t fan, bool valid)
{
    setFanValid(currentHvac_, fan, valid);
}

void HVACPanel::setFanValid(
    uint8_t hvacIndex,
    uint8_t fan,
    bool valid)
{
    if (hvacIndex >= HVAC_COUNT || fan >= FAN_COUNT)
        return;

    State &state = hvac_[hvacIndex];

    state.validFan[fan] = valid;

    if (!valid && state.fan == fan)
    {
        for (uint8_t i = 0; i < FAN_COUNT; ++i)
        {
            if (state.validFan[i])
            {
                state.fan = i;
                saveHvac(hvacIndex);
                return;
            }
        }

        // No valid fan speeds remain.
        state.fan = 0;
    }
    else if (valid &&
             (state.fan >= FAN_COUNT ||
              !state.validFan[state.fan]))
    {
        state.fan = fan;
    }

    saveHvac(hvacIndex);
}

// =================================================
// Fan
// =================================================

void HVACPanel::setFan(uint8_t fan)
{
    if (fan >= FAN_COUNT)
        return;

    if (!currentState().validFan[fan])
        return;

    currentState().fan = fan;
}

uint8_t HVACPanel::fan() const
{
    return currentState().fan;
}

bool HVACPanel::changeFan(bool forward)
{
    State &state = currentState();

    const uint8_t previousFan = state.fan;

    for (uint8_t offset = 1; offset <= FAN_COUNT; ++offset)
    {
        uint8_t index;

        if (forward)
        {
            index = (state.fan + offset) % FAN_COUNT;
        }
        else
        {
            index = (state.fan + FAN_COUNT - offset) % FAN_COUNT;
        }

        if (state.validFan[index])
        {
            state.fan = index;
            return state.fan != previousFan;
        }
    }

    return false;
}

void HVACPanel::setHvacMember(uint8_t index, bool enabled, uint8_t deviceSn, uint8_t deviceId)
{
    if (index >= HVAC_COUNT)
        return;

    State &state = hvac_[index];

    state.valid = enabled;
    state.deviceSn = deviceSn;
    state.deviceId = deviceId;

    saveHvac(index);

    if (!enabled && currentHvac_ == index)
    {
        selectFirstValidHvac(hvac_, HVAC_COUNT, currentHvac_);
    }
}

bool HVACPanel::isFanValid(uint8_t fan) const
{
    if (fan >= FAN_COUNT)
        return false;

    return currentState().validFan[fan];
}