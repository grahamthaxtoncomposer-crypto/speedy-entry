#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "ClipView.h"
#include "HouseLook.h"
#include "PluginProcessor.h"
#include "StaffView.h"
#include "Widgets.h"

#include <memory>

class StaffPaperEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit StaffPaperEditor (StaffPaperProcessor&);
    ~StaffPaperEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    bool keyPressed (const juce::KeyPress&) override;

private:
    StaffPaperProcessor& proc;
    std::unique_ptr<staffpaper::Editor> editor;
    int editorClip = -1;

    HouseLook look;
    juce::TooltipWindow tooltips { this, 600 };
    StaffView staff;
    ClipView clips;

    // header: the clip's name and clip actions; window buttons on the right
    struct ClipTitle : public juce::Component
    {
        juce::String number, name;
        juce::Colour colour, ink, muted;
        std::function<void()> onClick;
        void paint (juce::Graphics&) override;
        void mouseUp (const juce::MouseEvent&) override { if (onClick) onClick(); }
    } title;
    IconButton newClip { Icon::Plus, "New" }, dupClip { Icon::Duplicate }, delClip { Icon::Trash }, loopClip { Icon::Loop };
    IconButton undoButton { Icon::Undo }, redoButton { Icon::Redo };
    IconButton hearButton { Icon::Speaker, "Hear" };
    IconButton exportButton { Icon::Export, "MusicXML" }, dragButton { Icon::Drag, "MIDI" };
    IconButton commandsButton { Icon::Keyboard }, settingsButton { Icon::Sliders };
    IconButton zoomOut { Icon::Minus }, zoomIn { Icon::Plus }, themeButton { Icon::Sun }, colourButton { Icon::None, "Colour" };

    // launcher row (right side): sync and transport
    IconButton tempoSync { Icon::None, "Tempo: Bitwig" }, meterSync { Icon::None, "Meter: Bitwig" };
    IconButton playButton { Icon::Play }, stopButton { Icon::Stop }, rewindButton { Icon::Rewind };
    juce::Slider tempoSlider;

    // main toolbar
    IconButton speedyTool { Icon::Pencil, "Speedy" }, selectTool { Icon::Arrow, "Select" }, insertButton { Icon::Insert, "Insert" };
    IconButton layerButtons[staffpaper::kLayers];
    IconButton clefButton, grandStaffButton { Icon::None, "Grand staff" };
    IconButton staffUpButton { Icon::StaffUp }, staffDownButton { Icon::StaffDown };
    juce::ComboBox gridBox;

    // palettes: a row of tabs, and the open palette's buttons beside them
    struct PaletteItem
    {
        staffpaper::Command cmd;
        int arg;
        juce::String text, glyph;
        float glyphSize, glyphY;
        Icon icon;
        juce::String tip;
    };
    struct Palette
    {
        juce::String name;
        std::vector<PaletteItem> items;
    };
    std::vector<Palette> palettes;
    std::vector<std::unique_ptr<IconButton>> paletteTabs, paletteButtons;
    int openPalette = 0;
    std::vector<int> separators; // x positions of the group lines in the toolbars

    // status bar
    juce::String lastKey, lastCommand;

    // overlays
    std::unique_ptr<juce::Component> overlay;

    uint32_t shownTrackColour = 0;
    std::unique_ptr<juce::FileChooser> chooser;

    void bindEditor();
    void selectClip (int index);
    void edited();
    void applyTheme();
    juce::Colour accentColour() const;
    void refreshToolbar();
    void showCommandsWindow();
    void showSettings();
    void showColourMenu();
    void showClefMenu();
    void buildPalettes();
    void showPalette (int index);
    juce::String keysFor (staffpaper::Command, int arg) const;
    juce::String tipFor (const juce::String& what, staffpaper::Command, int arg = 0) const;
    void runCommand (staffpaper::Command, int arg = 0);
    void exportMusicXml();
    void startMidiDrag (int clipIndex);
    juce::File writeMidiFile (int clipIndex);
    void timerCallback() override;
    double writtenTickFromPerformance (int clip, double tick) const;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (StaffPaperEditor)
};
