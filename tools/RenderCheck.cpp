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
    ed.setCursor ({ 0, 1, 1, 35 });

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

    shoot ("scroll-light.png", 1600, view.preferredHeight());
    view.setDark (true);
    shoot ("scroll-dark.png", 1600, view.preferredHeight());
    view.setDark (false);
    view.setPageView (true);
    shoot ("page-light.png", 900, 700);
    view.setStaffSpace (14.0f);
    view.setPageView (false);
    shoot ("zoomed.png", 1400, view.preferredHeight());

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
    return bad == 0 ? 0 : 1;
}
