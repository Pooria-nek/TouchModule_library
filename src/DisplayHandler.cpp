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
    u8g2.setFont(u8g2_font_synchronizer_nbp_tf);
}

unsigned long previousMillis = 0;
// update and handle time matter things on display
void DisplayHandler::update(const HVACPanel &hvac)
{
    hvacPanel_ = &hvac;

    unsigned long now = millis();

    if (now - previousMillis >= 500)
    {
        previousMillis = now;
        drawScreen();
    }
}

void DisplayHandler::setBrightness(uint8_t brightness)
{
    u8g2.setContrast(brightness);
}

void DisplayHandler::setReturnPage(uint page)
{
    returnDestination = page;
}

void DisplayHandler::setReturnPage(uint page, uint8_t delay)
{
    returnDestination = page;
    returnDelay = delay;
}

void DisplayHandler::setValidPages(const uint8_t *pages)
{
    bool atLeastOneValid = false;

    for (uint8_t i = 0; i < 7; i++)
    {
        if (pages[i])
        {
            atLeastOneValid = true;
            break;
        }
    }

    if (!atLeastOneValid)
        return;

    for (uint8_t i = 0; i < 7; i++)
        validPages[i] = pages[i] ? 1 : 0;

    // Current page is no longer valid
    if (!validPages[currentPage])
    {
        for (uint8_t i = 0; i < 7; i++)
        {
            if (validPages[i])
            {
                currentPage = i;
                break;
            }
        }
    }
}

void DisplayHandler::setPageValid(uint8_t page, bool valid)
{
    if (page >= 7)
        return;

    if (!valid)
    {
        // Don't allow all pages to become invalid
        bool anotherPageValid = false;

        for (uint8_t i = 0; i < 7; i++)
        {
            if (i != page && validPages[i])
            {
                anotherPageValid = true;
                break;
            }
        }

        if (!anotherPageValid)
            return;
    }

    validPages[page] = valid;
}

bool DisplayHandler::isPageValid(uint8_t page) const
{
    if (page < 7)
        return validPages[page];

    return false;
}

const uint8_t *DisplayHandler::getValidPages() const
{
    return validPages;
}

bool DisplayHandler::changePage(bool forward)
{
    constexpr uint8_t FIRST_PAGE = 0;
    constexpr uint8_t LAST_PAGE = 6;

    const uint8_t oldPage = currentPage;
    uint8_t page = currentPage;

    for (uint8_t i = 0; i < PAGE_COUNT - 1; i++)
    {
        if (forward)
            page = (page >= LAST_PAGE) ? FIRST_PAGE : page + 1;
        else
            page = (page <= FIRST_PAGE) ? LAST_PAGE : page - 1;

        if (validPages[page])
        {
            currentPage = page;
            return page != oldPage;
        }
    }

    return false;
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
    if (sleep)
    {
        //     if (_ecoMode)
        //     {
        drawScreensaver();
        //     }
        //     else
        //     {
        //         drawScreenoff();
        //     }
    }
    else
    {
        switch (currentPage)
        {
        case 0:
            drawSwitchPage();
            break;
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
            drawHvacPage();
            break;

            // case 5:
            //     drawMusicPage();
            //     break;

        case 6:
            drawFloorheatPage();
            break;
        }
    }
}

void DisplayHandler::drawScreen()
{
    // u8g2.clearBuffer();

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
    // u8g2.setFont(u8g2_font_open_iconic_check_1x_t);

    u8g2.setFont(u8g2_font_open_iconic_all_1x_t);

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
    // u8g2.setFont(u8g2_font_luBS14_tn);
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
    // u8g2.drawVLine(0, 0, 30);
    // u8g2.drawVLine(63, 0, 30);

    // u8g2.drawVLine(0, 30, 30);
    // u8g2.drawVLine(63, 30, 30);

    // u8g2.drawVLine(0, 60, 30);
    // u8g2.drawVLine(63, 60, 30);

    // u8g2.drawVLine(0, 90, 30);
    // u8g2.drawVLine(63, 90, 30);
}

void DisplayHandler::drawChangePointer(uint8_t y)
{
    u8g2.setFont(u8g2_font_open_iconic_all_1x_t);
    u8g2.drawGlyph(0, y, 110);
    u8g2.drawGlyph(56, y, 111);
}

void DisplayHandler::drawHvacMode(uint8_t y, uint8_t mode)
{
    if (mode > 3)
        return;

    const uint8_t x = 0;
    const uint8_t icon = 16;
    const uint8_t margin = (32 - icon) / 2;

    if (mode == 0) // COOL
    {
        u8g2.drawXBMP(x + margin, y, 16, 16, cool_icon);
    }
    else if (mode == 1) // HEAT
    {
        u8g2.drawXBMP(x + margin, y, 16, 16, heat_icon);
    }

    // u8g2.drawBox(x + marger, y, icon, icon);
    u8g2.drawStr(x + 3, y + 19, hvac_mode[mode]);
}

void DisplayHandler::drawHvacPower(uint8_t y, uint8_t power)
{
    if (power > 1)
        return;

    const uint8_t x = 33;
    const uint8_t icon = 16;
    const uint8_t margin = (32 - icon) / 2;
    const uint8_t iconX = x + margin;

    u8g2.drawXBMP(x + margin, y, 16, 16, power_icon);

    u8g2.drawStr(x + 8, y + 19, power_texts[power]);
}

// void DisplayHandler::drawHvacTemp(uint8_t y, float_t current, float_t sensor)
// {
//     u8g2.setFont(u8g2_font_6x10_tf);

//     const uint8_t x = 8;
//     const uint8_t icon = 13;

//     current = constrain(current, 0.0f, 40.0f);
//     sensor  = constrain(sensor,  0.0f, 40.0f);

//     u8g2.drawXBMP(x, y + 2, icon, icon, temp_icon);
//     u8g2.setCursor(30, y + 4);
//     u8g2.print(current, 1);

//     u8g2.drawXBMP(x, y + 16, icon, icon, internal_icon);
//     u8g2.setCursor(30, y + 18);
//     u8g2.print(sensor, 1);

//     u8g2.setFont(u8g2_font_synchronizer_nbp_tf);
// }

void DisplayHandler::drawHvacTemp(uint8_t y, float_t current, float_t sensor)
{
    u8g2.setFont(u8g2_font_6x10_tf);

    const uint8_t x = 8;
    const uint8_t icon = 13;

    const bool currentValid = current >= 16.0f && current <= 40.0f;
    const bool sensorValid = sensor >= 16.0f && sensor <= 40.0f;

    u8g2.drawXBMP(x, y + 2, icon, icon, temp_icon);

    u8g2.setCursor(30, y + 4);
    if (currentValid)
        u8g2.print(current, 1);
    else
        u8g2.print("--.-");

    u8g2.drawXBMP(x, y + 16, icon, icon, internal_icon);

    u8g2.setCursor(30, y + 18);
    if (sensorValid)
        u8g2.print(sensor, 1);
    else
        u8g2.print("--.-");

    drawChangePointer(y + 11);

    u8g2.setFont(u8g2_font_synchronizer_nbp_tf);
}

void DisplayHandler::drawHvacSpeed(uint8_t y, uint8_t speed, bool autos)
{
    if (speed > 3)
        return;

    uint8_t w = 4;
    const uint8_t x = 8;
    const uint8_t gap = 1;

    // Speed text
    u8g2.drawStr(28, y + 11, autos ? hvac_speed[0] : hvac_speed[speed]);

    drawChangePointer(y + 11);

    // Speed bars

    if (speed == 1)
    { // HIGH
        u8g2.drawBox(x, y + 16, w, 6);
        u8g2.drawBox(x + w + gap, y + 12, w, 10);
        u8g2.drawBox(x + (w + gap) * 2, y + 8, w, 14);
    }
    else if (speed == 2)
    { // MEDI
        u8g2.drawBox(x, y + 16, w, 6);
        u8g2.drawBox(x + w + gap, y + 12, w, 10);
        u8g2.drawFrame(x + (w + gap) * 2, y + 8, w, 14);
    }
    else if (speed == 3)
    { // LOW
        u8g2.drawBox(x, y + 16, w, 6);
        u8g2.drawFrame(x + w + gap, y + 12, w, 10);
        u8g2.drawFrame(x + (w + gap) * 2, y + 8, w, 14);
    }
}

#ifdef OLDIE
void DisplayHandler::drawHvacPage()
{
    if (!hvacPanel_)
        return; // update(hvac) hasn't been called yet — nothing to draw

    const HVACPanel::State &state = hvacPanel_->currentState();

    // put mode icon in left and power in right
    u8g2.setDrawColor(1);

    drawHvacMode(0, state.mode);

    drawHvacPower(0, state.power ? 1 : 0);

    u8g2.drawVLine(33, 0, 30);
    u8g2.drawHLine(0, 30, 64);

    // top = target (set) temp, bottom = live sensor (current) temp
    drawHvacTemp(30, state.setTemp, state.currentTemp);

    // current and settemp here
    // up and down
    u8g2.drawHLine(0, 60, 64);

    // fan == 0 is "AUTO" (see hvac_speed[]); drawHvacSpeed already treats
    // speed 0 as "no bars", so passing it straight through is correct.
    drawHvacSpeed(60, state.fan, state.fan == 0);

    // fan speed icon in left and text in right
    u8g2.drawHLine(0, 90, 64);

    // u8g2.drawStr(28, 71, speed_texts[0]);

    // u8g2.drawBox(6, 67 + 8, 5, 8);
    // u8g2.drawBox(13, 67 + 4, 5, 12);
    // u8g2.drawFrame(20, 67, 5, 16);

    // current ac indicator here
}
#endif

#ifdef MODERN
void DisplayHandler::drawHvacPage()
{
    if (!hvacPanel_)
        return; // update(hvac) hasn't been called yet — nothing to draw

    const HVACPanel::State &state = hvacPanel_->currentState();
    const uint8_t zoneIndex = hvacPanel_->current();

    constexpr uint8_t SCREEN_W = 64;
    constexpr uint8_t SCREEN_H = 120;

    // ---------------------------------------------------------
    // AC layout
    // ---------------------------------------------------------

    // Top separator
    u8g2.drawHLine(0, 14, SCREEN_W);

    // Bottom separators
    u8g2.drawHLine(0, 92, SCREEN_W);
    u8g2.drawHLine(0, 112, SCREEN_W);

    // ---------------------------------------------------------
    // Header
    // ---------------------------------------------------------

    u8g2.setFont(u8g2_font_synchronizer_nbp_tr);

    // AC name / number
    String acName = "AC " + String(zoneIndex + 1);

    drawCenteredTextH(8, acName.c_str());

    // Power status
    if (state.power)
    {
        u8g2.setDrawColor(1);
        u8g2.drawGlyph(3, 10, 235); // power icon
    }
    else
    {
        u8g2.setDrawColor(1);
        u8g2.drawStr(3, 9, "OFF");
    }

    // ---------------------------------------------------------
    // Temperature
    // ---------------------------------------------------------

    // Current temperature
    u8g2.setFont(u8g2_font_helvB14_tr);

    String currentTemp = String(state.currentTemp, 1);
    drawCenteredTextH(45, currentTemp.c_str());

    // Degree symbol
    u8g2.setFont(u8g2_font_synchronizer_nbp_tr);
    u8g2.drawStr(50, 37, "C");

    // Set temperature
    u8g2.setFont(u8g2_font_helvB10_tr);

    String setTemp = String(state.setTemp, 1);
    drawCenteredTextH(67, setTemp.c_str());

    // ---------------------------------------------------------
    // Mode
    // ---------------------------------------------------------

    u8g2.setFont(u8g2_font_synchronizer_nbp_tr);

    String mode;

    switch (state.mode)
    {
    case 0:
        mode = "Cool";
        break;

    case 1:
        mode = "Heat";
        break;

    case 2:
        mode = "Dry";
        break;

    case 3:
        mode = "Fan";
        break;

    default:
        mode = "---";
        break;
    }

    drawCenteredTextH(85, mode.c_str());

    // ---------------------------------------------------------
    // Fan speed
    // ---------------------------------------------------------

    switch (state.fan)
    {
    case 0:
        drawCenteredTextH(104, "Auto");
        break;

    case 1:
        drawCenteredTextH(104, "Low");
        break;

    case 2:
        drawCenteredTextH(104, "Med");
        break;

    case 3:
        drawCenteredTextH(104, "High");
        break;

    default:
        drawCenteredTextH(104, "---");
        break;
    }

    // ---------------------------------------------------------
    // Navigation arrows
    // ---------------------------------------------------------

    drawChangePointer(104);

    // ---------------------------------------------------------
    // Reset draw color
    // ---------------------------------------------------------

    u8g2.setDrawColor(1);
}
#endif

void DisplayHandler::drawFheatPower(uint8_t y, uint8_t power)
{
    u8g2.setFont(u8g2_font_synchronizer_nbp_tf);

    const uint8_t x = 33;
    const uint8_t margin = (32 - 16) / 2;

    u8g2.drawXBMP(0, 0, 24, 24, heater_icon);

    u8g2.drawXBMP(x + margin, y, 16, 16, power_icon);
    u8g2.drawStr(x + 8, y + 19, power_texts[power]);
}

void DisplayHandler::drawFheatTemp(uint8_t y, float_t current, float_t sensor)
{
    u8g2.setFont(u8g2_font_6x10_tf);

    const uint8_t x = 8;
    const uint8_t icon = 13;

    const bool currentValid = current >= 16.0f && current <= 40.0f;
    const bool sensorValid = sensor >= 16.0f && sensor <= 40.0f;

    u8g2.drawXBMP(x, y + 2, icon, icon, temp_icon);

    u8g2.setCursor(30, y + 4);
    if (currentValid)
        u8g2.print(current, 1);
    else
        u8g2.print("--.-");

    u8g2.drawXBMP(x, y + 16, icon, icon, internal_icon);

    u8g2.setCursor(30, y + 18);
    if (sensorValid)
        u8g2.print(sensor, 1);
    else
        u8g2.print("--.-");

    drawChangePointer(y + 11);

    u8g2.setFont(u8g2_font_synchronizer_nbp_tf);
}

void DisplayHandler::drawFheatMode(uint8_t y, uint8_t mode)
{
    u8g2.setFont(u8g2_font_6x10_tf);

    const uint8_t x = 8;
    const uint8_t icon = 13;

    drawCenteredTextH(y + 11, fh_mode[mode]);

    drawChangePointer(y + 11);

    u8g2.setFont(u8g2_font_synchronizer_nbp_tf);
}

void DisplayHandler::drawFloorheatPage()
{
    // drawCenteredText("Floorheat");
    // u8g2.drawVLine(32, 0, 30);

    drawFheatPower(0, 1);
    drawFheatTemp(30, 16.5f, 27.8f);
    drawFheatMode(60, 1);
    // u8g2.drawHLine(0, 30, 64);
    // u8g2.drawHLine(0, 60, 64);
    // u8g2.drawHLine(0, 90, 64);

    u8g2.setFont(u8g2_font_open_iconic_all_1x_t);
}

// music page texts

void DisplayHandler::drawMusicPage()
{
    // u8g2.drawHLine(0, 30, 64);
    // u8g2.drawHLine(0, 60, 64);
    // u8g2.drawHLine(0, 90, 64);
    // drawCenteredText("Music");
    // u8g2.setFont(u8g2_font_open_iconic_all_1x_t);

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
    // u8g2.setFont(u8g2_font_open_iconic_all_1x_t);

    u8g2.drawGlyph(4, 66, 213);  // <<
    u8g2.drawGlyph(52, 66, 214); // >>
    // u8g2.setFont(u8g2_font_6x10_tf);
    drawCenteredTextH(78, "Input-name");
    // u8g2.setFont(u8g2_font_open_iconic_all_1x_t);

    u8g2.drawGlyph(4, 96, 278);  // valume up
    u8g2.drawGlyph(52, 96, 277); // valume down

    u8g2.drawRFrame(0, 110, 64, 5, 2);
    u8g2.drawRBox(0, 110, 24, 5, 2);
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////// DEVICE ANIMATIONS ///////////////////////////////

void DisplayHandler::startupAnimation()
{
    constexpr uint16_t totalPixels =
        ZELLER_LOGO_WIDTH * ZELLER_LOGO_HEIGHT;

    constexpr uint8_t pixelsPerFrame = 16;

    static uint16_t pixelOrder[totalPixels];

    // Generate pixel order
    for (uint16_t i = 0; i < totalPixels; i++)
        pixelOrder[i] = i;

    // Fisher-Yates shuffle
    for (uint16_t i = totalPixels - 1; i > 0; i--)
    {
        const uint16_t j = random(i + 1);

        const uint16_t temp = pixelOrder[i];
        pixelOrder[i] = pixelOrder[j];
        pixelOrder[j] = temp;
    }

    const uint8_t xOffset =
        (u8g2.getDisplayWidth() - ZELLER_LOGO_WIDTH) >> 1;

    const uint8_t yOffset =
        (u8g2.getDisplayHeight() - ZELLER_LOGO_HEIGHT) >> 1;

    u8g2.clearBuffer();
    u8g2.setDrawColor(1);

    uint16_t pixelIndex = 0;

    while (pixelIndex < totalPixels)
    {
        uint8_t count = 0;

        while (count++ < pixelsPerFrame &&
               pixelIndex < totalPixels)
        {
            const uint16_t pixel =
                pixelOrder[pixelIndex++];

            const uint8_t x = pixel & 0x3F;
            const uint8_t y = pixel >> 6;

            const uint16_t byteIndex =
                (y << 3) + (x >> 3);

            const uint8_t bitMask =
                1 << (x & 7);

            if (zeller_logo_bits[byteIndex] & bitMask)
            {
                u8g2.drawPixel(
                    x + xOffset,
                    y + yOffset
                );
            }
        }

        u8g2.sendBuffer();
        delay(15);
    }

    delay(3000);

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

void DisplayHandler::drawImage(const uint8_t *image)
{
    u8g2.setColorIndex(1);

    u8g2.drawXBMP(0, 0, 64, 120, image);
}

void DisplayHandler::drawChunk(uint8_t chunk, const uint8_t *buffer)
{
    if (chunk >= 5)
        return;

    u8g2.drawXBM(0, CHUNK_Y[chunk], 64, CHUNK_HEIGHT[chunk], buffer);
}

uint16_t DisplayHandler::getChunkSize(uint8_t chunk)
{
    if (chunk >= CHUNK_COUNT)
        return 0;

    return (64 * CHUNK_HEIGHT[chunk]) / 8;
}

// uint16_t DisplayHandler::getChunkHight(uint8_t chunk)
// {
//     if (chunk >= CHUNK_COUNT)
//         return 0;

//     return (64 * CHUNK_HEIGHT[chunk]) / 8;
// }

// *******************************************************************************************************
// *******************************************************************************************************

#ifdef OLDIE
void DisplayHandler::drawFooter()
{
    constexpr uint8_t FOOTER_Y = 120;
    constexpr uint8_t FOOTER_H = 8;
    constexpr uint8_t ITEM_W = 8;
    constexpr uint8_t GAP = 0;

    u8g2.setFont(u8g2_font_synchronizer_nbp_tr);

    // Footer background
    u8g2.setDrawColor(1);
    u8g2.drawBox(0, FOOTER_Y, 64, FOOTER_H);

    uint8_t x = 0;

    for (uint8_t i = 0; i < 7; i++)
    {
        if (!validPages[i])
            continue;

        if (i == currentPage)
        {
            u8g2.setDrawColor(0);
            u8g2.drawBox(x, FOOTER_Y, ITEM_W, FOOTER_H);

            u8g2.setDrawColor(1);
            u8g2.drawStr(
                x + 2,
                FOOTER_Y,
                String(i + 1).c_str());
        }
        else
        {
            u8g2.setDrawColor(0);
            u8g2.drawStr(
                x + 2,
                FOOTER_Y,
                String(i + 1).c_str());
        }

        x += ITEM_W + GAP;
    }

    u8g2.setDrawColor(1);
}
#endif

#ifdef MODERN
void DisplayHandler::drawFooter()
{
    constexpr uint8_t FOOTER_Y = 120;
    constexpr uint8_t FOOTER_H = 8;
    constexpr uint8_t ITEM_W = 8;
    constexpr uint8_t GAP = 0;
    constexpr uint8_t FOOTER_W = 64;

    u8g2.setFont(u8g2_font_synchronizer_nbp_tr);

    // Footer background
    u8g2.setDrawColor(0);
    u8g2.drawBox(0, FOOTER_Y, FOOTER_W, FOOTER_H);

    // Count valid pages
    uint8_t pageCount = 0;

    for (uint8_t i = 0; i < PAGE_COUNT; i++)
    {
        if (validPages[i])
            pageCount++;
    }

    if (pageCount == 0)
        return;

    // Calculate total width and center it
    const uint8_t totalWidth =
        pageCount * ITEM_W + (pageCount - 1) * GAP;

    uint8_t x = (FOOTER_W - totalWidth) / 2;

    for (uint8_t i = 0; i < PAGE_COUNT; i++)
    {
        if (!validPages[i])
            continue;

        if (i == currentPage)
        {
            // Selected page
            u8g2.setDrawColor(1);
            u8g2.drawRBox(
                x,
                FOOTER_Y,
                ITEM_W,
                FOOTER_H,
                2);

            // Page number
            u8g2.setDrawColor(0);
            u8g2.drawStr(
                x + 2,
                FOOTER_Y,
                String(i + 1).c_str());
        }
        else
        {
            // Unselected page
            u8g2.setDrawColor(1);
            u8g2.drawDisc(
                x + ITEM_W / 2,
                FOOTER_Y + FOOTER_H / 2,
                1);
        }

        x += ITEM_W + GAP;
    }

    u8g2.setDrawColor(0);
}
#endif

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
        u8g2.setColorIndex(0);
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
    // u8g2.setFont(u8g2_font_luBS08_tr);
    int8_t width = u8g2.getStrWidth(text);
    int8_t x = (u8g2.getDisplayWidth() - width) / 2;
    u8g2.drawStr(x, y, text);
}

void DisplayHandler::drawCenteredTextV(int8_t x, const char *text)
{
    // u8g2.setFont(u8g2_font_luBS08_tr);
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
    // u8g2.setFont(font);
}

U8G2_SSD1325_NHD_128X64_F_4W_HW_SPI &DisplayHandler::getU8g2()
{
    return u8g2;
}
