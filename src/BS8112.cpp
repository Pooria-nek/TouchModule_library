#include "BS8112.h"

namespace
{
    constexpr uint8_t REG_KEY_STATUS = 0x08; // 2 bytes: keys 1-8, keys 9-12
    constexpr uint8_t REG_CONFIG = 0xB0;     // start of the 17-byte config block
    constexpr uint8_t CONFIG_LEN = 17;
}

BS8112::BS8112(TwoWire &wire)
    : wire_(wire)
{
}

void BS8112::mapKeys(const uint8_t *pads, uint8_t count)
{
    keyCount_ = (count > MAX_KEYS) ? MAX_KEYS : count;

    for (uint8_t i = 0; i < MAX_KEYS; i++)
    {
        const bool valid = (i < keyCount_) && pads && pads[i] >= 1 && pads[i] <= MAX_KEYS;
        keyMask_[i] = valid ? (uint16_t(1) << (pads[i] - 1)) : 0;
    }
}

bool BS8112::begin(uint8_t threshold)
{
    state_ = 0;
    prevState_ = 0;
    pressedEdge_ = 0;
    releasedEdge_ = 0;
    holdActive_ = 0;

    if (threshold < 1)
        threshold = 1;
    if (threshold > 32)
        threshold = 32;

    uint8_t config[CONFIG_LEN];

    config[0] = 0b00000001;  // B0H: IRQ one-shot enabled
    config[1] = 0b00000000;  // B1H
    config[2] = 0x83;        // B2H
    config[3] = 0xF3;        // B3H
    config[4] = 0b10011000;  // B4H: Powersave
    config[5] = 0b10011000;  // B5H: Wakeup
    config[6] = threshold;   // B6H K2
    config[7] = threshold;   // B7H K3
    config[8] = threshold;   // B8H K4
    config[9] = threshold;   // B9H K5
    config[10] = threshold;  // BAH K6
    config[11] = threshold;  // BBH K7
    config[12] = threshold;  // BCH K8
    config[13] = threshold;  // BDH K9
    config[14] = threshold;  // BEH K10
    config[15] = threshold;  // BFH K11
    config[16] = 0b11011000; // C0H K12 ENABLE IRQ

    // Checksum for the register block transfer
    uint8_t checksum = 0;
    for (uint8_t i = 0; i < CONFIG_LEN; i++)
        checksum += config[i];

    wire_.beginTransmission(DEFAULT_ADDRESS);
    wire_.write(REG_CONFIG);
    for (uint8_t i = 0; i < CONFIG_LEN; i++)
        wire_.write(config[i]);
    wire_.write(checksum);

    return wire_.endTransmission() == 0;
}

/**
 * Samples the chip when an IRQ is pending (or for the one follow-up read that
 * follows it) and refreshes state / edge / hold bookkeeping.
 * @return true if any key state changed, false otherwise.
 */
bool BS8112::update()
{
    if (!irqFlag_ && !runAgain_)
        return false;

    // The IRQ is one-shot: after it fires, read once more on the next pass to
    // pick up the final state.
    if (irqFlag_)
    {
        irqFlag_ = false;
        runAgain_ = true;
    }
    else
    {
        runAgain_ = false;
    }

    // Read the 2-byte key status
    uint16_t rawState = 0;

    wire_.beginTransmission(DEFAULT_ADDRESS);
    wire_.write(REG_KEY_STATUS);
    wire_.endTransmission(false);

    if (wire_.requestFrom(DEFAULT_ADDRESS, (uint8_t)2) == 2)
    {
        const uint8_t low = wire_.read();
        const uint8_t high = wire_.read();

        rawState = (static_cast<uint16_t>(high) << 8) | low;
    }

    // Remap the mapped chip keys into a compact bitfield
    uint16_t newState = 0;

    for (uint8_t i = 0; i < keyCount_; i++)
    {
        if (rawState & keyMask_[i])
            newState |= (uint16_t(1) << i);
    }

    // Edge detection
    const bool changed = (newState != state_);

    prevState_ = state_;
    state_ = newState;

    pressedEdge_ = (~prevState_) & state_;
    releasedEdge_ = prevState_ & (~state_);

    // Restart hold timing for freshly pressed keys

    if (pressedEdge_ != 0)
    {
        const uint32_t now = millis();

        for (uint8_t key = 0; key < keyCount_; key++)
        {
            if (pressedEdge_ & (uint16_t(1) << key))
            {
                lastPressTime_[key] = now;
                holdActive_ &= ~(uint16_t(1) << key);
            }
        }
    }

    return changed;
}

void BS8112::discardPress()
{
    const uint32_t now = millis();

    pressedEdge_ = 0;
    holdActive_ = 0;

    for (uint8_t key = 0; key < keyCount_; key++)
        lastPressTime_[key] = now;
}

// true for the entire duration the key is held down
bool BS8112::isHold(uint8_t key) const
{
    if (key >= keyCount_)
        return false;
    return (state_ & (uint16_t(1) << key)) != 0;
}

// true once, on the press edge
bool BS8112::isPressed(uint8_t key) const
{
    if (key >= keyCount_)
        return false;
    return (pressedEdge_ & (uint16_t(1) << key)) != 0;
}

// true once, on the release edge
bool BS8112::isReleased(uint8_t key) const
{
    if (key >= keyCount_)
        return false;
    return (releasedEdge_ & (uint16_t(1) << key)) != 0;
}

// true once, the first time a key has been held past the hold time
bool BS8112::isHoldEdge(uint8_t key)
{
    if (key >= keyCount_)
        return false;

    if (isHold(key))
    {
        const uint32_t now = millis();

        if (!(holdActive_ & (uint16_t(1) << key)) && (now - lastPressTime_[key] >= holdTimeMs_))
        {
            holdActive_ |= (uint16_t(1) << key);
            return true;
        }
    }

    return false;
}

uint16_t BS8112::pressedKey()
{
    for (uint8_t key = 0; key < keyCount_; ++key)
    {
        if (pressedEdge_ & (uint16_t(1) << key))
        {
            pressedEdge_ &= ~(uint16_t(1) << key);
            return key;
        }
    }

    return NO_KEY;
}

void BS8112::writeRegister(uint8_t reg, uint8_t value)
{
    wire_.beginTransmission(DEFAULT_ADDRESS);
    wire_.write(reg);
    wire_.write(value);
    wire_.endTransmission();
}

uint8_t BS8112::readRegister(uint8_t reg)
{
    wire_.beginTransmission(DEFAULT_ADDRESS);
    wire_.write(reg);
    wire_.endTransmission(false);

    wire_.requestFrom(DEFAULT_ADDRESS, (uint8_t)1);
    if (wire_.available())
        return wire_.read();

    return 0;
}
