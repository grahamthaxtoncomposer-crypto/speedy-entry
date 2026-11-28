#include "staffpaper/Project.h"

#include <algorithm>
#include <cctype>
#include <sstream>

namespace staffpaper
{

const char* launchQuantizeName (LaunchQuantize q)
{
    switch (q)
    {
        case LaunchQuantize::None: return "Off";
        case LaunchQuantize::Beat: return "1 beat";
        case LaunchQuantize::Bar: return "1 bar";
        case LaunchQuantize::TwoBars: return "2 bars";
        case LaunchQuantize::FourBars: return "4 bars";
        default: return "?";
    }
}

Project Project::makeDefault()
{
    Project p;
    Clip c;
    c.name = "Clip 1";
    c.loop = false;
    c.score = Score::makeEmpty (8);
    p.clips.push_back (c);
    return p;
}

// ---------------------------------------------------------------- token I/O

namespace
{
    struct Writer
    {
        std::ostringstream o;
        Writer& tok (const std::string& s) { o << s << ' '; return *this; }
        Writer& num (long long v) { o << v << ' '; return *this; }
        Writer& real (double v) { o << v << ' '; return *this; }
        Writer& str (const std::string& s)
        {
            o << '"';
            for (char c : s)
            {
                if (c == '"' || c == '\\') o << '\\';
                if (c == '\n') { o << "\\n"; continue; }
                o << c;
            }
            o << "\" ";
            return *this;
        }
        Writer& nl() { o << '\n'; return *this; }
    };

    struct Reader
    {
        const std::string& s;
        size_t p = 0;
        bool ok = true;
        std::string err;

        explicit Reader (const std::string& text) : s (text) {}

        void skip()
        {
            while (p < s.size() && std::isspace ((unsigned char) s[p])) ++p;
        }
        bool atEnd() { skip(); return p >= s.size(); }
        std::string tok()
        {
            skip();
            size_t a = p;
            while (p < s.size() && ! std::isspace ((unsigned char) s[p])) ++p;
            if (a == p) fail ("unexpected end");
            return s.substr (a, p - a);
        }
        long long num()
        {
            auto t = tok();
            try { return std::stoll (t); }
            catch (...) { fail ("bad number '" + t + "'"); return 0; }
        }
        double real()
        {
            auto t = tok();
            try { return std::stod (t); }
            catch (...) { fail ("bad number '" + t + "'"); return 0; }
        }
        std::string str()
        {
            skip();
            if (p >= s.size() || s[p] != '"') { fail ("expected string"); return {}; }
            ++p;
            std::string out;
            while (p < s.size() && s[p] != '"')
            {
                if (s[p] == '\\' && p + 1 < s.size())
                {
                    ++p;
                    out += (s[p] == 'n') ? '\n' : s[p];
                }
                else
                    out += s[p];
                ++p;
            }
            ++p;
            return out;
        }
        void expect (const char* word)
        {
            auto t = tok();
            if (t != word) fail (std::string ("expected '") + word + "', got '" + t + "'");
        }
        void fail (const std::string& e)
        {
            if (ok) err = e;
            ok = false;
            p = s.size();
        }
    };

    template <typename E>
    E clampEnum (long long v, E count)
    {
        if (v < 0 || v >= (long long) count) return (E) 0;
        return (E) v;
    }

    void writeScore (Writer& w, const Score& s)
    {
        w.tok ("score").num (3).nl();
        w.tok ("title").str (s.title).nl();
        w.tok ("meter").num (s.timeSig.num).num (s.timeSig.den).tok ("key").num (s.keyFifths).nl();
        const auto& pb = s.playback;
        w.tok ("playback").real (pb.normalLength).real (pb.staccatoLength).num (pb.staccatoVelocity)
            .real (pb.tenutoLength).real (pb.legatoOverlap).real (pb.slurEndLength)
            .num (pb.slurStartVelocity).num (pb.slurEndVelocity).num (pb.velocity).num (pb.accentBoost).nl();
        w.tok ("dynamics");
        for (int v : pb.dynamicVelocity) w.num (v);
        w.nl();
        w.tok ("measures").num (s.numMeasures()).nl();
        for (const auto& m : s.measures)
        {
            w.tok ("m");
            if (m.timeSig) w.num (m.timeSig->num).num (m.timeSig->den); else w.num (0).num (0);
            if (m.keyFifths) w.num (1).num (*m.keyFifths); else w.num (0).num (0);
            w.num ((int) m.barline).num (m.repeatStart).num (m.repeatEnd).num (m.repeatTimes)
                .num (m.endingMask).num (m.pickupLength).nl();
        }
        w.tok ("staves").num ((long long) s.staves.size()).nl();
        for (const auto& st : s.staves)
        {
            w.tok ("staff").num ((int) st.clef).num (st.midiChannel).nl();
            for (const auto& sm : st.measures)
            {
                w.tok ("sm").num (sm.clefChange ? (int) *sm.clefChange : -1).num (kLayers).nl();
                for (const auto& layer : sm.layers)
                {
                    w.tok ("layer").num ((long long) layer.size()).nl();
                    for (const auto& e : layer)
                    {
                        unsigned flags = 0;
                        int bit = 0;
                        for (bool f : { e.tupletStart, e.rest, e.hidden, e.grace, e.flatBeam, e.staccato, e.tenuto, e.accent, e.slurStart, e.slurEnd, e.graceSlash })
                            flags |= (f ? 1u : 0u) << bit++;
                        w.tok ("e").num (e.value).num (e.dots).num (e.tuplet.actual).num (e.tuplet.normal)
                            .num (flags).num ((int) e.stem).num ((int) e.beam)
                            .num (e.clefBefore ? (int) *e.clefBefore : -1)
                            .num (e.dynamic ? (int) *e.dynamic : -1).num (e.velocity)
                            .num ((long long) e.notes.size());
                        for (const auto& n : e.notes)
                            w.num (n.pitch.dia).num (n.pitch.alter).num (n.tieToNext).num ((int) n.accidental).num (n.parentheses);
                        w.nl();
                    }
                }
            }
        }
        w.tok ("endscore").nl();
    }

    void readScore (Reader& r, Score& s)
    {
        s = Score {};
        r.expect ("score");
        const auto version = r.num();
        if (version < 1 || version > 3) r.fail ("unsupported score version");
        r.expect ("title");
        s.title = r.str();
        r.expect ("meter");
        s.timeSig.num = (int) std::max (1LL, r.num());
        s.timeSig.den = (int) std::max (1LL, r.num());
        r.expect ("key");
        s.keyFifths = (int) r.num();
        r.expect ("playback");
        auto& pb = s.playback;
        pb.normalLength = r.real();
        pb.staccatoLength = r.real();
        pb.staccatoVelocity = (int) r.num();
        pb.tenutoLength = r.real();
        pb.legatoOverlap = r.real();
        pb.slurEndLength = r.real();
        pb.slurStartVelocity = (int) r.num();
        pb.slurEndVelocity = (int) r.num();
        pb.velocity = (int) r.num();
        pb.accentBoost = (int) r.num();
        if (version >= 3)
        {
            r.expect ("dynamics");
            for (auto& v : pb.dynamicVelocity) v = std::clamp ((int) r.num(), 1, 127);
        }
        r.expect ("measures");
        const auto nm = r.num();
        if (nm < 1 || nm > 100000) r.fail ("bad measure count");
        for (long long i = 0; i < nm && r.ok; ++i)
        {
            r.expect ("m");
            MeasureInfo m;
            const int tn = (int) r.num(), td = (int) r.num();
            if (tn > 0 && td > 0) m.timeSig = TimeSig { tn, td };
            const bool hasKey = r.num() != 0;
            const int k = (int) r.num();
            if (hasKey) m.keyFifths = k;
            m.barline = clampEnum ((long long) r.num(), Barline::Count);
            m.repeatStart = r.num() != 0;
            m.repeatEnd = r.num() != 0;
            m.repeatTimes = (int) r.num();
            m.endingMask = (unsigned) r.num();
            m.pickupLength = r.num();
            s.measures.push_back (m);
        }
        r.expect ("staves");
        const auto ns = r.num();
        if (ns < 1 || ns > 16) r.fail ("bad staff count");
        for (long long si = 0; si < ns && r.ok; ++si)
        {
            r.expect ("staff");
            Staff st;
            st.clef = clampEnum (r.num(), Clef::Count);
            st.midiChannel = (int) r.num();
            for (long long mi = 0; mi < nm && r.ok; ++mi)
            {
                r.expect ("sm");
                StaffMeasure sm;
                const auto c = r.num();
                if (c >= 0) sm.clefChange = clampEnum (c, Clef::Count);
                const auto nl = (version >= 3) ? r.num() : 1;
                for (long long li = 0; li < nl && r.ok; ++li)
                {
                    if (version >= 3) r.expect ("layer");
                    const auto ne = r.num();
                    for (long long ei = 0; ei < ne && r.ok; ++ei)
                    {
                        r.expect ("e");
                        Entry e;
                        e.value = std::clamp ((int) r.num(), 1, 8);
                        e.dots = std::clamp ((int) r.num(), 0, 3);
                        e.tuplet.actual = (int) r.num();
                        e.tuplet.normal = (int) r.num();
                        const auto flags = (unsigned) r.num();
                        int bit = 0;
                        for (bool* f : { &e.tupletStart, &e.rest, &e.hidden, &e.grace, &e.flatBeam, &e.staccato, &e.tenuto, &e.accent, &e.slurStart, &e.slurEnd, &e.graceSlash })
                            *f = (flags >> bit++) & 1u;
                        e.stem = (StemDir) std::clamp ((int) r.num(), 0, 2);
                        e.beam = (BeamMode) std::clamp ((int) r.num(), 0, 2);
                        const auto cb = r.num();
                        if (cb >= 0) e.clefBefore = clampEnum (cb, Clef::Count);
                        if (version >= 3)
                        {
                            const auto dyn = r.num();
                            if (dyn >= 0) e.dynamic = clampEnum (dyn, Dynamic::Count);
                            e.velocity = std::clamp ((int) r.num(), 0, 127);
                        }
                        const auto nn = r.num();
                        for (long long k = 0; k < nn && r.ok; ++k)
                        {
                            Note n;
                            n.pitch.dia = (int) r.num();
                            n.pitch.alter = (int) r.num();
                            n.tieToNext = r.num() != 0;
                            n.accidental = (AccidentalMode) std::clamp ((int) r.num(), 0, 2);
                            n.parentheses = r.num() != 0;
                            e.notes.push_back (n);
                        }
                        if (li < kLayers) sm.layers[(size_t) li].push_back (e);
                    }
                }
                st.measures.push_back (sm);
            }
            s.staves.push_back (st);
        }
        r.expect ("endscore");
    }
} // namespace

std::string scoreToText (const Score& s)
{
    Writer w;
    writeScore (w, s);
    return w.o.str();
}

bool scoreFromText (const std::string& text, Score& out, std::string* error)
{
    Reader r (text);
    Score s;
    readScore (r, s);
    if (! r.ok)
    {
        if (error) *error = r.err;
        return false;
    }
    out = s;
    return true;
}

std::string Project::toText() const
{
    Writer w;
    w.tok ("staffpaper-project").num (2).nl();
    w.tok ("settings").num (selectedClip).num (songClip).num ((int) quantize).num (followHostTempo)
        .num (followHostMeter).real (offsetBars).num (midiThru).nl();
    w.tok ("settings2").num (audition).num (useTrackColour).num (manualColour).nl();
    w.tok ("clips").num ((long long) clips.size()).nl();
    for (const auto& c : clips)
    {
        w.tok ("clip").str (c.name).num (c.colour).num (c.loop).nl();
        writeScore (w, c.score);
    }
    return w.o.str();
}

bool Project::fromText (const std::string& text, Project& out, std::string* error)
{
    Reader r (text);
    Project p;
    r.expect ("staffpaper-project");
    const auto pv = r.num();
    if (pv < 1 || pv > 2) r.fail ("unsupported project version");
    r.expect ("settings");
    p.selectedClip = (int) r.num();
    p.songClip = (int) r.num();
    p.quantize = clampEnum (r.num(), LaunchQuantize::Count);
    p.followHostTempo = r.num() != 0;
    p.followHostMeter = r.num() != 0;
    p.offsetBars = r.real();
    p.midiThru = r.num() != 0;
    if (pv >= 2)
    {
        r.expect ("settings2");
        p.audition = r.num() != 0;
        p.useTrackColour = r.num() != 0;
        p.manualColour = (uint32_t) r.num();
    }
    r.expect ("clips");
    const auto nc = r.num();
    if (nc < 1 || nc > 1024) r.fail ("bad clip count");
    for (long long i = 0; i < nc && r.ok; ++i)
    {
        r.expect ("clip");
        Clip c;
        c.name = r.str();
        c.colour = (uint32_t) r.num();
        c.loop = r.num() != 0;
        readScore (r, c.score);
        p.clips.push_back (c);
    }
    if (! r.ok)
    {
        if (error) *error = r.err;
        return false;
    }
    p.selectedClip = std::clamp (p.selectedClip, 0, (int) p.clips.size() - 1);
    if (p.songClip >= (int) p.clips.size()) p.songClip = -1;
    out = std::move (p);
    return true;
}

} // namespace staffpaper
