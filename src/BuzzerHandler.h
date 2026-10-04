#pragma once

#include <Arduino.h>

// Note struct must be defined before any array of Notes is declared.
struct BuzzerNote
{
    uint16_t freq;
    uint16_t durationMs;
    uint16_t gapMs;
};

// Tiny UI click — single crisp tick
static const BuzzerNote clickSeq[] = {
    {5000, 8, 0},
};

// Soft confirm — two quick ascending taps
static const BuzzerNote confirmSeq[] = {
    {3000, 20, 10},
    {4000, 20, 0},
};

// Success — bright ascending triad
static const BuzzerNote successSeq[] = {
    {1500, 40, 20},
    {2200, 40, 20},
    {3000, 80, 0},
};

// Error — flat, harsh, low buzz repeated
static const BuzzerNote errorSeq[] = {
    {400, 60, 40},
    {400, 60, 40},
    {400, 60, 0},
};

// Warning — two low flat pulses
static const BuzzerNote warningSeq[] = {
    {1000, 70, 50},
    {1000, 70, 0},
};

// Startup jingle — three rising steps
static const BuzzerNote startupSeq[] = {
    {1200, 40, 30},
    {1800, 40, 30},
    {2600, 90, 0},
};

// Alarm — fast repeating stutter, urgent feel
static const BuzzerNote alarmSeq[] = {
    {2500, 50, 40},
    {2500, 50, 40},
    {2500, 50, 40},
    {2500, 50, 40},
};

// Coin / pickup — quick bright blip pair, video-game style
static const BuzzerNote coinSeq[] = {
    {4000, 15, 5},
    {6000, 25, 0},
};

// Laser / zap — fast descending sweep feel
static const BuzzerNote laserSeq[] = {
    {9000, 10, 0},
    {7000, 10, 0},
    {5000, 10, 0},
    {3000, 15, 0},
};

// Notification — gentle two-tone chime, slower than glassChime
static const BuzzerNote notifySeq[] = {
    {3200, 30, 40},
    {4200, 30, 0},
};

// Glass chime — bright ascending shimmer, faster/lighter than notifySeq
static const BuzzerNote glassChimeSeq[] = {
    {3500, 25, 15},
    {5000, 25, 15},
    {7000, 35, 0},
};

// Descending sad tone — for cancel/fail feedback
static const BuzzerNote cancelSeq[] = {
    {2000, 40, 20},
    {1500, 40, 20},
    {1000, 60, 0},
};

// Heartbeat / duplicate-ID pulse — two short thuds
static const BuzzerNote heartbeatSeq[] = {
    {800, 30, 20},
    {800, 30, 0},
};

class BuzzerHandler
{
public:
    using Note = BuzzerNote;

    explicit BuzzerHandler(uint8_t pin) : _pin(pin)
    {
        pinMode(_pin, OUTPUT);
    }

    // ---- All effects go through the millis()-based sequencer below. -------
    // Nothing here calls delay() or relies on back-to-back tone() calls, so
    // every method returns immediately and is always safe to call from
    // loop() without stalling Bus.poll() or anything else.
    //
    // Calling a new effect while one is already playing cleanly replaces it
    // (see playSequence()) — it never hangs or blocks waiting for the old
    // one to finish.

    void click()
    {
        playSequence(clickSeq, sizeof(clickSeq) / sizeof(clickSeq[0]));
    }

    void glassChime()
    {
        playSequence(glassChimeSeq, sizeof(glassChimeSeq) / sizeof(glassChimeSeq[0]));
    }

    void confirmBeep()
    {
        playSequence(confirmSeq, sizeof(confirmSeq) / sizeof(confirmSeq[0]));
    }

    void success()
    {
        playSequence(successSeq, sizeof(successSeq) / sizeof(successSeq[0]));
    }

    void warning()
    {
        playSequence(warningSeq, sizeof(warningSeq) / sizeof(warningSeq[0]));
    }

    void error()
    {
        playSequence(errorSeq, sizeof(errorSeq) / sizeof(errorSeq[0]));
    }

    void startup()
    {
        playSequence(clickSeq, sizeof(clickSeq) / sizeof(clickSeq[0]));
        // playSequence(startupSeq, sizeof(startupSeq) / sizeof(startupSeq[0]));
    }

    void alarm()
    {
        playSequence(alarmSeq, sizeof(alarmSeq) / sizeof(alarmSeq[0]));
    }

    void coin()
    {
        playSequence(coinSeq, sizeof(coinSeq) / sizeof(coinSeq[0]));
    }

    void laser()
    {
        playSequence(laserSeq, sizeof(laserSeq) / sizeof(laserSeq[0]));
    }

    void notify()
    {
        playSequence(notifySeq, sizeof(notifySeq) / sizeof(notifySeq[0]));
    }

    void cancel()
    {
        playSequence(cancelSeq, sizeof(cancelSeq) / sizeof(cancelSeq[0]));
    }

    void heartbeat()
    {
        playSequence(heartbeatSeq, sizeof(heartbeatSeq) / sizeof(heartbeatSeq[0]));
    }

    // count identical beeps, built on the fly into a small internal buffer
    // and played through the same non-blocking sequencer. Capped at
    // MAX_COUNT_BEEPS to keep the buffer static (no heap allocation).
    void countBeeps(uint8_t count, uint16_t freq = 2000, uint16_t durationMs = 60, uint16_t gapMs = 40)
    {
        if (count == 0)
            return;
        if (count > MAX_COUNT_BEEPS)
            count = MAX_COUNT_BEEPS;

        for (uint8_t i = 0; i < count; i++)
        {
            bool isLast = (i == count - 1);
            _countBeepsBuf[i] = {freq, durationMs, isLast ? (uint16_t)0 : gapMs};
        }
        playSequence(_countBeepsBuf, count);
    }

    // ---- millis()-based sequencer -----------------------------------------
    // Call poll() from loop() every iteration; it advances through the note
    // list on its own timing and never blocks. Starting a new sequence while
    // one is active immediately replaces it — no waiting, no stalling.
    void playSequence(const Note *notes, uint8_t count)
    {
        if (count == 0)
            return;
        _seq = notes;
        _seqLen = count;
        _seqIndex = 0;
        _seqLastMs = millis();
        _seqPlaying = true;
        tone(_pin, _seq[0].freq, _seq[0].durationMs);
    }

    void poll()
    {
        if (!_seqPlaying)
            return;

        const Note &n = _seq[_seqIndex];
        uint32_t elapsed = millis() - _seqLastMs;

        if (elapsed >= (uint32_t)n.durationMs + n.gapMs)
        {
            _seqIndex++;
            if (_seqIndex >= _seqLen)
            {
                _seqPlaying = false;
                return;
            }
            _seqLastMs = millis();
            tone(_pin, _seq[_seqIndex].freq, _seq[_seqIndex].durationMs);
        }
    }

    bool isSeqPlaying() const { return _seqPlaying; }

private:
    static constexpr uint8_t MAX_COUNT_BEEPS = 16;

    uint8_t _pin;

    const Note *_seq = nullptr;
    uint8_t _seqLen = 0;
    uint8_t _seqIndex = 0;
    uint32_t _seqLastMs = 0;
    bool _seqPlaying = false;

    Note _countBeepsBuf[MAX_COUNT_BEEPS];
};