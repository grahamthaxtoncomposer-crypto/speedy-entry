// Staff Paper — notation core: the score model.
// Pure C++17, no JUCE, so it can be unit-tested on its own.
#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace staffpaper
{

using Tick = int64_t;
constexpr Tick PPQ = 960; // ticks per quarter note

// ---------------------------------------------------------------- pitch

// A spelled pitch. `dia` is the diatonic step number counted from C0 (C0 = 0,
// D0 = 1, ... C4 = 28). `alter` is the chromatic alteration in half steps
// (-2 double flat ... +2 double sharp).
struct Pitch
{
    int dia = 28;
    int alter = 0;

    int step() const { return ((dia % 7) + 7) % 7; } // 0 = C ... 6 = B
    int octave() const { return (dia >= 0 ? dia / 7 : (dia - 6) / 7); }
    int midi() const;

    bool operator== (const Pitch& o) const { return dia == o.dia && alter == o.alter; }
    bool operator!= (const Pitch& o) const { return ! (*this == o); }
};

// Spell a MIDI note in a key: sharp keys spell black keys as sharps, flat keys as flats.
Pitch spellMidi (int midiNote, int keyFifths);

// ---------------------------------------------------------------- clefs, keys, meters

enum class Clef
{
    Treble,
    Bass,
    Alto,
    Tenor,
    Soprano,
    MezzoSoprano,
    Baritone,
    Treble8vb,
    Treble8va,
    Bass8vb,
    Bass8va,
    Percussion,
    Count
};

const char* clefName (Clef);
// Diatonic number of the pitch that sits on the middle staff line.
int clefMiddleLine (Clef);

// Alteration a key signature gives to a step. keyFifths: -7 (7 flats) ... +7 (7 sharps).
int keyAlterForStep (int keyFifths, int step);

struct TimeSig
{
    int num = 4;
    int den = 4;

    Tick measureLength() const { return (Tick) num * (4 * PPQ / den); }
    bool isCompound() const { return den >= 8 && num % 3 == 0 && num > 3; }
    bool operator== (const TimeSig& o) const { return num == o.num && den == o.den; }
    bool operator!= (const TimeSig& o) const { return ! (*this == o); }
};

enum class Barline
{
    Single,
    Double,
    Final,
    Dashed,
    Dotted,
    Thick,
    Invisible,
    Count
};
const char* barlineName (Barline);

// ---------------------------------------------------------------- entries

enum class StemDir
{
    Auto,
    Up,
    Down
};

enum class BeamMode
{
    Auto,        // follow the meter's beaming rules
    BreakBefore, // force a beam break before this entry
    JoinBefore   // force a beam join to the previous entry
};

enum class AccidentalMode
{
    Auto,  // shown when needed
    Force, // courtesy accidental
    Hide
};

struct Note
{
    Pitch pitch;
    bool tieToNext = false;
    AccidentalMode accidental = AccidentalMode::Auto;
    bool parentheses = false;
};

// Dynamics, softest to loudest. Each plays a fixed velocity (PlaybackSettings::dynamicVelocity).
enum class Dynamic
{
    pppp, ppp, pp, p, mp, mf, f, ff, fff, ffff,
    Count
};
const char* dynamicName (Dynamic);

struct Tuplet
{
    int actual = 0; // e.g. 3 in "3 in the time of 2"; 0 = no tuplet
    int normal = 0;
    bool active() const { return actual > 0 && normal > 0; }
};

// Duration value as typed on the number keys: 1 = 64th, 2 = 32nd, 3 = 16th,
// 4 = eighth, 5 = quarter, 6 = half, 7 = whole, 8 = double whole.
Tick baseTicksForValue (int value);

struct Entry
{
    int value = 5;   // 1..8, see baseTicksForValue
    int dots = 0;
    Tuplet tuplet;
    bool tupletStart = false; // first entry of a tuplet group (draws the bracket)
    bool rest = false;
    std::vector<Note> notes; // empty when rest

    bool hidden = false;
    bool grace = false;
    bool graceSlash = false; // slashed (acciaccatura) grace note
    StemDir stem = StemDir::Auto;
    BeamMode beam = BeamMode::Auto;
    bool flatBeam = false;

    // articulations / phrasing
    bool staccato = false;
    bool tenuto = false;
    bool accent = false;
    bool slurStart = false;
    bool slurEnd = false;

    // A clef change drawn just before this entry (mid-measure clef change).
    std::optional<Clef> clefBefore;

    // A dynamic mark at this entry; it sets the velocity from here on in this staff.
    std::optional<Dynamic> dynamic;
    // Hand-set velocity from the velocity lane (1..127); 0 = worked out from the dynamics.
    int velocity = 0;

    Tick duration() const; // 0 for grace notes
    int findNote (int dia) const; // index of the note at this staff position, or -1
    void sortNotes();
};

// ---------------------------------------------------------------- measures, staves, score

struct MeasureInfo
{
    std::optional<TimeSig> timeSig; // meter change at this measure
    std::optional<int> keyFifths;   // key change at this measure
    Barline barline = Barline::Single; // barline at the END of the measure
    bool repeatStart = false;
    bool repeatEnd = false;
    int repeatTimes = 2; // total passes for an end repeat
    unsigned endingMask = 0; // bit n set = this measure belongs to ending n+1; 0 = not an ending
    Tick pickupLength = 0; // >0 = this measure is shorter (pickup / anacrusis)
};

// Up to four independent layers (voices) per staff. Layer 1 is the main one;
// when another layer has music, layers 1 and 3 take stems up, 2 and 4 down.
constexpr int kLayers = 4;

struct StaffMeasure
{
    std::optional<Clef> clefChange;
    std::array<std::vector<Entry>, kLayers> layers;

    bool multiLayer() const; // more than one layer has entries
};

struct Staff
{
    Clef clef = Clef::Treble;
    int midiChannel = 1; // 1..16
    std::vector<StaffMeasure> measures;
};

// Defaults follow Classical-era performance practice: ordinary notes are
// slightly detached, staccato is light and short, notes under a slur connect
// (a small overlap), the first note of a slur leans a little and the last
// note of a slur is lifted (shortened and softer). All adjustable.
struct PlaybackSettings
{
    double normalLength = 0.88;   // fraction of written length for plain notes
    double staccatoLength = 0.40; // fraction of written length for staccato
    int staccatoVelocity = -8;    // staccato is lighter
    double tenutoLength = 1.00;
    double legatoOverlap = 0.04;  // slurred notes overlap the next by this fraction
    double slurEndLength = 0.70;  // the last note of a slur is lifted
    int slurStartVelocity = 8;    // the first note of a slur leans slightly
    int slurEndVelocity = -10;    // ...and the last note tapers
    int velocity = 80;            // before the first dynamic mark
    int accentBoost = 20;
    // Fixed velocity for each dynamic, pppp ... ffff.
    std::array<int, (size_t) Dynamic::Count> dynamicVelocity { 12, 22, 34, 46, 58, 70, 84, 98, 112, 124 };
};

struct Score
{
    TimeSig timeSig;   // opening meter
    int keyFifths = 0; // opening key
    std::vector<MeasureInfo> measures;
    std::vector<Staff> staves;
    PlaybackSettings playback;
    std::string title;

    static Score makeEmpty (int numMeasures = 4, bool grandStaff = false);

    int numMeasures() const { return (int) measures.size(); }
    void appendMeasure(); // adds a measure across all staves
    void insertMeasure (int index);
    void removeMeasure (int index);
    void setGrandStaff (bool on);
    bool isGrandStaff() const { return staves.size() >= 2; }

    TimeSig timeSigAt (int measure) const;
    int keyAt (int measure) const;
    Clef clefAt (int staff, int measure) const; // clef in force at the START of a measure
    // Clef in force at an entry (includes mid-measure changes); entry == size() = end of measure.
    Clef clefAtEntry (int staff, int measure, int entry, int layer = 0) const;
    // Clef in force at a time inside a measure (mid-measure changes in any layer count).
    Clef clefAtTick (int staff, int measure, Tick tick) const;
    Tick measureCapacity (int measure) const; // nominal length (pickup aware)
    Tick measureStart (int measure) const;    // written (unrolled-repeat-free) position
    int measureAtTick (Tick t) const;          // measure containing a written position (last one if past the end)
    Tick contentLength (int staff, int measure, int layer = 0) const;
    Tick onsetOf (int staff, int measure, int layer, int entry) const; // relative to the measure start

    // The alteration a note at `dia` gets by default at entry `entryIndex` of a
    // measure: carried accidentals earlier in the measure, else the key signature.
    int contextAlter (int staff, int measure, int entryIndex, int dia, int layer = 0) const;
    // The accidental to draw for a note, or nullopt for none.
    std::optional<int> displayedAccidental (int staff, int measure, int entryIndex, int noteIndex, int layer = 0) const;
};

} // namespace staffpaper
