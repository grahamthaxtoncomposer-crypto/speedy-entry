#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "ClipView.h"
#include "HouseLook.h"
#include "PluginProcessor.h"
#include "StaffView.h"

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
    StaffView staff;
    ClipView clips;

    // toolbar
    juce::TextButton speedyTool { "Speedy" }, selectTool { "Select" }, insertButton { "Insert" };
    juce::TextButton layerButtons[staffpaper::kLayers];
    juce::ComboBox gridBox;
    juce::TextButton tempoSync { "Tempo: Bitwig" }, meterSync { "Meter: Bitwig" };
    juce::TextButton playButton, stopButton, rewindButton;
    juce::Slider tempoSlider;
    juce::TextButton hearButton { "Hear notes" };
    juce::TextButton exportButton { "Export MusicXML" }, dragButton { "Drag MIDI" };
    juce::TextButton commandsButton { "Commands" }, settingsButton { "Settings" };
    juce::TextButton zoomOut { "-" }, zoomIn { "+" }, themeButton, colourButton;

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
    void exportMusicXml();
    void startMidiDrag (int clipIndex);
    juce::File writeMidiFile (int clipIndex);
    void timerCallback() override;
    double writtenTickFromPerformance (int clip, double tick) const;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (StaffPaperEditor)
};
