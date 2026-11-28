#include "staffpaper/Layout.h"

#include <algorithm>
#include <cstdlib>

namespace staffpaper
{

std::vector<Tick> entryOnsets (const std::vector<Entry>& entries)
{
    std::vector<Tick> out;
    out.reserve (entries.size());
    Tick t = 0;
    for (const auto& e : entries)
    {
        out.push_back (t);
        t += e.duration();
    }
    return out;
}

Tick beamGroupLength (const TimeSig& t)
{
    if (t.isCompound()) return (Tick) 3 * (4 * PPQ / t.den);
    if (t.den >= 8) return PPQ;          // e.g. 5/8, 7/8: beam in quarters
    if (t.den == 2) return 2 * PPQ;      // cut time: four eighths per beam
    return 4 * PPQ / t.den;              // simple meters: one beat
}

bool isBeamable (const Entry& e)
{
    return ! e.rest && e.value <= 4; // eighth or shorter
}

std::vector<bool> beamedToPrevious (const Score& s, int staff, int measure, int layer)
{
    const auto& es = s.staves[staff].measures[measure].layers[layer];
    const auto onsets = entryOnsets (es);
    const Tick g = beamGroupLength (s.timeSigAt (measure));
    std::vector<bool> out (es.size(), false);
    for (size_t i = 1; i < es.size(); ++i)
    {
        const auto& a = es[i - 1];
        const auto& b = es[i];
        if (! isBeamable (a) || ! isBeamable (b) || a.grace != b.grace) continue;
        if (b.beam == BeamMode::BreakBefore) continue;
        if (b.beam == BeamMode::JoinBefore) { out[i] = true; continue; }
        // tuplet groups beam together
        if (a.tuplet.active() && b.tuplet.active() && ! b.tupletStart) { out[i] = true; continue; }
        if (b.grace) { out[i] = true; continue; }
        out[i] = (onsets[i - 1] / g) == (onsets[i] / g);
    }
    return out;
}

std::vector<std::vector<int>> beamGroups (const Score& s, int staff, int measure, int layer)
{
    const auto joined = beamedToPrevious (s, staff, measure, layer);
    std::vector<std::vector<int>> groups;
    std::vector<int> cur;
    for (int i = 0; i < (int) joined.size(); ++i)
    {
        if (! joined[i])
        {
            if (cur.size() >= 2) groups.push_back (cur);
            cur.clear();
        }
        cur.push_back (i);
    }
    if (cur.size() >= 2) groups.push_back (cur);
    return groups;
}

int staffPosition (int dia, Clef clef)
{
    return dia - clefMiddleLine (clef);
}

int layerStemRule (const StaffMeasure& m, int layer)
{
    if (! m.multiLayer()) return 0;
    return (layer % 2 == 0) ? +1 : -1;
}

bool autoStemUp (const Entry& e, Clef clef, int layerRule)
{
    if (e.stem == StemDir::Up) return true;
    if (e.stem == StemDir::Down) return false;
    if (layerRule != 0) return layerRule > 0;
    if (e.notes.empty()) return true;
    const int lo = staffPosition (e.notes.front().pitch.dia, clef);
    const int hi = staffPosition (e.notes.back().pitch.dia, clef);
    // furthest note from the middle line decides; ties go down
    return std::abs (lo) > std::abs (hi) ? lo < 0 : hi < 0;
}

bool groupStemUp (const std::vector<Entry>& entries, const std::vector<int>& group, Clef clef, int layerRule)
{
    for (int i : group)
    {
        const auto& e = entries[i];
        if (e.stem == StemDir::Up) return true;
        if (e.stem == StemDir::Down) return false;
    }
    if (layerRule != 0) return layerRule > 0;
    int lo = 1000, hi = -1000;
    for (int i : group)
        for (const auto& n : entries[i].notes)
        {
            const int p = staffPosition (n.pitch.dia, clef);
            lo = std::min (lo, p);
            hi = std::max (hi, p);
        }
    if (lo > hi) return true;
    return std::abs (lo) > std::abs (hi) ? lo < 0 : hi < 0;
}

} // namespace staffpaper
