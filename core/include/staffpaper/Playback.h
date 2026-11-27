// Staff Paper — turning the score into timed MIDI notes, and playing them in
// step with the host transport.
#pragma once

#include "staffpaper/Score.h"

#include <cstdint>
#include <functional>
#include <vector>

namespace staffpaper
{

struct NoteEvent
{
    Tick start = 0;
    Tick length = 0;
    uint8_t note = 60;
    uint8_t velocity = 80;
    uint8_t channel = 1; // 1..16
};

struct Sequence
{
    std::vector<NoteEvent> events; // sorted by start
    Tick length = 0;               // total length in ticks (unrolled repeats)
};

// Measure indices in performance order (repeats and endings unrolled).
std::vector<int> playbackOrder (const Score& s);

// Flatten the score into notes: ties merged, articulations and slurs applied.
Sequence buildSequence (const Score& s);

// Real-time scheduler: no allocation after construction.
class Scheduler
{
public:
    Scheduler();

    struct Block
    {
        bool playing = false;
        double beat = 0;           // host position at the first sample, in quarter notes
        double beatsPerSample = 0; // tempo / 60 / sampleRate
        int numSamples = 0;
        bool loopActive = false;
        double loopStart = 0, loopEnd = 0;
        double offsetBeats = 0;    // host beat where the score's tick 0 sits
        bool loopSequence = false; // clip mode: the sequence repeats every `sequenceLoopBeats`
        double sequenceLoopBeats = 0;
    };

    // out(sampleOffset, noteOn, channel 1..16, note, velocity)
    using Out = std::function<void (int, bool, int, int, int)>;

    void process (const Sequence* seq, const Block& b, const Out& out);
    void allNotesOff (int sampleOffset, const Out& out);
    void reset() { wasPlaying = false; count = 0; }

private:
    struct Sounding
    {
        uint8_t note, channel;
        double offBeat; // host beats
    };
    static constexpr int kMaxSounding = 512;
    Sounding sounding[kMaxSounding];
    int count = 0;
    bool wasPlaying = false;
    double expectedBeat = 0;

    void segment (const Sequence* seq, const Block& b, double start, double end, int sampleBase, const Out& out);
};

} // namespace staffpaper
