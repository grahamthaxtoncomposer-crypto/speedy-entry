// MusicXML 4.0 export (partwise), readable by every notation program.
#pragma once

#include "staffpaper/Score.h"

#include <string>

namespace staffpaper
{

// The whole score as one part (two staves for a grand staff), layers as voices.
std::string toMusicXml (const Score& score, const std::string& title, const std::string& software);

// META-INF/container.xml for a compressed .mxl that holds `rootFileName`.
std::string musicXmlContainer (const std::string& rootFileName);

} // namespace staffpaper
