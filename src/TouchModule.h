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

#define TOUCH_HOLD_TIME 1000     // ms hold time
#define TOUCH_HOLD_REPEAT 600    // ms between repeats after hold
#define TOUCH_DOUBLE_TAP_GAP 250 // max gap between taps

#define DLP_PANEL

#define HAS_OLED_DISPLAY
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

    enum class ButtonType : uint8_t
    {
        // merged | tap
        SingleON = 0x02,    // 1 opration just turn on
        SingleOFF = 0x03,   // 1 opration just turn off
        SingleONOFF = 0x01, // 1 opration do both on/off

        // merged | tap
        CombinationON = 0x04,    // 49 opration just turn on
        CombinationOFF = 0x05,   // 49 opration just turn off
        CombinationONOFF = 0x07, // 49 opration do both on/off

        // merged | tap/release
        Momentary = 0x06, // 1 opration turn on for tap | 1 opration turn off for release

        // seprate | tap
        DblclickSingle = 0x10,   // 1 opration on tap | 49 opration double on tap
        DblclickCombined = 0x11, // 49 opration on tap | 49 opration double on tap

        // seprate | hold
        ShortLongPress = 0x17, // 49 opration on press | 49 opration on hold trig
        ShortLongJog = 0x16,   // 49 opration on press | 49 opration on jog trig (seprate left/right)

        Invalid = 0x00 // anything else it invalid
    };

    enum class ButtonOperationType : uint8_t
    {
        Scene = 0x55,                // param 1 -> Zone no | param 2 -> Scene no | param 3 -> --- | param 4 -> ---
        Sequence = 0x56,             // param 1 -> Zone no | param 2 -> Sequence | param 3 -> --- | param 4 -> ---
        TimerSwitch = 0x57,          // param 1 -> Switch no | param 2 -> Switch Statue | param 3 -> --- | param 4 -> ---
        UniversalSwitch = 0x58,      // param 1 -> Switch no | param 2 -> Switch Statue | param 3 -> --- | param 4 -> ---
        SingleChannelControl = 0x59, // param 1 -> Channel no | param 2 -> Intensity | param 3 -> Running time | param 4 -> ---
        CurtainSwitch = 0x60,        // param 1 -> Curtain no | param 2 -> Switch Status | param 3 -> --- | param 4 -> ---
        GPRSControl = 0x61,          // param 1 -> Message | param 2 -> no | param 3 -> --- | param 4 -> ---
        PanelControl = 0x62,         // param 1 -> Function | param 2 -> par1 | param 3 -> par2 | param 4 -> ---
        BroadcastScene = 0x63,       // param 1 -> All Zone | param 2 -> Scene no | param 3 -> --- | param 4 -> ---
        BroadcastChannel = 0x64,     // param 1 -> All Channel | param 2 -> Channel no | param 3 -> Running time | param 4 -> ---
        SecurityModule = 0x65,       // param 1 -> Zone no | param 2 -> Mode | param 3 -> --- | param 4 -> ---
        MusicControl = 0x67,         // param 1 -> par1 | param 2 -> par2 | param 3 -> par3 | param 4 -> ---
        UniversalControl = 0x68,     // param 1 -> par1 | param 2 -> par2 | param 3 -> --- | param 4 -> ---
        InfraredControl = 0x69,      // param 1 -> par1 | param 2 -> par2 | param 3 -> par3 | param 4 -> ---
        LogicLightAdjust = 0x70,     // param 1 -> Logic Light no | param 2 -> Intensity | param 3 -> color no | param 4 -> Duration[s]
        Invalid = 0x00               // anything else it invalid
    };

    TouchModule(TwoWire &wirePort, BusproTransport &bus, MemoryCore &flash, uint32_t sectorAddress, const uint8_t touchPins[TOUCH_PAD_COUNT], const uint8_t touchPads[TOUCH_PAD_COUNT], bool activeHigh = true);

    bool begin();

    void update();

    // uint8_t getLogicalButton(uint8_t key);
    void buttonUpdate();

    bool firstime();

    bool init();

    uint8_t maxZone();
    bool syncValues();
    bool syncProxValues();

    void process(const BusproFrame &frame);

    // // Call frequently from loop(); non-blocking.
    // void poll();

    // --- Direct touch control (also usable outside of bus commands) ---
    // bool setTouch(uint8_t channel /*0-3*/, bool on);
    // bool getTouch(uint8_t channel) const;
    // void setAllTouchs(const bool states[TOUCH_PAD_COUNT]);

    // // --- Scene table management (normally driven by a config tool, but
    // //     exposed directly too in case you want to seed scenes in code) ---
    // bool defineScene(uint8_t area, uint8_t scene, const bool states[TOUCH_PAD_COUNT]);
    // bool removeScene(uint8_t area, uint8_t scene);

    void sendResponse(uint16_t opcode, uint16_t dst, const uint8_t *payload, uint8_t payloadLen);
    bool sendConfirm(uint16_t opcode, uint16_t dst, const uint8_t *payload, uint8_t payloadLen);

    MemoryCore &flash() { return flash_; }

    uint32_t memoryAddress() const { return memoryaddress_; }

    void updateAPDS();

    void initBS8112();

    bool updateBS8112();
    uint8_t getPhysicalButton(uint8_t key);
    uint8_t getLogicalButton(uint8_t key);

    void irqTouchHandler(); // Should be called by external GPIO interrupt service routine

    void setKeyType(uint8_t key, ButtonType type);
    ButtonType getKeyType(uint8_t key);

    void fetchValidPages();

    void fetchProxValues();

    void fetchKeyType();
    void pullKeyType();

    // State query methods
    bool isPressed(uint8_t key);  // Returns true only on the initial press (edge)
    bool isReleased(uint8_t key); // Returns true only on the release (edge)
    bool isHold(uint8_t key);     // Returns true as long as the key is held down
    bool isHoldEdge(uint8_t key); // Returns true when held past TOUCH_HOLD_TIME
    // bool isDoubleTap(uint8_t key); // Returns true only on the two tap (edge)

    uint16_t getTouchState() const { return _touchState; }

    // // --- LED indicator control (forwards to the internal LedHandler) ---
    // void setLedMode(uint8_t channel, LedMode mode) { leds_.setLedMode(channel, mode); }
    // LedMode getLedMode(uint8_t channel) const { return leds_.getLedMode(channel); }
    // void setAllLedMode(LedMode mode) { leds_.setAllLedMode(mode); }

    // // Call once a Blinking action has actually finished; LED returns to Deactive.
    // void finishOperation(uint8_t channel) { leds_.finishOperation(channel); }

    // // Tunable brightness levels, range 0..LED_PWM_LEVELS-1 (0-31). blinkLevel is
    // // used for the "on" phase of Blink/Blinking. Values are clamped to range.
    // void setLedLevels(uint8_t activeLevel, uint8_t deactiveLevel, uint8_t blinkLevel = LED_PWM_LEVELS - 1)
    // {
    //     leds_.setLedLevels(activeLevel, deactiveLevel, blinkLevel);
    // }
    // void setLedBlinkTiming(uint16_t blinkMs, uint16_t blinkingPeriodMs)
    // {
    //     leds_.setLedBlinkTiming(blinkMs, blinkingPeriodMs);
    // }

    // // Call every loop(); non-blocking, drives blink timing for all channels.
    // void updateLeds() { leds_.updateLeds(); }

    // // Blocking — call once from setup(), before loop() takes over.
    // void startupAnimation() { leds_.startupAnimation(); }
    // void finditAnimation(uint8_t duration) { leds_.finditAnimation(duration); }
    // void sweep(uint8_t from, uint8_t to) { leds_.sweep(from, to); }

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

    // Sets a channel's logical Wake-mode state: high = lit ("on"), low = off/dim.
    // Remembered even while asleep, and applied immediately on the next wake().
    void setKeyHigh(uint8_t channel, bool high);
    bool isKeyHigh(uint8_t channel) const;

    // Tunable brightness levels (0..LED_PWM_LEVELS-1, 0-31) for Sleep/Wake.
    void setSleepLevel(uint8_t level);
    void setWakeLevels(uint8_t highLevel, uint8_t lowLevel);

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
    // void applyTouchHardware(uint8_t channel);
    // void readMcuUID();

    uint8_t configMode = 0;

    bool runSingle(uint8_t state, uint8_t button);

    bool runCombination(uint8_t state, uint8_t button);

    bool runMomentary(bool press, uint8_t button);

    bool runDblclick(bool lefty, bool combination, uint8_t button);

    bool runSeprateHold(bool lefty, bool combination, uint8_t button);

    uint32_t memoryaddress_; // its the refrens address of data on memoryflash

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

    bool touchEnable_[LOGICAL_BUTTON_COUNT] = {false};
    uint8_t touchDelay_[LOGICAL_BUTTON_COUNT] = {0};
    uint8_t touchProtect_[LOGICAL_BUTTON_COUNT] = {0};

    ButtonType keytype_[LOGICAL_BUTTON_COUNT] = {ButtonType::Invalid};

    // BS811x values
    TwoWire &wire_;
    volatile bool irqTouchFlag_;
    volatile bool _runAgain;

    uint8_t touch_address = 0x50; // i2c addresss of bs8112

    uint8_t touchPins_[TOUCH_PAD_COUNT];

    // State tracking
    uint16_t _touchState;     // Bitmask of currently active keys
    uint16_t _prevTouchState; // Bitmask of keys in previous update cycle
    uint16_t _pressedEdge;    // Bits set only during the press transition
    uint16_t _releasedEdge;   // Bits set only during the release transition

    // Hold/Timing logic
    uint32_t _lastPressTime[TOUCH_PAD_COUNT];
    uint16_t _holdActive;

    // Internal helper methods
    // uint16_t convert(uint16_t touchState); // Re-maps raw sensor bits to custom layout
    void writeRegister(uint8_t reg, uint8_t value);
    uint8_t readRegister(uint8_t reg);

    // --- LED indicator (state machine + software PWM), see LedHandler.h ---
    LedHandler<TOUCH_PAD_COUNT> leds_;

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

    /////////////////////////////////// SETTINGS ////////////////////////////////////

    void handleReadIndensity(const BusproFrame &frame);
    void handleModifyIndensity(const BusproFrame &frame);

    void handleReadUivalues(const BusproFrame &frame);
    void handleModifyUivalues(const BusproFrame &frame);

    void handleReadEnablePage(const BusproFrame &frame);
    void handleModifyEnablePage(const BusproFrame &frame);

    void handleReadPOpration2(const BusproFrame &frame);
    void handleModifyPOpration2(const BusproFrame &frame);

    void handleReadPOpration3(const BusproFrame &frame);
    void handleModifyPOpration3(const BusproFrame &frame);

    void handleReadPOpration4(const BusproFrame &frame);
    void handleModifyPOpration4(const BusproFrame &frame);

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

    ///////////////////////////////// FLOOR HEATING /////////////////////////////////

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