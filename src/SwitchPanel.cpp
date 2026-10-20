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

    return static_cast<uint8_t>(keytype_[key]);
}

// 9 byte return
void SwitchPanel::getKeyFunction(uint8_t *payload, uint8_t button, uint8_t function)
{
    const uint32_t address = KeyStorageLayout::functionAddress( button, function);

    if (address == UINT32_MAX)
        return;

    // Read the complete 7-byte function record
    flash_.read(address, payload, 7);
}

void SwitchPanel::setKeyFunction(const uint8_t *payload, uint8_t button, uint8_t function)
{
    const uint32_t address = KeyStorageLayout::functionAddress( button, function);

    if (address == UINT32_MAX)
        return;

    // Read the complete 7-byte function record
    flash_.update(address, payload, 7);
}

uint8_t SwitchPanel::getOperationLen(uint8_t type)
{
    switch (static_cast<OperationType>(type))
    {
    case OperationType::Scene:
        return 2;

    // param 1 -> Zone no
    // param 2 -> Sequence
    case OperationType::Sequence:
        return 2;

    // param 1 -> Switch no
    // param 2 -> Switch Status
    case OperationType::TimerSwitch:
        return 2;

    // param 1 -> Switch no
    // param 2 -> Switch Status
    case OperationType::UniversalSwitch:
        return 2;

    // param 1 -> Channel no
    // param 2 -> Intensity
    // param 3 -> Running time
    // param 4 -> ---
    case OperationType::SingleChannelControl:
        return 4;

    // param 1 -> Curtain no
    // param 2 -> Switch Status
    case OperationType::CurtainSwitch:
        return 2;

    // param 1 -> Message
    // param 2 -> no
    case OperationType::GPRSControl:
        return 2;

    // param 1 -> Function
    // param 2 -> par1
    // param 3 -> par2
    // param 4 -> ---
    case OperationType::PanelControl:
        return 3;

    // param 1 -> All Zone
    // param 2 -> Scene no
    case OperationType::BroadcastScene:
        return 2;

    // param 1 -> All Channel
    // param 2 -> Channel no
    // param 3 -> Running time
    case OperationType::BroadcastChannel:
        return 3;

    // param 1 -> Zone no
    // param 2 -> Mode
    case OperationType::SecurityModule:
        return 2;

    // param 1 -> par1
    // param 2 -> par2
    // param 3 -> par3
    case OperationType::MusicControl:
        return 3;

    // param 1 -> par1
    // param 2 -> par2
    case OperationType::UniversalControl:
        return 2;

    // param 1 -> par1
    // param 2 -> par2
    // param 3 -> par3
    case OperationType::InfraredControl:
        return 3;

    // param 1 -> Logic Light no
    // param 2 -> Intensity
    // param 3 -> color no
    // param 4 -> Duration[s]
    case OperationType::LogicLightAdjust:
        return 4;

    case OperationType::Invalid:
    default:
        return 0;
    }
}

uint16_t SwitchPanel::getOperationCode(uint8_t type)
{
    switch (static_cast<OperationType>(type))
    {
    case OperationType::Scene:
        return BusproOp::SCENE_CONTROL.req();

    case OperationType::Sequence:
        return UINT16_MAX;

    case OperationType::TimerSwitch:
        return UINT16_MAX;

    case OperationType::UniversalSwitch:
        return UINT16_MAX;

    case OperationType::SingleChannelControl:
        return BusproOp::CONTROL_SINGLE.req();

    case OperationType::CurtainSwitch:
        return UINT16_MAX;

    case OperationType::GPRSControl:
        return UINT16_MAX;

    case OperationType::PanelControl:
        return BusproOp::Touch::PANEL_CONTROL.req();

    case OperationType::BroadcastScene:
        return UINT16_MAX;

    case OperationType::BroadcastChannel:
        return UINT16_MAX;

    case OperationType::SecurityModule:
        return UINT16_MAX;

    case OperationType::MusicControl:
        return UINT16_MAX;

    case OperationType::UniversalControl:
        return UINT16_MAX;

    case OperationType::InfraredControl:
        return UINT16_MAX;

    case OperationType::LogicLightAdjust:
        return UINT16_MAX;

    default:
        return UINT16_MAX;
    }
}

uint8_t SwitchPanel::makeFunction(FunctionPacket &packet, uint8_t type, uint8_t state, const uint8_t *function)
{
    uint8_t len = getOperationLen(function[0]);
    uint8_t keytype = getKeyType(type);

    packet.dstAddress = (static_cast<uint16_t>(function[1]) << 8) | function[2];
    packet.oprCode = getOperationCode(function[0]);

    switch (static_cast<OperationType>(function[0]))
    {
    case OperationType::Scene:
        packet.data[0] = function[3];
        packet.data[1] = function[4];
        break;

    case OperationType::Sequence:
    case OperationType::TimerSwitch:
    case OperationType::UniversalSwitch:
        len = UINT8_MAX;
        break;

    case OperationType::SingleChannelControl:
        packet.data[0] = function[3];
        packet.data[1] = state ? 0x64 : 0x00;
        packet.data[2] = 0;
        packet.data[3] = 0;
        break;

    case OperationType::CurtainSwitch:
    case OperationType::GPRSControl:
    case OperationType::PanelControl:
    case OperationType::BroadcastScene:
    case OperationType::BroadcastChannel:
    case OperationType::SecurityModule:
    case OperationType::MusicControl:
    case OperationType::UniversalControl:
    case OperationType::InfraredControl:
    case OperationType::LogicLightAdjust:
        len = UINT8_MAX;
        break;

    default:
        break;
    }

    return len;
}