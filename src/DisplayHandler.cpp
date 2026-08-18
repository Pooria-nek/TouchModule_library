#include "DisplayHandler.h"

#define PICTURE_ADDRESS 10000

DisplayHandler::DisplayHandler(uint8_t cs, uint8_t dc, uint8_t reset)
    : u8g2(U8G2_R1, cs, dc, reset)
{
}

// just prepare and begin display things
void DisplayHandler::begin()
{
    // display_setTextSize(1);
    // u8g2.setFont(u8g2_font_6x10_tf);
    u8g2.setFontRefHeightExtendedText(); // ascent, descent
    // u8g2.setFontDirection(0);
    u8g2.setDrawColor(1);     // 0:clear 1:set 2:invert    0:black 1:white 2:inverse    1: foreground 0:background 2:inverse   0:off 1:on 2:toggle    0:transparent 1:solid 2:xor    0:fill 1:frame 2:fill&frame   0:normal 1:solid 2:xor
    u8g2.setFontPosTop();     // 0:top 1:baseline 2:center 3:bottom
    u8g2.setFontDirection(0); // 1:90 deg 2:180 deg 3:270 deg
    u8g2.setColorIndex(1);

    u8g2.begin();
    u8g2.setBusClock(8000000);
    u8g2.setFont(u8g2_font_6x10_tf);
}

unsigned long previousMillis = 0;
// refresh and handle time matter things on display
void DisplayHandler::refresh()
{
    unsigned long now = millis();

    if (now - previousMillis >= 500)
    {
        previousMillis = now;
        drawScreen();
    }
}

//
void DisplayHandler::setJumpToPage(uint8_t time, uint8_t destination)
{
}

//
void DisplayHandler::setValidPages(bool page1, bool page2, bool page3, bool page4, bool page5, bool page6, bool page7)
{
    // validPages[0] = page1;
    // validPages[1] = page2;
    // validPages[2] = page3;
    // validPages[3] = page4;
    // validPages[4] = page5;
    // validPages[5] = page6;
    // validPages[6] = page7;

    // bool flag = 0;
    // for (size_t i = 0; i < 7; i++)
    // {
    //     if (validPages[i] == 1)
    //     {
    //         flag = 1;
    //     }
    // }

    // if (flag)
    // {
    //     validPages[0] = 1;
    // }

    // drawScreen();
}

/* this set page number
values can be like this
0 -> ECO mode
1 -> switch page 1
2 -> switch page 2
3 -> switch page 3
4 -> switch page 4
5 -> hvac page
6 -> floor heat page
7 -> music page
*/
void DisplayHandler::gotoPage(uint8_t pageNum)
{
    if (pageNum > 7 || pageNum < 0)
    {
        return;
    }

    // uint8_t storedPageNum = pageNum;
    // if (validPages[pageNum] == 0)
    // {
    //     // search for next valid page
    //     do
    //     {
    //         pageNum = pageNum + 1;
    //         if (pageNum > 7)
    //         {
    //             pageNum = 0;
    //         }

    //         if (storedPageNum == pageNum)
    //         {
    //             validPages[0] = 1;
    //         }
    //     } while (validPages[pageNum] == 0);
    // }
}

void DisplayHandler::nextPage()
{
    currentPage = currentPage + 1;
    if (currentPage > 7)
    {
        currentPage = 1;
    }
}

void DisplayHandler::prevPage()
{
    currentPage = currentPage - 1;
    if (currentPage < 1)
    {
        currentPage = 7;
    }
}

void DisplayHandler::drawContent()
{
    // if (_systemControl)
    // {
    //     if (_waitforboot)
    //     {
    //         drawBootupPage();
    //     }
    //     else if (_loading)
    //     {
    //         drawLoadingPage();
    //     }
    //     else
    //     {
    //         drawSettingPage();
    //     }
    // }
    // else
    // if (_ecoMode)
    // {
    //     if (_ecoMode)
    //     {
    //         drawScreensaver();
    //     }
    //     else
    //     {
    //         drawScreenoff();
    //     }
    // }
    // else
    // {
        switch (currentPage)
        {
        case 1:
            drawSwitchPage();
            break;
        case 2:
            drawSwitchPage();
            break;
        case 3:
            drawSwitchPage();
            break;
        case 4:
            drawSwitchPage();
            break;

        case 5:
            drawHvacPage();
            break;

        case 6:
            drawFloorheatPage();
            break;

        case 7:
            drawMusicPage();
            break;
        }
    // }
}

void DisplayHandler::drawScreen()
{
    u8g2.clearBuffer();

    drawContent();
    drawFooter();

    u8g2.sendBuffer();
}

// *******************************************************************************************************
// system pages drawer
// *******************************************************************************************************

void DisplayHandler::drawBootupPage()
{
    drawCenteredText("Bootup");
}

void DisplayHandler::drawLoadingPage()
{
    drawCenteredText("Loading");
}

void DisplayHandler::drawSettingPage()
{
    drawCenteredText("Setting");
    u8g2.setFont(u8g2_font_open_iconic_check_1x_t);

    u8g2.drawGlyph(0, 101, 64);
    u8g2.drawGlyph(56, 101, 68);
}

// *******************************************************************************************************
// Idel pages drawer
// *******************************************************************************************************

void DisplayHandler::drawScreenoff()
{
    drawCenteredText("Screenoff");
}

void DisplayHandler::drawScreensaver()
{
    // u8g2.setContrast(0);
    u8g2.setFont(u8g2_font_luBS14_tn);
    drawCenteredText("Screensaver");
}

// *******************************************************************************************************
// interface pages drawer
// *******************************************************************************************************

void DisplayHandler::drawSwitchPage()
{
    // drawCenteredText("Switch");

    // theme 1
    // u8g2.drawHLine(0, 30, 64);
    // u8g2.drawHLine(0, 60, 64);
    // u8g2.drawHLine(0, 90, 64);

    // drawSwitchBtn(4, 6, 1);
    // drawSwitchBtn(44, 6, 1);

    // drawSwitchBtn(4, 36, 0);
    // drawSwitchBtn(44, 36, 0);

    // drawSwitchBtn(4, 66, 0);
    // drawSwitchBtn(44, 66, 1);

    // drawSwitchBtn(4, 96, 1);
    // drawSwitchBtn(44, 96, 0);

    // theme 2
    u8g2.drawVLine(0, 0, 30);
    u8g2.drawVLine(63, 0, 30);

    u8g2.drawVLine(0, 30, 30);
    u8g2.drawVLine(63, 30, 30);

    u8g2.drawVLine(0, 60, 30);
    u8g2.drawVLine(63, 60, 30);

    u8g2.drawVLine(0, 90, 30);
    u8g2.drawVLine(63, 90, 30);
}

void DisplayHandler::drawHvacPage()
{
    // drawCenteredText("Hvac");
    u8g2.drawVLine(32, 0, 30);
    u8g2.drawHLine(0, 30, 64);
    u8g2.drawHLine(0, 60, 64);
    u8g2.drawHLine(0, 90, 64);

    u8g2.setFont(u8g2_font_luBS08_tr);
    u8g2.drawStr(32 + ((32 / 2) - (u8g2.getStrWidth("Off") / 2)), 18, "Off");

    u8g2.drawStr(28, 34, "88.8");
    u8g2.drawStr(28, 48, "88.8");

    // drawCenteredTextH(34, "88.8");
    // drawCenteredTextH(48, "88.8");
    drawCenteredTextH(71, "High");
    drawCenteredTextH(101, "AC num");

    u8g2.setFont(u8g2_font_open_iconic_all_1x_t);
    u8g2.drawGlyph(44, 2, 235);

    u8g2.drawGlyph(14, 34, 184);
    u8g2.drawGlyph(14, 48, 183);

    u8g2.drawGlyph(0, 41, 110);
    u8g2.drawGlyph(56, 41, 111);

    u8g2.drawGlyph(0, 71, 110);
    u8g2.drawGlyph(56, 71, 111);
}

void DisplayHandler::drawFloorheatPage()
{
    // drawCenteredText("Floorheat");
    // u8g2.drawVLine(32, 0, 30);
    u8g2.drawHLine(0, 30, 64);
    u8g2.drawHLine(0, 60, 64);
    u8g2.drawHLine(0, 90, 64);

    drawCenteredTextH(41, "temp");
    drawCenteredTextH(101, "mode");

    u8g2.setFont(u8g2_font_open_iconic_all_1x_t);
    u8g2.drawGlyphX2(40, 6, 235);

    u8g2.drawGlyph(0, 41, 110);
    u8g2.drawGlyph(56, 41, 111);

    u8g2.drawGlyph(0, 101, 110);
    u8g2.drawGlyph(56, 101, 111);
}

// music page texts

void DisplayHandler::drawMusicPage()
{
    // u8g2.drawHLine(0, 30, 64);
    // u8g2.drawHLine(0, 60, 64);
    // u8g2.drawHLine(0, 90, 64);
    // drawCenteredText("Music");
    u8g2.setFont(u8g2_font_open_iconic_all_1x_t);

    u8g2.drawGlyph(4, 4, 210);
    // u8g2.drawGlyph(0, 4, 210);
    // u8g2.drawGlyph(16, 4, 211);
    // u8g2.drawGlyph(32, 4, 212);
    // u8g2.drawGlyph(48, 4, 217);
    u8g2.drawRFrame(0, 24, 64, 5, 2);
    u8g2.drawRBox(0, 24, 32, 5, 2);

    u8g2.drawGlyph(4, 36, 215);  // |<
    u8g2.drawGlyph(52, 36, 216); // >|

    drawCenteredTextH(48, "music-name");
    u8g2.setFont(u8g2_font_open_iconic_all_1x_t);

    u8g2.drawGlyph(4, 66, 213);  // <<
    u8g2.drawGlyph(52, 66, 214); // >>
    // u8g2.setFont(u8g2_font_6x10_tf);
    drawCenteredTextH(78, "Input-name");
    u8g2.setFont(u8g2_font_open_iconic_all_1x_t);

    u8g2.drawGlyph(4, 96, 278);  // valume up
    u8g2.drawGlyph(52, 96, 277); // valume down

    u8g2.drawRFrame(0, 110, 64, 5, 2);
    u8g2.drawRBox(0, 110, 24, 5, 2);
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////// DEVICE ANIMATIONS ///////////////////////////////

void DisplayHandler::startupAnimation()
{
    const uint8_t w = u8g2.getDisplayWidth();
    const uint8_t h = u8g2.getDisplayHeight();

    const uint8_t barMargin = 6;
    const uint8_t barW = w - (barMargin * 2);
    const uint8_t barH = 8;
    const uint8_t barY = (h / 2) - 4;

    // progress-bar sweep
    for (uint8_t i = 0; i <= barW; i += 2)
    {
        u8g2.clearBuffer();

        drawCenteredTextH(barY - 12, "Starting");
        u8g2.drawRFrame(barMargin, barY, barW, barH, 2);
        u8g2.drawRBox(barMargin, barY, i, barH, 2);

        u8g2.sendBuffer();
        delay(15);
    }

    // two quick full-screen flashes as a "boot confirmed" cue
    for (uint8_t i = 0; i < 2; i++)
    {
        u8g2.setDrawColor(1);
        u8g2.drawBox(0, 0, w, h);
        u8g2.sendBuffer();
        delay(80);

        u8g2.clearBuffer();
        u8g2.sendBuffer();
        delay(80);
    }

    u8g2.clearBuffer();
    drawCenteredText("Ready");
    u8g2.sendBuffer();
    delay(300);

    u8g2.clearBuffer();
    u8g2.sendBuffer();
}

void DisplayHandler::finditAnimation(uint8_t durationSeconds)
{
    const uint8_t w = u8g2.getDisplayWidth();
    const uint8_t h = u8g2.getDisplayHeight();

    const uint32_t endTime = millis() + (uint32_t)durationSeconds * 1000UL;
    bool on = false;

    while (millis() < endTime)
    {
        on = !on;
        u8g2.clearBuffer();

        if (on)
        {
            u8g2.setDrawColor(1);
            u8g2.drawBox(0, 0, w, h);
            u8g2.setDrawColor(0);
            drawCenteredTextH(h / 2, "HERE");
            u8g2.setDrawColor(1);
        }
        else
        {
            drawCenteredTextH(h / 2, "HERE");
        }

        u8g2.sendBuffer();
        delay(200);
    }

    u8g2.clearBuffer();
    u8g2.sendBuffer();
}

////////////////////////////// DEVICE ANIMATIONS ///////////////////////////////
////////////////////////////////////////////////////////////////////////////////


// *******************************************************************************************************
// *******************************************************************************************************

void DisplayHandler::drawFooter()
{
    u8g2.setFont(u8g2_font_6x10_tr);
    u8g2.setDrawColor(1);
    u8g2.drawBox(0, 119, 64, 9);

    for (size_t i = 1; i <= 7; i++)
    {
        // if (pages[i])
        // {
        u8g2.setDrawColor(0);
        u8g2.drawStr(1 + (i - 1) * (5 + 2), 119, String(i).c_str());
        // }
        if (i == currentPage)
        {
            u8g2.setDrawColor(2); // now white not black invert
            u8g2.drawBox((i - 1) * 7, 119, 7, 9);
        }
    }

    u8g2.setDrawColor(1);
}

void DisplayHandler::clear()
{
    u8g2.clearBuffer();
}

void DisplayHandler::send()
{
    u8g2.sendBuffer();
}

void DisplayHandler::drawText(int8_t x, int8_t y, const char *text)
{
    u8g2.drawStr(x, y, text);
}

// Core function to draw one image from flash
// this loads 32 to 80 pixle images
void DisplayHandler::drawImage(uint8_t page_number, uint8_t index, uint8_t y_offset)
{
    page_number--; // 0-based indexing

    for (uint8_t y = 0; y < 30; y++)
    {
        uint16_t base_addr = PICTURE_ADDRESS + (page_number * 4 * 16 * 20) + (index * 16 * 20) + (y * 10);
        // w25q16_read_array(base_addr, 10);

        uint8_t y_even = y + y_offset;
        u8g2.setColorIndex(1);
        // u8g2.drawXBM(0, y_even, 80, 1, read_data);
    }
}

void DisplayHandler::drawSwitchBtn(int8_t x, int8_t y, uint8_t state)
{
    uint8_t iconsize = 16;
    uint8_t r = iconsize / 4;
    uint8_t margin = 3;
    u8g2.drawRFrame(x, y, iconsize, iconsize, r);
    if (state == 1)
    {
        u8g2.drawRBox(x + margin, y + margin, iconsize - (margin * 2), iconsize - (margin * 2), r);
    }
}

void DisplayHandler::drawCenteredText(const char *text)
{
    int8_t width = u8g2.getStrWidth(text);
    int8_t x = (u8g2.getDisplayWidth() - width) / 2;

    int8_t ascent = u8g2.getAscent();
    int8_t descent = u8g2.getDescent();
    int8_t textHeight = ascent - descent;

    int8_t y = (u8g2.getDisplayHeight() + textHeight) / 2;

    u8g2.drawStr(x, y, text);
}

void DisplayHandler::drawCenteredTextH(int8_t y, const char *text)
{
    u8g2.setFont(u8g2_font_luBS08_tr);
    int8_t width = u8g2.getStrWidth(text);
    int8_t x = (u8g2.getDisplayWidth() - width) / 2;

    // int8_t ascent = u8g2.getAscent();
    // int8_t descent = u8g2.getDescent();
    // int8_t textHeight = ascent - descent;

    // int8_t y = (u8g2.getDisplayHeight() + textHeight) / 2;

    u8g2.drawStr(x, y, text);
}

void DisplayHandler::drawCenteredTextV(int8_t x, const char *text)
{
    u8g2.setFont(u8g2_font_luBS08_tr);
    // int8_t width = u8g2.getStrWidth(text);
    // int8_t x = (u8g2.getDisplayWidth() - width) / 2;

    int8_t ascent = u8g2.getAscent();
    int8_t descent = u8g2.getDescent();
    int8_t textHeight = ascent - descent;

    int8_t y = (u8g2.getDisplayHeight() + textHeight) / 2;

    u8g2.drawStr(x, y, text);
}

void DisplayHandler::setFont(const uint8_t *font)
{
    u8g2.setFont(font);
}

U8G2_SSD1325_NHD_128X64_F_4W_HW_SPI &DisplayHandler::getU8g2()
{
    return u8g2;
}
