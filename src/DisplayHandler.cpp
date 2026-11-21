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
    setFont(1);

    // -------------------------------------------------
    // Startup
    // -------------------------------------------------
    startupAnimation();
}

void DisplayHandler::updateResource(const HVACPanel &hvac)
{
    hvacPanel_ = &hvac;
}

void DisplayHandler::updateResource(const FloorHeatPanel &fheat)
{
    fheatPanel_ = &fheat;
}

void DisplayHandler::updateOutTemprature(const NTC &ntc)
{
    outTemp = ntc.temperature();
}

void DisplayHandler::updateInTemprature(const NTC &ntc)
{
    inTemp = ntc.temperature();
}

void DisplayHandler::setFont(uint8_t font_size)
{
    // u8g2_font_synchronizer_nbp_tr // num only
    // u8g2_font_synchronizer_nbp_tf // full
    // u8g2_font_6x10_tf
    // u8g2_font_open_iconic_all_1x_t
    // u8g2_font_luBS14_tn

    if (font_size == 1)
    {
        u8g2.setFont(u8g2_font_synchronizer_nbp_tf);
    }

    if (font_size == 2)
    {
        u8g2.setFont(u8g2_font_luBS14_tn);
    }

    if (font_size == 3)
    {
        u8g2.setFont(u8g2_font_6x10_tf);
    }

    if (font_size == 4)
    {
        u8g2.setFont(u8g2_font_open_iconic_all_1x_t);
    }

    if (font_size == 5)
    {
        u8g2.setFont(u8g2_font_twelvedings_t_all);
    }
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

    if (devPage_)
    {
        drawDeveloper();
        return;
    }

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
        if (fheatPanel_)
            drawFloorheatPage(*fheatPanel_);
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

bool DisplayHandler::getDeveloper()
{
    return devPage_;
}

void DisplayHandler::setDeveloper(bool state)
{
    devPage_ = state;
}

uint8_t DisplayHandler::getDeveloperPage()
{
    return devPageNum_;
}

void DisplayHandler::setDeveloperPage(uint8_t page)
{
    devPageNum_ = page;
}

void DisplayHandler::drawDeveloper()
{
    switch (devPageNum_)
    {
    case 0:
        /* code */
        return;

    case 1:
        drawSensorCheck();
        return;

    case 2:
        drawAddressCheck();
        return;

        // case 3:
        //     drawSetting();
        //     break;
    }
}

void DisplayHandler::drawSetting()
{
}

void DisplayHandler::drawSensorCheck()
{
}

void DisplayHandler::drawAddressCheck()
{
    // setFont(1);
    // y_t = 0;
    // // Loop over all label/value pairs
    // for (uint8_t i = 0; i < sizeof(values) / sizeof(values[0]); i++)
    // {
    //     strcpy(buf, labels[i]); // copy label
    //     itoa(*values[i], buf + strlen(buf), 10);
    //     u8g2.drawStr(0, y_t, buf); // draw everything

    //     y_t += inc;
    // }

    // // // Read firmware version ON OLED
    // // u8g2.drawStr(20, 102, FW_VERSION);   // draw everything firmware
    // // u8g2.drawStr(0, 116, FW_BUILD_DATE); // draw everything firmware

    // // ---- Draw icons ----
    // setFont(3);

    // u8g2.drawGlyph(0, 96, 115); // Check icon
    // if (address_reload)
    // {
    //     u8g2.drawGlyph(50, 96, 85); // Reload icon
    // }
    // else
    // {
    //     u8g2.drawGlyph(50, 96, 42); // Cross icon
    // }
}

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
    constexpr uint8_t temperatureMode = 1;

    constexpr uint8_t widgetWidth = 58;
    constexpr uint8_t singleHeight = 20;
    constexpr uint8_t dualHeight = 40;

    const uint8_t widgetHeight = (temperatureMode == 2) ? dualHeight : singleHeight;

    x = 5;
    y = random(10, 100 - widgetHeight + 1);

    char buffer[8];

    auto drawTemperature = [&](float temperature, int16_t posX, int16_t posY)
    {
        const int16_t whole = static_cast<int16_t>(temperature);
        const uint8_t decimal =
            static_cast<uint8_t>(fabsf(temperature - whole) * 10.0f);

        snprintf(buffer, sizeof(buffer), "%d.%d", whole, decimal);

        setFont(2);
        u8g2.drawStr(posX, posY, buffer);
    };

    switch (temperatureMode)
    {
    case 0: // Internal
        drawTemperature(inTemp, x, y + 15);

        setFont(3);
        u8g2.drawStr(x + 50, y + 12, "c");
        break;

    case 1: // External
        drawTemperature(outTemp, x, y + 15);

        setFont(3);
        u8g2.drawStr(x + 50, y + 12, "c");
        break;

    case 2: // Both
        drawTemperature(outTemp, x, y + 15);

        // u8g2.drawXBMP(x, y + 19, 13, 13, external_icon);

        drawTemperature(inTemp, x, y + 35);
        break;
    }
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
    setFont(4);
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

void DisplayHandler::drawHvacTemp(uint8_t y, float_t current, float_t sensor)
{
    setFont(3);

    const uint8_t x = 8;
    const uint8_t icon = 13;

    const bool currentValid = current >= 0.0f && current <= 100.0f;
    const bool sensorValid = sensor >= 0.0f && sensor <= 100.0f;

    u8g2.drawXBMP(x, y + 2, icon, icon, internal_icon);
    u8g2.setCursor(30, y + 4);
    if (sensorValid)
        u8g2.print(sensor, 1);
    else
        u8g2.print("--.-");

    u8g2.drawXBMP(x, y + 16, icon, icon, temp_icon);
    u8g2.setCursor(30, y + 18);
    if (currentValid)
        u8g2.print(current, 1);
    else
        u8g2.print("--.-");

    drawChangePointer(y + 11);

    setFont(1);
}

void DisplayHandler::drawTemp(uint8_t y, float_t sensor)
{
    setFont(2);

    const uint8_t x = 6;
    const uint8_t icon = 13;

    char buffer[6];

    int16_t whole = static_cast<int16_t>(sensor);
    uint8_t decimal = static_cast<uint8_t>(fabsf(sensor - whole) * 10.0f);

    snprintf(buffer, sizeof(buffer), "%d.%d", whole, decimal);

    setFont(2);
    u8g2.drawStr(x, y + 4, buffer);

    setFont(3);
    u8g2.drawStr(x + 50, y + 1, "c");

    setFont(1);
}

void DisplayHandler::drawHvacSpeed(uint8_t y, uint8_t speed, uint8_t bar)
{
    if (speed > 3)
        return;

    uint8_t w = 4;
    const uint8_t x = 8;
    const uint8_t gap = 1;

    // if (autos)
    // {
    //     // Speed text
    //     u8g2.drawStr(x + 10, y + 11, hvac_speed[0]);
    // }
    // else
    // {
    // Speed text
    u8g2.drawStr(x + 20, y + 11, hvac_speed[speed]);

    // Speed bars
    if (bar == 0)
    { // LOW
        u8g2.drawFrame(x, y + 16, w, 6);
        u8g2.drawFrame(x + w + gap, y + 12, w, 10);
        u8g2.drawFrame(x + (w + gap) * 2, y + 8, w, 14);
    }
    else if (bar == 1)
    { // HIGH
        u8g2.drawBox(x, y + 16, w, 6);
        u8g2.drawBox(x + w + gap, y + 12, w, 10);
        u8g2.drawBox(x + (w + gap) * 2, y + 8, w, 14);
    }
    else if (bar == 2)
    { // MEDI
        u8g2.drawBox(x, y + 16, w, 6);
        u8g2.drawBox(x + w + gap, y + 12, w, 10);
        u8g2.drawFrame(x + (w + gap) * 2, y + 8, w, 14);
    }
    else if (bar == 3)
    { // LOW
        u8g2.drawBox(x, y + 16, w, 6);
        u8g2.drawFrame(x + w + gap, y + 12, w, 10);
        u8g2.drawFrame(x + (w + gap) * 2, y + 8, w, 14);
    }
    // }

    drawChangePointer(y + 11);
}

#ifdef OLDIE
void DisplayHandler::drawHvacPage(const HVACPanel &hvac)
{
    if (!hvacPanel_)
        return; // update(hvac) hasn't been called yet — nothing to draw

    const HVACPanel::State &state = hvac.currentState();

    uint8_t currentHvac = hvac.currentUnit();

    // put mode icon in left and power in right
    u8g2.setDrawColor(1);

    drawHvacPower(0, state.power ? 1 : 0);

    if (state.power)
    {
        drawHvacMode(0, state.mode);

        float setTemp = 0.0f;

        switch (state.mode)
        {
        case 0: // Cool
            setTemp = state.setTempCool;
            break;

        case 1: // Heat
            setTemp = state.setTempHeat;
            break;

        case 2: // Auto
            setTemp = state.setTempAuto;
            break;
        }

        // top = target (set) temp, bottom = live sensor (current) temp
        drawHvacTemp(30, setTemp, state.currentTemp);

        drawHvacSpeed(60, state.fan, state.autofan);
    }
    else
    {
        drawTemp(50, state.currentTemp);
    }

    // Current HVAC image
    u8g2.drawXBMP(0, 90, 64, 30, hvac.currentImage());

    // u8g2.drawVLine(33, 0, 30);
    // u8g2.drawHLine(0, 30, 64);
    // u8g2.drawHLine(0, 60, 64);
    // u8g2.drawHLine(0, 90, 64);
}
#endif

void DisplayHandler::drawFheatPower(uint8_t y, uint8_t power)
{
    setFont(1);

    const uint8_t x = 33;
    const uint8_t margin = (32 - 16) / 2;

    u8g2.drawXBMP(0, 0, 24, 24, heater_icon);

    u8g2.drawXBMP(x + margin, y, 16, 16, power_icon);
    u8g2.drawStr(x + 8, y + 19, power_texts[power]);
}

void DisplayHandler::drawFheatTemp(uint8_t y, float_t current, float_t sensor)
{
    setFont(3);

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

    setFont(1);
}

void DisplayHandler::drawFheatMode(uint8_t y, uint8_t mode)
{
    setFont(3);

    const uint8_t x = 8;
    const uint8_t icon = 13;

    drawCenteredTextH(y + 11, fh_mode[mode]);

    drawChangePointer(y + 11);

    setFont(1);
}

void DisplayHandler::drawFloorheatPage(const FloorHeatPanel &fheat)
{
    if (!fheatPanel_)
        return; // update(floor heat) hasn't been called yet — nothing to draw

    const FloorHeatPanel::State &state = fheat.currentState();

    // put mode icon in left and power in right
    u8g2.setDrawColor(1);

    drawFheatPower(0, state.power ? 1 : 0);

    if (state.power)
    {
        float setTemp = state.setTemps[state.mode];

        // switch (state.mode)
        // {
        // case 0: // Normal
        //     setTemp = state.setTemps[0];
        //     break;

        // case 1: // Day
        //     setTemp = state.setTempDay;
        //     break;

        // case 2: // Night
        //     setTemp = state.setTempNight;
        //     break;

        // case 3: // Away
        //     setTemp = state.setTempAway;
        //     break;
        // }

        drawFheatTemp(30, setTemp, state.currentTemp);

        drawFheatMode(60, state.mode);
    }

    // Current HVAC image
    u8g2.drawXBMP(0, 90, 64, 30, fheat.currentImage());

    // u8g2.drawHLine(0, 30, 64);
    // u8g2.drawHLine(0, 60, 64);
    // u8g2.drawHLine(0, 90, 64);

    // setFont(4);
}

// music page texts

// void DisplayHandler::drawMusicPage()
// {
//     // u8g2.drawHLine(0, 30, 64);
//     // u8g2.drawHLine(0, 60, 64);
//     // u8g2.drawHLine(0, 90, 64);
//     // drawCenteredText("Music");
//     // setFont(4);

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
//     // setFont(4);

//     u8g2.drawGlyph(4, 66, 213);  // <<
//     u8g2.drawGlyph(52, 66, 214); // >>
//     // setFont(3);
//     drawCenteredTextH(78, "Input-name");
//     // setFont(4);

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

// #ifdef MODERN
void DisplayHandler::drawFooter()
{
    constexpr uint8_t FOOTER_Y = 120;
    constexpr uint8_t FOOTER_H = 8;
    constexpr uint8_t ITEM_W = 8;
    constexpr uint8_t GAP = 0;
    constexpr uint8_t FOOTER_W = 64;

    setFont(1);

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

void DisplayHandler::drawStr(int8_t x, int8_t y, const char *text)
{
    u8g2.drawStr(x, y, text);
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
