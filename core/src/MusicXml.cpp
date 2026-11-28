#include "staffpaper/MusicXml.h"

#include "staffpaper/Layout.h"

#include <algorithm>
#include <set>
#include <sstream>

namespace staffpaper
{

namespace
{
    std::string esc (const std::string& in)
    {
        std::string out;
        for (char c : in)
        {
            switch (c)
            {
                case '&': out += "&amp;"; break;
                case '<': out += "&lt;"; break;
                case '>': out += "&gt;"; break;
                case '"': out += "&quot;"; break;
                default: out += c;
            }
        }
        return out;
    }

    const char* typeName (int value)
    {
        static const char* names[] = { "", "64th", "32nd", "16th", "eighth", "quarter", "half", "whole", "breve" };
        return names[std::clamp (value, 1, 8)];
    }

    const char* stepName (int step)
    {
        static const char* names[] = { "C", "D", "E", "F", "G", "A", "B" };
        return names[std::clamp (step, 0, 6)];
    }

    const char* accidentalName (int alter)
    {
        switch (alter)
        {
            case -2: return "flat-flat";
            case -1: return "flat";
            case 1: return "sharp";
            case 2: return "double-sharp";
            default: return "natural";
        }
    }

    const char* barStyle (Barline b)
    {
        switch (b)
        {
            case Barline::Double: return "light-light";
            case Barline::Final: return "light-heavy";
            case Barline::Dashed: return "dashed";
            case Barline::Dotted: return "dotted";
            case Barline::Thick: return "heavy";
            case Barline::Invisible: return "none";
            default: return "regular";
        }
    }

    void writeClef (std::ostringstream& o, Clef c, int number)
    {
        const char* sign = "G";
        int line = 2, octave = 0;
        switch (c)
        {
            case Clef::Treble: break;
            case Clef::Treble8vb: octave = -1; break;
            case Clef::Treble8va: octave = 1; break;
            case Clef::Bass: sign = "F"; line = 4; break;
            case Clef::Bass8vb: sign = "F"; line = 4; octave = -1; break;
            case Clef::Bass8va: sign = "F"; line = 4; octave = 1; break;
            case Clef::Alto: sign = "C"; line = 3; break;
            case Clef::Tenor: sign = "C"; line = 4; break;
            case Clef::Soprano: sign = "C"; line = 1; break;
            case Clef::MezzoSoprano: sign = "C"; line = 2; break;
            case Clef::Baritone: sign = "C"; line = 5; break;
            case Clef::Percussion: sign = "percussion"; line = 3; break;
            default: break;
        }
        o << "        <clef number=\"" << number << "\"><sign>" << sign << "</sign>";
        if (c != Clef::Percussion) o << "<line>" << line << "</line>";
        if (octave != 0) o << "<clef-octave-change>" << octave << "</clef-octave-change>";
        o << "</clef>\n";
    }

    // Beam values per level for one entry of a beam group.
    std::vector<std::string> beamValues (const std::vector<Entry>& es, const std::vector<int>& group, size_t k)
    {
        auto beams = [&] (size_t idx) { const int v = es[(size_t) group[idx]].value; return v <= 4 ? 5 - v : 0; };
        std::vector<std::string> out;
        const int mine = beams (k);
        for (int level = 0; level < mine; ++level)
        {
            const bool prev = k > 0 && beams (k - 1) > level;
            const bool next = k + 1 < group.size() && beams (k + 1) > level;
            if (level == 0)
                out.push_back (k == 0 ? "begin" : (k + 1 == group.size() ? "end" : "continue"));
            else if (prev && next) out.push_back ("continue");
            else if (next) out.push_back ("begin");
            else if (prev) out.push_back ("end");
            else out.push_back (k + 1 < group.size() ? "forward hook" : "backward hook");
        }
        return out;
    }
} // namespace

std::string musicXmlContainer (const std::string& rootFileName)
{
    return "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
           "<container>\n  <rootfiles>\n    <rootfile full-path=\"" + esc (rootFileName) + "\" media-type=\"application/vnd.recordare.musicxml+xml\"/>\n"
           "  </rootfiles>\n</container>\n";
}

std::string toMusicXml (const Score& s, const std::string& title, const std::string& software)
{
    std::ostringstream o;
    const int nStaves = (int) s.staves.size();
    o << "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"no\"?>\n"
      << "<!DOCTYPE score-partwise PUBLIC \"-//Recordare//DTD MusicXML 4.0 Partwise//EN\" \"http://www.musicxml.org/dtds/partwise.dtd\">\n"
      << "<score-partwise version=\"4.0\">\n"
      << "  <work><work-title>" << esc (title) << "</work-title></work>\n"
      << "  <identification><encoding><software>" << esc (software) << "</software></encoding></identification>\n"
      << "  <part-list>\n    <score-part id=\"P1\"><part-name>" << esc (title.empty() ? "Music" : title) << "</part-name>\n";
    for (int st = 0; st < nStaves; ++st)
        o << "      <midi-instrument id=\"P1-I" << (st + 1) << "\"><midi-channel>" << std::clamp (s.staves[(size_t) st].midiChannel, 1, 16) << "</midi-channel></midi-instrument>\n";
    o << "    </score-part>\n  </part-list>\n  <part id=\"P1\">\n";

    // open ties and slurs, per staff and layer, in written order
    std::vector<std::array<std::set<int>, kLayers>> openTies ((size_t) nStaves);
    int number = 1;
    for (int m = 0; m < s.numMeasures(); ++m)
    {
        const auto& info = s.measures[(size_t) m];
        const bool pickup = info.pickupLength > 0 && m == 0;
        o << "    <measure number=\"" << (pickup ? 0 : number) << "\"" << (pickup ? " implicit=\"yes\"" : "") << ">\n";
        if (! pickup) ++number;

        // left barline: repeat start, ending start
        const bool endingStarts = info.endingMask != 0 && (m == 0 || s.measures[(size_t) m - 1].endingMask != info.endingMask);
        if (info.repeatStart || endingStarts)
        {
            o << "      <barline location=\"left\">";
            if (info.repeatStart) o << "<bar-style>heavy-light</bar-style>";
            if (endingStarts)
            {
                std::string nums;
                for (int b = 0; b < 8; ++b)
                    if (info.endingMask & (1u << b)) nums += (nums.empty() ? "" : ", ") + std::to_string (b + 1);
                o << "<ending number=\"" << nums << "\" type=\"start\">" << nums << ".</ending>";
            }
            if (info.repeatStart) o << "<repeat direction=\"forward\"/>";
            o << "</barline>\n";
        }

        // attributes
        const bool first = m == 0;
        bool clefChange = false;
        for (int st = 0; st < nStaves; ++st) clefChange = clefChange || s.staves[(size_t) st].measures[(size_t) m].clefChange.has_value();
        if (first || info.timeSig || info.keyFifths || clefChange)
        {
            o << "      <attributes>\n";
            if (first) o << "        <divisions>" << PPQ << "</divisions>\n";
            if (first || info.keyFifths) o << "        <key><fifths>" << s.keyAt (m) << "</fifths></key>\n";
            if (first || info.timeSig)
            {
                const auto t = s.timeSigAt (m);
                o << "        <time><beats>" << t.num << "</beats><beat-type>" << t.den << "</beat-type></time>\n";
            }
            if (first && nStaves > 1) o << "        <staves>" << nStaves << "</staves>\n";
            for (int st = 0; st < nStaves; ++st)
                if (first || s.staves[(size_t) st].measures[(size_t) m].clefChange)
                    writeClef (o, s.clefAt (st, m), st + 1);
            o << "      </attributes>\n";
        }

        const Tick cap = s.measureCapacity (m);
        bool firstBlock = true;
        for (int st = 0; st < nStaves; ++st)
        {
            const auto& sm = s.staves[(size_t) st].measures[(size_t) m];
            for (int l = 0; l < kLayers; ++l)
            {
                const auto& es = sm.layers[(size_t) l];
                if (es.empty() && l > 0) continue;
                if (! firstBlock) o << "      <backup><duration>" << cap << "</duration></backup>\n";
                firstBlock = false;
                const int voice = st * kLayers + l + 1;

                if (es.empty())
                {
                    o << "      <note><rest measure=\"yes\"/><duration>" << cap << "</duration><voice>" << voice << "</voice>";
                    if (nStaves > 1) o << "<staff>" << (st + 1) << "</staff>";
                    o << "</note>\n";
                    continue;
                }

                const auto groups = beamGroups (s, st, m, l);
                const int rule = layerStemRule (sm, l);
                Tick written = 0;
                int tupletLeft = 0;
                auto& ties = openTies[(size_t) st][(size_t) l];
                std::set<int> nextTies;
                for (int i = 0; i < (int) es.size(); ++i)
                {
                    const auto& e = es[(size_t) i];
                    const Clef clef = s.clefAtEntry (st, m, i, l);
                    if (e.clefBefore)
                    {
                        o << "      <attributes>\n";
                        writeClef (o, *e.clefBefore, st + 1);
                        o << "      </attributes>\n";
                    }
                    if (e.dynamic)
                    {
                        o << "      <direction placement=\"below\"><direction-type><dynamics><" << dynamicName (*e.dynamic)
                          << "/></dynamics></direction-type><voice>" << voice << "</voice>";
                        if (nStaves > 1) o << "<staff>" << (st + 1) << "</staff>";
                        o << "<sound dynamics=\"" << (s.playback.dynamicVelocity[(size_t) *e.dynamic] * 100 / 90) << "\"/></direction>\n";
                    }

                    // beam group of this entry
                    const std::vector<int>* group = nullptr;
                    size_t posInGroup = 0;
                    for (const auto& g : groups)
                        for (size_t k = 0; k < g.size(); ++k)
                            if (g[k] == i) { group = &g; posInGroup = k; }
                    const bool up = group ? groupStemUp (es, *group, clef, rule) : autoStemUp (e, clef, rule);

                    // tuplet bracket start/stop
                    bool tupletStart = false, tupletStop = false;
                    if (e.tuplet.active())
                    {
                        if (e.tupletStart || tupletLeft <= 0)
                        {
                            tupletStart = true;
                            tupletLeft = e.tuplet.actual;
                        }
                        --tupletLeft;
                        const bool nextIn = i + 1 < (int) es.size() && es[(size_t) i + 1].tuplet.active() && ! es[(size_t) i + 1].tupletStart;
                        tupletStop = tupletLeft <= 0 || ! nextIn;
                        if (tupletStop) tupletLeft = 0;
                    }

                    const size_t count = e.rest ? 1 : e.notes.size();
                    for (size_t k = 0; k < count; ++k)
                    {
                        o << "      <note";
                        if (e.hidden) o << " print-object=\"no\"";
                        if (e.velocity > 0) o << " dynamics=\"" << (e.velocity * 100 / 90) << "\"";
                        o << ">";
                        if (e.grace) o << (e.graceSlash ? "<grace slash=\"yes\"/>" : "<grace/>");
                        if (k > 0) o << "<chord/>";
                        bool tieStart = false, tieStop = false;
                        if (e.rest)
                            o << "<rest/>";
                        else
                        {
                            const auto& n = e.notes[k];
                            const int midi = n.pitch.midi();
                            tieStop = ties.count (midi) > 0;
                            tieStart = n.tieToNext;
                            if (tieStart) nextTies.insert (midi);
                            o << "<pitch><step>" << stepName (n.pitch.step()) << "</step>";
                            if (n.pitch.alter != 0) o << "<alter>" << n.pitch.alter << "</alter>";
                            o << "<octave>" << n.pitch.octave() << "</octave></pitch>";
                        }
                        if (! e.grace) o << "<duration>" << e.duration() << "</duration>";
                        if (tieStop) o << "<tie type=\"stop\"/>";
                        if (tieStart) o << "<tie type=\"start\"/>";
                        o << "<voice>" << voice << "</voice><type>" << typeName (e.value) << "</type>";
                        for (int d = 0; d < e.dots; ++d) o << "<dot/>";
                        if (! e.rest)
                            if (auto acc = s.displayedAccidental (st, m, i, (int) k, l))
                                o << "<accidental" << (e.notes[k].parentheses ? " parentheses=\"yes\"" : "") << ">" << accidentalName (*acc) << "</accidental>";
                        if (e.tuplet.active())
                            o << "<time-modification><actual-notes>" << e.tuplet.actual << "</actual-notes><normal-notes>" << e.tuplet.normal << "</normal-notes></time-modification>";
                        if (! e.rest && e.value <= 6) o << "<stem>" << (up ? "up" : "down") << "</stem>";
                        if (nStaves > 1) o << "<staff>" << (st + 1) << "</staff>";
                        if (group && k == 0)
                        {
                            int level = 1;
                            for (const auto& b : beamValues (es, *group, posInGroup))
                                o << "<beam number=\"" << level++ << "\">" << b << "</beam>";
                        }
                        // notations
                        std::string nt;
                        if (tieStop) nt += "<tied type=\"stop\"/>";
                        if (tieStart) nt += "<tied type=\"start\"/>";
                        if (k == 0)
                        {
                            if (e.slurStart && ! e.rest) nt += "<slur type=\"start\" number=\"1\"/>";
                            if (e.slurEnd && ! e.rest) nt += "<slur type=\"stop\" number=\"1\"/>";
                            if (tupletStart) nt += "<tuplet type=\"start\" bracket=\"yes\"/>";
                            if (tupletStop) nt += "<tuplet type=\"stop\"/>";
                            std::string art;
                            if (e.accent) art += "<accent/>";
                            if (e.staccato) art += "<staccato/>";
                            if (e.tenuto) art += "<tenuto/>";
                            if (! art.empty()) nt += "<articulations>" + art + "</articulations>";
                        }
                        if (! nt.empty()) o << "<notations>" << nt << "</notations>";
                        o << "</note>\n";
                    }
                    if (! e.grace)
                    {
                        ties = nextTies;
                        nextTies.clear();
                    }
                    written += e.duration();
                }
                // a short layer is padded so the voices line up
                if (written < cap && l > 0)
                    o << "      <forward><duration>" << (cap - written) << "</duration><voice>" << voice << "</voice>"
                      << (nStaves > 1 ? "<staff>" + std::to_string (st + 1) + "</staff>" : std::string()) << "</forward>\n";
                else if (written < cap)
                    o << "      <forward><duration>" << (cap - written) << "</duration></forward>\n";
                else if (written > cap)
                    o << "      <backup><duration>" << (written - cap) << "</duration></backup>\n";
            }
        }

        // right barline
        const bool endingStops = info.endingMask != 0 && (m + 1 == s.numMeasures() || s.measures[(size_t) m + 1].endingMask != info.endingMask);
        if (info.repeatEnd || endingStops || info.barline != Barline::Single)
        {
            o << "      <barline location=\"right\"><bar-style>"
              << (info.repeatEnd ? "light-heavy" : barStyle (info.barline))
              << "</bar-style>";
            if (endingStops)
            {
                std::string nums;
                for (int b = 0; b < 8; ++b)
                    if (info.endingMask & (1u << b)) nums += (nums.empty() ? "" : ", ") + std::to_string (b + 1);
                o << "<ending number=\"" << nums << "\" type=\"" << (info.repeatEnd ? "stop" : "discontinue") << "\"/>";
            }
            if (info.repeatEnd) o << "<repeat direction=\"backward\" times=\"" << std::max (2, info.repeatTimes) << "\"/>";
            o << "</barline>\n";
        }
        o << "    </measure>\n";
    }
    o << "  </part>\n</score-partwise>\n";
    return o.str();
}

} // namespace staffpaper
