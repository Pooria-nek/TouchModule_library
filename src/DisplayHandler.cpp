#include "DisplayHandler.h"

#define PICTURE_ADDRESS 10000

DisplayHandler::DisplayHandler(MemoryCore &flash, uint8_t cs, uint8_t dc, uint8_t reset)
    : u8g2(U8G2_R1, cs, dc, reset),
      flash_(flash)
{
}

// just prepare and begin display things
void DisplayHandler::begin()
{
    // -------------------------------------------------
    // Display initialization
    // -------------------------------------------------
    u8g2.begin();
    u8g2.setBusClock(8000000);

    // -------------------------------------------------
    // Drawing configuration
    // -------------------------------------------------
    u8g2.setDrawColor(1);
    u8g2.setColorIndex(1);

    u8g2.setFontRefHeightExtendedText();
    u8g2.setFontPosTop();
    u8g2.setFontDirection(0);

    // -------------------------------------------------
    // Default font
    // -------------------------------------------------
    u8g2.setFont(u8g2_font_synchronizer_nbp_tf);

    // -------------------------------------------------
    // Startup
    // -------------------------------------------------
    startupAnimation();
}

void DisplayHandler::updateResource(const HVACPanel &hvac)
{
    hvacPanel_ = &hvac;
}

// void DisplayHandler::updateResource(const FHeatPanel &fheat)
// {
//     fheatPanel_ = &fheat;
// }

void DisplayHandler::updateOutTemprature(const NTC &ntc)
{
    outTemp = ntc.temperature();
}

void DisplayHandler::updateInTemprature(const NTC &ntc)
{
    inTemp = ntc.temperature();
}

// void DisplayHandler::drawTemperature(const NTC &ntc, uint8_t x, uint8_t y)
// {
//     char buffer[20];

//     snprintf(
//         buffer,
//         sizeof(buffer),
//         "A:%u",
//         ntc.adc());

//     u8g2.drawStr(0, y, buffer);

//     snprintf(
//         buffer,
//         sizeof(buffer),
//         "R:%lu",
//         static_cast<unsigned long>(ntc.resistance()));

//     u8g2.drawStr(0, y + 12, buffer);

//     float temp = ntc.temperature();

//     int16_t whole = static_cast<int16_t>(temp);
//     uint8_t decimal = static_cast<uint8_t>(fabsf(temp - whole) * 10.0f);

//     snprintf(
//         buffer,
//         sizeof(buffer),
//         "T:%d.%d",
//         whole,
//         decimal);

//     u8g2.drawStr(0, y + 24, buffer);
// }

unsigned long previousMillis = 0;

constexpr unsigned long DISPLAY_UPDATE_MS = 100;
constexpr unsigned long SLEEP_UPDATE_MS = 20000;

// update and handle time matter things on display
void DisplayHandler::update()
{
    unsigned long now = millis();

    unsigned long interval = _sleeping ? SLEEP_UPDATE_MS : DISPLAY_UPDATE_MS;

    if (now - previousMillis >= interval)
    {
        previousMillis = now;
        drawScreen();
    }
}

void DisplayHandler::setBrightness(uint8_t brightness)
{
    u8g2.setContrast(brightness);
}

void DisplayHandler::setValidPages(const uint8_t *pages)
{
    bool atLeastOneValid = false;

    for (uint8_t i = 0; i < PAGE_COUNT; ++i)
    {
        if (pages[i])
        {
            atLeastOneValid = true;
            break;
        }
    }

    if (!atLeastOneValid)
        return;

    for (uint8_t i = 0; i < PAGE_COUNT; ++i)
        validPages[i] = pages[i] ? 1 : 0;

    // Current page is no longer valid
    if (!isPageValid(currentPage))
    {
        for (uint8_t page = 1; page <= PAGE_COUNT; ++page)
        {
            if (isPageValid(page))
            {
                currentPage = page;
                break;
            }
        }
    }
}

bool DisplayHandler::isPageValid(uint8_t page) const
{
    if ((page - 1) < PAGE_COUNT)
        return validPages[page - 1];

    return false;
}

const uint8_t *DisplayHandler::getValidPages() const
{
    return validPages;
}

bool DisplayHandler::changePage(bool forward)
{
    const uint8_t oldPage = currentPage;
    uint8_t page = currentPage;

    for (uint8_t i = 1; i < PAGE_COUNT; ++i)
    {
        if (forward)
            page = (page >= PAGE_COUNT) ? 1 : page + 1;
        else
            page = (page <= 1) ? PAGE_COUNT : page - 1;

        if (isPageValid(page))
        {
            currentPage = page;
            return page != oldPage;
        }
    }

    return false;
}

void DisplayHandler::drawContent()
{
    // if (_sleeping && ecoMode)
    if (_sleeping)
    {
        drawScreensaver();
        return;
    }

    switch (currentPage)
    {
    case 1:
    case 2:
    case 3:
    case 4:
        drawSwitchPage();
        break;

    case 5:
        if (hvacPanel_)
            drawHvacPage(*hvacPanel_);
        break;

    case 6:
        drawFloorheatPage();
        break;
    }

    drawFooter();
}

void DisplayHandler::drawScreen()
{
    u8g2.clearBuffer();

    drawContent();

    u8g2.sendBuffer();
}

// *******************************************************************************************************
// system pages drawer
// *******************************************************************************************************

// *******************************************************************************************************
// Idel pages drawer
// *******************************************************************************************************

void DisplayHandler::drawScreensaver()
{
    static uint8_t x = 0;
    static uint8_t y = 0;

    // 0 = internal
    // 1 = external
    // 2 = both
    uint8_t temperatureMode = 1;

    constexpr uint8_t widgetWidth = 58;

    uint8_t widgetHeight;

    if (temperatureMode == 2)
        widgetHeight = 40;
    else
        widgetHeight = 20;

    x = 5;
    y = random(5, 128 - widgetHeight + 1);

    char buffer[20];

    // // -------------------------
    // // Internal temperature
    // // -------------------------
    // if (temperatureMode == 0)
    // {
    //     u8g2.setFont(u8g2_font_luBS14_tn);

    //     int16_t whole = static_cast<int16_t>(inTemp);
    //     uint8_t decimal = static_cast<uint8_t>(fabsf(inTemp - whole) * 10.0f);

    //     snprintf(buffer, sizeof(buffer), "%d.%d", whole, decimal);

    //     u8g2.drawStr(x + 17, y + 11, buffer);
    // }

    // -------------------------
    // External temperature
    // -------------------------
    if (temperatureMode == 1)
    {
        int16_t whole = static_cast<int16_t>(outTemp);
        uint8_t decimal = static_cast<uint8_t>(fabsf(outTemp - whole) * 10.0f);

        snprintf(buffer, sizeof(buffer), "%d.%d", whole, decimal);

        u8g2.setFont(u8g2_font_luBS14_tn);
        u8g2.drawStr(x, y, buffer);

        u8g2.setFont(u8g2_font_6x10_tr);
        u8g2.drawStr(x + 50, y - 3, "c");
    }

    // // -------------------------
    // // Both temperatures
    // // -------------------------
    // if (temperatureMode == 2)
    // {
    //     int16_t whole = static_cast<int16_t>(outTemp);

    //     uint8_t decimal = static_cast<uint8_t>(fabsf(outTemp - whole) * 10.0f);

    //     snprintf(buffer, sizeof(buffer), "%d.%d", whole, decimal);

    //     u8g2.drawXBMP(x, y + 19, 13, 13, external_icon);

    //     u8g2.drawStr(x + 17, y + 30, buffer);
    // }
}

void DisplayHandler::sleep()
{
    if (_sleeping)
        return;

    _sleeping = true;

    previousMillis = millis();

    u8g2.clearBuffer();

    if (returnDestination != 0)
    {
        currentPage = returnDestination;
    }

    if (ecoMode)
    {
        // Eco ON:
        // Keep OLED visible but dim
        u8g2.setContrast(_ecoModeBrightness);
    }
    else
    {
        // Eco OFF:
        // Turn the screen visually off
        u8g2.setContrast(0);
    }

    // Draw the appropriate sleep screen
    drawScreen();
}

void DisplayHandler::wake()
{
    if (!_sleeping)
        return;

    _sleeping = false;

    previousMillis = millis();

    // Restore normal brightness
    u8g2.setContrast(lcdbrightness);

    // Redraw current page
    drawScreen();
}

void DisplayHandler::setEcoMode(bool enabled)
{
    ecoMode = enabled;
}

void DisplayHandler::setReturnPage(uint8_t page)
{
    if (page > 6)
        return;

    returnDestination = page;
}

// *******************************************************************************************************
// interface pages drawer
// *******************************************************************************************************

void DisplayHandler::drawSwitchPage()
{
    drawImage(currentPage);
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
void DisplayHandler::drawHvacPage(const HVACPanel &hvac)
{
    if (!hvacPanel_)
        return; // update(hvac) hasn't been called yet — nothing to draw

    const HVACPanel::State &state = hvac.currentState();

    uint8_t currentHvac = hvac.currentHvac();

    // put mode icon in left and power in right
    u8g2.setDrawColor(1);

    drawHvacMode(0, state.mode);

    drawHvacPower(0, state.power ? 1 : 0);

    // top = target (set) temp, bottom = live sensor (current) temp
    drawHvacTemp(30, state.setTemp, state.currentTemp);

    drawHvacSpeed(60, state.fan, state.fan == 0);

    // Current HVAC image
    u8g2.drawXBMP(0, 90, 64, 30, hvac.currentImage());

    u8g2.drawVLine(33, 0, 30);
    u8g2.drawHLine(0, 30, 64);
    u8g2.drawHLine(0, 60, 64);
    u8g2.drawHLine(0, 90, 64);
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

// void DisplayHandler::drawMusicPage()
// {
//     // u8g2.drawHLine(0, 30, 64);
//     // u8g2.drawHLine(0, 60, 64);
//     // u8g2.drawHLine(0, 90, 64);
//     // drawCenteredText("Music");
//     // u8g2.setFont(u8g2_font_open_iconic_all_1x_t);

//     u8g2.drawGlyph(4, 4, 210);
//     // u8g2.drawGlyph(0, 4, 210);
//     // u8g2.drawGlyph(16, 4, 211);
//     // u8g2.drawGlyph(32, 4, 212);
//     // u8g2.drawGlyph(48, 4, 217);
//     u8g2.drawRFrame(0, 24, 64, 5, 2);
//     u8g2.drawRBox(0, 24, 32, 5, 2);

//     u8g2.drawGlyph(4, 36, 215);  // |<
//     u8g2.drawGlyph(52, 36, 216); // >|

//     drawCenteredTextH(48, "music-name");
//     // u8g2.setFont(u8g2_font_open_iconic_all_1x_t);

//     u8g2.drawGlyph(4, 66, 213);  // <<
//     u8g2.drawGlyph(52, 66, 214); // >>
//     // u8g2.setFont(u8g2_font_6x10_tf);
//     drawCenteredTextH(78, "Input-name");
//     // u8g2.setFont(u8g2_font_open_iconic_all_1x_t);

//     u8g2.drawGlyph(4, 96, 278);  // valume up
//     u8g2.drawGlyph(52, 96, 277); // valume down

//     u8g2.drawRFrame(0, 110, 64, 5, 2);
//     u8g2.drawRBox(0, 110, 24, 5, 2);
// }

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
                    y + yOffset);
            }
        }

        u8g2.sendBuffer();
        delay(15);
    }

    delay(500);

    // u8g2.clearBuffer();
    // u8g2.sendBuffer();
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

void DisplayHandler::drawImage(const uint8_t image)
{

    uint8_t imageBuffer[960];

    if (image >= 1 && image <= 4)
    {
        for (uint8_t y = 0; y < 60; y++)
        {
            flash_.read(flash_.findAdrress(MemoryAdress::Touch::SECTOR_IMAGES, MemoryAdress::Touch::pageImage(image - 1, y)), imageBuffer + (y * 16), 16);
        }

        u8g2.setColorIndex(1);

        u8g2.drawXBMP(0, 0, 64, 120, imageBuffer);
    }
}

// void DisplayHandler::drawChunk(uint8_t chunk, const uint8_t image)
// {
//     if (chunk >= 5)
//         return;

//     uint8_t imageBuffer[240];

//     const uint8_t startY = chunk * 15;

//     for (uint8_t y = 0; y < 15; y++)
//     {
//         flash_.read(
//             flash_.findAdrress(
//                 MemoryAdress::Touch::SECTOR_IMAGES,
//                 MemoryAdress::Touch::pageImage(image, startY + y)),
//             imageBuffer + (y * 16),
//             16);
//     }

//     u8g2.drawXBMP(0, 90, 64, 30, imageBuffer);
// }

// *******************************************************************************************************
// *******************************************************************************************************

// #ifdef OLDIE
// void DisplayHandler::drawFooter()
// {
//     constexpr uint8_t FOOTER_Y = 120;
//     constexpr uint8_t FOOTER_H = 8;
//     constexpr uint8_t ITEM_W = 8;

//     u8g2.setFont(u8g2_font_synchronizer_nbp_tr);

//     // Footer background
//     u8g2.setDrawColor(1);
//     u8g2.drawBox(0, FOOTER_Y, 64, FOOTER_H);

//     uint8_t x = 0;

//     for (uint8_t page = 1; page <= PAGE_COUNT; ++page)
//     {
//         if (!isPageValid(page))
//             continue;

//         if (page == currentPage)
//         {
//             u8g2.setDrawColor(0);
//             u8g2.drawBox(x, FOOTER_Y, ITEM_W, FOOTER_H);

//             u8g2.setDrawColor(1);
//             u8g2.drawStr(
//                 x + 2,
//                 FOOTER_Y,
//                 String(page).c_str());
//         }
//         else
//         {
//             u8g2.setDrawColor(0);
//             u8g2.drawStr(
//                 x + 2,
//                 FOOTER_Y,
//                 String(page).c_str());
//         }

//         x += ITEM_W;
//     }

//     u8g2.setDrawColor(1);
// }
// #endif

// #ifdef MODERN
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

    for (uint8_t i = 1; i <= PAGE_COUNT; ++i)
    {
        if (isPageValid(i))
            pageCount++;
    }

    if (pageCount == 0)
        return;

    // Calculate total width and center it
    const uint8_t totalWidth = pageCount * ITEM_W + (pageCount - 1) * GAP;

    uint8_t x = (FOOTER_W - totalWidth) / 2;

    for (uint8_t i = 1; i <= PAGE_COUNT; ++i)
    {
        if (!isPageValid(i))
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
                String(i).c_str());
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

    u8g2.setDrawColor(1);
}
// #endif

void DisplayHandler::clear()
{
    u8g2.clearBuffer();
}

void DisplayHandler::send()
{
    u8g2.sendBuffer();
}

// void DisplayHandler::drawText(int8_t x, int8_t y, const char *text)
// {
//     u8g2.drawStr(x, y, text);
// }

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
    int8_t width = u8g2.getStrWidth(text);
    int8_t x = (u8g2.getDisplayWidth() - width) / 2;
    u8g2.drawStr(x, y, text);
}

// void DisplayHandler::drawCenteredTextV(int8_t x, const char *text)
// {
//     int8_t ascent = u8g2.getAscent();
//     int8_t descent = u8g2.getDescent();
//     int8_t textHeight = ascent - descent;

//     int8_t y = (u8g2.getDisplayHeight() + textHeight) / 2;

//     u8g2.drawStr(x, y, text);
// }

U8G2_SSD1325_NHD_128X64_F_4W_HW_SPI &DisplayHandler::getU8g2()
{
    return u8g2;
}
