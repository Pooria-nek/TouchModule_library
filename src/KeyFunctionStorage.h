#pragma once

#include <cstdint>
#include <climits>

template <uint32_t BaseAddress>
class KeyFunctionStorage
{
public:
    static constexpr uint32_t BASE_ADDRESS = BaseAddress;

    static constexpr uint8_t KEY_COUNT = 32;
    static constexpr uint8_t FUNCTION_COUNT = 100;

    static constexpr uint32_t FUNCTION_SIZE = 8;
    static constexpr uint32_t KEY_SIZE = uint32_t(FUNCTION_COUNT) * FUNCTION_SIZE;
    static constexpr uint32_t TOTAL_SIZE = uint32_t(KEY_COUNT) * KEY_SIZE;

    // static constexpr uint8_t FUNCTIONS_PER_HALF = FUNCTION_COUNT / 2;
    // static constexpr uint32_t HALF_SIZE = uint32_t(FUNCTIONS_PER_HALF) * FUNCTION_SIZE;

    static constexpr bool validKey(uint8_t key)
    {
        return key >= 1 && key <= KEY_COUNT;
    }

    // Function indexes: 1–100
    static constexpr bool validFunction(uint8_t function)
    {
        return function >= 1 && function <= FUNCTION_COUNT;
    }

    static constexpr uint32_t keyAddress(uint8_t key)
    {
        if (!validKey(key))
            return UINT32_MAX;

        return BASE_ADDRESS + uint32_t(key - 1) * KEY_SIZE;
    }

    static constexpr uint32_t functionAddress(uint8_t key, uint8_t function)
    {
        if (!validKey(key) || !validFunction(function))
            return UINT32_MAX;

        return keyAddress(key) + uint32_t(function - 1) * FUNCTION_SIZE;
    }

    // // Half 0: indexes 0–49
    // // Half 1: indexes 50–99
    // static constexpr bool validHalf(uint8_t half)
    // {
    //     return half <= 1;
    // }

    // static constexpr uint32_t halfAddress(uint8_t key, uint8_t half)
    // {
    //     if (!validKey(key) || !validHalf(half))
    //         return UINT32_MAX;

    //     return keyAddress(key) + uint32_t(half) * HALF_SIZE;
    // }

    // // Local function index within each half: 0–49
    // static constexpr uint32_t halfFunctionAddress(uint8_t key, uint8_t half, uint8_t function)
    // {
    //     if (!validKey(key) || !validHalf(half) || function >= FUNCTIONS_PER_HALF)
    //     {
    //         return UINT32_MAX;
    //     }

    //     return halfAddress(key, half) + uint32_t(function) * FUNCTION_SIZE;
    // }
};