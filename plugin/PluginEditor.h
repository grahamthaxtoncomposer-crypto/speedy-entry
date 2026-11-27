#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "ClipView.h"
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

    StaffView staff;
    ClipView clips;
    juce::Viewport clipViewport;

    // top bar
    juce::TextButton notationTab { "Notation" }, clipsTab { "Clips" };
    juce::Label clipName;
    juce::ToggleButton syncToggle { "Sync to Bitwig" };
    juce::TextButton playButton, stopButton, rewindButton;
    juce::Slider tempoSlider;
    juce::TextButton viewButton, zoomOut { "-" }, zoomIn { "+" }, themeButton, keysButton { "Keys" }, settingsButton { "Settings" };
    juce::TextButton dragButton { "Drag MIDI" };

    // status bar
    juce::String lastKey, lastCommand;

    // overlays
    std::unique_ptr<juce::Component> overlay;

    bool showingClips = false;
    juce::LookAndFeel_V4 lnf;

    void bindEditor();
    void edited();
    void applyTheme();
    void showNotation();
    void showClips();
    void showKeysWindow();
    void showSettings();
    void startMidiDrag (int clipIndex);
    juce::File writeMidiFile (int clipIndex);
    void timerCallback() override;
    void updateTransportButtons();
    double writtenTickFromPerformance (int clip, double tick) const;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (StaffPaperEditor)
};
