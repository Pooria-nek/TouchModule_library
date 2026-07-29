#pragma once

#include <Arduino.h>
#include "BusproFrame.h"
#include "BusproTransport.h"
#include "BusproDevice.h"
#include "MemoryCore.h"

#include "helpers.h"

#include "TouchController.h"

#define TOUCH4

// under progress
#ifdef TOUCH1
constexpr uint8_t TOUCH_CHANNEL_COUNT = 1; //
constexpr uint16_t TOUCH_TYPE = 5500;      //
#elifdef TOUCH2
constexpr uint8_t TOUCH_CHANNEL_COUNT = 2; //
constexpr uint16_t TOUCH_TYPE = 5501;      //
#elifdef TOUCH3
constexpr uint8_t TOUCH_CHANNEL_COUNT = 3; //
constexpr uint16_t TOUCH_TYPE = 467;       //
#elifdef TOUCH4
constexpr uint8_t TOUCH_CHANNEL_COUNT = 4; //
constexpr uint16_t TOUCH_TYPE = 1;         //
#elifdef TOUCH6
constexpr uint8_t TOUCH_CHANNEL_COUNT = 6; //
constexpr uint16_t TOUCH_TYPE = 426;       //
#elifdef TOUCH8
constexpr uint8_t TOUCH_CHANNEL_COUNT = 8; //
constexpr uint16_t TOUCH_TYPE = 463;       //
#elifdef TOUCH12
constexpr uint8_t TOUCH_CHANNEL_COUNT = 12; //
constexpr uint16_t TOUCH_TYPE = 464;        //
#elifdef TOUCH16
constexpr uint8_t TOUCH_CHANNEL_COUNT = 16; //
constexpr uint16_t TOUCH_TYPE = 466;        //
#elifdef TOUCH24
constexpr uint8_t TOUCH_CHANNEL_COUNT = 24; //
constexpr uint16_t TOUCH_TYPE = 432;        //
#endif

// constexpr uint8_t CURTAIN_CHANNEL_COUNT = (TOUCH_CHANNEL_COUNT / 2);
// constexpr uint8_t MAX_SCENE_ENTRIES = TOUCH_CHANNEL_COUNT * 2; // tune to taste / available RAM

class TouchModule
{
public:
    TouchModule(
        BusproTransport &bus,
        MemoryCore &flash,
        uint32_t sectorAddress,
        const uint8_t touchPins[TOUCH_CHANNEL_COUNT],
        bool activeHigh = true);

    bool begin();

    bool firstime();

    bool init();

    uint8_t maxZone();
    bool syncValues();

    void process(const BusproFrame &frame);

    // // Call frequently from loop(); non-blocking.
    // void poll();

    // --- Direct touch control (also usable outside of bus commands) ---
    bool setTouch(uint8_t channel /*0-3*/, bool on);
    bool getTouch(uint8_t channel) const;
    void setAllTouchs(const bool states[TOUCH_CHANNEL_COUNT]);

    // // --- Scene table management (normally driven by a config tool, but
    // //     exposed directly too in case you want to seed scenes in code) ---
    // bool defineScene(uint8_t area, uint8_t scene, const bool states[TOUCH_CHANNEL_COUNT]);
    // bool removeScene(uint8_t area, uint8_t scene);

    void sendResponse(uint16_t opcode, uint16_t dst, const uint8_t *payload, uint8_t payloadLen);

    MemoryCore &flash() { return flash_; }

    uint32_t memoryAddress() const { return memoryaddress_; }

private:
    void applyTouchHardware(uint8_t channel);
    // void readMcuUID();

    uint32_t memoryaddress_; // its the refrens address of data on memoryflash

    BusproTransport &bus_;
    MemoryCore &flash_;

    uint16_t deviceAddress_ = 0x0120;
    uint8_t fuid_[12];
    uint8_t uid_[12];
    uint16_t devType_ = TOUCH_TYPE; // gang device type per HDL spec

    uint8_t sceneCount = 0;
    uint8_t sceneActive = 0;
    uint8_t touchPins_[TOUCH_CHANNEL_COUNT];
    bool touchState_[TOUCH_CHANNEL_COUNT] = {false};
    bool activeHigh_;

    bool touchEnable_[TOUCH_CHANNEL_COUNT] = {false};
    uint8_t touchDelay_[TOUCH_CHANNEL_COUNT] = {0};
    uint8_t touchProtect_[TOUCH_CHANNEL_COUNT] = {0};

    TouchController controllerFunction;

    /////////////////////////////// BASIC INFORMATION ///////////////////////////////

    /////////////////////////////// ZONE SETTING ///////////////////////////////

    /////////////////////////////// SCENE SETTING ///////////////////////////////

    /////////////////////////////// CURTAIN ///////////////////////////////

    void handleReadZone(const BusproFrame &frame);
    void handleModifyZone(const BusproFrame &frame);
    void handleReadZoneRemark(const BusproFrame &frame);
    void handleModifyZoneRemark(const BusproFrame &frame);
    void handleReadSceneRemark(const BusproFrame &frame);
    void handleModifySceneRemark(const BusproFrame &frame);

    // Device
    // void handleSearchRequest(const BusproFrame &frame);
    // void handleModifyDeviceRemark(const BusproFrame &frame);

    // void handleReadMacaddress(const BusproFrame &frame);
    // void handleModifyMacaddress(const BusproFrame &frame);

    // Channel configuration
    void handleReadChannelRemark(const BusproFrame &frame);
    void handleModifyChannelRemark(const BusproFrame &frame);

    void handleReadChannelEnable(const BusproFrame &frame);
    void handleModifyChannelEnable(const BusproFrame &frame);

    void handleReadChannelOndelay(const BusproFrame &frame);
    void handleModifyChannelOndelay(const BusproFrame &frame);

    void handleReadChannelOnprotect(const BusproFrame &frame);
    void handleModifyChannelOnprotect(const BusproFrame &frame);

    void handleSceneRead(const BusproFrame &frame);
    void handleSceneModify(const BusproFrame &frame);

    void handleSceneResumeENRead(const BusproFrame &frame);
    void handleSceneResumeENModify(const BusproFrame &frame);

    void handleSceneResumeNumRead(const BusproFrame &frame);
    void handleSceneResumeNumModify(const BusproFrame &frame);

    void handleCurtainRead(const BusproFrame &frame);
    void handleCurtainModify(const BusproFrame &frame);

    // Status
    void handleReadStatusRequest(const BusproFrame &frame);

    /////////////////////////////// UNIVERSAL REQUEST ///////////////////////////////

    void handleReadFirmware(const BusproFrame &frame);
    void handleReadHardware(const BusproFrame &frame);
    void handleFindDevice(const BusproFrame &frame);

    void handleSearchDevice(const BusproFrame &frame);

    void handleReadMacaddress(const BusproFrame &frame);
    void handleModifyMacaddress(const BusproFrame &frame);

    void handleReadDeviceRemark(const BusproFrame &frame);
    void handleModifyDeviceRemark(const BusproFrame &frame);
};
