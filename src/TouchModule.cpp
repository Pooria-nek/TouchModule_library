#include "TouchModule.h"
#include <type_traits>

TouchModule::TouchModule(
    TwoWire &wirePort,
    BusproTransport &bus,
    MemoryCore &flash,
    const uint8_t ledPins[TOUCH_PAD_COUNT],
    const uint8_t touchPads[TOUCH_PAD_COUNT],
    bool activeHigh)
    : touch_(wirePort), bus_(bus), flash_(flash), activeHigh_(activeHigh)
#ifdef HAS_OLED_DISPLAY
      ,
      display_(flash, OLED_CS_PIN, OLED_DC_PIN, OLED_RS_PIN)
#endif
#ifdef HAS_BUZZER
      ,
      buzzer_(PIN_FB_BUZZER)
#endif
#ifdef HAS_APDS
      ,
      apds_(wirePort)
#endif
      ,
      // touch_(wirePort),
      hvac_(flash),
      fheat_(flash),
      switchPanel_(flash),
      relayNtc(PB0, 10000.0f, 1000),
      outsideNtc(PB1, 10000.0f, 1000)
{
    static_assert(std::is_reference<decltype(bus_)>::value, "bus_ must be a reference");
    touch_.mapKeys(touchPads, TOUCH_PAD_COUNT);
    // mcu::copyMcuUID(uid_);
    // for (uint8_t i = 0; i < TOUCH_PAD_COUNT; i++)
    // {
    //     touchPins_[i] = ;
    // }

    leds_.configure(ledPins, activeHigh);
    leds_.setLedLevels(25, 3, 31);
}

bool TouchModule::begin()
{
    relayNtc.begin();
    outsideNtc.begin();

    leds_.begin();
    leds_.startupAnimation(); // start the idle timer from "device ready"

#ifdef HAS_OLED_DISPLAY
    display_.begin();
#endif

#ifdef HAS_BUZZER
    buzzer_.startup();
#endif

    if (!touch_.begin())
    {
        error("touch");
    }

#ifdef HAS_APDS
    if (!apds_.begin())
    {
        error("prox");
    }

    if (!apds_.enableProximitySensor(true))
    {
        error("enb");
    }

    if (!apds_.setProximityThresholds(0, 800))
    {
        error("range");
    }

    if (!apds_.disarmProximityInterrupt())
    {
        error("int");
    }

    hvac_.load();
    fheat_.load();

//     if (apds_.begin() && apds_.enableProximitySensor(true)) // interrupts off for now
//     {
//     apds_.setProximityThresholds(500, 800); // far / near, PIHT = 800
//     apds_.disarmProximityInterrupt();       // clears any stale latched INT
// }
#endif

    // delay(3000);

    wake();

    // flash_.eraseSector(MemoryAdress::Touch::SECTOR_INFO);

    if (firstime())
    {
        buzzer_.error();
        init();
    }
    reMatch();

    switchPanel_.keyTypeFetch();

    return syncValues();
}

void TouchModule::error(const char *text)
{
    display_.clear();
    display_.drawCenteredText(text);
    display_.send();
    delay(3000);
}

// void TouchModule::error(const char *text)
// {
//     display_.clear();

//     uint8_t y = 10;
//     const uint8_t lineHeight = 10;

//     const char *start = text;

//     while (*start && y <= 60)
//     {
//         const char *end = strchr(start, '\n');
//         uint8_t len = end
//                           ? static_cast<uint8_t>(end - start)
//                           : strlen(start);

//         char line[32];
//         if (len >= sizeof(line))
//             len = sizeof(line) - 1;

//         memcpy(line, start, len);
//         line[len] = '\0';

//         display_.drawStr(0, y, line);
//         y += lineHeight;

//         if (!end)
//             break;

//         start = end + 1;
//     }

//     display_.send();
// }

void TouchModule::update()
{
    relayNtc.update();
    outsideNtc.update();

    bool t = touch_.update();

    if (deviceMode_ == DeviceMode::Sleep && t != 0)
    {
        wake();

        touch_.discardPress();
    }

    buttonUpdate();

    updateFunctionRunner();

    processCooldownSend();

    // Non-blocking — auto-sleep after sleepTimeoutMs_ with no activity
    checkAutoSleep();

    buzzer_.poll();

    // updateAPDS();

    bool p = apds_.update();

    if (deviceMode_ == DeviceMode::Sleep && p != 0)
    {
        if (apds_.isNear())
            wake();
    }

    if (deviceMode_ == DeviceMode::Sleep)
    {
        display_.updateInTemprature(relayNtc);
        display_.updateOutTemprature(outsideNtc);
    }

    hvac_.setCurrentTemp(outsideNtc.temperature());
    fheat_.setCurrentTemp(outsideNtc.temperature());

    if (display_.getPage() == 5)
    {
        hvac_.fetchImage();
        display_.updateResource(hvac_);
    }

    if (display_.getPage() == 6)
    {
        fheat_.fetchImage();
        display_.updateResource(fheat_);
    }

    display_.update();
}

void TouchModule::fetchValidPages()
{
    uint8_t value[6] = {};

    flash_.read(flash_.findAdrress(memoryaddress_, MemoryAdress::Touch::PAGE_VALID_BASE), value, 6);

    display_.setValidPages(value);
}

void TouchModule::fetchSleepValues()
{
    uint8_t value[6] = {};

    flash_.read(flash_.findAdrress(memoryaddress_, MemoryAdress::Touch::PAGE_SLEEP_TIME), value, 6);

    // 1 - sleep time 10 to 99 -> 100 = always on
    // 2 - sleep level
    // 3 - page number (0 = return off)
    // 4 - return page time
    // 5 -
    // 6 - trig button when it wake

    // display_.setSleepTime(value[0]);
    // display_.setSleeplevel(value[1]);
    display_.setReturnPage(value[2]);
}

void TouchModule::fetchProxValues()
{
    uint8_t value[8] = {};
    flash_.read(flash_.findAdrress(memoryaddress_, MemoryAdress::Touch::PROX_FLAG), value, 8);

    // #ifdef HAS_APDS
    //     if (value[0] == 0)
    //     {
    //         apds_.enableProximitySensor();
    //     }
    //     else
    //     {
    //         apds_.disableProximitySensor();
    //     }
    // #endif
}

void TouchModule::fetchTempValues()
{
    uint8_t value[1] = {};
    flash_.read(flash_.findAdrress(memoryaddress_, MemoryAdress::Touch::TEMP_CALIBRATE), value, 1);

    outsideNtc.setCalibration(value[0]);
}

void TouchModule::buttonUpdate()
{
#ifdef HAS_OLED_DISPLAY
    if (touch_.isPressed(8))
    {
        if (display_.changePage(false))
        {
            buzzer_.click();
            return;
        }
    }
    else if (touch_.isPressed(9))
    {
        if (display_.changePage(true))
        {
            buzzer_.click();
            return;
        }
    }

    if (functionRunnerState_.active)
    {
        leds_.setLedMode(8, LedMode::Blinking);
        leds_.setLedMode(9, LedMode::Blinking);
    }
    else
    {
        leds_.setLedMode(8, LedMode::Active);
        leds_.setLedMode(9, LedMode::Active);
    }

#endif

    const uint8_t page = display_.getPage();

    updatePageLeds(page);

    switch (page)
    {
    case 1:
    case 2:
    case 3:
    case 4:
    {
        uint16_t key = touch_.pressedKey();

        if (key != UINT16_MAX)
        {
            functionRunner(key);
        }
    }
    break;

    case 5:
    {
        constexpr float TEMP_STEP = 1.0f;
        bool acted = false;
        uint8_t ledIndex = 0;

        //------------------------------------- change work mode -------------------------------------
        if (touch_.isHoldEdge(0))
        {
            if (hvac_.nextMode())
            {
                acted = true;
                ledIndex = 0;
            }
        }

        //------------------------------------- power -------------------------------------
        if (touch_.isPressed(1))
        {
            hvac_.togglePower();
            acted = true;
            ledIndex = 1;
        }

        //------------------------------------- set point -------------------------------------
        if (touch_.isPressed(2))
        {
            if (hvac_.decreaseTemp(TEMP_STEP))
            {
                acted = true;
                ledIndex = 2;
            }
        }
        if (touch_.isPressed(3))
        {
            if (hvac_.increaseTemp(TEMP_STEP))
            {
                acted = true;
                ledIndex = 3;
            }
        }

        //------------------------------------- change hvac fan speed -------------------------------------
        if (touch_.isPressed(4))
        {
            if (hvac_.changeFan(true))
            {
                acted = true;
                ledIndex = 4;
            }
        }
        if (touch_.isPressed(5))
        {
            if (hvac_.changeFan(false))
            {
                acted = true;
                ledIndex = 5;
            }
        }

        //------------------------------------- change hvac unit -------------------------------------
        if (touch_.isPressed(6))
        {
            if (hvac_.changeUnit(false))
            {
                acted = true;
                ledIndex = 6;
            }
        }
        if (touch_.isPressed(7))
        {
            if (hvac_.changeUnit(true))
            {
                acted = true;
                ledIndex = 7;
            }
        }

        //------------------------------------- Oprations -------------------------------------
        if (acted)
        {
            buzzer_.click();
            leds_.setLedMode(ledIndex, LedHandler<TOUCH_PAD_COUNT>::LedMode::Blink);

            const HVACPanel::State &state = hvac_.currentState();

            const uint16_t hvacAddress = hvac_.address();

            if (state.irEnabled)
            {
                uint8_t irCode = 0;

                if (hvac_.power())
                {
                    irCode = (state.mode * 51) + ((state.fan > 0 ? state.fan - 1 : 0) * 17) + static_cast<uint8_t>(hvac_.setTemp()) - 13;
                }
                else
                {
                    irCode = 2;
                }

                uint8_t payload[2] = {irCode, 0x01};

                // Send IR code here
                // Replace this with your actual IR transmission function.
                sendResponse(BusproOp::UNIVERSAL_SWITCH.req(), hvacAddress, payload, sizeof(payload));
            }
            else
            {
                uint8_t payload[14] = {
                    0x01,                                                             // AC number
                    0x00,                                                             // Temp uint
                    static_cast<uint8_t>(outsideNtc.temperature()),                   // curent temperature
                    hvac_.getSetTempCool(),                                           // Cool set temperature value
                    hvac_.getSetTempHeat(),                                           // Heat set temperature value
                    hvac_.getSetTempAuto(),                                           // Auto set temperature value
                    0x00,                                                             // Reserved
                    static_cast<uint8_t>((hvac_.mode() << 4) | (hvac_.fan() & 0x0F)), // AC Mode & Fan Speed                                     // AC Mode & Fan Speed
                    hvac_.power(),                                                    // HVAC Power
                    hvac_.mode(),
                    hvac_.fan(),
                    0x00, // Reserved
                    0x00, // Reserved
                    0x00  // Reserved
                };

                // SwitchPanel::FunctionPacket hpacket{};
                // hpacket.size = sizeof(payload);
                // hpacket.dstAddress = hvacAddress;
                // hpacket.oprCode = BusproOp::Touch::HVAC_CONTROL.req();
                // hpacket.data[0] = payload[0];
                // hpacket.data[1] = payload[1];
                // hpacket.data[2] = payload[2];
                // hpacket.data[3] = payload[3];
                // hpacket.data[4] = payload[4];
                // hpacket.data[5] = payload[5];
                // hpacket.data[6] = payload[6];
                // hpacket.data[7] = payload[7];
                // hpacket.data[8] = payload[8];
                // hpacket.data[9] = payload[9];
                // hpacket.data[10] = payload[10];
                // hpacket.data[11] = payload[11];
                // hpacket.data[12] = payload[12];
                // hpacket.data[13] = payload[13];

                // sendConfirm(hpacket);

                sendResponse(BusproOp::Touch::HVAC_CONTROL.req(), hvacAddress, payload, sizeof(payload));
            }
        }
    }
    break;

    case 6:
    {
        constexpr float TEMP_STEP = 1.0f;
        bool acted = false;
        uint8_t ledIndex = 0;

        //------------------------------------- power -------------------------------------
        if (touch_.isPressed(0))
        {
            fheat_.togglePower();
            acted = true;
            ledIndex = 0;
        }
        if (touch_.isPressed(1))
        {
            fheat_.togglePower();
            acted = true;
            ledIndex = 1;
        }

        //------------------------------------- set point -------------------------------------
        if (touch_.isPressed(2))
        {
            if (fheat_.decreaseTemp(TEMP_STEP))
            {
                acted = true;
                ledIndex = 2;
            }
        }
        if (touch_.isPressed(3))
        {
            if (fheat_.increaseTemp(TEMP_STEP))
            {
                acted = true;
                ledIndex = 3;
            }
        }

        //------------------------------------- change work mode -------------------------------------
        if (touch_.isPressed(4))
        {
            if (fheat_.previousMode())
            {
                acted = true;
                ledIndex = 4;
            }
        }
        if (touch_.isPressed(5))
        {
            if (fheat_.nextMode())
            {
                acted = true;
                ledIndex = 5;
            }
        }

        //------------------------------------- change floor heat unit -------------------------------------
        if (touch_.isPressed(6))
        {
            if (fheat_.changeUnit(false))
            {
                acted = true;
                ledIndex = 6;
            }
        }
        if (touch_.isPressed(7))
        {
            if (fheat_.changeUnit(true))
            {
                acted = true;
                ledIndex = 7;
            }
        }

        //------------------------------------- Oprations -------------------------------------
        if (acted)
        {
            buzzer_.click();
            leds_.setLedMode(ledIndex, LedHandler<TOUCH_PAD_COUNT>::LedMode::Blink);

            const FloorHeatPanel::State &state = fheat_.currentState();

            const uint16_t fHeatAddress = (static_cast<uint16_t>(state.deviceSn) << 8) | state.deviceId;

            if (ledIndex == 0 | ledIndex == 1)
            {
                uint8_t payload[3] = {0x14, state.power, state.channel};

                sendResponse(BusproOp::Touch::PANEL_CONTROL.req(), fHeatAddress, payload, sizeof(payload));
            }

            if (ledIndex == 2 | ledIndex == 3)
            {
                uint8_t payload[3] = {0x17 + state.mode, state.setTemps[state.mode], state.channel};

                sendResponse(BusproOp::Touch::PANEL_CONTROL.req(), fHeatAddress, payload, sizeof(payload));
            }

            if (ledIndex == 4 | ledIndex == 5)
            {
                uint8_t payload[3] = {0x15, state.mode + 1, state.channel};

                sendResponse(BusproOp::Touch::PANEL_CONTROL.req(), fHeatAddress, payload, sizeof(payload));
            }
        }
    }
    break;
    }
}

void TouchModule::updatePageLeds(uint8_t page)
{
    if (page < 1 || page > 4 || deviceMode_ == DeviceMode::Sleep)
        return;

    const uint8_t start = (page - 1) * 8;

    for (uint8_t key = 0; key < 8; ++key)
    {
        const uint8_t ledAddress = start + key;

        switch (ledState[ledAddress])
        {
        case 0:
            leds_.setLedMode(key, LedMode::Deactive);
            break;

        case 1:
            leds_.setLedMode(key, LedMode::Active);
            break;

        case 2:
            leds_.setLedMode(key, LedMode::Blinking);
            break;

        default:
            break;
        }
    }
}

void TouchModule::functionRunner(uint16_t key)
{
    if (functionRunnerState_.active)
        return;

    uint8_t separate = getSplitButton(key);
    uint8_t merged = getMergedButton(key);

    uint8_t keyType = switchPanel_.getKeyTypeUINT(merged);

    using SwitchType = SwitchPanel::SwitchType;

    const auto isType = [keyType](SwitchType type)
    {
        return keyType == static_cast<uint8_t>(type);
    };

    bool isMerge = true;
    bool state = false;
    uint8_t functionEnd = 1;

    switch (static_cast<SwitchType>(keyType))
    {
    case SwitchType::SingleON:
    case SwitchType::CombinationON:
        state = true;
        functionEnd = isType(SwitchType::CombinationON) ? 50 : 1;
        break;

    case SwitchType::SingleOFF:
    case SwitchType::CombinationOFF:
        state = false;
        functionEnd = isType(SwitchType::CombinationOFF) ? 50 : 1;
        break;

    case SwitchType::SingleONOFF:
    case SwitchType::CombinationONOFF:
        state = !ledState[merged * 2];
        functionEnd = isType(SwitchType::CombinationONOFF) ? 50 : 1;
        break;

    case SwitchType::DblclickSingle:
        state = !ledState[separate];
        functionEnd = 1;
        isMerge = false;
        break;

    case SwitchType::DblclickCombined:
    case SwitchType::ShortLongPress:
    case SwitchType::ShortLongJog:
        state = true;
        functionEnd = 50;
        isMerge = false;
        break;

    case SwitchType::Invalid:
    default:
        // ledState[merged * 2] = 0;
        // ledState[merged * 2 + 1] = 0;
        return;
    }

    // if (isMerge)
    // {
    //     ledState[merged * 2] = !ledState[merged * 2];
    //     ledState[merged * 2 + 1] = !ledState[merged * 2 + 1];
    // }
    // else
    // {
    //     ledState[separate] = !ledState[separate];
    // }

    buzzer_.click();

    functionRunnerState_.isMerge = isMerge;
    functionRunnerState_.keyNumber = isMerge ? merged : separate;
    functionRunnerState_.state = state;
    functionRunnerState_.keyType = keyType;
    functionRunnerState_.functionEnd = functionEnd;

    functionRunnerState_.active = true;
    functionRunnerState_.functionExecuting = false;
    functionRunnerState_.functionIndex = 1;

    // uint8_t payload[5] = {
    //     static_cast<uint8_t>(functionRunnerState_.isMerge),
    //     static_cast<uint8_t>(functionRunnerState_.keyNumber),
    //     static_cast<uint8_t>(functionRunnerState_.state),
    //     functionRunnerState_.keyType,
    //     functionRunnerState_.functionEnd};

    // sendResponse(0xDDDD, 0xDDDD, payload, sizeof(payload));
}

void TouchModule::updateFunctionRunner()
{
    if (!functionRunnerState_.active)
        return;

    // Waiting for the previous function's response: don't send the next one yet
    if (functionRunnerState_.waitingConfirm)
    {
        if (millis() - functionRunnerState_.confirmStart <= FunctionRunnerState::TRY_WINDOW_MS)
            return; // still inside this try's window

        if (functionRunnerState_.tries < FunctionRunnerState::MAX_TRIES)
        {
            // no response: send the same packet again
            functionRunnerState_.tries++;
            functionRunnerState_.confirmStart = millis();
            sendResponse(functionRunnerState_.pendingOp,
                         functionRunnerState_.pendingDst,
                         functionRunnerState_.pendingPacket.data,
                         functionRunnerState_.pendingPacket.size);
            return;
        }

        // 3 tries used, give up on this function and move on
        functionRunnerState_.waitingConfirm = false;
    }

    // 2. Process functions
    uint8_t processed = 0;

    while (functionRunnerState_.functionIndex <= functionRunnerState_.functionEnd &&
           processed < functionRunnerState_.functionsPerUpdate)
    {
        const uint8_t functionIndex = functionRunnerState_.functionIndex++;

        uint8_t function[7];

        // Resolve storage key and half
        const uint8_t storageKey = functionRunnerState_.isMerge
                                       ? static_cast<uint8_t>(functionRunnerState_.keyNumber + 1)
                                       : static_cast<uint8_t>(functionRunnerState_.keyNumber / 2 + 1);

        const uint8_t half = functionRunnerState_.isMerge
                                 ? 0
                                 : static_cast<uint8_t>(functionRunnerState_.keyNumber % 2);

        switchPanel_.getKeyFunction(function, storageKey, half ? functionIndex + 49 : functionIndex);

        processed++;

        // Invalid / unused function
        if (function[0] == 0xFF || function[0] == static_cast<uint8_t>(SwitchPanel::OperationType::Invalid))
        {
            break;
        }

        SwitchPanel::FunctionPacket spacket{};
        spacket.size = switchPanel_.makeFunction(spacket, functionRunnerState_.keyType, functionRunnerState_.state, function);

        // sendResponse(0xCCCC, 0xCCCC, function, sizeof(function));
        // sendResponse(0xCCCC, 0xCCCC, spacket.data, spacket.size);

        if (spacket.size == UINT16_MAX)
            continue;

        // Execute: first try
        functionRunnerState_.pendingOp = spacket.oprCode;
        functionRunnerState_.pendingDst = spacket.dstAddress;
        functionRunnerState_.pendingPacket = spacket;
        functionRunnerState_.tries = 1;

        sendConfirm(spacket);

        return; // the finish block runs on a later call, after confirm or timeout
    }

    // Finished
    if (functionRunnerState_.functionIndex >= functionRunnerState_.functionEnd)
    {
        functionRunnerState_.active = false;
        functionRunnerState_.functionExecuting = false;
        functionRunnerState_.waitingConfirm = false;
    }
}

bool TouchModule::firstime()
{
    uint8_t payload[16];
    flash_.read(flash_.findAdrress(memoryaddress_, MemoryAdress::DEVICE_ADDRESS), payload, sizeof(payload));

    for (uint8_t i = 0; i < sizeof(payload); i++)
    {
        if (payload[i] != 0xFF)
            return false;
    }

    return true;

    // // uint8_t fuid_[12];
    // // flash_.read(flash_.findAdrress(memoryaddress_, MemoryAdress::DEVICE_MAC_ADDRESS), fuid_, 12);

    // uint16_t fdevType;
    // // flash_.readObject(flash_.findAdrress(memoryaddress_, MemoryAdress::DEVICE_TYPE), fdevType);

    // // if ((!mcu::bufferEquals(fuid_, uid_, 12)) || (devType_ != fdevType))
    // if (devType_ != fdevType)
    // {
    //
    //     return true;
    // }
    // else
    // {
    //     return false;
    // }
}

bool TouchModule::reMatch()
{
    char storedVersion[22] = {};

    auto address = flash_.findAdrress(memoryaddress_, MemoryAdress::DEVICE_SOFTWARE_VER);

    flash_.read(address, storedVersion, sizeof(storedVersion));

    if (strncmp(storedVersion, SOFTWARE_VERSION, sizeof(storedVersion)) == 0)
        return false;

    char newVersion[22] = {};
    strncpy(newVersion, SOFTWARE_VERSION, sizeof(newVersion) - 1);

    flash_.update(address, SOFTWARE_VERSION, sizeof(storedVersion));

    return true;
}

bool TouchModule::init()
{
    deviceAddress_ = defultAddress;
    uint8_t payload[2] = {static_cast<uint8_t>(deviceAddress_ >> 8), static_cast<uint8_t>(deviceAddress_ & 0xFF)};
    flash_.write(flash_.findAdrress(memoryaddress_, MemoryAdress::DEVICE_ADDRESS), payload, sizeof(payload));

    uint8_t payload1[2] = {static_cast<uint8_t>(devType_ >> 8), static_cast<uint8_t>(devType_ & 0xFF)};
    flash_.write(flash_.findAdrress(memoryaddress_, MemoryAdress::DEVICE_TYPE), payload1, sizeof(payload1));

    // MCU UID
    const uint8_t *uid = reinterpret_cast<const uint8_t *>(0x1FFFF7E8U);
    flash_.write(flash_.findAdrress(memoryaddress_, MemoryAdress::DEVICE_MAC_ADDRESS), uid, sizeof(uid));

    // uint8_t fuid_[12];
    // flash_.write(flash_.findAdrress(memoryaddress_, MemoryAdress::DEVICE_MAC_ADDRESS), fuid_, sizeof(fuid_));

    flash_.write(flash_.findAdrress(memoryaddress_, MemoryAdress::DEVICE_REMARK), FACTORY_NAME, 20);
    flash_.write(flash_.findAdrress(memoryaddress_, MemoryAdress::DEVICE_HARDWARE_VER), HARDWARE_VERSION, 30);
    flash_.write(flash_.findAdrress(memoryaddress_, MemoryAdress::DEVICE_SOFTWARE_VER), SOFTWARE_VERSION, 22);

    uint8_t value[8] = {0, 0, 0, 0, 0, 0, 0, 0};
    flash_.write(flash_.findAdrress(memoryaddress_, MemoryAdress::Touch::PROX_FLAG), value, sizeof(value));

    uint8_t validpage[6] = {1, 0, 0, 0, 0, 0};
    flash_.write(flash_.findAdrress(memoryaddress_, MemoryAdress::Touch::PAGE_VALID_BASE), validpage, sizeof(validpage));

    char remark[20];
    for (size_t i = 1; i <= LOGICAL_BUTTON_COUNT; i++)
    {
        snprintf(remark, sizeof(remark), "Button %d", static_cast<unsigned>(i));
        flash_.write(flash_.findAdrress(memoryaddress_, MemoryAdress::setRemark(MemoryAdress::CHANNEL_REMARK, i)), remark, sizeof(remark));

        uint8_t mode = static_cast<uint8_t>(SwitchPanel::SwitchType::Invalid);

        flash_.write(flash_.findAdrress(memoryaddress_, MemoryAdress::channelAddress(MemoryAdress::Touch::CHANNEL_MODE, i)), &mode, sizeof(mode));
        flash_.write(flash_.findAdrress(memoryaddress_, MemoryAdress::channelAddress(MemoryAdress::Touch::CHANNEL_STATUE, i)), 0, 1);
        flash_.write(flash_.findAdrress(memoryaddress_, MemoryAdress::channelAddress(MemoryAdress::Touch::CHANNEL_DIMMING, i)), 0, 1);
        flash_.write(flash_.findAdrress(memoryaddress_, MemoryAdress::channelAddress(MemoryAdress::Touch::CHANNEL_DIMMING_VALUE, i)), 0, 1);
    }

    return true;
}

bool TouchModule::syncValues()
{
    uint8_t payload[2] = {};
    flash_.read(flash_.findAdrress(memoryaddress_, MemoryAdress::DEVICE_ADDRESS), payload, 2);
    deviceAddress_ = (static_cast<uint16_t>(payload[0]) << 8) | payload[1];

    flash_.read(flash_.findAdrress(memoryaddress_, MemoryAdress::DEVICE_MAC_ADDRESS), uid_, sizeof(uid_));

    // flash_.read(flash_.findAdrress(memoryaddress_, MemoryAdress::Touch::CHANNEL_MODE), payload, LOGICAL_BUTTON_COUNT);
    // flash_.read(flash_.findAdrress(memoryaddress_, MemoryAdress::Touch::CHANNEL_STATUE), payload, LOGICAL_BUTTON_COUNT);
    // flash_.read(flash_.findAdrress(memoryaddress_, MemoryAdress::Touch::CHANNEL_DIMMING), payload, LOGICAL_BUTTON_COUNT);
    // flash_.read(flash_.findAdrress(memoryaddress_, MemoryAdress::Touch::CHANNEL_DIMMING_VALUE), payload, LOGICAL_BUTTON_COUNT);

    fetchValidPages();
    fetchSleepValues();
    fetchProxValues();
    fetchTempValues();

    return true;
}

void TouchModule::updateAPDS()
{
#ifdef HAS_APDS
    if (deviceMode_ == DeviceMode::Sleep)
    {
        if (!apds_.irqPending())
            return;     // nothing happened, no I2C
        apds_.update(); // reads data and clears INT
        if (apds_.isNear())
            wake();
        return;
    }

    if (apds_.update() && apds_.isNear())
        markActivity();
#endif
}

#ifdef HAS_APDS
void TouchModule::irqProxHandler()
{
    apds_.irq();
    markActivity();
}
bool TouchModule::getProxIrq()
{
    return apds_.irqPending();
}
#endif

void TouchModule::irqTouchHandler()
{
    touch_.irq();
    markActivity();
}
bool TouchModule::getTouchIrq()
{
    return touch_.irqPending();
}

uint8_t TouchModule::getSplitButton(uint8_t key)
{
    if (key >= 8)
        return 0;

    return key + ((display_.getPage() - 1) * 8);
}

uint8_t TouchModule::getMergedButton(uint8_t key)
{
    if (key >= 8)
        return 0;

    return (key / 2) + ((display_.getPage() - 1) * 4);
}

void TouchModule::checkAutoSleep()
{
    if (deviceMode_ == DeviceMode::Wake && (millis() - lastActivityTime_ >= sleepTimeoutMs_))
    {
        sleep();
    }
}

void TouchModule::sleep()
{
    deviceMode_ = DeviceMode::Sleep;
    leds_.sleep();
    display_.sleep();
    // #ifdef HAS_APDS
    //     apds_.armProximityInterrupt();
    // #endif
}

void TouchModule::wake()
{
    deviceMode_ = DeviceMode::Wake;
    leds_.wake();
    display_.wake();

    markActivity(); // waking counts as activity — restart the idle timer
    // #ifdef HAS_APDS
    //     apds_.disarmProximityInterrupt();
    // #endif
}

// LED indicator logic (mode state machine + software PWM) now lives entirely
// in LedHandler.h — TouchModule just owns a LedHandler<TOUCH_PAD_COUNT>
// instance (leds_) and forwards its public API (see TouchModule.h).

void TouchModule::process(const BusproFrame &frame)
{
    if (functionRunnerState_.waitingConfirm &&
        frame.srcAddress == functionRunnerState_.expSrc &&
        frame.opCode == functionRunnerState_.expOp)
    {
        functionRunnerState_.waitingConfirm = false;
    }

    // if (frame.devType != devType_)
    //     return;

    if (frame.srcAddress == deviceAddress_)
        return;

    if (frame.dstAddress == 0xFFFF) // universal comands
    {
        switch (frame.opCode)
        {
            // case BusproOp::DEVICE_SEARCH_HDL.req():
            //     handleSearchDevice(frame);
            //     break;

        case BusproOp::DEVICE_REMARK.readReq():
            handleReadDeviceRemark(frame);
            break;

            //////////////////////////////////// RESPONDS //////////////////////////////////////

        case BusproOp::CONTROL_SINGLE.resp():
            handleSingleChannel(frame);
            break;

        case BusproOp::CHANNEL_STATE:
            handleStateChannel(frame);
            break;

        case BusproOp::SCENE_CONTROL.resp():
            handleSceneControl(frame);
            break;

        case BusproOp::Touch::HVAC_CONTROL.resp():
            handleUpdateHvac(frame);
            break;

        case BusproOp::Touch::PANEL_CONTROL.resp():
            handlePanelControl(frame);
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

            // case BusproOp::DEVICE_SEARCH_HDL.req():
            //     handleSearchDevice(frame);
            //     break;

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

            /////////////////////////////// SETTING ///////////////////////////////
        case BusproOp::RESTORE.req():
            handleRestore(frame);
            break;

        case BusproOp::RESTORE_E112.req():
            handleRestoreUNK(frame);
            break;

            /////////////////////////////// SETTING ///////////////////////////////

        case BusproOp::Touch::INDENSITY.readReq():
            handleReadIndensity(frame);
            break;
        case BusproOp::Touch::INDENSITY.writeReq():
            handleModifyIndensity(frame);
            break;

        case BusproOp::Touch::UI_VALUES.readReq():
            handleReadUivalues(frame);
            break;
        case BusproOp::Touch::UI_VALUES.writeReq():
            handleModifyUivalues(frame);
            break;

        case BusproOp::Touch::PAGE_ENABLE.readReq():
            handleReadEnablePage(frame);
            break;
        case BusproOp::Touch::PAGE_ENABLE.writeReq():
            handleModifyEnablePage(frame);
            break;

        case BusproOp::Touch::TEMP_CALIBRATE.readReq():
            handleReadPtempcalibr(frame);
            break;
        case BusproOp::Touch::TEMP_CALIBRATE.writeReq():
            handleModifyPtempcalibr(frame);
            break;

        case BusproOp::Touch::TEMP_FLAG.readReq():
            handleReadPtempFlag(frame);
            break;
        case BusproOp::Touch::TEMP_FLAG.writeReq():
            handleModifyPtempFlag(frame);
            break;

        case BusproOp::Touch::SLEEPING.readReq():
            handleReadReturnPage(frame);
            break;
        case BusproOp::Touch::SLEEPING.writeReq():
            handleModifyReturnPage(frame);
            break;

        case BusproOp::Touch::TYPE_TIMEDATE.readReq():
            handleReadTimeDate(frame);
            break;
        case BusproOp::Touch::TYPE_TIMEDATE.writeReq():
            handleModifyTimeDate(frame);
            break;

            /////////////////////////////// 1 TO 4 PAGE ///////////////////////////////

        case BusproOp::Touch::CHANNEL_FUNCTION.writeReq():
            handleModifyTouchFunction(frame);
            break;
        case BusproOp::Touch::CHANNEL_FUNCTION.readReq():
            handleReadTouchFunction(frame);
            break;

        case BusproOp::Touch::CHANNEL_REMARK.writeReq():
            handleModifyTouchRemark(frame);
            break;
        case BusproOp::Touch::CHANNEL_REMARK.readReq():
            handleReadTouchRemark(frame);
            break;

        case BusproOp::Touch::CHANNEL_MODE.readReq():
            handleReadTouchMode(frame);
            break;
        case BusproOp::Touch::CHANNEL_MODE.writeReq():
            handleModifyTouchMode(frame);
            break;

        case BusproOp::Touch::CHANNEL_STATUS.readReq():
            handleReadTouchStatue(frame);
            break;
        case BusproOp::Touch::CHANNEL_STATUS.writeReq():
            handleModifyTouchStatue(frame);
            break;

        case BusproOp::Touch::CHANNEL_DIMMING.readReq():
            handleReadTouchDimming(frame);
            break;
        case BusproOp::Touch::CHANNEL_DIMMING.writeReq():
            handleModifyTouchDimming(frame);
            break;

        case BusproOp::Touch::CHANNEL_DIMMING_VALUE.readReq():
            handleReadTouchDimmingValue(frame);
            break;
        case BusproOp::Touch::CHANNEL_DIMMING_VALUE.writeReq():
            handleModifyTouchDimmingValue(frame);
            break;

            ////////////////////////////////////// AC ///////////////////////////////////////

        case BusproOp::Touch::AC_INFORMATION.readReq():
            handleReadAcInfo(frame);
            break;
        case BusproOp::Touch::AC_INFORMATION.writeReq():
            handleModifyAcInfo(frame);
            break;

        case BusproOp::Touch::AC_OPRATION_MODEL.readReq():
            handleReadAcOpration(frame);
            break;
        case BusproOp::Touch::AC_OPRATION_MODEL.writeReq():
            handleModifyAcOpration(frame);
            break;

        case BusproOp::Touch::AC_TEMPERATURE.readReq():
            handleReadACTemperature(frame);
            break;
        case BusproOp::Touch::AC_TEMPERATURE.writeReq():
            handleModifyAcTemperature(frame);
            break;

        case BusproOp::Touch::AC_TEMPSENSOR.readReq():
            handleReadHvacTempSensor(frame);
            break;
        case BusproOp::Touch::AC_TEMPSENSOR.writeReq():
            handleModifyHvacTempSensor(frame);
            break;

        case BusproOp::Touch::AC_ECOMODE.readReq():
            handleReadHvacEcomode(frame);
            break;
        case BusproOp::Touch::AC_ECOMODE.writeReq():
            handleModifyHvacEcomode(frame);
            break;

        case BusproOp::Touch::AC_IRREAD.readReq():
            handleReadHvacIRread(frame);
            break;
        case BusproOp::Touch::AC_IRREAD.writeReq():
            handleModifyHvacIRread(frame);
            break;

        case BusproOp::Touch::AC_IRCONTROL.readReq():
            handleReadHvacIRcontrol(frame);
            break;
        case BusproOp::Touch::AC_IRCONTROL.writeReq():
            handleModifyHvacIRcontrol(frame);
            break;

        case BusproOp::Touch::AC_TEST_PANEL.readReq():
            handleReadHvacTest(frame);
            break;
        case BusproOp::Touch::AC_TEST_PANEL.writeReq():
            handleModifyHvacTest(frame);
            break;

        case BusproOp::Touch::HVAC_MEMBER.readReq():
            handleReadHvacMember(frame);
            break;

        case BusproOp::Touch::HVAC_MEMBER.writeReq():
            handleModifyHvacMember(frame);
            break;

            ///////////////////////////////// FLOOR HEATING /////////////////////////////////

            // case BusproOp::Touch::FH_TEMPERATURE.readReq():
            //     break;
            // case BusproOp::Touch::FH_TEMPERATURE.writeReq():
            //     break;

        case BusproOp::Touch::FH_INFORMATION.readReq():
            handleReadFHInfo(frame);
            break;
        case BusproOp::Touch::FH_INFORMATION.writeReq():
            handleModifyFHInfo(frame);
            break;

        case BusproOp::Touch::FH_STATE.readReq():
            handleReadFHState(frame);
            break;
        case BusproOp::Touch::FH_STATE.writeReq():
            handleModifyFHState(frame);
            break;

        case BusproOp::Touch::FH_CHANNEL_CONFIG.req():
            handleModifyFHConfig(frame);
            break;

            //////////////////////////////////// MUSIC //////////////////////////////////////

        case BusproOp::Touch::MUSIC_SETTING.readReq():
            handleReadMusicSetting(frame);
            break;
        case BusproOp::Touch::MUSIC_SETTING.writeReq():
            handleModifyMusicSetting(frame);
            break;

        case BusproOp::Touch::MUSIC_OPRATION.readReq():
            handleReadMusicOpration(frame);
            break;
        case BusproOp::Touch::MUSIC_OPRATION.writeReq():
            handleModifyMusicOpration(frame);
            break;

        case BusproOp::Touch::MUSIC_CMD.readReq():
            handleReadMusicComand(frame);
            break;
        case BusproOp::Touch::MUSIC_CMD.writeReq():
            handleModifyMusicComand(frame);
            break;

            //////////////////////////////////// IMAGE //////////////////////////////////////

        case BusproOp::Touch::IMAGE_READING.req():
            handleReadImage(frame);
            break;
        case BusproOp::Touch::IMAGE_MODIFY.req():
            handleModifyImage(frame);
            break;

            //////////////////////////////////// RESPONDS //////////////////////////////////////

        case BusproOp::Touch::PANEL_CONTROL.req():
            handlePanelControl(frame);
            break;

        case BusproOp::CONTROL_SINGLE.resp():
            handleSingleChannel(frame);
            break;

            // case BusproOp::SCENE_CONTROL.resp():
            //     break;

        case BusproOp::Touch::READ_TEMP.req():
            handleReadTemp(frame);
            break;

        default:
            break;
        }

        return;
    }
}

void TouchModule::sendWithCooldown(uint16_t opcode, uint16_t dst, const uint8_t *payload, uint8_t payloadLen)
{
    if (randomSendPending_)
        return;

    if (payloadLen > sizeof(pendingPayload_))
        return;

    if (payloadLen > 0 && payload == nullptr)
        return;

    pendingOpcode_ = opcode;
    pendingDst_ = dst;
    pendingPayloadLen_ = payloadLen;

    if (payloadLen > 0)
        memcpy(pendingPayload_, payload, payloadLen);

    randomSendTime_ = millis() + random(100, 1000);
    randomSendPending_ = true;
}

void TouchModule::processCooldownSend()
{
    if (!randomSendPending_)
        return;

    if (static_cast<int32_t>(millis() - randomSendTime_) < 0)
        return;

    randomSendPending_ = false;

    sendResponse(pendingOpcode_, pendingDst_, pendingPayload_, pendingPayloadLen_);
}

void TouchModule::sendResponse(uint16_t opcode, uint16_t dst, const uint8_t *payload, uint8_t payloadLen)
{
    // delay(50);
    bus_.send(deviceAddress_, devType_, opcode, dst, payload, payloadLen);
}

void TouchModule::sendConfirm(SwitchPanel::FunctionPacket packet)
{
    // Execute: first try
    functionRunnerState_.pendingOp = packet.oprCode;
    functionRunnerState_.pendingDst = packet.dstAddress;
    functionRunnerState_.pendingPacket = packet;
    functionRunnerState_.tries = 1;

    bus_.send(deviceAddress_, devType_, packet.oprCode, packet.dstAddress, packet.data, packet.size);

    functionRunnerState_.expSrc = packet.dstAddress;
    functionRunnerState_.expOp = packet.oprCode + 1;
    functionRunnerState_.confirmStart = millis();
    functionRunnerState_.waitingConfirm = true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
//////////////////////////////// DLP Functions /////////////////////////////////

void TouchModule::handleRestore(const BusproFrame &frame)
{
    // if (frame.payloadLen != 0)
    //     return;

    // 00007 2026/09/27 14:47:49:130  17 FD FE FF FE 30 00 01 12 00 00 12 00 00 00 00 00 00 12 01 12 75 95
    // 00008 2026/09/27 14:47:49:239  17 FD FE FF FE 30 00 01 12 00 00 12 00 00 00 00 00 00 12 01 12 75 95
    // 00009 2026/09/27 14:47:49:241  0F 01 12 00 95 30 01 FD FE F8 00 00 0A C6 51
    // 00010 2026/09/27 14:47:49:529  0F 01 12 00 95 30 01 FD FE F8 FF FF 00 AB 87

    uint8_t payload[4] = {BusproOp::SUCCESS, 0, 0, 10};

    sendResponse(BusproOp::RESTORE.resp(), frame.srcAddress, payload, sizeof(payload));

    payload[0] = {BusproOp::SUCCESS};
    payload[1] = {0xFF};
    payload[2] = {0xFF};
    payload[3] = {0};

    sendResponse(BusproOp::RESTORE.resp(), frame.srcAddress, payload, sizeof(payload));
}

void TouchModule::handleRestoreUNK(const BusproFrame &frame)
{
    uint8_t payload[3] = {0, 0, 10};

    sendResponse(BusproOp::RESTORE_E112.resp(), frame.srcAddress, payload, sizeof(payload));
}

/////////////////////////////////// SETTINGS ////////////////////////////////////

void TouchModule::handleReadIndensity(const BusproFrame &frame)
{
    if (frame.payloadLen != 0)
        return;

    uint8_t payload[2] = {}; // Backlight LCD | Button LED

    // payload[0] = display_.brightness();
    // payload[1] = leds_.brightness();

    flash_.read(flash_.findAdrress(memoryaddress_, MemoryAdress::Touch::PAGE_LCD_DIMM), payload, 2);

    sendResponse(BusproOp::Touch::INDENSITY.readResp(), frame.srcAddress, payload, sizeof(payload));
}

void TouchModule::handleModifyIndensity(const BusproFrame &frame)
{
    if (frame.payloadLen != 11)
        return;

    flash_.update(flash_.findAdrress(memoryaddress_, MemoryAdress::Touch::PAGE_LCD_DIMM), frame.payload, 2);

    // 1 - LCD backlight
    // 2 - Button LED
    // 3 -
    // 4 -
    // 5 -
    // 6 -

    uint8_t payload[1] = {BusproOp::SUCCESS};

    sendResponse(BusproOp::Touch::INDENSITY.writeResp(), frame.srcAddress, payload, sizeof(payload));
}

void TouchModule::handleReadUivalues(const BusproFrame &frame)
{
    if (frame.payloadLen != 0)
        return;

    uint8_t payload[9] = {BusproOp::SUCCESS}; // if no temp 8 -> 9 with temp

    flash_.read(flash_.findAdrress(memoryaddress_, MemoryAdress::Touch::PROX_FLAG), payload + 1, 8);

    sendResponse(BusproOp::Touch::UI_VALUES.readResp(), frame.srcAddress, payload, sizeof(payload));
}
void TouchModule::handleModifyUivalues(const BusproFrame &frame)
{
    if (frame.payloadLen != 8)
        return;

    // 1 - Recieve IR (use as prox)
    // 2 - min dimming value
    // 3 - show/hide Temprature
    // 4 - long press time
    // 5 - show/hide Time date
    // 6 -
    // 7 - font size
    // 8 - Temprature Source

    flash_.update(flash_.findAdrress(memoryaddress_, MemoryAdress::Touch::PROX_FLAG), frame.payload, 8);

    fetchProxValues();

    uint8_t payload[1] = {BusproOp::SUCCESS};

    sendResponse(BusproOp::Touch::UI_VALUES.writeResp(), frame.srcAddress, payload, sizeof(payload));
}

void TouchModule::handleReadEnablePage(const BusproFrame &frame)
{
    if (frame.payloadLen != 0)
        return;

    uint8_t payload[7] = {};

    flash_.read(flash_.findAdrress(memoryaddress_, MemoryAdress::Touch::PAGE_VALID_BASE), payload, 7);

    payload[6] = payload[5];
    payload[5] = 0;

    sendResponse(BusproOp::Touch::PAGE_ENABLE.readResp(), frame.srcAddress, payload, sizeof(payload));
}
void TouchModule::handleModifyEnablePage(const BusproFrame &frame)
{
    if (frame.payloadLen != 7)
        return;

    uint8_t valid[7] = {};

    valid[0] = frame.payload[0];
    valid[1] = frame.payload[1];
    valid[2] = frame.payload[2];
    valid[3] = frame.payload[3];
    valid[4] = frame.payload[4];
    valid[5] = frame.payload[6];
    // valid[6] = frame.payload[5]; // for music

    flash_.update(flash_.findAdrress(memoryaddress_, MemoryAdress::Touch::PAGE_VALID_BASE), valid, sizeof(valid));

    fetchValidPages();

    uint8_t payload[1] = {BusproOp::SUCCESS};

    sendResponse(BusproOp::Touch::PAGE_ENABLE.writeResp(), frame.srcAddress, payload, sizeof(payload));
}

void TouchModule::handleReadPtempcalibr(const BusproFrame &frame)
{
    if (frame.payloadLen != 0)
        return;

    uint8_t payload[4] = {}; // stub — no backing state yet; zero rather than leak the stack

    sendResponse(BusproOp::Touch::TEMP_CALIBRATE.readResp(), frame.srcAddress, payload, sizeof(payload));
}
void TouchModule::handleModifyPtempcalibr(const BusproFrame &frame)
{
    if (frame.payloadLen != 4)
        return;

    // 1 - brodcast on/off
    // 2 - subnet id
    // 3 - device id
    // 4 - (maybe type) always 20

    uint8_t payload[1] = {BusproOp::SUCCESS};

    sendResponse(BusproOp::Touch::TEMP_CALIBRATE.writeResp(), frame.srcAddress, payload, sizeof(payload));
}

void TouchModule::handleReadPtempFlag(const BusproFrame &frame)
{
    if (frame.payloadLen != 0)
        return;

    uint8_t payload[1] = {0}; // stub — no backing state yet; zero rather than leak the stack

    sendResponse(BusproOp::Touch::TEMP_FLAG.readResp(), frame.srcAddress, payload, sizeof(payload));
}
void TouchModule::handleModifyPtempFlag(const BusproFrame &frame)
{
    // if (frame.payloadLen != 0)
    //     return;

    uint8_t payload[1] = {BusproOp::SUCCESS};

    sendResponse(BusproOp::Touch::TEMP_FLAG.writeResp(), frame.srcAddress, payload, sizeof(payload));
}

void TouchModule::handleReadReturnPage(const BusproFrame &frame)
{
    if (frame.payloadLen != 0)
        return;

    uint8_t payload[6] = {}; // stub — no backing state yet; zero rather than leak the stack

    flash_.read(flash_.findAdrress(memoryaddress_, MemoryAdress::Touch::PAGE_SLEEP_TIME), payload, sizeof(payload));

    sendResponse(BusproOp::Touch::SLEEPING.readResp(), frame.srcAddress, payload, sizeof(payload));
}
void TouchModule::handleModifyReturnPage(const BusproFrame &frame)
{
    if (frame.payloadLen != 8)
        return;

    // 1 - sleep time 10 to 99 -> 100 = always on
    // 2 - sleep level
    // 3 - page number (0 = return off)
    // 4 - return page time
    // 5 -
    // 6 - trig button when it wake

    flash_.update(flash_.findAdrress(memoryaddress_, MemoryAdress::Touch::PAGE_SLEEP_TIME), frame.payload, 6);

    fetchSleepValues();

    uint8_t payload[1] = {BusproOp::SUCCESS};

    sendResponse(BusproOp::Touch::SLEEPING.writeResp(), frame.srcAddress, payload, sizeof(payload));
}

void TouchModule::handleReadTimeDate(const BusproFrame &frame)
{
    if (frame.payloadLen != 0)
        return;

    uint8_t payload[9] = {}; // stub — no backing state yet; zero rather than leak the stack

    sendResponse(BusproOp::Touch::TYPE_TIMEDATE.readResp(), frame.srcAddress, payload, sizeof(payload));
}
void TouchModule::handleModifyTimeDate(const BusproFrame &frame)
{
    if (frame.payloadLen != 2)
        return;

    // time Format
    // date Format

    uint8_t payload[1] = {BusproOp::SUCCESS};

    sendResponse(BusproOp::Touch::TYPE_TIMEDATE.writeResp(), frame.srcAddress, payload, sizeof(payload));
}

///////////////////////////////// 1 TO 4 PAGE ///////////////////////////////////

//

void TouchModule::handleReadTouchMode(const BusproFrame &frame)
{
    if (frame.payloadLen != 0)
        return;

    uint8_t payload[LOGICAL_BUTTON_COUNT];

    switchPanel_.keyType(payload);

    sendResponse(BusproOp::Touch::CHANNEL_MODE.readResp(), frame.srcAddress, payload, sizeof(payload));
}
void TouchModule::handleModifyTouchMode(const BusproFrame &frame)
{
    if (frame.payloadLen != LOGICAL_BUTTON_COUNT)
        return;

    switchPanel_.keyTypePull(frame.payload);

    uint8_t payload[1] = {BusproOp::SUCCESS};

    sendResponse(BusproOp::Touch::CHANNEL_MODE.writeResp(), frame.srcAddress, payload, sizeof(payload));
}

//

void TouchModule::handleReadTouchStatue(const BusproFrame &frame)
{
    if (frame.payloadLen != 0)
        return;

    uint8_t payload[LOGICAL_BUTTON_COUNT];

    flash_.read(flash_.findAdrress(memoryaddress_, MemoryAdress::Touch::CHANNEL_STATUE), payload, LOGICAL_BUTTON_COUNT);

    sendResponse(BusproOp::Touch::CHANNEL_STATUS.readResp(), frame.srcAddress, payload, sizeof(payload));
}
void TouchModule::handleModifyTouchStatue(const BusproFrame &frame)
{
    uint8_t payload[1] = {BusproOp::SUCCESS};

    sendResponse(BusproOp::Touch::CHANNEL_STATUS.writeResp(), frame.srcAddress, payload, sizeof(payload));
}

//

void TouchModule::handleReadTouchDimming(const BusproFrame &frame)
{
    if (frame.payloadLen != 0)
        return;

    uint8_t payload[LOGICAL_BUTTON_COUNT];

    flash_.read(flash_.findAdrress(memoryaddress_, MemoryAdress::Touch::CHANNEL_DIMMING), payload, LOGICAL_BUTTON_COUNT);

    sendResponse(BusproOp::Touch::CHANNEL_DIMMING.readResp(), frame.srcAddress, payload, sizeof(payload));
}
void TouchModule::handleModifyTouchDimming(const BusproFrame &frame)
{
    uint8_t payload[1] = {BusproOp::SUCCESS};

    sendResponse(BusproOp::Touch::CHANNEL_DIMMING.writeResp(), frame.srcAddress, payload, sizeof(payload));
}

//

void TouchModule::handleReadTouchDimmingValue(const BusproFrame &frame)
{
    if (frame.payloadLen != 0)
        return;

    uint8_t payload[LOGICAL_BUTTON_COUNT];

    flash_.read(flash_.findAdrress(memoryaddress_, MemoryAdress::Touch::CHANNEL_DIMMING_VALUE), payload, LOGICAL_BUTTON_COUNT);

    sendResponse(BusproOp::Touch::CHANNEL_DIMMING_VALUE.readResp(), frame.srcAddress, payload, sizeof(payload));
}
void TouchModule::handleModifyTouchDimmingValue(const BusproFrame &frame)
{
    uint8_t payload[1] = {BusproOp::SUCCESS};

    sendResponse(BusproOp::Touch::CHANNEL_DIMMING_VALUE.writeResp(), frame.srcAddress, payload, sizeof(payload));
}

//

void TouchModule::handleReadTouchFunction(const BusproFrame &frame)
{
    if (frame.payloadLen != 2)
        return;

    uint8_t payload[9] = {};
    uint8_t functionData[7] = {};

    payload[0] = frame.payload[0]; // key
    payload[1] = frame.payload[1]; // function

    switchPanel_.getKeyFunction(functionData, frame.payload[0], frame.payload[1]);

    memcpy(payload + 2, functionData, sizeof(functionData));

    sendResponse(BusproOp::Touch::CHANNEL_FUNCTION.readResp(), frame.srcAddress, payload, sizeof(payload));
}
void TouchModule::handleModifyTouchFunction(const BusproFrame &frame)
{
    if (frame.payloadLen != 9)
        return;

    switchPanel_.setKeyFunction(frame.payload + 2, frame.payload[0], frame.payload[1]);

    uint8_t payload[9] = {};
    uint8_t functionData[7] = {};

    payload[0] = frame.payload[0]; // key
    payload[1] = frame.payload[1]; // function

    switchPanel_.getKeyFunction(functionData, frame.payload[0], frame.payload[1]);

    sendResponse(BusproOp::Touch::CHANNEL_FUNCTION.writeResp(), frame.srcAddress, payload, sizeof(payload));
}

//

void TouchModule::handleReadTouchRemark(const BusproFrame &frame)
{
    if (frame.payloadLen != 1)
        return;

    uint8_t touchNum = frame.payload[0];

    uint8_t payload[21] = {touchNum};

    flash_.read(flash_.findAdrress(memoryaddress_, MemoryAdress::setRemark(MemoryAdress::CHANNEL_REMARK, touchNum)), payload + 1, 20);

    sendResponse(BusproOp::Touch::CHANNEL_REMARK.readResp(), frame.srcAddress, payload, sizeof(payload));
}
void TouchModule::handleModifyTouchRemark(const BusproFrame &frame)
{
    if (frame.payloadLen != 21)
        return;

    uint8_t touchNum = frame.payload[0];

    uint8_t payload[21] = {touchNum};

    flash_.update(flash_.findAdrress(memoryaddress_, MemoryAdress::setRemark(MemoryAdress::CHANNEL_REMARK, touchNum)), frame.payload + 1, 20);

    sendResponse(BusproOp::Touch::CHANNEL_REMARK.writeResp(), frame.srcAddress, payload, sizeof(payload));
}

////////////////////////////////////// AC ///////////////////////////////////////

static uint8_t selectedHvac = 1; // was a plain global (external linkage) — could
// collide with another same-named global at
// link time. Tracks which AC index the last

uint8_t TouchModule::selectedHvacIndex() const
{
    return (selectedHvac >= 1 && selectedHvac <= HVACPanel::HVAC_COUNT)
               ? selectedHvac - 1
               : HVACPanel::HVAC_COUNT;
}

void TouchModule::handleReadAcInfo(const BusproFrame &frame)
{
    if (frame.payloadLen != 0)
        return;

    const uint8_t index = selectedHvacIndex();

    if (index >= HVACPanel::HVAC_COUNT)
        return;

    const auto &shvac = hvac_.get(index);

    // uint8_t subnetid = 0;
    // uint8_t deviceid = 0;

    uint8_t adjust = outsideNtc.calibration();

    uint8_t type = 1;
    uint8_t state = 1;

    const uint8_t payload[9] = {
        BusproOp::SUCCESS,
        shvac.valid,
        shvac.deviceSn,
        shvac.deviceId,
        adjust,
        selectedHvac,
        type,
        0,
        state};

    sendResponse(BusproOp::Touch::AC_INFORMATION.readResp(), frame.srcAddress, payload, sizeof(payload));
}
void TouchModule::handleModifyAcInfo(const BusproFrame &frame)
{
    if (frame.payloadLen != 8)
        return;

    const uint8_t tempCalibrate[1] = {frame.payload[3]};

    flash_.update(flash_.findAdrress(memoryaddress_, MemoryAdress::Touch::TEMP_CALIBRATE), tempCalibrate, 1);

    fetchTempValues();

    const uint8_t requestedHvac = frame.payload[4];

    if (requestedHvac == 0 || requestedHvac > HVACPanel::HVAC_COUNT)
        return;

    selectedHvac = requestedHvac;

    const uint8_t payload[] = {BusproOp::SUCCESS};

    sendResponse(BusproOp::Touch::AC_INFORMATION.writeResp(), frame.srcAddress, payload, sizeof(payload));
}

void TouchModule::handleReadAcOpration(const BusproFrame &frame)
{
    if (frame.payloadLen != 0)
        return;

    const uint8_t index = selectedHvacIndex();

    if (index >= HVACPanel::HVAC_COUNT)
        return;

    const auto &state = hvac_.get(index);

    uint8_t payload[11] = {};

    uint8_t fanCount = 0;

    for (uint8_t i = 0; i < HVACPanel::FAN_COUNT; ++i)
    {
        if (state.validFan[i])
            payload[1 + fanCount++] = i;
    }

    payload[0] = fanCount;

    uint8_t modeCount = 0;

    for (uint8_t i = 0; i < HVACPanel::MODE_COUNT; ++i)
    {
        if (state.validMode[i])
            payload[6 + modeCount++] = i;
    }

    payload[5] = modeCount;

    sendResponse(BusproOp::Touch::AC_OPRATION_MODEL.readResp(), frame.srcAddress, payload, sizeof(payload));
}
void TouchModule::handleModifyAcOpration(const BusproFrame &frame)
{
    if (frame.payloadLen != 11)
        return;

    const uint8_t index = selectedHvacIndex();

    if (index >= HVACPanel::HVAC_COUNT)
        return;

    const uint8_t fanCount = frame.payload[0];
    const uint8_t modeCount = frame.payload[5];

    if (fanCount > HVACPanel::FAN_COUNT ||
        modeCount > HVACPanel::MODE_COUNT)
        return;

    bool validFan[HVACPanel::FAN_COUNT] = {};
    bool validMode[HVACPanel::MODE_COUNT] = {};

    for (uint8_t i = 0; i < fanCount; ++i)
    {
        const uint8_t fan = frame.payload[1 + i];

        if (fan >= HVACPanel::FAN_COUNT)
            return;

        if (validFan[fan])
            return; // Reject duplicate entries.

        validFan[fan] = true;
    }

    for (uint8_t i = 0; i < modeCount; ++i)
    {
        const uint8_t mode = frame.payload[6 + i];

        if (mode >= HVACPanel::MODE_COUNT)
            return;

        if (validMode[mode])
            return; // Reject duplicate entries.

        validMode[mode] = true;
    }

    for (uint8_t i = 0; i < HVACPanel::FAN_COUNT; ++i)
        hvac_.setFanValid(index, i, validFan[i]);

    for (uint8_t i = 0; i < HVACPanel::MODE_COUNT; ++i)
        hvac_.setModeValid(index, i, validMode[i]);

    // Persist the updated HVAC configuration.
    hvac_.save(index);

    const uint8_t payload[1] = {BusproOp::SUCCESS};

    sendResponse(BusproOp::Touch::AC_OPRATION_MODEL.writeResp(), frame.srcAddress, payload, sizeof(payload));
}

void TouchModule::handleReadACTemperature(const BusproFrame &frame)
{
    if (frame.payloadLen != 0)
        return;

    const uint8_t index = selectedHvacIndex();

    if (index >= HVACPanel::HVAC_COUNT)
        return;

    const HVACPanel::State &state = hvac_.get(index);

    // if (!state.valid)
    //     return;

    const uint8_t payload[] = {
        state.rangeCool[0],
        state.rangeCool[1],

        state.rangeHeat[0],
        state.rangeHeat[1],

        state.rangeAuto[0],
        state.rangeAuto[1],

        0, // Reserved / unknown

        state.rangeDehum[0],
        state.rangeDehum[1],

        5,   // Reserved / unknown
        0x23 // Reserved / unknown
    };

    sendResponse(BusproOp::Touch::AC_TEMPERATURE.readResp(), frame.srcAddress, payload, sizeof(payload));
}
void TouchModule::handleModifyAcTemperature(const BusproFrame &frame)
{
    if (frame.payloadLen != 11)
        return;

    const uint8_t index = selectedHvacIndex();

    if (index >= HVACPanel::HVAC_COUNT)
        return;

    HVACPanel::State &state = hvac_.get(index);

    // if (!state.valid)
    //     return;

    // Build proposed ranges from the incoming payload.
    const uint8_t coolMin = frame.payload[0];
    const uint8_t coolMax = frame.payload[1];

    const uint8_t heatMin = frame.payload[2];
    const uint8_t heatMax = frame.payload[3];

    const uint8_t autoMin = frame.payload[4];
    const uint8_t autoMax = frame.payload[5];

    // frame.payload[6] is reserved / unknown.
    const uint8_t dehumMin = frame.payload[7];
    const uint8_t dehumMax = frame.payload[8];

    // Validate before modifying state.
    const auto validRange = [](uint8_t min, uint8_t max)
    {
        return min <= max;
    };

    if (!validRange(coolMin, coolMax) ||
        !validRange(heatMin, heatMax) ||
        !validRange(autoMin, autoMax) ||
        !validRange(dehumMin, dehumMax))
    {
        return;
    }

    state.rangeCool[0] = coolMin;
    state.rangeCool[1] = coolMax;

    state.rangeHeat[0] = heatMin;
    state.rangeHeat[1] = heatMax;

    state.rangeAuto[0] = autoMin;
    state.rangeAuto[1] = autoMax;

    state.rangeDehum[0] = dehumMin;
    state.rangeDehum[1] = dehumMax;

    // Persist the updated HVAC configuration.
    hvac_.save(index); // Call through the appropriate public HVACPanel API.

    const uint8_t payload[] = {
        BusproOp::SUCCESS,
        state.rangeCool[0], state.rangeCool[1],
        state.rangeHeat[0], state.rangeHeat[1],
        state.rangeAuto[0], state.rangeAuto[1],
        0,
        state.rangeDehum[0], state.rangeDehum[1],
        5,
        0x23};

    sendResponse(BusproOp::Touch::AC_TEMPERATURE.writeResp(), frame.srcAddress, payload, sizeof(payload));
}

void TouchModule::handleReadHvacTempSensor(const BusproFrame &frame)
{
    if (frame.payloadLen != 0)
        return;

    const uint8_t index = selectedHvacIndex();
    if (index >= HVACPanel::HVAC_COUNT)
        return;

    const HVACPanel::State &state = hvac_.get(index);

    uint8_t payload[33] = {};

    payload[0] = static_cast<uint8_t>(state.tempSource);

    for (uint8_t i = 0; i < HVACPanel::TEMP_SENSOR_COUNT; ++i)
    {
        const auto &sensor = state.tempSensors[i];
        const uint8_t offset = 1 + (i * 4);

        payload[offset] = sensor.state;
        payload[offset + 1] = sensor.deviceSn;
        payload[offset + 2] = sensor.deviceId;
        payload[offset + 3] = sensor.channel;
    }

    sendResponse(BusproOp::Touch::AC_TEMPSENSOR.readResp(), frame.srcAddress, payload, sizeof(payload));
}
void TouchModule::handleModifyHvacTempSensor(const BusproFrame &frame)
{
    if (frame.payloadLen != 33)
        return;
    const uint8_t index = selectedHvacIndex();
    if (index >= HVACPanel::HVAC_COUNT)
        return;

    const uint8_t source = frame.payload[0];

    if (source > static_cast<uint8_t>(HVACPanel::TempSource::Average))
        return;

    auto &state = hvac_.get(index);
    bool changed = false;

    const auto tempSource =
        static_cast<HVACPanel::TempSource>(source);

    if (state.tempSource != tempSource)
    {
        state.tempSource = tempSource;
        changed = true;
    }

    for (uint8_t i = 0; i < HVACPanel::TEMP_SENSOR_COUNT; ++i)
    {
        const uint8_t offset = 1 + (i * 4);
        auto &sensor = state.tempSensors[i];

        const uint8_t sensorState = frame.payload[offset];
        const uint8_t deviceSn = frame.payload[offset + 1];
        const uint8_t deviceId = frame.payload[offset + 2];
        const uint8_t channel = frame.payload[offset + 3];

        if (sensor.state != sensorState ||
            sensor.deviceSn != deviceSn ||
            sensor.deviceId != deviceId ||
            sensor.channel != channel)
        {
            sensor.state = sensorState;
            sensor.deviceSn = deviceSn;
            sensor.deviceId = deviceId;
            sensor.channel = channel;
            changed = true;
        }
    }

    if (changed)
        hvac_.save(index);

    const uint8_t payload[1] = {BusproOp::SUCCESS};

    sendResponse(BusproOp::Touch::AC_TEMPSENSOR.writeResp(), frame.srcAddress, payload, sizeof(payload));
}

void TouchModule::handleReadHvacSynchronous(const BusproFrame &frame)
{
    if (frame.payloadLen != 0)
        return;

    const uint8_t payload[17] = {};

    sendResponse(BusproOp::Touch::AC_SYNCHRONUS.readResp(), frame.srcAddress, payload, sizeof(payload));
}
void TouchModule::handleModifyHvacSynchronous(const BusproFrame &frame)
{
    if (frame.payloadLen != 17)
        return;

    const uint8_t payload[1] = {BusproOp::SUCCESS};

    sendResponse(BusproOp::Touch::AC_SYNCHRONUS.writeResp(), frame.srcAddress, payload, sizeof(payload));
}

void TouchModule::handleReadHvacEcomode(const BusproFrame &frame)
{
    if (frame.payloadLen != 0)
        return;

    uint8_t payload[1] = {};

    sendResponse(BusproOp::Touch::AC_ECOMODE.readResp(), frame.srcAddress, payload, sizeof(payload));
}
void TouchModule::handleModifyHvacEcomode(const BusproFrame &frame)
{
    uint8_t payload[1] = {BusproOp::SUCCESS};

    sendResponse(BusproOp::Touch::AC_ECOMODE.writeResp(), frame.srcAddress, payload, sizeof(payload));
}

void TouchModule::handleReadHvacIRread(const BusproFrame &frame)
{
    if (frame.payloadLen != 0)
        return;

    const uint8_t index = selectedHvacIndex();

    if (index >= HVACPanel::HVAC_COUNT)
        return;

    const HVACPanel::State &state = hvac_.get(index);

    if (!state.valid)
        return;

    uint8_t payload[1] = {static_cast<uint8_t>(state.irEnabled)};

    sendResponse(BusproOp::Touch::AC_IRREAD.readResp(), frame.srcAddress, payload, sizeof(payload));
}
void TouchModule::handleModifyHvacIRread(const BusproFrame &frame)
{
    if (frame.payloadLen != 1)
        return;

    const uint8_t index = selectedHvacIndex();

    if (index >= HVACPanel::HVAC_COUNT)
        return;

    const uint8_t value = frame.payload[0];

    // Accept only 0 (disabled) or 1 (enabled).
    if (value > 1)
        return;

    const bool changed = hvac_.setIrEnabled(index, value != 0);

    uint8_t payload[1] = {BusproOp::SUCCESS};

    sendResponse(BusproOp::Touch::AC_IRREAD.writeResp(), frame.srcAddress, payload, sizeof(payload));
}

void TouchModule::handleReadHvacIRcontrol(const BusproFrame &frame)
{
    if (frame.payloadLen != 0)
        return;

    uint8_t payload[1] = {};

    sendResponse(BusproOp::Touch::AC_IRCONTROL.readResp(), frame.srcAddress, payload, sizeof(payload));
}
void TouchModule::handleModifyHvacIRcontrol(const BusproFrame &frame)
{
    uint8_t payload[1] = {BusproOp::SUCCESS};

    sendResponse(BusproOp::Touch::AC_IRCONTROL.writeResp(), frame.srcAddress, payload, sizeof(payload));
}

void TouchModule::handleReadHvacTest(const BusproFrame &frame)
{
    if (frame.payloadLen != 0)
        return;

    uint8_t payload[10] = {};

    sendResponse(BusproOp::Touch::AC_TEST_PANEL.readResp(), frame.srcAddress, payload, sizeof(payload));
}
void TouchModule::handleModifyHvacTest(const BusproFrame &frame)
{
    uint8_t payload[11] = {BusproOp::SUCCESS};

    sendResponse(BusproOp::Touch::AC_TEST_PANEL.writeResp(), frame.srcAddress, payload, sizeof(payload));
}

void TouchModule::handleReadHvacMember(const BusproFrame &frame)
{
    if (frame.payloadLen != 1)
        return;

    const uint8_t hvacNumber = frame.payload[0];

    // Protocol numbers are 1-8; internal indexes are 0-7.
    if (hvacNumber < 1 || hvacNumber > HVACPanel::HVAC_COUNT)
        return;

    const uint8_t index = hvacNumber - 1;
    const HVACPanel::State &state = hvac_.get(index);

    uint8_t payload[5] = {
        BusproOp::SUCCESS,
        hvacNumber,
        static_cast<uint8_t>(state.valid ? 1 : 0),
        state.deviceSn,
        state.deviceId};

    sendResponse(BusproOp::Touch::HVAC_MEMBER.readResp(), frame.srcAddress, payload, sizeof(payload));
}
void TouchModule::handleModifyHvacMember(const BusproFrame &frame)
{
    if (frame.payloadLen != 4)
        return;

    const uint8_t hvacNumber = frame.payload[0];
    const uint8_t enabled = frame.payload[1];
    const uint8_t deviceSn = frame.payload[2];
    const uint8_t deviceId = frame.payload[3];

    if (hvacNumber < 1 || hvacNumber > HVACPanel::HVAC_COUNT)
        return;

    if (enabled > 1)
        return;

    const uint8_t index = hvacNumber - 1;

    hvac_.setHvacMember(
        index,
        enabled != 0,
        deviceSn,
        deviceId);

    uint8_t payload[1] = {BusproOp::SUCCESS};

    sendResponse(BusproOp::Touch::HVAC_MEMBER.writeResp(), frame.srcAddress, payload, sizeof(payload));
}

///////////////////////////////// FLOOR HEATING /////////////////////////////////

void TouchModule::handleReadFHInfo(const BusproFrame &frame)
{
    if (frame.payloadLen != 0)
        return;

    uint8_t payload[46] = {};

    payload[0] = 1; // floorheat mode

    for (uint8_t i = 0; i < 4; ++i)
    {
        const auto &shvac = fheat_.get(i);
        const uint8_t offset = 1 + (i * 4);

        payload[offset] = shvac.valid ? 0x01 : 0x00; // heating enable
        payload[offset + 1] = shvac.deviceSn;        // subnet
        payload[offset + 2] = shvac.deviceId;        // id
        payload[offset + 3] = shvac.channel;         // channel
    }

    payload[17] = 0; // temp sensor mode 0-inner sensor 1-read from bus 2-avrage

    payload[18] = 1; // sensor 1 state
    payload[19] = 0; // sensor 1 subnet
    payload[20] = 0; // sensor 1 id
    payload[21] = 0; // sensor 1 channel

    payload[22] = 1; // sensor 2 state
    payload[23] = 0; // sensor 2 subnet
    payload[24] = 0; // sensor 2 id
    payload[25] = 0; // sensor 2 channel

    payload[26] = 0; // protection state
    payload[27] = 0; // protection subnet
    payload[28] = 0; // protection id
    payload[29] = 0; // protection channel

    payload[30] = 0; // outdoor state
    payload[31] = 0; // outdoor temp subnet
    payload[32] = 0; // outdoor temp id
    payload[33] = 0; // outdoor temp channel

    payload[34] = 0; //

    payload[35] = 0;    // output
    payload[36] = 0;    // min PWM
    payload[37] = 2;    // heating speed
    payload[38] = 5;    // control cycle
    payload[39] = 0x0F; // mode
    payload[40] = 0;    //

    payload[41] = 9;  // day start HH
    payload[42] = 30; // day start MM
    payload[43] = 21; // night start HH
    payload[44] = 30; // night start MM
    payload[45] = 0;  //
    payload[46] = 0;  //

    sendResponse(BusproOp::Touch::FH_INFORMATION.readResp(), frame.srcAddress, payload, sizeof(payload));
}
void TouchModule::handleModifyFHInfo(const BusproFrame &frame)
{
    // if (frame.payloadLen != 8)
    //     return;

    uint8_t payload[1] = {BusproOp::SUCCESS};

    sendResponse(BusproOp::Touch::FH_INFORMATION.writeResp(), frame.srcAddress, payload, sizeof(payload));
}

void TouchModule::handleReadFHState(const BusproFrame &frame)
{
    if (frame.payloadLen != 0)
        return;

    uint8_t payload[8] = {};

    payload[1] = {static_cast<uint8_t>(outsideNtc.temperature())};
    payload[2] = 1; // power
    payload[3] = 1; // mode

    payload[4] = 21; // manual temp
    payload[5] = 22; // day temp
    payload[6] = 23; // night temp
    payload[7] = 24; //
    payload[8] = 0;  // away temp

    sendResponse(BusproOp::Touch::FH_STATE.readResp(), frame.srcAddress, payload, sizeof(payload));
}
void TouchModule::handleModifyFHState(const BusproFrame &frame)
{
    if (frame.payloadLen != 8)
        return;

    uint8_t payload[1] = {BusproOp::SUCCESS};

    sendResponse(BusproOp::Touch::FH_STATE.writeResp(), frame.srcAddress, payload, sizeof(payload));
}

void TouchModule::handleModifyFHConfig(const BusproFrame &frame)
{
    if (frame.payloadLen != 33)
        return;

    for (uint8_t i = 0; i < 4; ++i)
    {
        const uint8_t offset = i * 4;

        fheat_.setFheatMember(
            i,
            frame.payload[offset],
            frame.payload[offset + 1],
            frame.payload[offset + 2],
            frame.payload[offset + 3]);

        fheat_.save(i);
    }

    // frame.payload[5]; // heating 2 enable
    // frame.payload[6]; // heating 2 subnet
    // frame.payload[7]; // heating 2 id
    // frame.payload[8]; // heating 2 channel

    // frame.payload[9];  // heating 3 enable
    // frame.payload[10]; // heating 3 subnet
    // frame.payload[11]; // heating 3 id
    // frame.payload[12]; // heating 3 channel

    // frame.payload[13]; // heating 4 enable
    // frame.payload[14]; // heating 4 subnet
    // frame.payload[15]; // heating 4 id
    // frame.payload[16]; // heating 4 channel

    uint8_t payload[1] = {BusproOp::SUCCESS};

    sendResponse(BusproOp::Touch::FH_CHANNEL_CONFIG.resp(), frame.srcAddress, payload, sizeof(payload));
}

//////////////////////////////////// MUSIC //////////////////////////////////////

void TouchModule::handleReadMusicSetting(const BusproFrame &frame)
{
    if (frame.payloadLen != 0)
        return;

    uint8_t payload[3] = {};

    sendResponse(BusproOp::Touch::MUSIC_SETTING.readResp(), frame.srcAddress, payload, sizeof(payload));
}
void TouchModule::handleModifyMusicSetting(const BusproFrame &frame)
{
    if (frame.payloadLen != 3)
        return;

    uint8_t payload[1] = {BusproOp::SUCCESS};

    sendResponse(BusproOp::Touch::MUSIC_SETTING.writeResp(), frame.srcAddress, payload, sizeof(payload));
}

void TouchModule::handleReadMusicOpration(const BusproFrame &frame)
{
    if (frame.payloadLen != 1)
        return;

    uint8_t payload[26] = {BusproOp::SUCCESS};

    sendResponse(BusproOp::Touch::MUSIC_OPRATION.readResp(), frame.srcAddress, payload, sizeof(payload));
}
void TouchModule::handleModifyMusicOpration(const BusproFrame &frame)
{
    if (frame.payloadLen != 25)
        return;

    uint8_t payload[2] = {BusproOp::SUCCESS};

    sendResponse(BusproOp::Touch::MUSIC_OPRATION.writeResp(), frame.srcAddress, payload, sizeof(payload));
}

void TouchModule::handleReadMusicComand(const BusproFrame &frame)
{
    if (frame.payloadLen != 1)
        return;

    uint8_t payload[9] = {BusproOp::SUCCESS, frame.payload[0]};

    sendResponse(BusproOp::Touch::MUSIC_CMD.readResp(), frame.srcAddress, payload, sizeof(payload));
}
void TouchModule::handleModifyMusicComand(const BusproFrame &frame)
{
    if (frame.payloadLen != 8)
        return;

    uint8_t payload[2] = {BusproOp::SUCCESS};

    sendResponse(BusproOp::Touch::MUSIC_CMD.writeResp(), frame.srcAddress, payload, sizeof(payload));
}

//////////////////////////////////// IMAGE //////////////////////////////////////

//// old way of read ////
void TouchModule::handleReadImage(const BusproFrame &frame)
{
    if (frame.payloadLen != 2)
        return;

    uint8_t imageNumber = frame.payload[0];
    uint8_t packetNumber = frame.payload[1];

    // 0x0F, 0x1F, 0x2F, 0x3F are unused packet numbers
    bool unusedPacket = ((packetNumber & 0x0F) == 0x0F);

    // Compact packet number:
    // 0x00..0x0E -> 0..14
    // 0x10..0x1E -> 15..29
    // 0x20..0x2E -> 30..44
    // 0x30..0x3E -> 45..59
    uint8_t compactPacket = packetNumber - (packetNumber / 16);

    uint8_t payload[22] = {
        imageNumber,
        packetNumber};

    uint8_t flashData[16];

    // Unused packets don't contain image data.
    // Return 0xFF in their data area.
    if (unusedPacket)
    {
        memset(payload + 2, 0xFF, 20);
    }
    else
    {
        if (configMode == 0)
        {
            uint32_t address = flash_.findAdrress(MemoryAdress::Touch::SECTOR_IMAGES, MemoryAdress::Touch::pageImage(imageNumber, compactPacket));

            flash_.read(address, flashData, sizeof(flashData));
        }
        else if (configMode == 1)
        {
            uint32_t address = flash_.findAdrress(MemoryAdress::Touch::SECTOR_ICONS, MemoryAdress::Touch::pageImage(imageNumber, compactPacket));

            flash_.read(address, flashData, sizeof(flashData));
            ;
        }

        // First 8 bytes
        memcpy(payload + 2, flashData, 8);

        // Padding
        payload[10] = 0xFF;
        payload[11] = 0xFF;

        // Second 8 bytes
        memcpy(payload + 12, flashData + 8, 8);

        // Padding
        payload[20] = 0xFF;
        payload[21] = 0xFF;
    }

    sendResponse(
        BusproOp::Touch::IMAGE_READING.resp(),
        frame.srcAddress,
        payload,
        sizeof(payload));
}

void TouchModule::handleModifyImage(const BusproFrame &frame)
{
    if (frame.payloadLen != 22)
        return;

    uint8_t imageNumber = frame.payload[0];
    uint8_t packetNumber = frame.payload[1];

    // 0x0F, 0x1F, 0x2F, 0x3F are unused packet numbers
    bool unusedPacket = ((packetNumber & 0x0F) == 0x0F);

    // Compact packet number:
    // 0x00..0x0E -> 0..14
    // 0x10..0x1E -> 15..29
    // 0x20..0x2E -> 30..44
    // 0x30..0x3E -> 45..59
    uint8_t compactPacket = packetNumber - (packetNumber / 16);

    uint8_t flashData[16];

    // First 8 useful bytes
    memcpy(flashData, frame.payload + 2, 8);

    // Second 8 useful bytes
    memcpy(flashData + 8, frame.payload + 12, 8);

    // Only write useful packets
    if (!unusedPacket)
    {
        if (configMode == 0)
        {
            flash_.update(flash_.findAdrress(MemoryAdress::Touch::SECTOR_IMAGES, MemoryAdress::Touch::pageImage(imageNumber, compactPacket)), flashData, sizeof(flashData));
        }
        else if (configMode == 1)
        {
            flash_.update(flash_.findAdrress(MemoryAdress::Touch::SECTOR_ICONS, MemoryAdress::Touch::pageImage(imageNumber, compactPacket)), flashData, sizeof(flashData));
        }
    }

    uint8_t payload[3] = {BusproOp::SUCCESS, imageNumber, packetNumber};

    sendResponse(BusproOp::Touch::IMAGE_MODIFY.resp(), frame.srcAddress, payload, sizeof(payload));
}

void TouchModule::handlePanelControl(const BusproFrame &frame)
{
    if (frame.payloadLen != 3)
        return;

    if (frame.dstAddress == 0xFFFF)
    {
        uint8_t payload[3] = {frame.payload[0], frame.payload[1], frame.payload[2]};

        sendResponse(BusproOp::Touch::PANEL_CONTROL.resp(), frame.srcAddress, payload, sizeof(payload));
    }
    else
    {
        uint8_t oprator = frame.payload[0];
        uint8_t value = frame.payload[1];
        uint8_t channel = frame.payload[2];

        for (uint8_t i = 0; i < FloorHeatPanel::FHEAT_COUNT; ++i)
        {
            const FloorHeatPanel::State &state = fheat_.get(i);

            if (!state.valid)
                continue;

            if (state.channel != channel)
                continue;

            const uint16_t address = (static_cast<uint16_t>(state.deviceSn) << 8) | state.deviceId;

            if (frame.srcAddress != address)
                continue;

            switch (oprator)
            {
            case 0x14:
                fheat_.setPower(i, value);
                break;

            case 0x15:
                fheat_.setMode(i, value - 1);
                break;

            case 0x17:
                fheat_.setSetTempNormal(i, value);
                break;

            case 0x18:
                fheat_.setSetTempDay(i, value);
                break;

            case 0x19:
                fheat_.setSetTempNight(i, value);
                break;

            case 0x1A:
                fheat_.setSetTempAway(i, value);
                break;

            default:
                break;
            }
        }

        // uint8_t payload[3] = {frame.payload[0], frame.payload[1], frame.payload[2]};
        // sendResponse(BusproOp::Touch::PANEL_CONTROL.resp(), frame.srcAddress, payload, sizeof(payload));
    }
}

void TouchModule::handleReadTemp(const BusproFrame &frame)
{
    if (frame.payloadLen != 1)
        return;

    uint8_t payload[2] = {frame.payload[0], static_cast<uint8_t>(outsideNtc.temperature())};

    sendResponse(BusproOp::Touch::READ_TEMP.resp(), frame.srcAddress, payload, sizeof(payload));
}

void TouchModule::handleUpdateHvac(const BusproFrame &frame)
{
    if (frame.payloadLen != 14)
        return;

    for (uint8_t i = 0; i < HVACPanel::HVAC_COUNT; ++i)
    {
        const HVACPanel::State &state = hvac_.get(i);

        if (!state.valid)
            continue;

        const uint16_t address = (static_cast<uint16_t>(state.deviceSn) << 8) | state.deviceId;

        if (frame.srcAddress != address)
            continue;

        hvac_.setSetTempCool(i, frame.payload[3]);
        hvac_.setSetTempHeat(i, frame.payload[4]);
        hvac_.setSetTempAuto(i, frame.payload[5]);

        hvac_.setAutoFan(i, frame.payload[7]);
        hvac_.setPower(i, frame.payload[8] != 0);
        hvac_.setMode(i, frame.payload[9]);
        hvac_.setFan(i, frame.payload[10]);

        return;
    }
}

//////////////////////////////// DLP Functions /////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////////
/////////////////////////////// UNIVERSAL REQUEST ///////////////////////////////

void TouchModule::handleReadFirmware(const BusproFrame &frame)
{
    if (frame.payloadLen != 0)
        return;

    uint8_t payload[22] = {0x00};

    flash_.read(flash_.findAdrress(memoryaddress_, MemoryAdress::DEVICE_SOFTWARE_VER), payload, sizeof(payload));

    sendResponse(BusproOp::DEVICE_FIRMWARE.resp(), frame.srcAddress, payload, sizeof(payload));
}

void TouchModule::handleReadHardware(const BusproFrame &frame)
{
    if (frame.payloadLen != 0)
        return;

    uint8_t payload[32] = {};

    flash_.read(flash_.findAdrress(memoryaddress_, MemoryAdress::DEVICE_HARDWARE_VER), payload, sizeof(payload));

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
#ifdef HAS_BUZZER
    buzzer_.findIt();
#endif
    // #ifdef HAS_OLED_DISPLAY
    //     display_.finditAnimation(duration);
    // #endif
}

// void TouchModule::handleSearchDevice(const BusproFrame &frame)
// {
//     if (frame.payloadLen != 4 || frame.payloadLen != 2)
//         return;

//     uint8_t payload[30 + frame.payloadLen] = {0x00};

//     payload[0] = frame.payload[0];
//     payload[1] = frame.payload[1];

//     if (frame.payloadLen == 4)
//     {
//         payload[2] = frame.payload[2];
//         payload[3] = frame.payload[3];
//     }

//     flash_.read(flash_.findAdrress(memoryaddress_, MemoryAdress::DEVICE_REMARK), payload + frame.payloadLen, 20);

//     sendResponse(BusproOp::DEVICE_SEARCH_HDL.resp(), frame.srcAddress, payload, sizeof(payload));
// }

void TouchModule::handleReadMacaddress(const BusproFrame &frame)
{
    if (frame.payloadLen != 0)
        return;

    uint8_t payload[12] = {0x00};

    sendResponse(BusproOp::DEVICE_MAC_ADDRESS.readResp(), frame.srcAddress, payload, sizeof(payload));
}
void TouchModule::handleModifyMacaddress(const BusproFrame &frame)
{
    if (frame.payloadLen != 10)
        return;

    // if (!mcu::bufferEquals(frame.payload, mcu::getMcuUID()))
    //     return;

    uint8_t address[2] = {frame.payload[8], frame.payload[9]};

    flash_.update(flash_.findAdrress(memoryaddress_, MemoryAdress::DEVICE_ADDRESS), address, 2);
    syncValues();

    uint8_t payload[1] = {BusproOp::SUCCESS};
    sendResponse(BusproOp::DEVICE_MAC_ADDRESS.writeResp(), frame.srcAddress, payload, sizeof(payload));
}

void TouchModule::handleReadDeviceRemark(const BusproFrame &frame)
{
    if (frame.payloadLen != 0)
        return;

    uint8_t payload[20] = {}; // zeroed so the CONF= branch below (snprintf, which
                              // won't fill all 20 bytes) doesn't leak the stack

    if (configMode == 0)
    {
        flash_.read(flash_.findAdrress(memoryaddress_, MemoryAdress::DEVICE_REMARK), payload, sizeof(payload));
    }
    else
    {
        snprintf(reinterpret_cast<char *>(payload), sizeof(payload), "CONF=%d", configMode);
    }

    sendWithCooldown(BusproOp::DEVICE_REMARK.readResp(), 0xFFFF, payload, sizeof(payload));
}

void TouchModule::handleModifyDeviceRemark(const BusproFrame &frame)
{
    if (frame.payloadLen != 20)
        return;

    // Parse CONF= value
    if (strncmp(reinterpret_cast<const char *>(frame.payload), "CONF=", 5) == 0)
    {
        configMode = atoi(reinterpret_cast<const char *>(frame.payload + 5));
    }
    else if (strncmp(reinterpret_cast<const char *>(frame.payload), "INITIALIZATION", 14) == 0)
    {
        init();
    }
    else
    {
        flash_.update(flash_.findAdrress(memoryaddress_, MemoryAdress::DEVICE_REMARK), frame.payload, frame.payloadLen);
    }

    uint8_t payload[1] = {BusproOp::SUCCESS};
    sendResponse(BusproOp::DEVICE_REMARK.writeResp(), frame.srcAddress, payload, sizeof(payload));
}

/////////////////////////////// UNIVERSAL REQUEST ///////////////////////////////
/////////////////////////////////////////////////////////////////////////////////

void TouchModule::updateChannelLED(uint16_t srcAddress, uint8_t channel, bool state)
{
    using SwitchType = SwitchPanel::SwitchType;

    uint8_t function[7];

    for (uint8_t i = 0; i < 16; i++)
    {
        SwitchType keyType =
            static_cast<SwitchType>(switchPanel_.getKeyType(i));

        bool isMerge = false;

        switch (keyType)
        {
        case SwitchType::SingleON:
        case SwitchType::SingleOFF:
        case SwitchType::SingleONOFF:
        case SwitchType::CombinationON:
        case SwitchType::CombinationOFF:
        case SwitchType::CombinationONOFF:
            isMerge = true;
            break;

        case SwitchType::DblclickSingle:
        case SwitchType::DblclickCombined:
        case SwitchType::ShortLongPress:
        case SwitchType::ShortLongJog:
            isMerge = false;
            break;

        case SwitchType::Invalid:
        default:
            ledState[i * 2] = 0;
            ledState[i * 2 + 1] = 0;
            continue;
        }

        uint8_t functionIndices[2] = {1, 50};
        uint8_t count = isMerge ? 1 : 2;

        for (uint8_t j = 0; j < count; j++)
        {
            switchPanel_.getKeyFunction(function, i + 1, functionIndices[j]);

            uint16_t address =
                (static_cast<uint16_t>(function[1]) << 8) |
                function[2];

            uint16_t opCode = switchPanel_.getOperationCode(function[0]);

            if (address == srcAddress &&
                opCode == BusproOp::CONTROL_SINGLE.req() &&
                function[3] == channel)
            {
                uint8_t led = i * 2;

                if (isMerge)
                {
                    ledState[led] = state;
                    ledState[led + 1] = state;
                }
                else
                {
                    ledState[led + (functionIndices[j] == 50 ? 1 : 0)] = state;
                }
            }
        }
    }
}

void TouchModule::handleSingleChannel(const BusproFrame &frame)
{
    if (frame.payloadLen < 3)
        return;

    const uint8_t *payload = frame.payload;

    using SwitchType = SwitchPanel::SwitchType;

    // OLD FORMAT:
    // [channel] [result] [state]
    if (frame.payloadLen < 6)
    {
        uint8_t channel = payload[0];
        bool state = payload[2] != 0;

        updateChannelLED(frame.srcAddress, channel, state);
        return;
    }

    // NEW FORMAT:
    // [channel] [result] [state] [maxChannel] [bitmap_L] [bitmap_H] [...]

    if (frame.payloadLen >= 6)
    {
        uint8_t maxChannel = payload[3];

        if (maxChannel > 12)
            maxChannel = 12;

        uint16_t channelStates = static_cast<uint16_t>(payload[4]) | (static_cast<uint16_t>(payload[5]) << 8);

        for (uint8_t channel = 1; channel <= maxChannel; channel++)
        {
            bool state = (channelStates & (1U << (channel - 1))) != 0;

            updateChannelLED(frame.srcAddress, channel, state);
        }
    }
}

void TouchModule::handleStateChannel(const BusproFrame &frame)
{
    if (frame.payloadLen < 7)
        return;

    const uint8_t activeZones = frame.payload[0];

    const uint8_t *payload = frame.payload;

    uint16_t channelStates = static_cast<uint16_t>(payload[activeZones + 2]) | (static_cast<uint16_t>(payload[activeZones + 3]) << 8);

    for (uint8_t channel = 1; channel <= 12; channel++)
    {
        bool state = (channelStates & (1U << (channel - 1))) != 0;

        updateChannelLED(frame.srcAddress, channel, state);
    }
}

void TouchModule::handleSceneControl(const BusproFrame &frame)
{
    if (frame.payloadLen < 2)
        return;

    uint8_t zone = frame.payload[0];
    uint8_t scene = frame.payload[1];

    uint8_t function[7];

    using SwitchType = SwitchPanel::SwitchType;

    for (uint8_t i = 0; i < 16; i++)
    {
        SwitchType keyType = static_cast<SwitchType>(switchPanel_.getKeyType(i));

        bool isMerge = false;

        switch (keyType)
        {
        case SwitchType::SingleON:
        case SwitchType::SingleOFF:
        case SwitchType::SingleONOFF:
        case SwitchType::CombinationON:
        case SwitchType::CombinationOFF:
        case SwitchType::CombinationONOFF:
            isMerge = true;
            break;

        case SwitchType::DblclickSingle:
        case SwitchType::DblclickCombined:
        case SwitchType::ShortLongPress:
        case SwitchType::ShortLongJog:
            isMerge = false;
            break;

        case SwitchType::Invalid:
        default:
            ledState[i * 2] = 0;
            ledState[i * 2 + 1] = 0;
            continue;
        }

        uint8_t functionIndices[2];
        uint8_t count;

        if (isMerge)
        {
            functionIndices[0] = 1;
            count = 1;
        }
        else
        {
            functionIndices[0] = 1;
            functionIndices[1] = 50;
            count = 2;
        }

        for (uint8_t j = 0; j < count; j++)
        {
            switchPanel_.getKeyFunction(function, i + 1, functionIndices[j]);

            uint16_t address = (static_cast<uint16_t>(function[1]) << 8) | function[2];

            uint16_t opCode = switchPanel_.getOperationCode(function[0]);

            if (address == frame.srcAddress &&
                opCode == BusproOp::SCENE_CONTROL.req() &&
                function[3] == zone)
            {
                uint8_t led = i * 2;

                bool ledStateValue = (scene == function[4]);

                if (isMerge)
                {
                    ledState[led] = ledStateValue;
                    ledState[led + 1] = ledStateValue;
                }
                else
                {
                    ledState[led + (functionIndices[j] == 50 ? 1 : 0)] = ledStateValue;
                }
            }
        }
    }
}