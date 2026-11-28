// Staff Paper — notation rules shared by the editor and the renderer:
// beaming groups, automatic stem direction, onset times.
#pragma once

#include "staffpaper/Score.h"

#include <vector>

namespace staffpaper
{

// Start tick of every entry in a staff measure (relative to the measure start).
std::vector<Tick> entryOnsets (const std::vector<Entry>& entries);

// The length of one beaming group in a meter (e.g. a quarter in 4/4, a dotted quarter in 6/8).
Tick beamGroupLength (const TimeSig& t);

bool isBeamable (const Entry& e);

// For each entry: true when it is beamed to the previous entry.
std::vector<bool> beamedToPrevious (const Score& s, int staff, int measure, int layer = 0);

// Groups of entry indices that share a beam (only groups of 2 or more).
std::vector<std::vector<int>> beamGroups (const Score& s, int staff, int measure, int layer = 0);

// Stem rule from layers: +1 = up, -1 = down, 0 = decide from the notes.
// When a staff measure has music in more than one layer, odd layers point up
// and even layers down (layer numbers counted from 1).
int layerStemRule (const StaffMeasure& m, int layer);

// Staff position of a pitch: 0 = middle line, +1 = one step (line->space) up.
int staffPosition (int dia, Clef clef);

// Automatic stem direction for one entry: true = up.
bool autoStemUp (const Entry& e, Clef clef, int layerRule = 0);
// Direction for a whole beam group (majority / furthest note rule).
bool groupStemUp (const std::vector<Entry>& entries, const std::vector<int>& group, Clef clef, int layerRule = 0);

} // namespace staffpaper
