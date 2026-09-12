// Implementation for LedHandler<CHANNEL_COUNT>.
// Explicitly instantiated below for the channel counts this project
// actually uses (4, 8, 12). Using LedHandler with any other
// CHANNEL_COUNT will fail at link time with "undefined reference" --
// add another explicit instantiation line at the bottom if you need one.

#include "LedHandler.h"

template <uint8_t CHANNEL_COUNT>
void LedHandler<CHANNEL_COUNT>::configure(const uint8_t ledPins[CHANNEL_COUNT], bool activeHigh)
{
    activeHigh_ = activeHigh;
    for (uint8_t i = 0; i < CHANNEL_COUNT; i++)
    {
        ledPins_[i] = ledPins[i];
        ledMode_[i] = LedMode::Deactive;
        ledOn_[i] = false;
        ledPhaseStart_[i] = 0;
        ledLevel_[i] = 0;
    }
}

template <uint8_t CHANNEL_COUNT>
void LedHandler<CHANNEL_COUNT>::begin()
{
    for (uint8_t i = 0; i < CHANNEL_COUNT; i++)
        pinMode(ledPins_[i], OUTPUT);

    initPwmTimer();
}

template <uint8_t CHANNEL_COUNT>
void LedHandler<CHANNEL_COUNT>::updateLeds()
{
    uint32_t now = millis();

    for (uint8_t i = 0; i < CHANNEL_COUNT; i++)
    {
        switch (ledMode_[i])
        {
        case LedMode::Active:
            ledLevel_[i] = ledActiveLevel_;
            break;

        case LedMode::Deactive:
            ledLevel_[i] = ledDeactiveLevel_;
            break;

        case LedMode::Blink:
            // Single flash: stay "on" for ledBlinkMs_, then auto-drop to Deactive.
            if (now - ledPhaseStart_[i] < ledBlinkMs_)
            {
                ledLevel_[i] = ledBlinkLevel_;
            }
            else
            {
                setLedMode(i, LedMode::Deactive);
            }
            break;

        case LedMode::Blinking:
            // Repeating blink until finishOperation() is called from
            // outside (i.e. once the actual operation is confirmed done).
            if (now - ledPhaseStart_[i] >= ledBlinkingPeriodMs_)
            {
                ledPhaseStart_[i] = now;
                ledOn_[i] = !ledOn_[i];
            }
            ledLevel_[i] = ledOn_[i] ? ledBlinkLevel_ : ledDeactiveLevel_;
            break;
        }
    }
}

template <uint8_t CHANNEL_COUNT>
void LedHandler<CHANNEL_COUNT>::setLedMode(uint8_t channel, LedMode mode)
{
    if (channel >= CHANNEL_COUNT)
        return;

    ledMode_[channel] = mode;
    ledPhaseStart_[channel] = millis();
    ledOn_[channel] = true; // any blink sequence starts in its "on" phase
}

template <uint8_t CHANNEL_COUNT>
typename LedHandler<CHANNEL_COUNT>::LedMode LedHandler<CHANNEL_COUNT>::getLedMode(uint8_t channel) const
{
    if (channel >= CHANNEL_COUNT)
        return LedMode::Deactive;
    return ledMode_[channel];
}

template <uint8_t CHANNEL_COUNT>
void LedHandler<CHANNEL_COUNT>::setAllLedMode(LedMode mode)
{
    for (uint8_t i = 0; i < CHANNEL_COUNT; i++)
        setLedMode(i, mode);
}

template <uint8_t CHANNEL_COUNT>
void LedHandler<CHANNEL_COUNT>::finishOperation(uint8_t channel)
{
    setLedMode(channel, LedMode::Deactive);
}

template <uint8_t CHANNEL_COUNT>
void LedHandler<CHANNEL_COUNT>::setLedLevels(uint8_t activeLevel, uint8_t deactiveLevel, uint8_t blinkLevel)
{
    constexpr uint8_t kMax = LED_PWM_LEVELS - 1;
    ledActiveLevel_ = (activeLevel > kMax) ? kMax : activeLevel;
    ledDeactiveLevel_ = (deactiveLevel > kMax) ? kMax : deactiveLevel;
    ledBlinkLevel_ = (blinkLevel > kMax) ? kMax : blinkLevel;
}

template <uint8_t CHANNEL_COUNT>
void LedHandler<CHANNEL_COUNT>::setLedBlinkTiming(uint16_t blinkMs, uint16_t blinkingPeriodMs)
{
    ledBlinkMs_ = blinkMs;
    ledBlinkingPeriodMs_ = blinkingPeriodMs;
}

template <uint8_t CHANNEL_COUNT>
void LedHandler<CHANNEL_COUNT>::finditAnimation(uint8_t duration)
{
    LedMode previousMode[CHANNEL_COUNT];
    for (uint8_t i = 0; i < CHANNEL_COUNT; i++)
        previousMode[i] = ledMode_[i];

    uint16_t prevBlinkMs = ledBlinkMs_;
    uint16_t prevPeriodMs = ledBlinkingPeriodMs_;

    constexpr uint16_t kIdentifyPeriodMs = 200; // fast on/off half-period, easy to spot
    setLedBlinkTiming(prevBlinkMs, kIdentifyPeriodMs);
    setAllLedMode(LedMode::Blinking);

    uint32_t durationMs = static_cast<uint32_t>(duration) * 1000UL;
    uint32_t start = millis();
    while (millis() - start < durationMs)
    {
        updateLeds();
    }

    setLedBlinkTiming(prevBlinkMs, prevPeriodMs);

    for (uint8_t i = 0; i < CHANNEL_COUNT; i++)
        setLedMode(i, previousMode[i]);
}

template <uint8_t CHANNEL_COUNT>
void LedHandler<CHANNEL_COUNT>::startupAnimation()
{
    // for (size_t i = 0; i < CHANNEL_COUNT; i++)
    // {
    //     setLedMode(i, LedMode::Active);
    //     updateLeds();
    //     delay(1000);
    // }

    sweep(0, 8);
    sweep(8, 2);
    sweep(2, 12);
    sweep(12, 0);

    sweep(0, 31);

    for (int i = 0; i < 2; i++)
    {
        setLedLevels(16, 31, 31);
        updateLeds();
        delay(80);

        setLedLevels(16, 18, 31);
        updateLeds();
        delay(60);
    }

    setLedLevels(16, 31, 31);
    updateLeds();
}

template <uint8_t CHANNEL_COUNT>
void LedHandler<CHANNEL_COUNT>::sweep(uint8_t from, uint8_t to)
{
    const uint8_t maxLevel = 31;
    int step = (from < to) ? 1 : -1;

    for (int i = from;; i += step)
    {
        setLedLevels(16, i, maxLevel);
        updateLeds();

        delay(12 + abs(16 - i));

        if (i == to)
            break;
    }
}

template <uint8_t CHANNEL_COUNT>
void LedHandler<CHANNEL_COUNT>::initPwmTimer()
{
    pwmInstance_ = this;

#if defined(ARDUINO_ARCH_STM32)
    pwmTimer_ = new HardwareTimer(TIM4);
    pwmTimer_->setOverflow(static_cast<uint32_t>(LED_PWM_FREQUENCY_HZ) * LED_PWM_LEVELS, HERTZ_FORMAT);
    pwmTimer_->attachInterrupt(pwmIsrTrampoline);
    pwmTimer_->resume();
#endif
}

template <uint8_t CHANNEL_COUNT>
void LedHandler<CHANNEL_COUNT>::pwmIsrTrampoline()
{
    if (pwmInstance_)
        pwmInstance_->pwmTick();
}

template <uint8_t CHANNEL_COUNT>
void LedHandler<CHANNEL_COUNT>::pwmTick()
{
    pwmCounter_++;
    if (pwmCounter_ >= LED_PWM_LEVELS)
        pwmCounter_ = 0;

    for (uint8_t i = 0; i < CHANNEL_COUNT; i++)
    {
        bool on = (ledLevel_[i] > pwmCounter_);
        digitalWrite(ledPins_[i], activeHigh_ ? on : !on);
    }
}

template <uint8_t CHANNEL_COUNT>
LedHandler<CHANNEL_COUNT> *LedHandler<CHANNEL_COUNT>::pwmInstance_ = nullptr;

// --- Explicit template instantiation ---
// Every CHANNEL_COUNT this project uses must be listed here.
template class LedHandler<4>;
template class LedHandler<6>;
template class LedHandler<8>;
template class LedHandler<10>;