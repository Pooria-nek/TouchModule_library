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
| `handleFindDevice` | `DEVICE_FINDIT.req()` | "Identify" — replies with an empty 8-byte payload (implementation stub for a locate/blink action). |
| `handleSearchDevice` | `DEVICE_SEARCH_HDL.req()` | Echoes back the first two payload bytes of the search request (discovery response). |
| `handleReadMacaddress` | `DEVICE_MAC_ADDRESS.readReq()` | Responds with the MCU's unique ID. |
| `handleModifyMacaddress` | `DEVICE_MAC_ADDRESS.writeReq()` | Validates the request's UID matches this device's UID, then updates and persists the bus address from the last 2 payload bytes. |
| `handleReadDeviceRemark` | `DEVICE_REMARK.readReq()` | Responds with the stored 20-byte device remark/name. |
| `handleModifyDeviceRemark` | `DEVICE_REMARK.writeReq()` | Overwrites the stored device remark with the 20-byte payload. |

### Declared but not yet implemented handlers
`handleReadZone`, `handleModifyZone`, `handleReadZoneRemark`, `handleModifyZoneRemark`, `handleReadSceneRemark`, `handleModifySceneRemark` — declared in the header for zone/scene configuration over the bus, no definitions yet.

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
