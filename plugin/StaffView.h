// The staff view: draws a clip's score in a horizontal scroll view, with an
// enlarged entry frame around the measure being edited, a velocity lane under
// the staves, and turns key presses and mouse gestures into editor commands.
#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "staffpaper/Editor.h"
#include "staffpaper/KeyMap.h"
#include "staffpaper/Playback.h"
#include "staffpaper/Score.h"

#include <array>
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

    enum class Tool
    {
        Speedy, // click places the insertion bar and opens the entry frame
        Select  // click and drag select music
    };
    void setTool (Tool t);
    Tool getTool() const { return tool; }

    // Selection grid: 0 = free, 1..10 = 128th ... double whole, 11 = measure.
    void setGrid (int g) { grid = juce::jlimit (0, 11, g); }
    int getGrid() const { return grid; }
    static juce::String gridName (int g);

    void setShowVelocityLane (bool on) { showLane = on; repaint(); }

    struct Theme
    {
        juce::Colour paper, ink, hidden, panel, line, muted, accent, playhead;
        static Theme light (juce::Colour accent);
        static Theme dark (juce::Colour accent);
        // The accent as a mark on the paper (dark enough to read on light paper).
        juce::Colour mark() const;
    };
    void setTheme (const Theme& t);
    const Theme& getTheme() const { return theme; }

    // Called for every key press: (key description, command name or empty).
    std::function<void (const juce::String&, const juce::String&)> onKey;
    // Called after a menu or the mouse changed the score.
    std::function<void()> onScoreEdited;
    // Called for commands the window handles (show commands).
    std::function<void (staffpaper::Command)> onWindowCommand;

    void paint (juce::Graphics&) override;
    void resized() override { spacingDirty = true; }
    bool keyPressed (const juce::KeyPress&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

    // Run a command as if its key was pressed (used by the toolbar buttons).
    bool runCommand (staffpaper::Command cmd, int arg = 0);

    void showMeasureMenu (int measure, juce::Rectangle<int> area);
    void showDynamicsMenu();
    void showTupletDialog();

    // Exposed for the render check.
    static std::vector<staffpaper::KeyStroke> keyCandidates (const juce::KeyPress&);

private:
    staffpaper::Score* score = nullptr;
    staffpaper::Editor* editor = nullptr;
    const staffpaper::KeyMap* keyMap = nullptr;
    Theme theme = Theme::light (juce::Colour (0xffccff66));
    float space = 9.0f;
    float scrollX = 0, scrollY = 0;
    double playheadTick = -1;
    bool followCursor = true;
    Tool tool = Tool::Speedy;
    int grid = 0;
    bool showLane = true;

    // vertical spacing is worked out again only when the frame moves or closes
    bool spacingDirty = true;
    int spacingMeasure = -1;
    bool spacingFrame = true;
    std::vector<float> middleY; // per staff
    float stavesBottom = 0;

    struct EntryGeom
    {
        float x = 0;       // notehead left edge
        bool stemUp = true;
        float stemX = 0, stemEndY = 0;
        bool beamed = false;
    };
    using LayerGeoms = std::array<std::vector<EntryGeom>, staffpaper::kLayers>;
    struct MeasureGeom
    {
        float x = 0, width = 0;
        float firstNoteX = 0;
        bool frame = false;
        bool systemStart = false;
        std::vector<LayerGeoms> entries; // [staff][layer][entry]
        std::vector<std::pair<staffpaper::Tick, float>> tickX; // onset -> x (relative ticks)
    };
    struct Geometry
    {
        std::vector<MeasureGeom> measures;
        float totalWidth = 0, totalHeight = 0;
    } geo;
    staffpaper::VelocityTable velocities;

    // mouse
    enum class Drag { None, Select, Velocity } drag = Drag::None;
    juce::Point<float> dragStart, dragNow;
    struct VelDrag { int staff, measure, layer, entry, from; };
    std::vector<VelDrag> velDrag;
    staffpaper::Selection selBeforeDrag;

    void computeLayout();
    void computeSpacing();
    MeasureGeom layoutMeasure (int m, float x, bool systemStart);
    float mid (int staff) const { return middleY[(size_t) staff]; }
    float yForPos (int staff, int pos) const { return mid (staff) - pos * space * 0.5f; }
    float headWidth() const { return space * 1.18f; }
    float laneHeight() const { return showLane ? 92.0f : 0.0f; }
    juce::Rectangle<float> laneArea() const;
    void computeStems (int staff, int m, int layer);
    void ensureCursorVisible();
    bool layerDimmed (int layer) const;

    void drawMeasure (juce::Graphics&, int m);
    void drawStaffLines (juce::Graphics&, float x0, float x1);
    void drawClef (juce::Graphics&, staffpaper::Clef, int staff, float x, bool small);
    void drawKeySig (juce::Graphics&, int fifths, staffpaper::Clef, int staff, float x);
    void drawTimeSig (juce::Graphics&, const staffpaper::TimeSig&, int staff, float x);
    void drawBarline (juce::Graphics&, int m, float x);
    void drawEntry (juce::Graphics&, int staff, int m, int layer, int i);
    void drawBeams (juce::Graphics&, int staff, int m, int layer);
    void drawTiesAndSlurs (juce::Graphics&);
    void drawTuplets (juce::Graphics&, int staff, int m, int layer);
    void drawSelection (juce::Graphics&);
    void drawCursor (juce::Graphics&);
    void drawPlayhead (juce::Graphics&);
    void drawLane (juce::Graphics&);
    void glyph (juce::Graphics&, juce::juce_wchar, float x, float y, float scale = 1.0f);
    juce::Colour entryColour (int staff, int m, int layer, int i, const staffpaper::Entry& e) const;

    float clefWidth() const { return space * 3.6f; }

    // positions and hit testing
    bool hitTest (juce::Point<float> p, int& staff, int& measure, int& entry, int& dia) const;
    int measureAtX (float contentX) const;
    double tickAtX (float contentX) const;            // absolute written tick
    float xForTick (int m, staffpaper::Tick rel) const; // content x
    staffpaper::Tick gridTicks (int measure) const;   // 0 = free
    void selectRange (juce::Point<float> a, juce::Point<float> b, bool extend);
    bool laneHit (juce::Point<float> p, int& staff, int& measure, int& layer, int& entry) const;
};
