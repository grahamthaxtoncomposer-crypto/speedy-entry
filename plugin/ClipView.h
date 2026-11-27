// The clip strip: the clips laid out left to right like Bitwig's launcher
// slots, with a stop button, the "song" clip that follows the timeline,
// launch quantize, and a slot for a new clip.
#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "staffpaper/Project.h"

#include <functional>

class ClipView : public juce::Component
{
public:
    struct Theme
    {
        juce::Colour bg, slot, text, dim, line, trackColour, playing;
    };

    ClipView (staffpaper::Project& project);

    void setTheme (const Theme& t) { theme = t; repaint(); }
    void setStatus (int playing, int queued, bool hostPlaying, double playingProgress);

    std::function<void (int)> onLaunch;     // clip index
    std::function<void()> onStop;
    std::function<void()> onSong;
    std::function<void (int)> onSelect;     // show the clip in the staff view
    std::function<void()> onProjectEdited;  // structure changed (add, delete, rename...)
    std::function<void (int, const juce::MouseEvent&)> onDragOut; // drag a clip as MIDI

    // The colour a clip is drawn in: its own, or the track's when it follows the track.
    juce::Colour clipColour (int i) const;
    void scrollToSelected();
    int rightReserve = 0; // room on the right for the window's own buttons

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

private:
    staffpaper::Project& project;
    Theme theme;
    int playing = -1, queued = -2;
    bool hostPlaying = false;
    double progress = 0;
    int dragFrom = -1;
    bool dragStarted = false;
    float scroll = 0;

    static constexpr int kSlot = 132, kGap = 4;
    juce::Rectangle<int> stopButton() const;
    juce::Rectangle<int> songButton() const;
    juce::Rectangle<int> quantizeButton() const;
    juce::Rectangle<int> slotsArea() const;
    juce::Rectangle<int> slotArea (int i) const; // i == size() is the "+" slot
    juce::Rectangle<int> launchButton (int i) const;
    int slotAt (juce::Point<int>) const;
    void showClipMenu (int i);
    void rename (int i);
    void addClip();
};
