#include "staffpaper/Playback.h"

#include <algorithm>
#include <cmath>
#include <map>

namespace staffpaper
{

std::vector<int> playbackOrder (const Score& s)
{
    std::vector<int> order;
    const int n = s.numMeasures();
    int i = 0, pass = 1, sectionStart = 0;
    bool pendingReset = false, jumpedHere = false;
    int guard = 0;
    while (i < n && guard++ < 100000)
    {
        const auto& m = s.measures[i];
        if (pendingReset && m.endingMask == 0)
        {
            pass = 1;
            pendingReset = false;
            sectionStart = i;
        }
        if (m.repeatStart && ! jumpedHere)
        {
            sectionStart = i;
            pass = 1;
        }
        jumpedHere = false;

        if (m.endingMask != 0 && ! (m.endingMask & (1u << std::min (pass - 1, 31))))
        {
            if (m.repeatEnd) pendingReset = true;
            ++i;
            continue;
        }
        order.push_back (i);
        if (m.repeatEnd)
        {
            if (pass < std::max (2, m.repeatTimes))
            {
                ++pass;
                i = sectionStart;
                jumpedHere = true;
                continue;
            }
            pendingReset = true;
        }
        ++i;
    }
    return order;
}

namespace
{
    struct Pending
    {
        Tick start, written;
        int note, channel, velocity;
        bool staccato, tenuto, slurred, slurLast;
        bool tieOpen;
    };
} // namespace

VelocityTable computeVelocities (const Score& s)
{
    const auto& pb = s.playback;
    VelocityTable table (s.staves.size());
    for (size_t st = 0; st < s.staves.size(); ++st)
    {
        const auto& staff = s.staves[st];
        // dynamic marks of this staff by written time (any layer)
        std::vector<std::pair<Tick, int>> marks;
        for (int m = 0; m < s.numMeasures(); ++m)
            for (int l = 0; l < kLayers; ++l)
            {
                Tick t = s.measureStart (m);
                for (const auto& e : staff.measures[(size_t) m].layers[(size_t) l])
                {
                    if (e.dynamic) marks.push_back ({ t, pb.dynamicVelocity[(size_t) *e.dynamic] });
                    t += e.duration();
                }
            }
        std::stable_sort (marks.begin(), marks.end(), [] (const auto& a, const auto& b) { return a.first < b.first; });
        auto levelAt = [&] (Tick t) {
            int v = pb.velocity;
            for (const auto& [mt, mv] : marks)
            {
                if (mt > t) break;
                v = mv;
            }
            return v;
        };

        table[st].resize ((size_t) s.numMeasures());
        for (int l = 0; l < kLayers; ++l)
        {
            bool inSlur = false;
            for (int m = 0; m < s.numMeasures(); ++m)
            {
                const auto& es = staff.measures[(size_t) m].layers[(size_t) l];
                auto& out = table[st][(size_t) m][(size_t) l];
                out.assign (es.size(), 0);
                Tick t = s.measureStart (m);
                for (size_t i = 0; i < es.size(); ++i)
                {
                    const auto& e = es[i];
                    if (! e.rest)
                    {
                        const bool slurFirst = e.slurStart;
                        if (e.slurStart) inSlur = true;
                        const bool slurLast = inSlur && e.slurEnd;
                        if (e.slurEnd) inSlur = false;
                        int vel = levelAt (t);
                        if (e.accent) vel += pb.accentBoost;
                        if (slurFirst) vel += pb.slurStartVelocity;
                        if (slurLast) vel += pb.slurEndVelocity;
                        if (e.staccato) vel += pb.staccatoVelocity;
                        if (e.velocity > 0) vel = e.velocity;
                        out[i] = std::clamp (vel, 1, 127);
                    }
                    t += e.duration();
                }
            }
        }
    }
    return table;
}

Sequence buildSequence (const Score& s)
{
    Sequence seq;
    const auto order = playbackOrder (s);
    const auto& pb = s.playback;
    const auto velocities = computeVelocities (s);

    std::vector<Pending> all;
    for (int st = 0; st < (int) s.staves.size(); ++st)
    for (int layer = 0; layer < kLayers; ++layer)
    {
        const auto& staff = s.staves[st];
        const int channel = std::clamp (staff.midiChannel, 1, 16);
        std::map<int, size_t> openTies; // midi note -> index in `all`
        bool inSlur = false;
        Tick t = 0;
        for (int mi : order)
        {
            Tick pos = t;
            const auto& es = staff.measures[mi].layers[layer];
            for (size_t ei = 0; ei < es.size(); ++ei)
            {
                const auto& e = es[ei];
                const int entryVel = velocities[(size_t) st][(size_t) mi][(size_t) layer][ei];
                const Tick d = e.duration();
                if (e.grace)
                {
                    for (const auto& n : e.notes)
                    {
                        const Tick gl = PPQ / 10;
                        all.push_back ({ std::max<Tick> (0, pos - gl), gl, n.pitch.midi(), channel, entryVel,
                                         false, false, false, false, false });
                    }
                    continue;
                }
                if (e.rest)
                {
                    openTies.clear();
                    pos += d;
                    continue;
                }
                if (e.slurStart) inSlur = true;
                const bool slurLast = inSlur && e.slurEnd;
                const bool slurred = inSlur && ! e.slurEnd;
                if (e.slurEnd) inSlur = false;

                std::map<int, size_t> nextTies;
                for (const auto& n : e.notes)
                {
                    const int midi = std::clamp (n.pitch.midi(), 0, 127);
                    auto it = openTies.find (midi);
                    size_t idx;
                    if (it != openTies.end())
                    {
                        idx = it->second;
                        all[idx].written += d;
                        // the articulation of the last tied note decides the release
                        all[idx].staccato = e.staccato;
                        all[idx].tenuto = e.tenuto;
                        all[idx].slurred = slurred;
                        all[idx].slurLast = slurLast;
                    }
                    else
                    {
                        all.push_back ({ pos, d, midi, channel, std::clamp (entryVel, 1, 127),
                                         e.staccato, e.tenuto, slurred, slurLast, false });
                        idx = all.size() - 1;
                    }
                    if (n.tieToNext) nextTies[midi] = idx;
                }
                openTies = std::move (nextTies);
                pos += d;
            }
            t += s.measureCapacity (mi);
        }
    }

    seq.events.reserve (all.size());
    for (const auto& p : all)
    {
        double f = pb.normalLength;
        if (p.staccato) f = pb.staccatoLength;
        else if (p.tenuto) f = pb.tenutoLength;
        else if (p.slurred) f = 1.0 + pb.legatoOverlap;
        else if (p.slurLast) f = pb.slurEndLength;
        if (p.slurred && p.staccato) f = std::max (pb.staccatoLength, 0.75); // portato under a slur
        const Tick len = std::max<Tick> (10, (Tick) std::llround ((double) p.written * f));
        seq.events.push_back ({ p.start, len, (uint8_t) p.note, (uint8_t) p.velocity, (uint8_t) p.channel });
    }
    std::stable_sort (seq.events.begin(), seq.events.end(), [] (const NoteEvent& a, const NoteEvent& b) { return a.start < b.start; });

    Tick total = 0;
    for (int mi : order) total += s.measureCapacity (mi);
    seq.length = total;
    return seq;
}

// ---------------------------------------------------------------- Scheduler

Scheduler::Scheduler() = default;

void Scheduler::allNotesOff (int sampleOffset, const Out& out)
{
    for (int i = 0; i < count; ++i)
        out (sampleOffset, false, sounding[i].channel, sounding[i].note, 0);
    count = 0;
}

void Scheduler::process (const Sequence* seq, const Block& b, const Out& out)
{
    if (! b.playing || b.numSamples <= 0 || b.beatsPerSample <= 0)
    {
        if (wasPlaying || count > 0) allNotesOff (0, out);
        wasPlaying = false;
        return;
    }

    const double start = b.beat;
    const double end = start + b.numSamples * b.beatsPerSample;

    if (wasPlaying && std::abs (start - expectedBeat) > 1e-4)
        allNotesOff (0, out); // the playhead jumped

    if (b.loopActive && b.loopEnd > b.loopStart && start < b.loopEnd && end > b.loopEnd + 1e-9)
    {
        const int split = std::clamp ((int) std::floor ((b.loopEnd - start) / b.beatsPerSample), 0, b.numSamples);
        segment (seq, b, start, b.loopEnd, 0, out);
        allNotesOff (split, out);
        const double rest = end - b.loopEnd;
        segment (seq, b, b.loopStart, b.loopStart + rest, split, out);
        expectedBeat = b.loopStart + rest;
    }
    else
    {
        segment (seq, b, start, end, 0, out);
        expectedBeat = end;
    }
    wasPlaying = true;
}

void Scheduler::segment (const Sequence* seq, const Block& b, double start, double end, int sampleBase, const Out& out)
{
    auto sampleAt = [&] (double beat) {
        const int s = sampleBase + (int) std::floor ((beat - start) / b.beatsPerSample);
        return std::clamp (s, 0, b.numSamples - 1);
    };

    // note offs due in this segment
    for (int i = 0; i < count;)
    {
        if (sounding[i].offBeat < end)
        {
            out (sampleAt (std::max (sounding[i].offBeat, start)), false, sounding[i].channel, sounding[i].note, 0);
            sounding[i] = sounding[--count];
        }
        else
            ++i;
    }

    if (! seq || seq->events.empty()) return;

    // Score time of this segment. In clip mode the sequence repeats.
    double s0 = start - b.offsetBeats, s1 = end - b.offsetBeats;
    if (s1 <= 0) return;
    double cycleBase = 0;
    if (b.loopSequence && b.sequenceLoopBeats > 0)
    {
        cycleBase = std::floor (s0 / b.sequenceLoopBeats) * b.sequenceLoopBeats;
        s0 -= cycleBase;
        s1 -= cycleBase;
    }

    for (int pass = 0; pass < 2; ++pass)
    {
        const Tick t0 = (Tick) std::ceil (std::max (0.0, s0) * PPQ - 1e-6);
        const double endTicks = s1 * PPQ;
        auto it = std::lower_bound (seq->events.begin(), seq->events.end(), t0,
                                    [] (const NoteEvent& e, Tick t) { return e.start < t; });
        for (; it != seq->events.end() && (double) it->start < endTicks - 1e-6; ++it)
        {
            const double onBeat = b.offsetBeats + cycleBase + (double) it->start / PPQ;
            const double offBeat = onBeat + (double) it->length / PPQ;
            // retrigger: end a sounding note of the same pitch first
            for (int i = 0; i < count; ++i)
                if (sounding[i].note == it->note && sounding[i].channel == it->channel)
                {
                    out (sampleAt (onBeat), false, it->channel, it->note, 0);
                    sounding[i] = sounding[--count];
                    break;
                }
            out (sampleAt (onBeat), true, it->channel, it->note, it->velocity);
            if (offBeat < end)
                out (sampleAt (offBeat), false, it->channel, it->note, 0);
            else if (count < kMaxSounding)
                sounding[count++] = { it->note, it->channel, offBeat };
        }

        // clip mode: the segment may cross the end of the sequence
        if (! (b.loopSequence && b.sequenceLoopBeats > 0 && s1 > b.sequenceLoopBeats)) break;
        cycleBase += b.sequenceLoopBeats;
        s0 = 0;
        s1 -= b.sequenceLoopBeats;
    }
}

} // namespace staffpaper
