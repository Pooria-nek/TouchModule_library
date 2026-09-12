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
      display_(OLED_CS_PIN, OLED_DC_PIN, OLED_RS_PIN)
#endif
#ifdef HAS_BUZZER
      ,
      buzzer_(PIN_FB_BUZZER)
#endif
#ifdef HAS_APDS
      ,
      apds_(wirePort)
#endif
{
    // mcu::copyMcuUID(uid_);
    for (uint8_t i = 0; i < TOUCH_PAD_COUNT; i++)
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
    wake();

    // flash_.eraseSector(MemoryAdress::Touch::SECTOR_INFO);

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
    // buzzer_.startup();

    // Poll the BS8112 for new touch state (call every loop iteration)
    updateBS8112();

    buttonUpdate();

    // Non-blocking — auto-sleep after sleepTimeoutMs_ with no activity
    checkAutoSleep();

    // Non-blocking — advances LED blink timing / output (call every loop)
    leds_.updateLeds();

    buzzer_.poll();

    updateAPDS();

    display_.clear();

    // char payload[10];

    // const uint8_t page = display_.getPage();

    // for (uint8_t button = 1; button <= 4; button++)
    // {
    //     memset(payload, 0, sizeof(payload));

    //     const uint8_t globalButton = button + (page * 4);

    //     flash_.read(flash_.findAdrress(MemoryAdress::Touch::SECTOR_INFO, MemoryAdress::channelRemark(globalButton)), payload, 9);

    //     payload[9] = '\0';

    //     display_.drawCenteredTextH(7 + ((button - 1) * 30), payload);
    // }

    readImage();

    // display_.setBrightness(apds_.getAutoBrightness());

    display_.update(hvac_);
}

void TouchModule::setKeyType(uint8_t key, ButtonType type)
{
    if (key >= LOGICAL_BUTTON_COUNT)
        return;

    keytype_[key] = type;
}

TouchModule::ButtonType TouchModule::getKeyType(uint8_t key)
{
    if (key >= LOGICAL_BUTTON_COUNT)
        return ButtonType::Invalid;
    return keytype_[key - 1];
}

void TouchModule::fetchValidPages()
{
    uint8_t value[7] = {};

    flash_.read(flash_.findAdrress(MemoryAdress::Touch::SECTOR_INFO, MemoryAdress::Touch::PAGE_VALID_BASE), value, 7);

    display_.setValidPages(value);
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

void TouchModule::fetchKeyType()
{
    flash_.read(flash_.findAdrress(MemoryAdress::Touch::SECTOR_INFO, MemoryAdress::Touch::CHANNEL_MODE), keytype_, LOGICAL_BUTTON_COUNT);
}
void TouchModule::pullKeyType()
{
    flash_.update(flash_.findAdrress(MemoryAdress::Touch::SECTOR_INFO, MemoryAdress::Touch::CHANNEL_MODE), keytype_, LOGICAL_BUTTON_COUNT);
}

void TouchModule::buttonUpdate()
{
    const uint8_t page = display_.getPage();

#ifdef HAS_OLED_DISPLAY
    // if (k < 8)
    // {

    //     char buf[32];

    //     sprintf(buf, "%02d-%02d-%02d", logicalButton, physicalButton, static_cast<uint8_t>(type));

    //     display_.drawText(0, 16, buf);
    // }
    // else
    if (isPressed(8))
    {
        // buzzer_.click();
        display_.changePage(false);
        // continue;
    }
    else if (isPressed(9))
    {
        // buzzer_.click();
        display_.changePage(true);
        // continue;
    }
#endif

    switch (page)
    {
    case 0:
    case 1:
    case 2:
    case 3:
    {
        // Switch pages
    }
    break;

    case 4:
    {
        //------------------------------------ mode ----------------------------------
        if (isHold(1)
            // && AcChangeMode(curent_ac)
            ) //&& (evt_touch==2) )
        {
            // buzzer_call(50);
        }

        // ------------------------------------ power -----------------------------
        if (isPressed(2)
            // && AcTogglePower(curent_ac)
            ) // bnb
        {
            // buzzer_call(50);
        }

        // --------------------------------- set point ----------------------------
        if (isPressed(3)
            // && AcSetpointChange(curent_ac, -1)
        )
        {
            // buzzer_call(50);
        }
        if (isPressed(4)
            // && AcSetpointChange(curent_ac, 1)
        )
        {
            // buzzer_call(50);
        }

        //------------------------------- speed ---------------------------------------------
        if (isPressed(5)
            // && AcFanSpeed(curent_ac, -1)
            ) // decrise
        {
            // buzzer_call(50);
        }
        if (isPressed(6)
            // && AcFanSpeed(curent_ac, 1)
            ) // increse
        {
            // buzzer_call(50);
        }

        //------------------------------------ ac ----------------------------------------------
        if (isPressed(7)
            // && AcNavigate(-1)
            ) // change ac page
        {
            hvac_.previous();
            // buzzer_call(50);
        }
        if (isPressed(8)
            // && AcNavigate(1)
            ) // change ac page
        {
            hvac_.next();
            // buzzer_call(50);
        }
    }
    break;

        // case 5:
        // {
        //     //Music page
        // }
        // break;

    case 6:
    {
        //--------------------------------- power -------------------------
        if ((isPressed(1) || isPressed(2))
            // && toggle_floor_heat_power()
        )
        {
            // control_output(1, floor_heat_power_ddp);
            // control_output(2, floor_heat_power_ddp);
            // buzzer_call(50);
        }

        //------------------------------------- set point ----------------------------------------------
        if (isPressed(3)
            // && floor_heat_temp_mode_switch(false)
        )
        {
            // buzzer_call(50);
        }
        if (isPressed(4)
            // && floor_heat_temp_mode_switch(true)
        )
        {
            // buzzer_call(50);
        }

        //-------------------------change work mode -------------------------------------
        if (isPressed(7)) // change floorheat mode
        {
            // buzzer_call(50);
            // if (floor_heat_power_ddp)
            // {
            //     if (floor_heat_temp_mode > 1)
            //         floor_heat_temp_mode--;
            //     else
            //         floor_heat_temp_mode = 4;
            //     change_floor_heat_mode_switch();
            //     // showtemp_flag = true;
            // }
        }

        if (isPressed(8)) // change floorheat mode
        {
            // buzzer_call(50);
            // if (floor_heat_power_ddp)
            // {
            //     if (floor_heat_temp_mode < 4)
            //         floor_heat_temp_mode++;
            //     else
            //         floor_heat_temp_mode = 1;
            //     change_floor_heat_mode_switch();
            //     // showtemp_flag = true;
            // }
        }
    }
    break;
    }

    // // if (updateBS8112())
    // // {
    // for (uint8_t k = 0; k < TOUCH_PAD_COUNT; k++)
    // {
    //     const uint8_t physicalButton = getPhysicalButton(k);
    //     const uint8_t logicalButton = getLogicalButton(k);
    //     const ButtonType type = getKeyType(logicalButton);

    //     // Page navigation keys
    //     if (isPressed(k))
    //     {
    //         // buzzer_.click();

    //         // uint8_t payload[7];

    //         // flash_.read(flash_.findAdrress(MemoryAdress::Touch::SECTOR_FUNCTIONS, MemoryAdress::Touch::channelFunction(logicalButton, 1)), payload, 7);

    //         // sendResponse(0xBBBB, 0xAAAA, payload, sizeof(payload));

    //         switch (type)
    //         {
    //         case ButtonType::SingleON:
    //         {
    //             runSingle(1, logicalButton);
    //         }
    //         break;
    //         case ButtonType::SingleOFF:
    //         {
    //             runSingle(0, logicalButton);
    //         }
    //         break;
    //         case ButtonType::SingleONOFF:
    //         {
    //             runSingle(2, logicalButton);
    //         }
    //         break;
    //             ///////////
    //         case ButtonType::CombinationON:
    //         {
    //             runCombination(1, logicalButton);
    //         }
    //         break;
    //         case ButtonType::CombinationOFF:
    //         {
    //             runCombination(0, logicalButton);
    //         }
    //         break;
    //         case ButtonType::CombinationONOFF:
    //         {
    //             runCombination(2, logicalButton);
    //         }
    //         break;
    //             ///////////
    //         case ButtonType::Momentary:
    //         {
    //             runMomentary(true, logicalButton);
    //         }
    //         break;
    //         }
    //     }

    //     if (isReleased(k))
    //     {
    //         switch (type)
    //         {
    //         case ButtonType::Momentary:
    //         {
    //             runMomentary(false, logicalButton);
    //         }
    //         break;

    //         default:
    //             break;
    //         }
    //     }
    // }
}

// 0 -> off
// 1 -> on
// 2 -> toggle
bool TouchModule::runSingle(uint8_t state, uint8_t button)
{
    uint8_t payload[7];

    flash_.read(flash_.findAdrress(MemoryAdress::Touch::SECTOR_FUNCTIONS, MemoryAdress::Touch::channelFunction(button, 1)), payload, sizeof(payload));

    auto operation = static_cast<ButtonOperationType>(payload[0]);
    uint16_t destination = (static_cast<uint16_t>(payload[1]) << 8) | payload[2];
    uint16_t oprationCode;

    switch (operation)
    {
    case ButtonOperationType::Scene:
    {
        oprationCode = BusproOp::SCENE_CONTROL.req();
        uint8_t respond[2] = {payload[3], payload[4]};
        sendConfirm(oprationCode, destination, respond, sizeof(respond));
        return true;
    }

    case ButtonOperationType::Sequence:
    {
        oprationCode = BusproOp::SEQUENCE_CONTROL.req();
        return true;
    }

    case ButtonOperationType::TimerSwitch:
    {
        return true;
    }

    case ButtonOperationType::UniversalSwitch:
    {
        return true;
    }

    case ButtonOperationType::SingleChannelControl:
    {
        oprationCode = BusproOp::CONTROL_SINGLE.req();
        uint8_t value;

        if (state == 0)
        {
            value = 0x00;
        }
        else if (state == 1)
        {
            value = 0x64;
        }
        else if (state == 2)
        {
            value = (payload[4] == 0x64) ? 0x00 : 0x64;

            payload[4] = value;
            flash_.update(flash_.findAdrress(MemoryAdress::Touch::SECTOR_FUNCTIONS, MemoryAdress::Touch::channelFunction(button, 1)), payload, sizeof(payload));
        }

        uint8_t respond[4] = {payload[3], value, payload[5], payload[6]};
        sendConfirm(oprationCode, destination, respond, sizeof(respond));
        return true;
    }

    case ButtonOperationType::CurtainSwitch:
    {
        return true;
    }

    case ButtonOperationType::GPRSControl:
    {
        return true;
    }

    case ButtonOperationType::PanelControl:
    {
        oprationCode = BusproOp::Touch::PANEL_CONTROL.req();
        return true;
    }

    case ButtonOperationType::BroadcastScene:
    {
        oprationCode = BusproOp::SCENE_CONTROL.req();
        const uint8_t respond[4] = {};

        sendConfirm(BusproOp::CONTROL_SINGLE.req(), 0xFFFF, respond, sizeof(respond));
        return true;
    }

    case ButtonOperationType::BroadcastChannel:
    {
        oprationCode = BusproOp::CONTROL_SINGLE.req();
        const uint8_t respond[4] = {};

        sendConfirm(BusproOp::CONTROL_SINGLE.req(), 0xFFFF, respond, sizeof(respond));
        return true;
    }

    case ButtonOperationType::SecurityModule:
    {
        return true;
    }

    case ButtonOperationType::MusicControl:
    {
        return true;
    }

    case ButtonOperationType::UniversalControl:
    {
        return true;
    }

    case ButtonOperationType::InfraredControl:
    {
        return true;
    }

    case ButtonOperationType::LogicLightAdjust:
    {
        return true;
    }

    default:
        return false;
    }
}

bool TouchModule::runCombination(uint8_t state, uint8_t button)
{
    bool sent = false;

    for (uint8_t function = 1; function <= 50; function++)
    {
        uint8_t payload[7];

        const uint32_t address = flash_.findAdrress(
            MemoryAdress::Touch::SECTOR_FUNCTIONS,
            MemoryAdress::Touch::channelFunction(button, function));

        flash_.read(address, payload, sizeof(payload));

        // End of combination
        if (payload[0] == 0x00 || payload[0] == 0xFF)
            break;

        const auto operation =
            static_cast<ButtonOperationType>(payload[0]);

        const uint16_t destination =
            (static_cast<uint16_t>(payload[1]) << 8) | payload[2];

        switch (operation)
        {
        case ButtonOperationType::Scene:
        {
            const uint8_t respond[2] = {
                payload[3],
                payload[4]};

            sendConfirm(
                BusproOp::SCENE_CONTROL.req(),
                destination,
                respond,
                sizeof(respond));

            sent = true;
            break;
        }

        case ButtonOperationType::SingleChannelControl:
        {
            uint8_t value;

            if (state == 0)
            {
                value = 0x00;
            }
            else if (state == 1)
            {
                value = 0x64;
            }
            else if (state == 2)
            {
                // Toggle will be implemented later
                continue;
            }
            else
            {
                continue;
            }

            const uint8_t respond[4] = {payload[3], value, payload[5], payload[6]};

            sendConfirm(BusproOp::CONTROL_SINGLE.req(), destination, respond, sizeof(respond));

            sent = true;
            break;
        }

        default:
            break;
        }

        // delay(100);
    }

    return sent;
}

bool TouchModule::runMomentary(bool press, uint8_t button)
{
    uint8_t payload[7];

    // Pressed -> function 1
    // Released -> function 50
    // const uint8_t function = press ? 1 : 50;

    flash_.read(flash_.findAdrress(MemoryAdress::Touch::SECTOR_FUNCTIONS, MemoryAdress::Touch::channelFunction(button, 1)), payload, sizeof(payload));

    auto operation = static_cast<ButtonOperationType>(payload[0]);
    uint16_t destination = (static_cast<uint16_t>(payload[1]) << 8) | payload[2];
    uint16_t oprationCode;

    switch (operation)
    {
    case ButtonOperationType::Scene:
    {
        oprationCode = BusproOp::SCENE_CONTROL.req();
        uint8_t respond[2] = {payload[3], payload[4]};
        sendConfirm(oprationCode, destination, respond, sizeof(respond));
        return true;
    }

    case ButtonOperationType::SingleChannelControl:
    {
        uint8_t value;

        if (press)
        {
            value = 0x64;
        }
        else
        {
            value = 0x00;
        }

        oprationCode = BusproOp::CONTROL_SINGLE.req();
        uint8_t respond[4] = {payload[3], value, payload[5], payload[6]};
        sendConfirm(oprationCode, destination, respond, sizeof(respond));
        return true;
    }

    default:
        return false;
    }

    return true;
}

bool TouchModule::runDblclick(bool lefty, bool combination, uint8_t button)
{
    if (lefty)
    {
        /* code */
    }
    else
    {
        /* code */
    }
    return true;
}

/*
 *
 */
bool TouchModule::runSeprateHold(bool lefty, bool combination, uint8_t button)
{
    if (lefty)
    {
        /* code */
    }
    else
    {
        /* code */
    }
    return true;
}

bool TouchModule::firstime()
{
    uint8_t fuid_[12];
    flash_.read(flash_.findAdrress(MemoryAdress::Touch::SECTOR_INFO, MemoryAdress::DEVICE_MAC_ADDRESS), fuid_, 12);

    uint16_t fdevType;
    flash_.readObject(flash_.findAdrress(MemoryAdress::Touch::SECTOR_INFO, MemoryAdress::DEVICE_TYPE), fdevType);

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
    flash_.writeObject(flash_.findAdrress(MemoryAdress::Touch::SECTOR_INFO, MemoryAdress::DEVICE_ADDRESS), deviceAddress_);
    flash_.writeObject(flash_.findAdrress(MemoryAdress::Touch::SECTOR_INFO, MemoryAdress::DEVICE_TYPE), devType_);

    flash_.writeObject(flash_.findAdrress(MemoryAdress::Touch::SECTOR_INFO, MemoryAdress::DEVICE_MAC_ADDRESS), uid_);

    flash_.writeObject(flash_.findAdrress(MemoryAdress::Touch::SECTOR_INFO, MemoryAdress::DEVICE_REMARK), "DLP Touch panel");
    flash_.writeObject(flash_.findAdrress(MemoryAdress::Touch::SECTOR_INFO, MemoryAdress::DEVICE_HARDWARE_VER), "hardware");
    flash_.writeObject(flash_.findAdrress(MemoryAdress::Touch::SECTOR_INFO, MemoryAdress::DEVICE_SOFTWARE_VER), "software");

    char remark[20];
    for (size_t i = 1; i <= LOGICAL_BUTTON_COUNT; i++)
    {
        // uint8_t channel = i + 1;
        snprintf(remark, sizeof(remark), "Button %u", static_cast<unsigned>(i));
        flash_.writeObject(flash_.findAdrress(MemoryAdress::Touch::SECTOR_INFO, MemoryAdress::channelRemark(i)), remark);

        flash_.writeObject(flash_.findAdrress(MemoryAdress::Touch::SECTOR_INFO, MemoryAdress::channelAddress(MemoryAdress::Touch::CHANNEL_MODE, i)), ButtonType::SingleON);
        flash_.writeObject(flash_.findAdrress(MemoryAdress::Touch::SECTOR_INFO, MemoryAdress::channelAddress(MemoryAdress::Touch::CHANNEL_STATUE, i)), uint8_t{0x00});
        flash_.writeObject(flash_.findAdrress(MemoryAdress::Touch::SECTOR_INFO, MemoryAdress::channelAddress(MemoryAdress::Touch::CHANNEL_DIMMING, i)), uint8_t{0x00});
        flash_.writeObject(flash_.findAdrress(MemoryAdress::Touch::SECTOR_INFO, MemoryAdress::channelAddress(MemoryAdress::Touch::CHANNEL_DIMMING_VALUE, i)), uint8_t{0x00});
    }

    // for (size_t z = 0; z < LOGICAL_BUTTON_COUNT; z++)
    // {
    //     snprintf(remark, sizeof(remark), "Zone %u", static_cast<unsigned>(z));
    //     flash_.writeObject(flash_.findAdrress(MemoryAdress::Touch::SECTOR_INFO, MemoryAdress::zoneRemark(z)), remark);

    //     for (size_t s = 0; s < (TOUCH_PAD_COUNT * 2); s++)
    //     {
    //         /* code */
    //     }
    // }

    return true;
}

bool TouchModule::syncValues()
{
    flash_.readObject(flash_.findAdrress(MemoryAdress::Touch::SECTOR_INFO, MemoryAdress::DEVICE_ADDRESS), deviceAddress_);
    flash_.readObject(flash_.findAdrress(MemoryAdress::Touch::SECTOR_INFO, MemoryAdress::DEVICE_MAC_ADDRESS), uid_);

    fetchValidPages();

    // flash_.read(flash_.findAdrress(MemoryAdress::Touch::SECTOR_INFO, MemoryAdress::Touch::CHANNEL_MODE), payload, LOGICAL_BUTTON_COUNT);
    // flash_.read(flash_.findAdrress(MemoryAdress::Touch::SECTOR_INFO, MemoryAdress::Touch::CHANNEL_STATUE), payload, LOGICAL_BUTTON_COUNT);
    // flash_.read(flash_.findAdrress(MemoryAdress::Touch::SECTOR_INFO, MemoryAdress::Touch::CHANNEL_DIMMING), payload, LOGICAL_BUTTON_COUNT);
    // flash_.read(flash_.findAdrress(MemoryAdress::Touch::SECTOR_INFO, MemoryAdress::Touch::CHANNEL_DIMMING_VALUE), payload, LOGICAL_BUTTON_COUNT);

    return true;
}

// uint8_t TouchModule::maxZone()
// {
//     uint8_t payload[TOUCH_PAD_COUNT];

//     // flash_.read(flash_.findAdrress(MemoryAdress::Touch::SECTOR_INFO, MemoryAdress::RELAY_CHANNEL_ZONE), payload, TOUCH_PAD_COUNT);

//     for (uint8_t i = 0; i < TOUCH_PAD_COUNT; ++i)
//     {
//         if (payload[i] > sceneCount)
//         {
//             sceneCount = payload[i];
//         }
//     }

//     return sceneCount;
// }

void TouchModule::updateAPDS()
{
    apds_.update();

    uint16_t prox = apds_.getProximity();

    if (prox > 500)
    {
        if (deviceMode_ == DeviceMode::Sleep)
        {
            wake();
        }
        if (deviceMode_ == DeviceMode::Wake)
        {
            lastActivityTime_ = millis();
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

    fetchKeyType();
}

void TouchModule::irqTouchHandler()
{
    irqTouchFlag_ = true;
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

    // Remap only the configured physical pads into a compact bitfield,
    // where bit i of newState corresponds to TOUCH_PADS[i] (not the raw hardware bit position).
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

    for (uint8_t key = 0; key < TOUCH_PAD_COUNT; key++)
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
                buzzer_.click();
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

uint8_t TouchModule::getPhysicalButton(uint8_t key)
{
    if (key >= 8)
        return 0;

    return key + 1 + (display_.getPage() * 8);
}

uint8_t TouchModule::getLogicalButton(uint8_t key)
{
    if (key >= 8)
        return 0;

    return (key / 2) + 1 + (display_.getPage() * 4);
}

void TouchModule::sleep()
{
    deviceMode_ = DeviceMode::Sleep;
    leds_.setLedLevels(sleepLevel_, sleepLevel_, sleepLevel_);
    leds_.setAllLedMode(LedMode::Active); // uniform dim glow across every channel

    // display_.sleep = true;
}

void TouchModule::wake()
{
    deviceMode_ = DeviceMode::Wake;
    leds_.setLedLevels(wakeHighLevel_, wakeLowLevel_, wakeHighLevel_);
    for (uint8_t i = 0; i < TOUCH_PAD_COUNT; i++)
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
    if (channel >= TOUCH_PAD_COUNT)
        return;

    keyHigh_[channel] = high;

    if (deviceMode_ == DeviceMode::Wake)
        leds_.setLedMode(channel, high ? LedMode::Active : LedMode::Deactive);
    // If asleep, the new state is just remembered — wake() will apply it.
}

bool TouchModule::isKeyHigh(uint8_t channel) const
{
    if (channel >= TOUCH_PAD_COUNT)
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

        case BusproOp::Touch::OPRATION3.readReq():
            handleReadPOpration3(frame);
            break;
        case BusproOp::Touch::OPRATION3.writeReq():
            handleModifyPOpration3(frame);
            break;

        case BusproOp::Touch::OPRATION4.readReq():
            handleReadPOpration4(frame);
            break;
        case BusproOp::Touch::OPRATION4.writeReq():
            handleModifyPOpration4(frame);
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

            ///////////////////////////////// FLOOR HEATING /////////////////////////////////

            // case BusproOp::Touch::FH_TEMPERATURE.readReq():
            //     break;
            // case BusproOp::Touch::FH_TEMPERATURE.writeReq():
            //     break;

            // case BusproOp::Touch::FH_OPRATION2.readReq():
            //     break;
            // case BusproOp::Touch::FH_OPRATION2.writeReq():
            //     break;

            // case BusproOp::Touch::FH_OPRATION3.readReq():
            //     break;
            // case BusproOp::Touch::FH_OPRATION3.writeReq():
            //     break;

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

            /////////////////////////////////////////////////////////////////////////////////////////////

            // case BusproOp::TOUCH_CHANNEL_MODE.writeReq():
            //     handleModifyTouchMode(frame);
            //     break;

            // case BusproOp::TOUCH_CHANNEL_MODE.readReq():
            //     handleReadTouchMode(frame);
            //     break;

            // case BusproOp::TOUCH_CHANNEL_REMARK.writeReq():
            //     handleModifyTouchRemark(frame);
            //     break;

            // case BusproOp::TOUCH_CHANNEL_REMARK.readReq():
            //     handleReadTouchRemark(frame);
            //     break;

            // case BusproOp::TOUCH_OPRATION1.req():
            //     handleReadOpration1(frame);
            //     break;
            // case BusproOp::TOUCH_OPRATION2.req():
            //     handleReadOpration2(frame);
            //     break;
            // case BusproOp::TOUCH_OPRATION3.req():
            //     handleReadOpration3(frame);
            //     break;
            // case BusproOp::TOUCH_OPRATION4.req():
            //     handleReadOpration4(frame);
            //     break;

            // case BusproOp::TOUCH_INDENSITY.readReq():
            //     handleReadIndensity(frame);
            //     break;
            // case BusproOp::TOUCH_INDENSITY.writeReq():
            //     handleModifyIndensity(frame);
            //     break;

            // case BusproOp::TOUCH_OPRATION6.readReq():
            //     handleReadOpration6(frame);
            //     break;
            // case BusproOp::TOUCH_OPRATION6.writeReq():
            //     handleModifyOpration6(frame);
            //     break;

            // case BusproOp::TOUCH_OPRATION7.readReq():
            //     handleReadOpration7(frame);
            //     break;
            // case BusproOp::TOUCH_OPRATION7.writeReq():
            //     handleModifyOpration7(frame);
            //     break;
        }
    }
}

void TouchModule::sendResponse(uint16_t opcode, uint16_t dst, const uint8_t *payload, uint8_t payloadLen)
{
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

////////////////////////////////////////////////////////////////////////////////
/////////////////////////////// DEVICE HANDLERS ////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
//////////////////////////////// DLP Functions /////////////////////////////////

/////////////////////////////////// SETTINGS ////////////////////////////////////

void TouchModule::handleReadIndensity(const BusproFrame &frame)
{
    if (frame.payloadLen != 0)
        return;

    uint8_t payload[2] = {}; // Backlight LCD | Button LED

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

    // 1 - Recieve IR
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

    sendResponse(BusproOp::Touch::PAGE_ENABLE.readResp(), frame.srcAddress, payload, sizeof(payload));
}
void TouchModule::handleModifyEnablePage(const BusproFrame &frame)
{
    if (frame.payloadLen != 7)
        return;

    flash_.update(flash_.findAdrress(MemoryAdress::Touch::SECTOR_INFO, MemoryAdress::Touch::PAGE_VALID_BASE), frame.payload, 7);

    fetchValidPages();

    uint8_t payload[1] = {BusproOp::SUCCESS};

    sendResponse(BusproOp::Touch::PAGE_ENABLE.writeResp(), frame.srcAddress, payload, sizeof(payload));
}

void TouchModule::handleReadPOpration3(const BusproFrame &frame)
{
    if (frame.payloadLen != 0)
        return;

    uint8_t payload[9];

    sendResponse(BusproOp::Touch::OPRATION3.readResp(), frame.srcAddress, payload, sizeof(payload));
}
void TouchModule::handleModifyPOpration3(const BusproFrame &frame)
{
    if (frame.payloadLen != 4)
        return;

    // 1 - brodcast on/off
    // 2 - subnet id
    // 3 - device id
    // 4 - (maybe type) always 20

    uint8_t payload[1] = {BusproOp::SUCCESS};

    sendResponse(BusproOp::Touch::OPRATION3.writeResp(), frame.srcAddress, payload, sizeof(payload));
}

void TouchModule::handleReadPOpration4(const BusproFrame &frame)
{
    if (frame.payloadLen != 0)
        return;

    uint8_t payload[9];

    sendResponse(BusproOp::Touch::OPRATION4.readResp(), frame.srcAddress, payload, sizeof(payload));
}
void TouchModule::handleModifyPOpration4(const BusproFrame &frame)
{
    // if (frame.payloadLen != 0)
    //     return;

    uint8_t payload[1] = {BusproOp::SUCCESS};

    sendResponse(BusproOp::Touch::OPRATION4.writeResp(), frame.srcAddress, payload, sizeof(payload));
}

void TouchModule::handleReadPOpration5(const BusproFrame &frame)
{
    if (frame.payloadLen != 0)
        return;

    uint8_t payload[9];

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

    uint8_t payload[1] = {BusproOp::SUCCESS};

    sendResponse(BusproOp::Touch::SLEEPING.writeResp(), frame.srcAddress, payload, sizeof(payload));
}

void TouchModule::handleReadTimeDate(const BusproFrame &frame)
{
    if (frame.payloadLen != 0)
        return;

    uint8_t payload[9];

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

    flash_.read(flash_.findAdrress(MemoryAdress::Touch::SECTOR_INFO, MemoryAdress::Touch::CHANNEL_MODE), payload, LOGICAL_BUTTON_COUNT);

    sendResponse(BusproOp::Touch::CHANNEL_MODE.readResp(), frame.srcAddress, payload, sizeof(payload));
}
void TouchModule::handleModifyTouchMode(const BusproFrame &frame)
{
    if (frame.payloadLen != LOGICAL_BUTTON_COUNT)
        return;

    flash_.update(flash_.findAdrress(MemoryAdress::Touch::SECTOR_INFO, MemoryAdress::Touch::CHANNEL_MODE), frame.payload, LOGICAL_BUTTON_COUNT);

    fetchKeyType();

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

    uint8_t touchNum = frame.payload[0];
    uint8_t functionNum = frame.payload[1];

    uint8_t payload[9];
    payload[0] = touchNum;
    payload[1] = functionNum;

    flash_.read(flash_.findAdrress(MemoryAdress::Touch::SECTOR_FUNCTIONS, MemoryAdress::Touch::channelFunction(touchNum, functionNum)), payload + 2, 7);

    sendResponse(BusproOp::Touch::CHANNEL_FUNCTION.readResp(), frame.srcAddress, payload, sizeof(payload));
}
void TouchModule::handleModifyTouchFunction(const BusproFrame &frame)
{
    if (frame.payloadLen != 9)
        return;

    uint8_t touchNum = frame.payload[0];
    uint8_t functionNum = frame.payload[1];

    flash_.update(flash_.findAdrress(MemoryAdress::Touch::SECTOR_FUNCTIONS, MemoryAdress::Touch::channelFunction(touchNum, functionNum)), frame.payload + 2, 7);

    uint8_t payload[9];

    payload[0] = touchNum;
    payload[1] = functionNum;

    flash_.read(flash_.findAdrress(MemoryAdress::Touch::SECTOR_FUNCTIONS, MemoryAdress::Touch::channelFunction(touchNum, functionNum)), payload + 2, 7);

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
uint8_t hvacnumber;
void TouchModule::handleReadAcInfo(const BusproFrame &frame)
{
    if (frame.payloadLen != 0)
        return;

    uint8_t enable = 1;
    uint8_t subnetid;
    uint8_t deviceid;
    uint8_t adjust;
    //
    uint8_t type = 1; //
    uint8_t state;

    uint8_t payload[9] = {BusproOp::SUCCESS, enable, subnetid, deviceid, adjust, hvacnumber, type, 0, state};

    sendResponse(BusproOp::Touch::AC_INFORMATION.readResp(), frame.srcAddress, payload, sizeof(payload));
}
void TouchModule::handleModifyAcInfo(const BusproFrame &frame)
{
    if (frame.payloadLen != 8)
        return;

    uint8_t enable = frame.payload[0];
    uint8_t subnetid = frame.payload[1];
    uint8_t deviceid = frame.payload[2];
    uint8_t adjust = frame.payload[3];
    hvacnumber = frame.payload[4];   //
    uint8_t type = frame.payload[5]; //
    uint8_t state = frame.payload[7];

    uint8_t payload[1] = {BusproOp::SUCCESS};

    sendResponse(BusproOp::Touch::AC_INFORMATION.writeResp(), frame.srcAddress, payload, sizeof(payload));
}

void TouchModule::handleReadAcOpration(const BusproFrame &frame)
{
    if (frame.payloadLen != 0)
        return;

    uint8_t payload[11] = {};

    sendResponse(BusproOp::Touch::AC_OPRATION_MODEL.readResp(), frame.srcAddress, payload, sizeof(payload));
}
void TouchModule::handleModifyAcOpration(const BusproFrame &frame)
{
    if (frame.payloadLen != 11)
        return;

    uint8_t payload[12] = {BusproOp::SUCCESS};

    sendResponse(BusproOp::Touch::AC_OPRATION_MODEL.writeResp(), frame.srcAddress, payload, sizeof(payload));
}

void TouchModule::handleReadACTemperature(const BusproFrame &frame)
{
    if (frame.payloadLen != 0)
        return;

    uint8_t payload[11] = {0};

    sendResponse(BusproOp::Touch::AC_TEMPERATURE.readResp(), frame.srcAddress, payload, sizeof(payload));
}
void TouchModule::handleModifyAcTemperature(const BusproFrame &frame)
{
    if (frame.payloadLen != 11)
        return;

    uint8_t payload[12] = {BusproOp::SUCCESS};

    sendResponse(BusproOp::Touch::AC_TEMPERATURE.writeResp(), frame.srcAddress, payload, sizeof(payload));
}

///////////////////////////////// FLOOR HEATING /////////////////////////////////

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

void TouchModule::handleReadImage(const BusproFrame &frame)
{
    if (frame.payloadLen != 2)
        return;

    uint8_t imageNumber = frame.payload[0];
    uint8_t packetNumber = frame.payload[1];

    uint8_t payload[22] = {imageNumber, packetNumber};

    uint8_t flashData[16];

    if (packetNumber <= 60)
    {
        flash_.read(flash_.findAdrress(MemoryAdress::Touch::SECTOR_IMAGES, MemoryAdress::Touch::pageImage(imageNumber, packetNumber)), flashData, sizeof(flashData));
    }
    else
    {
        // Invalid request -> return empty/unused data
        memset(flashData, 0xFF, sizeof(flashData));
    }

    // Padding
    payload[2] = 0xFF;

    // First 8 bytes
    memcpy(payload + 3, flashData, 8);

    // Padding
    payload[11] = 0xFF;
    payload[12] = 0xFF;

    // Second 8 bytes
    memcpy(payload + 13, flashData + 8, 8);

    // Padding
    payload[21] = 0xFF;

    sendResponse(BusproOp::Touch::IMAGE_READING.resp(), frame.srcAddress, payload, sizeof(payload));
}

void TouchModule::handleModifyImage(const BusproFrame &frame)
{
    if (frame.payloadLen != 22)
        return;

    uint8_t imageNumber = frame.payload[0];
    uint8_t packetNumber = frame.payload[1];

    uint8_t flashData[16];

    // First 8 useful bytes
    memcpy(flashData, frame.payload + 3, 8);

    // Second 8 useful bytes
    memcpy(flashData + 8, frame.payload + 13, 8);

    flash_.update(flash_.findAdrress(MemoryAdress::Touch::SECTOR_IMAGES, MemoryAdress::Touch::pageImage(imageNumber, packetNumber)), flashData, sizeof(flashData));

    uint8_t payload[3] = {BusproOp::SUCCESS, imageNumber, packetNumber};

    sendResponse(BusproOp::Touch::IMAGE_MODIFY.resp(), frame.srcAddress, payload, sizeof(payload));
}

// void TouchModule::readChunk(
//     uint8_t imageNumber,
//     uint8_t chunk,
//     uint8_t *buffer)
// {
//     constexpr uint8_t chunkY[] = {0, 30, 60, 90, 120};
//     constexpr uint8_t chunkHeight[] = {30, 30, 30, 30, 8};

//     if (imageNumber >= 8 || chunk >= 5)
//         return;

//     const uint8_t height = chunkHeight[chunk];

//     for (uint8_t y = 0; y < height; y++)
//     {
//         const uint8_t sourceY = chunkY[chunk] + y;
//         +flash_.read(
//             flash_.findAdrress(
//                 MemoryAdress::Touch::SECTOR_IMAGES,
//                 MemoryAdress::Touch::pageImage(imageNumber, sourceY)),
//             buffer + (y * 8),
//             8);
//     }
// }

void TouchModule::readImage()
{
    uint8_t imageBuffer[960];

    uint8_t page = display_.getPage();

    if (page >= 0 && page <= 3)
    {
        for (uint8_t y = 0; y < 60; y++)
        {
            flash_.read(flash_.findAdrress(MemoryAdress::Touch::SECTOR_IMAGES, MemoryAdress::Touch::pageImage(page, y)), imageBuffer + (y * 16), 16);
        }

        display_.drawImage(imageBuffer);
    }
}

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

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// ////////////////////////////////////////////////////////////////////////////////
// //////////////////////////////// Gang Functions ////////////////////////////////

// //////////////////////////////// BUTTON SETTINGS ////////////////////////////////

// void TouchModule::handleReadTouchMode(const BusproFrame &frame)
// {
//     if (frame.payloadLen != 0)
//         return;

//     uint8_t payload[TOUCH_PAD_COUNT] = {0x00};

//     sendResponse(BusproOp::TOUCH_CHANNEL_MODE.readResp(), frame.srcAddress, payload, sizeof(payload));
// }

// void TouchModule::handleModifyTouchMode(const BusproFrame &frame)
// {
//     if (frame.payloadLen != 1)
//         return;

//     uint8_t payload[1] = {BusproOp::SUCCESS};

//     sendResponse(BusproOp::TOUCH_CHANNEL_MODE.writeResp(), frame.srcAddress, payload, sizeof(payload));
// }

// void TouchModule::handleReadTouchRemark(const BusproFrame &frame)
// {
//     if (frame.payloadLen != 1)
//         return;

//     uint8_t touchNum = frame.payload[0];

//     uint8_t payload[21] = {touchNum};

//     sendResponse(BusproOp::TOUCH_CHANNEL_REMARK.readResp(), frame.srcAddress, payload, sizeof(payload));
// }

// void TouchModule::handleModifyTouchRemark(const BusproFrame &frame)
// {
//     if (frame.payloadLen != 0)
//         return;

//     uint8_t payload[1] = {BusproOp::SUCCESS};

//     sendResponse(BusproOp::TOUCH_CHANNEL_REMARK.writeResp(), frame.srcAddress, payload, sizeof(payload));
// }

// void TouchModule::handleReadOpration1(const BusproFrame &frame)
// {
//     if (frame.payloadLen != 0)
//         return;

//     uint8_t payload[TOUCH_PAD_COUNT] = {0x01};

//     sendResponse(BusproOp::TOUCH_OPRATION1.resp(), frame.srcAddress, payload, sizeof(payload));
// }
// void TouchModule::handleReadOpration2(const BusproFrame &frame)
// {
//     if (frame.payloadLen != 0)
//         return;

//     uint8_t payload[TOUCH_PAD_COUNT] = {0x02};

//     sendResponse(BusproOp::TOUCH_OPRATION2.resp(), frame.srcAddress, payload, sizeof(payload));
// }
// void TouchModule::handleReadOpration3(const BusproFrame &frame)
// {
//     if (frame.payloadLen != 0)
//         return;

//     uint8_t payload[TOUCH_PAD_COUNT] = {0x03};

//     sendResponse(BusproOp::TOUCH_OPRATION3.resp(), frame.srcAddress, payload, sizeof(payload));
// }

// void TouchModule::handleReadOpration4(const BusproFrame &frame)
// {
//     if (frame.payloadLen != 0)
//         return;

//     uint8_t payload[1] = {BusproOp::SUCCESS};

//     sendResponse(BusproOp::TOUCH_OPRATION4.resp(), frame.srcAddress, payload, sizeof(payload));
// }

// /////////////////////////////// BASIC INFORMATION ///////////////////////////////

// /* it usses in following devices:
//  *
//  * DLP -> Settings -> Indicator indensity
//  * Key switches -> basic information -> Indicator indensity
//  */
// void TouchModule::handleReadIndensity(const BusproFrame &frame)
// {
//     if (frame.payloadLen != 0)
//         return;

//     uint8_t payload[2] = {0x00, 0x00}; // Backlight | Status

//     sendResponse(BusproOp::TOUCH_INDENSITY.readResp(), frame.srcAddress, payload, sizeof(payload));
// }

// /* it usses in following devices:
//  *
//  * DLP -> Settings -> Indicator indensity
//  * Key switches -> basic information -> Indicator indensity
//  */
// void TouchModule::handleModifyIndensity(const BusproFrame &frame)
// {
//     if (frame.payloadLen != 3)
//         return;

//     uint8_t payload[1] = {BusproOp::SUCCESS};

//     sendResponse(BusproOp::TOUCH_INDENSITY.writeResp(), frame.srcAddress, payload, sizeof(payload));
// }

// void TouchModule::handleReadOpration6(const BusproFrame &frame)
// {
//     if (frame.payloadLen != 0)
//         return;

//     uint8_t payload[3] = {0x00, 0x00, 0x00}; //||

//     sendResponse(BusproOp::TOUCH_OPRATION6.readResp(), frame.srcAddress, payload, sizeof(payload));
// }
// void TouchModule::handleModifyOpration6(const BusproFrame &frame)
// {
//     if (frame.payloadLen != 8)
//         return;

//     uint8_t payload[1] = {BusproOp::SUCCESS};

//     sendResponse(BusproOp::TOUCH_OPRATION6.writeResp(), frame.srcAddress, payload, sizeof(payload));
// }

// void TouchModule::handleReadOpration7(const BusproFrame &frame)
// {
//     if (frame.payloadLen != 0)
//         return;

//     uint8_t payload[1] = {0x00};

//     sendResponse(BusproOp::TOUCH_OPRATION7.readResp(), frame.srcAddress, payload, sizeof(payload));
// }
// void TouchModule::handleModifyOpration7(const BusproFrame &frame)
// {
//     if (frame.payloadLen != 4)
//         return;

//     uint8_t payload[1] = {BusproOp::SUCCESS};

//     sendResponse(BusproOp::TOUCH_OPRATION7.writeResp(), frame.srcAddress, payload, sizeof(payload));
// }

// //////////////////////////////// Gang Functions ////////////////////////////////
// ////////////////////////////////////////////////////////////////////////////////

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

    flash_.readObject(flash_.findAdrress(MemoryAdress::Touch::SECTOR_INFO, MemoryAdress::DEVICE_SOFTWARE_VER), payload);

    sendResponse(BusproOp::DEVICE_FIRMWARE.resp(), frame.srcAddress, payload, sizeof(payload));
}

void TouchModule::handleReadHardware(const BusproFrame &frame)
{
    if (frame.payloadLen != 0)
        return;

    uint8_t payload[32] = {0x00};

    flash_.readObject(flash_.findAdrress(MemoryAdress::Touch::SECTOR_INFO, MemoryAdress::DEVICE_HARDWARE_VER), payload);

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
#ifdef HAS_BUZZER
    buzzer_.click();
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

    flash_.read(flash_.findAdrress(MemoryAdress::Touch::SECTOR_INFO, MemoryAdress::DEVICE_REMARK), payload + 2, 20);

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

    flash_.updateObject(flash_.findAdrress(MemoryAdress::Touch::SECTOR_INFO, MemoryAdress::DEVICE_ADDRESS), address);
    syncValues();

    uint8_t payload[1] = {BusproOp::SUCCESS};
    sendResponse(BusproOp::DEVICE_MAC_ADDRESS.writeResp(), frame.srcAddress, payload, sizeof(payload));
}

void TouchModule::handleReadDeviceRemark(const BusproFrame &frame)
{
    if (frame.payloadLen != 0)
        return;

    uint8_t payload[20];

    if (configMode == 0)
    {
        flash_.readObject(flash_.findAdrress(MemoryAdress::Touch::SECTOR_INFO, MemoryAdress::DEVICE_REMARK), payload);
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

/////////////////////////////////////////////////////////////////////////////////
/////////////////////////////// UNIVERSAL REQUEST ///////////////////////////////
/////////////////////////////////////////////////////////////////////////////////