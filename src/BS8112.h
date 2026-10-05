#pragma once

#include <Arduino.h>
#include <Wire.h>

#include <stdint.h>

/**
 * Driver for the Holtek BS8112 12-key capacitive touch controller (I2C).
 *
 * Responsibilities
 *   - write the key configuration block (thresholds, IRQ mode, checksum)
 *   - read the key status register when the IRQ line says something changed
 *   - remap the chip's physical key numbers into a compact 0..N-1 bitfield
 *   - provide edge (pressed / released) and hold detection
 *
 * It knows nothing about LEDs, sleep, pages or the bus protocol — the owner
 * decides what a touch *means*.
 *
 * Typical use
 *
 *     BS8112 touch(Wire);
 *     touch.mapKeys(pads, padCount);          // pads[i] = chip key (1..12) for logical key i
 *     touch.begin();                          // after Wire.begin()
 *     attachInterrupt(pin, []{ touch.irq(); }, FALLING);
 *
 *     loop():
 *         touch.update();                     // cheap when nothing happened
 *         if (touch.isPressed(0)) { ... }
 *
 * Edges live for one update() pass that actually sampled the chip. Because the
 * IRQ is one-shot, update() samples twice per touch event (the IRQ, then one
 * follow-up read to catch the final state), so edges are visible for one loop.
 */
class BS8112
{
public:
    static constexpr uint8_t DEFAULT_ADDRESS = 0x50;
    static constexpr uint8_t MAX_KEYS = 12;
    static constexpr uint8_t DEFAULT_THRESHOLD = 7; // valid range 1..32
    static constexpr uint32_t DEFAULT_HOLD_MS = 1000;
    static constexpr uint16_t NO_KEY = UINT16_MAX;

    explicit BS8112(TwoWire &wire);

    // ---- setup ---------------------------------------------------------

    // pads[i] is the chip key number (1..12) wired to logical key i.
    // Entries outside 1..12 are accepted but never read as pressed.
    // At most MAX_KEYS entries are used.
    void mapKeys(const uint8_t *pads, uint8_t count);

    void setHoldTime(uint32_t ms) { holdTimeMs_ = ms; }
    uint32_t getHoldTime() const { return holdTimeMs_; }

    // Writes the configuration block. Call after Wire.begin().
    // Returns false if the chip did not ACK.
    bool begin(uint8_t threshold = DEFAULT_THRESHOLD);

    // ---- interrupt -----------------------------------------------------

    void irq() { irqFlag_ = true; } // call from the GPIO ISR
    bool irqPending() const { return irqFlag_; }

    // ---- polling -------------------------------------------------------

    // Call every loop(). Reads the chip only when an IRQ is pending (or for the
    // one follow-up read after it). Returns true if the key state changed.
    bool update();

    // ---- state ---------------------------------------------------------

    uint8_t keyCount() const { return keyCount_; }

    uint16_t state() const { return state_; }                // keys currently down
    uint16_t pressedEdges() const { return pressedEdge_; }   // down this sample
    uint16_t releasedEdges() const { return releasedEdge_; } // up this sample

    bool isHold(uint8_t key) const;     // true for as long as the key is down
    bool isPressed(uint8_t key) const;  // true only on the press edge
    bool isReleased(uint8_t key) const; // true only on the release edge
    bool isHoldEdge(uint8_t key);       // true once when held past the hold time

    // Pops the lowest pending press edge. Returns NO_KEY when there is none.
    uint16_t pressedKey();

    // Drops any pending press and restarts hold timing for every key. Use it
    // when a touch was only meant to wake the device and must not also act.
    void discardPress();

    // ---- raw access ----------------------------------------------------

    void writeRegister(uint8_t reg, uint8_t value);
    uint8_t readRegister(uint8_t reg);

private:
    TwoWire &wire_;
    uint8_t address_;

    volatile bool irqFlag_ = false;
    volatile bool runAgain_ = false;

    uint8_t keyCount_ = 0;
    uint16_t keyMask_[MAX_KEYS] = {0}; // chip status bit for each logical key

    uint16_t state_ = 0;
    uint16_t prevState_ = 0;
    uint16_t pressedEdge_ = 0;
    uint16_t releasedEdge_ = 0;

    uint32_t holdTimeMs_ = DEFAULT_HOLD_MS;
    uint32_t lastPressTime_[MAX_KEYS] = {0};
    uint16_t holdActive_ = 0;
};
