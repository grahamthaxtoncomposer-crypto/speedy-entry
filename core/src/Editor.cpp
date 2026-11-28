#include "staffpaper/Editor.h"

#include "staffpaper/Layout.h"

#include <algorithm>
#include <cstdlib>

namespace staffpaper
{

std::vector<std::pair<int, int>> decomposeTicks (Tick length)
{
    std::vector<std::pair<int, int>> out;
    while (length >= baseTicksForValue (1))
    {
        bool took = false;
        for (int v = 8; v >= 1 && ! took; --v)
            for (int dots = 1; dots >= 0; --dots)
            {
                Entry probe;
                probe.value = v;
                probe.dots = dots;
                if (dots == 1 && v == 1) continue;
                if (probe.duration() <= length)
                {
                    out.push_back ({ v, dots });
                    length -= probe.duration();
                    took = true;
                    break;
                }
            }
        if (! took) break;
    }
    return out;
}

Editor::Editor (Score& s) : score (s)
{
    clampCursor();
}

void Editor::setCursor (Cursor c)
{
    cur = c;
    frame = true;
    clampCursor();
    navigated();
}

void Editor::setFrameActive (bool on)
{
    frame = on;
    message = on ? "" : "Entry frame closed (Esc opens it again)";
    if (onChange) onChange();
}

Clipboard& Editor::clipboard()
{
    static Clipboard c;
    return c;
}

Tick Editor::cursorTick() const
{
    return score.measureStart (cur.measure) + score.onsetOf (cur.staff, cur.measure, cur.layer, cur.entry);
}

void Editor::audition (const Ref& r)
{
    if (! onAudition || ! r.valid) return;
    const auto& es = score.staves[r.staff].measures[r.measure].layers[r.layer];
    if (r.entry < 0 || r.entry >= (int) es.size() || es[r.entry].rest) return;
    std::vector<int> notes;
    for (const auto& n : es[r.entry].notes) notes.push_back (std::clamp (n.pitch.midi(), 0, 127));
    onAudition (notes, std::clamp (score.staves[r.staff].midiChannel, 1, 16));
}

void Editor::clampCursor()
{
    if (score.staves.empty()) score = Score::makeEmpty();
    cur.staff = std::clamp (cur.staff, 0, (int) score.staves.size() - 1);
    cur.layer = std::clamp (cur.layer, 0, kLayers - 1);
    cur.measure = std::clamp (cur.measure, 0, score.numMeasures() - 1);
    cur.entry = std::clamp (cur.entry, 0, (int) entries().size());
    cur.dia = std::clamp (cur.dia, 0, 74);
}

void Editor::checkpoint()
{
    undoStack.push_back (score);
    if (undoStack.size() > 300) undoStack.erase (undoStack.begin());
    redoStack.clear();
}

void Editor::clearHistory()
{
    undoStack.clear();
    redoStack.clear();
}

void Editor::changed()
{
    clampCursor();
    if (onChange) onChange();
}

void Editor::ensureMeasure (int index)
{
    while (score.numMeasures() <= index)
        score.appendMeasure();
}

// ---------------------------------------------------------------- targets

bool Editor::targetRef (Ref& out) const
{
    if (last.valid && last.staff < (int) score.staves.size() && last.measure < score.numMeasures()
        && last.entry < (int) score.staves[last.staff].measures[last.measure].layers[last.layer].size())
    {
        out = last;
        return true;
    }
    const auto& es = score.staves[cur.staff].measures[cur.measure].layers[cur.layer];
    if (cur.entry < (int) es.size())
    {
        out = { true, cur.staff, cur.measure, cur.entry, cur.layer };
        return true;
    }
    if (! es.empty())
    {
        out = { true, cur.staff, cur.measure, (int) es.size() - 1, cur.layer };
        return true;
    }
    return false;
}

int Editor::targetEntry() const
{
    Ref r;
    if (! targetRef (r) || r.measure != cur.measure || r.staff != cur.staff || r.layer != cur.layer) return -1;
    return r.entry;
}

Entry* Editor::target()
{
    Ref r;
    if (! targetRef (r)) return nullptr;
    return &entriesAt (r)[r.entry];
}

Note* Editor::targetNote (Entry& e)
{
    if (e.rest || e.notes.empty()) return nullptr;
    const int i = e.findNote (cur.dia);
    if (i >= 0) return &e.notes[i];
    if (e.notes.size() == 1) return &e.notes[0];
    return nullptr;
}

Entry* Editor::previousEntry (const Ref& r, Ref* where)
{
    int m = r.measure, i = r.entry - 1;
    while (m >= 0)
    {
        auto& es = score.staves[r.staff].measures[m].layers[r.layer];
        if (i >= 0 && i < (int) es.size())
        {
            if (where) *where = { true, r.staff, m, i, r.layer };
            return &es[i];
        }
        --m;
        if (m >= 0) i = (int) score.staves[r.staff].measures[m].layers[r.layer].size() - 1;
    }
    return nullptr;
}

// ---------------------------------------------------------------- entering

Entry Editor::makeEntry (int value, bool asRest)
{
    Entry e;
    e.value = value;
    if (asRest)
    {
        e.rest = true;
    }
    else if (midiEntry && heldMidiNotes)
    {
        const auto held = heldMidiNotes();
        if (held.empty())
            e.rest = true;
        else
        {
            const int key = score.keyAt (cur.measure);
            for (int n : held)
                e.notes.push_back ({ spellMidi (n, key) });
            e.sortNotes();
            cur.dia = e.notes.front().pitch.dia;
        }
    }
    else
    {
        Note n;
        n.pitch.dia = cur.dia;
        n.pitch.alter = score.contextAlter (cur.staff, cur.measure, cur.entry, cur.dia, cur.layer);
        e.notes.push_back (n);
    }

    if (tupletPending.active() && tupletRemaining > 0)
    {
        e.tuplet = tupletPending;
        e.tupletStart = (tupletRemaining == tupletPending.actual);
        if (--tupletRemaining == 0) tupletPending = {};
    }
    return e;
}

bool Editor::enterValue (int value, bool forceInsert, bool asRest)
{
    checkpoint();
    auto& es = entries();
    const bool onEntry = cur.entry < (int) es.size();

    if (onEntry && ! insert && ! forceInsert)
    {
        // A number key on an existing entry changes its value.
        auto& e = es[cur.entry];
        e.value = value;
        e.dots = 0;
        if (asRest)
        {
            e.rest = true;
            e.notes.clear();
        }
        last = { true, cur.staff, cur.measure, cur.entry, cur.layer };
        cur.entry++;
    }
    else
    {
        Entry e = makeEntry (value, asRest);
        // a layer other than 1 may start part way into the measure: nothing to do, it starts where it is
        es.insert (es.begin() + cur.entry, e);
        last = { true, cur.staff, cur.measure, cur.entry, cur.layer };
        cur.entry++;
    }
    const Ref typed = last;
    afterEdit (cur.staff, cur.measure, cur.layer);
    message.clear();
    if (! asRest) audition (last.valid ? last : typed);
    changed();
    return true;
}

int Editor::fitMeasure (int staff, int measure, int layer, bool overwrite)
{
    auto& es = score.staves[staff].measures[measure].layers[layer];
    const Tick cap = score.measureCapacity (measure);
    Tick total = 0;
    for (const auto& e : es) total += e.duration();
    if (total <= cap) return 0;

    // Move overflowing material from the end into the next measure, tied across the barline.
    std::vector<Entry> carry;
    while (total > cap && ! es.empty())
    {
        Entry e = es.back();
        es.pop_back();
        const Tick d = e.duration();
        const Tick start = total - d;
        total = start;
        if (start >= cap || e.tuplet.active() || e.grace)
        {
            carry.insert (carry.begin(), e); // moves whole
            continue;
        }
        // split: [start, cap) stays, the rest carries
        auto keep = decomposeTicks (cap - start);
        auto rest = decomposeTicks (d - (cap - start));
        std::vector<Entry> keepEntries, restEntries;
        auto makePiece = [&e] (std::pair<int, int> vd, bool tieOut) {
            Entry p = e;
            p.value = vd.first;
            p.dots = vd.second;
            p.slurEnd = false;
            for (auto& n : p.notes) n.tieToNext = tieOut ? true : n.tieToNext;
            return p;
        };
        for (size_t i = 0; i < keep.size(); ++i)
        {
            auto p = makePiece (keep[i], ! e.rest);
            keepEntries.push_back (p);
        }
        for (size_t i = 0; i < rest.size(); ++i)
        {
            const bool lastPiece = (i + 1 == rest.size());
            Entry p = makePiece (rest[i], ! e.rest && ! lastPiece);
            if (lastPiece)
            {
                for (size_t k = 0; k < p.notes.size(); ++k) p.notes[k].tieToNext = e.notes[k].tieToNext;
                p.slurEnd = e.slurEnd;
            }
            p.slurStart = false;
            p.staccato = p.tenuto = p.accent = false;
            p.dynamic.reset();
            p.clefBefore.reset();
            p.beam = BeamMode::Auto;
            restEntries.push_back (p);
        }
        // only the last kept piece and middle pieces lose articulations that belong to the attack
        for (size_t i = 1; i < keepEntries.size(); ++i)
        {
            keepEntries[i].slurStart = false;
            keepEntries[i].staccato = keepEntries[i].tenuto = keepEntries[i].accent = false;
        }
        es.insert (es.end(), keepEntries.begin(), keepEntries.end());
        carry.insert (carry.begin(), restEntries.begin(), restEntries.end());
        total = cap;
    }
    ensureMeasure (measure + 1);
    if (overwrite)
    {
        // write over the start of the next measure instead of pushing it along
        Tick len = 0;
        for (const auto& e : carry) len += e.duration();
        const Tick at = score.measureStart (measure + 1);
        const Tick removedEnd = removeRange (staff, layer, at, at + len);
        for (auto [v, d] : decomposeTicks (std::max<Tick> (0, removedEnd - (at + len))))
        {
            Entry r;
            r.value = v;
            r.dots = d;
            r.rest = true;
            carry.push_back (r);
        }
    }
    auto& next = score.staves[staff].measures[measure + 1].layers[layer];
    next.insert (next.begin(), carry.begin(), carry.end());
    fitMeasure (staff, measure + 1, layer); // cascade
    return (int) carry.size();
}

void Editor::afterEdit (int staff, int measure, int layer)
{
    const int before = (int) score.staves[staff].measures[measure].layers[layer].size();
    int carried = 0;
    if (score.contentLength (staff, measure, layer) > score.measureCapacity (measure))
    {
        auto policy = forcedOverflow ? *forcedOverflow : overflowPolicy;
        if (policy == Overflow::Ask && ! onOverflow) policy = Overflow::Push;
        switch (policy)
        {
            case Overflow::Ask:
                pendingOverflow = { true, staff, measure, 0, layer };
                onOverflow();
                return; // the cursor waits until the choice is made
            case Overflow::Tie: carried = fitMeasure (staff, measure, layer, true); break;
            case Overflow::Push: carried = fitMeasure (staff, measure, layer); break;
            case Overflow::Meter:
            {
                const Ref saved = pendingOverflow;
                pendingOverflow = { true, staff, measure, 0, layer };
                if (auto ts = fittingMeter())
                {
                    const TimeSig old = score.timeSigAt (measure);
                    if (measure + 1 < score.numMeasures() && ! score.measures[(size_t) measure + 1].timeSig)
                        score.measures[(size_t) measure + 1].timeSig = old;
                    score.measures[(size_t) measure].timeSig = *ts;
                }
                else
                    carried = fitMeasure (staff, measure, layer);
                pendingOverflow = saved;
                break;
            }
            case Overflow::Leave: break;
        }
    }
    const int after = (int) score.staves[staff].measures[measure].layers[layer].size();

    if (cur.staff == staff && cur.layer == layer && cur.measure == measure + 1)
        cur.entry += carried;

    if (last.valid && last.staff == staff && last.layer == layer && last.measure == measure && last.entry >= after)
    {
        // the entry we just typed moved (partly) into the next measure
        last.measure = measure + 1;
        last.entry = last.entry - after;
        if (last.entry >= carried) last.entry = carried - 1;
        if (last.entry < 0) last.valid = false;
    }

    if (cur.staff == staff && cur.layer == layer && cur.measure == measure)
    {
        const bool atEnd = cur.entry >= after;
        const bool full = score.contentLength (staff, measure, layer) >= score.measureCapacity (measure);
        if (atEnd && full)
        {
            // jump to the next measure, past anything that was carried over
            ensureMeasure (measure + 1);
            const int overflowIntoCursor = std::max (0, cur.entry - after);
            cur.measure = measure + 1;
            cur.entry = std::max (carried, overflowIntoCursor);
        }
    }
    (void) before;
}

// ---------------------------------------------------------------- pitch edits

bool Editor::alterTarget (int delta, bool wholeMeasure)
{
    Ref r;
    if (! targetRef (r)) return false;
    auto& es = entriesAt (r);
    Note* n = targetNote (es[r.entry]);
    if (! n) return false;
    checkpoint();
    const int dia = n->pitch.dia;
    const int newAlter = std::clamp (n->pitch.alter + delta, -2, 2);
    n->pitch.alter = newAlter;
    if (wholeMeasure)
        for (size_t i = (size_t) r.entry + 1; i < es.size(); ++i)
            for (auto& other : es[i].notes)
                if (other.pitch.dia == dia) other.pitch.alter = newAlter;
    audition (r);
    changed();
    return true;
}

static bool respell (Pitch& p)
{
    const int midi = p.midi();
    std::vector<Pitch> options;
    for (int d = p.dia - 2; d <= p.dia + 2; ++d)
    {
        Pitch q { d, 0 };
        q.alter = midi - q.midi();
        if (std::abs (q.alter) <= 2) options.push_back (q);
    }
    if (options.size() < 2) return false;
    std::stable_sort (options.begin(), options.end(), [] (const Pitch& a, const Pitch& b) {
        return std::abs (a.alter) < std::abs (b.alter);
    });
    for (size_t i = 0; i < options.size(); ++i)
        if (options[i] == p)
        {
            p = options[(i + 1) % options.size()];
            return true;
        }
    p = options.front();
    return true;
}

bool Editor::enharmonic (bool wholeMeasure)
{
    Ref r;
    if (! targetRef (r)) return false;
    auto& es = entriesAt (r);
    auto& e = es[r.entry];
    if (e.rest) return false;
    checkpoint();
    Note* n = targetNote (e);
    if (n)
    {
        const Pitch old = n->pitch;
        respell (n->pitch);
        if (wholeMeasure)
            for (size_t i = (size_t) r.entry + 1; i < es.size(); ++i)
                for (auto& other : es[i].notes)
                    if (other.pitch == old) other.pitch = n->pitch;
        if (cur.dia == old.dia) cur.dia = n->pitch.dia;
    }
    else
    {
        for (auto& note : e.notes) respell (note.pitch);
    }
    e.sortNotes();
    changed();
    return true;
}

// ---------------------------------------------------------------- navigation

bool Editor::moveEntry (int delta)
{
    navigated();
    if (delta < 0)
    {
        if (cur.entry > 0) cur.entry--;
        else if (cur.measure > 0)
        {
            cur.measure--;
            cur.entry = std::max (0, (int) entries().size() - 1);
        }
        else return false;
    }
    else
    {
        if (cur.entry < (int) entries().size()) cur.entry++;
        else
        {
            ensureMeasure (cur.measure + 1);
            cur.measure++;
            cur.entry = 0;
        }
    }
    changed();
    return true;
}

void Editor::startTuplet (int actual, int normal)
{
    actual = std::clamp (actual, 2, 64);
    normal = std::clamp (normal, 1, 64);
    tupletPending = { actual, normal };
    tupletRemaining = actual;
    message = "Tuplet " + std::to_string (actual) + ":" + std::to_string (normal);
    changed();
}

// ---------------------------------------------------------------- commands

bool Editor::perform (Command cmd, int arg)
{
    message.clear();
    // a too-full measure waits for its answer (undo also cancels it)
    if (pendingOverflow.valid)
    {
        if (cmd != Command::Undo) return false;
        pendingOverflow.valid = false;
    }

    // With the entry frame closed only these work; everything else waits for Esc or a click.
    switch (cmd)
    {
        case Command::ToggleFrame: case Command::Undo: case Command::Redo: case Command::Copy: case Command::Cut:
        case Command::Paste: case Command::SelectAll: case Command::ClearSelection: case Command::DeleteEntry:
        case Command::ClearToRest: case Command::SetDynamic: case Command::MoveToStaffAbove: case Command::MoveToStaffBelow:
        case Command::GrandStaff: case Command::SetClef:
            break;
        default:
            if (! frame) return false;
    }

    // Moving or typing ends a selection; the selection commands keep it.
    switch (cmd)
    {
        case Command::SelectNext: case Command::SelectPrev: case Command::SelectMeasure: case Command::SelectAll:
        case Command::Copy: case Command::Cut: case Command::ClearSelection: case Command::DeleteEntry:
        case Command::ClearToRest: case Command::SetDynamic: case Command::Undo: case Command::Redo: case Command::ToggleFrame:
        case Command::Staccato: case Command::Tenuto: case Command::Accent:
        case Command::MoveToStaffAbove: case Command::MoveToStaffBelow: case Command::SetClef:
            break;
        default:
            sel.active = false;
            selAnchor = -1;
    }

    switch (cmd)
    {
        case Command::None: return false;

        case Command::EnterValue: return enterValue (std::clamp (arg, 1, 8), false, false);
        case Command::InsertValue: return enterValue (std::clamp (arg, 1, 8), true, false);
        case Command::EnterRestValue: return enterValue (std::clamp (arg, 1, 8), false, true);

        case Command::Dot:
        {
            Ref r;
            if (! targetRef (r)) return false;
            checkpoint();
            auto& e = entriesAt (r)[r.entry];
            e.dots = (e.dots + 1) % 4; // up to three dots, then back to none
            last = r;
            afterEdit (r.staff, r.measure, r.layer);
            changed();
            return true;
        }

        case Command::StartTuplet:
        {
            const int n = std::clamp (arg, 2, 9);
            int normal = 1;
            while (normal * 2 < n) normal *= 2;
            if (n == 2) normal = 3; // duplet: 2 in the time of 3
            startTuplet (n, normal);
            return true;
        }

        case Command::TupletDialog: return false; // the UI asks for the ratio

        case Command::AddChordNote:
        {
            Entry* e = target();
            if (! e) return false;
            checkpoint();
            Note n;
            n.pitch.dia = cur.dia;
            n.pitch.alter = score.contextAlter (cur.staff, cur.measure, cur.entry, cur.dia, cur.layer);
            if (e->rest)
            {
                e->rest = false;
                e->notes = { n };
            }
            else if (e->findNote (cur.dia) < 0)
            {
                e->notes.push_back (n);
                e->sortNotes();
            }
            Ref r;
            if (targetRef (r)) audition (r);
            changed();
            return true;
        }

        case Command::RemoveChordNote:
        {
            Entry* e = target();
            if (! e || e->rest) return false;
            const int i = e->findNote (cur.dia);
            if (i < 0) return false;
            checkpoint();
            if (e->notes.size() > 1)
                e->notes.erase (e->notes.begin() + i);
            else
            {
                e->notes.clear();
                e->rest = true;
            }
            changed();
            return true;
        }

        case Command::ToggleRest:
        {
            Entry* e = target();
            if (! e) return false;
            checkpoint();
            if (e->rest)
            {
                Note n;
                n.pitch.dia = cur.dia;
                n.pitch.alter = score.contextAlter (cur.staff, cur.measure, cur.entry, cur.dia, cur.layer);
                e->rest = false;
                e->notes = { n };
            }
            else
            {
                e->rest = true;
                e->notes.clear();
                e->staccato = e->tenuto = e->accent = false;
            }
            changed();
            return true;
        }

        case Command::ClearToRest:
        {
            if (sel.active) return clearSelectionMusic();
            if (! frame) return false;
            Entry* e = target();
            if (! e || e->rest) return false;
            checkpoint();
            const auto dyn = e->dynamic;
            makeRest (*e);
            e->dynamic = dyn;
            for (auto& n : e->notes) n.tieToNext = false;
            changed();
            return true;
        }

        case Command::DeleteEntry:
        {
            if (sel.active) return clearSelectionMusic();
            if (! frame) return false;
            Ref r;
            if (! targetRef (r)) return false;
            checkpoint();
            auto& es = entriesAt (r);
            es.erase (es.begin() + r.entry);
            if (r.staff == cur.staff && r.measure == cur.measure && r.entry < cur.entry) cur.entry--;
            last.valid = false;
            changed();
            return true;
        }

        case Command::RaiseHalfStep: return alterTarget (+1, false);
        case Command::LowerHalfStep: return alterTarget (-1, false);
        case Command::RaiseHalfStepMeasure: return alterTarget (+1, true);
        case Command::LowerHalfStepMeasure: return alterTarget (-1, true);
        case Command::Enharmonic: return enharmonic (false);
        case Command::EnharmonicMeasure: return enharmonic (true);

        case Command::ToggleAccidental:
        {
            Ref r;
            if (! targetRef (r)) return false;
            auto& e = entriesAt (r)[r.entry];
            Note* n = targetNote (e);
            if (! n) return false;
            checkpoint();
            if (n->accidental != AccidentalMode::Auto)
                n->accidental = AccidentalMode::Auto;
            else
            {
                const auto shown = score.displayedAccidental (r.staff, r.measure, r.entry, (int) (n - e.notes.data()));
                n->accidental = shown ? AccidentalMode::Hide : AccidentalMode::Force;
            }
            changed();
            return true;
        }

        case Command::AccidentalParens:
        {
            Entry* e = target();
            Note* n = e ? targetNote (*e) : nullptr;
            if (! n) return false;
            checkpoint();
            n->parentheses = ! n->parentheses;
            if (n->parentheses && n->accidental == AccidentalMode::Auto) n->accidental = AccidentalMode::Force;
            changed();
            return true;
        }

        case Command::TieNext:
        {
            Entry* e = target();
            if (! e || e->rest) return false;
            checkpoint();
            if (Note* n = (e->findNote (cur.dia) >= 0 ? &e->notes[e->findNote (cur.dia)] : nullptr))
                n->tieToNext = ! n->tieToNext;
            else
            {
                const bool any = std::any_of (e->notes.begin(), e->notes.end(), [] (const Note& n) { return n.tieToNext; });
                for (auto& n : e->notes) n.tieToNext = ! any;
            }
            changed();
            return true;
        }

        case Command::TiePrevious:
        {
            Ref r, pr;
            if (! targetRef (r)) return false;
            Entry* p = previousEntry (r, &pr);
            if (! p || p->rest) return false;
            checkpoint();
            const auto& e = entriesAt (r)[r.entry];
            bool any = false;
            for (auto& pn : p->notes)
                for (const auto& n : e.notes)
                    if (n.pitch.midi() == pn.pitch.midi()) { pn.tieToNext = ! pn.tieToNext; any = true; }
            if (! any) for (auto& pn : p->notes) pn.tieToNext = ! pn.tieToNext;
            changed();
            return true;
        }

        case Command::BreakJoinBeam:
        {
            Ref r;
            if (! targetRef (r)) return false;
            checkpoint();
            const auto joined = beamedToPrevious (score, r.staff, r.measure, r.layer);
            auto& e = entriesAt (r)[r.entry];
            e.beam = joined[r.entry] ? BeamMode::BreakBefore : BeamMode::JoinBefore;
            changed();
            return true;
        }

        case Command::ResetBeams:
        {
            checkpoint();
            for (auto& e : entries()) { e.beam = BeamMode::Auto; e.flatBeam = false; }
            changed();
            return true;
        }

        case Command::FlatBeam:
        {
            Entry* e = target();
            if (! e) return false;
            checkpoint();
            e->flatBeam = ! e->flatBeam;
            changed();
            return true;
        }

        case Command::FreezeStem:
        {
            Entry* e = target();
            if (! e) return false;
            checkpoint();
            Ref r;
            targetRef (r);
            const bool up = autoStemUp (*e, score.clefAtEntry (r.staff, r.measure, r.entry, r.layer), layerStemRule (score.staves[r.staff].measures[r.measure], r.layer));
            e->stem = up ? StemDir::Down : StemDir::Up; // flip from what is showing
            changed();
            return true;
        }

        case Command::AutoStem:
        {
            Entry* e = target();
            if (! e) return false;
            checkpoint();
            e->stem = StemDir::Auto;
            changed();
            return true;
        }

        case Command::FreezeAccidental:
        {
            Ref r;
            if (! targetRef (r)) return false;
            auto& e = entriesAt (r)[r.entry];
            Note* n = targetNote (e);
            if (! n) return false;
            checkpoint();
            n->accidental = (n->accidental == AccidentalMode::Force) ? AccidentalMode::Auto : AccidentalMode::Force;
            changed();
            return true;
        }

        case Command::SlashGraceNote:
        {
            Ref r;
            if (! targetRef (r)) return false;
            checkpoint();
            auto& e = entriesAt (r)[r.entry];
            if (e.grace && e.graceSlash) e.grace = e.graceSlash = false;
            else e.grace = e.graceSlash = true;
            if (e.grace && e.value > 4) e.value = 4; // slashed grace notes are flagged
            afterEdit (r.staff, r.measure, r.layer);
            changed();
            return true;
        }

        case Command::HideEntry:
        case Command::GraceNote:
        case Command::Staccato:
        case Command::Tenuto:
        case Command::Accent:
        case Command::SlurStart:
        case Command::SlurEnd:
        {
            Ref r;
            if (! targetRef (r)) return false;
            checkpoint();
            auto& e = entriesAt (r)[r.entry];
            switch (cmd)
            {
                case Command::HideEntry: e.hidden = ! e.hidden; break;
                case Command::GraceNote: e.grace = ! e.grace; e.graceSlash = false; break;
                case Command::Staccato: e.staccato = ! e.staccato; if (e.staccato) e.tenuto = false; break;
                case Command::Tenuto: e.tenuto = ! e.tenuto; if (e.tenuto) e.staccato = false; break;
                case Command::Accent: e.accent = ! e.accent; break;
                case Command::SlurStart: e.slurStart = ! e.slurStart; break;
                case Command::SlurEnd: e.slurEnd = ! e.slurEnd; break;
                default: break;
            }
            if (cmd == Command::GraceNote) afterEdit (r.staff, r.measure, r.layer);
            changed();
            return true;
        }

        case Command::CursorUp: cur.dia = std::min (74, cur.dia + 1); changed(); return true;
        case Command::CursorDown: cur.dia = std::max (0, cur.dia - 1); changed(); return true;
        case Command::OctaveUp: cur.dia = std::min (74, cur.dia + 7); changed(); return true;
        case Command::OctaveDown: cur.dia = std::max (0, cur.dia - 7); changed(); return true;

        case Command::PrevEntry: return moveEntry (-1);
        case Command::NextEntry: return moveEntry (+1);

        case Command::PrevMeasure:
            if (cur.measure == 0) return false;
            navigated();
            cur.measure--;
            cur.entry = 0;
            changed();
            return true;

        case Command::NextMeasure:
            navigated();
            ensureMeasure (cur.measure + 1);
            cur.measure++;
            cur.entry = 0;
            changed();
            return true;

        case Command::StaffUp:
        case Command::StaffDown:
        {
            const int s = cur.staff + (cmd == Command::StaffUp ? -1 : +1);
            if (s < 0 || s >= (int) score.staves.size()) return false;
            navigated();
            const Clef from = score.clefAtEntry (cur.staff, cur.measure, cur.entry, cur.layer);
            cur.staff = s;
            cur.entry = std::min (cur.entry, (int) entries().size());
            // keep the crossbar at the same place on the staff
            cur.dia = cur.dia - clefMiddleLine (from) + clefMiddleLine (score.clefAtEntry (cur.staff, cur.measure, cur.entry, cur.layer));
            cur.entry = std::min (cur.entry, (int) entries().size());
            changed();
            return true;
        }

        case Command::ToggleInsertMode:
            insert = ! insert;
            message = insert ? "Insert mode" : "Overwrite mode";
            changed();
            return true;

        case Command::MoveToStaffAbove: return moveToStaff (-1);
        case Command::MoveToStaffBelow: return moveToStaff (+1);
        case Command::GrandStaff: return setGrandStaff (! score.isGrandStaff());
        case Command::SetClef:
        {
            static const Clef clefs[] = { Clef::Treble, Clef::Bass, Clef::Alto, Clef::Tenor };
            return setClefAtCursor (clefs[std::clamp (arg, 1, 4) - 1]);
        }

        case Command::Undo:
            if (undoStack.empty()) return false;
            redoStack.push_back (score);
            score = undoStack.back();
            undoStack.pop_back();
            last.valid = false;
            changed();
            return true;

        case Command::Redo:
            if (redoStack.empty()) return false;
            undoStack.push_back (score);
            score = redoStack.back();
            redoStack.pop_back();
            last.valid = false;
            changed();
            return true;

        case Command::SetLayer:
        case Command::ToggleVoice:
        {
            const int l = (cmd == Command::ToggleVoice) ? (cur.layer == 1 ? 0 : 1) : std::clamp (arg - 1, 0, kLayers - 1);
            navigated();
            const Tick t = score.onsetOf (cur.staff, cur.measure, cur.layer, cur.entry);
            cur.layer = l;
            // land on the entry at (or after) the same time in the new layer
            const auto& es = entries();
            Tick pos = 0;
            int i = 0;
            while (i < (int) es.size() && pos < t) pos += es[(size_t) i++].duration();
            cur.entry = i;
            message = "Layer " + std::to_string (l + 1);
            changed();
            return true;
        }

        case Command::ToggleFrame:
            setFrameActive (! frame);
            return true;

        case Command::SelectNext: extendSelection (+1); return true;
        case Command::SelectPrev: extendSelection (-1); return true;

        case Command::SelectMeasure:
            sel = { true, cur.staff, cur.staff, score.measureStart (cur.measure), score.measureStart (cur.measure) + score.measureCapacity (cur.measure) };
            changed();
            return true;

        case Command::SelectAll:
            sel = { true, 0, (int) score.staves.size() - 1, 0, score.measureStart (score.numMeasures()) };
            message = "Selected everything";
            changed();
            return true;

        case Command::Copy:
            if (! sel.active) return false;
            copySelection();
            message = "Copied";
            changed();
            return true;

        case Command::Cut:
            if (! sel.active) return false;
            copySelection();
            clearSelectionMusic();
            message = "Cut";
            return true;

        case Command::ClearSelection: return clearSelectionMusic();
        case Command::Paste: return pasteAtCursor();
        case Command::SetDynamic: return setDynamic (std::clamp (arg - 1, 0, (int) Dynamic::Count - 1));

        case Command::DynamicsMenu: // handled by the UI
        case Command::ShowCommands:
        case Command::PrevClip:
        case Command::NextClip:
        case Command::MeasureMenu:
        case Command::Count: return false;
    }
    return false;
}

// ---------------------------------------------------------------- selection, copy and paste

void Editor::extendSelection (int delta)
{
    if (selAnchor < 0 || ! sel.active) selAnchor = cursorTick();
    if (delta > 0)
    {
        if (cur.entry < (int) entries().size()) cur.entry++;
        else
        {
            ensureMeasure (cur.measure + 1);
            cur.measure++;
            cur.entry = 0;
            if (! entries().empty()) cur.entry = 1;
        }
    }
    else
    {
        if (cur.entry > 0) cur.entry--;
        else if (cur.measure > 0)
        {
            cur.measure--;
            cur.entry = std::max (0, (int) entries().size() - 1);
        }
    }
    navigated();
    const Tick t = cursorTick();
    sel.active = t != selAnchor;
    sel.staffFrom = sel.staffTo = cur.staff;
    sel.start = std::min (t, selAnchor);
    sel.end = std::max (t, selAnchor);
    changed();
}

void Editor::copySelection()
{
    if (! sel.active) return;
    Clipboard cb;
    cb.length = sel.end - sel.start;
    for (int st = sel.staffFrom; st <= sel.staffTo && st < (int) score.staves.size(); ++st)
    {
        std::array<std::vector<Entry>, kLayers> layers;
        for (int l = 0; l < kLayers; ++l)
        {
            Tick pos = sel.start;
            for (int m = score.measureAtTick (sel.start); m < score.numMeasures() && score.measureStart (m) < sel.end; ++m)
            {
                const Tick ms = score.measureStart (m);
                Tick t = ms;
                for (const auto& e : score.staves[st].measures[m].layers[l])
                {
                    if (t >= sel.start && t < sel.end)
                    {
                        if (t > pos)
                            for (auto [v, d] : decomposeTicks (t - pos))
                            {
                                Entry r;
                                r.value = v;
                                r.dots = d;
                                r.rest = true;
                                layers[l].push_back (r);
                            }
                        layers[l].push_back (e);
                        pos = std::max (pos, t + e.duration());
                    }
                    t += e.duration();
                }
            }
            // a layer with only filler rests copies as nothing
            bool any = false;
            for (const auto& e : layers[l]) any = any || ! e.rest;
            if (l > 0 && ! any) layers[l].clear();
        }
        cb.staves.push_back (std::move (layers));
    }
    clipboard() = std::move (cb);
}

bool Editor::clearSelectionMusic()
{
    if (! sel.active) return false;
    checkpoint();
    for (int st = sel.staffFrom; st <= sel.staffTo && st < (int) score.staves.size(); ++st)
        for (int l = 0; l < kLayers; ++l)
            for (int m = score.measureAtTick (sel.start); m < score.numMeasures() && score.measureStart (m) < sel.end; ++m)
            {
                auto& es = score.staves[st].measures[m].layers[l];
                Tick t = score.measureStart (m);
                std::vector<Entry> kept;
                for (auto e : es)
                {
                    if (t >= sel.start && t < sel.end && e.grace) continue; // grace notes go
                    if (t >= sel.start && t < sel.end)
                    {
                        // keep the rhythm: the music becomes rests
                        Entry r;
                        r.value = e.value;
                        r.dots = e.dots;
                        r.tuplet = e.tuplet;
                        r.tupletStart = e.tupletStart;
                        r.rest = true;
                        e = r;
                    }
                    t += e.duration();
                    kept.push_back (e);
                }
                es = std::move (kept);
                if (l > 0 && std::all_of (es.begin(), es.end(), [] (const Entry& e) { return e.rest; })) es.clear();
            }
    last.valid = false;
    clampCursor();
    changed();
    return true;
}

Tick Editor::removeRange (int staff, int layer, Tick from, Tick to)
{
    Tick removedEnd = from;
    for (int m = score.measureAtTick (from); m < score.numMeasures() && score.measureStart (m) < to; ++m)
    {
        auto& es = score.staves[staff].measures[m].layers[layer];
        Tick t = score.measureStart (m);
        std::vector<Entry> keep;
        for (const auto& e : es)
        {
            if (t >= from && t < to) removedEnd = std::max (removedEnd, t + e.duration());
            else keep.push_back (e);
            t += e.duration();
        }
        es = std::move (keep);
    }
    return removedEnd;
}

void Editor::insertAt (int staff, int layer, Tick at, std::vector<Entry> add)
{
    ensureMeasure (score.measureAtTick (at));
    const int m = score.measureAtTick (at);
    const Tick off = at - score.measureStart (m);
    auto& es = score.staves[staff].measures[m].layers[layer];
    Tick pos = 0;
    size_t idx = 0;
    while (idx < es.size() && pos < off) pos += es[idx++].duration();
    std::vector<Entry> pad;
    if (pos < off)
        for (auto [v, d] : decomposeTicks (off - pos))
        {
            Entry r;
            r.value = v;
            r.dots = d;
            r.rest = true;
            pad.push_back (r);
        }
    add.insert (add.begin(), pad.begin(), pad.end());
    es.insert (es.begin() + (long) idx, add.begin(), add.end());
    fitMeasure (staff, m, layer);
}

bool Editor::pasteAtCursor()
{
    auto& cb = clipboard();
    if (cb.empty()) return false;
    checkpoint();
    const Tick at = cursorTick();
    const Tick to = at + cb.length;
    while (score.measureStart (score.numMeasures()) < to) score.appendMeasure();
    for (size_t k = 0; k < cb.staves.size(); ++k)
    {
        const int st = cur.staff + (int) k;
        if (st >= (int) score.staves.size()) break;
        for (int l = 0; l < kLayers; ++l)
        {
            const bool hasMusic = ! cb.staves[k][(size_t) l].empty();
            if (! hasMusic && l > 0) continue;
            const Tick removedEnd = removeRange (st, l, at, to);
            std::vector<Entry> add = cb.staves[k][(size_t) l];
            // fill out to the copied length, and to anything longer we replaced
            Tick len = 0;
            for (const auto& e : add) len += e.duration();
            const Tick fillTo = std::max (to, removedEnd);
            if (l == 0 || hasMusic)
                for (auto [v, d] : decomposeTicks (std::max<Tick> (0, fillTo - (at + len))))
                {
                    Entry r;
                    r.value = v;
                    r.dots = d;
                    r.rest = true;
                    add.push_back (r);
                }
            insertAt (st, l, at, std::move (add));
        }
    }
    // the cursor goes to the end of the pasted music
    const int m = score.measureAtTick (std::min (to, score.measureStart (score.numMeasures()) - 1));
    cur.measure = m;
    cur.entry = 0;
    const auto& es = entries();
    Tick pos = score.measureStart (m);
    while (cur.entry < (int) es.size() && pos < to) pos += es[(size_t) cur.entry++].duration();
    sel = { true, cur.staff, std::min ((int) score.staves.size() - 1, cur.staff + (int) cb.staves.size() - 1), at, to };
    last.valid = false;
    message = "Pasted";
    changed();
    return true;
}

bool Editor::setDynamic (int d)
{
    const auto dyn = (Dynamic) std::clamp (d, 0, (int) Dynamic::Count - 1);
    if (sel.active)
    {
        // on the first note of each selected staff
        checkpoint();
        for (int st = sel.staffFrom; st <= sel.staffTo; ++st)
        {
            const int m = score.measureAtTick (sel.start);
            auto& es = score.staves[st].measures[m].layers[0];
            Tick t = score.measureStart (m);
            for (auto& e : es)
            {
                if (t >= sel.start) { e.dynamic = dyn; break; }
                t += e.duration();
            }
        }
        changed();
        return true;
    }
    Ref r;
    if (! targetRef (r)) return false;
    checkpoint();
    auto& e = entriesAt (r)[r.entry];
    if (e.dynamic && *e.dynamic == dyn) e.dynamic.reset();
    else e.dynamic = dyn;
    message = std::string ("Dynamic ") + dynamicName (dyn);
    changed();
    return true;
}

} // namespace staffpaper


namespace staffpaper
{

void Editor::makeRest (Entry& e)
{
    e.rest = true;
    e.notes.clear();
    e.staccato = e.tenuto = e.accent = false;
    e.slurStart = e.slurEnd = false;
    e.stem = StemDir::Auto;
    e.velocity = 0;
}

void Editor::placeNotes (int staff, int preferredLayer, Tick at, const Entry& rhythm, const std::vector<Note>& notes)
{
    if (notes.empty()) return;
    const int m = score.measureAtTick (at);
    const Tick ms = score.measureStart (m);
    const Tick d = rhythm.duration();
    const Tick cap = score.measureCapacity (m);
    auto restsFor = [] (Tick len) {
        std::vector<Entry> out;
        for (auto [v, dots] : decomposeTicks (std::max<Tick> (0, len)))
        {
            Entry r;
            r.value = v;
            r.dots = dots;
            r.rest = true;
            out.push_back (r);
        }
        return out;
    };
    Entry add = rhythm;
    add.rest = false;
    add.grace = false;
    add.notes = notes;
    add.clefBefore.reset();
    add.dynamic.reset();
    add.velocity = 0;
    add.sortNotes();

    // try the preferred layer, then the others
    std::vector<int> order { preferredLayer };
    for (int l = 0; l < kLayers; ++l)
        if (l != preferredLayer) order.push_back (l);

    for (int l : order)
    {
        auto& es = score.staves[staff].measures[m].layers[l];
        if (es.empty())
        {
            std::vector<Entry> v = restsFor (at - ms);
            const bool lone = ! add.tuplet.active();
            Entry e = add;
            if (! lone) { e.tuplet = {}; e.tupletStart = false; }
            if (! lone && l > 0) continue; // a lone tuplet note can't stand alone; try elsewhere
            v.push_back (lone ? add : e);
            if (l == 0)
            {
                auto tail = restsFor (cap - (at - ms) - v.back().duration());
                v.insert (v.end(), tail.begin(), tail.end());
            }
            es = std::move (v);
            return;
        }
        // exact rhythm match: join the chord
        Tick t = ms;
        int first = -1, last = -1;
        bool fits = true, chord = false;
        for (int i = 0; i < (int) es.size(); ++i)
        {
            auto& e = es[(size_t) i];
            const Tick ed = e.duration();
            if (e.grace) continue;
            if (t == at && ed == d)
            {
                if (e.rest)
                {
                    e.rest = false;
                    e.notes.clear();
                }
                for (const auto& n : notes)
                    if (e.findNote (n.pitch.dia) < 0) e.notes.push_back (n);
                e.sortNotes();
                chord = true;
                break;
            }
            if (t < at + d && t + ed > at) // overlaps the new note
            {
                if (! e.rest || e.tuplet.active()) fits = false;
                if (first < 0) first = i;
                last = i;
            }
            t += ed;
        }
        if (chord) return;
        if (! fits) continue;
        if (first < 0)
        {
            // the layer ends before the note: pad with rests
            Tick len = 0;
            for (const auto& e : es) len += e.duration();
            if (len > at - ms) continue;
            auto v = restsFor (at - ms - len);
            es.insert (es.end(), v.begin(), v.end());
            es.push_back (add);
            return;
        }
        Tick firstStart = ms, lastEnd = ms;
        {
            Tick tt = ms;
            for (int i = 0; i < (int) es.size(); ++i)
            {
                if (i == first) firstStart = tt;
                tt += es[(size_t) i].duration();
                if (i == last) lastEnd = tt;
            }
        }
        if (add.tuplet.active()) continue; // a tuplet note needs its matching rhythm
        std::vector<Entry> v = restsFor (at - firstStart);
        v.push_back (add);
        auto tail = restsFor (lastEnd - (at + d));
        v.insert (v.end(), tail.begin(), tail.end());
        es.erase (es.begin() + first, es.begin() + last + 1);
        es.insert (es.begin() + first, v.begin(), v.end());
        return;
    }
    // no room anywhere: overwrite in the preferred layer
    const Tick removedEnd = removeRange (staff, preferredLayer, at, at + d);
    std::vector<Entry> v { add };
    auto tail = restsFor (removedEnd - (at + d));
    v.insert (v.end(), tail.begin(), tail.end());
    insertAt (staff, preferredLayer, at, std::move (v));
}

bool Editor::moveToStaff (int dir)
{
    struct Move
    {
        int from, layer;
        Tick at;
        Entry rhythm;
        std::vector<Note> notes;
    };
    std::vector<Move> moves;
    const int nStaves = (int) score.staves.size();

    auto clearLayerIfEmpty = [this] (int st, int m, int l) {
        auto& es = score.staves[st].measures[m].layers[l];
        if (l > 0 && std::all_of (es.begin(), es.end(), [] (const Entry& e) { return e.rest; })) es.clear();
    };

    if (sel.active)
    {
        if (sel.staffFrom + dir < 0 || sel.staffTo + dir >= nStaves)
        {
            message = nStaves < 2 ? "Only one staff: turn on the grand staff first" : "There is no staff there";
            if (onChange) onChange();
            return false;
        }
        checkpoint();
        for (int st = sel.staffFrom; st <= sel.staffTo; ++st)
            for (int m = score.measureAtTick (sel.start); m < score.numMeasures() && score.measureStart (m) < sel.end; ++m)
                for (int l = 0; l < kLayers; ++l)
                {
                    auto& es = score.staves[st].measures[m].layers[l];
                    Tick t = score.measureStart (m);
                    for (auto& e : es)
                    {
                        if (! e.grace && ! e.rest && t >= sel.start && t < sel.end)
                        {
                            moves.push_back ({ st, l, t, e, e.notes });
                            const auto dyn = e.dynamic;
                            makeRest (e);
                            e.dynamic = dyn;
                        }
                        t += e.duration();
                    }
                }
        sel.staffFrom += dir;
        sel.staffTo += dir;
    }
    else
    {
        Ref r;
        if (! targetRef (r)) return false;
        if (r.staff + dir < 0 || r.staff + dir >= nStaves)
        {
            message = nStaves < 2 ? "Only one staff: turn on the grand staff first" : "There is no staff there";
            if (onChange) onChange();
            return false;
        }
        auto& e = entriesAt (r)[(size_t) r.entry];
        if (e.rest || e.grace) return false;
        checkpoint();
        const Tick at = score.measureStart (r.measure) + score.onsetOf (r.staff, r.measure, r.layer, r.entry);
        const Entry rhythm = e;
        std::vector<Note> notes;
        const int k = e.notes.size() > 1 ? e.findNote (cur.dia) : -1;
        if (k >= 0 && r.staff == cur.staff)
        {
            notes.push_back (e.notes[(size_t) k]);
            e.notes.erase (e.notes.begin() + k);
        }
        else
        {
            notes = e.notes;
            const auto dyn = e.dynamic;
            makeRest (e);
            e.dynamic = dyn;
        }
        moves.push_back ({ r.staff, r.layer, at, rhythm, notes });
    }

    for (auto& mv : moves)
    {
        placeNotes (mv.from + dir, mv.layer, mv.at, mv.rhythm, mv.notes);
        clearLayerIfEmpty (mv.from, score.measureAtTick (mv.at), mv.layer);
    }
    last.valid = false;
    message = dir < 0 ? "Moved to the staff above" : "Moved to the staff below";
    changed();
    return true;
}

bool Editor::setGrandStaff (bool on)
{
    if (on == score.isGrandStaff()) return false;
    checkpoint();
    constexpr int middleC = 28; // C4: it and everything above stay on the top staff
    if (on)
    {
        score.setGrandStaff (true);
        if (score.staves[0].clef == Clef::Bass) score.staves[0].clef = Clef::Treble;
        auto& top = score.staves[0];
        auto& bottom = score.staves[1];
        for (int m = 0; m < score.numMeasures(); ++m)
        {
            auto& tm = top.measures[(size_t) m];
            auto& bm = bottom.measures[(size_t) m];
            bm.layers = tm.layers;
            bm.clefChange.reset();
            for (int l = 0; l < kLayers; ++l)
            {
                auto split = [&] (std::vector<Entry>& es, bool keepHigh) {
                    std::vector<Entry> out;
                    for (auto e : es)
                    {
                        if (! keepHigh) e.clefBefore.reset();
                        if (! e.rest)
                        {
                            std::vector<Note> kept;
                            for (const auto& n : e.notes)
                                if ((n.pitch.dia >= middleC) == keepHigh) kept.push_back (n);
                            e.notes = kept;
                            if (e.notes.empty())
                            {
                                if (e.grace) continue;
                                const auto dyn = e.dynamic;
                                makeRest (e);
                                e.dynamic = dyn;
                            }
                        }
                        out.push_back (e);
                    }
                    if (l > 0 && std::all_of (out.begin(), out.end(), [] (const Entry& e) { return e.rest; })) out.clear();
                    es = std::move (out);
                };
                split (tm.layers[(size_t) l], true);
                split (bm.layers[(size_t) l], false);
            }
        }
        if (cur.staff == 0 && cur.dia < middleC)
        {
            cur.staff = 1;
            cur.dia += clefMiddleLine (Clef::Bass) - clefMiddleLine (score.clefAtEntry (0, cur.measure, cur.entry, cur.layer));
            cur.dia = std::clamp (cur.dia, 0, 80);
        }
        message = "Grand staff: split at middle C";
    }
    else
    {
        // bring the lower staves' notes up into the top staff
        for (int st = 1; st < (int) score.staves.size(); ++st)
            for (int m = 0; m < score.numMeasures(); ++m)
                for (int l = 0; l < kLayers; ++l)
                {
                    Tick t = score.measureStart (m);
                    for (const auto& e : score.staves[(size_t) st].measures[(size_t) m].layers[(size_t) l])
                    {
                        if (! e.rest && ! e.grace) placeNotes (0, l, t, e, e.notes);
                        t += e.duration();
                    }
                }
        score.setGrandStaff (false);
        cur.staff = 0;
        message = "Single staff";
    }
    sel.active = false;
    last.valid = false;
    changed();
    return true;
}

bool Editor::setClefAtCursor (Clef c)
{
    checkpoint();
    auto& sm = score.staves[(size_t) cur.staff].measures[(size_t) cur.measure];
    auto& es = sm.layers[(size_t) cur.layer];
    if (cur.entry == 0 || es.empty())
    {
        if (cur.measure == 0)
        {
            score.staves[(size_t) cur.staff].clef = c;
            sm.clefChange.reset();
        }
        else
            sm.clefChange = c;
        if (! es.empty()) es[0].clefBefore.reset();
    }
    else if (cur.entry < (int) es.size())
        es[(size_t) cur.entry].clefBefore = c;
    else
    {
        ensureMeasure (cur.measure + 1);
        score.staves[(size_t) cur.staff].measures[(size_t) cur.measure + 1].clefChange = c;
    }
    message = std::string ("Clef: ") + clefName (c);
    changed();
    return true;
}

} // namespace staffpaper

namespace staffpaper
{

std::optional<TimeSig> Editor::fittingMeter() const
{
    if (! pendingOverflow.valid) return {};
    const int m = pendingOverflow.measure;
    Tick len = 0;
    for (int l = 0; l < kLayers; ++l)
        for (size_t st = 0; st < score.staves.size(); ++st)
            len = std::max (len, score.contentLength ((int) st, m, l));
    const TimeSig cur = score.timeSigAt (m);
    for (int den = cur.den; den <= 32; den *= 2)
    {
        const Tick unit = 4 * PPQ / den;
        if (len % unit == 0 && len / unit <= 32) return TimeSig { (int) (len / unit), den };
    }
    return {};
}

void Editor::resolveOverflow (Overflow choice)
{
    if (! pendingOverflow.valid) return;
    const Ref p = pendingOverflow;
    pendingOverflow.valid = false;
    if (choice == Overflow::Ask)
    {
        perform (Command::Undo);
        message = "Cancelled";
        changed();
        return;
    }
    if (p.staff >= (int) score.staves.size() || p.measure >= score.numMeasures()) return;
    forcedOverflow = choice;
    if (choice == Overflow::Meter) pendingOverflow = p;
    afterEdit (p.staff, p.measure, p.layer);
    pendingOverflow.valid = false;
    forcedOverflow.reset();
    changed();
}

} // namespace staffpaper
