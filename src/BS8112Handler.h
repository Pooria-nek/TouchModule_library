// #ifndef BS8112_H
// #define BS8112_H

// #include <Arduino.h>
// #include <Wire.h>

// #define I2C_ADDRESS 0x50 // i2c addresss of bs8112

// class BS8112Handler
// {
// public:
//     BS8112Handler(TwoWire &wirePort);
//     ~BS8112Handler();

//     void irqTouchHandler();
//     void init();
//     bool update();

// private:
//     TwoWire &wire_;
//     volatile bool irqFlag_;
//     volatile bool _runAgain;

//     // State tracking
//     uint16_t _touchState;     // Bitmask of currently active keys
//     uint16_t _prevTouchState; // Bitmask of keys in previous update cycle
//     uint16_t _pressedEdge;    // Bits set only during the press transition
//     uint16_t _releasedEdge;   // Bits set only during the release transition

//     // Hold/Timing logic
//     uint32_t _lastPressTime[TOUCH_CHANNEL_COUNT];
//     uint16_t _holdActive;
// };

// #endif