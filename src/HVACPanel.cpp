#include "HVACPanel.h"

HVACPanel::HVACPanel()
    : currentHvac_(0)
{
}

// =====================================================
// HVAC selection
// =====================================================

void HVACPanel::setCurrent(uint8_t index)
{
    if (index < HVAC_COUNT)
        currentHvac_ = index;
}

uint8_t HVACPanel::current() const
{
    return currentHvac_;
}

void HVACPanel::next()
{
    currentHvac_ = (currentHvac_ + 1) % HVAC_COUNT;
}

void HVACPanel::previous()
{
    currentHvac_ = (currentHvac_ == 0) ? HVAC_COUNT - 1 : currentHvac_ - 1;
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
    if (index >= HVAC_COUNT)
        index = 0;

    return hvac_[index];
}

const HVACPanel::State &HVACPanel::get(uint8_t index) const
{
    if (index >= HVAC_COUNT)
        index = 0;

    return hvac_[index];
}

// =====================================================
// Power
// =====================================================

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

// =====================================================
// Temperature
// =====================================================

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

// =====================================================
// Mode
// =====================================================

void HVACPanel::setMode(uint8_t mode)
{
    if (mode < MODE_COUNT)
        currentState().mode = mode;
}

uint8_t HVACPanel::mode() const
{
    return currentState().mode;
}

void HVACPanel::setModeValid(uint8_t mode, bool valid)
{
    if (mode < MODE_COUNT)
        currentState().validMode[mode] = valid;
}

bool HVACPanel::isModeValid(uint8_t mode) const
{
    if (mode >= MODE_COUNT)
        return false;

    return currentState().validMode[mode];
}

// =====================================================
// Fan
// =====================================================

void HVACPanel::setFan(uint8_t fan)
{
    if (fan < FAN_COUNT)
        currentState().fan = fan;
}

uint8_t HVACPanel::fan() const
{
    return currentState().fan;
}

void HVACPanel::setFanValid(uint8_t fan, bool valid)
{
    if (fan < FAN_COUNT)
        currentState().validFan[fan] = valid;
}

bool HVACPanel::isFanValid(uint8_t fan) const
{
    if (fan >= FAN_COUNT)
        return false;

    return currentState().validFan[fan];
}