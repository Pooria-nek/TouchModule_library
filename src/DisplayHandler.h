#ifndef DISPLAYHANDLER_H
#define DISPLAYHANDLER_H

#include <Arduino.h>
#include <U8g2lib.h>

class DisplayHandler
{
public:
    DisplayHandler(uint8_t cs, uint8_t dc, uint8_t reset);

    void begin();
    void refresh();
    void drawScreen();  // draw screen clear -> put content,footer,... -> send buffer
    void drawContent(); // draw content of screen page things
    void drawFooter();  // draw footer page numbers

    void clear();
    void send();

    void setJumpToPage(uint8_t time, uint8_t destination); //

    void setValidPages(bool page1, bool page2, bool page3, bool page4, bool page5, bool page6, bool page7); // this just set witch pages should show
    void gotoPage(uint8_t pageNum);
    void prevPage(); // goes to next page
    void nextPage(); // goes to previos page

    void drawText(int8_t x, int8_t y, const char *text);
    void drawCenteredText(const char *text);
    void drawCenteredTextH(int8_t y, const char *text);
    void drawCenteredTextV(int8_t x, const char *text);

    void setFont(const uint8_t *font);

    U8G2_SSD1325_NHD_128X64_F_4W_HW_SPI &getU8g2();

    // Blocking — call once from setup(), after begin(), before refresh() takes over.
    void startupAnimation();

    // Blocking — flashes "HERE" on screen for durationSeconds so the unit can be
    // visually located. Pair with LedHandler::finditAnimation() for combined effect.
    void finditAnimation(uint8_t durationSeconds);

private:
    //-----------------------------------------------------------
    // SOFTWARE VALUES
    //-----------------------------------------------------------
    bool buttonPage1 = false;
    bool buttonPage2 = false;
    bool buttonPage3 = false;
    bool buttonPage4 = false;
    bool acPage = false;
    bool floorHeatPage = false;
    bool musicPage = false;

    // return to page values
    uint8_t _returnDestination = 1; // 1 to 7
    uint8_t _returnDelay = 20;      // 20 to 150 sec

    // indicator intensity
    uint8_t _lcdbrightness = 50;    // 0 to 100 persent
    uint8_t _buttonBrightness = 50; // 0 to 100 persent
    bool _ecoMode = false;          // false -> always on | true -> eco mode
    // if was eco :
    uint8_t _ecoModeDelay = 10;      // 10 to 99 sec
    uint8_t _ecoModeBrightness = 10; // 0 to 100 persent
    bool _triggerLcdWake = false;    // UNKNOWN

    //-----------------------------------------------------------
    //-----------------------------------------------------------

    U8G2_SSD1325_NHD_128X64_F_4W_HW_SPI u8g2;

    uint8_t currentPage = 0;
    uint8_t previosPage = 0;

    // bool validPages[7] = {
    //     1, // switch page 1
    //     0, // switch page 2
    //     0, // switch page 3
    //     0, // switch page 4
    //     0, // hvac page
    //     0, // heating page
    //     0  // music page
    // };
    // bool hvacPages[8] = {0, 0, 0, 0, 0, 0, 0};
    // bool heatPages[8] = {0, 0, 0, 0, 0, 0, 0};

    // bool _systemControl = false; //

    // bool _dreaming = false;
    // bool _waitforboot = false;
    // bool _loading = false;

    void drawImage(uint8_t page_number, uint8_t index, uint8_t y_offset);
    void drawSwitchBtn(int8_t x, int8_t y, uint8_t state);

    void drawBootupPage();
    void drawLoadingPage();
    void drawSettingPage();

    void drawScreenoff();
    void drawScreensaver();

    void drawSwitchPage();
    void drawHvacPage();
    void drawFloorheatPage();
    void drawMusicPage();
};

#endif
