// The clip launcher view: a column of clip slots like a launcher track,
// plus "song" mode, where a clip follows the host's timeline.
#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "staffpaper/Project.h"

#include <functional>

class ClipView : public juce::Component
{
public:
    struct Theme
    {
        juce::Colour bg, slot, text, dim, accent;
    };

    ClipView (staffpaper::Project& project);

    void setTheme (const Theme& t) { theme = t; repaint(); }
    void setStatus (int playing, int queued, bool hostPlaying);

    std::function<void (int)> onLaunch;     // clip index
    std::function<void()> onStop;
    std::function<void()> onSong;
    std::function<void (int)> onEdit;       // open the clip in the staff view
    std::function<void()> onProjectEdited;  // structure changed (add, delete, rename...)
    std::function<void (int, const juce::MouseEvent&)> onDragOut; // drag a clip as MIDI

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;

private:
    staffpaper::Project& project;
    Theme theme;
    int playing = -1, queued = -2;
    bool hostPlaying = false;
    int dragFrom = -1;
    bool dragStarted = false;

    static constexpr int kRow = 34;
    juce::Rectangle<int> rowArea (int i) const;
    juce::Rectangle<int> launchButton (int i) const;
    int rowAt (juce::Point<int>) const;
    void showClipMenu (int i);
};
