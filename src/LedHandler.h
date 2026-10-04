#pragma once

#include <Arduino.h>

#if defined(ARDUINO_ARCH_STM32)
#include <HardwareTimer.h>
#endif

// ============================================================================
// Build options
// ============================================================================

// Hardware timer used for the software PWM (STM32 only).
// Override with -DLED_PWM_TIMER_INSTANCE=TIM3 if TIM4 is already in use.
#ifndef LED_PWM_TIMER_INSTANCE
#define LED_PWM_TIMER_INSTANCE TIM4
#endif

// Direct BSRR GPIO writes inside the ISR (much faster than digitalWrite()).
// Enabled automatically when the STM32 core exposes the needed macros.
// Force off with -DLED_USE_FAST_GPIO=0 if your core does not compile with it.
#ifndef LED_USE_FAST_GPIO
#if defined(ARDUINO_ARCH_STM32) && defined(STM_PORT) && defined(STM_PIN)
#define LED_USE_FAST_GPIO 1
#else
#define LED_USE_FAST_GPIO 0
#endif
#endif

// ============================================================================
// Software PWM
// ============================================================================
//
// The hardware timer ISR runs at:
//
//     LED_PWM_FREQUENCY_HZ * LED_PWM_LEVELS
//
// A free-running PWM counter (0 .. LED_PWM_LEVELS-1) is compared against
// each LED's target brightness level.
//
// Example:
//     LED_PWM_FREQUENCY_HZ = 100 Hz
//     LED_PWM_LEVELS       = 32
//
// Timer frequency = 3200 Hz
// Visible PWM frequency = 100 Hz
// Brightness resolution = 32 levels
//
// On non-STM32 targets there is no hardware timer. Instead, call update()
// regularly from loop(); LEDs are then driven with simple digital ON/OFF
// (a level in the upper half of the range = ON, lower half = OFF).
//

constexpr uint16_t LED_PWM_FREQUENCY_HZ = 100;
constexpr uint8_t LED_PWM_LEVELS = 32;

// Frequency of the PWM timer interrupt.
constexpr uint32_t LED_PWM_TIMER_HZ =
    static_cast<uint32_t>(LED_PWM_FREQUENCY_HZ) * LED_PWM_LEVELS;

// ============================================================================
// LedHandler
// ============================================================================
//
// Controls a bank of CHANNEL_COUNT LEDs.
//
// Features:
//   - Per-channel LED modes
//   - Software PWM brightness
//   - Blink / Blinking modes
//   - Sleep / Wake
//   - Timer-driven animations
//
// On STM32, all LED state updates and animations are driven by the hardware
// timer. No updateLeds(), millis(), or delay() calls are required.
// On other targets, call update() from loop().
//
// All public methods are safe to call from the main context while the timer
// ISR is running.
//
// Only one LedHandler instance can own the PWM timer at a time.
//

template <uint8_t CHANNEL_COUNT>
class LedHandler
{
public:
    // ========================================================================
    // LED modes
    // ========================================================================

    enum class LedMode : uint8_t
    {
        Sleep = 0,
        Deactive, // Steady at deactive brightness
        Active,   // Steady at active brightness
        Blink,    // Blink once, then return to the previous mode
        Blinking  // Continue blinking until explicitly changed
    };

    LedHandler() = default;

    // ========================================================================
    // Configuration
    // ========================================================================

    // Configure LED GPIO pins and polarity.
    // Must be called before begin().
    void configure(
        const uint8_t ledPins[CHANNEL_COUNT],
        bool activeHigh = true);

    // Configure GPIOs and start the hardware PWM timer.
    void begin();

#if !defined(ARDUINO_ARCH_STM32)
    // Non-STM32 fallback: call regularly from loop().
    // Advances LED state / animations using millis() and drives the pins.
    void update();
#endif

    // ========================================================================
    // LED control
    // ========================================================================

    void setLedMode(
        uint8_t channel,
        LedMode mode);

    LedMode getLedMode(
        uint8_t channel) const;

    void setAllLedMode(
        LedMode mode);

    // Return a channel to Deactive mode after an operation finishes.
    void finishOperation(
        uint8_t channel);

    // ========================================================================
    // Brightness / timing
    // ========================================================================

    // Brightness range:
    //     0 .. LED_PWM_LEVELS - 1
    //
    // activeLevel:
    //     Brightness used by Active mode.
    //
    // deactiveLevel:
    //     Brightness used by Deactive mode.
    //
    // blinkLevel:
    //     Brightness used during Blink/Blinking ON phase.
    void setLedLevels(
        uint8_t activeLevel,
        uint8_t deactiveLevel,
        uint8_t blinkLevel = LED_PWM_LEVELS - 1);

    // blinkMs:
    //     Duration of a single Blink operation.
    //
    // blinkingPeriodMs:
    //     ON/OFF period used by Blinking mode.
    void setLedBlinkTiming(
        uint16_t blinkMs,
        uint16_t blinkingPeriodMs);

    // ========================================================================
    // Animations
    // ========================================================================

    // Start the Find-It animation.
    //
    // duration is specified in seconds.
    //
    // Non-blocking. The animation runs automatically.
    // LED modes and blink timing are restored when it finishes.
    void finditAnimation(
        uint8_t duration);

    // Start the startup animation.
    //
    // Non-blocking. The animation runs automatically.
    // The brightness levels set with setLedLevels() are restored when it
    // finishes.
    void startupAnimation();

    // ========================================================================
    // Sleep / Wake
    // ========================================================================

    // Put all LEDs into Sleep mode and remember their current modes.
    void sleep();

    // Restore the LED modes that were active before sleep().
    void wake();

private:
    // ========================================================================
    // LED configuration
    // ========================================================================

    uint8_t ledPins_[CHANNEL_COUNT];

#if LED_USE_FAST_GPIO
    // Cached port / bit mask per LED for direct BSRR writes.
    GPIO_TypeDef *ledPort_[CHANNEL_COUNT];
    uint32_t ledMask_[CHANNEL_COUNT];
#endif

    bool activeHigh_ = true;

    LedMode ledMode_[CHANNEL_COUNT];

    // Mode to return to when a one-shot Blink finishes.
    LedMode previosLedMode_[CHANNEL_COUNT];

    // Mode to restore on wake().
    LedMode sleepSavedMode_[CHANNEL_COUNT];

    bool ledOn_[CHANNEL_COUNT];

    // Current PWM target level for each LED.
    uint8_t ledLevel_[CHANNEL_COUNT];

    // ========================================================================
    // LED state timing
    // ========================================================================

    // Number of 1 ms state-update ticks elapsed for each LED.
    uint32_t ledPhaseTicks_[CHANNEL_COUNT];

    // Divides the high-frequency PWM timer into a 1 kHz
    // LED state / animation update.
    uint32_t msAccumulator_ = 0;

    // ========================================================================
    // Brightness
    // ========================================================================

    uint8_t ledActiveLevel_ = LED_PWM_LEVELS - 1;
    uint8_t ledDeactiveLevel_ = 3;
    uint8_t ledBlinkLevel_ = LED_PWM_LEVELS - 1;

    // ========================================================================
    // Blink timing
    // ========================================================================

    uint16_t ledBlinkMs_ = 500;
    uint16_t ledBlinkingPeriodMs_ = 500;

    // ========================================================================
    // Sleep state
    // ========================================================================

    bool sleeping_ = false;

    // ========================================================================
    // PWM timer
    // ========================================================================

    uint8_t pwmCounter_ = 0;

#if defined(ARDUINO_ARCH_STM32)
    HardwareTimer *pwmTimer_ = nullptr;
#else
    uint32_t lastUpdateMs_ = 0;
#endif

    // Only one LedHandler instance can own the timer ISR.
    static LedHandler<CHANNEL_COUNT> *pwmInstance_;

    // ========================================================================
    // Timer / PWM functions
    // ========================================================================

    void initPwmTimer();

    static void pwmIsrTrampoline();

    void pwmTick();

    void updatePwm();

    // Drive one LED pin to the requested physical level.
    void writePin(
        uint8_t channel,
        bool high);

    // ========================================================================
    // LED state machine
    // ========================================================================

    // 1 ms state + animation update.
    void tick1ms();

    void updateLedState();

    // Mode the channel returns to once any one-shot Blink is over.
    LedMode baseMode(
        uint8_t channel) const;

    // Set ledLevel_ / ledOn_ from the channel's current mode.
    void applyModeLevel(
        uint8_t channel);

    void updateBlink(
        uint8_t channel);

    void updateBlinking(
        uint8_t channel);

    // ========================================================================
    // Animation state
    // ========================================================================

    enum class AnimationType : uint8_t
    {
        None,
        FindIt,
        Startup
    };

    AnimationType animationType_ = AnimationType::None;

    // ------------------------------------------------------------------------
    // General animation timing
    // ------------------------------------------------------------------------

    uint32_t animationElapsedMs_ = 0;
    uint32_t animationDurationMs_ = 0;

    // ------------------------------------------------------------------------
    // Startup animation state
    // ------------------------------------------------------------------------

    uint8_t animationStep_ = 0;
    uint8_t animationPulseCount_ = 0;

    uint8_t animationFrom_ = 0;
    uint8_t animationTo_ = 0;

    uint16_t animationStepElapsedMs_ = 0;
    uint16_t animationPulseElapsedMs_ = 0;

    bool animationPulseHigh_ = true;

    // User brightness levels, restored when the startup animation ends.
    uint8_t animationSavedActive_ = LED_PWM_LEVELS - 1;
    uint8_t animationSavedDeactive_ = 3;
    uint8_t animationSavedBlink_ = LED_PWM_LEVELS - 1;

    // ------------------------------------------------------------------------
    // Find-It state
    // ------------------------------------------------------------------------

    uint16_t animationPreviousBlinkMs_ = 0;
    uint16_t animationPreviousPeriodMs_ = 0;

    LedMode animationPreviousMode_[CHANNEL_COUNT];

    // ========================================================================
    // Animation functions
    // ========================================================================

    void updateAnimation();

    void updateFindItAnimation();

    void updateStartupAnimation();

    bool updateSweepStep();

    void finishAnimation();
};