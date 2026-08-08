# TouchModule

A touch subdevice library for an **HDL-Buspro-style RS485 bus**, built for STM32duino/Arduino platforms. It drives a **BS8112 capacitive touch controller** over I²C, translates raw touch events into press / release / hold state, and speaks the Buspro protocol for device discovery, addressing, and remarks over the bus.

> ⚠️ **Status: early / in-progress (v0.0.1).** Several declared methods (scene management, zone handling, direct output control) are not yet implemented — see [Development Status](#development-status) below.

## Features

- Wraps a **BS8112** capacitive touch IC (I²C address `0x50`) — configures key trigger thresholds and reads the 12-bit touch status register.
- Debounced touch state machine per channel:
  - `isTouched()` — currently held
  - `isPressed()` / `isReleased()` — single-shot edge detection
  - `isHold()` — held past a configurable hold time (`TOUCH_HOLD_TIME`, default 1000 ms)
- **Per-channel LED status indicator** ([`LedHandler.h`](./LedHandler.h) — a standalone, reusable class), driven as a non-blocking state machine with real (software) brightness control:
  - `Deactive` — off
  - `Active` — steady on, brightness configurable
  - `Blink` — single blink (quick acknowledge, fired automatically on touch press)
  - `Blinking` — blinks continuously until you call `finishOperation()` once the underlying action completes
  - Brightness is 32-level software PWM driven by a TIM3 timer interrupt at a 100 Hz refresh rate (STM32; other cores currently fall back to plain on/off — see [DOCUMENTATION.md](./DOCUMENTATION.md))
  - Includes a small blocking `startupAnimation()` boot sequence
- Configurable channel count (1–12, plus AC/DLP panel presets) via compile-time macros, each mapped to the correct **HDL Buspro device type code**.
- Persists device identity (MAC/UID, bus address, remark strings, hardware/firmware version) to flash via `MemoryCore`.
- Handles core Buspro "universal" requests out of the box: device search, firmware/hardware version read, find-device (identify), MAC address read/write, device remark read/write.

## Dependencies

This library does not stand alone — it expects the following sibling libraries to be available (per `library.json` / `library.properties`):

| Library | Purpose |
|---|---|
| `BusproCore` (`^0.1.0`) | Provides `BusproFrame`, `BusproTransport`, `BusproDevice`, and the `BusproOp` opcode table used to parse/build bus frames. |
| `MemoryCore` (`^0.1.0`) | Flash-backed key/value storage (`read`, `readObject`, `write`, `writeObject`, `updateObject`, `findAdrress`) used to persist device configuration. |

Also requires the Arduino `Wire` library (I²C) for talking to the BS8112.

## Installation

**Arduino IDE**
1. Download this repository as a `.zip`.
2. Sketch → Include Library → Add .ZIP Library…
3. Install `BusproCore` and `MemoryCore` the same way (required dependencies).

**PlatformIO**

```ini
lib_deps =
    https://github.com/Pooria-nek/TouchModule_library.git
```

(`BusproCore` and `MemoryCore` must also be added as `lib_deps`.)

## Selecting the channel count

The number of touch channels — and the matching HDL device-type code sent on the bus — is chosen at compile time in `TouchModule.h` via one of these defines:

```cpp
#define TOUCH1     // 1 channel
#define TOUCH2     // 2 channels
#define TOUCH3     // 3 channels
#define TOUCH4     // 4 channels   <-- currently active default
#define TOUCH5     // 5 channels
#define TOUCH6     // 6 channels
#define TOUCH8     // 8 channels
#define TOUCH12    // 12 channels
#define AC_PANEL   // 24-channel AC panel preset
#define DLP_PANEL  // 24-channel DLP panel preset
```

Only one should be active at a time. Each preset also selects one `TOUCH_TYPE` value from a list of candidate HDL device-type codes in the header (the rest are left commented out for reference — uncomment the one matching your physical device model).

## Quick start

```cpp
#include <Wire.h>
#include "TouchModule.h"
#include "BusproTransport.h"
#include "MemoryCore.h"

BusproTransport bus(/* ...transport config... */);
MemoryCore flash(/* ...flash config... */);

// One entry per channel, matching TOUCH_CHANNEL_COUNT (4, for TOUCH4)
const uint8_t ledPins[]  = {2, 3, 4, 5};   // LED/output pin per channel
const uint8_t touchPads[] = {1, 2, 3, 4};  // BS8112 pad index per channel

TouchModule touch(Wire, bus, flash, /*sectorAddress=*/0x1000, ledPins, touchPads);

void setup() {
    touch.begin();  // also runs a short blocking startupAnimation() boot sequence
}

void loop() {
    // Polls touch state and advances LED blink timing (non-blocking) in one call
    touch.update();

    for (uint8_t ch = 0; ch < 4; ch++) {
        if (touch.isPressed(ch)) {
            // Touch already got an automatic single-flash (Blink).
            // For a longer action, switch to Blinking and clear it
            // once the action is actually confirmed done:
            touch.setLedMode(ch, TouchModule::LedMode::Blinking);
            startSomeLongRunningAction(ch);
        }
        if (touch.isReleased(ch)) { /* single-shot: channel just released */ }
        if (touch.isHold(ch))     { /* held past TOUCH_HOLD_TIME */ }

        if (someLongRunningActionFinished(ch)) {
            touch.finishOperation(ch); // LED returns to Deactive
        }
    }

    // Feed incoming bus frames to let TouchModule answer Buspro requests
    // (device search, firmware/hardware read, MAC/remark read-write, etc.)
    // BusproFrame frame = bus.receive();
    // if (frame.valid) touch.process(frame);
}
```

Attach `touch.irqHandler()` to the BS8112's IRQ pin (via `attachInterrupt`) if you want interrupt-driven polling instead of / in addition to calling `updateBS8112()` every loop.

## API Reference

See [DOCUMENTATION.md](./DOCUMENTATION.md) for the full class and method reference.

## Development Status

The header (`TouchModule.h`) declares more surface area than is currently implemented in `TouchModule.cpp`. Based on the current source:

**Implemented**
- Construction, `begin()`, `firstime()`, `init()`, `syncValues()`, `maxZone()`
- BS8112 init/poll (`initBS8112()`, `updateBS8112()`, `irqHandler()`)
- Touch state queries: `isTouched`, `isPressed`, `isReleased`, `isHold`, `getTouchState`
- Per-channel LED indicator state machine: `Sleep` / `Idle` / `Operation1` / `Operation2`
- Buspro universal-request handlers: firmware read, hardware read, find-device, device search, MAC address read/write, device remark read/write

**Declared but not yet implemented** (present in the header, no definition in the `.cpp`)
- `setTouch()`, `getTouch()`, `setAllTouchs()` — direct output control
- `applyTouchHardware()` — private hardware-apply helper
- Zone handlers: `handleReadZone`, `handleModifyZone`, `handleReadZoneRemark`, `handleModifyZoneRemark`
- Scene remark handlers: `handleReadSceneRemark`, `handleModifySceneRemark`

Scene table management, double-tap, and hold-repeat are sketched out in commented-out code in the header for future work.

## License

GPL-3.0 — see [LICENSE](./LICENSE).