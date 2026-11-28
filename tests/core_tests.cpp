// Unit tests for the notation core. Plain asserts, no framework.
#include "staffpaper/Editor.h"
#include "staffpaper/KeyMap.h"
#include "staffpaper/Layout.h"
#include "staffpaper/MusicXml.h"
#include "staffpaper/Playback.h"
#include "staffpaper/Project.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

using namespace staffpaper;

static int failures = 0;
#define CHECK(cond)                                                              \
    do                                                                           \
    {                                                                            \
        if (! (cond))                                                            \
        {                                                                        \
            std::printf ("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);         \
            ++failures;                                                          \
        }                                                                        \
    } while (0)

static void testPitch()
{
    CHECK ((Pitch { 28, 0 }.midi()) == 60); // C4
    CHECK ((Pitch { 34, 0 }.midi()) == 71); // B4
    CHECK ((Pitch { 31, 1 }.midi()) == 66); // F#4
    CHECK (spellMidi (61, 0) == (Pitch { 28, 1 }));   // C#
    CHECK (spellMidi (61, -3) == (Pitch { 29, -1 })); // Db in Eb major
    CHECK (keyAlterForStep (2, 3) == 1);  // D major: F#
    CHECK (keyAlterForStep (2, 0) == 1);  // C#
    CHECK (keyAlterForStep (2, 4) == 0);  // G natural
    CHECK (keyAlterForStep (-1, 6) == -1); // F major: Bb
    CHECK (baseTicksForValue (5) == PPQ);
    CHECK (baseTicksForValue (1) == PPQ / 16);
    CHECK (baseTicksForValue (8) == 8 * PPQ);
}

static void testKeyMap()
{
    auto km = KeyMap::defaults();
    KeyStroke k;
    CHECK (parseKey ("5", k));
    CHECK (km.lookup (k).command == Command::EnterValue && km.lookup (k).arg == 5);
    CHECK (parseKey ("alt+3", k) && km.lookup (k).command == Command::StartTuplet);
    CHECK (parseKey ("plus", k) && km.lookup (k).command == Command::RaiseHalfStep);
    CHECK (parseKey ("equals", k) && km.lookup (k).command == Command::TieNext);
    CHECK (parseKey ("shift+up", k) && km.lookup (k).command == Command::StaffUp);
    CHECK (parseKey ("*", k) && km.lookup (k).command == Command::ToggleAccidental);
    CHECK (parseKey ("alt+s", k) && km.lookup (k).command == Command::Staccato);
    // round trip
    KeyMap again;
    CHECK (again.load (km.save()).empty());
    CHECK (again.lookup ({ '9', 0 }).command == Command::Enharmonic);
    CHECK (! km.load ("q = NoSuchCommand\n").empty());
}

static void testEntryAndFill()
{
    Score s = Score::makeEmpty (2);
    Editor ed (s);
    ed.setCursor ({ 0, 0, 0, 30 }); // E4
    for (int i = 0; i < 4; ++i) ed.perform (Command::EnterValue, 5);
    CHECK (s.staves[0].measures[0].layers[0].size() == 4);
    CHECK (ed.cursor().measure == 1); // jumped to the next measure
    CHECK (ed.cursor().entry == 0);
    CHECK (s.staves[0].measures[0].layers[0][0].notes[0].pitch.midi() == 64);

    // the dot applies to the note just entered
    ed.perform (Command::EnterValue, 5);
    ed.perform (Command::Dot);
    CHECK (s.staves[0].measures[1].layers[0][0].dots == 1);
    CHECK (s.contentLength (0, 1) == PPQ * 3 / 2);

    // a half note now overflows: 1.5 + 2 = 3.5, fits, then a half more overflows by a quarter
    ed.perform (Command::EnterValue, 6);
    ed.perform (Command::EnterValue, 6);
    CHECK (s.contentLength (0, 1) == 4 * PPQ);
    CHECK (s.numMeasures() >= 3);
    // the split half note becomes eighth tied over the bar? 4 - 3.5 = half a beat stays
    const auto& m1 = s.staves[0].measures[1].layers[0];
    CHECK (m1.back().notes[0].tieToNext);
    CHECK (s.contentLength (0, 2) == PPQ * 3 / 2);
    CHECK (ed.cursor().measure == 2);
    CHECK (ed.cursor().entry == (int) s.staves[0].measures[2].layers[0].size());

    // undo restores
    ed.perform (Command::Undo);
    CHECK (s.contentLength (0, 1) == PPQ * 7 / 2);
}

static void testAccidentals()
{
    Score s = Score::makeEmpty (1);
    Editor ed (s);
    ed.setCursor ({ 0, 0, 0, 31 }); // F4
    ed.perform (Command::EnterValue, 5);
    ed.perform (Command::RaiseHalfStep);
    CHECK (s.staves[0].measures[0].layers[0][0].notes[0].pitch.alter == 1);
    CHECK (s.displayedAccidental (0, 0, 0, 0).value_or (99) == 1);
    // next F carries the sharp and shows no accidental
    ed.perform (Command::EnterValue, 5);
    CHECK (s.staves[0].measures[0].layers[0][1].notes[0].pitch.alter == 1);
    CHECK (! s.displayedAccidental (0, 0, 1, 0).has_value());
    // enharmonic: F# -> Gb
    ed.perform (Command::Enharmonic);
    const auto p = s.staves[0].measures[0].layers[0][1].notes[0].pitch;
    CHECK (p.midi() == 66);
    CHECK (p.dia == 32 && p.alter == -1);
}

static void testChordsRestsTies()
{
    Score s = Score::makeEmpty (2);
    Editor ed (s);
    ed.setCursor ({ 0, 0, 0, 28 });
    ed.perform (Command::EnterValue, 6);
    ed.perform (Command::CursorUp);
    ed.perform (Command::CursorUp);
    ed.perform (Command::AddChordNote);
    CHECK (s.staves[0].measures[0].layers[0][0].notes.size() == 2);
    ed.perform (Command::TieNext);
    ed.perform (Command::CursorDown);
    ed.perform (Command::CursorDown);
    ed.perform (Command::EnterValue, 6);
    ed.perform (Command::CursorUp);
    ed.perform (Command::CursorUp);
    ed.perform (Command::AddChordNote);
    const auto seq = buildSequence (s);
    // the tie on the E only (crossbar on E): C plays twice, E once for a whole note
    int cCount = 0, eLen = 0;
    for (const auto& e : seq.events)
    {
        if (e.note == 60) ++cCount;
        if (e.note == 64) eLen = (int) e.length;
    }
    CHECK (cCount == 2);
    CHECK (eLen == (int) std::llround (4 * PPQ * s.playback.normalLength));

    ed.perform (Command::ToggleRest);
    CHECK (s.staves[0].measures[0].layers[0][1].rest);
}

static void testRepeats()
{
    Score s = Score::makeEmpty (5);
    s.measures[1].repeatStart = true;
    s.measures[2].repeatEnd = true;
    auto order = playbackOrder (s);
    CHECK ((order == std::vector<int> { 0, 1, 2, 1, 2, 3, 4 }));

    // first and second endings
    Score t = Score::makeEmpty (5);
    t.measures[2].endingMask = 1;
    t.measures[2].repeatEnd = true;
    t.measures[3].endingMask = 2;
    order = playbackOrder (t);
    CHECK ((order == std::vector<int> { 0, 1, 2, 0, 1, 3, 4 }));
}

static void testArticulations()
{
    Score s = Score::makeEmpty (1);
    Editor ed (s);
    ed.setCursor ({ 0, 0, 0, 28 });
    ed.perform (Command::EnterValue, 5);
    ed.perform (Command::Staccato);
    ed.perform (Command::EnterValue, 5);
    ed.perform (Command::SlurStart);
    ed.perform (Command::EnterValue, 5);
    ed.perform (Command::EnterValue, 5);
    ed.perform (Command::SlurEnd);
    const auto seq = buildSequence (s);
    CHECK (seq.events.size() == 4);
    const auto& pb = s.playback;
    CHECK (seq.events[0].length == (Tick) std::llround (PPQ * pb.staccatoLength));
    CHECK (seq.events[1].length > PPQ);                                   // legato overlap
    CHECK (seq.events[3].length == (Tick) std::llround (PPQ * pb.slurEndLength)); // lifted
    CHECK (seq.events[1].velocity > seq.events[2].velocity);
}

static void testScheduler()
{
    Score s = Score::makeEmpty (1);
    Editor ed (s);
    ed.setCursor ({ 0, 0, 0, 28 });
    for (int i = 0; i < 4; ++i) ed.perform (Command::EnterValue, 5);
    const auto seq = buildSequence (s);

    Scheduler sch;
    struct Ev { int sample; bool on; int note; };
    std::vector<Ev> evs;
    auto out = [&] (int smp, bool on, int, int note, int) { evs.push_back ({ smp, on, note }); };

    Scheduler::Block b;
    b.playing = true;
    b.beatsPerSample = 2.0 / 48000.0; // 120 bpm at 48k
    b.numSamples = 512;
    double beat = 0;
    int total = 0;
    for (int blk = 0; blk < 200; ++blk)
    {
        b.beat = beat;
        evs.clear();
        sch.process (&seq, b, out);
        for (auto& e : evs)
        {
            if (e.on)
            {
                const double at = beat + e.sample * b.beatsPerSample;
                CHECK (std::abs (at - std::round (at)) < b.beatsPerSample * 1.01);
                ++total;
            }
        }
        beat += b.numSamples * b.beatsPerSample;
    }
    CHECK (total == 4);

    // stopping sends note offs for anything sounding
    Scheduler s2;
    b.beat = 0;
    evs.clear();
    s2.process (&seq, b, out);
    b.playing = false;
    evs.clear();
    s2.process (&seq, b, out);
    CHECK (evs.size() == 1 && ! evs[0].on);
}

static void testSerialization()
{
    Project p = Project::makeDefault();
    Editor ed (p.clips[0].score);
    ed.setCursor ({ 0, 0, 0, 33 });
    ed.perform (Command::EnterValue, 4);
    ed.perform (Command::Accent);
    ed.perform (Command::RaiseHalfStep);
    p.clips[0].score.measures[0].repeatStart = true;
    p.clips[0].score.staves[0].measures[0].layers[0][0].clefBefore = Clef::Alto;
    p.clips[0].name = "Theme \"A\"";
    Clip c2;
    c2.name = "Clip 2";
    c2.score = Score::makeEmpty (2, true);
    p.clips.push_back (c2);

    const auto text = p.toText();
    Project q;
    std::string err;
    CHECK (Project::fromText (text, q, &err));
    CHECK (q.clips.size() == 2);
    CHECK (q.clips[0].name == "Theme \"A\"");
    CHECK (q.clips[1].score.isGrandStaff());
    const auto& e = q.clips[0].score.staves[0].measures[0].layers[0][0];
    CHECK (e.accent && e.notes[0].pitch.alter == 1 && e.clefBefore == Clef::Alto);
    CHECK (q.toText() == text);
    CHECK (! Project::fromText ("garbage", q, &err));
}

static void testBeams()
{
    Score s = Score::makeEmpty (1);
    Editor ed (s);
    ed.setCursor ({ 0, 0, 0, 30 });
    for (int i = 0; i < 8; ++i) ed.perform (Command::EnterValue, 4);
    auto groups = beamGroups (s, 0, 0);
    CHECK (groups.size() == 4); // four pairs in 4/4
    ed.setCursor ({ 0, 0, 2, 30 });
    ed.perform (Command::BreakJoinBeam); // join entry 2 to entry 1
    groups = beamGroups (s, 0, 0);
    CHECK (groups.size() == 3);
    CHECK (groups[0].size() == 4);
}

static void testFinaleMacKeys()
{
    const auto km = KeyMap::defaults();
    KeyStroke k;
    CHECK (parseKey ("alt+3", k) && km.lookup (k).command == Command::StartTuplet && km.lookup (k).arg == 3);
    CHECK (parseKey ("alt+1", k) && km.lookup (k).command == Command::TupletDialog);
    CHECK (parseKey ("alt+shift+5", k) && km.lookup (k).command == Command::EnterRestValue && km.lookup (k).arg == 5);
    CHECK (parseKey ("alt+9", k) && km.lookup (k).command == Command::EnharmonicMeasure);
    CHECK (parseKey ("alt+l", k) && km.lookup (k).command == Command::AutoStem);
    CHECK (parseKey ("alt+equals", k) && km.lookup (k).command == Command::TiePrevious);
    CHECK (parseKey ("'", k) && km.lookup (k).command == Command::ToggleVoice);
    CHECK (parseKey ("ctrl+alt+3", k) && km.lookup (k).command == Command::SetLayer && km.lookup (k).arg == 3);
    CHECK (parseKey ("(", k) && km.lookup (k).command == Command::SlurStart);
    CHECK (parseKey ("esc", k) && km.lookup (k).command == Command::ToggleFrame);
    CHECK (parseKey ("ctrl+v", k) && km.lookup (k).command == Command::Paste);
}

static void testLayers()
{
    Score s = Score::makeEmpty (2);
    Editor ed (s);
    ed.setCursor ({ 0, 0, 0, 35 });
    for (int i = 0; i < 4; ++i) ed.perform (Command::EnterValue, 5);
    CHECK (ed.cursor().measure == 1);
    ed.setCursor ({ 0, 0, 0, 30 });
    ed.perform (Command::ToggleVoice);
    CHECK (ed.cursor().layer == 1);
    ed.perform (Command::EnterValue, 6);
    ed.perform (Command::EnterValue, 6);
    CHECK (s.staves[0].measures[0].layers[1].size() == 2);
    CHECK (s.staves[0].measures[0].layers[0].size() == 4);
    CHECK (layerStemRule (s.staves[0].measures[0], 0) == 1);
    CHECK (layerStemRule (s.staves[0].measures[0], 1) == -1);
    // both layers play
    const auto seq = buildSequence (s);
    CHECK (seq.events.size() == 6);
    // the frame: closed, entry keys do nothing
    ed.perform (Command::ToggleFrame);
    CHECK (! ed.frameActive());
    CHECK (! ed.perform (Command::EnterValue, 5));
    ed.perform (Command::ToggleFrame);
    CHECK (ed.frameActive());
}

static void testCopyPaste()
{
    Score s = Score::makeEmpty (4);
    Editor ed (s);
    ed.setCursor ({ 0, 0, 0, 30 });
    for (int i = 0; i < 4; ++i)
    {
        ed.perform (Command::EnterValue, 5);
        ed.perform (Command::CursorUp);
    }
    ed.setCursor ({ 0, 0, 0, 30 });
    ed.perform (Command::SelectMeasure);
    CHECK (ed.selection().active);
    ed.perform (Command::Copy);
    CHECK (! Editor::clipboard().empty());
    ed.setCursor ({ 0, 2, 0, 30 });
    ed.perform (Command::Paste);
    CHECK (s.staves[0].measures[2].layers[0].size() == 4);
    CHECK (s.staves[0].measures[2].layers[0][3].notes[0].pitch.dia == 33);
    CHECK (s.contentLength (0, 3) == 0);
    // paste over existing music replaces it
    ed.setCursor ({ 0, 0, 2, 30 });
    ed.perform (Command::Paste);
    CHECK (s.contentLength (0, 0) == 4 * PPQ);
    CHECK (s.staves[0].measures[0].layers[0][2].notes[0].pitch.dia == 30);
    CHECK (s.staves[0].measures[1].layers[0].size() >= 2);
    // clear leaves rests
    ed.setCursor ({ 0, 2, 0, 30 });
    ed.perform (Command::SelectMeasure);
    ed.perform (Command::DeleteEntry);
    CHECK (s.staves[0].measures[2].layers[0].size() == 4 && s.staves[0].measures[2].layers[0][0].rest);
}

static void testDynamics()
{
    Score s = Score::makeEmpty (2);
    Editor ed (s);
    ed.setCursor ({ 0, 0, 0, 30 });
    ed.perform (Command::EnterValue, 5);
    ed.perform (Command::SetDynamic, 3); // pp
    ed.perform (Command::EnterValue, 5);
    ed.perform (Command::EnterValue, 5);
    ed.perform (Command::SetDynamic, 7); // f
    ed.perform (Command::EnterValue, 5);
    auto v = computeVelocities (s);
    CHECK (v[0][0][0][0] == s.playback.dynamicVelocity[2]);
    CHECK (v[0][0][0][1] == s.playback.dynamicVelocity[2]);
    CHECK (v[0][0][0][2] == s.playback.dynamicVelocity[6]);
    CHECK (v[0][0][0][3] == s.playback.dynamicVelocity[6]);
    s.staves[0].measures[0].layers[0][1].velocity = 101;
    v = computeVelocities (s);
    CHECK (v[0][0][0][1] == 101);
    const auto seq = buildSequence (s);
    CHECK (seq.events[1].velocity == 101);
}

static void testLayerSerializationAndXml()
{
    Project p = Project::makeDefault();
    auto& sc = p.clips[0].score;
    sc.setGrandStaff (true);
    Editor ed (sc);
    ed.setCursor ({ 0, 0, 0, 30 });
    ed.perform (Command::StartTuplet, 3);
    for (int i = 0; i < 3; ++i) ed.perform (Command::EnterValue, 4);
    ed.perform (Command::SetDynamic, 6);
    ed.perform (Command::EnterValue, 6);
    ed.perform (Command::SetLayer, 2);
    ed.perform (Command::EnterValue, 7);
    p.audition = false;
    p.manualColour = 0xff123456;
    Project q;
    CHECK (Project::fromText (p.toText(), q));
    CHECK (! q.audition && q.manualColour == 0xff123456u);
    CHECK (q.clips[0].score.staves[0].measures[0].layers[1].size() == 1);
    CHECK (q.clips[0].score.staves[0].measures[0].layers[0][2].dynamic == Dynamic::mf);
    const auto xml = toMusicXml (sc, "Test", "test");
    CHECK (xml.find ("<staves>2</staves>") != std::string::npos);
    CHECK (xml.find ("<actual-notes>3</actual-notes>") != std::string::npos);
    CHECK (xml.find ("<dynamics><mf/></dynamics>") != std::string::npos);
    CHECK (xml.find ("<voice>2</voice>") != std::string::npos);
    CHECK (xml.find ("<backup>") != std::string::npos);
    // tags balance (cheap well-formedness check)
    int open = 0;
    for (size_t i = 0; i + 1 < xml.size(); ++i)
        if (xml[i] == '<' && xml[i + 1] != '?' && xml[i + 1] != '!')
        {
            const auto close = xml.find ('>', i);
            const bool selfClosing = xml[close - 1] == '/';
            if (xml[i + 1] == '/') --open;
            else if (! selfClosing) ++open;
        }
    CHECK (open == 0);
}

static void testGrandStaffSplitAndMove()
{
    Score s = Score::makeEmpty (2);
    Editor ed (s);
    // C4 (dia 28), B3 (27), then a chord E3+G4, then A5
    ed.setCursor ({ 0, 0, 0, 28 });
    ed.perform (Command::EnterValue, 5);
    ed.setCursor ({ 0, 0, 1, 27 });
    ed.perform (Command::EnterValue, 5);
    ed.setCursor ({ 0, 0, 2, 23 });
    ed.perform (Command::EnterValue, 5);
    ed.setCursor ({ 0, 0, 2, 32 });
    ed.perform (Command::AddChordNote);
    ed.setCursor ({ 0, 0, 3, 40 });
    ed.perform (Command::EnterValue, 5);
    CHECK (s.staves[0].measures[0].layers[0].size() == 4);

    CHECK (ed.perform (Command::GrandStaff));
    CHECK (s.isGrandStaff());
    const auto& top = s.staves[0].measures[0].layers[0];
    const auto& bot = s.staves[1].measures[0].layers[0];
    CHECK (top.size() == 4 && bot.size() == 4);
    CHECK (! top[0].rest && top[0].notes[0].pitch.dia == 28); // middle C stays up top
    CHECK (top[1].rest && ! bot[1].rest && bot[1].notes[0].pitch.dia == 27);
    CHECK (top[2].notes.size() == 1 && top[2].notes[0].pitch.dia == 32);
    CHECK (bot[2].notes.size() == 1 && bot[2].notes[0].pitch.dia == 23);
    CHECK (bot[0].rest && bot[3].rest);
    CHECK (s.staves[1].clef == Clef::Bass);
    // playback is unchanged: 5 notes
    CHECK (buildSequence (s).events.size() == 5);

    // move the high A down to the lower staff, then back up
    ed.setCursor ({ 0, 0, 3, 40 });
    CHECK (ed.perform (Command::MoveToStaffBelow));
    CHECK (s.staves[0].measures[0].layers[0][3].rest);
    CHECK (! s.staves[1].measures[0].layers[0][3].rest);
    CHECK (s.staves[1].measures[0].layers[0][3].notes[0].pitch.dia == 40);
    CHECK (buildSequence (s).events.size() == 5);
    ed.setCursor ({ 1, 0, 3, 40 });
    CHECK (ed.perform (Command::MoveToStaffAbove));
    CHECK (! s.staves[0].measures[0].layers[0][3].rest);
    CHECK (! ed.perform (Command::MoveToStaffAbove)); // no staff above the top

    // a note whose rhythm doesn't match goes into another layer
    ed.setCursor ({ 0, 1, 0, 35 });
    ed.perform (Command::EnterValue, 4); // eighth
    ed.setCursor ({ 1, 1, 0, 20 });
    ed.perform (Command::EnterValue, 6); // half
    ed.setCursor ({ 0, 1, 0, 35 });
    CHECK (ed.perform (Command::MoveToStaffBelow));
    const auto& m1 = s.staves[1].measures[1];
    CHECK (m1.layers[0].size() >= 1 && m1.layers[0][0].value == 6);
    CHECK (! m1.layers[1].empty() && m1.layers[1][0].notes[0].pitch.dia == 35);

    // back to one staff: nothing is lost
    const auto before = buildSequence (s).events.size();
    CHECK (ed.perform (Command::GrandStaff));
    CHECK (! s.isGrandStaff());
    CHECK (buildSequence (s).events.size() == before);

    // clefs at the insertion bar
    ed.setCursor ({ 0, 0, 0, 30 });
    CHECK (ed.perform (Command::SetClef, 3));
    CHECK (s.staves[0].clef == Clef::Alto);
    ed.setCursor ({ 0, 0, 2, 30 });
    ed.perform (Command::SetClef, 2);
    CHECK (s.staves[0].measures[0].layers[0][2].clefBefore == Clef::Bass);
    ed.setCursor ({ 0, 1, 0, 30 });
    ed.perform (Command::SetClef, 1);
    CHECK (s.staves[0].measures[1].clefChange == Clef::Treble);
}

int main()
{
    testPitch();
    testKeyMap();
    testEntryAndFill();
    testAccidentals();
    testChordsRestsTies();
    testRepeats();
    testArticulations();
    testScheduler();
    testSerialization();
    testBeams();
    testFinaleMacKeys();
    testLayers();
    testCopyPaste();
    testDynamics();
    testLayerSerializationAndXml();
    testGrandStaffSplitAndMove();
    if (failures == 0) std::printf ("all core tests passed\n");
    return failures == 0 ? 0 : 1;
}
