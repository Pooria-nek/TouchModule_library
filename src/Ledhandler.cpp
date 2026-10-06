#include "LedHandler.h"

// ============================================================================
// Critical section
// ============================================================================
//
// Public methods run in the main context while pwmTick() runs in the timer
// ISR, so multi-field state changes must not be interrupted by the ISR.
// The guard saves/restores PRIMASK, so it is safe to nest and safe to use
// from inside the ISR itself (animations call setLedMode()/setLedLevels()).
//
// On non-STM32 targets there is no ISR (update() is polled from loop()),
// so the guard does nothing.

namespace
{
#if defined(ARDUINO_ARCH_STM32)
    class CriticalSection
    {
    public:
        CriticalSection() : primask_(__get_PRIMASK())
        {
            __disable_irq();
            __asm volatile("" ::: "memory");
        }

        ~CriticalSection()
        {
            __asm volatile("" ::: "memory");
            if (!primask_)
                __enable_irq();
        }

        CriticalSection(const CriticalSection &) = delete;
        CriticalSection &operator=(const CriticalSection &) = delete;

    private:
        uint32_t primask_;
    };
#else
    struct CriticalSection
    {
        CriticalSection() {}
        ~CriticalSection() {}
    };
#endif
} // namespace

// ============================================================================
// Configuration
// ============================================================================

template <uint8_t CHANNEL_COUNT>
void LedHandler<CHANNEL_COUNT>::configure(
    const uint8_t ledPins[CHANNEL_COUNT],
    bool activeHigh)
{
    CriticalSection cs;

    activeHigh_ = activeHigh;

    for (uint8_t i = 0; i < CHANNEL_COUNT; i++)
    {
        ledPins_[i] = ledPins[i];

        ledMode_[i] = LedMode::Deactive;
        previosLedMode_[i] = LedMode::Deactive;
        sleepSavedMode_[i] = LedMode::Deactive;
        animationPreviousMode_[i] = LedMode::Deactive;

        ledPhaseTicks_[i] = 0;
        applyModeLevel(i);
    }

    pwmCounter_ = 0;
    msAccumulator_ = 0;

    sleeping_ = false;
    animationType_ = AnimationType::None;
}

// ============================================================================
// Initialization
// ============================================================================

template <uint8_t CHANNEL_COUNT>
void LedHandler<CHANNEL_COUNT>::begin()
{
    {
        CriticalSection cs;

        for (uint8_t i = 0; i < CHANNEL_COUNT; i++)
        {
            pinMode(ledPins_[i], OUTPUT);

            // Force LED OFF immediately.
            digitalWrite(
                ledPins_[i],
                activeHigh_ ? LOW : HIGH);

#if LED_USE_FAST_GPIO
            const PinName pinName = digitalPinToPinName(ledPins_[i]);
            ledPort_[i] = get_GPIO_Port(STM_PORT(pinName));
            ledMask_[i] = static_cast<uint32_t>(1U) << STM_PIN(pinName);
#endif

            ledMode_[i] = LedMode::Deactive;
            previosLedMode_[i] = LedMode::Deactive;
            sleepSavedMode_[i] = LedMode::Deactive;

            ledPhaseTicks_[i] = 0;
            applyModeLevel(i);
        }

        pwmCounter_ = 0;
        msAccumulator_ = 0;

        sleeping_ = false;
        animationType_ = AnimationType::None;
    }

    initPwmTimer();
}

template <uint8_t CHANNEL_COUNT>
void LedHandler<CHANNEL_COUNT>::initPwmTimer()
{
    pwmInstance_ = this;

#if defined(ARDUINO_ARCH_STM32)

    // begin() may be called more than once: reuse the timer object
    // instead of leaking a new one.
    if (pwmTimer_ == nullptr)
        pwmTimer_ = new HardwareTimer(LED_PWM_TIMER_INSTANCE);
    else
        pwmTimer_->pause();

    pwmTimer_->setOverflow(
        LED_PWM_TIMER_HZ,
        HERTZ_FORMAT);

    pwmTimer_->attachInterrupt(
        pwmIsrTrampoline);

    pwmTimer_->resume();

#else

    lastUpdateMs_ = millis();

#endif
}

#if !defined(ARDUINO_ARCH_STM32)

// Non-STM32 fallback: no timer, no PWM. Called from loop().
template <uint8_t CHANNEL_COUNT>
void LedHandler<CHANNEL_COUNT>::update()
{
    const uint32_t now = millis();

    uint32_t elapsed = now - lastUpdateMs_;

    // Avoid a long catch-up burst if loop() stalled.
    if (elapsed > 250)
    {
        lastUpdateMs_ = now - 250;
        elapsed = 250;
    }

    while (elapsed--)
    {
        lastUpdateMs_++;
        tick1ms();
    }

    for (uint8_t i = 0; i < CHANNEL_COUNT; i++)
    {
        const bool on =
            (static_cast<uint16_t>(ledLevel_[i]) * 2) >= LED_PWM_LEVELS;

        writePin(i, activeHigh_ ? on : !on);
    }
}

#endif

// ============================================================================
// LED control
// ============================================================================

template <uint8_t CHANNEL_COUNT>
uint8_t LedHandler<CHANNEL_COUNT>::getLedMode(uint8_t channel) const
{
    return static_cast<uint8_t>(ledMode_[channel]);
}

template <uint8_t CHANNEL_COUNT>
void LedHandler<CHANNEL_COUNT>::setLedMode(
    uint8_t channel,
    LedMode mode)
{
    if (channel >= CHANNEL_COUNT)
        return;

    CriticalSection cs;

    // Remember the mode to return to after a one-shot Blink.
    // Do not overwrite it when re-triggering Blink during a Blink,
    // otherwise the LED would return to "Blink" forever.
    if (mode == LedMode::Blink && ledMode_[channel] != LedMode::Blink)
        previosLedMode_[channel] = ledMode_[channel];

    ledMode_[channel] = mode;
    ledPhaseTicks_[channel] = 0;

    // Blink sequences always start ON.
    applyModeLevel(channel);
}

template <uint8_t CHANNEL_COUNT>
void LedHandler<CHANNEL_COUNT>::setAllLedMode(LedMode mode)
{
    CriticalSection cs;

    for (uint8_t i = 0; i < CHANNEL_COUNT; i++)
        setLedMode(i, mode);
}

template <uint8_t CHANNEL_COUNT>
void LedHandler<CHANNEL_COUNT>::finishOperation(uint8_t channel)
{
    setLedMode(channel, LedMode::Deactive);
}

// ============================================================================
// Brightness / timing
// ============================================================================

template <uint8_t CHANNEL_COUNT>
void LedHandler<CHANNEL_COUNT>::setLedLevels(
    uint8_t activeLevel,
    uint8_t deactiveLevel,
    uint8_t blinkLevel)
{
    constexpr uint8_t MAX_LEVEL = LED_PWM_LEVELS - 1;

    CriticalSection cs;

    ledActiveLevel_ =
        (activeLevel > MAX_LEVEL)
            ? MAX_LEVEL
            : activeLevel;

    ledDeactiveLevel_ =
        (deactiveLevel > MAX_LEVEL)
            ? MAX_LEVEL
            : deactiveLevel;

    ledBlinkLevel_ =
        (blinkLevel > MAX_LEVEL)
            ? MAX_LEVEL
            : blinkLevel;
}

template <uint8_t CHANNEL_COUNT>
void LedHandler<CHANNEL_COUNT>::setLedBlinkTiming(
    uint16_t blinkMs,
    uint16_t blinkingPeriodMs)
{
    CriticalSection cs;

    ledBlinkMs_ = blinkMs;
    ledBlinkingPeriodMs_ = blinkingPeriodMs;
}

// ============================================================================
// Sleep / Wake
// ============================================================================

template <uint8_t CHANNEL_COUNT>
void LedHandler<CHANNEL_COUNT>::sleep()
{
    CriticalSection cs;

    if (sleeping_)
        return;

    for (uint8_t i = 0; i < CHANNEL_COUNT; i++)
    {
        // Saved separately from previosLedMode_ so a pending Blink
        // cannot corrupt the mode restored by wake().
        sleepSavedMode_[i] = baseMode(i);

        ledMode_[i] = LedMode::Sleep;
        ledPhaseTicks_[i] = 0;
        applyModeLevel(i);
    }

    sleeping_ = true;
}

template <uint8_t CHANNEL_COUNT>
void LedHandler<CHANNEL_COUNT>::wake()
{
    CriticalSection cs;

    if (!sleeping_)
        return;

    sleeping_ = false;

    for (uint8_t i = 0; i < CHANNEL_COUNT; i++)
        setLedMode(i, sleepSavedMode_[i]);
}

// ============================================================================
// Timer ISR
// ============================================================================

template <uint8_t CHANNEL_COUNT>
void LedHandler<CHANNEL_COUNT>::pwmIsrTrampoline()
{
    if (pwmInstance_)
        pwmInstance_->pwmTick();
}

// ============================================================================
// PWM
// ============================================================================

template <uint8_t CHANNEL_COUNT>
void LedHandler<CHANNEL_COUNT>::writePin(uint8_t channel, bool high)
{
#if LED_USE_FAST_GPIO
    // Single atomic write: low half sets the pin, high half resets it.
    ledPort_[channel]->BSRR =
        high
            ? ledMask_[channel]
            : (ledMask_[channel] << 16);
#else
    digitalWrite(ledPins_[channel], high ? HIGH : LOW);
#endif
}

template <uint8_t CHANNEL_COUNT>
void LedHandler<CHANNEL_COUNT>::updatePwm()
{
    for (uint8_t i = 0; i < CHANNEL_COUNT; i++)
    {
        const bool on =
            ledLevel_[i] > pwmCounter_;

        writePin(i, activeHigh_ ? on : !on);
    }
}

// ============================================================================
// LED state machine
// ============================================================================

template <uint8_t CHANNEL_COUNT>
typename LedHandler<CHANNEL_COUNT>::LedMode
LedHandler<CHANNEL_COUNT>::baseMode(uint8_t channel) const
{
    return (ledMode_[channel] == LedMode::Blink)
               ? previosLedMode_[channel]
               : ledMode_[channel];
}

// Single source of truth for "which brightness does this mode use".
template <uint8_t CHANNEL_COUNT>
void LedHandler<CHANNEL_COUNT>::applyModeLevel(uint8_t channel)
{
    switch (ledMode_[channel])
    {
    case LedMode::Sleep:
        ledLevel_[channel] = 3;
        ledOn_[channel] = false;
        break;

    case LedMode::Deactive:
        ledLevel_[channel] = ledDeactiveLevel_;
        ledOn_[channel] = false;
        break;

    case LedMode::Active:
        ledLevel_[channel] = ledActiveLevel_;
        ledOn_[channel] = true;
        break;

    case LedMode::Blink:
    case LedMode::Blinking:
        ledLevel_[channel] = ledBlinkLevel_;
        ledOn_[channel] = true;
        break;
    }
}

template <uint8_t CHANNEL_COUNT>
void LedHandler<CHANNEL_COUNT>::tick1ms()
{
    updateLedState();
    updateAnimation();
}

template <uint8_t CHANNEL_COUNT>
void LedHandler<CHANNEL_COUNT>::updateLedState()
{
    for (uint8_t i = 0; i < CHANNEL_COUNT; i++)
    {
        switch (ledMode_[i])
        {
        case LedMode::Sleep:
        case LedMode::Deactive:
        case LedMode::Active:
            // Re-apply every tick so setLedLevels() takes effect
            // on LEDs that are already in a steady mode.
            applyModeLevel(i);
            break;

        case LedMode::Blink:
            updateBlink(i);
            break;

        case LedMode::Blinking:
            updateBlinking(i);
            break;
        }
    }
}

template <uint8_t CHANNEL_COUNT>
void LedHandler<CHANNEL_COUNT>::updateBlink(uint8_t channel)
{
    ledLevel_[channel] = ledBlinkLevel_;

    if (++ledPhaseTicks_[channel] >= ledBlinkMs_)
    {
        ledPhaseTicks_[channel] = 0;

        LedMode next = previosLedMode_[channel];

        // Defensive: never "return" to a one-shot Blink.
        if (next == LedMode::Blink)
            next = LedMode::Deactive;

        ledMode_[channel] = next;
        applyModeLevel(channel);
    }
}

template <uint8_t CHANNEL_COUNT>
void LedHandler<CHANNEL_COUNT>::updateBlinking(uint8_t channel)
{
    if (++ledPhaseTicks_[channel] < ledBlinkingPeriodMs_)
        return;

    ledPhaseTicks_[channel] = 0;

    ledOn_[channel] = !ledOn_[channel];

    ledLevel_[channel] =
        ledOn_[channel]
            ? ledBlinkLevel_
            : ledDeactiveLevel_;
}

// ============================================================================
// Hardware timer
// ============================================================================

template <uint8_t CHANNEL_COUNT>
void LedHandler<CHANNEL_COUNT>::pwmTick()
{
    // ---------------------------------------------------------
    // Software PWM counter
    // ---------------------------------------------------------

    if (++pwmCounter_ >= LED_PWM_LEVELS)
        pwmCounter_ = 0;

    // ---------------------------------------------------------
    // PWM output
    // ---------------------------------------------------------

    updatePwm();

    // ---------------------------------------------------------
    // 1 ms LED state / animation update
    // ---------------------------------------------------------

    msAccumulator_ += 1000;

    if (msAccumulator_ >= LED_PWM_TIMER_HZ)
    {
        msAccumulator_ -= LED_PWM_TIMER_HZ;

        tick1ms();
    }
}

// ============================================================================
// Static timer instance
// ============================================================================

template <uint8_t CHANNEL_COUNT>
LedHandler<CHANNEL_COUNT> *
    LedHandler<CHANNEL_COUNT>::pwmInstance_ = nullptr;

/////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////// Visual Effect ///////////////////////////////////////

template <uint8_t CHANNEL_COUNT>
void LedHandler<CHANNEL_COUNT>::updateAnimation()
{
    switch (animationType_)
    {
    case AnimationType::None:
        break;

    case AnimationType::FindIt:
        updateFindItAnimation();
        break;

    case AnimationType::Startup:
        updateStartupAnimation();
        break;
    }
}

template <uint8_t CHANNEL_COUNT>
void LedHandler<CHANNEL_COUNT>::finditAnimation(uint8_t duration)
{
    CriticalSection cs;

    if (animationType_ != AnimationType::None)
        return;

    // Save the mode each LED returns to. A pending one-shot Blink is
    // resolved to its underlying mode, otherwise the LED would
    // return to Blink and never leave it.
    for (uint8_t i = 0; i < CHANNEL_COUNT; i++)
        animationPreviousMode_[i] = baseMode(i);

    animationType_ = AnimationType::FindIt;

    animationElapsedMs_ = 0;
    animationDurationMs_ =
        static_cast<uint32_t>(duration) * 1000UL;

    animationPreviousBlinkMs_ = ledBlinkMs_;
    animationPreviousPeriodMs_ = ledBlinkingPeriodMs_;

    setLedBlinkTiming(
        ledBlinkMs_,
        200);

    setAllLedMode(LedMode::Blinking);
}

template <uint8_t CHANNEL_COUNT>
void LedHandler<CHANNEL_COUNT>::updateFindItAnimation()
{
    // Called once per 1 ms tick.
    animationElapsedMs_++;

    // duration was converted to milliseconds in finditAnimation().
    if (animationElapsedMs_ >= animationDurationMs_)
    {
        finishAnimation();
    }
}

template <uint8_t CHANNEL_COUNT>
void LedHandler<CHANNEL_COUNT>::startupAnimation()
{
    CriticalSection cs;

    if (animationType_ != AnimationType::None)
        return;

    animationType_ = AnimationType::Startup;

    animationStep_ = 0;
    animationPulseCount_ = 0;

    animationStepElapsedMs_ = 0;
    animationPulseElapsedMs_ = 0;

    animationFrom_ = 0;
    animationTo_ = 8;

    // Remember the user's brightness levels; restored in finishAnimation().
    animationSavedActive_ = ledActiveLevel_;
    animationSavedDeactive_ = ledDeactiveLevel_;
    animationSavedBlink_ = ledBlinkLevel_;

    // Initial state of the first sweep.
    setLedLevels(16, 0, 31);
}

template <uint8_t CHANNEL_COUNT>
void LedHandler<CHANNEL_COUNT>::updateStartupAnimation()
{
    switch (animationStep_)
    {
    // ---------------------------------------------------------
    // 0 → 8
    // ---------------------------------------------------------
    case 0:

        if (updateSweepStep())
        {
            animationStep_ = 1;

            animationFrom_ = 8;
            animationTo_ = 2;
            animationStepElapsedMs_ = 0;

            setLedLevels(16, 8, 31);
        }

        break;

    // ---------------------------------------------------------
    // 8 → 2
    // ---------------------------------------------------------
    case 1:

        if (updateSweepStep())
        {
            animationStep_ = 2;

            animationFrom_ = 2;
            animationTo_ = 12;
            animationStepElapsedMs_ = 0;

            setLedLevels(16, 2, 31);
        }

        break;

    // ---------------------------------------------------------
    // 2 → 12
    // ---------------------------------------------------------
    case 2:

        if (updateSweepStep())
        {
            animationStep_ = 3;

            animationFrom_ = 12;
            animationTo_ = 0;
            animationStepElapsedMs_ = 0;

            setLedLevels(16, 12, 31);
        }

        break;

    // ---------------------------------------------------------
    // 12 → 0
    // ---------------------------------------------------------
    case 3:

        if (updateSweepStep())
        {
            animationStep_ = 4;

            animationFrom_ = 0;
            animationTo_ = 31;
            animationStepElapsedMs_ = 0;

            setLedLevels(16, 0, 31);
        }

        break;

    // ---------------------------------------------------------
    // 0 → 31
    // ---------------------------------------------------------
    case 4:

        if (updateSweepStep())
        {
            animationStep_ = 5;

            animationPulseCount_ = 0;
            animationPulseElapsedMs_ = 0;

            setLedLevels(16, 31, 31);
        }

        break;

    // ---------------------------------------------------------
    // Pulse animation
    // ---------------------------------------------------------
    case 5:

        animationPulseElapsedMs_++;

        // ON → OFF
        if (animationPulseCount_ < 2)
        {
            if (ledDeactiveLevel_ == 31)
            {
                if (animationPulseElapsedMs_ >= 80)
                {
                    animationPulseElapsedMs_ = 0;

                    setLedLevels(
                        16,
                        18,
                        31);
                }
            }
            else
            {
                if (animationPulseElapsedMs_ >= 60)
                {
                    animationPulseElapsedMs_ = 0;

                    animationPulseCount_++;

                    setLedLevels(
                        16,
                        31,
                        31);
                }
            }
        }
        else
        {
            finishAnimation();
        }

        break;
    }
}

template <uint8_t CHANNEL_COUNT>
bool LedHandler<CHANNEL_COUNT>::updateSweepStep()
{
    const int distanceFromMid =
        16 - static_cast<int>(animationFrom_);

    const uint16_t stepDelay =
        12 + static_cast<uint16_t>(
                 distanceFromMid < 0 ? -distanceFromMid : distanceFromMid);

    animationStepElapsedMs_++;

    if (animationStepElapsedMs_ < stepDelay)
        return false;

    animationStepElapsedMs_ = 0;

    if (animationFrom_ == animationTo_)
        return true;

    if (animationFrom_ < animationTo_)
        animationFrom_++;
    else
        animationFrom_--;

    setLedLevels(
        16,
        animationFrom_,
        31);

    return false;
}

template <uint8_t CHANNEL_COUNT>
void LedHandler<CHANNEL_COUNT>::finishAnimation()
{
    CriticalSection cs;

    if (animationType_ == AnimationType::FindIt)
    {
        setLedBlinkTiming(
            animationPreviousBlinkMs_,
            animationPreviousPeriodMs_);

        for (uint8_t i = 0; i < CHANNEL_COUNT; i++)
            setLedMode(i, animationPreviousMode_[i]);
    }
    else if (animationType_ == AnimationType::Startup)
    {
        // Give the user their brightness levels back.
        setLedLevels(
            animationSavedActive_,
            animationSavedDeactive_,
            animationSavedBlink_);
    }

    animationType_ = AnimationType::None;

    animationElapsedMs_ = 0;
    animationDurationMs_ = 0;

    animationStep_ = 0;
    animationPulseCount_ = 0;

    animationStepElapsedMs_ = 0;
    animationPulseElapsedMs_ = 0;
}

/////////////////////////////////////// Visual Effect ///////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////

// ============================================================================
// Explicit template instantiation
// ============================================================================
//
// Must stay at the END of the file: an explicit instantiation only
// instantiates members whose definitions are already visible.

template class LedHandler<4>;
template class LedHandler<6>;
template class LedHandler<8>;
template class LedHandler<10>;