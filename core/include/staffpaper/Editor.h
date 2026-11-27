// Staff Paper — the keyboard-driven note entry editor.
// Behaves like a classic "speedy" entry frame: an insertion bar moves through
// the entries of a measure, a crossbar marks the pitch, and number keys enter
// notes of a given value at the crossbar.
#pragma once

#include "staffpaper/KeyMap.h"
#include "staffpaper/Score.h"

#include <functional>
#include <string>
#include <vector>

namespace staffpaper
{

struct Cursor
{
    int staff = 0;
    int measure = 0;
    int entry = 0;   // index of the entry under the insertion bar; == size() means the empty slot at the end
    int dia = 34;    // crossbar pitch (diatonic number), defaults to B4
};

class Editor
{
public:
    explicit Editor (Score& score);

    // Run a command. Returns false when nothing changed (so the caller can let the key through).
    bool perform (Command cmd, int arg = 0);
    bool perform (const Binding& b) { return perform (b.command, b.arg); }

    // MIDI-keyboard entry: if this returns non-empty pitches, number keys enter a chord
    // of these MIDI notes (or a rest when nothing is held and midi entry is on).
    std::function<std::vector<int>()> heldMidiNotes;
    bool midiEntry = false;

    const Cursor& cursor() const { return cur; }
    void setCursor (Cursor c);
    bool insertMode() const { return insert; }
    const Tuplet& pendingTuplet() const { return tupletPending; }
    const std::string& lastMessage() const { return message; }

    bool canUndo() const { return ! undoStack.empty(); }
    bool canRedo() const { return ! redoStack.empty(); }
    void checkpoint(); // push an undo snapshot (call before external edits, e.g. from menus)
    void edited() { changed(); }
    // Start a tuplet: the next `actual` entries play in the time of `normal`.
    void startTuplet (int actual, int normal); // call after editing the score directly (menus)
    void clearHistory();

    // Index of the entry that edit commands (dot, accidentals, ties...) act on:
    // the entry under the insertion bar, or the one just entered when the bar
    // sits in the empty slot at the end of the measure. -1 if none.
    int targetEntry() const;

    // Called after every change; the plugin rebuilds playback from it.
    std::function<void()> onChange;

private:
    Score& score;
    Cursor cur;
    bool insert = false;
    Tuplet tupletPending;
    int tupletRemaining = 0;
    std::string message;

    std::vector<Score> undoStack, redoStack;

    // The entry most recently entered; edit keys act on it until the cursor moves.
    struct Ref
    {
        bool valid = false;
        int staff = 0, measure = 0, entry = 0;
    } last;

    std::vector<Entry>& entries() { return score.staves[cur.staff].measures[cur.measure].entries; }
    bool targetRef (Ref& out) const;
    Entry* target();
    Note* targetNote (Entry& e);
    Entry* previousEntry (const Ref& r, Ref* where = nullptr);

    bool enterValue (int value, bool forceInsert, bool asRest);
    Entry makeEntry (int value, bool asRest);
    int fitMeasure (int staff, int measure); // returns entries carried into the next measure
    void afterEdit (int staff, int measure);
    void ensureMeasure (int index);
    void clampCursor();
    void changed();
    void navigated() { last.valid = false; }
    bool moveEntry (int delta);
    bool alterTarget (int delta, bool wholeMeasure);
    bool enharmonic (bool wholeMeasure);
};

// Split a length into tie-able note values (largest first, dotted where possible).
std::vector<std::pair<int, int>> decomposeTicks (Tick length); // {value, dots}

} // namespace staffpaper
