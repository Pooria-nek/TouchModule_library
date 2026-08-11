#pragma once

#include <Arduino.h>
#include <string.h>

// Small platform helpers used by TouchModule (and other Buspro subdevices)
// for reading a stable per-chip unique ID and doing basic buffer ops.
//
// mcu::getMcuUID() / mcu::copyMcuUID() read a 12-byte (96-bit) hardware ID:
//  - On STM32 (STM32duino core), this is the factory-programmed 96-bit
//    Unique Device ID, read directly from its fixed memory address
//    (0x1FFF7590 on most STM32 families — double-check your specific
//    series' reference manual if you're on something unusual).
//  - On ESP32, there's no 96-bit ID register, so a 64-bit efuse MAC is
//    used and zero-padded out to MCU_UID_LEN.
//  - On any other core, there's no standard per-chip ID register at a
//    fixed, documented address, so this falls back to a fixed placeholder.
//    That means every board sharing this firmware on an unsupported core
//    will report the *same* "UID" — fine for bench testing, not fine for
//    production. Replace the fallback branch with a real per-unit ID
//    source (e.g. an EEPROM-provisioned serial number) if you need one.

namespace mcu
{
    constexpr size_t MCU_UID_LEN = 12; // bytes

#if defined(ARDUINO_ARCH_STM32)
#ifndef STM32_UID_BASE
#define STM32_UID_BASE 0x1FFF7590UL
#endif
#endif

    // Returns a pointer to a MCU_UID_LEN-byte buffer holding this chip's
    // unique ID. The buffer is owned internally (static, computed once) —
    // copy it out with copyMcuUID()/copyArray() if you need your own copy.
    inline const uint8_t *getMcuUID()
    {
        static uint8_t uid[MCU_UID_LEN];
        static bool initialized = false;

        if (!initialized)
        {
#if defined(ARDUINO_ARCH_STM32)
            memcpy(uid, reinterpret_cast<const void *>(STM32_UID_BASE), MCU_UID_LEN);
#elif defined(ARDUINO_ARCH_ESP32)
            uint64_t chipId = ESP.getEfuseMac();
            memset(uid, 0, MCU_UID_LEN);
            memcpy(uid, &chipId, sizeof(chipId));
#else
            // Placeholder fallback — see file header comment.
            memset(uid, 0xA5, MCU_UID_LEN);
#endif
            initialized = true;
        }

        return uid;
    }

    // Copies the full MCU_UID_LEN-byte unique ID into dest.
    inline void copyMcuUID(uint8_t dest[MCU_UID_LEN])
    {
        memcpy(dest, getMcuUID(), MCU_UID_LEN);
    }

    // Plain memcpy-style helper.
    inline void copyArray(const uint8_t *src, uint8_t *dest, size_t len)
    {
        memcpy(dest, src, len);
    }

    // Plain memcmp-style helper. Defaults to 8 bytes, since that's the
    // length TouchModule uses when matching a bus frame's MAC payload
    // against this device's UID (the Buspro MAC-write frame only carries
    // 8 UID bytes, not the full 12-byte STM32 unique ID). Pass len
    // explicitly (e.g. 12) to compare the full UID, as firstime() does.
    inline bool bufferEquals(const uint8_t *a, const uint8_t *b, size_t len = 8)
    {
        return memcmp(a, b, len) == 0;
    }
}