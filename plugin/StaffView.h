// The staff view: draws a clip's score in a horizontal scroll view, with an
// enlarged entry frame around the measure being edited, and turns key presses
// into editor commands.
#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "staffpaper/Editor.h"
#include "staffpaper/KeyMap.h"
#include "staffpaper/Score.h"

#include <functional>
#include <map>
#include <vector>

class StaffView : public juce::Component
{
public:
    StaffView();

    void setScore (staffpaper::Score* score, staffpaper::Editor* editor);
    void setKeyMap (const staffpaper::KeyMap* km) { keyMap = km; }
    void setPlayheadTick (double tick); // < 0 hides the playhead
    void setStaffSpace (float s);
    float getStaffSpace() const { return space; }
    int preferredHeight() const;
    void setPageView (bool page);
    bool isPageView() const { return pageView; }

    struct Theme
    {
        juce::Colour paper, ink, hidden, frame, cursor, playhead;
        static Theme light();
        static Theme dark();
    };
    void setDark (bool dark);
    const Theme& getTheme() const { return theme; }

    // Called for every key press: (key description, command name or empty).
    std::function<void (const juce::String&, const juce::String&)> onKey;
    // Called after a menu changed the score.
    std::function<void()> onScoreEdited;

    void paint (juce::Graphics&) override;
    bool keyPressed (const juce::KeyPress&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

    void showMeasureMenu (int measure, juce::Rectangle<int> area);
    void showTupletDialog();

    // Exposed for the render check.
    static std::vector<staffpaper::KeyStroke> keyCandidates (const juce::KeyPress&);

private:
    staffpaper::Score* score = nullptr;
    staffpaper::Editor* editor = nullptr;
    const staffpaper::KeyMap* keyMap = nullptr;
    Theme theme = Theme::light();
    float space = 9.0f;
    float scrollX = 0, scrollY = 0;
    bool pageView = false;
    float yOff = 0; // vertical offset of the system being drawn
    double playheadTick = -1;
    bool followCursor = true;

    struct EntryGeom
    {
        float x = 0;       // notehead left edge
        bool stemUp = true;
        float stemX = 0, stemEndY = 0;
        bool beamed = false;
    };
    struct MeasureGeom
    {
        float x = 0, width = 0;
        float firstNoteX = 0;
        bool frame = false;
        bool systemStart = false;
        float yOff = 0;
        std::vector<std::vector<EntryGeom>> entries; // [staff][entry]
        std::vector<std::pair<staffpaper::Tick, float>> tickX; // onset -> x (for the playhead)
    };
    struct System
    {
        int first = 0, last = 0;
        float yOff = 0, x0 = 0, x1 = 0;
    };
    struct Geometry
    {
        std::vector<MeasureGeom> measures;
        std::vector<System> systems;
        std::vector<float> middleY; // per staff, for the first system
        float totalWidth = 0, totalHeight = 0;
    } geo;

    void computeLayout();
    MeasureGeom layoutMeasure (int m, float x, bool systemStart, float stretch);
    float mid (int staff) const { return geo.middleY[(size_t) staff] + yOff; }
    float yForPos (int staff, int pos) const { return mid (staff) - pos * space * 0.5f; }
    void useMeasure (int m) { yOff = (m >= 0 && m < (int) geo.measures.size()) ? geo.measures[(size_t) m].yOff : 0.0f; }
    float headWidth() const { return space * 1.18f; }
    void computeStems (int staff, int m);
    void ensureCursorVisible();

    void drawMeasure (juce::Graphics&, int m);
    void drawStaffLines (juce::Graphics&, float x0, float x1);
    void drawClef (juce::Graphics&, staffpaper::Clef, int staff, float x, bool small);
    void drawKeySig (juce::Graphics&, int fifths, staffpaper::Clef, int staff, float x);
    void drawTimeSig (juce::Graphics&, const staffpaper::TimeSig&, int staff, float x);
    void drawBarline (juce::Graphics&, int m, float x);
    void drawEntry (juce::Graphics&, int staff, int m, int i);
    void drawBeams (juce::Graphics&, int staff, int m);
    void drawTiesAndSlurs (juce::Graphics&);
    void drawTuplets (juce::Graphics&, int staff, int m);
    void drawCursor (juce::Graphics&);
    void drawPlayhead (juce::Graphics&);
    void glyph (juce::Graphics&, juce::juce_wchar, float x, float y, float scale = 1.0f);

    float clefWidth() const { return space * 3.6f; }
    static int keySigWidthCount (int fifths) { return std::abs (fifths); }

    // hit testing
    bool hitTest (juce::Point<float> p, int& staff, int& measure, int& entry, int& dia) const;
};
