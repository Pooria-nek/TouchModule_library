// #include "BS8112Handler.h"

// /**
//  * @brief Constructor - Instantiates BS8112Handler object
//  */
// BS8112Handler::BS8112Handler(TwoWire &wirePort)
//     : wire_(wirePort),
//       irqFlag_(false)
// {
// }

// BS8112Handler::~BS8112Handler()
// {
// }

// void BS8112Handler::irqTouchHandler()
// {
//     irqFlag_ = true;
// }

// void BS8112Handler::init()
// {
//     _touchState = 0;

//     // BS8112 Initialization
//     uint8_t config[17];
//     uint8_t KeyTriggerthresholdvalue = 12; // 1-32

//     // BS8112 configuration bytes
//     config[0] = 0b00000001;                // B0H: IRQ one-shot enabled
//     config[1] = 0b00000000;                // B1H
//     config[2] = 0x83;                      // B2H
//     config[3] = 0xF3;                      // B3H
//     config[4] = 0b10011000;                // B4H: Powersave
//     config[5] = 0b10011000;                // B5H: Wakeup
//     config[6] = KeyTriggerthresholdvalue;  // B6H K2
//     config[7] = KeyTriggerthresholdvalue;  // B7H K3
//     config[8] = KeyTriggerthresholdvalue;  // B8H K4
//     config[9] = KeyTriggerthresholdvalue;  // B9H K5
//     config[10] = KeyTriggerthresholdvalue; // BAH K6
//     config[11] = KeyTriggerthresholdvalue; // BBH K7
//     config[12] = KeyTriggerthresholdvalue; // BCH K8
//     config[13] = KeyTriggerthresholdvalue; // BDH K9
//     config[14] = KeyTriggerthresholdvalue; // BEH K10
//     config[15] = KeyTriggerthresholdvalue; // BFH K11
//     config[16] = 0b11011000;               // C0H K12 ENABLE IRQ

//     // Calculate checksum for register block transfer
//     uint8_t checksum = 0;
//     for (uint8_t i = 0; i < 17; i++)
//         checksum += config[i];

//     // Write configuration to hardware
//     wire_.beginTransmission(I2C_ADDRESS);
//     wire_.write(0xB0); // Start register
//     for (uint8_t i = 0; i < 17; i++)
//         wire_.write(config[i]);
//     wire_.write(checksum);

//     wire_.endTransmission();
// }

// /**
//  * @brief Polls the hardware for state changes on the configured pads only.
//  * @return true if any key state changed, false otherwise.
//  */
// bool BS8112Handler::update()
// {
//     if (!irqFlag_ && !_runAgain)
//         return false;

//     // Manage IRQ flag for continuous polling if required
//     if (irqFlag_)
//     {
//         irqFlag_ = false;
//         _runAgain = true;
//     }
//     else
//     {
//         _runAgain = false;
//     }

//     // Read 2-byte touch status from the device
//     uint16_t rawState = 0;
//     wire_.beginTransmission(I2C_ADDRESS);
//     wire_.write(0x08);
//     wire_.endTransmission(false);
//     if (wire_.requestFrom(I2C_ADDRESS, (uint8_t)2) == 2)
//     {
//         uint8_t low = wire_.read();
//         uint8_t high = wire_.read();
//         rawState = (static_cast<uint16_t>(high) << 8) | low;
//     }

//     // Remap only the configured physical pads into a compact bitfield,
//     // where bit i of newState corresponds to TOUCH_PADS[i] (not the raw hardware bit position).
//     uint16_t newState = 0;
//     for (uint8_t i = 0; i < TOUCH_CHANNEL_COUNT; i++)
//     {
//         if (rawState & (1 << (touchPins_[i] - 1)))
//             newState |= (1 << i);
//     }

//     // Detect edge transitions
//     bool changed = (newState != _touchState);
//     _prevTouchState = _touchState;
//     _touchState = newState;
//     _pressedEdge = (~_prevTouchState) & _touchState;
//     _releasedEdge = _prevTouchState & (~_touchState);

//     // Update timing for hold detection
//     uint32_t now = millis();

//     if (_pressedEdge != 0)
//         // lastActivityTime_ = now; // any fresh press counts as activity
//         // TODO should link to TouchModule and update last active time

//     for (uint8_t key = 0; key < TOUCH_CHANNEL_COUNT; key++)
//     {
//         if (_pressedEdge & (1 << key))
//         {
//             _lastPressTime[key] = now;
//             _holdActive &= ~(1 << key);

//             if (deviceMode_ == DeviceMode::Sleep)
//             {
//                 // Any touch wakes the device — restores every channel's
//                 // stored high/low state, so skip the acknowledge flash below.
//                 wake();
//             }
//             else if (!keyHigh_[key] && leds_.getLedMode(key) != LedMode::Blinking)
//             {
//                 // Acknowledge the touch with a single flash — but only for
//                 // channels currently "low". A channel already lit "high"
//                 // doesn't need it, and this avoids Blink's auto-return-to-
//                 // Deactive fighting with a channel that should stay high.
//                 // Also skipped while mid-operation (Blinking).
//                 leds_.setLedMode(key, LedMode::Blink);
//             }
//         }
//     }

//     return changed;
// }
