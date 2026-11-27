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
    clampCursor();
    navigated();
}

void Editor::clampCursor()
{
    if (score.staves.empty()) score = Score::makeEmpty();
    cur.staff = std::clamp (cur.staff, 0, (int) score.staves.size() - 1);
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
        && last.entry < (int) score.staves[last.staff].measures[last.measure].entries.size())
    {
        out = last;
        return true;
    }
    const auto& es = score.staves[cur.staff].measures[cur.measure].entries;
    if (cur.entry < (int) es.size())
    {
        out = { true, cur.staff, cur.measure, cur.entry };
        return true;
    }
    if (! es.empty())
    {
        out = { true, cur.staff, cur.measure, (int) es.size() - 1 };
        return true;
    }
    return false;
}

int Editor::targetEntry() const
{
    Ref r;
    if (! targetRef (r) || r.measure != cur.measure || r.staff != cur.staff) return -1;
    return r.entry;
}

Entry* Editor::target()
{
    Ref r;
    if (! targetRef (r)) return nullptr;
    return &score.staves[r.staff].measures[r.measure].entries[r.entry];
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
        auto& es = score.staves[r.staff].measures[m].entries;
        if (i >= 0 && i < (int) es.size())
        {
            if (where) *where = { true, r.staff, m, i };
            return &es[i];
        }
        --m;
        if (m >= 0) i = (int) score.staves[r.staff].measures[m].entries.size() - 1;
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
        n.pitch.alter = score.contextAlter (cur.staff, cur.measure, cur.entry, cur.dia);
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
        last = { true, cur.staff, cur.measure, cur.entry };
        cur.entry++;
    }
    else
    {
        Entry e = makeEntry (value, asRest);
        es.insert (es.begin() + cur.entry, e);
        last = { true, cur.staff, cur.measure, cur.entry };
        cur.entry++;
    }
    afterEdit (cur.staff, cur.measure);
    message.clear();
    changed();
    return true;
}

int Editor::fitMeasure (int staff, int measure)
{
    auto& es = score.staves[staff].measures[measure].entries;
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
    auto& next = score.staves[staff].measures[measure + 1].entries;
    next.insert (next.begin(), carry.begin(), carry.end());
    fitMeasure (staff, measure + 1); // cascade
    return (int) carry.size();
}

void Editor::afterEdit (int staff, int measure)
{
    const int before = (int) score.staves[staff].measures[measure].entries.size();
    const int carried = fitMeasure (staff, measure);
    const int after = (int) score.staves[staff].measures[measure].entries.size();

    if (cur.staff == staff && cur.measure == measure + 1)
        cur.entry += carried;

    if (last.valid && last.staff == staff && last.measure == measure && last.entry >= after)
    {
        // the entry we just typed moved (partly) into the next measure
        last.measure = measure + 1;
        last.entry = last.entry - after;
        if (last.entry >= carried) last.entry = carried - 1;
        if (last.entry < 0) last.valid = false;
    }

    if (cur.staff == staff && cur.measure == measure)
    {
        const bool atEnd = cur.entry >= after;
        const bool full = score.contentLength (staff, measure) >= score.measureCapacity (measure);
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
    auto& es = score.staves[r.staff].measures[r.measure].entries;
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
    auto& es = score.staves[r.staff].measures[r.measure].entries;
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
            auto& e = score.staves[r.staff].measures[r.measure].entries[r.entry];
            e.dots = (e.dots + 1) % 4; // up to three dots, then back to none
            last = r;
            afterEdit (r.staff, r.measure);
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
            n.pitch.alter = score.contextAlter (cur.staff, cur.measure, cur.entry, cur.dia);
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
                n.pitch.alter = score.contextAlter (cur.staff, cur.measure, cur.entry, cur.dia);
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

        case Command::DeleteEntry:
        {
            Ref r;
            if (! targetRef (r)) return false;
            checkpoint();
            auto& es = score.staves[r.staff].measures[r.measure].entries;
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
            auto& e = score.staves[r.staff].measures[r.measure].entries[r.entry];
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
            const auto& e = score.staves[r.staff].measures[r.measure].entries[r.entry];
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
            const auto joined = beamedToPrevious (score, r.staff, r.measure);
            auto& e = score.staves[r.staff].measures[r.measure].entries[r.entry];
            e.beam = joined[r.entry] ? BeamMode::BreakBefore : BeamMode::JoinBefore;
            changed();
            return true;
        }

        case Command::ResetBeams:
        {
            checkpoint();
            for (auto& e : entries()) e.beam = BeamMode::Auto;
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
            const bool up = autoStemUp (*e, score.clefAtEntry (r.staff, r.measure, r.entry));
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
            auto& e = score.staves[r.staff].measures[r.measure].entries[r.entry];
            switch (cmd)
            {
                case Command::HideEntry: e.hidden = ! e.hidden; break;
                case Command::GraceNote: e.grace = ! e.grace; break;
                case Command::Staccato: e.staccato = ! e.staccato; if (e.staccato) e.tenuto = false; break;
                case Command::Tenuto: e.tenuto = ! e.tenuto; if (e.tenuto) e.staccato = false; break;
                case Command::Accent: e.accent = ! e.accent; break;
                case Command::SlurStart: e.slurStart = ! e.slurStart; break;
                case Command::SlurEnd: e.slurEnd = ! e.slurEnd; break;
                default: break;
            }
            if (cmd == Command::GraceNote) afterEdit (r.staff, r.measure);
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
            const Clef from = score.clefAtEntry (cur.staff, cur.measure, cur.entry);
            cur.staff = s;
            cur.entry = std::min (cur.entry, (int) entries().size());
            // keep the crossbar at the same place on the staff
            cur.dia = cur.dia - clefMiddleLine (from) + clefMiddleLine (score.clefAtEntry (cur.staff, cur.measure, cur.entry));
            cur.entry = std::min (cur.entry, (int) entries().size());
            changed();
            return true;
        }

        case Command::ToggleInsertMode:
            insert = ! insert;
            message = insert ? "Insert mode" : "Overwrite mode";
            changed();
            return true;

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

        case Command::MeasureMenu: // handled by the UI
        case Command::Count: return false;
    }
    return false;
}

} // namespace staffpaper
