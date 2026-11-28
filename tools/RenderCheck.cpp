// Draws a sample score with the real staff view into PNG files, so the
// notation can be checked without a DAW. Usage: StaffPaperRenderCheck <outdir>
#include <juce_gui_basics/juce_gui_basics.h>

#include "StaffView.h"
#include "staffpaper/Editor.h"

using namespace staffpaper;

static void type (Editor& ed, std::initializer_list<std::pair<Command, int>> cmds)
{
    for (auto [c, a] : cmds) ed.perform (c, a);
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    const juce::File outDir (argc > 1 ? juce::File::getCurrentWorkingDirectory().getChildFile (argv[1]) : juce::File::getCurrentWorkingDirectory());
    outDir.createDirectory();

    Score score = Score::makeEmpty (6, true);
    score.keyFifths = 2;
    Editor ed (score);
    ed.setCursor ({ 0, 0, 0, 33 }); // A4
    type (ed, { { Command::EnterValue, 4 }, { Command::Staccato, 0 }, { Command::CursorUp, 0 }, { Command::EnterValue, 4 }, { Command::Staccato, 0 },
                { Command::CursorUp, 0 }, { Command::EnterValue, 4 }, { Command::SlurStart, 0 }, { Command::CursorUp, 0 }, { Command::EnterValue, 4 },
                { Command::CursorUp, 0 }, { Command::EnterValue, 3 }, { Command::CursorDown, 0 }, { Command::EnterValue, 3 },
                { Command::CursorDown, 0 }, { Command::EnterValue, 3 }, { Command::RaiseHalfStep, 0 }, { Command::CursorDown, 0 }, { Command::EnterValue, 3 },
                { Command::SlurEnd, 0 }, { Command::EnterValue, 5 }, { Command::Accent, 0 }, { Command::Tenuto, 0 } });
    // measure 2: triplet, chord, dotted, tie over the bar
    type (ed, { { Command::StartTuplet, 3 }, { Command::EnterValue, 4 }, { Command::CursorUp, 0 }, { Command::EnterValue, 4 }, { Command::CursorUp, 0 },
                { Command::EnterValue, 4 }, { Command::EnterValue, 6 }, { Command::CursorUp, 0 }, { Command::CursorUp, 0 }, { Command::AddChordNote, 0 },
                { Command::EnterValue, 5 }, { Command::Dot, 0 }, { Command::EnterValue, 5 } });
    // measure 3-4: rests, low notes with ledger lines, sixteenths
    ed.setCursor ({ 0, 3, 0, 24 });
    type (ed, { { Command::EnterRestValue, 5 }, { Command::EnterValue, 3 }, { Command::CursorUp, 0 }, { Command::EnterValue, 3 },
                { Command::CursorUp, 0 }, { Command::EnterValue, 3 }, { Command::LowerHalfStep, 0 }, { Command::CursorUp, 0 }, { Command::EnterValue, 3 },
                { Command::EnterValue, 6 }, { Command::FreezeStem, 0 } });
    // bass staff
    ed.setCursor ({ 1, 0, 0, 14 });
    type (ed, { { Command::EnterValue, 7 }, { Command::OctaveUp, 0 }, { Command::EnterValue, 6 }, { Command::CursorUp, 0 }, { Command::EnterValue, 6 },
                { Command::EnterValue, 7 }, { Command::TieNext, 0 }, { Command::EnterValue, 7 } });
    score.measures[1].repeatStart = true;
    score.measures[2].repeatEnd = true;
    score.measures[2].endingMask = 1;
    score.measures[3].endingMask = 2;
    score.measures[4].timeSig = TimeSig { 6, 8 };
    score.staves[0].measures[4].clefChange = Clef::Alto;
    score.measures[5].barline = Barline::Final;
    ed.setCursor ({ 0, 4, 0, 28 });
    for (int i = 0; i < 6; ++i)
    {
        ed.perform (Command::EnterValue, 4);
        ed.perform (Command::CursorUp);
    }
    // a second layer in measure 6 and a few dynamics
    ed.setCursor ({ 0, 5, 0, 30 });
    ed.perform (Command::EnterValue, 6);
    ed.perform (Command::SetDynamic, 3);
    ed.perform (Command::EnterValue, 6);
    ed.perform (Command::SetDynamic, 8);
    ed.setCursor ({ 0, 5, 0, 35, 1 });
    for (int i = 0; i < 4; ++i) { ed.perform (Command::EnterValue, 5); ed.perform (Command::CursorDown); }
    ed.setCursor ({ 0, 0, 0, 33 });
    ed.perform (Command::SetDynamic, 6);
    // very high notes on the upper staff, very low on the lower: the staves make room
    score.appendMeasure();
    const int hm = score.numMeasures() - 1;
    score.staves[0].measures[(size_t) hm].clefChange = Clef::Treble;
    ed.setCursor ({ 0, hm, 0, 47 });
    ed.perform (Command::EnterValue, 6);
    ed.setCursor ({ 1, hm, 0, 8 });
    ed.perform (Command::EnterValue, 6);
    ed.setCursor ({ 0, 1, 1, 35 });
    ed.setSelection ({ true, 0, 0, score.measureStart (2), score.measureStart (3) });
    score.staves[0].measures[0].layers[0][2].velocity = 120;

    StaffView view;
    view.setScore (&score, &ed);

    auto shoot = [&] (const char* name, int w, int h) {
        view.setSize (w, h);
        auto img = view.createComponentSnapshot (view.getLocalBounds(), true, 2.0f);
        auto f = outDir.getChildFile (name);
        f.deleteFile();
        juce::FileOutputStream os (f);
        juce::PNGImageFormat().writeImageToStream (img, os);
        std::printf ("wrote %s\n", f.getFullPathName().toRawUTF8());
    };

    const auto lime = juce::Colour (0xffccff66);
    view.setTheme (StaffView::Theme::light (lime));
    shoot ("scroll-light.png", 1800, 520);
    view.setTheme (StaffView::Theme::dark (lime));
    shoot ("scroll-dark.png", 1800, 520);
    view.setTheme (StaffView::Theme::dark (juce::Colour (0xffd9534f))); // a Bitwig track colour
    ed.setFrameActive (false);
    shoot ("frame-closed-dark.png", 1800, 520);
    ed.setFrameActive (true);
    view.setTheme (StaffView::Theme::light (lime));
    view.setStaffSpace (14.0f);
    shoot ("zoomed.png", 1600, 700);

    // key translation sanity checks
    int bad = 0;
    auto expect = [&] (juce::KeyPress kp, const char* want) {
        const auto c = StaffView::keyCandidates (kp);
        const auto got = c.empty() ? std::string ("<none>") : describeKey (c.front());
        if (got != want) { std::printf ("key mismatch: want %s got %s\n", want, got.c_str()); ++bad; }
    };
    expect (juce::KeyPress ('5', 0, '5'), "5");
    expect (juce::KeyPress ('+', 0, '+'), "plus");
    expect (juce::KeyPress ('=', juce::ModifierKeys::shiftModifier, '+'), "plus");
    expect (juce::KeyPress ('*', juce::ModifierKeys::shiftModifier, '*'), "*");
    expect (juce::KeyPress ('3', juce::ModifierKeys::commandModifier, 0), "ctrl+3");
    expect (juce::KeyPress (juce::KeyPress::upKey, juce::ModifierKeys::shiftModifier, 0), "shift+up");
    expect (juce::KeyPress ('s', juce::ModifierKeys::altModifier, 0x00df), "alt+s");
    expect (juce::KeyPress ('5', juce::ModifierKeys::shiftModifier, '%'), "%");
    expect (juce::KeyPress ('\'', 0, '\''), "'");
    expect (juce::KeyPress ('3', juce::ModifierKeys::altModifier, 0x00b3), "alt+3");
    return bad == 0 ? 0 : 1;
}
