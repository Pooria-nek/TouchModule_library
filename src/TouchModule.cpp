#include "TouchModule.h"

TouchModule::TouchModule(
    TwoWire &wirePort,
    BusproTransport &bus,
    MemoryCore &flash,
    uint32_t sectorAddress,
    const uint8_t ledPins[TOUCH_CHANNEL_COUNT],
    const uint8_t touchPads[TOUCH_CHANNEL_COUNT],
    bool activeHigh)
    : _wire(wirePort),
      bus_(bus),
      flash_(flash),
      memoryaddress_(sectorAddress),
      activeHigh_(activeHigh)
{
    mcu::copyMcuUID(uid_);
    for (uint8_t i = 0; i < TOUCH_CHANNEL_COUNT; i++)
    {
        touchPins_[i] = touchPads[i];
    }

    leds_.configure(ledPins, activeHigh);
}

bool TouchModule::begin()
{
    _wire.begin();

    initBS8112();

    leds_.begin();

    // flash_.eraseSector(memoryaddress_);

    if (firstime())
    {
        init();
    }

    // Boot sweep is done — hand LEDs over to the state machine, starting Deactive.
    leds_.setAllLedMode(LedMode::Deactive);

    leds_.startupAnimation();

    return syncValues();
}

void TouchModule::update()
{
    // Poll the BS8112 for new touch state (call every loop iteration)
    updateBS8112();

    // Non-blocking — advances LED blink timing / output (call every loop)
    leds_.updateLeds();
}

bool TouchModule::firstime()
{
    uint8_t fuid_[12];
    flash_.read(flash_.findAdrress(memoryaddress_, MemoryAdress::DEVICE_MAC_ADDRESS), fuid_, 12);

    // bool sameUid = (memcmp(fuid_, getMcuUID(), 12) == 0);

    uint16_t fdevType;
    flash_.readObject(flash_.findAdrress(memoryaddress_, MemoryAdress::DEVICE_TYPE), fdevType);

    // bool sameType = (devType_ == fdevType);

    if ((!mcu::bufferEquals(fuid_, uid_, 12)) || (devType_ != fdevType))
    {
        return true;
    }
    else
    {
        return false;
    }
}

bool TouchModule::init()
{
    flash_.writeObject(flash_.findAdrress(memoryaddress_, MemoryAdress::DEVICE_ADDRESS), deviceAddress_);
    flash_.writeObject(flash_.findAdrress(memoryaddress_, MemoryAdress::DEVICE_TYPE), devType_);

    flash_.writeObject(flash_.findAdrress(memoryaddress_, MemoryAdress::DEVICE_MAC_ADDRESS), uid_);

    flash_.writeObject(flash_.findAdrress(memoryaddress_, MemoryAdress::DEVICE_REMARK), "Touch");
    flash_.writeObject(flash_.findAdrress(memoryaddress_, MemoryAdress::DEVICE_HARDWARE_VER), "hardware");
    flash_.writeObject(flash_.findAdrress(memoryaddress_, MemoryAdress::DEVICE_SOFTWARE_VER), "software");

    char remark[20];
    for (size_t i = 1; i <= TOUCH_CHANNEL_COUNT; i++)
    {
        // uint8_t channel = i + 1;
        snprintf(remark, sizeof(remark), "Touch %u", static_cast<unsigned>(i));
        flash_.writeObject(flash_.findAdrress(memoryaddress_, MemoryAdress::channelRemark(i)), remark);

        flash_.writeObject(flash_.findAdrress(memoryaddress_, MemoryAdress::channelAddress(MemoryAdress::RELAY_CHANNEL_ENABLE, i)), uint8_t{0x01});
        flash_.writeObject(flash_.findAdrress(memoryaddress_, MemoryAdress::channelAddress(MemoryAdress::RELAY_CHANNEL_ONDELAY, i)), uint8_t{0x00});
        flash_.writeObject(flash_.findAdrress(memoryaddress_, MemoryAdress::channelAddress(MemoryAdress::RELAY_CHANNEL_ONPROTECT, i)), uint8_t{0x00});
    }

    for (size_t z = 0; z < TOUCH_CHANNEL_COUNT; z++)
    {
        snprintf(remark, sizeof(remark), "Zone %u", static_cast<unsigned>(z));
        flash_.writeObject(flash_.findAdrress(memoryaddress_, MemoryAdress::zoneRemark(memoryaddress_, z)), remark);

        for (size_t s = 0; s < (TOUCH_CHANNEL_COUNT * 2); s++)
        {
            /* code */
        }
    }

    return true;
}

bool TouchModule::syncValues()
{
    flash_.readObject(flash_.findAdrress(memoryaddress_, MemoryAdress::DEVICE_ADDRESS), deviceAddress_);
    flash_.readObject(flash_.findAdrress(memoryaddress_, MemoryAdress::DEVICE_MAC_ADDRESS), uid_);
    // flash_.readObject(flash_.findAdrress(memoryaddress_, MemoryAdress::RELAY_CHANNEL_ENABLE), relayEnable_);
    // flash_.readObject(flash_.findAdrress(memoryaddress_, MemoryAdress::RELAY_CHANNEL_ONDELAY), relayDelay_);
    // flash_.readObject(flash_.findAdrress(memoryaddress_, MemoryAdress::RELAY_CHANNEL_ONPROTECT), relayProtect_);

    maxZone();

    return true;
}

uint8_t TouchModule::maxZone()
{
    uint8_t payload[TOUCH_CHANNEL_COUNT];

    flash_.read(flash_.findAdrress(memoryaddress_, MemoryAdress::RELAY_CHANNEL_ZONE), payload, TOUCH_CHANNEL_COUNT);

    for (uint8_t i = 0; i < TOUCH_CHANNEL_COUNT; ++i)
    {
        if (payload[i] > sceneCount)
        {
            sceneCount = payload[i];
        }
    }

    return sceneCount;
}

////////////////////////////////////////////////////////////////////////////////
/////////////////////////////// BS811x Functions ///////////////////////////////

void TouchModule::initBS8112()
{
    _touchState = 0;

    // BS8112 Initialization
    uint8_t config[17];
    uint8_t KeyTriggerthresholdvalue = 12; // 1-32

    // BS8112 configuration bytes
    config[0] = 0b00000001;                // B0H: IRQ one-shot enabled
    config[1] = 0b00000000;                // B1H
    config[2] = 0x83;                      // B2H
    config[3] = 0xF3;                      // B3H
    config[4] = 0b10011000;                // B4H: Powersave
    config[5] = 0b10011000;                // B5H: Wakeup
    config[6] = KeyTriggerthresholdvalue;  // B6H K2
    config[7] = KeyTriggerthresholdvalue;  // B7H K3
    config[8] = KeyTriggerthresholdvalue;  // B8H K4
    config[9] = KeyTriggerthresholdvalue;  // B9H K5
    config[10] = KeyTriggerthresholdvalue; // BAH K6
    config[11] = KeyTriggerthresholdvalue; // BBH K7
    config[12] = KeyTriggerthresholdvalue; // BCH K8
    config[13] = KeyTriggerthresholdvalue; // BDH K9
    config[14] = KeyTriggerthresholdvalue; // BEH K10
    config[15] = KeyTriggerthresholdvalue; // BFH K11
    config[16] = 0b11011000;               // C0H K12 ENABLE IRQ

    // Calculate checksum for register block transfer
    uint8_t checksum = 0;
    for (uint8_t i = 0; i < 17; i++)
        checksum += config[i];

    // Write configuration to hardware
    _wire.beginTransmission(touch_address);
    _wire.write(0xB0); // Start register
    for (uint8_t i = 0; i < 17; i++)
        _wire.write(config[i]);
    _wire.write(checksum);

    _wire.endTransmission();
}

void TouchModule::irqHandler()
{
    _irqFlag = true;
}

// // In the header, alongside the other private members:
// static const uint8_t TOUCH_PADS[] = {11, 2, 10, 3};
// static const uint8_t TOUCH_CHANNEL_COUNT = sizeof(TOUCH_PADS) / sizeof(TOUCH_PADS[0]);

/**
 * @brief Polls the hardware for state changes on the configured pads only.
 * @return true if any key state changed, false otherwise.
 */
bool TouchModule::updateBS8112()
{
    if (!_irqFlag && !_runAgain)
        return false;

    // Manage IRQ flag for continuous polling if required
    if (_irqFlag)
    {
        _irqFlag = false;
        _runAgain = true;
    }
    else
    {
        _runAgain = false;
    }

    // Read 2-byte touch status from the device
    uint16_t rawState = 0;
    _wire.beginTransmission(touch_address);
    _wire.write(0x08);
    _wire.endTransmission(false);
    if (_wire.requestFrom(touch_address, (uint8_t)2) == 2)
    {
        uint8_t low = _wire.read();
        uint8_t high = _wire.read();
        rawState = (static_cast<uint16_t>(high) << 8) | low;
    }

    // Remap only the configured physical pads into a compact bitfield,
    // where bit i of newState corresponds to TOUCH_PADS[i] (not the raw
    // hardware bit position). This replaces the old "& 0x0FFF, all 12 bits"
    // approach.
    uint16_t newState = 0;
    for (uint8_t i = 0; i < TOUCH_CHANNEL_COUNT; i++)
    {
        if (rawState & (1 << (touchPins_[i] - 1)))
            newState |= (1 << i);
    }

    // Detect edge transitions
    bool changed = (newState != _touchState);
    _prevTouchState = _touchState;
    _touchState = newState;
    _pressedEdge = (~_prevTouchState) & _touchState;
    _releasedEdge = _prevTouchState & (~_touchState);

    // Update timing for hold detection
    uint32_t now = millis();
    for (uint8_t key = 0; key < TOUCH_CHANNEL_COUNT; key++)
    {
        if (_pressedEdge & (1 << key))
        {
            _lastPressTime[key] = now;
            _holdActive &= ~(1 << key);

            // Acknowledge the touch with a single flash. If the channel is
            // mid-operation (Blinking), leave it alone — don't interrupt
            // a long-running action's blink with a fresh single-flash.
            if (leds_.getLedMode(key) != LedMode::Blinking)
                leds_.setLedMode(key, LedMode::Blink);
        }
    }

    return changed;
}

// it runs till you hold it
bool TouchModule::isTouched(uint8_t key)
{
    if (key >= TOUCH_CHANNEL_COUNT)
        return false;
    return (_touchState & (1 << key)) != 0;
}

// just run on touch edge once
bool TouchModule::isPressed(uint8_t key)
{
    if (key >= TOUCH_CHANNEL_COUNT)
        return false;
    return (_pressedEdge & (1 << key)) != 0;
}

// just run on release edge once
bool TouchModule::isReleased(uint8_t key)
{
    if (key >= TOUCH_CHANNEL_COUNT)
        return false;
    return (_releasedEdge & (1 << key)) != 0;
}

bool TouchModule::isHold(uint8_t key)
{
    if (key >= TOUCH_CHANNEL_COUNT)
        return false;

    if (isTouched(key))
    {
        uint32_t now = millis();
        if (!(_holdActive & (1 << key)) && (now - _lastPressTime[key] >= TOUCH_HOLD_TIME))
        {
            _holdActive |= (1 << key);
            return true;
        }
    }
    return false;
}

// bool TouchModule::isHoldRepeat(uint8_t key)
// {
//     if (key >= TOUCH_CHANNEL_COUNT)
//         return false;

//     uint32_t now = millis();

//     // Must be touched and the first hold must have happened
//     if (!isTouched(key) || !_holdActive[key])
//         return false;

//     // If time for next repeat
//     if (now >= _nextRepeatTime[key])
//     {
//         _nextRepeatTime[key] = now + TOUCH_HOLD_REPEAT;
//         return true;
//     }

//     return false;
// }

// bool TouchModule::isDoubleTap(uint8_t key)
// {
//     if (key >= TOUCH_CHANNEL_COUNT)
//         return false;

//     uint32_t now = millis();

//     if (isPressed(key))
//     {
//         if (_lastReleaseTime[key] != 0 &&
//             (now - _lastReleaseTime[key] <= TOUCH_DOUBLE_TAP_GAP) &&
//             (now - _lastPressTime[key] < TOUCH_HOLD_TIME)) // not a hold
//         {
//             return true;
//         }
//     }

//     return false;
// }

uint16_t TouchModule::convert(uint16_t touchState)
{
    uint16_t result = 0;

    // Map hardware bit output to software logical key index
    // if (touchState & (1 << X))  result |= (1 << Y);  // bit (X) -> bit (Y)
    if (touchState & (1 << 1))
        result |= (1 << 9);
    if (touchState & (1 << 2))
        result |= (1 << 1);
    if (touchState & (1 << 3))
        result |= (1 << 3);
    if (touchState & (1 << 4))
        result |= (1 << 5);
    if (touchState & (1 << 5))
        result |= (1 << 7);
    if (touchState & (1 << 6))
        result |= (1 << 10);
    if (touchState & (1 << 7))
        result |= (1 << 8);
    if (touchState & (1 << 8))
        result |= (1 << 6);
    if (touchState & (1 << 9))
        result |= (1 << 4);
    if (touchState & (1 << 10))
        result |= (1 << 2);

    return result;
}

void TouchModule::writeRegister(uint8_t reg, uint8_t value)
{
    _wire.beginTransmission(touch_address);
    _wire.write(reg);
    _wire.write(value);
    _wire.endTransmission();
}

uint8_t TouchModule::readRegister(uint8_t reg)
{
    _wire.beginTransmission(touch_address);
    _wire.write(reg);
    _wire.endTransmission(false);

    _wire.requestFrom(touch_address, (uint8_t)1);
    if (_wire.available())
        return _wire.read();

    return 0;
}

/////////////////////////////// BS811x Functions ///////////////////////////////
////////////////////////////////////////////////////////////////////////////////

// LED indicator logic (mode state machine + software PWM) now lives entirely
// in LedHandler.h — TouchModule just owns a LedHandler<TOUCH_CHANNEL_COUNT>
// instance (leds_) and forwards its public API (see TouchModule.h).

void TouchModule::process(const BusproFrame &frame)
{
    // if (frame.devType != devType_)
    //     return;

    if (frame.dstAddress == 0xFFFF) // universal comands
    {
        switch (frame.opCode)
        {
        case BusproOp::DEVICE_SEARCH_HDL.req():
            handleSearchDevice(frame);
            break;

        case BusproOp::DEVICE_REMARK.readReq():
            handleReadDeviceRemark(frame);
            break;
        }
    }
    else if (frame.dstAddress == deviceAddress_) // my comands
    {
        switch (frame.opCode)
        {
            /////////////////////////////// UNIVERSAL REQUEST ///////////////////////////////

        case BusproOp::DEVICE_FIRMWARE.req():
            handleReadFirmware(frame);
            break;

        case BusproOp::DEVICE_HARDWARE.req():
            handleReadHardware(frame);
            break;

        case BusproOp::DEVICE_FINDIT.req():
            handleFindDevice(frame);
            break;

        case BusproOp::DEVICE_SEARCH_HDL.req():
            handleSearchDevice(frame);
            break;

        case BusproOp::DEVICE_MAC_ADDRESS.readReq():
            handleReadMacaddress(frame);
            break;

        case BusproOp::DEVICE_MAC_ADDRESS.writeReq():
            handleModifyMacaddress(frame);
            break;

        case BusproOp::DEVICE_REMARK.readReq():
            handleReadDeviceRemark(frame);
            break;

        case BusproOp::DEVICE_REMARK.writeReq():
            handleModifyDeviceRemark(frame);
            break;

            /////////////////////////////// DEVICE REQUEST ///////////////////////////////
        }
    }
}

void TouchModule::sendResponse(uint16_t opcode, uint16_t dst, const uint8_t *payload, uint8_t payloadLen)
{
    bus_.send(deviceAddress_, devType_, opcode, dst, payload, payloadLen);
}

////////////////////////////////////////////////////////////////////////////////
/////////////////////////////// DEVICE HANDLERS ////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

/////////////////////////////// BASIC INFORMATION ///////////////////////////////

/////////////////////////////////////////////////////////////////////////////////
/////////////////////////////// UNIVERSAL REQUEST ///////////////////////////////
/////////////////////////////////////////////////////////////////////////////////

void TouchModule::handleReadFirmware(const BusproFrame &frame)
{
    if (frame.payloadLen != 0)
        return;

    uint8_t payload[32] = {0x00};

    flash_.readObject(flash_.findAdrress(memoryaddress_, MemoryAdress::DEVICE_SOFTWARE_VER), payload);

    sendResponse(BusproOp::DEVICE_FIRMWARE.resp(), frame.srcAddress, payload, sizeof(payload));
}

void TouchModule::handleReadHardware(const BusproFrame &frame)
{
    if (frame.payloadLen != 0)
        return;

    uint8_t payload[32] = {0x00};

    flash_.readObject(flash_.findAdrress(memoryaddress_, MemoryAdress::DEVICE_HARDWARE_VER), payload);

    sendResponse(BusproOp::DEVICE_HARDWARE.resp(), frame.srcAddress, payload, sizeof(payload));
}

void TouchModule::handleFindDevice(const BusproFrame &frame)
{
    if (frame.payloadLen != 0)
        return;

    uint8_t payload[8] = {0x00};

    sendResponse(BusproOp::DEVICE_FINDIT.resp(), frame.srcAddress, payload, sizeof(payload));
}

void TouchModule::handleSearchDevice(const BusproFrame &frame)
{
    // if (frame.payloadLen != 4)
    //     return;

    // if (frame.payload)
    // {
    //     /* code */
    // }

    uint8_t payload[32] = {0x00};

    payload[0] = frame.payload[0];
    payload[1] = frame.payload[1];

    sendResponse(BusproOp::DEVICE_SEARCH_HDL.resp(), frame.srcAddress, payload, sizeof(payload));
}

void TouchModule::handleReadMacaddress(const BusproFrame &frame)
{
    if (frame.payloadLen != 0)
        return;

    uint8_t payload[8] = {0x00};

    mcu::copyArray(mcu::getMcuUID(), payload, 8);

    sendResponse(BusproOp::DEVICE_MAC_ADDRESS.readResp(), frame.srcAddress, payload, sizeof(payload));
}

void TouchModule::handleModifyMacaddress(const BusproFrame &frame)
{
    if (frame.payloadLen != 10)
        return;

    if (!mcu::bufferEquals(frame.payload, mcu::getMcuUID()))
        return;

    uint16_t address = (static_cast<uint16_t>(frame.payload[8]) << 8) | frame.payload[9];

    flash_.updateObject(flash_.findAdrress(memoryaddress_, MemoryAdress::DEVICE_ADDRESS), address);
    syncValues();

    uint8_t payload[1] = {0xF8};
    sendResponse(BusproOp::DEVICE_MAC_ADDRESS.writeResp(), frame.srcAddress, payload, sizeof(payload));
}

void TouchModule::handleReadDeviceRemark(const BusproFrame &frame)
{
    if (frame.payloadLen != 0)
        return;

    uint8_t payload[20];

    flash_.readObject(flash_.findAdrress(memoryaddress_, MemoryAdress::DEVICE_REMARK), payload);

    sendResponse(BusproOp::DEVICE_REMARK.readResp(), 0xFFFF, payload, sizeof(payload));
}

void TouchModule::handleModifyDeviceRemark(const BusproFrame &frame)
{
    if (frame.payloadLen != 20)
        return;

    flash_.update(flash_.findAdrress(memoryaddress_, MemoryAdress::DEVICE_REMARK), frame.payload, frame.payloadLen);

    uint8_t payload[1] = {0xF8};
    sendResponse(BusproOp::DEVICE_REMARK.writeResp(), frame.srcAddress, payload, sizeof(payload));
}

/////////////////////////////////////////////////////////////////////////////////
/////////////////////////////// UNIVERSAL REQUEST ///////////////////////////////
/////////////////////////////////////////////////////////////////////////////////