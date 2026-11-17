#include "HVACPanel.h"
#include <string.h>

namespace
{
    // Setpoint for the state's active mode (nullptr in dehumidify / invalid mode).
    template <typename S>
    auto setpointOf(S &s) -> decltype(&s.setTempCool)
    {
        switch (s.mode)
        {
        case HVACPanel::MODE_COOL:
            return &s.setTempCool;
        case HVACPanel::MODE_HEAT:
            return &s.setTempHeat;
        case HVACPanel::MODE_AUTO:
            return &s.setTempAuto;
        default:
            return nullptr;
        }
    }

    // Move `current` to the next/previous index (circular) accepted by isValid.
    template <typename Pred>
    bool stepToValid(uint8_t count, uint8_t &current, bool forward, Pred isValid)
    {
        uint8_t idx = (current < count) ? current : count - 1;

        for (uint8_t n = 1; n < count; ++n)
        {
            if (forward)
                idx = (idx + 1 >= count) ? 0 : idx + 1;
            else
                idx = (idx == 0) ? count - 1 : idx - 1;

            if (isValid(idx))
            {
                current = idx;
                return true;
            }
        }
        return false;
    }

    // Set flags[item] and keep `selected` pointing at a valid entry.
    // Returns true if anything changed (i.e. worth persisting).
    template <size_t N>
    bool setFlagAndRepair(bool (&flags)[N], uint8_t &selected, uint8_t item, bool valid)
    {
        const bool flagChanged = (flags[item] != valid);
        const uint8_t before = selected;

        flags[item] = valid;

        if (!valid && selected == item)
        {
            selected = 0;
            for (uint8_t i = 0; i < N; ++i)
            {
                if (flags[i])
                {
                    selected = i;
                    break;
                }
            }
        }
        else if (valid && (selected >= N || !flags[selected]))
        {
            selected = item;
        }

        return flagChanged || selected != before;
    }

    // Keep `selected` if valid, otherwise first valid entry, otherwise 0.
    template <size_t N>
    uint8_t repairSelection(const bool (&flags)[N], uint8_t selected)
    {
        if (selected < N && flags[selected])
            return selected;

        for (uint8_t i = 0; i < N; ++i)
        {
            if (flags[i])
                return i;
        }
        return 0;
    }
}

HVACPanel::HVACPanel(MemoryCore &flash)
    : currentHvac_(0),
      flash_(flash)
{
}

// =================================================
// Flash
// =================================================

void HVACPanel::load()
{
    for (uint8_t i = 0; i < HVAC_COUNT; ++i)
    {
        fetchUnit(i);
        snapshotValid_[i] = false;
    }

    currentHvac_ = 0;
    refreshSelection(0);
}

void HVACPanel::fetchUnit(uint8_t index)
{
    if (index >= HVAC_COUNT)
        return;

    FlashHvac data{};
    const uint32_t address = flash_.findAdrress(
        MemoryAdress::Touch::SECTOR_INFO,
        MemoryAdress::Touch::HVAC_ADDRESSES[index]);
    flash_.read(address, reinterpret_cast<uint8_t *>(&data), sizeof(data));

    State &state = hvac_[index];
    state = State{};

    // Erased record: keep defaults.
    if (data.valid > 1)
        return;

    state.valid = data.valid != 0;
    state.deviceSn = data.deviceSn;
    state.deviceId = data.deviceId;
    state.power = data.power != 0;
    state.irEnabled = data.irEnabled != 0;

    state.mode = data.mode;
    state.fan = data.fan;
    state.autofan = data.autofan;

    for (uint8_t i = 0; i < MODE_COUNT; ++i)
        state.validMode[i] = (data.validModeMask & (1u << i)) != 0;

    for (uint8_t i = 0; i < FAN_COUNT; ++i)
        state.validFan[i] = (data.validFanMask & (1u << i)) != 0;

    memcpy(state.rangeCool, data.rangeCool, sizeof(state.rangeCool));
    memcpy(state.rangeHeat, data.rangeHeat, sizeof(state.rangeHeat));
    memcpy(state.rangeAuto, data.rangeAuto, sizeof(state.rangeAuto));
    memcpy(state.rangeDehum, data.rangeDehum, sizeof(state.rangeDehum));

    state.tempSource = (data.tempSource <= static_cast<uint8_t>(TempSource::Average))
                           ? static_cast<TempSource>(data.tempSource)
                           : TempSource::Internal;

    memcpy(state.tempSensors, data.tempSensors, sizeof(state.tempSensors));

    normalizeState(state);
}

void HVACPanel::saveUnit(uint8_t index)
{
    if (index >= HVAC_COUNT)
        return;

    const State &state = hvac_[index];

    FlashHvac data{};

    data.valid = state.valid ? 1 : 0;
    data.deviceSn = state.deviceSn;
    data.deviceId = state.deviceId;
    data.power = state.power ? 1 : 0;
    data.irEnabled = state.irEnabled ? 1 : 0;

    data.mode = state.mode;
    data.fan = state.fan;
    data.autofan = state.autofan;

    for (uint8_t i = 0; i < MODE_COUNT; ++i)
        if (state.validMode[i])
            data.validModeMask |= (1u << i);

    for (uint8_t i = 0; i < FAN_COUNT; ++i)
        if (state.validFan[i])
            data.validFanMask |= (1u << i);

    memcpy(data.rangeCool, state.rangeCool, sizeof(data.rangeCool));
    memcpy(data.rangeHeat, state.rangeHeat, sizeof(data.rangeHeat));
    memcpy(data.rangeAuto, state.rangeAuto, sizeof(data.rangeAuto));
    memcpy(data.rangeDehum, state.rangeDehum, sizeof(data.rangeDehum));

    data.tempSource = static_cast<uint8_t>(state.tempSource);
    memcpy(data.tempSensors, state.tempSensors, sizeof(data.tempSensors));

    const uint32_t address = flash_.findAdrress(
        MemoryAdress::Touch::SECTOR_INFO,
        MemoryAdress::Touch::HVAC_ADDRESSES[index]);

    flash_.update(address, reinterpret_cast<uint8_t *>(&data), sizeof(data));
}

void HVACPanel::saveCurrent()
{
    saveUnit(currentHvac_);
}

void HVACPanel::save(uint8_t index)
{
    saveUnit(index);
}

void HVACPanel::normalizeState(State &state)
{
    state.mode = repairSelection(state.validMode, state.mode);
    state.fan = repairSelection(state.validFan, state.fan);

    // Invalid range -> defaults.
    auto fixRange = [](uint8_t (&range)[2])
    {
        if (range[0] == 0 || range[1] == 0 || range[0] > range[1])
        {
            range[0] = 15;
            range[1] = 30;
        }
    };

    fixRange(state.rangeCool);
    fixRange(state.rangeHeat);
    fixRange(state.rangeAuto);
    fixRange(state.rangeDehum);

    // Setpoints must sit inside their range (also catches NaN).
    auto clampTo = [](float &t, const uint8_t (&range)[2])
    {
        if (!(t >= range[0]))
            t = range[0];
        else if (t > range[1])
            t = range[1];
    };

    clampTo(state.setTempCool, state.rangeCool);
    clampTo(state.setTempHeat, state.rangeHeat);
    clampTo(state.setTempAuto, state.rangeAuto);
}

// =================================================
// Change detection
// =================================================

bool HVACPanel::update()
{
    const State &state = hvac_[currentHvac_];

    if (!state.valid)
        return false;

    const float *setpoint = setpointOf(state);

    const ControlSnapshot current{
        state.power,
        setpoint ? *setpoint : 0.0f,
        state.mode,
        state.fan,
        state.autofan};

    if (snapshotValid_[currentHvac_] && current == lastSent_[currentHvac_])
        return false;

    lastSent_[currentHvac_] = current;
    snapshotValid_[currentHvac_] = true;
    return true;
}

// =================================================
// Icon (current unit only)
// =================================================

void HVACPanel::loadCurrentImage()
{
    if (!hvac_[currentHvac_].valid)
        return;

    const uint32_t address = flash_.findAdrress(
        MemoryAdress::Touch::SECTOR_ICONS,
        MemoryAdress::Touch::HVAC_IMAGE[currentHvac_]);
    flash_.read(address, image_, HVAC_IMAGE_SIZE);
}

void HVACPanel::fetchImage()
{
    loadCurrentImage();
}

void HVACPanel::fetchImage(uint8_t index)
{
    if (index == currentHvac_)
        loadCurrentImage();
}

const uint8_t *HVACPanel::currentImage() const
{
    return hvac_[currentHvac_].valid ? image_ : nullptr;
}

// =================================================
// HVAC selection
// =================================================

// Ensure currentHvac_ points at a valid unit (if any exists) and that the
// icon buffer matches it.
void HVACPanel::refreshSelection(uint8_t changedIndex)
{
    const uint8_t before = currentHvac_;

    if (!hvac_[currentHvac_].valid)
    {
        currentHvac_ = 0;
        for (uint8_t i = 0; i < HVAC_COUNT; ++i)
        {
            if (hvac_[i].valid)
            {
                currentHvac_ = i;
                break;
            }
        }
    }

    if (currentHvac_ != before || changedIndex == currentHvac_)
        loadCurrentImage();
}

void HVACPanel::setCurrentUnit(uint8_t index)
{
    if (index >= HVAC_COUNT || !hvac_[index].valid || index == currentHvac_)
        return;

    currentHvac_ = index;
    loadCurrentImage();
}

uint8_t HVACPanel::currentUnit() const
{
    return currentHvac_;
}

bool HVACPanel::changeUnit(bool forward)
{
    const bool moved = stepToValid(HVAC_COUNT, currentHvac_, forward,
                                   [this](uint8_t i)
                                   { return hvac_[i].valid; });
    if (moved)
        loadCurrentImage();

    return moved;
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

    static State invalidState;
    invalidState = State{}; // callers may have scribbled on it
    return invalidState;
}

const HVACPanel::State &HVACPanel::get(uint8_t index) const
{
    if (index < HVAC_COUNT)
        return hvac_[index];

    static const State invalidState{};
    return invalidState;
}

// =================================================
// HVAC validity
// =================================================

void HVACPanel::setHvacMember(uint8_t index, bool enabled, uint8_t deviceSn, uint8_t deviceId)
{
    if (index >= HVAC_COUNT)
        return;

    State &state = hvac_[index];

    // Avoid pointless flash writes.
    if (state.valid == enabled && state.deviceSn == deviceSn && state.deviceId == deviceId)
        return;

    state.valid = enabled;
    state.deviceSn = deviceSn;
    state.deviceId = deviceId;
    snapshotValid_[index] = false; // new/removed target -> resend

    saveUnit(index);
    refreshSelection(index);
}

void HVACPanel::setHvacValid(uint8_t index, bool valid)
{
    if (index >= HVAC_COUNT)
        return;

    const State &state = hvac_[index];
    setHvacMember(index, valid, state.deviceSn, state.deviceId);
}

bool HVACPanel::hasValidUnit() const
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
    return index < HVAC_COUNT && hvac_[index].valid;
}

uint16_t HVACPanel::address() const
{
    const State &state = currentState();
    return (static_cast<uint16_t>(state.deviceSn) << 8) | state.deviceId;
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
    State &state = currentState();
    state.power = !state.power;
}

bool HVACPanel::setPower(uint8_t index, bool power)
{
    if (index >= HVAC_COUNT)
        return false;

    State &state = hvac_[index];

    if (!state.valid || state.power == power)
        return false;

    state.power = power;
    return true;
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

uint8_t HVACPanel::getSetTempCool() const
{
    return static_cast<uint8_t>(currentState().setTempCool);
}

uint8_t HVACPanel::getSetTempHeat() const
{
    return static_cast<uint8_t>(currentState().setTempHeat);
}

uint8_t HVACPanel::getSetTempAuto() const
{
    return static_cast<uint8_t>(currentState().setTempAuto);
}

// Clamp to the mode's range, truncate to whole degrees, store.
// Returns true if the stored value changed.
bool HVACPanel::applySetpoint(State &state, uint8_t mode, float temp)
{
    float *target;
    const uint8_t *range;

    switch (mode)
    {
    case MODE_COOL:
        target = &state.setTempCool;
        range = state.rangeCool;
        break;
    case MODE_HEAT:
        target = &state.setTempHeat;
        range = state.rangeHeat;
        break;
    case MODE_AUTO:
        target = &state.setTempAuto;
        range = state.rangeAuto;
        break;
    default:
        return false;
    }

    if (range[0] > range[1])
        return false;

    if (temp < range[0])
        temp = range[0];
    else if (temp > range[1])
        temp = range[1];

    const float value = static_cast<float>(static_cast<uint8_t>(temp));

    if (*target == value)
        return false;

    *target = value;
    return true;
}

bool HVACPanel::setSetTemp(float temp)
{
    if (!(temp >= 0.0f && temp <= 100.0f))
        return false;

    State &state = currentState();
    return applySetpoint(state, state.mode, temp);
}

float HVACPanel::setTemp() const
{
    const State &state = currentState();
    const float *setpoint = setpointOf(state);
    return setpoint ? *setpoint : state.setTempCool;
}

bool HVACPanel::increaseTemp(float amount)
{
    if (!(amount > 0.0f && amount <= 100.0f))
        return false;

    const float target = setTemp() + amount;
    return setSetTemp(target > 100.0f ? 100.0f : target);
}

bool HVACPanel::decreaseTemp(float amount)
{
    if (!(amount > 0.0f && amount <= 100.0f))
        return false;

    const float target = setTemp() - amount;
    return setSetTemp(target < 0.0f ? 0.0f : target);
}

bool HVACPanel::setSetTempCool(uint8_t index, uint8_t temp)
{
    return index < HVAC_COUNT && hvac_[index].valid &&
           applySetpoint(hvac_[index], MODE_COOL, temp);
}

bool HVACPanel::setSetTempHeat(uint8_t index, uint8_t temp)
{
    return index < HVAC_COUNT && hvac_[index].valid &&
           applySetpoint(hvac_[index], MODE_HEAT, temp);
}

bool HVACPanel::setSetTempAuto(uint8_t index, uint8_t temp)
{
    return index < HVAC_COUNT && hvac_[index].valid &&
           applySetpoint(hvac_[index], MODE_AUTO, temp);
}

void HVACPanel::setIrEnabled(bool enabled)
{
    setIrEnabled(currentHvac_, enabled);
}

bool HVACPanel::irEnabled() const
{
    return currentState().irEnabled;
}

bool HVACPanel::setIrEnabled(uint8_t index, bool enabled)
{
    if (index >= HVAC_COUNT)
        return false;

    State &state = hvac_[index];

    if (!state.valid || state.irEnabled == enabled)
        return false;

    state.irEnabled = enabled;
    saveUnit(index);
    return true;
}

// =================================================
// Mode
// =================================================

void HVACPanel::setMode(uint8_t mode)
{
    State &state = currentState();

    if (mode < MODE_COUNT && state.validMode[mode])
        state.mode = mode;
}

bool HVACPanel::setMode(uint8_t index, uint8_t mode)
{
    if (index >= HVAC_COUNT || mode >= MODE_COUNT)
        return false;

    State &state = hvac_[index];

    if (!state.valid || !state.validMode[mode] || state.mode == mode)
        return false;

    state.mode = mode;
    return true;
}

uint8_t HVACPanel::mode() const
{
    return currentState().mode;
}

bool HVACPanel::nextMode()
{
    State &state = currentState();
    return stepToValid(MODE_COUNT, state.mode, true,
                       [&state](uint8_t i)
                       { return state.validMode[i]; });
}

bool HVACPanel::previousMode()
{
    State &state = currentState();
    return stepToValid(MODE_COUNT, state.mode, false,
                       [&state](uint8_t i)
                       { return state.validMode[i]; });
}

void HVACPanel::setModeValid(uint8_t mode, bool valid)
{
    setModeValid(currentHvac_, mode, valid);
}

void HVACPanel::setModeValid(uint8_t hvacIndex, uint8_t mode, bool valid)
{
    if (hvacIndex >= HVAC_COUNT || mode >= MODE_COUNT)
        return;

    State &state = hvac_[hvacIndex];

    if (setFlagAndRepair(state.validMode, state.mode, mode, valid))
        saveUnit(hvacIndex);
}

// =================================================
// Fan
// =================================================

void HVACPanel::setFan(uint8_t fan)
{
    State &state = currentState();

    if (fan < FAN_COUNT && state.validFan[fan])
        state.fan = fan;
}

bool HVACPanel::setAutoFan(uint8_t index, uint8_t fan)
{
    if (index >= HVAC_COUNT || fan >= FAN_COUNT)
        return false;

    State &state = hvac_[index];

    if (!state.valid || !state.validFan[fan] || state.autofan == fan)
        return false;

    state.autofan = fan;
    return true;
}

bool HVACPanel::setFan(uint8_t index, uint8_t fan)
{
    if (index >= HVAC_COUNT || fan >= FAN_COUNT)
        return false;

    State &state = hvac_[index];

    if (!state.valid || !state.validFan[fan] || state.fan == fan)
        return false;

    state.fan = fan;
    return true;
}

uint8_t HVACPanel::fan() const
{
    return currentState().fan;
}

bool HVACPanel::changeFan(bool forward)
{
    State &state = currentState();
    return stepToValid(FAN_COUNT, state.fan, forward,
                       [&state](uint8_t i)
                       { return state.validFan[i]; });
}

void HVACPanel::setFanValid(uint8_t fan, bool valid)
{
    setFanValid(currentHvac_, fan, valid);
}

void HVACPanel::setFanValid(uint8_t hvacIndex, uint8_t fan, bool valid)
{
    if (hvacIndex >= HVAC_COUNT || fan >= FAN_COUNT)
        return;

    State &state = hvac_[hvacIndex];

    if (setFlagAndRepair(state.validFan, state.fan, fan, valid))
        saveUnit(hvacIndex);
}

bool HVACPanel::isFanValid(uint8_t fan) const
{
    return fan < FAN_COUNT && currentState().validFan[fan];
}