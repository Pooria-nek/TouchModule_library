#pragma once

#include <Arduino.h>

#if defined(ARDUINO_ARCH_STM32)
#include <HardwareTimer.h>
#endif

// --- Software PWM (LED brightness) ---
// A TIM3 hardware-timer interrupt (STM32) ticks at LED_PWM_FREQUENCY_HZ *
// LED_PWM_LEVELS and compares each channel's target level against a
// free-running 0..LED_PWM_LEVELS-1 counter (threshold/software PWM). This
// gives a LED_PWM_FREQUENCY_HZ visible refresh rate with LED_PWM_LEVELS
// brightness steps, without needing hardware-PWM-capable pins.
// On non-STM32 targets this currently falls back to plain digital on/off
// (level > 0 => on) — see initPwmTimer()/pwmTick().
constexpr uint16_t LED_PWM_FREQUENCY_HZ = 100; // visible refresh rate
constexpr uint8_t LED_PWM_LEVELS = 32;         // brightness resolution (0..31)

// Owns and drives a bank of CHANNEL_COUNT status LEDs: a per-channel mode
// state machine (Deactive/Active/Blink/Blinking), software-PWM brightness,
// and a small boot animation.
//
// Template, so it can be sized to any TouchModule's TOUCH_CHANNEL_COUNT.
// Method bodies live in LedHandler.tpp (included at the bottom of this
// file) rather than here, to keep the declaration readable — this is
// still effectively header-only, as required for a class template.
//
// NOTE: only one LedHandler instance can own the TIM3 tick at a time (see
// pwmInstance_) — fine for a single touch panel per MCU, but if a board
// ever needs two independent LedHandlers, the second one constructed will
// steal the timer interrupt from the first.
template <uint8_t CHANNEL_COUNT>
class LedHandler
{
public:
    enum class LedMode : uint8_t
    {
        Deactive = 0, // off
        Active,       // on, steady
        Blink,        // blink once, then auto-return to Deactive
        Blinking      // blink until finishOperation() is called
    };

    LedHandler() = default;

    // Stores the per-channel output pin and polarity. Call before begin().
    void configure(const uint8_t ledPins[CHANNEL_COUNT], bool activeHigh = true);

    // Sets all pins to OUTPUT and starts the software-PWM timer tick.
    void begin();

    // Non-blocking, millis()-driven — advances each channel's LedMode/blink
    // timing and updates its *target* brightness (ledLevel_[i]). The actual
    // GPIO toggling happens separately, inside pwmTick(), driven by the
    // timer interrupt. Call every loop().
    void updateLeds();

    void setLedMode(uint8_t channel, LedMode mode);

    LedMode getLedMode(uint8_t channel) const;

    void setAllLedMode(LedMode mode);

    // Call once a Blinking action has actually finished; LED returns to Deactive.
    void finishOperation(uint8_t channel);

    // Tunable brightness levels, range 0..LED_PWM_LEVELS-1 (0-31). blinkLevel
    // is used for the "on" phase of Blink/Blinking. Values are clamped.
    void setLedLevels(uint8_t activeLevel, uint8_t deactiveLevel, uint8_t blinkLevel = LED_PWM_LEVELS - 1);

    void setLedBlinkTiming(uint16_t blinkMs, uint16_t blinkingPeriodMs);

    // Blocking — flashes every channel together for `duration` seconds, meant
    // for a "find/identify this device" command (e.g. Buspro DEVICE_FINDIT).
    // Restores each channel's previous LedMode once finished, so it doesn't
    // clobber whatever a channel was already doing (e.g. mid-Blinking).
    void finditAnimation(uint8_t duration);

    // Small blocking boot animation — call once, before handing control over
    // to updateLeds() in loop(). Uses delay(), so only call it from setup().
    void startupAnimation();

    // Blocking brightness ramp from `from` to `to` (0-31), applied to the
    // "deactive" level while active/blink stay fixed — used by startupAnimation().
    void sweep(uint8_t from, uint8_t to);

private:
    uint8_t ledPins_[CHANNEL_COUNT];
    bool activeHigh_ = true;

    LedMode ledMode_[CHANNEL_COUNT];
    bool ledOn_[CHANNEL_COUNT];             // current blink phase (Blink/Blinking)
    uint32_t ledPhaseStart_[CHANNEL_COUNT]; // millis() timestamp of the current phase
    uint8_t ledLevel_[CHANNEL_COUNT];       // current target brightness (0..LED_PWM_LEVELS-1),
                                            // written by updateLeds(), read by the timer ISR

    uint8_t ledActiveLevel_ = LED_PWM_LEVELS - 1; // full brightness
    uint8_t ledDeactiveLevel_ = 0;                // off
    uint8_t ledBlinkLevel_ = LED_PWM_LEVELS - 1;  // "on" phase level while blinking

    uint16_t ledBlinkMs_ = 150;          // duration of the single Blink flash
    uint16_t ledBlinkingPeriodMs_ = 300; // on/off half-period while Blinking is active

#if defined(ARDUINO_ARCH_STM32)
    HardwareTimer *pwmTimer_ = nullptr;
#endif
    volatile uint8_t pwmCounter_ = 0; // free-running 0..LED_PWM_LEVELS-1 PWM phase counter

    static LedHandler *pwmInstance_; // for the timer ISR trampoline (one active instance)

    // Sets up the software-PWM tick. On STM32 this uses TIM3 running at
    // LED_PWM_FREQUENCY_HZ * LED_PWM_LEVELS. On other cores this is
    // currently a no-op — pwmTick() then acts as plain digital on/off.
    void initPwmTimer();

    // Timer callbacks must be free functions, so this static forwards to
    // the one active LedHandler instance's pwmTick().
    static void pwmIsrTrampoline();

    // Runs at LED_PWM_FREQUENCY_HZ * LED_PWM_LEVELS inside the timer ISR.
    // Keep this fast — no flash access, no long loops.
    void pwmTick();
};