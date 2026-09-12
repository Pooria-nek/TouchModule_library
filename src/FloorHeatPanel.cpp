#include "FloorHeatPanel.h"

FloorHeatPanel::FloorHeatPanel()
    : currentZone_(0)
{
}

// =====================================================
// Zone selection
// =====================================================

void FloorHeatPanel::setCurrent(uint8_t index)
{
    if (index < ZONE_COUNT)
        currentZone_ = index;
}

uint8_t FloorHeatPanel::current() const
{
    return currentZone_;
}

FloorHeatPanel::State &FloorHeatPanel::get(uint8_t index)
{
    if (index >= ZONE_COUNT)
        index = 0;

    return zones_[index];
}

const FloorHeatPanel::State &FloorHeatPanel::get(uint8_t index) const
{
    if (index >= ZONE_COUNT)
        index = 0;

    return zones_[index];
}

FloorHeatPanel::State &FloorHeatPanel::currentState()
{
    return zones_[currentZone_];
}

const FloorHeatPanel::State &FloorHeatPanel::currentState() const
{
    return zones_[currentZone_];
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

void FloorHeatPanel::togglePower()
{
    currentState().power = !currentState().power;
}

// =====================================================
// Temperature
// =====================================================

void FloorHeatPanel::setCurrentTemp(float value)
{
    currentState().currentTemp = value;
}

float FloorHeatPanel::currentTemp() const
{
    return currentState().currentTemp;
}

void FloorHeatPanel::setSetTemp(float value)
{
    currentState().setTemp = value;
}

float FloorHeatPanel::setTemp() const
{
    return currentState().setTemp;
}

void FloorHeatPanel::increaseTemp(float amount)
{
    currentState().setTemp += amount;
}

void FloorHeatPanel::decreaseTemp(float amount)
{
    currentState().setTemp -= amount;
}

// =====================================================
// Mode
// =====================================================

void FloorHeatPanel::setMode(uint8_t mode)
{
    currentState().mode = mode;
}

uint8_t FloorHeatPanel::mode() const
{
    return currentState().mode;
}

// =====================================================
// Speed
// =====================================================

void FloorHeatPanel::setSpeed(uint8_t speed)
{
    if (speed < SPEED_COUNT)
        currentState().speed = speed;
}

uint8_t FloorHeatPanel::speed() const
{
    return currentState().speed;
}

void FloorHeatPanel::setSpeedValid(uint8_t speed, bool valid)
{
    if (speed < SPEED_COUNT)
        currentState().validSpeed[speed] = valid;
}

bool FloorHeatPanel::isSpeedValid(uint8_t speed) const
{
    if (speed >= SPEED_COUNT)
        return false;

    return currentState().validSpeed[speed];
}