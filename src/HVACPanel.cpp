#include "HVACPanel.h"

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
    loadValidHvac();
    loadImages();

    for (uint8_t i = 0; i < HVAC_COUNT; ++i)
    {
        if (hvac_[i].valid)
            loadHvac(i);
    }

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

void HVACPanel::loadValidHvac()
{
    uint8_t validMask = 0;

    flash_.read(
        flash_.findAdrress(
            MemoryAdress::Touch::SECTOR_INFO,
            MemoryAdress::Touch::HVAC_VALID),
        &validMask,
        sizeof(validMask));

    for (uint8_t i = 0; i < HVAC_COUNT; ++i)
    {
        hvac_[i].valid =
            (validMask & (1 << i)) != 0;
    }
}

void HVACPanel::loadHvac(uint8_t index)
{
    if (index >= HVAC_COUNT)
        return;

    FlashHvac data{};

    flash_.read(
        flash_.findAdrress(
            MemoryAdress::Touch::SECTOR_INFO,
            MemoryAdress::Touch::HVAC_ADDRESSES[index]),
        reinterpret_cast<uint8_t *>(&data),
        sizeof(data));

    fromFlash(index, data);
}

void HVACPanel::loadImages()
{
    for (uint8_t i = 0; i < HVAC_COUNT; ++i)
    {
        flash_.read(
            flash_.findAdrress(
                MemoryAdress::Touch::SECTOR_INFO,
                MemoryAdress::Touch::HVAC_IMAGE[i]),
            images_[i],
            HVAC_IMAGE_SIZE);
    }
}

// =================================================
// HVAC selection
// =================================================

const uint8_t *HVACPanel::currentImage() const
{
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

void HVACPanel::nextHvac()
{
    for (uint8_t offset = 1; offset <= HVAC_COUNT; ++offset)
    {
        uint8_t index =
            (currentHvac_ + offset) % HVAC_COUNT;

        if (hvac_[index].valid)
        {
            currentHvac_ = index;
            return;
        }
    }
}

void HVACPanel::previousHvac()
{
    for (uint8_t offset = 1; offset <= HVAC_COUNT; ++offset)
    {
        uint8_t index =
            (currentHvac_ + HVAC_COUNT - offset) % HVAC_COUNT;

        if (hvac_[index].valid)
        {
            currentHvac_ = index;
            return;
        }
    }
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
    return hvac_[index];
}

const HVACPanel::State &HVACPanel::get(uint8_t index) const
{
    return hvac_[index];
}

// =================================================
// HVAC validity
// =================================================

void HVACPanel::setHvacValid(uint8_t index, bool valid)
{
    if (index >= HVAC_COUNT)
        return;

    hvac_[index].valid = valid;

    if (!valid && currentHvac_ == index)
    {
        for (uint8_t offset = 1; offset <= HVAC_COUNT; ++offset)
        {
            uint8_t next =
                (index + offset) % HVAC_COUNT;

            if (hvac_[next].valid)
            {
                currentHvac_ = next;
                break;
            }
        }
    }

    if (valid && !hvac_[currentHvac_].valid)
    {
        currentHvac_ = index;
    }
}

bool HVACPanel::isHvacValid(uint8_t index) const
{
    if (index >= HVAC_COUNT)
        return false;

    return hvac_[index].valid;
}

uint8_t HVACPanel::hvacValidMask() const
{
    uint8_t mask = 0;

    for (uint8_t i = 0; i < HVAC_COUNT; ++i)
    {
        if (hvac_[i].valid)
            mask |= (1 << i);
    }

    return mask;
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

void HVACPanel::setSetTemp(float temp)
{
    currentState().setTemp = temp;
}

float HVACPanel::setTemp() const
{
    return currentState().setTemp;
}

void HVACPanel::increaseTemp(float amount)
{
    currentState().setTemp += amount;
}

void HVACPanel::decreaseTemp(float amount)
{
    currentState().setTemp -= amount;
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

void HVACPanel::nextMode()
{
    State &state = currentState();

    for (uint8_t offset = 1; offset <= MODE_COUNT; ++offset)
    {
        uint8_t index = (state.mode + offset) % MODE_COUNT;

        if (state.validMode[index])
        {
            state.mode = index;
            return;
        }
    }
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
    if (mode >= MODE_COUNT)
        return;

    State &state = currentState();

    state.validMode[mode] = valid;

    if (!valid && state.mode == mode)
    {
        for (uint8_t i = 0; i < MODE_COUNT; ++i)
        {
            if (state.validMode[i])
            {
                state.mode = i;
                return;
            }
        }
    }

    // If there was no valid mode before and one is now enabled.
    if (valid)
    {
        bool hasValidMode = false;

        for (uint8_t i = 0; i < MODE_COUNT; ++i)
        {
            if (state.validMode[i])
            {
                hasValidMode = true;
                break;
            }
        }

        if (hasValidMode && !state.validMode[state.mode])
        {
            state.mode = mode;
        }
    }
}

bool HVACPanel::isModeValid(uint8_t mode) const
{
    if (mode >= MODE_COUNT)
        return false;

    return currentState().validMode[mode];
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

void HVACPanel::nextFan()
{
    State &state = currentState();

    for (uint8_t offset = 1; offset <= FAN_COUNT; ++offset)
    {
        uint8_t index =
            (state.fan + offset) % FAN_COUNT;

        if (state.validFan[index])
        {
            state.fan = index;
            return;
        }
    }
}

void HVACPanel::previousFan()
{
    State &state = currentState();

    for (uint8_t offset = 1; offset <= FAN_COUNT; ++offset)
    {
        uint8_t index =
            (state.fan + FAN_COUNT - offset) % FAN_COUNT;

        if (state.validFan[index])
        {
            state.fan = index;
            return;
        }
    }
}

void HVACPanel::setFanValid(uint8_t fan, bool valid)
{
    if (fan >= FAN_COUNT)
        return;

    State &state = currentState();

    state.validFan[fan] = valid;

    if (!valid && state.fan == fan)
    {
        for (uint8_t i = 0; i < FAN_COUNT; ++i)
        {
            if (state.validFan[i])
            {
                state.fan = i;
                return;
            }
        }
    }

    // If there was no valid fan before and one is now enabled.
    if (valid)
    {
        bool hasValidFan = false;

        for (uint8_t i = 0; i < FAN_COUNT; ++i)
        {
            if (state.validFan[i])
            {
                hasValidFan = true;
                break;
            }
        }

        if (hasValidFan && !state.validFan[state.fan])
        {
            state.fan = fan;
        }
    }
}

bool HVACPanel::isFanValid(uint8_t fan) const
{
    if (fan >= FAN_COUNT)
        return false;

    return currentState().validFan[fan];
}

// =================================================
// Flash conversion
// =================================================

HVACPanel::FlashHvac HVACPanel::toFlash(uint8_t index) const
{
    FlashHvac data{};

    if (index >= HVAC_COUNT)
        return data;

    const State &state = hvac_[index];

    data.power = state.power ? 1 : 0;
    data.setTemp = state.setTemp;
    data.mode = state.mode;
    data.fan = state.fan;

    for (uint8_t i = 0; i < MODE_COUNT; ++i)
    {
        if (state.validMode[i])
            data.validModeMask |= (1 << i);
    }

    for (uint8_t i = 0; i < FAN_COUNT; ++i)
    {
        if (state.validFan[i])
            data.validFanMask |= (1 << i);
    }

    return data;
}

void HVACPanel::fromFlash(
    uint8_t index,
    const FlashHvac &data)
{
    if (index >= HVAC_COUNT)
        return;

    State &state = hvac_[index];

    state.power = data.power != 0;
    state.setTemp = data.setTemp;
    state.mode = data.mode;
    state.fan = data.fan;

    for (uint8_t i = 0; i < MODE_COUNT; ++i)
    {
        state.validMode[i] =
            (data.validModeMask & (1 << i)) != 0;
    }

    for (uint8_t i = 0; i < FAN_COUNT; ++i)
    {
        state.validFan[i] =
            (data.validFanMask & (1 << i)) != 0;
    }
}