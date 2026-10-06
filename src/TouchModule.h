#pragma once

#include <Arduino.h>
#include <Wire.h>
#include "BusproFrame.h"
#include "BusproTransport.h"
#include "BusproDevice.h"
#include "MemoryCore.h"

#include <stdint.h>

#include "helpers.h"
#include "LedHandler.h"
#include "DisplayHandler.h"
#include "BuzzerHandler.h"
#include "APDSHandler.h"
#include "HVACPanel.h"
#include "SwitchPanel.h"

#include "NTC.h"
#include "BS8112.h"

#define TOUCH_HOLD_TIME 1000     // ms hold time
#define TOUCH_HOLD_REPEAT 600    // ms between repeats after hold
#define TOUCH_DOUBLE_TAP_GAP 250 // max gap between taps

#define DLP_PANEL

#define HAS_BUZZER
#define HAS_APDS

// under progress
#ifdef TOUCH1
constexpr uint8_t TOUCH_PAD_COUNT = 1; //

// // key
// constexpr uint16_t TOUCH_TYPE = 216;  //
// constexpr uint16_t TOUCH_TYPE = 223;  //
// constexpr uint16_t TOUCH_TYPE = 245;  //
// constexpr uint16_t TOUCH_TYPE = 255;  //
// constexpr uint16_t TOUCH_TYPE = 258;  //
// constexpr uint16_t TOUCH_TYPE = 261;  //
// constexpr uint16_t TOUCH_TYPE = 270;  //
constexpr uint16_t TOUCH_TYPE = 275; //
// constexpr uint16_t TOUCH_TYPE = 288;  //
// constexpr uint16_t TOUCH_TYPE = 2029; //
// constexpr uint16_t TOUCH_TYPE = 2055; //
// constexpr uint16_t TOUCH_TYPE = 2062; //
// constexpr uint16_t TOUCH_TYPE = 3050; //
// constexpr uint16_t TOUCH_TYPE = 5034; //
// constexpr uint16_t TOUCH_TYPE = 5038; //
// constexpr uint16_t TOUCH_TYPE = 5046; //
// constexpr uint16_t TOUCH_TYPE = 5050; //

// // button
// constexpr uint16_t TOUCH_TYPE = 242;  //
// constexpr uint16_t TOUCH_TYPE = 2014; //
// constexpr uint16_t TOUCH_TYPE = 2019; //
// constexpr uint16_t TOUCH_TYPE = 2033; //
// constexpr uint16_t TOUCH_TYPE = 2040; //

#elifdef TOUCH2
constexpr uint8_t TOUCH_PAD_COUNT = 2; //
// key
constexpr uint16_t TOUCH_TYPE = 277; //

// // button
// constexpr uint16_t TOUCH_TYPE = 243;  //
// constexpr uint16_t TOUCH_TYPE = 2015; //
// constexpr uint16_t TOUCH_TYPE = 2020; //
// constexpr uint16_t TOUCH_TYPE = 2034; //
// constexpr uint16_t TOUCH_TYPE = 2041; //

#elifdef TOUCH3
constexpr uint8_t TOUCH_PAD_COUNT = 3; //
// key
constexpr uint16_t TOUCH_TYPE = 278; //

// button

#elifdef TOUCH4
constexpr uint8_t TOUCH_PAD_COUNT = 4; //
constexpr uint8_t LOGICAL_BUTTON_COUNT = 4;
// 4 key

// constexpr uint16_t TOUCH_TYPE = 219;  //
// constexpr uint16_t TOUCH_TYPE = 222;  //
// constexpr uint16_t TOUCH_TYPE = 226;  //
// constexpr uint16_t TOUCH_TYPE = 248;  //
// constexpr uint16_t TOUCH_TYPE = 252;  //
// constexpr uint16_t TOUCH_TYPE = 265;  //
// constexpr uint16_t TOUCH_TYPE = 273;  //
constexpr uint16_t TOUCH_TYPE = 279; //
// constexpr uint16_t TOUCH_TYPE = 282;  //
// constexpr uint16_t TOUCH_TYPE = 286;  //
// constexpr uint16_t TOUCH_TYPE = 289;  //
// constexpr uint16_t TOUCH_TYPE = 295;  //
// constexpr uint16_t TOUCH_TYPE = 298;  //
// constexpr uint16_t TOUCH_TYPE = 2002; //
// constexpr uint16_t TOUCH_TYPE = 2006; //
// constexpr uint16_t TOUCH_TYPE = 2011; //
// constexpr uint16_t TOUCH_TYPE = 2023; //
// constexpr uint16_t TOUCH_TYPE = 2026; //
// constexpr uint16_t TOUCH_TYPE = 2032; //
// constexpr uint16_t TOUCH_TYPE = 2045; //
// constexpr uint16_t TOUCH_TYPE = 2049; //
// constexpr uint16_t TOUCH_TYPE = 5037; //
// constexpr uint16_t TOUCH_TYPE = 2057; //
// constexpr uint16_t TOUCH_TYPE = 2060; //
// constexpr uint16_t TOUCH_TYPE = 2065; //
// constexpr uint16_t TOUCH_TYPE = 2067; //
// constexpr uint16_t TOUCH_TYPE = 3053; //
// constexpr uint16_t TOUCH_TYPE = 5049; //
// constexpr uint16_t TOUCH_TYPE = 5031; //
// constexpr uint16_t TOUCH_TYPE = 5054; //
// constexpr uint16_t TOUCH_TYPE = 5041; //
// constexpr uint16_t TOUCH_TYPE = 5043; //
// constexpr uint16_t TOUCH_TYPE = 5053; //
// constexpr uint16_t TOUCH_TYPE = 5055; //
// constexpr uint16_t TOUCH_TYPE = 5057; //
// constexpr uint16_t TOUCH_TYPE = 5059; //

// 4 button

// constexpr uint16_t TOUCH_TYPE = 244; //
// constexpr uint16_t TOUCH_TYPE = 2017; //
// constexpr uint16_t TOUCH_TYPE = 2021; //
// constexpr uint16_t TOUCH_TYPE = 2036; //
// constexpr uint16_t TOUCH_TYPE = 2043; //

#elifdef TOUCH5
constexpr uint8_t TOUCH_PAD_COUNT = 5; //
constexpr uint16_t TOUCH_TYPE = 280;   //

#elifdef TOUCH6
constexpr uint8_t TOUCH_PAD_COUNT = 6; //
constexpr uint8_t LOGICAL_BUTTON_COUNT = 6;
// key
constexpr uint16_t TOUCH_TYPE = 281; //
// button
#elifdef TOUCH8
constexpr uint8_t TOUCH_PAD_COUNT = 8; //
// key
constexpr uint16_t TOUCH_TYPE = 290; //
// button
#elifdef TOUCH10
constexpr uint8_t TOUCH_PAD_COUNT = 12; //
// key
constexpr uint16_t TOUCH_TYPE = 284; // wrong number
// button
#elifdef TOUCH12
constexpr uint8_t TOUCH_PAD_COUNT = 12; //
// key
constexpr uint16_t TOUCH_TYPE = 284; //
// button
#elifdef AC_PANEL
constexpr uint8_t TOUCH_PAD_COUNT = 24; //
// key

// button
#elifdef DLP_PANEL
constexpr uint8_t TOUCH_PAD_COUNT = 10; // pysical touch pad
// constexpr uint16_t TOUCH_TYPE = 84;         // no floorheat
// constexpr uint16_t TOUCH_TYPE = 86;         // no floorheat
// constexpr uint16_t TOUCH_TYPE = 87;         // no floorheat
// constexpr uint16_t TOUCH_TYPE = 149;        //
// constexpr uint16_t TOUCH_TYPE = 154;        //
// constexpr uint16_t TOUCH_TYPE = 155;        //
// constexpr uint16_t TOUCH_TYPE = 156;        //
// constexpr uint16_t TOUCH_TYPE = 157;        //
// constexpr uint16_t TOUCH_TYPE = 158;        //
constexpr uint16_t TOUCH_TYPE = 160; //
// constexpr uint16_t TOUCH_TYPE = 2058;       //

constexpr uint8_t TOUCH_BUTTON_COUNT = 8;
constexpr uint8_t PAGE_BUTTON_COUNT = 4;
constexpr uint8_t LOGICAL_BUTTON_COUNT = 16;

constexpr uint8_t PREVIOUS_PAGE_KEY = 8;
constexpr uint8_t NEXT_PAGE_KEY = 9;

#define HAS_OLED_DISPLAY
#endif

// OLED display pin definitions — only DLP_PANEL and AC_PANEL carry a display
#ifdef HAS_OLED_DISPLAY
#define OLED_DC_PIN PA11
#define OLED_CS_PIN PA15
#define OLED_RS_PIN PA12
#endif

#ifdef HAS_BUZZER
#define PIN_FB_BUZZER PB3
#endif

// constexpr uint8_t CURTAIN_CHANNEL_COUNT = (TOUCH_PAD_COUNT / 2);
// constexpr uint8_t MAX_SCENE_ENTRIES = TOUCH_PAD_COUNT * 2; // tune to taste / available RAM

class TouchModule
{
public:
    // --- Per-channel LED indicator state (see LedHandler.h) ---
    // Deactive : off               - device idle/asleep, low power indicator
    // Active   : on, steady        - ready, steady, no activity
    // Blink    : blink once        - quick acknowledge (e.g. touch registered)
    // Blinking : blink until told to stop - long-running action in progress, call
    //            finishOperation() once the underlying action actually completes
    using LedMode = LedHandler<TOUCH_PAD_COUNT>::LedMode;

    TouchModule(TwoWire &wirePort, BusproTransport &bus, MemoryCore &flash, const uint8_t touchPins[TOUCH_PAD_COUNT], const uint8_t touchPads[TOUCH_PAD_COUNT], bool activeHigh = true);

    NTC ntc1;
    NTC ntc2;

    bool begin();

    void update();

    void error(const char *text);

    void buttonUpdate();

    bool firstime();

    bool init();
    bool reMatch();

    bool syncValues();

    void process(const BusproFrame &frame);

    void sendResponse(uint16_t opcode, uint16_t dst, const uint8_t *payload, uint8_t payloadLen);
    bool sendConfirm(uint16_t opcode, uint16_t dst, const uint8_t *payload, uint8_t payloadLen);

    MemoryCore &flash() { return flash_; }

    uint32_t memoryAddress() const { return memoryaddress_; }

    void updateAPDS();

    uint8_t getPhysicalButton(uint8_t key);
    uint8_t getLogicalButton(uint8_t key);

    void irqTouchHandler(); // Should be called by external GPIO interrupt service routine

    bool getIrq();

    // void setKeyType(uint8_t key, ButtonType type);
    // ButtonType getKeyType(uint8_t key);

    void fetchValidPages();

    void fetchSleepValues();

    void fetchProxValues();

    // --- Device mode: Sleep (uniform dim glow, any touch wakes) vs Wake
    //     (each channel independently shows a "high" or "low" LED state) ---
    enum class DeviceMode : uint8_t
    {
        Sleep,
        Wake
    };

    void sleep(); // enter Sleep mode: every channel drops to one dim glow level
    void wake();  // enter Wake mode: each channel restores its stored high/low state
    DeviceMode getDeviceMode() const { return deviceMode_; }

    // Auto-sleep: if no touch/activity for this many ms while Wake, sleep()
    // is entered automatically (checked from update()). Default 30000 (30s).
    void setSleepTimeout(uint32_t ms) { sleepTimeoutMs_ = ms; }
    uint32_t getSleepTimeout() const { return sleepTimeoutMs_; }

    // Resets the idle timer without changing mode — call if some other
    // activity (besides a touch press or FINDIT) should also count.
    void markActivity() { lastActivityTime_ = millis(); }

    DisplayHandler getTouch() const { return display_; }

    BuzzerHandler getBuzzer() const { return buzzer_; }
#ifdef HAS_APDS
    APDSHandler getProx() const { return apds_; }
#endif
#ifdef DLP_PANEL
    HVACPanel &hvac() { return hvac_; }
    const HVACPanel &hvac() const { return hvac_; }
#endif
private:
    static constexpr const char *SOFTWARE_VERSION = "v0.30.0-beta";

    // void applyTouchHardware(uint8_t channel);
    // void readMcuUID();

    uint8_t configMode = 0;

    // bool runSingle(uint8_t state, uint8_t button);

    // bool runCombination(uint8_t state, uint8_t button);

    // bool runMomentary(bool press, uint8_t button);

    // bool runDblclick(bool lefty, bool combination, uint8_t button);

    // bool runSeprateHold(bool lefty, bool combination, uint8_t button);

    uint32_t memoryaddress_ = MemoryAdress::Touch::SECTOR_INFO; // its the refrens address of data on memoryflash

    BusproTransport &bus_;
    MemoryCore &flash_;

    uint16_t deviceAddress_ = 0x0140;
    uint8_t fuid_[12];
    uint8_t uid_[12];
    uint16_t devType_ = TOUCH_TYPE; // gang device type per HDL spec

    uint8_t sceneCount = 0;
    uint8_t sceneActive = 0;
    // bool touchState_[TOUCH_PAD_COUNT] = {false};
    bool activeHigh_;

    uint8_t ledState[LOGICAL_BUTTON_COUNT * 2] = {0};

    /////////////////////////////////////////////////////////////////////////

    // --- LED indicator (state machine + software PWM), see LedHandler.h ---
    LedHandler<TOUCH_PAD_COUNT> leds_;

    BS8112 touch_;

#ifdef HAS_OLED_DISPLAY
    // --- OLED Display (UI interface)
    DisplayHandler display_;
#endif

#ifdef HAS_BUZZER
    BuzzerHandler buzzer_;
#endif

#ifdef HAS_APDS
    APDSHandler apds_;

    // volatile bool irqProxFlag_;
#endif

#ifdef DLP_PANEL
    // hvac — single source of truth for all 8 HVAC zones (power, temps,
    // mode, fan, current-zone selection). See HVACPanel.h. Also owned by
    // reference inside DisplayHandler (via update()) so the OLED can render
    // whatever this holds without keeping its own copy.
    HVACPanel hvac_;

    // floorheat

    bool fheatPower[8] = {
        false, false, false, false};

    float fheatCurrentTemp[8] = {
        24.0, 24.0, 24.0, 24.0};

    float fheatSetTemp[8] = {
        24.0, 24.0, 24.0, 24.0};

    uint8_t fheatMode[8] = {
        0, 0, 0, 0};

    uint8_t currentFheat = 0;
#endif

    SwitchPanel switchPanel_;

    void updatePageLeds(uint8_t page);
    void ledRunner(uint8_t page, uint16_t key);

    struct FunctionRunnerState
    {
        bool active = false;
        bool functionExecuting = false;

        uint8_t page = 0;
        uint16_t key = 0;

        uint16_t logicalButton = 0;
        uint16_t physicalButton = 0;

        uint8_t ledChannel = 0;

        uint8_t functionIndex = 0;
        uint8_t functionEnd = 0;

        uint8_t functionsPerUpdate = 3;
    };
    FunctionRunnerState functionRunnerState_;

    void functionRunner(uint8_t page, uint16_t key);
    void updateFunctionRunner();

    // --- Device mode (Sleep/Wake) ---
    DeviceMode deviceMode_ = DeviceMode::Wake;
    bool keyHigh_[TOUCH_PAD_COUNT] = {false}; // per-channel high(on)/low(off) state, remembered across sleep

    uint8_t sleepLevel_ = 2;                     // low dim glow while asleep
    uint8_t wakeHighLevel_ = LED_PWM_LEVELS - 1; // "high" state brightness while awake
    uint8_t wakeLowLevel_ = 4;                   // "low" state brightness while awake

    uint32_t sleepTimeoutMs_ = 10000; // auto-sleep after this long with no activity
    uint32_t lastActivityTime_ = 0;   // millis() of the last touch press / FINDIT / markActivity()

    void checkAutoSleep(); // called from update(); enters Sleep once idle past sleepTimeoutMs_

    /////////////////////////////////////////////////////////////////////////////////////////////

    // void handleRead(const BusproFrame &frame);
    // void handleModify(const BusproFrame &frame);

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    ////////////////////////////////////////////////////////////////////////////////
    //////////////////////////////// DLP Functions /////////////////////////////////

    void handlePanelControl(const BusproFrame &frame);

    void handleRestore(const BusproFrame &frame);
    void handleRestoreUNK(const BusproFrame &frame);

    /////////////////////////////////// SETTINGS ////////////////////////////////////

    void handleReadIndensity(const BusproFrame &frame);
    void handleModifyIndensity(const BusproFrame &frame);

    void handleReadUivalues(const BusproFrame &frame);
    void handleModifyUivalues(const BusproFrame &frame);

    void handleReadEnablePage(const BusproFrame &frame);
    void handleModifyEnablePage(const BusproFrame &frame);

    void handleReadPOpration2(const BusproFrame &frame);
    void handleModifyPOpration2(const BusproFrame &frame);

    void handleReadPtempcalibr(const BusproFrame &frame);
    void handleModifyPtempcalibr(const BusproFrame &frame);

    void handleReadPtempFlag(const BusproFrame &frame);
    void handleModifyPtempFlag(const BusproFrame &frame);

    void handleReadPOpration5(const BusproFrame &frame);
    void handleModifyPOpration5(const BusproFrame &frame);

    void handleReadTimeDate(const BusproFrame &frame);
    void handleModifyTimeDate(const BusproFrame &frame);

    ///////////////////////////////// 1 TO 4 PAGE ///////////////////////////////////

    void handleReadTouchMode(const BusproFrame &frame);
    void handleModifyTouchMode(const BusproFrame &frame);

    void handleReadTouchStatue(const BusproFrame &frame);
    void handleModifyTouchStatue(const BusproFrame &frame);

    void handleReadTouchDimming(const BusproFrame &frame);
    void handleModifyTouchDimming(const BusproFrame &frame);

    void handleReadTouchDimmingValue(const BusproFrame &frame);
    void handleModifyTouchDimmingValue(const BusproFrame &frame);

    void handleModifyTouchFunction(const BusproFrame &frame);
    void handleReadTouchFunction(const BusproFrame &frame);

    void handleReadTouchRemark(const BusproFrame &frame);
    void handleModifyTouchRemark(const BusproFrame &frame);

    void readChannelRemarks();

    ////////////////////////////////////// AC ///////////////////////////////////////

    void handleReadAcInfo(const BusproFrame &frame);
    void handleModifyAcInfo(const BusproFrame &frame);

    void handleReadAcOpration(const BusproFrame &frame);
    void handleModifyAcOpration(const BusproFrame &frame);

    void handleReadACTemperature(const BusproFrame &frame);
    void handleModifyAcTemperature(const BusproFrame &frame);

    void handleReadHvacTempSensor(const BusproFrame &frame);
    void handleModifyHvacTempSensor(const BusproFrame &frame);

    void handleReadHvacEcomode(const BusproFrame &frame);
    void handleModifyHvacEcomode(const BusproFrame &frame);

    void handleReadHvacIRread(const BusproFrame &frame);
    void handleModifyHvacIRread(const BusproFrame &frame);

    void handleReadHvacIRcontrol(const BusproFrame &frame);
    void handleModifyHvacIRcontrol(const BusproFrame &frame);

    void handleReadHvacTest(const BusproFrame &frame);
    void handleModifyHvacTest(const BusproFrame &frame);

    ///////////////////////////////// FLOOR HEATING /////////////////////////////////

    void handleReadFHInfo(const BusproFrame &frame);
    void handleModifyFHInfo(const BusproFrame &frame);

    //////////////////////////////////// MUSIC //////////////////////////////////////

    void handleReadMusicSetting(const BusproFrame &frame);
    void handleModifyMusicSetting(const BusproFrame &frame);

    void handleReadMusicOpration(const BusproFrame &frame);
    void handleModifyMusicOpration(const BusproFrame &frame);

    void handleReadMusicComand(const BusproFrame &frame);
    void handleModifyMusicComand(const BusproFrame &frame);

    //////////////////////////////////// IMAGE //////////////////////////////////////

    void handleReadImage(const BusproFrame &frame);
    void handleModifyImage(const BusproFrame &frame);

    void readChunk(uint8_t imageNumber, uint8_t chunk, uint8_t *buffer);
    // void readImage(uint8_t imageNumber, uint8_t *buffer);
    void readImage();

    bool isChunkEmpty(const uint8_t *buffer, uint16_t size);

    //////////////////////////////// DLP Functions /////////////////////////////////
    ////////////////////////////////////////////////////////////////////////////////

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    // ////////////////////////////////////////////////////////////////////////////////
    // //////////////////////////////// Gang Functions ////////////////////////////////

    // //////////////////////////////// BUTTON SETTINGS ////////////////////////////////

    // void handleReadTouchMode(const BusproFrame &frame);
    // void handleModifyTouchMode(const BusproFrame &frame);

    // void handleReadTouchRemark(const BusproFrame &frame);
    // void handleModifyTouchRemark(const BusproFrame &frame);

    // void handleReadOpration1(const BusproFrame &frame);
    // void handleReadOpration2(const BusproFrame &frame);
    // void handleReadOpration3(const BusproFrame &frame);
    // void handleReadOpration4(const BusproFrame &frame);

    // /////////////////////////////// BASIC INFORMATION ///////////////////////////////

    // void handleReadIndensity(const BusproFrame &frame);
    // void handleModifyIndensity(const BusproFrame &frame);

    // void handleReadOpration6(const BusproFrame &frame);
    // void handleModifyOpration6(const BusproFrame &frame);

    // void handleReadOpration7(const BusproFrame &frame);
    // void handleModifyOpration7(const BusproFrame &frame);

    // //////////////////////////////// Gang Functions ////////////////////////////////
    // ////////////////////////////////////////////////////////////////////////////////

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