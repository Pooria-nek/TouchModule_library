#pragma once

#include <stdint.h>
#include "BusproFrame.h"
#include "BusproTransport.h"
#include "BusproDevice.h"
#include "MemoryCore.h"

class SwitchPanel
{
public:
    enum class SwitchType : uint8_t
    {
        // merged | tap
        SingleON = 0x02,    // 1 opration just turn on
        SingleOFF = 0x03,   // 1 opration just turn off
        SingleONOFF = 0x01, // 1 opration do both on/off

        // merged | tap
        CombinationON = 0x04,    // 99 opration just turn on
        CombinationOFF = 0x05,   // 99 opration just turn off
        CombinationONOFF = 0x07, // 99 opration do both on/off

        // merged | tap/release
        // Momentary = 0x06, // 1 opration turn on for tap | 1 opration turn off for release

        // seprate | tap
        DblclickSingle = 0x0A,   // 1 opration on tap | 49 opration double on tap
        DblclickCombined = 0x0B, // 49 opration on tap | 49 opration double on tap

        // seprate | hold
        ShortLongPress = 0x11, // 49 opration on press | 49 opration on hold trig (seprate left/right)
        ShortLongJog = 0x10,   // 49 opration on press | 49 opration on jog trig (seprate left/right)

        Invalid = 0x00 // anything else it invalid
    };

    enum class OperationType : uint8_t
    {
        Scene = 0x55, // param 1 -> Zone no | param 2 -> Scene no | param 3 -> --- | param 4 -> ---
        // Sequence = 0x56,             // param 1 -> Zone no | param 2 -> Sequence | param 3 -> --- | param 4 -> ---
        // TimerSwitch = 0x57,          // param 1 -> Switch no | param 2 -> Switch Statue | param 3 -> --- | param 4 -> ---
        // UniversalSwitch = 0x58,      // param 1 -> Switch no | param 2 -> Switch Statue | param 3 -> --- | param 4 -> ---
        SingleChannelControl = 0x59, // param 1 -> Channel no | param 2 -> Intensity | param 3 -> Running time | param 4 -> ---
        // CurtainSwitch = 0x60,        // param 1 -> Curtain no | param 2 -> Switch Status | param 3 -> --- | param 4 -> ---
        // GPRSControl = 0x61,          // param 1 -> Message | param 2 -> no | param 3 -> --- | param 4 -> ---
        PanelControl = 0x62, // param 1 -> Function | param 2 -> par1 | param 3 -> par2 | param 4 -> ---
        // BroadcastScene = 0x63,       // param 1 -> All Zone | param 2 -> Scene no | param 3 -> --- | param 4 -> ---
        // BroadcastChannel = 0x64,     // param 1 -> All Channel | param 2 -> Channel no | param 3 -> Running time | param 4 -> ---
        // SecurityModule = 0x65,       // param 1 -> Zone no | param 2 -> Mode | param 3 -> --- | param 4 -> ---
        // MusicControl = 0x67,         // param 1 -> par1 | param 2 -> par2 | param 3 -> par3 | param 4 -> ---
        // UniversalControl = 0x68,     // param 1 -> par1 | param 2 -> par2 | param 3 -> --- | param 4 -> ---
        // InfraredControl = 0x69,      // param 1 -> par1 | param 2 -> par2 | param 3 -> par3 | param 4 -> ---
        // LogicLightAdjust = 0x70,     // param 1 -> Logic Light no | param 2 -> Intensity | param 3 -> color no | param 4 -> Duration[s]
        Invalid = 0x00 // anything else it invalid
    };

    SwitchPanel(MemoryCore &flash);

    // void keyType(uint8_t num);
    const uint8_t *keyType() const;
    void keyType(uint8_t *payload);
    void keyTypePull(const uint8_t *keytype);
    void keyTypeFetch();

    void getKeyFunction(uint8_t *payload, uint8_t button, uint8_t function);
    void setKeyFunction(uint8_t *payload, uint8_t button, uint8_t function);

    uint16_t getOperationCode(uint8_t type);
    uint8_t getOperationLen(uint8_t type);

    void setKeyType(uint8_t key, SwitchType type);
    // SwitchType getKeyType(uint8_t key);
    uint8_t getKeyType(uint8_t key);

    bool runSingle(uint8_t state, uint8_t button);
    bool runCombination(uint8_t state, uint8_t button);
    bool runMomentary(bool press, uint8_t button);
    bool runDblclick(bool lefty, bool combination, uint8_t button);
    bool runSeprateHold(bool lefty, bool combination, uint8_t button);

    // uint8_t runOperationScene(
    //     uint8_t state,
    //     uint8_t button,
    //     uint8_t *response);

    // uint8_t runOperationSequence(
    //     uint8_t state,
    //     uint8_t button,
    //     uint8_t *response);

    uint16_t runOperationSingleChannel(uint8_t *response, uint8_t function, uint8_t type);
    uint16_t runOperationScene(uint8_t *response);

    // uint8_t runOperationPanelControl(
    //     uint8_t state,
    //     uint8_t button,
    //     uint8_t *response);

private:
    MemoryCore &flash_;

    SwitchType keytype_[16] = {SwitchType::Invalid};
};
