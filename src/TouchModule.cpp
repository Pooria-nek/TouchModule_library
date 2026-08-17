#include "TouchModule.h"

TouchModule::TouchModule(
    TwoWire &wirePort,
    BusproTransport &bus,
    MemoryCore &flash,
    uint32_t sectorAddress,
    const uint8_t ledPins[TOUCH_CHANNEL_COUNT],
    const uint8_t touchPads[TOUCH_CHANNEL_COUNT],
    bool activeHigh)
    : wire_(wirePort)
      , bus_(bus)
      , flash_(flash)
      , memoryaddress_(sectorAddress)
      , activeHigh_(activeHigh)
#ifdef HAS_OLED_DISPLAY
      , display_(OLED_CS_PIN, OLED_DC_PIN, OLED_RS_PIN)
#endif
{
    // mcu::copyMcuUID(uid_);
    for (uint8_t i = 0; i < TOUCH_CHANNEL_COUNT; i++)
    {
        touchPins_[i] = touchPads[i];
    }

    leds_.configure(ledPins, activeHigh);
}

bool TouchModule::begin()
{
    wire_.begin();

    initBS8112();

    leds_.begin();

#ifdef HAS_OLED_DISPLAY
    display_.begin();
    display_.startupAnimation();
#endif

    flash_.eraseSector(memoryaddress_);

    if (firstime())
    {
        init();
    }

    // Boot sweep is done — hand LEDs over to the state machine, starting Deactive.
    leds_.setAllLedMode(LedMode::Deactive);

    leds_.startupAnimation();

    lastActivityTime_ = millis(); // start the idle timer from "device ready"

    return syncValues();
}

void TouchModule::update()
{
    // Poll the BS8112 for new touch state (call every loop iteration)
    updateBS8112();

    buttonUpdate();

    // Non-blocking — auto-sleep after sleepTimeoutMs_ with no activity
    checkAutoSleep();

    // Non-blocking — advances LED blink timing / output (call every loop)
    leds_.updateLeds();
}

void TouchModule::setKeyType(uint8_t key, ButtonType type)
{
    if (key >= TOUCH_CHANNEL_COUNT)
        return;

    keytype_[key] = type;
}

TouchModule::ButtonType TouchModule::getKeyType(uint8_t key)
{
    if (key >= TOUCH_CHANNEL_COUNT)
        return ButtonType::Invalid;
    return keytype_[key];
}

void TouchModule::buttonUpdate()
{
    for (size_t k = 0; k < TOUCH_CHANNEL_COUNT; k++)
    {
        if (isPressed(k))
        {
            if (leds_.getLedMode(k) == LedMode::Active)
            {
                leds_.setLedMode(k, LedMode::Deactive);
            }
            else
            {
                leds_.setLedMode(k, LedMode::Active);
            }

            // switch (getKeyType(k))
            // {
            // case ButtonType::SingleON:
            //     runSingleOn();
            //     break;

            // case ButtonType::SingleOFF:
            //     runSingleOff();
            //     break;

            // case ButtonType::SingleONOFF:
            //     runSingleOnOff();
            //     break;

            // case ButtonType::CombinationON:
            //     runCombinationOn();
            //     break;

            // case ButtonType::CombinationOFF:
            //     runCombinationOff();
            //     break;

            // case ButtonType::CombinationONOFF:
            //     runCombinationOnOff();
            //     break;

            // case ButtonType::DblclickSingle:
            //     runSingleOnOff();
            //     break;

            // case ButtonType::DblclickCombined:
            //     runCombinationOnOff();
            //     break;

            // case ButtonType::Momentary:
            //     runSingleOn();
            //     break;

            // case ButtonType::ShortLongPress:
            //     runCombinationOnOff();
            //     break;

            // case ButtonType::ShortLongJog:
            //     runCombinationOnOff();
            //     break;
            // }
        }
        else if (isReleased(k))
        {
            switch (getKeyType(k))
            {
            case ButtonType::Momentary:
                runSingleOff();
                break;
            }
        }
        else if (isHoldEdge(k))
        {
            switch (getKeyType(k))
            {
            case ButtonType::DblclickSingle:
                runSingleOnOff();
                break;

            case ButtonType::DblclickCombined:
                runCombinationOnOff();
                break;

            case ButtonType::ShortLongPress:
                runCombinationOnOff();
                break;
            }
        }
        else if (isHold(k))
        {
            switch (getKeyType(k))
            {
            case ButtonType::ShortLongJog:
                runCombinationOnOff();
                break;
            }
        }
    }
}

void TouchModule::runSingleOn()
{
}
void TouchModule::runSingleOff()
{
}
void TouchModule::runSingleOnOff()
{
}

void TouchModule::runCombinationOn()
{
}
void TouchModule::runCombinationOff()
{
}
void TouchModule::runCombinationOnOff()
{
}

bool TouchModule::firstime()
{
    uint8_t fuid_[12];
    flash_.read(flash_.findAdrress(memoryaddress_, MemoryAdress::DEVICE_MAC_ADDRESS), fuid_, 12);

    uint16_t fdevType;
    flash_.readObject(flash_.findAdrress(memoryaddress_, MemoryAdress::DEVICE_TYPE), fdevType);

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
        flash_.writeObject(flash_.findAdrress(memoryaddress_, MemoryAdress::zoneRemark(z)), remark);

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
    wire_.beginTransmission(touch_address);
    wire_.write(0xB0); // Start register
    for (uint8_t i = 0; i < 17; i++)
        wire_.write(config[i]);
    wire_.write(checksum);

    wire_.endTransmission();
}

void TouchModule::irqHandler()
{
    _irqFlag = true;
}

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
    wire_.beginTransmission(touch_address);
    wire_.write(0x08);
    wire_.endTransmission(false);
    if (wire_.requestFrom(touch_address, (uint8_t)2) == 2)
    {
        uint8_t low = wire_.read();
        uint8_t high = wire_.read();
        rawState = (static_cast<uint16_t>(high) << 8) | low;
    }

    // Remap only the configured physical pads into a compact bitfield,
    // where bit i of newState corresponds to TOUCH_PADS[i] (not the raw hardware bit position).
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

    if (_pressedEdge != 0)
        lastActivityTime_ = now; // any fresh press counts as activity

    for (uint8_t key = 0; key < TOUCH_CHANNEL_COUNT; key++)
    {
        if (_pressedEdge & (1 << key))
        {
            _lastPressTime[key] = now;
            _holdActive &= ~(1 << key);

            if (deviceMode_ == DeviceMode::Sleep)
            {
                // Any touch wakes the device — restores every channel's
                // stored high/low state, so skip the acknowledge flash below.
                wake();
            }
            else if (!keyHigh_[key] && leds_.getLedMode(key) != LedMode::Blinking)
            {
                // Acknowledge the touch with a single flash — but only for
                // channels currently "low". A channel already lit "high"
                // doesn't need it, and this avoids Blink's auto-return-to-
                // Deactive fighting with a channel that should stay high.
                // Also skipped while mid-operation (Blinking).
                leds_.setLedMode(key, LedMode::Blink);
            }
        }
    }

    return changed;
}

void TouchModule::sleep()
{
    deviceMode_ = DeviceMode::Sleep;
    leds_.setLedLevels(sleepLevel_, sleepLevel_, sleepLevel_);
    leds_.setAllLedMode(LedMode::Active); // uniform dim glow across every channel
}

void TouchModule::wake()
{
    deviceMode_ = DeviceMode::Wake;
    leds_.setLedLevels(wakeHighLevel_, wakeLowLevel_, wakeHighLevel_);
    for (uint8_t i = 0; i < TOUCH_CHANNEL_COUNT; i++)
        leds_.setLedMode(i, keyHigh_[i] ? LedMode::Active : LedMode::Deactive);

    lastActivityTime_ = millis(); // waking counts as activity — restart the idle timer
}

void TouchModule::checkAutoSleep()
{
    if (deviceMode_ == DeviceMode::Wake && (millis() - lastActivityTime_ >= sleepTimeoutMs_))
    {
        sleep();
    }
}

void TouchModule::setKeyHigh(uint8_t channel, bool high)
{
    if (channel >= TOUCH_CHANNEL_COUNT)
        return;

    keyHigh_[channel] = high;

    if (deviceMode_ == DeviceMode::Wake)
        leds_.setLedMode(channel, high ? LedMode::Active : LedMode::Deactive);
    // If asleep, the new state is just remembered — wake() will apply it.
}

bool TouchModule::isKeyHigh(uint8_t channel) const
{
    if (channel >= TOUCH_CHANNEL_COUNT)
        return false;
    return keyHigh_[channel];
}

void TouchModule::setSleepLevel(uint8_t level)
{
    sleepLevel_ = level;
    if (deviceMode_ == DeviceMode::Sleep)
        leds_.setLedLevels(sleepLevel_, sleepLevel_, sleepLevel_);
}

void TouchModule::setWakeLevels(uint8_t highLevel, uint8_t lowLevel)
{
    wakeHighLevel_ = highLevel;
    wakeLowLevel_ = lowLevel;
    if (deviceMode_ == DeviceMode::Wake)
        leds_.setLedLevels(wakeHighLevel_, wakeLowLevel_, wakeHighLevel_);
}

// true for the entire duration the channel is held down
bool TouchModule::isHold(uint8_t key)
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

// true once, the first time a channel has been held past TOUCH_HOLD_TIME
bool TouchModule::isHoldEdge(uint8_t key)
{
    if (key >= TOUCH_CHANNEL_COUNT)
        return false;

    if (isHold(key))
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

void TouchModule::writeRegister(uint8_t reg, uint8_t value)
{
    wire_.beginTransmission(touch_address);
    wire_.write(reg);
    wire_.write(value);
    wire_.endTransmission();
}

uint8_t TouchModule::readRegister(uint8_t reg)
{
    wire_.beginTransmission(touch_address);
    wire_.write(reg);
    wire_.endTransmission(false);

    wire_.requestFrom(touch_address, (uint8_t)1);
    if (wire_.available())
        return wire_.read();

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

        case BusproOp::TOUCH_CHANNEL_MODE.writeReq():
            handleModifyTouchMode(frame);
            break;

        case BusproOp::TOUCH_CHANNEL_MODE.readReq():
            handleReadTouchMode(frame);
            break;

        case BusproOp::TOUCH_CHANNEL_REMARK.writeReq():
            handleModifyTouchRemark(frame);
            break;

        case BusproOp::TOUCH_CHANNEL_REMARK.readReq():
            handleReadTouchRemark(frame);
            break;

        case BusproOp::TOUCH_OPRATION1.req():
            handleReadOpration1(frame);
            break;
        case BusproOp::TOUCH_OPRATION2.req():
            handleReadOpration2(frame);
            break;
        case BusproOp::TOUCH_OPRATION3.req():
            handleReadOpration3(frame);
            break;
        case BusproOp::TOUCH_OPRATION4.req():
            handleReadOpration4(frame);
            break;

        case BusproOp::TOUCH_OPRATION5.readReq():
            handleReadOpration5(frame);
            break;
        case BusproOp::TOUCH_OPRATION5.writeReq():
            handleModifyOpration5(frame);
            break;

        case BusproOp::TOUCH_OPRATION6.readReq():
            handleReadOpration6(frame);
            break;
        case BusproOp::TOUCH_OPRATION6.writeReq():
            handleModifyOpration6(frame);
            break;

        case BusproOp::TOUCH_OPRATION7.readReq():
            handleReadOpration7(frame);
            break;
        case BusproOp::TOUCH_OPRATION7.writeReq():
            handleModifyOpration7(frame);
            break;
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

//////////////////////////////// BUTTON SETTINGS ////////////////////////////////

void TouchModule::handleReadTouchMode(const BusproFrame &frame)
{
    if (frame.payloadLen != 0)
        return;

    uint8_t payload[TOUCH_CHANNEL_COUNT] = {0x00};

    sendResponse(BusproOp::TOUCH_CHANNEL_MODE.readResp(), frame.srcAddress, payload, sizeof(payload));
}

void TouchModule::handleModifyTouchMode(const BusproFrame &frame)
{
    if (frame.payloadLen != 1)
        return;

    uint8_t payload[1] = {0xF8};

    sendResponse(BusproOp::TOUCH_CHANNEL_MODE.writeResp(), frame.srcAddress, payload, sizeof(payload));
}

void TouchModule::handleReadTouchRemark(const BusproFrame &frame)
{
    if (frame.payloadLen != 1)
        return;

    uint8_t touchNum = frame.payload[0];

    uint8_t payload[21] = {touchNum};

    sendResponse(BusproOp::TOUCH_CHANNEL_REMARK.readResp(), frame.srcAddress, payload, sizeof(payload));
}

void TouchModule::handleModifyTouchRemark(const BusproFrame &frame)
{
    if (frame.payloadLen != 0)
        return;

    uint8_t payload[1] = {0xF8};

    sendResponse(BusproOp::TOUCH_CHANNEL_REMARK.writeResp(), frame.srcAddress, payload, sizeof(payload));
}

void TouchModule::handleReadOpration1(const BusproFrame &frame)
{
    if (frame.payloadLen != 0)
        return;

    uint8_t payload[TOUCH_CHANNEL_COUNT] = {0x01};

    sendResponse(BusproOp::TOUCH_OPRATION1.resp(), frame.srcAddress, payload, sizeof(payload));
}
void TouchModule::handleReadOpration2(const BusproFrame &frame)
{
    if (frame.payloadLen != 0)
        return;

    uint8_t payload[TOUCH_CHANNEL_COUNT] = {0x02};

    sendResponse(BusproOp::TOUCH_OPRATION2.resp(), frame.srcAddress, payload, sizeof(payload));
}
void TouchModule::handleReadOpration3(const BusproFrame &frame)
{
    if (frame.payloadLen != 0)
        return;

    uint8_t payload[TOUCH_CHANNEL_COUNT] = {0x03};

    sendResponse(BusproOp::TOUCH_OPRATION3.resp(), frame.srcAddress, payload, sizeof(payload));
}

void TouchModule::handleReadOpration4(const BusproFrame &frame)
{
    if (frame.payloadLen != 0)
        return;

    uint8_t payload[1] = {0xF8};

    sendResponse(BusproOp::TOUCH_OPRATION4.resp(), frame.srcAddress, payload, sizeof(payload));
}

/////////////////////////////// BASIC INFORMATION ///////////////////////////////

void TouchModule::handleReadOpration5(const BusproFrame &frame)
{
    if (frame.payloadLen != 0)
        return;

    uint8_t payload[2] = {0x00, 0x00}; // Backlight | Status

    sendResponse(BusproOp::TOUCH_OPRATION5.readResp(), frame.srcAddress, payload, sizeof(payload));
}
void TouchModule::handleModifyOpration5(const BusproFrame &frame)
{
    if (frame.payloadLen != 3)
        return;

    uint8_t payload[1] = {0xF8};

    sendResponse(BusproOp::TOUCH_OPRATION5.writeResp(), frame.srcAddress, payload, sizeof(payload));
}

void TouchModule::handleReadOpration6(const BusproFrame &frame)
{
    if (frame.payloadLen != 0)
        return;

    uint8_t payload[3] = {0x00, 0x00, 0x00}; //||

    sendResponse(BusproOp::TOUCH_OPRATION6.readResp(), frame.srcAddress, payload, sizeof(payload));
}
void TouchModule::handleModifyOpration6(const BusproFrame &frame)
{
    if (frame.payloadLen != 8)
        return;

    uint8_t payload[1] = {0xF8};

    sendResponse(BusproOp::TOUCH_OPRATION6.writeResp(), frame.srcAddress, payload, sizeof(payload));
}

void TouchModule::handleReadOpration7(const BusproFrame &frame)
{
    if (frame.payloadLen != 0)
        return;

    uint8_t payload[1] = {0x00};

    sendResponse(BusproOp::TOUCH_OPRATION7.readResp(), frame.srcAddress, payload, sizeof(payload));
}
void TouchModule::handleModifyOpration7(const BusproFrame &frame)
{
    if (frame.payloadLen != 4)
        return;

    uint8_t payload[1] = {0xF8};

    sendResponse(BusproOp::TOUCH_OPRATION7.writeResp(), frame.srcAddress, payload, sizeof(payload));
}

/////////////////////////////// DEVICE HANDLERS ////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

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
    if (frame.payloadLen != 1)
        return;

    uint8_t duration = frame.payload[0];

    // A FINDIT request counts as activity too — wake() also restarts the idle timer.
    if (deviceMode_ == DeviceMode::Sleep)
        wake();
    else
        markActivity();

    uint8_t payload[8] = {0x00};

    sendResponse(BusproOp::DEVICE_FINDIT.resp(), frame.srcAddress, payload, sizeof(payload));
    leds_.finditAnimation(duration);
#ifdef HAS_OLED_DISPLAY
    display_.finditAnimation(duration);
#endif
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