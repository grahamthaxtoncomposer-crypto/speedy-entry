#include "staffpaper/Score.h"

#include <algorithm>

namespace staffpaper
{

static const int kStepSemis[7] = { 0, 2, 4, 5, 7, 9, 11 };

int Pitch::midi() const
{
    return 12 * (octave() + 1) + kStepSemis[step()] + alter;
}

Pitch spellMidi (int midiNote, int keyFifths)
{
    const int pc = ((midiNote % 12) + 12) % 12;
    const int octave = midiNote / 12 - 1;
    // natural steps
    for (int s = 0; s < 7; ++s)
        if (kStepSemis[s] == pc)
        {
            // In keys that alter this step, prefer the respelling that matches the key
            // only for black keys; white keys stay natural.
            return { octave * 7 + s, 0 };
        }
    // black key
    const bool useFlats = keyFifths < 0;
    if (useFlats)
    {
        const int s = [pc] { for (int i = 0; i < 7; ++i) if (kStepSemis[i] == pc + 1) return i; return 0; }();
        return { octave * 7 + s, -1 };
    }
    const int s = [pc] { for (int i = 0; i < 7; ++i) if (kStepSemis[i] == pc - 1) return i; return 0; }();
    return { octave * 7 + s, +1 };
}

const char* clefName (Clef c)
{
    switch (c)
    {
        case Clef::Treble: return "Treble";
        case Clef::Bass: return "Bass";
        case Clef::Alto: return "Alto";
        case Clef::Tenor: return "Tenor";
        case Clef::Soprano: return "Soprano";
        case Clef::MezzoSoprano: return "Mezzo-soprano";
        case Clef::Baritone: return "Baritone";
        case Clef::Treble8vb: return "Treble 8vb";
        case Clef::Treble8va: return "Treble 8va";
        case Clef::Bass8vb: return "Bass 8vb";
        case Clef::Bass8va: return "Bass 8va";
        case Clef::Percussion: return "Percussion";
        default: return "?";
    }
}

int clefMiddleLine (Clef c)
{
    // Diatonic numbers: C4 = 28, so B4 = 34, D3 = 22, ...
    switch (c)
    {
        case Clef::Treble: return 34;       // B4
        case Clef::Bass: return 22;         // D3
        case Clef::Alto: return 28;         // C4
        case Clef::Tenor: return 26;        // A3
        case Clef::Soprano: return 32;      // G4 (C clef on bottom line)
        case Clef::MezzoSoprano: return 30; // E4 (C clef on 2nd line)
        case Clef::Baritone: return 24;     // F3 (C clef on top line)
        case Clef::Treble8vb: return 27;    // sounds an octave lower
        case Clef::Treble8va: return 41;
        case Clef::Bass8vb: return 15;
        case Clef::Bass8va: return 29;
        case Clef::Percussion: return 34;
        default: return 34;
    }
}

int keyAlterForStep (int keyFifths, int step)
{
    // order of sharps: F C G D A E B ; flats: B E A D G C F
    static const int sharps[7] = { 3, 0, 4, 1, 5, 2, 6 };
    static const int flats[7] = { 6, 2, 5, 1, 4, 0, 3 };
    if (keyFifths > 0)
    {
        for (int i = 0; i < std::min (keyFifths, 7); ++i)
            if (sharps[i] == step) return 1;
    }
    else if (keyFifths < 0)
    {
        for (int i = 0; i < std::min (-keyFifths, 7); ++i)
            if (flats[i] == step) return -1;
    }
    return 0;
}

const char* barlineName (Barline b)
{
    switch (b)
    {
        case Barline::Single: return "Normal";
        case Barline::Double: return "Double";
        case Barline::Final: return "Final";
        case Barline::Dashed: return "Dashed";
        case Barline::Dotted: return "Dotted";
        case Barline::Thick: return "Thick";
        case Barline::Invisible: return "Invisible";
        default: return "?";
    }
}

Tick baseTicksForValue (int value)
{
    value = std::clamp (value, 1, 8);
    return (PPQ / 16) << (value - 1); // 1 -> 60 (64th) ... 5 -> 960 ... 8 -> 7680
}

Tick Entry::duration() const
{
    if (grace) return 0;
    const Tick base = baseTicksForValue (value);
    Tick total = base, add = base;
    for (int i = 0; i < dots; ++i)
    {
        add /= 2;
        total += add;
    }
    if (tuplet.active())
        total = total * tuplet.normal / tuplet.actual;
    return total;
}

int Entry::findNote (int dia) const
{
    for (size_t i = 0; i < notes.size(); ++i)
        if (notes[i].pitch.dia == dia) return (int) i;
    return -1;
}

void Entry::sortNotes()
{
    std::sort (notes.begin(), notes.end(), [] (const Note& a, const Note& b) { return a.pitch.dia < b.pitch.dia; });
}

// ---------------------------------------------------------------- Score

Score Score::makeEmpty (int numMeasures, bool grandStaff)
{
    Score s;
    s.staves.push_back ({});
    if (grandStaff)
    {
        Staff bass;
        bass.clef = Clef::Bass;
        s.staves.push_back (bass);
    }
    for (int i = 0; i < std::max (1, numMeasures); ++i)
        s.appendMeasure();
    return s;
}

void Score::appendMeasure()
{
    measures.push_back ({});
    for (auto& st : staves)
        st.measures.push_back ({});
}

void Score::insertMeasure (int index)
{
    index = std::clamp (index, 0, numMeasures());
    measures.insert (measures.begin() + index, MeasureInfo {});
    for (auto& st : staves)
        st.measures.insert (st.measures.begin() + index, StaffMeasure {});
}

void Score::removeMeasure (int index)
{
    if (numMeasures() <= 1 || index < 0 || index >= numMeasures()) return;
    measures.erase (measures.begin() + index);
    for (auto& st : staves)
        st.measures.erase (st.measures.begin() + index);
}

void Score::setGrandStaff (bool on)
{
    if (on && staves.size() == 1)
    {
        Staff bass;
        bass.clef = Clef::Bass;
        bass.midiChannel = staves[0].midiChannel;
        bass.measures.resize (measures.size());
        staves.push_back (bass);
    }
    else if (! on && staves.size() > 1)
    {
        staves.resize (1);
    }
}

TimeSig Score::timeSigAt (int measure) const
{
    TimeSig t = timeSig;
    for (int i = 0; i <= measure && i < numMeasures(); ++i)
        if (measures[i].timeSig) t = *measures[i].timeSig;
    return t;
}

int Score::keyAt (int measure) const
{
    int k = keyFifths;
    for (int i = 0; i <= measure && i < numMeasures(); ++i)
        if (measures[i].keyFifths) k = *measures[i].keyFifths;
    return k;
}

Clef Score::clefAt (int staff, int measure) const
{
    if (staff < 0 || staff >= (int) staves.size()) return Clef::Treble;
    const auto& st = staves[staff];
    Clef c = st.clef;
    for (int i = 0; i <= measure && i < (int) st.measures.size(); ++i)
    {
        if (st.measures[i].clefChange) c = *st.measures[i].clefChange;
        if (i < measure)
            for (const auto& e : st.measures[i].entries)
                if (e.clefBefore) c = *e.clefBefore;
    }
    return c;
}

Clef Score::clefAtEntry (int staff, int measure, int entry) const
{
    Clef c = clefAt (staff, measure);
    if (staff < 0 || staff >= (int) staves.size() || measure < 0 || measure >= numMeasures()) return c;
    const auto& es = staves[staff].measures[measure].entries;
    for (int i = 0; i <= entry && i < (int) es.size(); ++i)
        if (es[i].clefBefore) c = *es[i].clefBefore;
    return c;
}

Tick Score::measureCapacity (int measure) const
{
    if (measure >= 0 && measure < numMeasures() && measures[measure].pickupLength > 0)
        return measures[measure].pickupLength;
    return timeSigAt (measure).measureLength();
}

Tick Score::measureStart (int measure) const
{
    Tick t = 0;
    for (int i = 0; i < measure && i < numMeasures(); ++i)
        t += measureCapacity (i);
    return t;
}

Tick Score::contentLength (int staff, int measure) const
{
    Tick t = 0;
    for (const auto& e : staves[staff].measures[measure].entries)
        t += e.duration();
    return t;
}

int Score::contextAlter (int staff, int measure, int entryIndex, int dia) const
{
    const auto& entries = staves[staff].measures[measure].entries;
    for (int i = std::min (entryIndex, (int) entries.size()) - 1; i >= 0; --i)
    {
        const auto& e = entries[i];
        if (e.rest) continue;
        for (const auto& n : e.notes)
            if (n.pitch.dia == dia) return n.pitch.alter;
    }
    return keyAlterForStep (keyAt (measure), ((dia % 7) + 7) % 7);
}

std::optional<int> Score::displayedAccidental (int staff, int measure, int entryIndex, int noteIndex) const
{
    const auto& e = staves[staff].measures[measure].entries[entryIndex];
    if (e.rest || noteIndex < 0 || noteIndex >= (int) e.notes.size()) return std::nullopt;
    const auto& n = e.notes[noteIndex];
    if (n.accidental == AccidentalMode::Hide) return std::nullopt;
    if (n.accidental == AccidentalMode::Force) return n.pitch.alter;

    // A note tied from the previous entry does not repeat its accidental.
    if (entryIndex > 0)
    {
        const auto& prev = staves[staff].measures[measure].entries[entryIndex - 1];
        for (const auto& pn : prev.notes)
            if (pn.tieToNext && pn.pitch == n.pitch) return std::nullopt;
    }
    if (contextAlter (staff, measure, entryIndex, n.pitch.dia) != n.pitch.alter)
        return n.pitch.alter;
    return std::nullopt;
}

} // namespace staffpaper
