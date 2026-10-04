#pragma once

#include <Arduino.h>

class NTC
{
public:
    NTC(
        uint8_t pin,
        float fixedResistance,
        uint32_t intervalMs,
        uint8_t movingAverageWindow = 10);

    void begin();
    void update();

    float temperature() const;
    float resistance() const;
    uint16_t adc() const;
    bool valid() const;

    void setCalibration(float offset);
    float calibration() const;

private:
    void read();
    void addToBuffer(float value);
    float getMovingAverage() const;

    uint8_t pin_;

    float fixedResistance_;
    uint32_t intervalMs_;
    uint32_t lastRead_;

    uint16_t adc_;
    float resistance_;
    float temperature_;

    float calibrationOffset_;

    // Moving average
    static constexpr uint8_t MAX_AVERAGE_WINDOW = 20;

    float temperatureBuffer_[MAX_AVERAGE_WINDOW];
    float runningSum_;
    uint8_t bufferIndex_;
    uint8_t bufferCount_;
    uint8_t movingAverageWindow_;

    bool valid_;
};