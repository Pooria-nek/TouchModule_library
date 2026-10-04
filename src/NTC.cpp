#include "NTC.h"

#include <math.h>

NTC::NTC(
    uint8_t pin,
    float fixedResistance,
    uint32_t intervalMs,
    uint8_t movingAverageWindow)
    : pin_(pin),
      fixedResistance_(fixedResistance),
      intervalMs_(intervalMs),
      lastRead_(0),
      adc_(0),
      resistance_(0.0f),
      temperature_(0.0f),
      runningSum_(0.0f),
      bufferIndex_(0),
      bufferCount_(0),
      movingAverageWindow_(movingAverageWindow),
      valid_(false)
{
    if (movingAverageWindow_ == 0)
        movingAverageWindow_ = 1;

    if (movingAverageWindow_ > MAX_AVERAGE_WINDOW)
        movingAverageWindow_ = MAX_AVERAGE_WINDOW;

    for (uint8_t i = 0; i < MAX_AVERAGE_WINDOW; i++)
        temperatureBuffer_[i] = 0.0f;
}

void NTC::begin()
{
    pinMode(pin_, INPUT_ANALOG);

    analogReadResolution(12);

    lastRead_ = millis() - intervalMs_;

    adc_ = 0;
    resistance_ = 0.0f;
    temperature_ = 0.0f;

    runningSum_ = 0.0f;
    bufferIndex_ = 0;
    bufferCount_ = 0;

    valid_ = false;
}

void NTC::update()
{
    const uint32_t now = millis();

    if (now - lastRead_ < intervalMs_)
        return;

    lastRead_ = now;

    read();
}

void NTC::read()
{
    constexpr float ADC_MAX = 4095.0f;

    // Steinhart-Hart coefficients
    constexpr float C1 = 1.009249522e-03f;
    constexpr float C2 = 2.378405444e-04f;
    constexpr float C3 = 2.019202697e-07f;

    adc_ = analogRead(pin_);

    if (adc_ == 0 || adc_ >= ADC_MAX)
    {
        valid_ = false;
        return;
    }

    // 3.3V
    //  |
    // R_FIXED
    //  |
    //  +------ ADC
    //  |
    // NTC
    //  |
    // GND

    resistance_ =
        fixedResistance_ *
        static_cast<float>(adc_) /
        (ADC_MAX - static_cast<float>(adc_));

    if (resistance_ <= 0.0f)
    {
        valid_ = false;
        return;
    }

    const float logR = logf(resistance_);

    const float temperatureK =
        1.0f /
        (C1 +
         C2 * logR +
         C3 * logR * logR * logR);

    const float temperatureC =
        temperatureK - 273.15f;

    addToBuffer(temperatureC);

    temperature_ = getMovingAverage();

    valid_ = true;
}

void NTC::addToBuffer(float value)
{
    runningSum_ -= temperatureBuffer_[bufferIndex_];

    temperatureBuffer_[bufferIndex_] = value;

    runningSum_ += value;

    bufferIndex_++;

    if (bufferIndex_ >= movingAverageWindow_)
        bufferIndex_ = 0;

    if (bufferCount_ < movingAverageWindow_)
        bufferCount_++;
}

float NTC::getMovingAverage() const
{
    if (bufferCount_ == 0)
        return 0.0f;

    return runningSum_ / bufferCount_;
}

float NTC::temperature() const
{
    return temperature_;
}

float NTC::resistance() const
{
    return resistance_;
}

uint16_t NTC::adc() const
{
    return adc_;
}

bool NTC::valid() const
{
    return valid_;
}

void NTC::setCalibration(float offset)
{
    if (offset > 8.0f)
        offset = 8.0f;

    if (offset < -8.0f)
        offset = -8.0f;

    calibrationOffset_ = offset;

    // Immediately apply the new calibration
    if (valid_)
        temperature_ =
            getMovingAverage() +
            calibrationOffset_;
}

float NTC::calibration() const
{
    return calibrationOffset_;
}