#include "FloorHeatPanel.h"

namespace
{
    constexpr float kMinSetTemp = 5.0f;
    constexpr float kMaxSetTemp = 35.0f;

    // First configured unit, or 0 when none are valid.
    uint8_t firstValidUnit(const FloorHeatPanel::State *states)
    {
        for (uint8_t i = 0; i < FloorHeatPanel::FHEAT_COUNT; ++i)
        {
            if (states[i].valid)
                return i;
        }
        return 0;
    }

    // Guards against an out-of-range mode written through get().
    uint8_t modeIndex(const FloorHeatPanel::State &state)
    {
        return state.mode < FloorHeatPanel::MODE_COUNT ? state.mode : 0;
    }
}

FloorHeatPanel::FloorHeatPanel(MemoryCore &flash)
    : flash_(flash)
{
}

// =====================================================
// Flash
// =====================================================

uint32_t FloorHeatPanel::unitAddress(uint8_t index)
{
    return flash_.findAdrress(MemoryAdress::Touch::SECTOR_INFO,
                              MemoryAdress::Touch::FHEAT_ADDRESSES[index]);
}

void FloorHeatPanel::load()
{
    for (uint8_t i = 0; i < FHEAT_COUNT; ++i)
    {
        fetchUnit(i);
        lastSent_[i] = ControlSnapshot{};
    }

    fetchImage();
    currentUnit_ = firstValidUnit(floorheat_);
}

bool FloorHeatPanel::update()
{
    const State &state = floorheat_[currentUnit_];

    if (!state.valid)
        return false;

    const ControlSnapshot current{true, state.power, state.mode, state.setTemps[modeIndex(state)]};
    ControlSnapshot &last = lastSent_[currentUnit_];

    if (last.valid &&
        last.power == current.power &&
        last.mode == current.mode &&
        last.setTemp == current.setTemp)
        return false;

    last = current;
    return true;
}

void FloorHeatPanel::fetchUnit(uint8_t index)
{
    if (index >= FHEAT_COUNT)
        return;

    FlashHvac data{};
    flash_.read(unitAddress(index), reinterpret_cast<uint8_t *>(&data), sizeof(data));

    State state{};

    // Erased/invalid records (valid > 1) stay at defaults with valid = 0.
    if (data.valid <= 1)
    {
        state.valid = data.valid;
        state.deviceSn = data.deviceSn;
        state.deviceId = data.deviceId;
        state.channel = data.channel;
        state.power = data.power != 0;
        state.mode = data.mode < MODE_COUNT ? data.mode : 0;
    }

    floorheat_[index] = state;
}

void FloorHeatPanel::saveUnit(uint8_t index)
{
    if (index >= FHEAT_COUNT)
        return;

    const State &state = floorheat_[index];

    FlashHvac data{};
    data.valid = state.valid ? 1 : 0;
    data.deviceSn = state.deviceSn;
    data.deviceId = state.deviceId;
    data.channel = state.channel;
    data.power = state.power ? 1 : 0;
    data.mode = state.mode;

    flash_.update(unitAddress(index), reinterpret_cast<uint8_t *>(&data), sizeof(data));
}

void FloorHeatPanel::saveCurrent()
{
    saveUnit(currentUnit_);
}

void FloorHeatPanel::save(uint8_t index)
{
    saveUnit(index);
}

void FloorHeatPanel::fetchImage(uint8_t index)
{
    if (index >= FHEAT_COUNT)
        return;

    const uint32_t address = flash_.findAdrress(
        MemoryAdress::Touch::SECTOR_ICONS,
        MemoryAdress::Touch::FHEAT_IMAGE[index]);
    flash_.read(address, images_[index], FHEAT_IMAGE_SIZE);
}

void FloorHeatPanel::fetchImage()
{
    for (uint8_t i = 0; i < FHEAT_COUNT; ++i)
        fetchImage(i);
}

// =====================================================
// Unit selection
// =====================================================

const uint8_t *FloorHeatPanel::currentImage() const
{
    return floorheat_[currentUnit_].valid ? images_[currentUnit_] : nullptr;
}

void FloorHeatPanel::setCurrentUnit(uint8_t index)
{
    if (index < FHEAT_COUNT && floorheat_[index].valid)
        currentUnit_ = index;
}

uint8_t FloorHeatPanel::currentUnit() const
{
    return currentUnit_;
}

bool FloorHeatPanel::changeUnit(bool forward)
{
    const uint8_t step = forward ? 1 : FHEAT_COUNT - 1;
    uint8_t unit = currentUnit_;

    // Steps 1..FHEAT_COUNT-1 never land back on the starting unit.
    for (uint8_t i = 1; i < FHEAT_COUNT; ++i)
    {
        unit = (unit + step) % FHEAT_COUNT;
        if (floorheat_[unit].valid)
        {
            currentUnit_ = unit;
            return true;
        }
    }
    return false;
}

FloorHeatPanel::State &FloorHeatPanel::currentState()
{
    return floorheat_[currentUnit_];
}

const FloorHeatPanel::State &FloorHeatPanel::currentState() const
{
    return floorheat_[currentUnit_];
}

FloorHeatPanel::State &FloorHeatPanel::get(uint8_t index)
{
    if (index < FHEAT_COUNT)
        return floorheat_[index];

    static State invalid;
    invalid = State{}; // discard anything a caller wrote last time
    return invalid;
}

const FloorHeatPanel::State &FloorHeatPanel::get(uint8_t index) const
{
    static const State invalid{};
    return index < FHEAT_COUNT ? floorheat_[index] : invalid;
}

uint16_t FloorHeatPanel::address() const
{
    const State &state = currentState();
    return (static_cast<uint16_t>(state.deviceSn) << 8) | state.deviceId;
}

// =====================================================
// Power
// =====================================================

void FloorHeatPanel::setPower(bool value)
{
    currentState().power = value;
}

bool FloorHeatPanel::power() const
{
    return currentState().power;
}

bool FloorHeatPanel::setPower(uint8_t index, bool power)
{
    if (index >= FHEAT_COUNT)
        return false;

    State &state = floorheat_[index];

    if (!state.valid || state.power == power)
        return false;

    state.power = power;
    return true;
}


bool FloorHeatPanel::setMode(uint8_t index, uint8_t mode)
{
    if (index >= FHEAT_COUNT)
        return false;

    State &state = floorheat_[index];

    if (!state.valid || state.mode == mode)
        return false;

    state.mode = mode;
    return true;
}

void FloorHeatPanel::togglePower()
{
    State &state = currentState();
    state.power = !state.power;
}

// =====================================================
// Temperature
// =====================================================

void FloorHeatPanel::setCurrentTemp(float temp)
{
    currentState().currentTemp = temp;
}

float FloorHeatPanel::currentTemp() const
{
    return currentState().currentTemp;
}

float FloorHeatPanel::getSetTempNormal() const { return currentState().setTemps[Normal]; }
float FloorHeatPanel::getSetTempDay() const { return currentState().setTemps[Day]; }
float FloorHeatPanel::getSetTempNight() const { return currentState().setTemps[Night]; }
float FloorHeatPanel::getSetTempAway() const { return currentState().setTemps[Away]; }

float FloorHeatPanel::setTemp() const
{
    const State &state = currentState();
    return state.setTemps[modeIndex(state)];
}

bool FloorHeatPanel::setSetTemp(float temp)
{
    if (temp != temp) // NaN
        return false;

    State &state = currentState();
    float &target = state.setTemps[modeIndex(state)];
    const float clamped = constrain(temp, kMinSetTemp, kMaxSetTemp);

    if (target == clamped)
        return false;

    target = clamped;
    return true;
}

bool FloorHeatPanel::setSetTempNormal(uint8_t index, uint8_t temp)
{
    if (index >= FHEAT_COUNT)
        return false;

    State &state = floorheat_[index];

    if (!state.valid || state.setTemps[0] == temp)
        return false;

    state.setTemps[0] = temp;
    return true;
}

bool FloorHeatPanel::setSetTempDay(uint8_t index, uint8_t temp)
{
    if (index >= FHEAT_COUNT)
        return false;

    State &state = floorheat_[index];

    if (!state.valid || state.setTemps[1] == temp)
        return false;

    state.setTemps[1] = temp;
    return true;
}

bool FloorHeatPanel::setSetTempNight(uint8_t index, uint8_t temp)
{
    if (index >= FHEAT_COUNT)
        return false;

    State &state = floorheat_[index];

    if (!state.valid || state.setTemps[2] == temp)
        return false;

    state.setTemps[2] = temp;
    return true;
}

bool FloorHeatPanel::setSetTempAway(uint8_t index, uint8_t temp)
{
    if (index >= FHEAT_COUNT)
        return false;

    State &state = floorheat_[index];

    if (!state.valid || state.setTemps[3] == temp)
        return false;

    state.setTemps[3] = temp;
    return true;
}



bool FloorHeatPanel::increaseTemp(float amount)
{
    if (!(amount > 0.0f && amount <= 100.0f))
        return false;

    return setSetTemp(setTemp() + amount);
}

bool FloorHeatPanel::decreaseTemp(float amount)
{
    if (!(amount > 0.0f && amount <= 100.0f))
        return false;

    return setSetTemp(setTemp() - amount);
}

// =====================================================
// Mode
// =====================================================

void FloorHeatPanel::setMode(uint8_t mode)
{
    if (mode < MODE_COUNT)
        currentState().mode = mode;
}

uint8_t FloorHeatPanel::mode() const
{
    return currentState().mode;
}

bool FloorHeatPanel::nextMode()
{
    State &state = currentState();
    state.mode = (state.mode + 1) % MODE_COUNT;
    return true;
}

bool FloorHeatPanel::previousMode()
{
    State &state = currentState();
    state.mode = (state.mode + MODE_COUNT - 1) % MODE_COUNT;
    return true;
}

// =====================================================
// Membership
// =====================================================

void FloorHeatPanel::setFheatMember(uint8_t index, bool enabled, uint8_t deviceSn, uint8_t deviceId, uint8_t channel)
{
    if (index >= FHEAT_COUNT)
        return;

    State &state = floorheat_[index];
    state.valid = enabled;
    state.deviceSn = deviceSn;
    state.deviceId = deviceId;
    state.channel = channel;

    lastSent_[index] = ControlSnapshot{}; // force a resend after reconfiguration
    saveUnit(index);

    if (!enabled && currentUnit_ == index)
        currentUnit_ = firstValidUnit(floorheat_);
    else if (enabled && !floorheat_[currentUnit_].valid)
        currentUnit_ = index;
}