#include "staffpaper/KeyMap.h"

#include <cctype>
#include <sstream>

namespace staffpaper
{

namespace
{
    struct CmdName
    {
        Command cmd;
        const char* name;
    };

    const CmdName kCommandNames[] = {
        { Command::None, "None" },
        { Command::EnterValue, "EnterValue" },
        { Command::InsertValue, "InsertValue" },
        { Command::EnterRestValue, "EnterRestValue" },
        { Command::Dot, "Dot" },
        { Command::StartTuplet, "StartTuplet" },
        { Command::TupletDialog, "TupletDialog" },
        { Command::AddChordNote, "AddChordNote" },
        { Command::RemoveChordNote, "RemoveChordNote" },
        { Command::ClearToRest, "ClearToRest" },
        { Command::ToggleRest, "ToggleRest" },
        { Command::DeleteEntry, "DeleteEntry" },
        { Command::RaiseHalfStep, "RaiseHalfStep" },
        { Command::LowerHalfStep, "LowerHalfStep" },
        { Command::RaiseHalfStepMeasure, "RaiseHalfStepMeasure" },
        { Command::LowerHalfStepMeasure, "LowerHalfStepMeasure" },
        { Command::Enharmonic, "Enharmonic" },
        { Command::EnharmonicMeasure, "EnharmonicMeasure" },
        { Command::ToggleAccidental, "ToggleAccidental" },
        { Command::AccidentalParens, "AccidentalParens" },
        { Command::TieNext, "TieNext" },
        { Command::TiePrevious, "TiePrevious" },
        { Command::BreakJoinBeam, "BreakJoinBeam" },
        { Command::ResetBeams, "ResetBeams" },
        { Command::FlatBeam, "FlatBeam" },
        { Command::FreezeStem, "FreezeStem" },
        { Command::AutoStem, "AutoStem" },
        { Command::HideEntry, "HideEntry" },
        { Command::GraceNote, "GraceNote" },
        { Command::SlashGraceNote, "SlashGraceNote" },
        { Command::FreezeAccidental, "FreezeAccidental" },
        { Command::Staccato, "Staccato" },
        { Command::Tenuto, "Tenuto" },
        { Command::Accent, "Accent" },
        { Command::SlurStart, "SlurStart" },
        { Command::SlurEnd, "SlurEnd" },
        { Command::CursorUp, "CursorUp" },
        { Command::CursorDown, "CursorDown" },
        { Command::OctaveUp, "OctaveUp" },
        { Command::OctaveDown, "OctaveDown" },
        { Command::PrevEntry, "PrevEntry" },
        { Command::NextEntry, "NextEntry" },
        { Command::PrevMeasure, "PrevMeasure" },
        { Command::NextMeasure, "NextMeasure" },
        { Command::StaffUp, "StaffUp" },
        { Command::StaffDown, "StaffDown" },
        { Command::ToggleInsertMode, "ToggleInsertMode" },
        { Command::SetLayer, "SetLayer" },
        { Command::ToggleVoice, "ToggleVoice" },
        { Command::ToggleFrame, "ToggleFrame" },
        { Command::SelectNext, "SelectNext" },
        { Command::SelectPrev, "SelectPrev" },
        { Command::SelectMeasure, "SelectMeasure" },
        { Command::SelectAll, "SelectAll" },
        { Command::Copy, "Copy" },
        { Command::Cut, "Cut" },
        { Command::Paste, "Paste" },
        { Command::ClearSelection, "ClearSelection" },
        { Command::SetDynamic, "SetDynamic" },
        { Command::DynamicsMenu, "DynamicsMenu" },
        { Command::ShowCommands, "ShowCommands" },
        { Command::PrevClip, "PrevClip" },
        { Command::NextClip, "NextClip" },
        { Command::Undo, "Undo" },
        { Command::Redo, "Redo" },
        { Command::MeasureMenu, "MeasureMenu" },
        { Command::MoveToStaffAbove, "MoveToStaffAbove" },
        { Command::MoveToStaffBelow, "MoveToStaffBelow" },
        { Command::GrandStaff, "GrandStaff" },
        { Command::SetClef, "SetClef" },
    };

    struct KeyName
    {
        int key;
        const char* name;
    };

    const KeyName kKeyNames[] = {
        { KeyUp, "up" },
        { KeyDown, "down" },
        { KeyLeft, "left" },
        { KeyRight, "right" },
        { KeyEnter, "enter" },
        { KeyBackspace, "backspace" },
        { KeyDelete, "delete" },
        { KeyInsert, "insert" },
        { KeyTab, "tab" },
        { KeyEscape, "esc" },
        { KeyHome, "home" },
        { KeyEnd, "end" },
        { KeyPageUp, "pageup" },
        { KeyPageDown, "pagedown" },
        { KeySpace, "space" },
        // characters that are awkward to type in a "key = command" line
        { '=', "equals" },
        { '#', "hash" },
        { '+', "plus" },
        { '-', "minus" },
    };

    std::string trim (const std::string& s)
    {
        size_t a = 0, b = s.size();
        while (a < b && std::isspace ((unsigned char) s[a])) ++a;
        while (b > a && std::isspace ((unsigned char) s[b - 1])) --b;
        return s.substr (a, b - a);
    }

    std::string lower (std::string s)
    {
        for (auto& c : s) c = (char) std::tolower ((unsigned char) c);
        return s;
    }
} // namespace

const char* commandName (Command c)
{
    for (const auto& cn : kCommandNames)
        if (cn.cmd == c) return cn.name;
    return "None";
}

Command commandFromName (const std::string& n)
{
    const auto l = lower (n);
    for (const auto& cn : kCommandNames)
        if (lower (cn.name) == l) return cn.cmd;
    return Command::None;
}

std::string describeKey (const KeyStroke& k)
{
    std::string s;
    if (k.mods & ModCtrl) s += "ctrl+";
    if (k.mods & ModAlt) s += "alt+";
    if (k.mods & ModShift) s += "shift+";
    for (const auto& kn : kKeyNames)
        if (kn.key == k.key) return s + kn.name;
    if (k.key > 0 && k.key < 128)
        s += (char) k.key;
    else
        s += "?";
    return s;
}

bool parseKey (const std::string& textIn, KeyStroke& out)
{
    auto text = lower (trim (textIn));
    out = {};
    // Peel modifiers. A trailing "+" alone (e.g. "ctrl++") is the plus key.
    for (;;)
    {
        bool found = false;
        for (auto [prefix, bit] : { std::pair<const char*, unsigned> { "ctrl+", ModCtrl },
                                    { "cmd+", ModCtrl },
                                    { "alt+", ModAlt },
                                    { "option+", ModAlt },
                                    { "shift+", ModShift } })
        {
            const std::string p (prefix);
            if (text.size() > p.size() && text.compare (0, p.size(), p) == 0)
            {
                out.mods |= bit;
                text = text.substr (p.size());
                found = true;
            }
        }
        if (! found) break;
    }
    if (text.empty()) return false;
    for (const auto& kn : kKeyNames)
        if (text == kn.name)
        {
            out.key = kn.key;
            return true;
        }
    if (text.size() == 1)
    {
        out.key = (unsigned char) text[0];
        return true;
    }
    return false;
}

// The default map follows the published Speedy Entry keyboard commands
// (non-MIDI entry: arrows place the pitch, number keys enter the note).
// Lines marked "(Staff Paper)" are additions for features Speedy Entry
// handled with other tools.
const char* KeyMap::defaultText()
{
    return R"(# Key map
# Format:  key = Command [argument]
# Modifiers: ctrl+ (Command on Mac), alt+ (Option on Mac), shift+
# Named keys: up down left right enter backspace delete insert tab esc
#             home end pageup pagedown space equals plus minus hash
# The defaults follow the published Speedy Entry keys for the Mac. Where a
# second key is listed it is an alias (Cmd instead of Option, laptop keys).

# --- durations: 1 = 64th, 2 = 32nd, 3 = 16th, 4 = eighth, 5 = quarter, 6 = half, 7 = whole, 8 = double whole
1 = EnterValue 1
2 = EnterValue 2
3 = EnterValue 3
4 = EnterValue 4
5 = EnterValue 5
6 = EnterValue 6
7 = EnterValue 7
8 = EnterValue 8
# shift + number inserts before the insertion bar
shift+1 = InsertValue 1
shift+2 = InsertValue 2
shift+3 = InsertValue 3
shift+4 = InsertValue 4
shift+5 = InsertValue 5
shift+6 = InsertValue 6
shift+7 = InsertValue 7
# option + shift + number enters a rest
alt+shift+1 = EnterRestValue 1
alt+shift+2 = EnterRestValue 2
alt+shift+3 = EnterRestValue 3
alt+shift+4 = EnterRestValue 4
alt+shift+5 = EnterRestValue 5
alt+shift+6 = EnterRestValue 6
alt+shift+7 = EnterRestValue 7
alt+shift+8 = EnterRestValue 8
ctrl+shift+1 = EnterRestValue 1
ctrl+shift+2 = EnterRestValue 2
ctrl+shift+3 = EnterRestValue 3
ctrl+shift+4 = EnterRestValue 4
ctrl+shift+5 = EnterRestValue 5
ctrl+shift+6 = EnterRestValue 6
ctrl+shift+7 = EnterRestValue 7
ctrl+shift+8 = EnterRestValue 8
. = Dot
# option + number starts a tuplet (3 = triplet, 5 = quintuplet ...); option+1 = any ratio
alt+1 = TupletDialog
alt+2 = StartTuplet 2
alt+3 = StartTuplet 3
alt+4 = StartTuplet 4
alt+5 = StartTuplet 5
alt+6 = StartTuplet 6
alt+7 = StartTuplet 7
ctrl+1 = TupletDialog
ctrl+2 = StartTuplet 2
ctrl+3 = StartTuplet 3
ctrl+4 = StartTuplet 4
ctrl+5 = StartTuplet 5
ctrl+6 = StartTuplet 6
ctrl+7 = StartTuplet 7

# --- chords, rests, deleting
enter = AddChordNote
shift+delete = RemoveChordNote
shift+backspace = RemoveChordNote
backspace = DeleteEntry
delete = ClearToRest

# --- accidentals and spelling
plus = RaiseHalfStep
minus = LowerHalfStep
alt+plus = RaiseHalfStepMeasure
alt+minus = LowerHalfStepMeasure
ctrl+plus = RaiseHalfStepMeasure
ctrl+minus = LowerHalfStepMeasure
9 = Enharmonic
alt+9 = EnharmonicMeasure
ctrl+9 = EnharmonicMeasure
* = ToggleAccidental
shift+8 = ToggleAccidental
alt+* = FreezeAccidental
alt+shift+* = FreezeAccidental
p = AccidentalParens

# --- ties, beams, stems, visibility, grace notes
equals = TieNext
t = TieNext
alt+equals = TiePrevious
shift+t = TiePrevious
ctrl+equals = TiePrevious
/ = BreakJoinBeam
b = BreakJoinBeam
shift+b = ResetBeams
\ = FlatBeam
m = FlatBeam
l = FreezeStem
alt+l = AutoStem
ctrl+l = AutoStem
o = HideEntry
h = HideEntry
; = GraceNote
` = SlashGraceNote

# --- layers (voices)
' = ToggleVoice
ctrl+alt+1 = SetLayer 1
ctrl+alt+2 = SetLayer 2
ctrl+alt+3 = SetLayer 3
ctrl+alt+4 = SetLayer 4

# --- articulations, slurs, dynamics (these change playback)
alt+s = Staccato
alt+t = Tenuto
alt+a = Accent
( = SlurStart
) = SlurEnd
alt+d = DynamicsMenu

# --- moving around
up = CursorUp
down = CursorDown
alt+up = OctaveUp
alt+down = OctaveDown
left = PrevEntry
right = NextEntry
[ = PrevMeasure
] = NextMeasure
shift+up = StaffUp
shift+down = StaffDown
ctrl+shift+up = MoveToStaffAbove
ctrl+shift+down = MoveToStaffBelow
insert = ToggleInsertMode
ctrl+i = ToggleInsertMode
shift+i = ToggleInsertMode
esc = ToggleFrame

# --- selecting, copy and paste
shift+right = SelectNext
shift+left = SelectPrev
ctrl+shift+m = SelectMeasure
ctrl+a = SelectAll
ctrl+c = Copy
ctrl+x = Cut
ctrl+v = Paste

# --- general
ctrl+z = Undo
ctrl+y = Redo
ctrl+shift+z = Redo
alt+m = MeasureMenu
ctrl+k = ShowCommands
ctrl+alt+left = PrevClip
ctrl+alt+right = NextClip
ctrl+shift+g = GrandStaff
ctrl+alt+shift+1 = SetClef 1
ctrl+alt+shift+2 = SetClef 2
ctrl+alt+shift+3 = SetClef 3
ctrl+alt+shift+4 = SetClef 4
)";
}

KeyMap KeyMap::defaults()
{
    KeyMap k;
    k.load (defaultText());
    return k;
}

std::vector<std::string> KeyMap::load (const std::string& text)
{
    std::vector<std::string> problems;
    std::istringstream in (text);
    std::string line;
    int lineNo = 0;
    while (std::getline (in, line))
    {
        ++lineNo;
        auto t = trim (line);
        if (t.empty() || t[0] == '#') continue;
        // split on the LAST " = " so that keys like "=" can't confuse us
        const auto eq = t.find (" = ");
        if (eq == std::string::npos)
        {
            problems.push_back ("line " + std::to_string (lineNo) + ": expected 'key = Command'");
            continue;
        }
        KeyStroke ks;
        if (! parseKey (t.substr (0, eq), ks))
        {
            problems.push_back ("line " + std::to_string (lineNo) + ": unknown key '" + trim (t.substr (0, eq)) + "'");
            continue;
        }
        std::istringstream rhs (t.substr (eq + 3));
        std::string cmdName;
        int arg = 0;
        rhs >> cmdName >> arg;
        const auto cmd = commandFromName (cmdName);
        if (cmd == Command::None && lower (cmdName) != "none")
        {
            problems.push_back ("line " + std::to_string (lineNo) + ": unknown command '" + cmdName + "'");
            continue;
        }
        map[ks] = { cmd, arg };
    }
    return problems;
}

std::string KeyMap::save() const
{
    std::string out = "# Staff Paper key map\n";
    for (const auto& [k, b] : map)
    {
        out += describeKey (k) + " = " + commandName (b.command);
        if (b.arg != 0) out += " " + std::to_string (b.arg);
        out += "\n";
    }
    return out;
}

Binding KeyMap::lookup (const KeyStroke& k) const
{
    auto it = map.find (k);
    return it == map.end() ? Binding {} : it->second;
}

} // namespace staffpaper
