#pragma once

#include "BusproFrame.h"

class TouchModule;

class TouchController
{
public:
    explicit TouchController(TouchModule &module);

    void handleSingleChannelControl(const BusproFrame &frame);
    void handleReversingControl(const BusproFrame &frame);
    void handleReadStatusRequest(const BusproFrame &frame);

private:
    TouchModule &module_;

    uint8_t relayToBrightness(uint8_t value);
};