#include "TouchModule.h"

TouchModule::TouchModule(
    TwoWire &wirePort,
    BusproTransport &bus,
    MemoryCore &flash,
    uint32_t sectorAddress,
    const uint8_t ledPins[TOUCH_PAD_COUNT],
    const uint8_t touchPads[TOUCH_PAD_COUNT],
    bool activeHigh)
    : wire_(wirePort), bus_(bus), flash_(flash), memoryaddress_(sectorAddress), activeHigh_(activeHigh)
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
      hvac_(flash),
      switchPanel_(flash),
      ntc1(PB0, 10000.0f, 1000),
      ntc2(PB1, 10000.0f, 1000)
{
    // mcu::copyMcuUID(uid_);
    for (uint8_t i = 0; i < TOUCH_PAD_COUNT; i++)
    {
        touchPins_[i] = touchPads[i];
    }

    leds_.configure(ledPins, activeHigh);
    leds_.setLedLevels(25, 2, 31);
}

bool TouchModule::begin()
{
    ntc1.begin();
    ntc2.begin();

    wire_.begin();

    leds_.begin();
    leds_.startupAnimation(); // start the idle timer from "device ready"

#ifdef HAS_OLED_DISPLAY
    display_.begin();
#endif

#ifdef HAS_BUZZER
    buzzer_.startup();
#endif

#ifdef HAS_APDS
    if (apds_.init())
    {
        apds_.enableProximitySensor();
        apds_.enableLightSensor();

        apds_.setLightThresholds(20, 200);
        apds_.setBrightnessRange(7, 15);
    }
#endif

    delay(3000);

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
}

void TouchModule::update()
{
    ntc1.update();
    ntc2.update();

    // Poll the BS8112 for new touch state (call every loop iteration)
    updateBS8112();

    buttonUpdate();

    updateFunctionRunner();

    // Non-blocking — auto-sleep after sleepTimeoutMs_ with no activity
    checkAutoSleep();

    buzzer_.poll();

    updateAPDS();

    display_.clear();

    hvac_.fetchImage();

    display_.updateResource(hvac_);

    display_.updateInTemprature(ntc1);
    display_.updateOutTemprature(ntc2);

    display_.update();
}

// void TouchModule::setKeyType(uint8_t key, ButtonType type)
// {
//     if (key >= LOGICAL_BUTTON_COUNT)
//         return;

//     keytype_[key] = type;
// }

// TouchModule::ButtonType TouchModule::getKeyType(uint8_t key)
// {
//     if (key >= LOGICAL_BUTTON_COUNT)
//         return ButtonType::Invalid;
//     // NOTE: was keytype_[key - 1] — inconsistent with setKeyType()'s 0-based
//     // indexing, and for key == 0 the uint8_t underflow (0 - 1 == 255) read
//     // 255 elements past the array. Fixed to match setKeyType().
//     return keytype_[key];
// }

void TouchModule::fetchValidPages()
{
    uint8_t value[6] = {};

    flash_.read(flash_.findAdrress(MemoryAdress::Touch::SECTOR_INFO, MemoryAdress::Touch::PAGE_VALID_BASE), value, 6);

    display_.setValidPages(value);
}

void TouchModule::fetchSleepValues()
{
    uint8_t value[6] = {};

    flash_.read(flash_.findAdrress(MemoryAdress::Touch::SECTOR_INFO, MemoryAdress::Touch::PAGE_SLEEP_TIME), value, 6);

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
    flash_.read(flash_.findAdrress(MemoryAdress::Touch::SECTOR_INFO, MemoryAdress::Touch::PROX_FLAG), value, 8);

    if (value[0] == 0)
    {
        apds_.enableProximitySensor();
    }
    else
    {
        apds_.disableProximitySensor();
    }
}

void TouchModule::buttonUpdate()
{
    const uint8_t page = display_.getPage();
    updatePageLeds(page);
#ifdef HAS_OLED_DISPLAY

    if (isPressed(8))
    {
        if (display_.changePage(false))
        {
            buzzer_.click();
        }
    }
    else if (isPressed(9))
    {
        if (display_.changePage(true))
        {
            buzzer_.click();
        }
    }

    leds_.setLedMode(8, LedHandler<TOUCH_PAD_COUNT>::LedMode::Active);

    leds_.setLedMode(9, LedHandler<TOUCH_PAD_COUNT>::LedMode::Active);

#endif

    switch (page)
    {
    case 1:
    case 2:
    case 3:
    case 4:
    {

        uint16_t key = pressedKey();

        if (key != UINT16_MAX)
        {
            buzzer_.click();
            // ledRunner(page, key);
            functionRunner(page, key);
        }

        // Switch pages
    }
    break;

    case 5:
    {
        constexpr float TEMP_STEP = 1.0f;
        bool acted = false;
        uint8_t ledIndex = 0;

        if (isHoldEdge(0))
        {
            hvac_.nextMode();
            acted = true;
            ledIndex = 0;
        }

        if (isHoldEdge(1))
        {
            hvac_.togglePower();
            acted = true;
            ledIndex = 1;
        }

        if (isPressed(2))
        {
            hvac_.decreaseTemp(TEMP_STEP);
            acted = true;
            ledIndex = 2;
        }

        if (isPressed(3))
        {
            hvac_.increaseTemp(TEMP_STEP);
            acted = true;
            ledIndex = 3;
        }

        if (isPressed(4))
        {
            hvac_.changeFan(false);
            acted = true;
            ledIndex = 4;
        }

        if (isPressed(5))
        {
            hvac_.changeFan(true);
            acted = true;
            ledIndex = 5;
        }

        if (isPressed(6))
        {
            hvac_.changeHvac(false);
            acted = true;
            ledIndex = 6;
        }

        if (isPressed(7))
        {
            hvac_.changeHvac(true);
            acted = true;
            ledIndex = 7;
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

        leds_.setLedMode(key, ledState[ledAddress] ? LedMode::Active : LedMode::Deactive);
    }
}

void TouchModule::ledRunner(uint8_t page, uint16_t key)
{
    uint16_t L = getLogicalButton(key);
    uint16_t P = getPhysicalButton(key);

    uint8_t keyType = switchPanel_.getKeyType(L);

    uint8_t ledAddressSingle = ((page - 1) * 8) + key;

    switch (keyType)
    {
    case static_cast<uint8_t>(SwitchPanel::SwitchType::SingleON):
    case static_cast<uint8_t>(SwitchPanel::SwitchType::SingleOFF):
    case static_cast<uint8_t>(SwitchPanel::SwitchType::SingleONOFF):
    case static_cast<uint8_t>(SwitchPanel::SwitchType::CombinationON):
    case static_cast<uint8_t>(SwitchPanel::SwitchType::CombinationOFF):
    case static_cast<uint8_t>(SwitchPanel::SwitchType::CombinationONOFF):
        ledState[ledAddressSingle] = !ledState[ledAddressSingle];

        // Toggle its paired LED
        uint8_t pairedAddress;

        if ((ledAddressSingle % 2) == 0)
            pairedAddress = ledAddressSingle + 1;
        else
            pairedAddress = ledAddressSingle - 1;

        ledState[pairedAddress] = !ledState[pairedAddress];

        break;

    case static_cast<uint8_t>(SwitchPanel::SwitchType::DblclickSingle):
    case static_cast<uint8_t>(SwitchPanel::SwitchType::DblclickCombined):
    case static_cast<uint8_t>(SwitchPanel::SwitchType::ShortLongPress):
    case static_cast<uint8_t>(SwitchPanel::SwitchType::ShortLongJog):
        ledState[ledAddressSingle] = !ledState[ledAddressSingle];
        break;

    case static_cast<uint8_t>(SwitchPanel::SwitchType::Invalid):
        ledState[ledAddressSingle] = 0x00;
        break;

    default:
        break;
    }

    // uint8_t payload[5];

    // payload[0] = page;
    // payload[1] = key;
    // payload[2] = L;

    // payload[3] = P;
    // payload[4] = keyType;
    // payload[5] = ledState[ledAddress];

    // sendResponse(0xCCCC, 0xCCCC, payload, sizeof(payload));
    //     sendResponse(0xCCCC, 0xCCCC, ledState, sizeof(ledState));
}

void TouchModule::functionRunner(uint8_t page, uint16_t key)
{
    if (functionRunnerState_.active)
        return;

    uint16_t L = getLogicalButton(key);
    uint16_t P = getPhysicalButton(key);

    uint8_t keyType = switchPanel_.getKeyType(L);

    uint8_t functionStart = 0;
    uint8_t functionCount = 0;

    // ---------------------------------------------------------
    // LED
    // ---------------------------------------------------------

    const uint8_t ledAddress = ((page - 1) * 8) + key;

    switch (keyType)
    {
    case static_cast<uint8_t>(SwitchPanel::SwitchType::SingleON):
    case static_cast<uint8_t>(SwitchPanel::SwitchType::SingleOFF):
    case static_cast<uint8_t>(SwitchPanel::SwitchType::SingleONOFF):
    case static_cast<uint8_t>(SwitchPanel::SwitchType::CombinationON):
    case static_cast<uint8_t>(SwitchPanel::SwitchType::CombinationOFF):
    case static_cast<uint8_t>(SwitchPanel::SwitchType::CombinationONOFF):
        ledState[ledAddress] = !ledState[ledAddress];

        // Toggle its paired LED
        uint8_t pairedAddress;

        if ((ledAddress % 2) == 0)
            pairedAddress = ledAddress + 1;
        else
            pairedAddress = ledAddress - 1;

        ledState[pairedAddress] = !ledState[pairedAddress];

        break;

    case static_cast<uint8_t>(SwitchPanel::SwitchType::DblclickCombined):
        ledState[ledAddress] = !ledState[ledAddress];
        break;

    case static_cast<uint8_t>(SwitchPanel::SwitchType::DblclickSingle):
    case static_cast<uint8_t>(SwitchPanel::SwitchType::ShortLongPress):
    case static_cast<uint8_t>(SwitchPanel::SwitchType::ShortLongJog):
        ledState[ledAddress] = !ledState[ledAddress];
        break;

    case static_cast<uint8_t>(SwitchPanel::SwitchType::Invalid):
        ledState[ledAddress] = 0x00;
        return;

    default:
        return;
    }

    // ---------------------------------------------------------
    // Function
    // ---------------------------------------------------------

    switch (keyType)
    {
    case static_cast<uint8_t>(SwitchPanel::SwitchType::SingleON):
    case static_cast<uint8_t>(SwitchPanel::SwitchType::SingleOFF):
    case static_cast<uint8_t>(SwitchPanel::SwitchType::SingleONOFF):
        functionCount = 1;
        functionStart = 0;
        break;

    case static_cast<uint8_t>(SwitchPanel::SwitchType::CombinationON):
    case static_cast<uint8_t>(SwitchPanel::SwitchType::CombinationOFF):
    case static_cast<uint8_t>(SwitchPanel::SwitchType::CombinationONOFF):
        functionCount = 99;
        functionStart = 0;
        break;

    case static_cast<uint8_t>(SwitchPanel::SwitchType::DblclickCombined):
        functionCount = 50;
        functionStart = (P & 1) ? 0 : 50;
        break;

    default:
        return;
    }

    // ---------------------------------------------------------
    // Function runner state
    // ---------------------------------------------------------

    const uint8_t ledChannel = static_cast<uint8_t>(P) - 1;

    functionRunnerState_.active = true;
    functionRunnerState_.functionExecuting = false;

    functionRunnerState_.page = page;
    functionRunnerState_.key = key;

    functionRunnerState_.logicalButton = L;
    functionRunnerState_.physicalButton = P;

    functionRunnerState_.ledChannel = ledChannel;

    functionRunnerState_.functionIndex = functionStart;
    functionRunnerState_.functionEnd = functionStart + functionCount;
}

void TouchModule::updateFunctionRunner()
{
    if (!functionRunnerState_.active)
        return;

    uint8_t processed = 0;

    while (
        functionRunnerState_.functionIndex <
            functionRunnerState_.functionEnd &&
        processed < functionRunnerState_.functionsPerUpdate)
    {
        uint8_t functionIndex =
            functionRunnerState_.functionIndex++;

        uint8_t function[7];

        flash_.read(
            flash_.findAdrress(
                MemoryAdress::Touch::SECTOR_FUNCTIONS,
                MemoryAdress::Touch::channelFunction(
                    functionRunnerState_.logicalButton,
                    functionIndex)),
            function,
            sizeof(function));

        processed++;

        // ---------------------------------------------------------
        // Invalid / unused function
        // ---------------------------------------------------------

        if (function[0] == 0xFF || function[0] == static_cast<uint8_t>(SwitchPanel::OperationType::Invalid))
        {
            continue;
        }

        // ---------------------------------------------------------
        // Destination
        // ---------------------------------------------------------

        uint16_t dstAddress =
            (static_cast<uint16_t>(function[1]) << 8) |
            function[2];

        uint8_t spacket[4] = {
            function[3],
            function[4],
            function[5],
            function[6]};

        // ---------------------------------------------------------
        // Operation information
        // ---------------------------------------------------------

        uint16_t oprCode =
            switchPanel_.getOperationCode(function[0]);

        uint8_t ssize =
            switchPanel_.getOperationLen(function[0]);

        if (oprCode == 0 || ssize == 0)
            continue;

        // ---------------------------------------------------------
        // First valid function
        // ---------------------------------------------------------

        if (!functionRunnerState_.functionExecuting)
        {
            functionRunnerState_.functionExecuting = true;

            // Start LED indication
            if (functionRunnerState_.ledChannel < TOUCH_PAD_COUNT)
            {
                leds_.setLedMode(
                    functionRunnerState_.ledChannel,
                    LedHandler<TOUCH_PAD_COUNT>::LedMode::Blinking);
            }

            // DblclickCombined uses the paired LED too
            if (
                functionRunnerState_.key == static_cast<uint8_t>(SwitchPanel::SwitchType::SingleON) ||
                functionRunnerState_.key == static_cast<uint8_t>(SwitchPanel::SwitchType::SingleOFF) ||
                functionRunnerState_.key == static_cast<uint8_t>(SwitchPanel::SwitchType::SingleONOFF) ||
                functionRunnerState_.key == static_cast<uint8_t>(SwitchPanel::SwitchType::CombinationON) ||
                functionRunnerState_.key == static_cast<uint8_t>(SwitchPanel::SwitchType::CombinationOFF) ||
                functionRunnerState_.key == static_cast<uint8_t>(SwitchPanel::SwitchType::CombinationONOFF))
            {
                uint8_t pairedAddress;

                if ((functionRunnerState_.ledChannel % 2) == 0)
                    pairedAddress = functionRunnerState_.ledChannel + 1;
                else
                    pairedAddress = functionRunnerState_.ledChannel - 1;

                if (pairedAddress < TOUCH_PAD_COUNT)
                {
                    leds_.setLedMode(
                        pairedAddress,
                        LedHandler<TOUCH_PAD_COUNT>::LedMode::Blinking);
                }
            }
        }

        // ---------------------------------------------------------
        // Execute
        // ---------------------------------------------------------

        sendConfirm(
            oprCode,
            dstAddress,
            spacket,
            sizeof(spacket));
    }

    // -------------------------------------------------------------
    // Finished
    // -------------------------------------------------------------

    if (functionRunnerState_.functionIndex >=
        functionRunnerState_.functionEnd)
    {
        if (functionRunnerState_.functionExecuting)
        {
            if (functionRunnerState_.ledChannel < TOUCH_PAD_COUNT)
            {
                leds_.finishOperation(
                    functionRunnerState_.ledChannel);
            }

            // Finish paired LED for DblclickCombined
            if (
                functionRunnerState_.key == static_cast<uint8_t>(SwitchPanel::SwitchType::SingleON) ||
                functionRunnerState_.key == static_cast<uint8_t>(SwitchPanel::SwitchType::SingleOFF) ||
                functionRunnerState_.key == static_cast<uint8_t>(SwitchPanel::SwitchType::SingleONOFF) ||
                functionRunnerState_.key == static_cast<uint8_t>(SwitchPanel::SwitchType::CombinationON) ||
                functionRunnerState_.key == static_cast<uint8_t>(SwitchPanel::SwitchType::CombinationOFF) ||
                functionRunnerState_.key == static_cast<uint8_t>(SwitchPanel::SwitchType::CombinationONOFF))
            {
                uint8_t pairedAddress;

                if ((functionRunnerState_.ledChannel % 2) == 0)
                    pairedAddress = functionRunnerState_.ledChannel + 1;
                else
                    pairedAddress = functionRunnerState_.ledChannel - 1;

                if (pairedAddress < TOUCH_PAD_COUNT)
                {
                    leds_.finishOperation(pairedAddress);
                }
            }
        }

        functionRunnerState_.active = false;
        functionRunnerState_.functionExecuting = false;
    }
}

bool TouchModule::firstime()
{
    uint8_t payload[16];
    flash_.read(flash_.findAdrress(MemoryAdress::Touch::SECTOR_INFO, MemoryAdress::DEVICE_ADDRESS), payload, sizeof(payload));

    for (uint8_t i = 0; i < sizeof(payload); i++)
    {
        if (payload[i] != 0xFF)
            return false;
    }

    return true;

    // // uint8_t fuid_[12];
    // // flash_.read(flash_.findAdrress(MemoryAdress::Touch::SECTOR_INFO, MemoryAdress::DEVICE_MAC_ADDRESS), fuid_, 12);

    // uint16_t fdevType;
    // // flash_.readObject(flash_.findAdrress(MemoryAdress::Touch::SECTOR_INFO, MemoryAdress::DEVICE_TYPE), fdevType);

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
    char storedVersion[20] = {};

    auto address = flash_.findAdrress(MemoryAdress::Touch::SECTOR_INFO, MemoryAdress::DEVICE_SOFTWARE_VER);

    flash_.read(address, storedVersion, sizeof(storedVersion));

    if (strncmp(storedVersion, SOFTWARE_VERSION, sizeof(storedVersion)) == 0)
        return false;

    char newVersion[20] = {};
    strncpy(newVersion, SOFTWARE_VERSION, sizeof(newVersion) - 1);

    flash_.update(address, SOFTWARE_VERSION, sizeof(storedVersion));

    return true;
}

bool TouchModule::init()
{
    uint8_t payload[2] = {static_cast<uint8_t>(deviceAddress_ >> 8), static_cast<uint8_t>(deviceAddress_ & 0xFF)};
    flash_.write(flash_.findAdrress(MemoryAdress::Touch::SECTOR_INFO, MemoryAdress::DEVICE_ADDRESS), payload, sizeof(payload));

    uint8_t payload1[2] = {static_cast<uint8_t>(devType_ >> 8), static_cast<uint8_t>(devType_ & 0xFF)};
    flash_.write(flash_.findAdrress(MemoryAdress::Touch::SECTOR_INFO, MemoryAdress::DEVICE_TYPE), payload1, sizeof(payload1));

    // // MCU UID
    // const uint8_t *uid = mcu::getMcuUID();
    // flash_.write(flash_.findAdrress(MemoryAdress::Touch::SECTOR_INFO, MemoryAdress::DEVICE_TYPE), uid, mcu::MCU_UID_LEN);

    // uint8_t fuid_[12];
    // flash_.write(flash_.findAdrress(MemoryAdress::Touch::SECTOR_INFO, MemoryAdress::DEVICE_MAC_ADDRESS), fuid_, sizeof(fuid_));

    flash_.write(flash_.findAdrress(MemoryAdress::Touch::SECTOR_INFO, MemoryAdress::DEVICE_REMARK), "Zeller Z27", 20);
    flash_.write(flash_.findAdrress(MemoryAdress::Touch::SECTOR_INFO, MemoryAdress::DEVICE_HARDWARE_VER), "STM3F103RB", 30);
    flash_.write(flash_.findAdrress(MemoryAdress::Touch::SECTOR_INFO, MemoryAdress::DEVICE_SOFTWARE_VER), SOFTWARE_VERSION, 20);

    uint8_t value[8] = {0, 0, 0, 0, 0, 0, 0, 0};
    flash_.write(flash_.findAdrress(MemoryAdress::Touch::SECTOR_INFO, MemoryAdress::Touch::PROX_FLAG), value, sizeof(value));

    uint8_t validpage[6] = {1, 0, 0, 0, 0, 0};
    flash_.write(flash_.findAdrress(MemoryAdress::Touch::SECTOR_INFO, MemoryAdress::Touch::PAGE_VALID_BASE), validpage, sizeof(validpage));

    char remark[20];
    for (size_t i = 1; i <= LOGICAL_BUTTON_COUNT; i++)
    {
        // uint8_t channel = i + 1;
        snprintf(remark, sizeof(remark), "Button %d", static_cast<unsigned>(i));
        flash_.write(flash_.findAdrress(MemoryAdress::Touch::SECTOR_INFO, MemoryAdress::channelRemark(i)), remark, sizeof(remark));

        uint8_t mode = static_cast<uint8_t>(SwitchPanel::SwitchType::Invalid);

        flash_.write(flash_.findAdrress(MemoryAdress::Touch::SECTOR_INFO, MemoryAdress::channelAddress(MemoryAdress::Touch::CHANNEL_MODE, i)), &mode, sizeof(mode));
        flash_.write(flash_.findAdrress(MemoryAdress::Touch::SECTOR_INFO, MemoryAdress::channelAddress(MemoryAdress::Touch::CHANNEL_STATUE, i)), 0, 1);
        flash_.write(flash_.findAdrress(MemoryAdress::Touch::SECTOR_INFO, MemoryAdress::channelAddress(MemoryAdress::Touch::CHANNEL_DIMMING, i)), 0, 1);
        flash_.write(flash_.findAdrress(MemoryAdress::Touch::SECTOR_INFO, MemoryAdress::channelAddress(MemoryAdress::Touch::CHANNEL_DIMMING_VALUE, i)), 0, 1);
    }
}

bool TouchModule::syncValues()
{
    uint8_t payload[2] = {};
    flash_.read(flash_.findAdrress(MemoryAdress::Touch::SECTOR_INFO, MemoryAdress::DEVICE_ADDRESS), payload, 2);
    deviceAddress_ = (static_cast<uint16_t>(payload[0]) << 8) | payload[1];

    // flash_.readObject(flash_.findAdrress(MemoryAdress::Touch::SECTOR_INFO, MemoryAdress::DEVICE_MAC_ADDRESS), uid_);

    // flash_.read(flash_.findAdrress(MemoryAdress::Touch::SECTOR_INFO, MemoryAdress::Touch::CHANNEL_MODE), payload, LOGICAL_BUTTON_COUNT);
    // flash_.read(flash_.findAdrress(MemoryAdress::Touch::SECTOR_INFO, MemoryAdress::Touch::CHANNEL_STATUE), payload, LOGICAL_BUTTON_COUNT);
    // flash_.read(flash_.findAdrress(MemoryAdress::Touch::SECTOR_INFO, MemoryAdress::Touch::CHANNEL_DIMMING), payload, LOGICAL_BUTTON_COUNT);
    // flash_.read(flash_.findAdrress(MemoryAdress::Touch::SECTOR_INFO, MemoryAdress::Touch::CHANNEL_DIMMING_VALUE), payload, LOGICAL_BUTTON_COUNT);

    fetchValidPages();
    fetchSleepValues();
    fetchProxValues();

    return true;
}

void TouchModule::updateAPDS()
{
    apds_.update();

    uint16_t prox = apds_.getProximity();

    if (prox > 800)
    {
        if (deviceMode_ == DeviceMode::Sleep)
        {
            wake();
        }
        if (deviceMode_ == DeviceMode::Wake)
        {
            markActivity();
        }
    }
}

////////////////////////////////////////////////////////////////////////////////
/////////////////////////////// BS811x Functions ///////////////////////////////

void TouchModule::initBS8112()
{
    _touchState = 0;

    // BS8112 Initialization
    uint8_t config[17];
    uint8_t KeyTriggerthresholdvalue = 7; // 1-32

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

void TouchModule::irqTouchHandler()
{
    irqTouchFlag_ = true;
}

bool TouchModule::getIrq()
{
    return irqTouchFlag_;
}

/**
 * @brief Polls the hardware for state changes on the configured pads only.
 * @return true if any key state changed, false otherwise.
 */
bool TouchModule::updateBS8112()
{
    if (!irqTouchFlag_ && !_runAgain)
        return false;

    // Manage IRQ flag for continuous polling if required
    if (irqTouchFlag_)
    {
        irqTouchFlag_ = false;
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

    // Remap configured physical pads into compact bitfield
    uint16_t newState = 0;

    for (uint8_t i = 0; i < TOUCH_PAD_COUNT; i++)
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

    // -------------------------------------------------
    // SLEEP MODE
    // First touch only wakes the device.
    // It must NOT perform the normal button action.
    // -------------------------------------------------
    if (deviceMode_ == DeviceMode::Sleep && _pressedEdge != 0)
    {
        wake();

        // Clear the press so the same touch cannot
        // accidentally be treated as a normal action.
        _pressedEdge = 0;

        // Reset hold state for safety
        _holdActive = 0;

        for (uint8_t key = 0; key < TOUCH_PAD_COUNT; key++)
            _lastPressTime[key] = now;

        return changed;
    }

    for (uint8_t key = 0; key < TOUCH_PAD_COUNT; key++)
    {
        if (_pressedEdge & (1 << key))
        {
            _lastPressTime[key] = now;
            _holdActive &= ~(1 << key);
        }
    }

    return changed;
}

uint8_t TouchModule::getPhysicalButton(uint8_t key)
{
    if (key >= 8)
        return 0;

    return key + 1 + ((display_.getPage() - 1) * 8);
}

uint8_t TouchModule::getLogicalButton(uint8_t key)
{
    if (key >= 8)
        return 0;

    return (key / 2) + 1 + ((display_.getPage() - 1) * 4);
}

void TouchModule::sleep()
{
    deviceMode_ = DeviceMode::Sleep;
    leds_.sleep();
    display_.sleep();
}

void TouchModule::wake()
{
    deviceMode_ = DeviceMode::Wake;
    leds_.wake();
    display_.wake();

    markActivity(); // waking counts as activity — restart the idle timer
}

void TouchModule::checkAutoSleep()
{
    if (deviceMode_ == DeviceMode::Wake && (millis() - lastActivityTime_ >= sleepTimeoutMs_))
    {
        sleep();
    }
}

// true for the entire duration the channel is held down
bool TouchModule::isHold(uint8_t key)
{
    if (key >= TOUCH_PAD_COUNT)
        return false;
    return (_touchState & (1 << key)) != 0;
}

// just run on touch edge once
bool TouchModule::isPressed(uint8_t key)
{
    if (key >= TOUCH_PAD_COUNT)
        return false;
    return (_pressedEdge & (1 << key)) != 0;
}

uint16_t TouchModule::pressedKey()
{
    for (uint8_t key = 0; key < TOUCH_PAD_COUNT; ++key)
    {
        if (_pressedEdge & (uint16_t(1) << key))
        {
            _pressedEdge &= ~(uint16_t(1) << key);
            return key;
        }
    }

    return UINT16_MAX;
}

// just run on release edge once
bool TouchModule::isReleased(uint8_t key)
{
    if (key >= TOUCH_PAD_COUNT)
        return false;
    return (_releasedEdge & (1 << key)) != 0;
}

// true once, the first time a channel has been held past TOUCH_HOLD_TIME
bool TouchModule::isHoldEdge(uint8_t key)
{
    if (key >= TOUCH_PAD_COUNT)
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
// in LedHandler.h — TouchModule just owns a LedHandler<TOUCH_PAD_COUNT>
// instance (leds_) and forwards its public API (see TouchModule.h).

void TouchModule::process(const BusproFrame &frame)
{
    // if (frame.devType != devType_)
    //     return;

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

        case BusproOp::Touch::PANEL_CONTROL.req():
            handlePanelControl(frame);
            break;

            //////////////////////////////////// RESPONDS //////////////////////////////////////

        case BusproOp::CONTROL_SINGLE.resp():
            break;

        case BusproOp::SCENE_CONTROL.resp():
            break;
        }
    }
}

void TouchModule::sendResponse(uint16_t opcode, uint16_t dst, const uint8_t *payload, uint8_t payloadLen)
{
    delay(50);
    bus_.send(deviceAddress_, devType_, opcode, dst, payload, payloadLen);
}

bool TouchModule::sendConfirm(uint16_t opcode, uint16_t dst, const uint8_t *payload, uint8_t payloadLen)
{
    constexpr uint8_t MAX_RETRIES = 3;
    constexpr uint32_t TIMEOUT = 100;

    const uint16_t expectedOpcode = opcode + 1;
    const uint16_t expectedAddress = dst;

    for (uint8_t attempt = 0; attempt < MAX_RETRIES; attempt++)
    {
        bus_.send(deviceAddress_, devType_, opcode, dst, payload, payloadLen);

        const uint32_t start = millis();

        while (millis() - start < TIMEOUT)
        {
            BusproFrame frame;
            if (bus_.poll(frame))
            {
                if (frame.srcAddress == expectedAddress &&
                    frame.opCode == expectedOpcode)
                {
                    return true;
                }
            }
        }
    }

    return false;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// 00001 2026/09/27 15:04:59:235  0D 01 12 00 95 00 02 01 06 01 01 97 FE
// 00002 2026/09/27 15:04:59:936  0D 01 12 00 95 00 02 01 06 02 01 C2 AD

// 00005 2026/09/27 15:05:14:690  0D 01 12 00 95 00 1A 01 06 04 01 6E 7C
// 00006 2026/09/27 15:05:15:290  0D 01 12 00 95 00 02 01 07 01 01 A0 CE

// 00008 2026/09/27 15:05:21:011  0D 01 12 00 95 00 02 01 06 03 01 F1 9C

// 00011 2026/09/27 15:05:25:026  0D 01 12 00 95 00 02 01 06 01 00 87 DF
// 00012 2026/09/27 15:05:25:729  0D 01 12 00 95 00 02 01 06 02 00 D2 8C
// 00013 2026/09/27 15:05:26:430  0D 01 12 00 95 00 02 01 06 03 00 E1 BD
// 00014 2026/09/27 15:05:27:133  0D 01 12 00 95 00 02 01 06 04 00 78 2A
// 00015 2026/09/27 15:05:27:936  0D 01 12 00 95 00 02 01 07 01 00 B0 EF
// 00016 2026/09/27 15:05:28:540  0D 01 12 00 95 00 02 01 04 01 00 E9 BF
// 00017 2026/09/27 15:05:29:240  0D 01 12 00 95 00 02 01 05 01 00 DE 8F
// 00018 2026/09/27 15:05:32:652  0D 01 12 00 95 E0 1C 01 3A 02 00 22 82

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

    flash_.read(flash_.findAdrress(MemoryAdress::Touch::SECTOR_INFO, MemoryAdress::Touch::PAGE_LCD_DIMM), payload, 2);

    sendResponse(BusproOp::Touch::INDENSITY.readResp(), frame.srcAddress, payload, sizeof(payload));
}

void TouchModule::handleModifyIndensity(const BusproFrame &frame)
{
    if (frame.payloadLen != 11)
        return;

    flash_.update(flash_.findAdrress(MemoryAdress::Touch::SECTOR_INFO, MemoryAdress::Touch::PAGE_LCD_DIMM), frame.payload, 2);

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

    flash_.read(flash_.findAdrress(MemoryAdress::Touch::SECTOR_INFO, MemoryAdress::Touch::PROX_FLAG), payload + 1, 8);

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

    flash_.update(flash_.findAdrress(MemoryAdress::Touch::SECTOR_INFO, MemoryAdress::Touch::PROX_FLAG), frame.payload, 8);

    fetchProxValues();

    uint8_t payload[1] = {BusproOp::SUCCESS};

    sendResponse(BusproOp::Touch::UI_VALUES.writeResp(), frame.srcAddress, payload, sizeof(payload));
}

void TouchModule::handleReadEnablePage(const BusproFrame &frame)
{
    if (frame.payloadLen != 0)
        return;

    uint8_t payload[7] = {};

    flash_.read(flash_.findAdrress(MemoryAdress::Touch::SECTOR_INFO, MemoryAdress::Touch::PAGE_VALID_BASE), payload, 7);

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

    flash_.update(flash_.findAdrress(MemoryAdress::Touch::SECTOR_INFO, MemoryAdress::Touch::PAGE_VALID_BASE), valid, sizeof(valid));

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

    flash_.update(flash_.findAdrress(MemoryAdress::Touch::SECTOR_INFO, MemoryAdress::Touch::PAGE_SLEEP_TIME), frame.payload, 6);

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

    flash_.read(flash_.findAdrress(MemoryAdress::Touch::SECTOR_INFO, MemoryAdress::Touch::CHANNEL_STATUE), payload, LOGICAL_BUTTON_COUNT);

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

    flash_.read(flash_.findAdrress(MemoryAdress::Touch::SECTOR_INFO, MemoryAdress::Touch::CHANNEL_DIMMING), payload, LOGICAL_BUTTON_COUNT);

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

    flash_.read(flash_.findAdrress(MemoryAdress::Touch::SECTOR_INFO, MemoryAdress::Touch::CHANNEL_DIMMING_VALUE), payload, LOGICAL_BUTTON_COUNT);

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

    uint8_t payload[9];

    switchPanel_.getKeyFunction(payload, frame.payload[0], frame.payload[1]);

    sendResponse(BusproOp::Touch::CHANNEL_FUNCTION.readResp(), frame.srcAddress, payload, sizeof(payload));
}
void TouchModule::handleModifyTouchFunction(const BusproFrame &frame)
{
    if (frame.payloadLen != 9)
        return;

    uint8_t payload[9];
    memcpy(payload, frame.payload, sizeof(payload));

    switchPanel_.setKeyFunction(payload, frame.payload[0], frame.payload[1]);

    sendResponse(BusproOp::Touch::CHANNEL_FUNCTION.writeResp(), frame.srcAddress, payload, sizeof(payload));
}

//

void TouchModule::handleReadTouchRemark(const BusproFrame &frame)
{
    if (frame.payloadLen != 1)
        return;

    uint8_t touchNum = frame.payload[0];

    uint8_t payload[21] = {touchNum};

    flash_.read(flash_.findAdrress(MemoryAdress::Touch::SECTOR_INFO, MemoryAdress::channelRemark(touchNum)), payload + 1, 20);

    sendResponse(BusproOp::Touch::CHANNEL_REMARK.readResp(), frame.srcAddress, payload, sizeof(payload));
}
void TouchModule::handleModifyTouchRemark(const BusproFrame &frame)
{
    if (frame.payloadLen != 21)
        return;

    uint8_t touchNum = frame.payload[0];

    uint8_t payload[21] = {touchNum};

    flash_.update(flash_.findAdrress(MemoryAdress::Touch::SECTOR_INFO, MemoryAdress::channelRemark(touchNum)), frame.payload + 1, 20);

    sendResponse(BusproOp::Touch::CHANNEL_REMARK.writeResp(), frame.srcAddress, payload, sizeof(payload));
}

////////////////////////////////////// AC ///////////////////////////////////////

static uint8_t selectedHvac = 0; // was a plain global (external linkage) — could
// collide with another same-named global at
// link time. Tracks which AC index the last
// AC_INFORMATION write referred to.

void TouchModule::handleReadAcInfo(const BusproFrame &frame)
{
    if (frame.payloadLen != 0)
        return;

    HVACPanel::State shvac = hvac_.get(selectedHvac);

    // uint8_t subnetid = 0;
    // uint8_t deviceid = 0;
    uint8_t adjust = 0;

    uint8_t type = 1;
    uint8_t state = 1;

    uint8_t payload[9] = {BusproOp::SUCCESS, shvac.valid, shvac.deviceSn, shvac.deviceId, adjust, selectedHvac, type, 0, state};

    sendResponse(BusproOp::Touch::AC_INFORMATION.readResp(), frame.srcAddress, payload, sizeof(payload));
}
void TouchModule::handleModifyAcInfo(const BusproFrame &frame)
{
    if (frame.payloadLen != 8)
        return;

    // HVACPanel::State shvac;

    // shvac.valid = frame.payload[0];
    // uint8_t subnetid = frame.payload[1];
    // uint8_t deviceid = frame.payload[2];
    // uint8_t adjust = frame.payload[3];

    if (frame.payload[4] > 7)
    {
        selectedHvac = 7;
    }
    else
    {
        selectedHvac = frame.payload[4]; //
    }

    // uint8_t type = frame.payload[5]; //
    // uint8_t state = frame.payload[7];

    uint8_t payload[1] = {BusproOp::SUCCESS};

    sendResponse(BusproOp::Touch::AC_INFORMATION.writeResp(), frame.srcAddress, payload, sizeof(payload));
}

void TouchModule::handleReadAcOpration(const BusproFrame &frame)
{
    if (frame.payloadLen != 0)
        return;

    const HVACPanel::State &shvac = hvac_.get(selectedHvac);

    uint8_t payload[11] = {};

    // Fan count + valid fan numbers
    uint8_t fanCount = 0;

    for (uint8_t i = 0; i < HVACPanel::FAN_COUNT; ++i)
    {
        if (shvac.validFan[i])
        {
            payload[1 + fanCount] = i;
            fanCount++;
        }
    }

    payload[0] = fanCount;

    // Mode count + valid mode numbers
    uint8_t modeCount = 0;

    for (uint8_t i = 0; i < HVACPanel::MODE_COUNT; ++i)
    {
        if (shvac.validMode[i])
        {
            payload[6 + modeCount] = i;
            modeCount++;
        }
    }

    payload[5] = modeCount;

    sendResponse(BusproOp::Touch::AC_OPRATION_MODEL.readResp(), frame.srcAddress, payload, sizeof(payload));
}
void TouchModule::handleModifyAcOpration(const BusproFrame &frame)
{
    if (frame.payloadLen != 11)
        return;

    HVACPanel::State shvac;

    // Fan configuration
    uint8_t fanCount = frame.payload[0];

    for (uint8_t i = 0; i < HVACPanel::FAN_COUNT; ++i)
    {
        shvac.validFan[i] = false;
    }

    for (uint8_t i = 0; i < fanCount; ++i)
    {
        uint8_t fan = frame.payload[1 + i];

        if (fan < HVACPanel::FAN_COUNT)
        {
            shvac.validFan[fan] = true;
        }
    }

    // Mode configuration
    uint8_t modeCount = frame.payload[5];

    for (uint8_t i = 0; i < HVACPanel::MODE_COUNT; ++i)
    {
        shvac.validMode[i] = false;
    }

    for (uint8_t i = 0; i < modeCount; ++i)
    {
        uint8_t mode = frame.payload[6 + i];

        if (mode < HVACPanel::MODE_COUNT)
        {
            shvac.validMode[mode] = true;
        }
    }

    uint8_t payload[12] = {BusproOp::SUCCESS};

    sendResponse(BusproOp::Touch::AC_OPRATION_MODEL.writeResp(), frame.srcAddress, payload, sizeof(payload));
}

void TouchModule::handleReadACTemperature(const BusproFrame &frame)
{
    if (frame.payloadLen != 0)
        return;

    uint8_t payload[11] = {0};

    HVACPanel::State shvac = hvac_.get(selectedHvac);

    payload[0] = shvac.rangeCool[0];
    payload[1] = shvac.rangeCool[1];

    payload[2] = shvac.rangeHeat[0];
    payload[3] = shvac.rangeHeat[1];

    payload[4] = shvac.rangeAuto[0];
    payload[5] = shvac.rangeAuto[1];

    payload[5] = 0; // ?

    payload[7] = shvac.rangeDehum[0];
    payload[8] = shvac.rangeDehum[1];

    payload[9] = 0;  // ?
    payload[10] = 0; // ?

    sendResponse(BusproOp::Touch::AC_TEMPERATURE.readResp(), frame.srcAddress, payload, sizeof(payload));
}
void TouchModule::handleModifyAcTemperature(const BusproFrame &frame)
{
    if (frame.payloadLen != 11)
        return;

    uint8_t payload[12] = {BusproOp::SUCCESS};

    HVACPanel::State shvac = hvac_.get(selectedHvac);

    payload[1] = shvac.rangeCool[0];
    payload[2] = shvac.rangeCool[1];

    payload[3] = shvac.rangeHeat[0];
    payload[4] = shvac.rangeHeat[1];

    payload[5] = shvac.rangeAuto[0];
    payload[6] = shvac.rangeAuto[1];

    payload[8] = shvac.rangeDehum[0];
    payload[9] = shvac.rangeDehum[1];

    sendResponse(BusproOp::Touch::AC_TEMPERATURE.writeResp(), frame.srcAddress, payload, sizeof(payload));
}

void TouchModule::handleReadHvacTempSensor(const BusproFrame &frame)
{
    if (frame.payloadLen != 0)
        return;

    uint8_t payload[32] = {};

    sendResponse(BusproOp::Touch::AC_TEMPSENSOR.readResp(), frame.srcAddress, payload, sizeof(payload));
}
void TouchModule::handleModifyHvacTempSensor(const BusproFrame &frame)
{
    if (frame.payloadLen != 33)
        return;

    uint8_t payload[1] = {BusproOp::SUCCESS};

    sendResponse(BusproOp::Touch::AC_TEMPSENSOR.writeResp(), frame.srcAddress, payload, sizeof(payload));
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
        packetNumber
    };

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
            uint32_t address =
                flash_.findAdrress(
                    MemoryAdress::Touch::SECTOR_IMAGES,
                    MemoryAdress::Touch::pageImage(
                        imageNumber,
                        compactPacket));

            flash_.read(address, flashData, sizeof(flashData));
        }
        else if (configMode == 1)
        {
            // 4 HVAC images per page
            uint8_t hvacIndex =
                imageNumber * 4 + (packetNumber / 15);

            // Packet inside the selected 256-byte image
            uint8_t imagePacket =
                packetNumber % 15;

            // Safety check
            if (hvacIndex >= 8)
                return;

            uint32_t address =
                MemoryAdress::Touch::HVAC_IMAGE[hvacIndex] +
                (imagePacket * 16);

            flash_.read(
                flash_.findAdrress(
                    MemoryAdress::Touch::SECTOR_ICONS,
                    address),
                flashData,
                sizeof(flashData));
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
//// new way of read ////
// void TouchModule::handleReadImage(const BusproFrame &frame)
// {
//     if (frame.payloadLen != 2)
//         return;

//     uint8_t imageNumber = frame.payload[0];
//     uint8_t packetNumber = frame.payload[1];

//     uint8_t payload[22] = {imageNumber, packetNumber};

//     uint8_t flashData[16];

//     flash_.read(flash_.findAdrress(MemoryAdress::Touch::SECTOR_IMAGES, MemoryAdress::Touch::pageImage(imageNumber, packetNumber)), flashData, sizeof(flashData));

//     // Padding
//     payload[2] = 0xFF;

//     // First 8 bytes
//     memcpy(payload + 3, flashData, 8);

//     // Padding
//     payload[11] = 0xFF;
//     payload[12] = 0xFF;

//     // Second 8 bytes
//     memcpy(payload + 13, flashData + 8, 8);

//     // Padding
//     payload[21] = 0xFF;

//     sendResponse(BusproOp::Touch::IMAGE_READING.resp(), frame.srcAddress, payload, sizeof(payload));
// }
//// old way of modify ////
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
            // 4 HVAC images per page
            uint8_t hvacIndex = imageNumber * 4 + (packetNumber / 15);

            // Packet inside the selected 256-byte image
            uint8_t imagePacket = packetNumber % 15;

            // Safety check
            if (hvacIndex >= 8)
                return;

            uint32_t address = MemoryAdress::Touch::HVAC_IMAGE[hvacIndex] + (imagePacket * 16);

            flash_.update(flash_.findAdrress(MemoryAdress::Touch::SECTOR_ICONS, address), flashData, sizeof(flashData));
        }
    }

    uint8_t payload[3] = {BusproOp::SUCCESS, imageNumber, packetNumber};

    sendResponse(BusproOp::Touch::IMAGE_MODIFY.resp(), frame.srcAddress, payload, sizeof(payload));
}
//// new way of modify ////
// void TouchModule::handleModifyImage(const BusproFrame &frame)
// {
//     if (frame.payloadLen != 22)
//         return;

//     uint8_t imageNumber = frame.payload[0];
//     uint8_t packetNumber = frame.payload[1];

//     uint8_t flashData[16];

//     // First 8 useful bytes
//     memcpy(flashData, frame.payload + 3, 8);

//     // Second 8 useful bytes
//     memcpy(flashData + 8, frame.payload + 13, 8);

//     if (configMode == 0)
//     {
//         flash_.update(flash_.findAdrress(MemoryAdress::Touch::SECTOR_IMAGES, MemoryAdress::Touch::pageImage(imageNumber, packetNumber)), flashData, sizeof(flashData));
//     }
//     else if (configMode == 1)
//     {
//         // 4 HVAC images per page
//         uint8_t hvacIndex = imageNumber * 4 + (packetNumber / 16);

//         // Packet inside the selected 256-byte image
//         uint8_t imagePacket = packetNumber % 16;

//         // Safety check
//         if (hvacIndex >= 8 || packetNumber >= 64)
//             return;

//         uint32_t address = MemoryAdress::Touch::HVAC_IMAGE[hvacIndex] + (imagePacket * 16);

//         flash_.update(flash_.findAdrress(MemoryAdress::Touch::SECTOR_ICONS, address), flashData, sizeof(flashData));
//     }

//     uint8_t payload[3] = {BusproOp::SUCCESS, imageNumber, packetNumber};

//     sendResponse(BusproOp::Touch::IMAGE_MODIFY.resp(), frame.srcAddress, payload, sizeof(payload));
// }

// void TouchModule::readImage()
// {
//     uint8_t imageBuffer[960];

//     uint8_t page = display_.getPage();

//     if (page >= 0 && page <= 3)
//     {
//         for (uint8_t y = 0; y < 60; y++)
//         {
//             flash_.read(flash_.findAdrress(MemoryAdress::Touch::SECTOR_IMAGES, MemoryAdress::Touch::pageImage(page, y)), imageBuffer + (y * 16), 16);
//         }

//         display_.drawImage(imageBuffer);
//     }
// }

bool TouchModule::isChunkEmpty(const uint8_t *buffer, uint16_t size)
{
    for (uint16_t i = 0; i < size; i++)
    {
        if (buffer[i] != 0x00)
            return false;
    }

    return true;
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

    uint8_t payload[20] = {0x00};

    flash_.read(flash_.findAdrress(MemoryAdress::Touch::SECTOR_INFO, MemoryAdress::DEVICE_SOFTWARE_VER), payload, sizeof(payload));

    sendResponse(BusproOp::DEVICE_FIRMWARE.resp(), frame.srcAddress, payload, sizeof(payload));
}

void TouchModule::handleReadHardware(const BusproFrame &frame)
{
    if (frame.payloadLen != 0)
        return;

    uint8_t payload[30] = {};

    flash_.read(flash_.findAdrress(MemoryAdress::Touch::SECTOR_INFO, MemoryAdress::DEVICE_HARDWARE_VER), payload, sizeof(payload));

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
    buzzer_.countBeeps(duration);
#endif
#ifdef HAS_OLED_DISPLAY
    display_.finditAnimation(duration);
#endif
}

void TouchModule::handleSearchDevice(const BusproFrame &frame)
{
    if (frame.payloadLen != 4 || frame.payloadLen != 2)
        return;

    uint8_t payload[30 + frame.payloadLen] = {0x00};

    payload[0] = frame.payload[0];
    payload[1] = frame.payload[1];

    if (frame.payloadLen == 4)
    {
        payload[2] = frame.payload[2];
        payload[3] = frame.payload[3];
    }

    flash_.read(flash_.findAdrress(MemoryAdress::Touch::SECTOR_INFO, MemoryAdress::DEVICE_REMARK), payload + frame.payloadLen, 20);

    sendResponse(BusproOp::DEVICE_SEARCH_HDL.resp(), frame.srcAddress, payload, sizeof(payload));
}

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

    flash_.update(flash_.findAdrress(MemoryAdress::Touch::SECTOR_INFO, MemoryAdress::DEVICE_ADDRESS), address, 2);
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
        flash_.read(flash_.findAdrress(MemoryAdress::Touch::SECTOR_INFO, MemoryAdress::DEVICE_REMARK), payload, sizeof(payload));
    }
    else
    {
        snprintf(reinterpret_cast<char *>(payload), sizeof(payload), "CONF=%d", configMode);
    }

    sendResponse(BusproOp::DEVICE_REMARK.readResp(), 0xFFFF, payload, sizeof(payload));
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
        flash_.update(flash_.findAdrress(MemoryAdress::Touch::SECTOR_INFO, MemoryAdress::DEVICE_REMARK), frame.payload, frame.payloadLen);
    }

    uint8_t payload[1] = {BusproOp::SUCCESS};
    sendResponse(BusproOp::DEVICE_REMARK.writeResp(), frame.srcAddress, payload, sizeof(payload));
}

/////////////////////////////// UNIVERSAL REQUEST ///////////////////////////////
/////////////////////////////////////////////////////////////////////////////////