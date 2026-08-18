#include <Arduino.h>
#include "APDSHandler.h"

/**
 * @brief Constructor - Instantiates APDSHandler object
 */
APDSHandler::APDSHandler(TwoWire &wirePort)
    : _wire(wirePort),
      _irqFlag(false)
{
}

/**
 * @brief Destructor
 */
APDSHandler::~APDSHandler()
{
}

/**
 * @brief Configures I2C communications and initializes registers to defaults
 *
 * @return True if initialized successfully. False otherwise.
 */
bool APDSHandler::init()
{
    uint8_t id;

    /* Initialize I2C */
    _wire.begin();

    /* Read ID register and check against known values for APDS-9930 */
    if (!wireReadDataByte(APDS9930_ID, id))
    {
        return false;
    }
    if (!(id == APDS9930_ID_1 || id == APDS9930_ID_2))
    {
        return false;
    }

    /* Set ENABLE register to 0 (disable all features) */
    if (!setMode(ALL, OFF))
    {
        return false;
    }

    if (!wireWriteDataByte(APDS9930_PTIME, DEFAULT_PTIME))
    {
        return false;
    }

    /* Set default values for ambient light and proximity registers */
    if (!wireWriteDataByte(APDS9930_ATIME, DEFAULT_ATIME))
    {
        return false;
    }
    if (!wireWriteDataByte(APDS9930_WTIME, DEFAULT_WTIME))
    {
        return false;
    }
    if (!wireWriteDataByte(APDS9930_PPULSE, DEFAULT_PPULSE))
    {
        return false;
    }
    if (!wireWriteDataByte(APDS9930_POFFSET, DEFAULT_POFFSET))
    {
        return false;
    }
    if (!wireWriteDataByte(APDS9930_CONFIG, DEFAULT_CONFIG))
    {
        return false;
    }
    if (!setLEDDrive(DEFAULT_PDRIVE))
    {
        return false;
    }
    if (!setProximityGain(DEFAULT_PGAIN))
    {
        return false;
    }
    if (!setAmbientLightGain(DEFAULT_AGAIN))
    {
        return false;
    }
    if (!setProximityDiode(DEFAULT_PDIODE))
    {
        return false;
    }
    if (!setProximityIntLowThreshold(DEFAULT_PILT))
    {
        return false;
    }
    if (!setProximityIntHighThreshold(DEFAULT_PIHT))
    {
        return false;
    }
    // if (!setLightIntLowThreshold(DEFAULT_AILT))
    // {
    //     return false;
    // }
    // if (!setLightIntHighThreshold(DEFAULT_AIHT))
    // {
    //     return false;
    // }
    if (!wireWriteDataByte(APDS9930_PERS, DEFAULT_PERS))
    {
        return false;
    }

    return true;
}

void APDSHandler::irqHandler()
{
    _irqFlag = true;
}

void APDSHandler::update()
{
    if (!_irqFlag)
        return;

    // Clear hardware interrupt — safe to do here (not inside ISR)
    if (!clearAllInts())
    {
        // Optional: log or set an error flag
        // Serial.println("APDS: failed to clear interrupt!");
    }

    // Reset software flag
    _irqFlag = false;

    // // Optional: read fresh sensor values
    // // (If you want auto-updating)
    // uint16_t prox;
    // unsigned long lux;

    // if (readProximity(prox))
    //     _lastProximity = prox;

    // if (readAmbientLightLux(lux))
    //     _lastLux = lux;
}

uint16_t APDSHandler::getProximity()
{
    return _lastProximity;
}

unsigned long APDSHandler::getAmbientLightLux()
{
    return _lastLux;
}

/*******************************************************************************
 * Public methods for controlling the APDS-9930
 ******************************************************************************/

/**
 * @brief Reads and returns the contents of the ENABLE register
 *
 * @return Contents of the ENABLE register. 0xFF if error.
 */
uint8_t APDSHandler::getMode()
{
    uint8_t enable_value;

    /* Read current ENABLE register */
    if (!wireReadDataByte(APDS9930_ENABLE, enable_value))
    {
        return ERROR;
    }

    return enable_value;
}

/**
 * @brief Enables or disables a feature in the APDS-9930
 *
 * @param[in] mode which feature to enable
 * @param[in] enable ON (1) or OFF (0)
 * @return True if operation success. False otherwise.
 */
bool APDSHandler::setMode(uint8_t mode, uint8_t enable)
{
    uint8_t reg_val;

    /* Read current ENABLE register */
    reg_val = getMode();
    if (reg_val == ERROR)
    {
        return false;
    }

    /* Change bit(s) in ENABLE register */
    enable = enable & 0x01;
    if (mode <= 6)
    {
        if (enable)
        {
            reg_val |= (1 << mode);
        }
        else
        {
            reg_val &= ~(1 << mode);
        }
    }
    else if (mode == ALL)
    {
        if (enable)
        {
            reg_val = 0x7F;
        }
        else
        {
            reg_val = 0x00;
        }
    }

    /* Write value back to ENABLE register */
    if (!wireWriteDataByte(APDS9930_ENABLE, reg_val))
    {
        return false;
    }

    return true;
}

/**
 * @brief Starts the light (Ambient/IR) sensor on the APDS-9930
 *
 * @param[in] interrupts true to enable hardware interrupt on high or low light
 * @return True if sensor enabled correctly. False on error.
 */
bool APDSHandler::enableLightSensor(bool interrupts)
{

    /* Set default gain, interrupts, enable power, and enable sensor */
    if (!setAmbientLightGain(DEFAULT_AGAIN))
    {
        return false;
    }
    if (interrupts)
    {
        if (!setAmbientLightIntEnable(1))
        {
            return false;
        }
    }
    else
    {
        if (!setAmbientLightIntEnable(0))
        {
            return false;
        }
    }
    if (!enablePower())
    {
        return false;
    }
    if (!setMode(AMBIENT_LIGHT, 1))
    {
        return false;
    }

    return true;
}

/**
 * @brief Ends the light sensor on the APDS-9930
 *
 * @return True if sensor disabled correctly. False on error.
 */
bool APDSHandler::disableLightSensor()
{
    if (!setAmbientLightIntEnable(0))
    {
        return false;
    }
    if (!setMode(AMBIENT_LIGHT, 0))
    {
        return false;
    }

    return true;
}

/**
 * @brief Starts the proximity sensor on the APDS-9930
 *
 * @param[in] interrupts true to enable hardware external interrupt on proximity
 * @return True if sensor enabled correctly. False on error.
 */
bool APDSHandler::enableProximitySensor(bool interrupts)
{
    /* Set default gain, LED, interrupts, enable power, and enable sensor */
    if (!setProximityGain(DEFAULT_PGAIN))
    {
        return false;
    }
    if (!setLEDDrive(DEFAULT_PDRIVE))
    {
        return false;
    }
    if (interrupts)
    {
        if (!setProximityIntEnable(1))
        {
            return false;
        }
    }
    else
    {
        if (!setProximityIntEnable(0))
        {
            return false;
        }
    }
    if (!enablePower())
    {
        return false;
    }
    if (!setMode(PROXIMITY, 1))
    {
        return false;
    }

    return true;
}

/**
 * @brief Ends the proximity sensor on the APDS-9930
 *
 * @return True if sensor disabled correctly. False on error.
 */
bool APDSHandler::disableProximitySensor()
{
    if (!setProximityIntEnable(0))
    {
        return false;
    }
    if (!setMode(PROXIMITY, 0))
    {
        return false;
    }

    return true;
}

/**
 * Turn the APDS-9930 on
 *
 * @return True if operation successful. False otherwise.
 */
bool APDSHandler::enablePower()
{
    if (!setMode(POWER, 1))
    {
        return false;
    }

    return true;
}

/**
 * Turn the APDS-9930 off
 *
 * @return True if operation successful. False otherwise.
 */
bool APDSHandler::disablePower()
{
    if (!setMode(POWER, 0))
    {
        return false;
    }

    return true;
}

/*******************************************************************************
 * Ambient light sensor controls
 ******************************************************************************/

bool APDSHandler::readAmbientLightLux(unsigned long &val)
{
    uint16_t Ch0;
    uint16_t Ch1;

    /* Read value from channel 0 */
    if (!readCh0Light(Ch0))
    {
        return false;
    }

    /* Read value from channel 1 */
    if (!readCh1Light(Ch1))
    {
        return false;
    }

    val = ulongAmbientToLux(Ch0, Ch1);
    return true;
}

float APDSHandler::floatAmbientToLux(uint16_t Ch0, uint16_t Ch1)
{
    uint8_t x[4] = {1, 8, 16, 120};
    float ALSIT = 2.73 * (256 - DEFAULT_ATIME);
    float iac = max(Ch0 - ALS_B * Ch1, ALS_C * Ch0 - ALS_D * Ch1);
    if (iac < 0)
        iac = 0;
    float lpc = GA * DF / (ALSIT * x[getAmbientLightGain()]);
    return iac * lpc;
}

unsigned long APDSHandler::ulongAmbientToLux(uint16_t Ch0, uint16_t Ch1)
{
    uint8_t x[4] = {1, 8, 16, 120};
    float ALSIT = 2.73f * (256 - DEFAULT_ATIME);
    float iac = max(Ch0 - ALS_B * Ch1, ALS_C * Ch0 - ALS_D * Ch1);

    if (iac < 0)
        iac = 0;

    float lpc = GA * DF / (ALSIT * x[getAmbientLightGain()]);
    return (unsigned long)(iac * lpc);
}

bool APDSHandler::readCh0Light(uint16_t &val)
{
    uint8_t val_byte;
    val = 0;

    /* Read value from channel 0 */
    if (!wireReadDataByte(APDS9930_Ch0DATAL, val_byte))
    {
        return false;
    }
    val = val_byte;
    if (!wireReadDataByte(APDS9930_Ch0DATAH, val_byte))
    {
        return false;
    }
    val += ((uint16_t)val_byte << 8);
    return true;
}

bool APDSHandler::readCh1Light(uint16_t &val)
{
    uint8_t val_byte;
    val = 0;

    /* Read value from channel 0 */
    if (!wireReadDataByte(APDS9930_Ch1DATAL, val_byte))
    {
        return false;
    }
    val = val_byte;
    if (!wireReadDataByte(APDS9930_Ch1DATAH, val_byte))
    {
        return false;
    }
    val += ((uint16_t)val_byte << 8);
    return true;
}

/*******************************************************************************
 * Proximity sensor controls
 ******************************************************************************/

/**
 * @brief Reads the proximity level as an 8-bit value
 *
 * @param[out] val value of the proximity sensor.
 * @return True if operation successful. False otherwise.
 */
bool APDSHandler::readProximity(uint16_t &val)
{
    val = 0;
    uint8_t val_byte;

    /* Read value from proximity data register */
    if (!wireReadDataByte(APDS9930_PDATAL, val_byte))
    {
        return false;
    }
    val = val_byte;
    if (!wireReadDataByte(APDS9930_PDATAH, val_byte))
    {
        return false;
    }
    val += ((uint16_t)val_byte << 8);

    return true;
}

/*******************************************************************************
 * Getters and setters for register values
 ******************************************************************************/

/**
 * @brief Returns the lower threshold for proximity detection
 *
 * @return lower threshold
 */
uint16_t APDSHandler::getProximityIntLowThreshold()
{
    uint16_t val;
    uint8_t val_byte;

    /* Read value from PILT register */
    if (!wireReadDataByte(APDS9930_PILTL, val_byte))
    {
        val = 0;
    }
    val = val_byte;
    if (!wireReadDataByte(APDS9930_PILTH, val_byte))
    {
        val = 0;
    }
    val |= ((uint16_t)val_byte << 8);

    return val;
}

/**
 * @brief Sets the lower threshold for proximity detection
 *
 * @param[in] threshold the lower proximity threshold
 * @return True if operation successful. False otherwise.
 */
bool APDSHandler::setProximityIntLowThreshold(uint16_t threshold)
{
    uint8_t lo;
    uint8_t hi;
    hi = threshold >> 8;
    lo = threshold & 0x00FF;

    if (!wireWriteDataByte(APDS9930_PILTL, lo))
    {
        return false;
    }
    if (!wireWriteDataByte(APDS9930_PILTH, hi))
    {
        return false;
    }

    return true;
}

/**
 * @brief Returns the high threshold for proximity detection
 *
 * @return high threshold
 */
uint16_t APDSHandler::getProximityIntHighThreshold()
{
    uint16_t val;
    uint8_t val_byte;

    /* Read value from PILT register */
    if (!wireReadDataByte(APDS9930_PIHTL, val_byte))
    {
        val = 0;
    }
    val = val_byte;
    if (!wireReadDataByte(APDS9930_PIHTH, val_byte))
    {
        val = 0;
    }
    val |= ((uint16_t)val_byte << 8);

    return val;
}

/**
 * @brief Sets the high threshold for proximity detection
 *
 * @param[in] threshold the high proximity threshold
 * @return True if operation successful. False otherwise.
 */
bool APDSHandler::setProximityIntHighThreshold(uint16_t threshold)
{
    uint8_t lo;
    uint8_t hi;
    hi = threshold >> 8;
    lo = threshold & 0x00FF;

    if (!wireWriteDataByte(APDS9930_PIHTL, lo))
    {
        return false;
    }
    if (!wireWriteDataByte(APDS9930_PIHTH, hi))
    {
        return false;
    }

    return true;
}

/**
 * @brief Returns LED drive strength for proximity and ALS
 *
 * Value    LED Current
 *   0        100 mA
 *   1         50 mA
 *   2         25 mA
 *   3       12.5 mA
 *
 * @return the value of the LED drive strength. 0xFF on failure.
 */
uint8_t APDSHandler::getLEDDrive()
{
    uint8_t val;

    /* Read value from CONTROL register */
    if (!wireReadDataByte(APDS9930_CONTROL, val))
    {
        return ERROR;
    }

    /* Shift and mask out LED drive bits */
    val = (val >> 6) & 0b00000011;

    return val;
}

/**
 * @brief Sets the LED drive strength for proximity and ALS
 *
 * Value    LED Current
 *   0        100 mA
 *   1         50 mA
 *   2         25 mA
 *   3       12.5 mA
 *
 * @param[in] drive the value (0-3) for the LED drive strength
 * @return True if operation successful. False otherwise.
 */
bool APDSHandler::setLEDDrive(uint8_t drive)
{
    uint8_t val;

    /* Read value from CONTROL register */
    if (!wireReadDataByte(APDS9930_CONTROL, val))
    {
        return false;
    }

    /* Set bits in register to given value */
    drive &= 0b00000011;
    drive = drive << 6;
    val &= 0b00111111;
    val |= drive;

    /* Write register value back into CONTROL register */
    if (!wireWriteDataByte(APDS9930_CONTROL, val))
    {
        return false;
    }

    return true;
}

/**
 * @brief Returns receiver gain for proximity detection
 *
 * Value    Gain
 *   0       1x
 *   1       2x
 *   2       4x
 *   3       8x
 *
 * @return the value of the proximity gain. 0xFF on failure.
 */
uint8_t APDSHandler::getProximityGain()
{
    uint8_t val;

    /* Read value from CONTROL register */
    if (!wireReadDataByte(APDS9930_CONTROL, val))
    {
        return ERROR;
    }

    /* Shift and mask out PDRIVE bits */
    val = (val >> 2) & 0b00000011;

    return val;
}

/**
 * @brief Sets the receiver gain for proximity detection
 *
 * Value    Gain
 *   0       1x
 *   1       2x
 *   2       4x
 *   3       8x
 *
 * @param[in] drive the value (0-3) for the gain
 * @return True if operation successful. False otherwise.
 */
bool APDSHandler::setProximityGain(uint8_t drive)
{
    uint8_t val;

    /* Read value from CONTROL register */
    if (!wireReadDataByte(APDS9930_CONTROL, val))
    {
        return false;
    }

    /* Set bits in register to given value */
    drive &= 0b00000011;
    drive = drive << 2;
    val &= 0b11110011;
    val |= drive;

    /* Write register value back into CONTROL register */
    if (!wireWriteDataByte(APDS9930_CONTROL, val))
    {
        return false;
    }

    return true;
}

/**
 * @brief Returns the proximity diode
 *
 * Value    Diode selection
 *   0       Reserved
 *   1       Reserved
 *   2       Use Ch1 diode
 *   3       Reserved
 *
 * @return the selected diode. 0xFF on failure.
 */
uint8_t APDSHandler::getProximityDiode()
{
    uint8_t val;

    /* Read value from CONTROL register */
    if (!wireReadDataByte(APDS9930_CONTROL, val))
    {
        return ERROR;
    }

    /* Shift and mask out PDRIVE bits */
    val = (val >> 4) & 0b00000011;

    return val;
}

/**
 * @brief Selects the proximity diode
 *
 * Value    Diode selection
 *   0       Reserved
 *   1       Reserved
 *   2       Use Ch1 diode
 *   3       Reserved
 *
 * @param[in] drive the value (0-3) for the diode
 * @return True if operation successful. False otherwise.
 */
bool APDSHandler::setProximityDiode(uint8_t drive)
{
    uint8_t val;

    /* Read value from CONTROL register */
    if (!wireReadDataByte(APDS9930_CONTROL, val))
    {
        return false;
    }

    /* Set bits in register to given value */
    drive &= 0b00000011;
    drive = drive << 4;
    val &= 0b11001111;
    val |= drive;

    /* Write register value back into CONTROL register */
    if (!wireWriteDataByte(APDS9930_CONTROL, val))
    {
        return false;
    }

    return true;
}

/**
 * @brief Returns receiver gain for the ambient light sensor (ALS)
 *
 * Value    Gain
 *   0        1x
 *   1        8x
 *   2       16x
 *   3      120x
 *
 * @return the value of the ALS gain. 0xFF on failure.
 */
uint8_t APDSHandler::getAmbientLightGain()
{
    uint8_t val;

    /* Read value from CONTROL register */
    if (!wireReadDataByte(APDS9930_CONTROL, val))
    {
        return ERROR;
    }

    /* Shift and mask out ADRIVE bits */
    val &= 0b00000011;

    return val;
}

/**
 * @brief Sets the receiver gain for the ambient light sensor (ALS)
 *
 * Value    Gain
 *   0        1x
 *   1        8x
 *   2       16x
 *   3       64x
 *
 * @param[in] drive the value (0-3) for the gain
 * @return True if operation successful. False otherwise.
 */
bool APDSHandler::setAmbientLightGain(uint8_t drive)
{
    uint8_t val;

    /* Read value from CONTROL register */
    if (!wireReadDataByte(APDS9930_CONTROL, val))
    {
        return false;
    }

    /* Set bits in register to given value */
    drive &= 0b00000011;
    val &= 0b11111100;
    val |= drive;

    /* Write register value back into CONTROL register */
    if (!wireWriteDataByte(APDS9930_CONTROL, val))
    {
        return false;
    }

    return true;
}

/*******************************************************************************
 * Interupts value handler
 ******************************************************************************/

/**
 * @brief Gets the low threshold for ambient light interrupts
 *
 * @param[out] threshold current low threshold stored on the APDS-9930
 * @return True if operation successful. False otherwise.
 */
bool APDSHandler::getLightIntLowThreshold(uint16_t &threshold)
{
    uint8_t val_byte;
    threshold = 0;

    /* Read value from ambient light low threshold, low byte register */
    if (!wireReadDataByte(APDS9930_AILTL, val_byte))
    {
        return false;
    }
    threshold = val_byte;

    /* Read value from ambient light low threshold, high byte register */
    if (!wireReadDataByte(APDS9930_AILTH, val_byte))
    {
        return false;
    }
    threshold = threshold + ((uint16_t)val_byte << 8);

    return true;
}

/**
 * @brief Sets the low threshold for ambient light interrupts
 *
 * @param[in] threshold low threshold value for interrupt to trigger
 * @return True if operation successful. False otherwise.
 */
bool APDSHandler::setLightIntLowThreshold(uint16_t threshold)
{
    uint8_t val_low;
    uint8_t val_high;

    /* Break 16-bit threshold into 2 8-bit values */
    val_low = threshold & 0x00FF;
    val_high = (threshold & 0xFF00) >> 8;

    /* Write low byte */
    if (!wireWriteDataByte(APDS9930_AILTL, val_low))
    {
        return false;
    }

    /* Write high byte */
    if (!wireWriteDataByte(APDS9930_AILTH, val_high))
    {
        return false;
    }

    return true;
}

/**
 * @brief Gets the high threshold for ambient light interrupts
 *
 * @param[out] threshold current low threshold stored on the APDS-9930
 * @return True if operation successful. False otherwise.
 */
bool APDSHandler::getLightIntHighThreshold(uint16_t &threshold)
{
    uint8_t val_byte;
    threshold = 0;

    /* Read value from ambient light high threshold, low byte register */
    if (!wireReadDataByte(APDS9930_AIHTL, val_byte))
    {
        return false;
    }
    threshold = val_byte;

    /* Read value from ambient light high threshold, high byte register */
    if (!wireReadDataByte(APDS9930_AIHTH, val_byte))
    {
        return false;
    }
    threshold = threshold + ((uint16_t)val_byte << 8);

    return true;
}

/**
 * @brief Sets the high threshold for ambient light interrupts
 *
 * @param[in] threshold high threshold value for interrupt to trigger
 * @return True if operation successful. False otherwise.
 */
bool APDSHandler::setLightIntHighThreshold(uint16_t threshold)
{
    uint8_t val_low;
    uint8_t val_high;

    /* Break 16-bit threshold into 2 8-bit values */
    val_low = threshold & 0x00FF;
    val_high = (threshold & 0xFF00) >> 8;

    /* Write low byte */
    if (!wireWriteDataByte(APDS9930_AIHTL, val_low))
    {
        return false;
    }

    /* Write high byte */
    if (!wireWriteDataByte(APDS9930_AIHTH, val_high))
    {
        return false;
    }

    return true;
}

/**
 * @brief Gets if ambient light interrupts are enabled or not
 *
 * @return 1 if interrupts are enabled, 0 if not. 0xFF on error.
 */
uint8_t APDSHandler::getAmbientLightIntEnable()
{
    uint8_t val;

    /* Read value from ENABLE register */
    if (!wireReadDataByte(APDS9930_ENABLE, val))
    {
        return ERROR;
    }

    /* Shift and mask out AIEN bit */
    val = (val >> 4) & 0b00000001;

    return val;
}

/**
 * @brief Turns ambient light interrupts on or off
 *
 * @param[in] enable 1 to enable interrupts, 0 to turn them off
 * @return True if operation successful. False otherwise.
 */
bool APDSHandler::setAmbientLightIntEnable(uint8_t enable)
{
    uint8_t val;

    /* Read value from ENABLE register */
    if (!wireReadDataByte(APDS9930_ENABLE, val))
    {
        return false;
    }

    /* Set bits in register to given value */
    enable &= 0b00000001;
    enable = enable << 4;
    val &= 0b11101111;
    val |= enable;

    /* Write register value back into ENABLE register */
    if (!wireWriteDataByte(APDS9930_ENABLE, val))
    {
        return false;
    }

    return true;
}

/**
 * @brief Gets if proximity interrupts are enabled or not
 *
 * @return 1 if interrupts are enabled, 0 if not. 0xFF on error.
 */
uint8_t APDSHandler::getProximityIntEnable()
{
    uint8_t val;

    /* Read value from ENABLE register */
    if (!wireReadDataByte(APDS9930_ENABLE, val))
    {
        return ERROR;
    }

    /* Shift and mask out PIEN bit */
    val = (val >> 5) & 0b00000001;

    return val;
}

/**
 * @brief Turns proximity interrupts on or off
 *
 * @param[in] enable 1 to enable interrupts, 0 to turn them off
 * @return True if operation successful. False otherwise.
 */
bool APDSHandler::setProximityIntEnable(uint8_t enable)
{
    uint8_t val;

    /* Read value from ENABLE register */
    if (!wireReadDataByte(APDS9930_ENABLE, val))
    {
        return false;
    }

    /* Set bits in register to given value */
    enable &= 0b00000001;
    enable = enable << 5;
    val &= 0b11011111;
    val |= enable;

    /* Write register value back into ENABLE register */
    if (!wireWriteDataByte(APDS9930_ENABLE, val))
    {
        return false;
    }

    return true;
}

/*******************************************************************************
 * Interupts cleaner handler
 ******************************************************************************/

/**
 * @brief Clears the ambient light interrupt
 *
 * @return True if operation completed successfully. False otherwise.
 */
bool APDSHandler::clearAmbientLightInt()
{
    if (!wireWriteByte(CLEAR_ALS_INT))
    {
        return false;
    }

    return true;
}

/**
 * @brief Clears the proximity interrupt
 *
 * @return True if operation completed successfully. False otherwise.
 */
bool APDSHandler::clearProximityInt()
{
    if (!wireWriteByte(CLEAR_PROX_INT))
    {
        return false;
    }

    return true;
}

/**
 * @brief Clears all interrupts
 *
 * @return True if operation completed successfully. False otherwise.
 */
bool APDSHandler::clearAllInts()
{
    if (!wireWriteByte(CLEAR_ALL_INTS))
    {
        return false;
    }

    return true;
}

/*******************************************************************************
 * Raw I2C Reads and Writes
 ******************************************************************************/

/**
 * @brief Writes a single byte to the I2C device (no register)
 *
 * @param[in] val the 1-byte value to write to the I2C device
 * @return True if successful write operation. False otherwise.
 */
bool APDSHandler::wireWriteByte(uint8_t val)
{
    _wire.beginTransmission(APDS9930_I2C_ADDR);
    _wire.write(val);
    if (_wire.endTransmission() != 0)
    {
        return false;
    }

    return true;
}

/**
 * @brief Writes a single byte to the I2C device and specified register
 *
 * @param[in] reg the register in the I2C device to write to
 * @param[in] val the 1-byte value to write to the I2C device
 * @return True if successful write operation. False otherwise.
 */
bool APDSHandler::wireWriteDataByte(uint8_t reg, uint8_t val)
{
    _wire.beginTransmission(APDS9930_I2C_ADDR);
    _wire.write(reg | AUTO_INCREMENT);
    _wire.write(val);
    if (_wire.endTransmission() != 0)
    {
        return false;
    }

    return true;
}

/**
 * @brief Writes a block (array) of bytes to the I2C device and register
 *
 * @param[in] reg the register in the I2C device to write to
 * @param[in] val pointer to the beginning of the data byte array
 * @param[in] len the length (in bytes) of the data to write
 * @return True if successful write operation. False otherwise.
 */
bool APDSHandler::wireWriteDataBlock(uint8_t reg, uint8_t *val, unsigned int len)
{
    unsigned int i;

    _wire.beginTransmission(APDS9930_I2C_ADDR);
    _wire.write(reg | AUTO_INCREMENT);

    for (i = 0; i < len; i++)
    {
        _wire.write(val[i]);
    }

    return (_wire.endTransmission() == 0);
}

/**
 * @brief Reads a single byte from the I2C device and specified register
 *
 * @param[in] reg the register to read from
 * @param[out] the value returned from the register
 * @return True if successful read operation. False otherwise.
 */
bool APDSHandler::wireReadDataByte(uint8_t reg, uint8_t &val)
{

    /* Indicate which register we want to read from */
    if (!wireWriteByte(reg | AUTO_INCREMENT))
    {
        return false;
    }

    /* Read from register */
    uint8_t received = _wire.requestFrom(APDS9930_I2C_ADDR, (uint8_t)1);
    if (received != 1)
    {
        return false;
    }

    if (_wire.available())
    {
        val = _wire.read();
        return true;
    }

    return false;
}

/**
 * @brief Reads a block (array) of bytes from the I2C device and register
 *
 * @param[in] reg the register to read from
 * @param[out] val pointer to the beginning of the data
 * @param[in] len number of bytes to read
 * @return Number of bytes read. -1 on read error.
 */
int APDSHandler::wireReadDataBlock(uint8_t reg, uint8_t *val, unsigned int len)
{
    /* Indicate which register we want to read from */
    if (!wireWriteByte(reg | AUTO_INCREMENT))
    {
        return -1;
    }

    /* Read block data */
    uint8_t received = _wire.requestFrom(APDS9930_I2C_ADDR, (uint8_t)len);
    if (received == 0)
    {
        return -1;
    }

    unsigned int i = 0;
    while (_wire.available() && i < len)
    {
        val[i++] = _wire.read();
    }

    return i;
}