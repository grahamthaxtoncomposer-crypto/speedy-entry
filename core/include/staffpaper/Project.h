// Staff Paper — a project: clips (each a score) plus launcher settings,
// and saving/loading as text.
#pragma once

#include "staffpaper/Score.h"

#include <string>
#include <vector>

namespace staffpaper
{

enum class LaunchQuantize
{
    None,
    Beat,
    Bar,
    TwoBars,
    FourBars,
    Count
};
const char* launchQuantizeName (LaunchQuantize);

struct Clip
{
    std::string name = "Clip";
    uint32_t colour = 0; // 0 = the track's colour
    bool loop = true;
    Score score;
};

struct Project
{
    std::vector<Clip> clips;
    int selectedClip = 0;    // the clip shown in the staff view
    int songClip = 0;        // the clip that follows Bitwig's timeline when nothing is launched (-1 = none)
    LaunchQuantize quantize = LaunchQuantize::Bar;
    bool followHostTempo = true;
    bool followHostMeter = true;
    double offsetBars = 0;   // song clip: where bar 1 of the score sits in Bitwig (in bars)
    bool midiThru = false;   // pass incoming MIDI to the instrument
    bool audition = true;    // play each note once as it is entered
    bool useTrackColour = true;        // take the colour of the Bitwig track
    uint32_t manualColour = 0xffccff66; // used when useTrackColour is off (or the host gives none)

    static Project makeDefault();
    Clip& current() { return clips[(size_t) selectedClip]; }

    std::string toText() const;
    static bool fromText (const std::string& text, Project& out, std::string* error = nullptr);
};

// Plain-text score encoding (also used to copy a clip).
std::string scoreToText (const Score&);
bool scoreFromText (const std::string&, Score& out, std::string* error = nullptr);

} // namespace staffpaper
