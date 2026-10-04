// #ifndef BS8112_H
// #define BS8112_H

// #include <Arduino.h>
// #include <Wire.h>

// #define BS8112_ADDRESS 0x50 // i2c addresss of bs8112

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
//     volatile bool irqFlag;
//     volatile bool runAgain;

//     // Number of enabled touch pads: 1..12
//     uint8_t touchPadCount;

//     // Physical BS8112 pad mapping
//     uint8_t touchPins_[12];

//     // // State tracking
//     uint16_t touchState; // Bitmask of currently active keys
//     uint16_t prevTouchState; // Bitmask of keys in previous update cycle
//     uint16_t pressedEdge;    // Bits set only during the press transition
//     uint16_t releasedEdge;   // Bits set only during the release transition

//     // // Hold/Timing logic
//     // uint32_t _lastPressTime[TOUCH_CHANNEL_COUNT];
//     // uint16_t _holdActive;
// };

// #endif