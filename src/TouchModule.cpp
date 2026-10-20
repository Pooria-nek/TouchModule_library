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
      switchPanel_(flash),
      ntc1(PB0, 10000.0f, 1000),
      ntc2(PB1, 10000.0f, 1000)
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
    ntc1.begin();
    ntc2.begin();

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
    ntc1.update();
    ntc2.update();

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
        display_.updateInTemprature(ntc1);
        display_.updateOutTemprature(ntc2);
    }

    if (display_.getPage() == 5)
    {
        hvac_.fetchImage();
        display_.updateResource(hvac_);
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

    leds_.setLedMode(8, LedHandler<TOUCH_PAD_COUNT>::LedMode::Active);
    leds_.setLedMode(9, LedHandler<TOUCH_PAD_COUNT>::LedMode::Active);
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

        // Switch pages
    }
    break;

    case 5:
    {
        constexpr float TEMP_STEP = 1.0f;
        bool acted = false;
        uint8_t ledIndex = 0;

        if (touch_.isHoldEdge(0))
        {
            if (hvac_.nextMode())
            {
                acted = true;
                ledIndex = 0;
            }
        }

        if (touch_.isHoldEdge(1))
        {
            hvac_.togglePower();
            acted = true;
            ledIndex = 1;
        }

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

        if (touch_.isPressed(4))
        {
            if (hvac_.changeFan(false))
            {
                acted = true;
                ledIndex = 4;
            }
        }

        if (touch_.isPressed(5))
        {
            if (hvac_.changeFan(true))
            {
                acted = true;
                ledIndex = 5;
            }
        }

        if (touch_.isPressed(6))
        {
            if (hvac_.changeHvac(false))
            {
                acted = true;
                ledIndex = 6;
            }
        }

        if (touch_.isPressed(7))
        {
            if (hvac_.changeHvac(true))
            {
                acted = true;
                ledIndex = 7;
            }
        }

        if (acted)
        {
            buzzer_.click();
            leds_.setLedMode(ledIndex, LedHandler<TOUCH_PAD_COUNT>::LedMode::Blink);
        }
    }
    break;

    case 6:
    {
        // //--------------------------------- power -------------------------
        // if ((isPressed(1) || isPressed(2)))
        // {
        //     fheat.togglePower();
        // }

        // //------------------------------------- set point ----------------------------------------------
        // if (isPressed(3))
        // {
        //     fheat.changeTemp(false);
        // }
        // if (isPressed(4))
        // {
        //     fheat.changeTemp(true);
        // }

        // //-------------------------change work mode -------------------------------------
        // if (isPressed(7)) // change floorheat mode
        // {
        //     fheat.changeMode(false);

        //     // if (floor_heat_power_ddp)
        //     // {
        //     //     if (floor_heat_temp_mode > 1)
        //     //         floor_heat_temp_mode--;
        //     //     else
        //     //         floor_heat_temp_mode = 4;
        //     //     change_floor_heat_mode_switch();
        //     //     // showtemp_flag = true;
        //     // }
        // }

        // if (isPressed(8)) // change floorheat mode
        // {
        //     fheat.changeMode(true);

        //     // if (floor_heat_power_ddp)
        //     // {
        //     //     if (floor_heat_temp_mode < 4)
        //     //         floor_heat_temp_mode++;
        //     //     else
        //     //         floor_heat_temp_mode = 1;
        //     //     change_floor_heat_mode_switch();
        //     //     // showtemp_flag = true;
        //     // }
        // }
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

    uint8_t keyType = switchPanel_.getKeyType(merged);

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
        ledState[merged * 2] = 0;
        ledState[merged * 2 + 1] = 0;
        return;
    }

    if (isMerge)
    {
        ledState[merged * 2] = !ledState[merged * 2];
        ledState[merged * 2 + 1] = !ledState[merged * 2 + 1];
    }
    else
    {
        ledState[separate] = !ledState[separate];
    }

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

        if (functionRunnerState_.functionExecuting)
        {
            if (functionRunnerState_.isMerge)
            {
                uint8_t merged = functionRunnerState_.keyNumber;

                // ledState[(merged * 2)] = 0x00;
                // ledState[(merged * 2) + 1] = 0x00;
            }
            else
            {
                uint8_t separate = functionRunnerState_.keyNumber;

                // ledState[separate] = 0x00;
            }
        }

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
    uint8_t payload[2] = {static_cast<uint8_t>(deviceAddress_ >> 8), static_cast<uint8_t>(deviceAddress_ & 0xFF)};
    flash_.write(flash_.findAdrress(memoryaddress_, MemoryAdress::DEVICE_ADDRESS), payload, sizeof(payload));

    uint8_t payload1[2] = {static_cast<uint8_t>(devType_ >> 8), static_cast<uint8_t>(devType_ & 0xFF)};
    flash_.write(flash_.findAdrress(memoryaddress_, MemoryAdress::DEVICE_TYPE), payload1, sizeof(payload1));

    // // MCU UID
    // const uint8_t *uid = mcu::getMcuUID();
    // flash_.write(flash_.findAdrress(memoryaddress_, MemoryAdress::DEVICE_TYPE), uid, mcu::MCU_UID_LEN);

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

    // flash_.readObject(flash_.findAdrress(memoryaddress_, MemoryAdress::DEVICE_MAC_ADDRESS), uid_);

    // flash_.read(flash_.findAdrress(memoryaddress_, MemoryAdress::Touch::CHANNEL_MODE), payload, LOGICAL_BUTTON_COUNT);
    // flash_.read(flash_.findAdrress(memoryaddress_, MemoryAdress::Touch::CHANNEL_STATUE), payload, LOGICAL_BUTTON_COUNT);
    // flash_.read(flash_.findAdrress(memoryaddress_, MemoryAdress::Touch::CHANNEL_DIMMING), payload, LOGICAL_BUTTON_COUNT);
    // flash_.read(flash_.findAdrress(memoryaddress_, MemoryAdress::Touch::CHANNEL_DIMMING_VALUE), payload, LOGICAL_BUTTON_COUNT);

    fetchValidPages();
    fetchSleepValues();
    fetchProxValues();

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

        case BusproOp::CONTROL_SINGLE.resp():
            handleSingleChannel(frame);
            break;

        case BusproOp::SCENE_CONTROL.resp():
            handleSceneControl(frame);
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
            handleReadPOpration5(frame);
            break;
        case BusproOp::Touch::SLEEPING.writeReq():
            handleModifyPOpration5(frame);
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
            break;
        case BusproOp::Touch::FH_STATE.writeReq():
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

            // case BusproOp::CONTROL_SINGLE.resp():
            //     break;

            // case BusproOp::SCENE_CONTROL.resp():
            //     break;
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

    // payload[5] = payload[6];
    // payload[6] = payload[5];

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

    uint8_t payload[9] = {}; // stub — no backing state yet; zero rather than leak the stack

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

void TouchModule::handleReadPOpration5(const BusproFrame &frame)
{
    if (frame.payloadLen != 0)
        return;

    uint8_t payload[9] = {}; // stub — no backing state yet; zero rather than leak the stack

    sendResponse(BusproOp::Touch::SLEEPING.readResp(), frame.srcAddress, payload, sizeof(payload));
}
void TouchModule::handleModifyPOpration5(const BusproFrame &frame)
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

    uint8_t adjust = 10;

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

    constexpr uint8_t HVAC_INDEX_OFFSET = 4;

    const uint8_t requestedHvac = frame.payload[HVAC_INDEX_OFFSET];

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

    uint8_t payload[1] = {};

    sendResponse(BusproOp::Touch::AC_IRREAD.readResp(), frame.srcAddress, payload, sizeof(payload));
}
void TouchModule::handleModifyHvacIRread(const BusproFrame &frame)
{
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
    // if (frame.payloadLen != 0)
    //     return;

    uint8_t payload[9] = {};

    sendResponse(BusproOp::Touch::FH_INFORMATION.readResp(), frame.srcAddress, payload, sizeof(payload));
}
void TouchModule::handleModifyFHInfo(const BusproFrame &frame)
{
    // if (frame.payloadLen != 8)
    //     return;

    uint8_t payload[5] = {BusproOp::SUCCESS};

    sendResponse(BusproOp::Touch::FH_INFORMATION.writeResp(), frame.srcAddress, payload, sizeof(payload));
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

    uint8_t payload[3] = {frame.payload[0], frame.payload[1], frame.payload[2]};

    sendResponse(BusproOp::Touch::PANEL_CONTROL.resp(), frame.srcAddress, payload, sizeof(payload));
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

    // mcu::copyArray(mcu::getMcuUID(), payload, 8);

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
    else
    {
        flash_.update(flash_.findAdrress(memoryaddress_, MemoryAdress::DEVICE_REMARK), frame.payload, frame.payloadLen);
    }

    uint8_t payload[1] = {BusproOp::SUCCESS};
    sendResponse(BusproOp::DEVICE_REMARK.writeResp(), frame.srcAddress, payload, sizeof(payload));
}

/////////////////////////////// UNIVERSAL REQUEST ///////////////////////////////
/////////////////////////////////////////////////////////////////////////////////

void TouchModule::handleSingleChannel(const BusproFrame &frame)
{
    if (frame.payloadLen < 3)
        return;

    uint8_t channel = frame.payload[0];
    bool state = frame.payload[2];

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

void TouchModule::handleSceneControl(const BusproFrame &frame)
{
    if (frame.payloadLen < 2)
        return;

    uint8_t scene = frame.payload[0];
    bool state = frame.payload[1];

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
                function[3] == scene)
            {
                uint8_t led = i * 2;

                bool ledStateValue = (state == function[4]);

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