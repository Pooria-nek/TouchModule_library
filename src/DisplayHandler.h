#ifndef DISPLAYHANDLER_H
#define DISPLAYHANDLER_H

#include <Arduino.h>
#include <U8g2lib.h>
#include "HVACPanel.h"

#define OLDIE
// #define MODERN

// 16x16
static unsigned char power_icon[] = {
    0x00, 0x00, 0x80, 0x01, 0x80, 0x01, 0xb0, 0x0d, 0xb8, 0x1d, 0x9c, 0x39,
    0x8c, 0x31, 0x8c, 0x31, 0x0c, 0x30, 0x0c, 0x30, 0x1c, 0x38, 0x38, 0x1c,
    0xf0, 0x0f, 0xe0, 0x07, 0x00, 0x00, 0x00, 0x00};

// 16x16
static const uint8_t heat_icon[] U8X8_PROGMEM = {
    0x00, 0x01, 0x80, 0x00, 0x04, 0x21, 0x88, 0x10, 0x10, 0x08, 0xc0, 0x03,
    0xe0, 0x07, 0xe5, 0x57, 0xea, 0xa7, 0xe0, 0x07, 0xc0, 0x03, 0x10, 0x08,
    0x08, 0x11, 0x84, 0x20, 0x00, 0x01, 0x80, 0x00};

// 16x16
static unsigned char cool_icon[] = {
    0x00, 0x00, 0x80, 0x02, 0x00, 0x01, 0xa0, 0x0a, 0x30, 0x19, 0x40, 0x05,
    0x94, 0x53, 0xe8, 0x2f, 0x94, 0x53, 0x40, 0x05, 0x30, 0x19, 0xa0, 0x0a,
    0x00, 0x01, 0x80, 0x02, 0x00, 0x00, 0x00, 0x00};

// 13x13
static unsigned char temp_icon[] = {
    0xe0, 0xe0, 0x10, 0xe1, 0x10, 0xe1, 0x10, 0xe1, 0x50, 0xe1, 0x50, 0xe1,
    0x50, 0xe1, 0x50, 0xe1, 0x58, 0xe3, 0xe8, 0xe2, 0xe8, 0xe2, 0x18, 0xe3,
    0xf0, 0xe1};

// 13x13
static unsigned char house_icon[] = {
    0x00, 0xe0, 0x40, 0xe0, 0xe0, 0xe0, 0xf0, 0xe1, 0xf8, 0xe3, 0xfc, 0xe7,
    0x08, 0xe2, 0xa8, 0xe2, 0x08, 0xe2, 0x48, 0xe2, 0x48, 0xe2, 0x00, 0xe0,
    0x00, 0xe0};

// 13x13
static unsigned char internal_icon[] = {
    0x00, 0xe0, 0x10, 0xe0, 0x38, 0xe0, 0x7c, 0xe0, 0xfe, 0xe0, 0xff, 0xe1,
    0x82, 0xe0, 0x82, 0xe0, 0x92, 0xe0, 0xba, 0xe0, 0xba, 0xe0, 0x00, 0xe0,
    0x00, 0xe0};

// 13x13
static unsigned char external_icon[] = {
    0x00, 0xe0, 0x10, 0xe0, 0x38, 0xe0, 0x7c, 0xe0, 0xfe, 0xe0, 0xff, 0xe1,
    0x82, 0xe0, 0x82, 0xe0, 0x82, 0xe4, 0x82, 0xee, 0x82, 0xee, 0x00, 0xe0,
    0x00, 0xe0};

// 32x25
static unsigned char heater_icon[] = {
    0x00, 0x00, 0x00, 0xfc, 0xff, 0x1f, 0x00, 0x00, 0x20, 0xfc, 0xff, 0x4f,
    0x00, 0x00, 0x50, 0x00, 0x00, 0x50, 0xf0, 0xff, 0x4f, 0x08, 0x00, 0x20,
    0xe4, 0xff, 0x1f, 0x14, 0x00, 0x00, 0x14, 0x00, 0x00, 0xe4, 0xff, 0x1f,
    0x08, 0x00, 0x20, 0xf0, 0xff, 0x4f, 0x00, 0x00, 0x50, 0x00, 0x00, 0x50,
    0xf0, 0xff, 0x4f, 0x08, 0x00, 0x20, 0xe4, 0xff, 0x1f, 0x14, 0x00, 0x00,
    0x14, 0x00, 0x00, 0xe4, 0xff, 0x3f, 0x08, 0x00, 0x00, 0xf0, 0xff, 0x3f};

// 64x16
#define ZELLER_LOGO_WIDTH 64
#define ZELLER_LOGO_HEIGHT 16
static unsigned char zeller_logo_bits[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0xfe, 0xf3, 0xcf, 0x01, 0xe0, 0xfc, 0xf3, 0x1f,
    0xfe, 0xf3, 0xcf, 0x01, 0xe0, 0xfc, 0xf3, 0x3f, 0xfe, 0xf3, 0xcf, 0x01,
    0xe0, 0xfc, 0xf3, 0x7f, 0x00, 0x00, 0xc0, 0x01, 0xe0, 0x00, 0x00, 0x70,
    0xe0, 0x03, 0xc0, 0x01, 0xe0, 0x00, 0x00, 0x70, 0xf0, 0xf1, 0xcf, 0x01,
    0xe0, 0xfc, 0xf3, 0x7f, 0xf8, 0xf0, 0xcf, 0x01, 0xe0, 0xfc, 0xf3, 0x3f,
    0x7c, 0xf0, 0xcf, 0x01, 0xe0, 0xfc, 0xf3, 0x1f, 0x3e, 0x00, 0xc0, 0x01,
    0xe0, 0x00, 0x00, 0x00, 0x00, 0x00, 0xc0, 0x01, 0xe0, 0x00, 0x00, 0x00,
    0xfe, 0xf3, 0xcf, 0x3f, 0xff, 0xfc, 0x73, 0x1e, 0xfe, 0xf3, 0xcf, 0x3f,
    0xff, 0xfc, 0x73, 0x3c, 0xfe, 0xf3, 0xcf, 0x3f, 0xff, 0xfc, 0x73, 0x78,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

static const char *hvac_mode[] = {"COOL", "HEAT", "FAN", "AUTO"};

static const char *power_texts[] = {"OFF", "ON"};

static const char *hvac_speed[] = {"AUTO", "HIGH", "MEDI", "LOW"};

static const char *fh_mode[] = {"Day", "Night", "Away", "Normal"};

class DisplayHandler
{
public:
    DisplayHandler(uint8_t cs, uint8_t dc, uint8_t reset);

    void begin();
    // hvac is a non-owning reference to TouchModule's single HVACPanel
    // instance — DisplayHandler no longer keeps its own copy of HVAC state.
    void update(const HVACPanel &hvac);
    void drawScreen();  // draw screen clear -> put content,footer,... -> send buffer
    void drawContent(); // draw content of screen page things
    void drawFooter();  // draw footer page numbers

    void clear();
    void send();

    // void setJumpToPage(uint8_t time, uint8_t destination); //

    // void setValidPages(bool page1, bool page2, bool page3, bool page4, bool page5, bool page6, bool page7); // this just set witch pages should show
    void gotoPage(uint8_t pageNum);
    // void prevPage(); // goes to next page
    // void nextPage(); // goes to previos page
    uint8_t getPage() const { return currentPage; }

    bool changePage(bool forward);

    void drawText(int8_t x, int8_t y, const char *text);
    void drawCenteredText(const char *text);
    void drawCenteredTextH(int8_t y, const char *text);
    void drawCenteredTextV(int8_t x, const char *text);

    void setFont(const uint8_t *font);

    U8G2_SSD1325_NHD_128X64_F_4W_HW_SPI &getU8g2();

    // Blocking — call once from setup(), after begin(), before update() takes over.
    void startupAnimation();

    // Blocking — flashes "HERE" on screen for durationSeconds so the unit can be
    // visually located. Pair with LedHandler::finditAnimation() for combined effect.
    void finditAnimation(uint8_t durationSeconds);

    static constexpr uint8_t CHUNK_COUNT = 5;
    static constexpr uint8_t CHUNK_Y[CHUNK_COUNT] = {0, 30, 60, 90, 120};
    static constexpr uint8_t CHUNK_HEIGHT[CHUNK_COUNT] = {30, 30, 30, 30, 8};

    void drawImage(const uint8_t *image);
    void drawChunk(uint8_t chunk, const uint8_t *buffer);
    uint16_t getChunkSize(uint8_t chunk);
    // uint16_t getChunkHight(uint8_t chunk);

    void setBrightness(uint8_t brightness);

    void setReturnPage(uint page);
    void setReturnPage(uint page, uint8_t delay);

    void setValidPages(const uint8_t *pages);
    void setPageValid(uint8_t page, bool valid);
    bool isPageValid(uint8_t page) const;
    const uint8_t *getValidPages() const;

private:
    //-----------------------------------------------------------
    // SOFTWARE VALUES
    //-----------------------------------------------------------
    const uint8_t PAGE_COUNT = 7; // it usses for in function calculations

    // return to page values
    uint8_t returnDestination = 1; // 1 to 7 -> 0 mean no return
    uint8_t returnDelay = 20;      // 20 to 150 sec

    // indicator intensity
    uint8_t lcdbrightness = 50; // 0 to 100 persent
    // uint8_t _buttonBrightness = 50; // 0 to 100 persent
    bool ecoMode = false; // false -> always on | true -> eco mode
    // if was eco :
    uint8_t _ecoModeDelay = 10;      // 10 to 99 sec
    uint8_t _ecoModeBrightness = 10; // 0 to 100 persent
    bool _triggerLcdWake = false;    // UNKNOWN

    //-----------------------------------------------------------
    //-----------------------------------------------------------

    U8G2_SSD1325_NHD_128X64_F_4W_HW_SPI u8g2;

    uint8_t currentPage = 0;
    uint8_t previosPage = 7;

    bool sleep = false;

    uint8_t validPages[7] = {
        1, // switch page 1
        0, // switch page 2
        0, // switch page 3
        0, // switch page 4
        0, // hvac page
        0, // heating page
        0  // music page
    };
    // bool hvacPages[8] = {0, 0, 0, 0, 0, 0, 0};
    // bool heatPages[8] = {0, 0, 0, 0, 0, 0, 0};

    // bool _systemControl = false; //

    // bool _dreaming = false;
    // bool _waitforboot = false;
    // bool _loading = false;

    // hvac — no longer stored here. DisplayHandler reads from the
    // HVACPanel instance TouchModule owns; the pointer is refreshed on
    // every update(const HVACPanel&) call (see below) and read by
    // drawHvacPage(). Null until the first update() call.
    const HVACPanel *hvacPanel_ = nullptr;

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

    ///////////////////////////////////////////////////////////////////

    void drawImage(uint8_t page_number, uint8_t index, uint8_t y_offset);
    void drawSwitchBtn(int8_t x, int8_t y, uint8_t state);

    void drawBootupPage();
    void drawLoadingPage();
    void drawSettingPage();

    void drawScreenoff();
    void drawScreensaver();

    void drawChangePointer(uint8_t y);

    void drawSwitchPage();
    void drawHvacPage();
    void drawFloorheatPage();
    void drawMusicPage();

    void drawHvacMode(uint8_t y, uint8_t mode);
    void drawHvacPower(uint8_t y, uint8_t power);
    void drawHvacTemp(uint8_t y, float_t current, float_t sensor);
    void drawHvacSpeed(uint8_t y, uint8_t speed, bool autos);

    void drawFheatPower(uint8_t y, uint8_t power);
    void drawFheatTemp(uint8_t y, float_t current, float_t sensor);
    void drawFheatMode(uint8_t y, uint8_t mode);
};
#endif