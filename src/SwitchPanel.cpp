#include "SwitchPanel.h"

SwitchPanel::SwitchPanel(MemoryCore &flash) : flash_(flash)
{
}

const uint8_t *SwitchPanel::keyType() const
{
    return reinterpret_cast<const uint8_t *>(keytype_);
}

void SwitchPanel::keyType(uint8_t *payload)
{
    memcpy(payload, keytype_, 16);
}

void SwitchPanel::keyTypeFetch()
{
    flash_.read(flash_.findAdrress(MemoryAdress::Touch::SECTOR_INFO, MemoryAdress::Touch::CHANNEL_MODE), keytype_, 16);
}

void SwitchPanel::keyTypePull(const uint8_t *keytype)
{
    flash_.update(flash_.findAdrress(MemoryAdress::Touch::SECTOR_INFO, MemoryAdress::Touch::CHANNEL_MODE), keytype, 16);

    memcpy(keytype_, keytype, 16);
}

void SwitchPanel::setKeyType(uint8_t key, SwitchType type)
{
    if (key >= 16)
        return;

    keytype_[key] = type;
}

// SwitchPanel::SwitchType SwitchPanel::getKeyType(uint8_t key)
// {
//     if (key >= 16)
//         return SwitchType::Invalid;
//     return keytype_[key];
// }

uint8_t SwitchPanel::getKeyType(uint8_t key)
{
    if (key > 16)
        return static_cast<uint8_t>(SwitchType::Invalid);

    return static_cast<uint8_t>(keytype_[key - 1]);
}

// 9 byte return
void SwitchPanel::getKeyFunction(uint8_t *payload, uint8_t button, uint8_t function)
{
    payload[0] = button;
    payload[1] = function;

    flash_.read(flash_.findAdrress(MemoryAdress::Touch::SECTOR_FUNCTIONS, MemoryAdress::Touch::channelFunction(button, function)), payload + 2, 7);
}

void SwitchPanel::setKeyFunction(uint8_t *payload, uint8_t button, uint8_t function)
{
    flash_.update(flash_.findAdrress(MemoryAdress::Touch::SECTOR_FUNCTIONS, MemoryAdress::Touch::channelFunction(button, function)), payload + 2, 7);

    getKeyFunction(payload, button, function);
}

uint16_t SwitchPanel::getOperationCode(uint8_t type)
{
    switch (static_cast<OperationType>(type))
    {
    case OperationType::Scene:
        return BusproOp::SCENE_CONTROL.req();

    // case OperationType::Sequence:
    //     return 0x0056;

    // case OperationType::TimerSwitch:
    //     return 0x0057;

    // case OperationType::UniversalSwitch:
    //     return 0x0058;

    case OperationType::SingleChannelControl:
        return 0x0059;

    // case OperationType::CurtainSwitch:
    //     return 0x0060;

    // case OperationType::GPRSControl:
    //     return 0x0061;

    case OperationType::PanelControl:
        return 0x0062;

    // case OperationType::BroadcastScene:
    //     return 0x0063;

    // case OperationType::BroadcastChannel:
    //     return 0x0064;

    // case OperationType::SecurityModule:
    //     return 0x0065;

    // case OperationType::MusicControl:
    //     return 0x0067;

    // case OperationType::UniversalControl:
    //     return 0x0068;

    // case OperationType::InfraredControl:
    //     return 0x0069;

    // case OperationType::LogicLightAdjust:
    //     return 0x0070;

    default:
        return 0x0000;
    }
}

uint8_t SwitchPanel::getOperationLen(uint8_t type)
{
    switch (static_cast<OperationType>(type))
    {
        case OperationType::Scene:
            return 2;

        // // param 1 -> Zone no 
        // // param 2 -> Sequence
        // case OperationType::Sequence:
        //     return 2;

        // // param 1 -> Switch no 
        // // param 2 -> Switch Status
        // case OperationType::TimerSwitch:
        //     return 2;

        // // param 1 -> Switch no 
        // // param 2 -> Switch Status
        // case OperationType::UniversalSwitch:
        //     return 2;

        // param 1 -> Channel no 
        // param 2 -> Intensity
        // param 3 -> Running time 
        // param 4 -> ---
        case OperationType::SingleChannelControl:
            return 4;

        // // param 1 -> Curtain no 
        // // param 2 -> Switch Status
        // case OperationType::CurtainSwitch:
        //     return 2;

        // // param 1 -> Message 
        // // param 2 -> no
        // case OperationType::GPRSControl:
        //     return 2;

        // param 1 -> Function 
        // param 2 -> par1
        // param 3 -> par2 
        // param 4 -> ---
        case OperationType::PanelControl:
            return 3;

        // // param 1 -> All Zone 
        // // param 2 -> Scene no
        // case OperationType::BroadcastScene:
        //     return 2;

        // // param 1 -> All Channel 
        // // param 2 -> Channel no
        // // param 3 -> Running time
        // case OperationType::BroadcastChannel:
        //     return 3;

        // // param 1 -> Zone no 
        // // param 2 -> Mode
        // case OperationType::SecurityModule:
        //     return 2;

        // // param 1 -> par1 
        // // param 2 -> par2
        // // param 3 -> par3
        // case OperationType::MusicControl:
        //     return 3;

        // // param 1 -> par1 
        // // param 2 -> par2
        // case OperationType::UniversalControl:
        //     return 2;

        // // param 1 -> par1 
        // // param 2 -> par2
        // // param 3 -> par3
        // case OperationType::InfraredControl:
        //     return 3;

        // // param 1 -> Logic Light no 
        // // param 2 -> Intensity
        // // param 3 -> color no 
        // // param 4 -> Duration[s]
        // case OperationType::LogicLightAdjust:
        //     return 4;

        case OperationType::Invalid:
        default:
            return 0;
    }
}

bool SwitchPanel::runSingle(uint8_t state, uint8_t button)
{
    uint8_t payload[7];

    flash_.read(flash_.findAdrress(MemoryAdress::Touch::SECTOR_FUNCTIONS, MemoryAdress::Touch::channelFunction(button, 1)), payload, sizeof(payload));

    auto operation = static_cast<OperationType>(payload[0]);
    uint16_t destination = (static_cast<uint16_t>(payload[1]) << 8) | payload[2];
    uint16_t oprationCode;

    switch (operation)
    {
    case OperationType::Scene:
    {
        oprationCode = BusproOp::SCENE_CONTROL.req();
        uint8_t respond[2] = {payload[3], payload[4]};
        // sendConfirm(oprationCode, destination, respond, sizeof(respond));
        return true;
    }

    // case OperationType::Sequence:
    // {
    //     oprationCode = BusproOp::SEQUENCE_CONTROL.req();
    //     return true;
    // }

    // case OperationType::TimerSwitch:
    // {
    //     return true;
    // }

    // case OperationType::UniversalSwitch:
    // {
    //     return true;
    // }

    case OperationType::SingleChannelControl:
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
        // else if (state == 2)
        // {
        //     value = (payload[4] == 0x64) ? 0x00 : 0x64;

        //     payload[4] = value;
        //     flash_.update(flash_.findAdrress(MemoryAdress::Touch::SECTOR_FUNCTIONS, MemoryAdress::Touch::channelFunction(button, 1)), payload, sizeof(payload));
        // }

        uint8_t respond[4] = {payload[3], value, payload[5], payload[6]};

        // sendConfirm(oprationCode, destination, respond, sizeof(respond));
        return true;
    }

    // case OperationType::CurtainSwitch:
    // {
    //     return true;
    // }

    // case OperationType::GPRSControl:
    // {
    //     return true;
    // }

    case OperationType::PanelControl:
    {
        oprationCode = BusproOp::Touch::PANEL_CONTROL.req();
        return true;
    }

    // case OperationType::BroadcastScene:
    // {
    //     oprationCode = BusproOp::SCENE_CONTROL.req();
    //     const uint8_t respond[4] = {};

    //     // sendConfirm(oprationCode, 0xFFFF, respond, sizeof(respond));
    //     return true;
    // }

    // case OperationType::BroadcastChannel:
    // {
    //     oprationCode = BusproOp::CONTROL_SINGLE.req();
    //     const uint8_t respond[4] = {};

    //     // sendConfirm(oprationCode, 0xFFFF, respond, sizeof(respond));
    //     return true;
    // }

    // case OperationType::SecurityModule:
    // {
    //     return true;
    // }

    // case OperationType::MusicControl:
    // {
    //     return true;
    // }

    // case OperationType::UniversalControl:
    // {
    //     return true;
    // }

    // case OperationType::InfraredControl:
    // {
    //     return true;
    // }

    // case OperationType::LogicLightAdjust:
    // {
    //     return true;
    // }

    default:
        return false;
    }
}

bool SwitchPanel::runCombination(uint8_t state, uint8_t button)
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
            static_cast<OperationType>(payload[0]);

        const uint16_t destination =
            (static_cast<uint16_t>(payload[1]) << 8) | payload[2];

        switch (operation)
        {
        case OperationType::Scene:
        {
            const uint8_t respond[2] = {
                payload[3],
                payload[4]};

            // sendConfirm(
            //     BusproOp::SCENE_CONTROL.req(),
            //     destination,
            //     respond,
            //     sizeof(respond));

            sent = true;
            break;
        }

        case OperationType::SingleChannelControl:
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

            // sendConfirm(BusproOp::CONTROL_SINGLE.req(), destination, respond, sizeof(respond));

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

bool SwitchPanel::runMomentary(bool press, uint8_t button)
{
    uint8_t payload[7];

    // Pressed -> function 1
    // Released -> function 50
    // const uint8_t function = press ? 1 : 50;

    flash_.read(flash_.findAdrress(MemoryAdress::Touch::SECTOR_FUNCTIONS, MemoryAdress::Touch::channelFunction(button, 1)), payload, sizeof(payload));

    auto operation = static_cast<OperationType>(payload[0]);
    uint16_t destination = (static_cast<uint16_t>(payload[1]) << 8) | payload[2];
    uint16_t oprationCode;

    switch (operation)
    {
    case OperationType::Scene:
    {
        oprationCode = BusproOp::SCENE_CONTROL.req();
        uint8_t respond[2] = {payload[3], payload[4]};
        // sendConfirm(oprationCode, destination, respond, sizeof(respond));
        return true;
    }

    case OperationType::SingleChannelControl:
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
        // sendConfirm(oprationCode, destination, respond, sizeof(respond));
        return true;
    }

    default:
        return false;
    }

    return true;
}

bool SwitchPanel::runDblclick(bool lefty, bool combination, uint8_t button)
{
    if (lefty)
    {
    }
    else
    {
    }
    return true;
}

/*
 *
 */
bool SwitchPanel::runSeprateHold(bool lefty, bool combination, uint8_t button)
{
    if (lefty)
    {
    }
    else
    {
    }
    return true;
}

//////////////////////////////////////////////////////////////////////////////////////////

uint16_t SwitchPanel::runOperationSingleChannel(uint8_t *response, uint8_t function, uint8_t type)
{
    uint8_t value;

    switch (type)
    {
    case 0:
        value = 0x00;
        break;

    case 1:
        value = 0x64;
        break;

    case 2:
        value = (response[4] == 0x64) ? 0x00 : 0x64;

        response[4] = value;

        flash_.update(
            flash_.findAdrress(
                MemoryAdress::Touch::SECTOR_FUNCTIONS,
                function),
            response,
            7);
        break;

    default:
        return 0;
    }

    const uint8_t output[4] = {response[3], value, response[5], response[6]};

    memcpy(response, output, 4);

    return BusproOp::CONTROL_SINGLE.req();
}

uint16_t runOperationScene(uint8_t *response)
{
    const uint8_t output[4] = {response[3], response[4]};

    memcpy(response, output, 4);

    return BusproOp::SCENE_CONTROL.req();
}