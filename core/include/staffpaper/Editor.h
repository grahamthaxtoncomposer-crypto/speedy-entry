// Staff Paper — the keyboard-driven note entry editor.
// Behaves like a classic "speedy" entry frame: an insertion bar moves through
// the entries of a measure, a crossbar marks the pitch, and number keys enter
// notes of a given value at the crossbar.
#pragma once

#include "staffpaper/KeyMap.h"
#include "staffpaper/Score.h"

#include <array>
#include <functional>
#include <optional>
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
    int layer = 0;   // 0..kLayers-1
};

// A selected stretch of music: whole staves, from one written time to another.
struct Selection
{
    bool active = false;
    int staffFrom = 0, staffTo = 0; // inclusive
    Tick start = 0, end = 0;        // written ticks from the start of the score

    bool contains (int staff, Tick t) const { return active && staff >= staffFrom && staff <= staffTo && t >= start && t < end; }
};

// Copied music: for each staff, every layer as a run of entries starting at the copy start.
struct Clipboard
{
    Tick length = 0;
    std::vector<std::array<std::vector<Entry>, kLayers>> staves;
    bool empty() const { return staves.empty(); }
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
    void setCursor (Cursor c); // also opens the entry frame there

    // The entry frame (the enlarged measure). When it is closed, entry keys do nothing.
    bool frameActive() const { return frame; }
    void setFrameActive (bool on);

    const Selection& selection() const { return sel; }
    void setSelection (const Selection& s) { sel = s; if (onChange) onChange(); }
    void clearSelection() { sel.active = false; }
    // Written position of the insertion bar.
    Tick cursorTick() const;

    // Shared by every editor, so music can be copied from one clip to another.
    static Clipboard& clipboard();

    // Play these MIDI notes once, short (on the given channel 1..16).
    std::function<void (const std::vector<int>& notes, int channel)> onAudition;
    bool insertMode() const { return insert; }
    void setInsertMode (bool on) { insert = on; if (onChange) onChange(); }
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

    // Selection editing (also used by the mouse tools).
    void copySelection();
    bool clearSelectionMusic();
    bool pasteAtCursor();
    bool setDynamic (int dynamicIndex); // 0..9 = pppp..ffff on the target entry; toggles off when the same

    // Move the note at the crossbar (or the whole entry, or every selected note) to
    // the staff above (dir -1) or below (+1). It keeps its rhythm: it joins a chord
    // there when the rhythm matches, else it goes into the first layer with room.
    bool moveToStaff (int dir);
    // Single staff -> grand staff splits the music at middle C (C4 and up stay on
    // top). Grand staff -> single staff brings the lower staff's notes back up.
    bool setGrandStaff (bool on);
    // Put a clef at the insertion bar (the staff's opening clef at the very start).
    bool setClefAtCursor (Clef c);

    // What happens when a note or rest doesn't fit in the rest of the measure.
    enum class Overflow
    {
        Ask,   // call onOverflow and wait for resolveOverflow (Push when nobody asks)
        Tie,   // split it with a tie over the barline, writing over the start of the next measure
        Push,  // split it with a tie and push the following music along (rebar)
        Meter, // give the measure a time signature that fits
        Leave  // leave the measure too full
    };
    Overflow overflowPolicy = Overflow::Ask;
    std::function<void()> onOverflow;
    bool overflowPending() const { return pendingOverflow.valid; }
    // Settle the measure that was too full. Ask = cancel (undo the entry).
    void resolveOverflow (Overflow choice);
    // A meter that fits the too-full measure (e.g. 5/4), if there is one.
    std::optional<TimeSig> fittingMeter() const;

    // Called after every change; the plugin rebuilds playback from it.
    std::function<void()> onChange;

private:
    Score& score;
    Cursor cur;
    bool insert = false;
    Tuplet tupletPending;
    int tupletRemaining = 0;
    std::string message;
    bool frame = true;
    Selection sel;
    Tick selAnchor = -1;

    std::vector<Score> undoStack, redoStack;

    // The entry most recently entered; edit keys act on it until the cursor moves.
    struct Ref
    {
        bool valid = false;
        int staff = 0, measure = 0, entry = 0, layer = 0;
    } last;

    std::vector<Entry>& entries() { return score.staves[cur.staff].measures[cur.measure].layers[cur.layer]; }
    std::vector<Entry>& entriesAt (const Ref& r) { return score.staves[r.staff].measures[r.measure].layers[r.layer]; }
    bool targetRef (Ref& out) const;
    Entry* target();
    Note* targetNote (Entry& e);
    Entry* previousEntry (const Ref& r, Ref* where = nullptr);

    bool enterValue (int value, bool forceInsert, bool asRest);
    Entry makeEntry (int value, bool asRest);
    int fitMeasure (int staff, int measure, int layer, bool overwrite = false); // returns entries carried into the next measure
    Ref pendingOverflow;
    std::optional<Overflow> forcedOverflow;
    void afterEdit (int staff, int measure, int layer);
    void audition (const Ref& r);
    void extendSelection (int delta);
    Tick removeRange (int staff, int layer, Tick from, Tick to); // returns the end of what was removed
    void insertAt (int staff, int layer, Tick at, std::vector<Entry> entries);
    void ensureMeasure (int index);
    void placeNotes (int staff, int preferredLayer, Tick at, const Entry& rhythm, const std::vector<Note>& notes);
    static void makeRest (Entry& e);
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
