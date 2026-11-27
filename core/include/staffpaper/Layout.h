// Staff Paper — notation rules shared by the editor and the renderer:
// beaming groups, automatic stem direction, onset times.
#pragma once

#include "staffpaper/Score.h"

#include <vector>

namespace staffpaper
{

// Start tick of every entry in a staff measure (relative to the measure start).
std::vector<Tick> entryOnsets (const StaffMeasure& m);

// The length of one beaming group in a meter (e.g. a quarter in 4/4, a dotted quarter in 6/8).
Tick beamGroupLength (const TimeSig& t);

bool isBeamable (const Entry& e);

// For each entry: true when it is beamed to the previous entry.
std::vector<bool> beamedToPrevious (const Score& s, int staff, int measure);

// Groups of entry indices that share a beam (only groups of 2 or more).
std::vector<std::vector<int>> beamGroups (const Score& s, int staff, int measure);

// Staff position of a pitch: 0 = middle line, +1 = one step (line->space) up.
int staffPosition (int dia, Clef clef);

// Automatic stem direction for one entry: true = up.
bool autoStemUp (const Entry& e, Clef clef);
// Direction for a whole beam group (majority / furthest note rule).
bool groupStemUp (const StaffMeasure& m, const std::vector<int>& group, Clef clef);

} // namespace staffpaper
