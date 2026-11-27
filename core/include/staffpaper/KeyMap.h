// Staff Paper — keyboard map: key strokes -> editor commands.
// The map is plain text so it can be corrected without rebuilding.
#pragma once

#include <map>
#include <string>
#include <vector>

namespace staffpaper
{

enum class Command
{
    None,
    // entry
    EnterValue,         // arg = 1..8; enter/replace with a note of that value at the cursor pitch
    InsertValue,        // arg = 1..8; insert a note before the insertion point
    EnterRestValue,     // arg = 1..8; enter a rest of that value
    Dot,
    StartTuplet,        // arg = 3..9 (n in the time of the next lower power of two)
    TupletDialog,       // custom ratio (handled by the UI, which calls Editor::startTuplet)
    AddChordNote,       // add the pitch at the crossbar to the entry (or turn a rest into a note)
    RemoveChordNote,
    ToggleRest,         // note <-> rest
    DeleteEntry,
    // pitch
    RaiseHalfStep,
    LowerHalfStep,
    RaiseHalfStepMeasure,
    LowerHalfStepMeasure,
    Enharmonic,
    EnharmonicMeasure,
    ToggleAccidental,   // show/hide; forces a courtesy accidental when none would show
    AccidentalParens,
    // ties, beams, stems
    TieNext,
    TiePrevious,
    BreakJoinBeam,
    ResetBeams,
    FlatBeam,
    FreezeStem,         // cycles up / down
    AutoStem,
    HideEntry,
    GraceNote,
    // articulations and phrasing (playback-aware)
    Staccato,
    Tenuto,
    Accent,
    SlurStart,
    SlurEnd,
    // navigation
    CursorUp,
    CursorDown,
    OctaveUp,
    OctaveDown,
    PrevEntry,
    NextEntry,
    PrevMeasure,
    NextMeasure,
    StaffUp,
    StaffDown,
    ToggleInsertMode,
    // general
    Undo,
    Redo,
    MeasureMenu,
    Count
};

const char* commandName (Command);
Command commandFromName (const std::string&);

// Modifier bits. "ctrl" means Ctrl on Windows/Linux and Command on macOS.
enum Mods : unsigned
{
    ModShift = 1u,
    ModCtrl = 2u,
    ModAlt = 4u, // Alt / Option
};

// Named non-character keys live above the Unicode range we care about.
enum SpecialKey : int
{
    KeyUp = 0x110000,
    KeyDown,
    KeyLeft,
    KeyRight,
    KeyEnter,
    KeyBackspace,
    KeyDelete,
    KeyInsert,
    KeyTab,
    KeyEscape,
    KeyHome,
    KeyEnd,
    KeyPageUp,
    KeyPageDown,
    KeySpace,
};

struct KeyStroke
{
    int key = 0;       // lowercase character code, or a SpecialKey
    unsigned mods = 0; // Mods bits
    bool operator< (const KeyStroke& o) const { return key != o.key ? key < o.key : mods < o.mods; }
    bool operator== (const KeyStroke& o) const { return key == o.key && mods == o.mods; }
};

std::string describeKey (const KeyStroke&);
bool parseKey (const std::string& text, KeyStroke& out);

struct Binding
{
    Command command = Command::None;
    int arg = 0;
};

class KeyMap
{
public:
    static KeyMap defaults();
    static const char* defaultText();

    // Parse "key = Command [arg]" lines; '#' starts a comment. Returns problems found.
    std::vector<std::string> load (const std::string& text);
    std::string save() const;

    Binding lookup (const KeyStroke& k) const;
    void bind (const KeyStroke& k, Binding b) { map[k] = b; }

private:
    std::map<KeyStroke, Binding> map;
};

} // namespace staffpaper
