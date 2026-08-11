# TouchModule — API Documentation

`class TouchModule` (`TouchModule.h` / `TouchModule.cpp`)

Represents one touch panel node on an HDL-Buspro-style RS485 bus, backed by a **BS8112** capacitive touch controller reached over I²C.

---

## Configuration macros

| Macro | Meaning |
|---|---|
| `TOUCH_HOLD_TIME` | Milliseconds a channel must stay touched before `isHold()` returns true. Default `1000`. |
| `TOUCH1` … `TOUCH12`, `AC_PANEL`, `DLP_PANEL` | Select `TOUCH_CHANNEL_COUNT` and the HDL `TOUCH_TYPE` device-type code at compile time. Exactly one should be defined. `TOUCH4` is active by default in the current header. |
| `TOUCH_CHANNEL_COUNT` | `constexpr uint8_t`, derived from the active preset above — the number of touch channels this build supports. |
| `TOUCH_TYPE` | `constexpr uint16_t`, the HDL Buspro device-type code reported for this panel, derived from the active preset. |

---

## Constructor

```cpp
TouchModule(
    TwoWire &wirePort,
    BusproTransport &bus,
    MemoryCore &flash,
    uint32_t sectorAddress,
    const uint8_t touchPins[TOUCH_CHANNEL_COUNT],
    const uint8_t touchPads[TOUCH_CHANNEL_COUNT],
    bool activeHigh = true
);
```

| Parameter | Description |
|---|---|
| `wirePort` | The `TwoWire` (I²C) instance used to talk to the BS8112. |
| `bus` | Reference to the shared `BusproTransport`, used to send responses on the RS485 bus. |
| `flash` | Reference to a `MemoryCore` instance used to persist device configuration. |
| `sectorAddress` | Base flash sector/offset this device's configuration is stored at. |
| `touchPins` | Array (length `TOUCH_CHANNEL_COUNT`) of output/LED pins, one per channel. Stored as `ledPins_`. |
| `touchPads` | Array (length `TOUCH_CHANNEL_COUNT`) mapping each logical channel to its BS8112 physical pad index. Stored as `touchPins_`. |
| `activeHigh` | Whether outputs are driven active-high. Defaults to `true`. |

The constructor also copies the MCU's unique ID into `uid_` via `mcu::copyMcuUID()`.

---

## Lifecycle

### `bool begin()`
Initializes I²C, configures the BS8112 (`initBS8112()`), sets all output pins, and runs a brief LED "power-on" sweep (drives each output HIGH with a 500 ms delay — useful as a visual boot indicator). If `firstime()` reports this is a fresh/unrecognized device, calls `init()` to write default configuration to flash. Finally calls and returns `syncValues()`.

### `bool firstime()`
Reads the stored UID and device type from flash and compares them against the current MCU UID / compiled `devType_`. Returns `true` if they differ (i.e., this is either a brand-new device or the firmware/device type changed), signaling that `init()` should run.

### `bool init()`
Writes default configuration to flash: device bus address, device type, MCU UID, a default remark (`"Touch"`), placeholder hardware/software version strings, and a default per-channel remark (`"Touch N"`) plus default enable/on-delay/on-protect bytes for every channel. Also seeds a default zone remark per channel (the nested scene-seeding loop is currently a no-op placeholder).

### `bool syncValues()`
Reloads `deviceAddress_` and `uid_` from flash, and refreshes the zone/scene count via `maxZone()`. Called at the end of `begin()` and after address changes.

### `uint8_t maxZone()`
Reads the per-channel zone assignment from flash and returns the highest zone number found (also updates `sceneCount`).

---

## Touch state (BS8112 driver)

### `void initBS8112()`
Writes the BS8112's configuration block (registers starting at `0xB0`): IRQ mode, power-save/wakeup behavior, per-key trigger threshold (`KeyTriggerthresholdvalue = 12`, applied uniformly to keys K2–K12), and an 8-bit checksum trailer, per the BS811x register map.

### `bool updateBS8112()`
Polls the BS8112 for a state change. Only re-reads hardware if `_irqFlag` (set by `irqHandler()`) or `_runAgain` is set — otherwise returns `false` immediately, so it's cheap to call every `loop()` iteration.

When it does read: fetches the 2-byte raw touch bitmask from register `0x08`, remaps only the configured pads (`touchPins_[]`) into a compact per-channel bitfield (`newState`, where bit *i* corresponds to logical channel *i*, not the raw hardware bit), computes `_pressedEdge` / `_releasedEdge` from the transition, and records `millis()` timestamps per channel for hold detection. Returns `true` if any channel's state changed.

### `void irqHandler()`
Sets `_irqFlag`. Intended to be attached to the BS8112's IRQ pin via `attachInterrupt()`; `updateBS8112()` will then do a hardware read on the next call.

### `uint16_t getTouchState() const`
Returns the current channel bitmask (`_touchState`) directly.

---

## Touch state queries

All take a zero-based `key` (channel index `< TOUCH_CHANNEL_COUNT`); out-of-range indices return `false`.

| Method | Behavior |
|---|---|
| `bool isTouched(uint8_t key)` | `true` for the entire duration the channel is held down. |
| `bool isPressed(uint8_t key)` | `true` only on the press edge (one call, the instant it becomes true). |
| `bool isReleased(uint8_t key)` | `true` only on the release edge (one call, the instant it becomes false). |
| `bool isHold(uint8_t key)` | `true` once, the first time a channel has been continuously touched for ≥ `TOUCH_HOLD_TIME` ms. |

`isHoldRepeat()` and `isDoubleTap()` are sketched in commented-out code for future implementation but not currently active.

---

## Direct output control — *declared, not yet implemented*

```cpp
bool setTouch(uint8_t channel, bool on);
bool getTouch(uint8_t channel) const;
void setAllTouchs(const bool states[TOUCH_CHANNEL_COUNT]);
```

These are declared in the header for controlling channel outputs directly (outside of a bus command), but have no definition in the current `TouchModule.cpp`.

---

## LED indicator (LedHandler)

LED status logic lives in its own file, `LedHandler.h` — a small header-only template class, `LedHandler<CHANNEL_COUNT>`. `TouchModule` owns one instance (sized to `TOUCH_CHANNEL_COUNT`) and forwards its public LED API (`setLedMode`, `updateLeds`, etc.) straight through, so existing calls on a `TouchModule` object work unchanged — but the LED logic itself is now reusable for any other channel-based device (relays, dimmers, ...) without depending on touch/Buspro code at all.

There are two moving parts:

1. **`updateLeds()`** (called from `loop()`, or via the combined `TouchModule::update()` helper) — millis()-driven, decides each channel's *target brightness* (0–31) based on its `LedMode` and blink timing.
2. **A TIM3 hardware-timer interrupt**, owned by `LedHandler` — the actual software PWM. It ticks at `LED_PWM_FREQUENCY_HZ * LED_PWM_LEVELS` = 100 Hz × 32 = **3200 Hz**, and on each tick compares every channel's target level against a free-running 0–31 counter, driving the GPIO high or low accordingly (classic threshold/counter software PWM). This gives a 100 Hz visible refresh rate with 32 brightness steps, without needing hardware PWM-capable pins.

> **Platform note:** the timer setup (`LedHandler::initPwmTimer()`) uses STM32duino's `HardwareTimer` on `TIM3`, guarded by `#if defined(ARDUINO_ARCH_STM32)`. On other cores (e.g. AVR, also listed as a supported platform in `library.json`) it's currently a no-op, so channels fall back to plain digital on/off (`level > 0` behaves like "on"). A real AVR Timer3 implementation would need the ATmega `TCCR3A`/`OCR3A`/`TIMSK3` registers instead — not yet implemented.
>
> **One instance at a time:** `LedHandler` uses a single static pointer (`pwmInstance_`) to route the timer ISR back to an instance method, since timer callbacks must be free functions. That means only **one `LedHandler` (i.e. one `TouchModule`) can own the TIM3 tick per MCU** — fine for a single touch panel per board, but a second instance would steal the interrupt from the first.

```cpp
enum class LedMode : uint8_t
{
    Deactive,  // off — device idle/asleep
    Active,    // on, steady — ready, no activity
    Blink,     // blink once — quick acknowledge (e.g. "touch registered")
    Blinking   // blink continuously until finishOperation() is called —
               // a longer-running action is in progress
};
```

### Methods (available on `TouchModule`, forwarded to `leds_`)

| Method | Description |
|---|---|
| `void setLedMode(uint8_t channel, LedMode mode)` | Sets a channel's LED mode and resets its blink phase timer. |
| `LedMode getLedMode(uint8_t channel) const` | Returns the channel's current mode. |
| `void setAllLedMode(LedMode mode)` | Applies a mode to every channel at once. |
| `void finishOperation(uint8_t channel)` | Call once a `Blinking` action has actually completed — returns the LED to `Deactive`. |
| `void setLedLevels(uint8_t activeLevel, uint8_t deactiveLevel, uint8_t blinkLevel = 31)` | Sets the brightness level (0–31) used by `Active`, `Deactive`, and the blink "on" phase. Values above 31 are clamped. |
| `void setLedBlinkTiming(uint16_t blinkMs, uint16_t blinkingPeriodMs)` | Configures the `Blink` flash duration and the `Blinking` on/off half-period, in milliseconds. |
| `void updateLeds()` | **Call every `loop()` iteration** (or via `update()`, below). Non-blocking — advances blink timing and updates each channel's target brightness based on `millis()`. The actual PWM output is driven separately by the TIM3 ISR. |
| `void startupAnimation()` | **Blocking** — a short boot animation (channel sweep + a couple of brightness pulses). Call once from `setup()`/`begin()`, never from `loop()`. |
| `void finditAnimation(uint8_t duration)` | **Blocking** — flashes every channel together for `duration` seconds (fast, easy-to-spot blink), then restores each channel's prior `LedMode`. Wired into `handleFindDevice()` for the Buspro FINDIT command. |
| `void sweep(uint8_t from, uint8_t to)` | **Blocking** — ramps brightness from `from` to `to` (0–31); the building block `startupAnimation()` is made of. |

### `void update()`

Convenience wrapper on `TouchModule` that calls `updateBS8112()` (touch polling) followed by `leds_.updateLeds()` (LED state advance) in one call — the single method you need in `loop()` for both subsystems.

### Default behavior

- The constructor calls `leds_.configure(ledPins, activeHigh)`, which initializes every channel to `Deactive`.
- `begin()` calls `leds_.begin()` (sets pin modes + starts the TIM3 tick) right after BS8112 init, then (after `firstime()`/`init()` if needed) calls `leds_.setAllLedMode(LedMode::Deactive)` followed by the blocking `leds_.startupAnimation()`, before returning.
- `updateBS8112()` automatically fires a single `Blink` flash on every fresh touch press (`isPressed` edge), unless that channel is currently `Blinking` (so a long-running action's blink isn't interrupted by the next touch).
- Defaults: `ledActiveLevel_ = 31` (full), `ledDeactiveLevel_ = 0` (off), `ledBlinkLevel_ = 31`, `Blink` flash `150 ms`, `Blinking` half-period `300 ms`.

### Typical usage

```cpp
void setup() {
    touch.begin();   // includes the blocking startupAnimation()
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

// Turn a channel's LED steady on (e.g. output is currently active):
touch.setLedMode(3, TouchModule::LedMode::Active);

// Dim the "active" brightness to about 40% (level 12 of 0-31):
touch.setLedLevels(/*activeLevel=*/12, /*deactiveLevel=*/0, /*blinkLevel=*/31);
```

---

## Button types

Each channel can be assigned a `ButtonType`, which decides *how* its touch gestures translate into a `ButtonEvent` fired to a single registered callback. This is RAM-only (not persisted to flash) and independent of the LED indicator system.

```cpp
enum class ButtonEvent : uint8_t
{
    On,  // e.g. SingleON fired, ONOFF toggled on, short-press fired, double-click detected
    Off, // e.g. SingleOFF fired, ONOFF toggled off, long-press/hold fired
    Jog  // ShortLongJog only: repeat tick while held past the hold threshold
};

using ButtonCallback = void (*)(uint8_t channel, ButtonEvent event);
```

### Methods

| Method | Description |
|---|---|
| `void setButtonType(uint8_t channel, ButtonType type)` | Assigns a channel's gesture behavior. Defaults to `ButtonType::Invalid` (fires nothing) for every channel. Resets any in-progress gesture state (toggle phase, pending double-click, etc.) for that channel. |
| `ButtonType getButtonType(uint8_t channel) const` | Returns the channel's current type. |
| `void setButtonCallback(ButtonCallback callback)` | Registers the single callback invoked whenever any channel's gesture fires. There's one callback total, not one per channel — branch on the `channel` argument inside it. |
| `void setButtonTiming(uint16_t doubleClickGapMs, uint16_t jogIntervalMs)` | Tunes the max gap between taps for `Dblclick*` (default `300` ms) and the repeat interval for `ShortLongJog` (default `500` ms). |

### `void processButtons()`

Called automatically from `update()` every loop iteration — drives every channel's `ButtonType` state machine off the `isPressed()`/`isReleased()`/`isTouched()`/`isHold()` edges computed by the preceding `updateBS8112()` call in the same `update()`.

### What each `ButtonType` fires

| ButtonType | Behavior |
|---|---|
| `Invalid` | Fires nothing. |
| `SingleON` | Fires `On` on every press. |
| `SingleOFF` | Fires `Off` on every press. |
| `SingleONOFF` | Toggles per press: fires `On`, then `Off`, then `On`, ... |
| `CombinationON` / `CombinationOFF` / `CombinationONOFF` | **Currently identical to their `Single*` counterpart above** — see note below. |
| `Momentary` | Fires `On` on press, `Off` on release — mirrors the touch contact directly (e.g. simulating a doorbell/momentary switch). |
| `DblclickSingle` / `DblclickCombined` | Fires `On` when two releases land within `doubleClickGapMs` of each other. A lone tap (window expires with no second release) fires nothing. Both variants currently behave the same. |
| `ShortLongPress` | Release before `TOUCH_HOLD_TIME` (1000 ms default) fires `On` ("short"). Still held when `TOUCH_HOLD_TIME` is crossed fires `Off` ("long"), once. |
| `ShortLongJog` | Same short/long split as `ShortLongPress`, but once "long" has fired, keeps firing `Jog` every `jogIntervalMs` for as long as the channel stays held — meant for continuous adjustment (e.g. dimmer ramp) while held. |

> **On `Combination*` and short/long semantics — both are assumptions, not spec.** Per design discussion, `Combination*` types are *not* about pairing two physical keys — there's no cross-channel logic here — but no other distinguishing behavior was specified either, so they're implemented as plain aliases of `Single*` for now. Similarly, "short press → `On`, long press → `Off`" for `ShortLongPress`/`ShortLongJog` was picked as a reasonable default, not derived from a spec. Both are easy to change once the actual intended behavior is known — flag it if either doesn't match what you need.

### Typical usage

```cpp
void onButtonEvent(uint8_t channel, TouchModule::ButtonEvent event)
{
    switch (event)
    {
    case TouchModule::ButtonEvent::On:
        relay.setChannel(channel, true);
        touch.setKeyHigh(channel, true);
        break;
    case TouchModule::ButtonEvent::Off:
        relay.setChannel(channel, false);
        touch.setKeyHigh(channel, false);
        break;
    case TouchModule::ButtonEvent::Jog:
        dimmer.step(channel, +1);
        break;
    }
}

void setup() {
    touch.begin();
    touch.setButtonType(0, TouchModule::ButtonType::SingleONOFF);
    touch.setButtonType(1, TouchModule::ButtonType::Momentary);
    touch.setButtonType(2, TouchModule::ButtonType::ShortLongJog);
    touch.setButtonCallback(onButtonEvent);
}

void loop() {
    touch.update(); // also drives processButtons() internally
}
```

---

## Device mode (Sleep / Wake)

A device-level mode sits on top of the per-channel LED state above. It answers "what should the whole panel look like right now" rather than any one channel's state.

```cpp
enum class DeviceMode : uint8_t
{
    Sleep, // every LED shows one uniform dim glow; any touch or FINDIT wakes it
    Wake   // each channel independently shows its stored high/low state
};
```

| Method | Description |
|---|---|
| `void sleep()` | Enter `Sleep`: sets every channel's LED level to `sleepLevel_` and mode to `Active`, so they all show one uniform dim glow regardless of individual channel state. |
| `void wake()` | Enter `Wake`: restores each channel's brightness levels (`wakeHighLevel_`/`wakeLowLevel_`) and sets each channel's `LedMode` back to `Active`/`Deactive` per its stored `keyHigh_[]` state. Also resets the idle timer. |
| `DeviceMode getDeviceMode() const` | Returns the current mode. |
| `void setKeyHigh(uint8_t channel, bool high)` | Sets a channel's logical Wake-mode state — `true` ("high"/lit) or `false` ("low"/off-dim). Call this whenever the real thing the channel represents changes (e.g. a relay output toggles). Applied immediately if awake; just remembered if asleep, and applied on the next `wake()`. |
| `bool isKeyHigh(uint8_t channel) const` | Returns the channel's stored high/low state. |
| `void setSleepLevel(uint8_t level)` | Sets the `Sleep` glow brightness (0–31). Default `2`. |
| `void setWakeLevels(uint8_t highLevel, uint8_t lowLevel)` | Sets the `Wake`-mode "high"/"low" brightness levels (0–31). Defaults `31`/`0`. |
| `void setSleepTimeout(uint32_t ms)` / `uint32_t getSleepTimeout() const` | Auto-sleep idle timeout, checked every `update()`. Default `30000` (30s). |
| `void markActivity()` | Resets the idle timer without changing mode — for counting some other event as activity. |

### Auto-sleep behavior

- `update()` calls `checkAutoSleep()` every loop iteration: once `millis() - lastActivityTime_ >= sleepTimeoutMs_` while `Wake`, it calls `sleep()` automatically.
- The idle timer resets on: any fresh touch press (inside `updateBS8112()`), a FINDIT request (whether it wakes the device or the device was already awake), and `wake()` itself.
- `begin()` sets the idle timer's starting point to "device ready" (right after the boot animation), so the 30s countdown begins from boot, not from `time 0`.
- There's no separate "wake acknowledge" animation — the LEDs jumping from the uniform sleep glow to each channel's real high/low state on `wake()` is itself the visual feedback.
- The touch-press acknowledge flash (`Blink`) is skipped for channels currently "high" (see `updateBS8112()`), specifically so it doesn't fight with `Blink`'s auto-return-to-`Deactive` — a "high" channel flashing would otherwise incorrectly settle back to looking "low" once the flash ends.

```cpp
// Elsewhere in your sketch, whenever a channel's real output changes:
relay.setChannel(2, true);
touch.setKeyHigh(2, true);   // LED reflects it immediately if awake

// Put the panel to sleep manually (e.g. from a "goodnight" scene):
touch.sleep();

// Extend the idle timeout to 2 minutes:
touch.setSleepTimeout(120000);
```

---

## Buspro protocol integration

### `void process(const BusproFrame &frame)`
Main protocol entry point — call this with every frame received off the bus. Dispatches by `frame.dstAddress`:

- **`0xFFFF` (broadcast / universal commands):** handles `DEVICE_SEARCH_HDL` and read-`DEVICE_REMARK`.
- **`deviceAddress_` (addressed to this device):** handles firmware read, hardware read, find-device, device search, MAC address read/write, and device remark read/write.

A commented-out section marks where device-specific (zone/scene/curtain) opcodes will be dispatched once implemented.

### `void sendResponse(uint16_t opcode, uint16_t dst, const uint8_t *payload, uint8_t payloadLen)`
Thin wrapper around `bus_.send()`, automatically filling in this device's address and `devType_`.

### Implemented Buspro handlers

| Handler | Opcode(s) | Behavior |
|---|---|---|
| `handleReadFirmware` | `DEVICE_FIRMWARE.req()` | Responds with the stored software-version string. |
| `handleReadHardware` | `DEVICE_HARDWARE.req()` | Responds with the stored hardware-version string. |
| `handleFindDevice` | `DEVICE_FINDIT.req()` | "Identify" — requires a 1-byte payload (`duration`, seconds). Wakes the device if asleep (or marks activity if already awake), sends the acknowledgment, then runs the blocking `leds_.finditAnimation(duration)` — every channel blinks fast for `duration` seconds, then restores its prior LED state. |
| `handleSearchDevice` | `DEVICE_SEARCH_HDL.req()` | Echoes back the first two payload bytes of the search request (discovery response). |
| `handleReadMacaddress` | `DEVICE_MAC_ADDRESS.readReq()` | Responds with the MCU's unique ID. |
| `handleModifyMacaddress` | `DEVICE_MAC_ADDRESS.writeReq()` | Validates the request's UID matches this device's UID, then updates and persists the bus address from the last 2 payload bytes. |
| `handleReadDeviceRemark` | `DEVICE_REMARK.readReq()` | Responds with the stored 20-byte device remark/name. |
| `handleModifyDeviceRemark` | `DEVICE_REMARK.writeReq()` | Overwrites the stored device remark with the 20-byte payload. |

### Declared but not yet implemented handlers
The zone/scene handler declarations (`handleReadZone`, `handleModifyZone`, `handleReadZoneRemark`, `handleModifyZoneRemark`, `handleReadSceneRemark`, `handleModifySceneRemark`) have been commented out in the header — no definitions exist yet, and `process()` doesn't dispatch to them.

> **Note on `handleFindDevice`:** the response is sent *before* `finditAnimation()` runs, so the bus ack isn't delayed by the blink — but `finditAnimation()` itself is blocking (spins on `updateLeds()` for `duration` seconds), so `process()` won't handle any further frames until it returns. Fine for an occasional "find my device" command; would need rework if FINDIT could plausibly overlap with other time-sensitive bus traffic.

### `ButtonOperationType` and `ButtomMode` — still reserved
`ButtonType` is now wired up (see [Button types](#button-types) below). `ButtonOperationType` and the (currently empty) `ButtomMode` remain declared but unused — `ButtonOperationType` looks like it maps to HDL Buspro's per-key bound-action codes (Scene, Sequence, Channel control, ...), for a future step where a fired `ButtonEvent` gets translated into an actual bus action. Nothing in the library currently constructs or switches on either.

---

## Accessors

| Method | Returns |
|---|---|
| `MemoryCore &flash()` | The `MemoryCore` instance this device uses for persistence. |
| `uint32_t memoryAddress() const` | This device's configured flash sector/offset. |

---

## Internal helpers (private)

| Method | Purpose |
|---|---|
| `uint16_t convert(uint16_t touchState)` | Remaps a raw 10-bit hardware bit pattern to a custom logical key layout (fixed lookup table). Not currently called from `updateBS8112()`, which does its own pad-index-based remap — likely a leftover/alternate mapping strategy. |
| `void writeRegister(uint8_t reg, uint8_t value)` | Single-byte I²C register write to the BS8112. |
| `uint8_t readRegister(uint8_t reg)` | Single-byte I²C register read from the BS8112. |
| `void applyTouchHardware(uint8_t channel)` | Declared, not yet implemented. |

---

## Hardware notes: BS8112

- I²C address: `0x50` (`touch_address`).
- Configuration block is written starting at register `0xB0` (17 bytes: mode/power config + one trigger-threshold byte per key K2–K11, plus a combined config/IRQ-enable byte for K12) followed by a simple additive checksum byte.
- Touch status is read as 2 bytes starting at register `0x08`.
- Per-key trigger threshold is currently hardcoded to `12` (range 1–32) for all keys in `initBS8112()`.