#include "PluginEditor.h"

#include "Glyphs.h"

#include "staffpaper/Layout.h"
#include "staffpaper/MusicXml.h"

using namespace staffpaper;

namespace
{
const char* kGroups[][2] = {
    { "Entering notes", "EnterValue InsertValue EnterRestValue Dot StartTuplet TupletDialog AddChordNote RemoveChordNote ToggleRest DeleteEntry GraceNote SlashGraceNote ToggleInsertMode" },
    { "Pitch and accidentals", "RaiseHalfStep LowerHalfStep RaiseHalfStepMeasure LowerHalfStepMeasure Enharmonic EnharmonicMeasure ToggleAccidental FreezeAccidental AccidentalParens" },
    { "Ties, beams, stems", "TieNext TiePrevious BreakJoinBeam ResetBeams FlatBeam FreezeStem AutoStem HideEntry" },
    { "Layers (voices)", "ToggleVoice SetLayer" },
    { "Articulations, slurs, dynamics (these change playback)", "Staccato Tenuto Accent SlurStart SlurEnd DynamicsMenu SetDynamic" },
    { "Staves and clefs", "GrandStaff SetClef MoveToStaffAbove MoveToStaffBelow" },
    { "Moving around", "CursorUp CursorDown OctaveUp OctaveDown PrevEntry NextEntry PrevMeasure NextMeasure StaffUp StaffDown ToggleFrame PrevClip NextClip" },
    { "Selecting, copy and paste", "SelectNext SelectPrev SelectMeasure SelectAll Copy Cut Paste ClearSelection" },
    { "General", "Undo Redo MeasureMenu ShowCommands" },
};

juce::String friendlyKey (const KeyStroke& k)
{
    juce::String s;
#if JUCE_MAC
    if (k.mods & ModCtrl) s << juce::String (juce::CharPointer_UTF8 ("\xe2\x8c\x98"));   // ⌘
    if (k.mods & ModAlt) s << juce::String (juce::CharPointer_UTF8 ("\xe2\x8c\xa5"));    // ⌥
    if (k.mods & ModShift) s << juce::String (juce::CharPointer_UTF8 ("\xe2\x87\xa7"));  // ⇧
#else
    if (k.mods & ModCtrl) s << "Ctrl+";
    if (k.mods & ModAlt) s << "Alt+";
    if (k.mods & ModShift) s << "Shift+";
#endif
    KeyStroke bare { k.key, 0 };
    auto name = juce::String (describeKey (bare));
    if (name.length() > 1) name = name.substring (0, 1).toUpperCase() + name.substring (1);
    else name = name.toUpperCase();
    return s + name;
}

juce::String describeCommand (Command c, int arg)
{
    static const char* values[] = { "", "64th", "32nd", "16th", "eighth", "quarter", "half", "whole", "double whole" };
    switch (c)
    {
        case Command::EnterValue: return juce::String ("Enter a ") + values[juce::jlimit (1, 8, arg)];
        case Command::InsertValue: return juce::String ("Insert a ") + values[juce::jlimit (1, 8, arg)];
        case Command::EnterRestValue: return juce::String ("Enter a ") + values[juce::jlimit (1, 8, arg)] + " rest";
        case Command::StartTuplet: return "Start a tuplet of " + juce::String (arg);
        case Command::TupletDialog: return "Tuplet with any ratio...";
        case Command::SetLayer: return "Layer " + juce::String (arg);
        case Command::SetDynamic: return juce::String ("Dynamic ") + dynamicName ((Dynamic) juce::jlimit (0, 9, arg - 1));
        case Command::ToggleVoice: return "Switch between layer 1 and 2";
        case Command::ToggleFrame: return "Leave / re-enter the entry frame";
        case Command::DynamicsMenu: return "Dynamics (pppp to ffff)...";
        case Command::ShowCommands: return "Show this list";
        case Command::PrevClip: return "Previous clip";
        case Command::NextClip: return "Next clip";
        case Command::SelectNext: return "Extend selection right";
        case Command::SelectPrev: return "Extend selection left";
        case Command::ToggleInsertMode: return "Insert mode on / off";
        case Command::MoveToStaffAbove: return "Move the note to the staff above";
        case Command::MoveToStaffBelow: return "Move the note to the staff below";
        case Command::GrandStaff: return "Grand staff on / off (splits at middle C)";
        case Command::SetClef:
        {
            static const char* names[] = { "Treble", "Bass", "Alto", "Tenor" };
            return juce::String (names[juce::jlimit (1, 4, arg) - 1]) + " clef at the insertion bar";
        }
        default: break;
    }
    // split CamelCase
    juce::String n (commandName (c)), out;
    for (int i = 0; i < n.length(); ++i)
    {
        if (i > 0 && juce::CharacterFunctions::isUpperCase (n[i])) out << ' ';
        out << (i == 0 ? n.substring (0, 1) : n.substring (i, i + 1).toLowerCase());
    }
    return out;
}

// --------------------------------------------------------------------------- overlay panels

struct Panel : public juce::Component
{
    juce::TextButton close { "Close" };
    juce::String title;
    juce::Colour bg, fg;
    std::function<void()> onClose;

    Panel (const juce::String& t, juce::Colour b, juce::Colour f) : title (t), bg (b), fg (f)
    {
        addAndMakeVisible (close);
        close.onClick = [this] { if (onClose) onClose(); };
    }
    void paint (juce::Graphics& g) override
    {
        g.fillAll (bg);
        g.setColour (fg);
        g.setFont (juce::FontOptions (HouseLook::uiFontName(), 16.0f, juce::Font::bold));
        g.drawText (title, 16, 8, getWidth() - 140, 28, juce::Justification::centredLeft);
    }
    juce::Rectangle<int> content() const { return getLocalBounds().withTrimmedTop (44).reduced (16, 8); }
    void resized() override { close.setBounds (getWidth() - 96, 10, 80, 26); }
};

struct KeysPanel : public Panel
{
    juce::TextEditor text;
    juce::TextButton openFile { "Edit key map file" }, reload { "Reload key map" };

    KeysPanel (StaffPaperProcessor& p, juce::Colour b, juce::Colour f) : Panel ("Commands", b, f)
    {
        text.setMultiLine (true);
        text.setReadOnly (true);
        text.setCaretVisible (false);
        text.setFont (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), 14.0f, juce::Font::plain));
        text.setColour (juce::TextEditor::backgroundColourId, b);
        text.setColour (juce::TextEditor::textColourId, f);
        text.setColour (juce::TextEditor::outlineColourId, juce::Colours::transparentBlack);
        addAndMakeVisible (text);
        addAndMakeVisible (openFile);
        addAndMakeVisible (reload);
        fill (p);
        openFile.onClick = [&p] {
            auto f = p.keyMapFile();
            if (! f.existsAsFile())
            {
                f.getParentDirectory().createDirectory();
                f.replaceWithText (KeyMap::defaultText());
            }
            f.startAsProcess();
        };
        reload.onClick = [this, &p] {
            p.reloadKeyMap();
            fill (p);
        };
    }

    void fill (StaffPaperProcessor& p)
    {
        // gather keys per (command, arg)
        std::map<std::pair<int, int>, juce::StringArray> keys;
        const auto saved = p.keyMap.save();
        for (const auto& line : juce::StringArray::fromLines (juce::String (saved)))
        {
            const int eq = line.indexOf (" = ");
            if (eq < 0) continue;
            KeyStroke k;
            if (! parseKey (line.substring (0, eq).toStdString(), k)) continue;
            const auto b = p.keyMap.lookup (k);
            keys[{ (int) b.command, b.arg }].add (friendlyKey (k));
        }
        juce::String out;
        if (p.keyMapProblems.isNotEmpty()) out << "Problems in your key map file:\n" << p.keyMapProblems << "\n";
        for (auto& grp : kGroups)
        {
            out << grp[0] << "\n";
            const auto names = juce::StringArray::fromTokens (grp[1], " ", "");
            for (const auto& name : names)
            {
                const auto cmd = commandFromName (name.toStdString());
                for (auto& [key, list] : keys)
                    if (key.first == (int) cmd)
                        out << "  " << list.joinIntoString (" or ").paddedRight (' ', 22) << describeCommand (cmd, key.second) << "\n";
            }
            out << "\n";
        }
        out << "Mouse\n";
        out << "  Speedy tool: click a measure to put the insertion bar there and open the entry frame.\n";
        out << "  Select tool: drag over music to select it (snaps to the Grid), click a note, double-click a measure.\n";
        out << "  Velocity lane: drag a dot up or down (all selected notes move together); double-click resets it.\n";
        out << "  Right-click a measure (or " << friendlyKey ({ 'm', ModAlt }) << ") for meter, key, clefs, barlines, repeats, endings, dynamics.\n";
        out << "  Cmd + mouse wheel zooms. Space goes to Bitwig (play / stop).\n";
        text.setText (out, false);
    }

    void resized() override
    {
        Panel::resized();
        auto r = content();
        auto buttons = r.removeFromBottom (32);
        openFile.setBounds (buttons.removeFromLeft (170).reduced (0, 2));
        buttons.removeFromLeft (8);
        reload.setBounds (buttons.removeFromLeft (150).reduced (0, 2));
        text.setBounds (r.withTrimmedBottom (8));
    }
};

struct SettingsPanel : public Panel
{
    struct Row
    {
        std::unique_ptr<juce::Label> label;
        std::unique_ptr<juce::Slider> slider;
        bool dynamics = false;
    };
    std::vector<Row> rows;
    juce::ToggleButton allClips { "Apply to all clips" }, midiEntry { "Enter pitches from a MIDI keyboard" },
        midiThru { "Pass MIDI input through to the instrument" };
    juce::Label offsetLabel;
    juce::Slider offset;
    juce::TextButton classical { "Classical defaults" };
    StaffPaperProcessor& proc;
    std::function<void()> onChanged;
    std::function<bool()> getMidiEntry;
    std::function<void (bool)> setMidiEntry;

    SettingsPanel (StaffPaperProcessor& p, juce::Colour b, juce::Colour f) : Panel ("Playback settings", b, f), proc (p)
    {
        auto& pb = proc.project.current().score.playback;
        addRow ("Ordinary notes length (%)", 30, 100, pb.normalLength * 100, [] (PlaybackSettings& s, double v) { s.normalLength = v / 100; });
        addRow ("Staccato length (%)", 10, 90, pb.staccatoLength * 100, [] (PlaybackSettings& s, double v) { s.staccatoLength = v / 100; });
        addRow ("Staccato velocity change", -40, 20, pb.staccatoVelocity, [] (PlaybackSettings& s, double v) { s.staccatoVelocity = (int) v; });
        addRow ("Tenuto length (%)", 70, 110, pb.tenutoLength * 100, [] (PlaybackSettings& s, double v) { s.tenutoLength = v / 100; });
        addRow ("Slur overlap (%)", 0, 30, pb.legatoOverlap * 100, [] (PlaybackSettings& s, double v) { s.legatoOverlap = v / 100; });
        addRow ("Last note of a slur length (%)", 30, 100, pb.slurEndLength * 100, [] (PlaybackSettings& s, double v) { s.slurEndLength = v / 100; });
        addRow ("First note of a slur velocity change", -20, 30, pb.slurStartVelocity, [] (PlaybackSettings& s, double v) { s.slurStartVelocity = (int) v; });
        addRow ("Last note of a slur velocity change", -40, 10, pb.slurEndVelocity, [] (PlaybackSettings& s, double v) { s.slurEndVelocity = (int) v; });
        addRow ("Base velocity", 1, 127, pb.velocity, [] (PlaybackSettings& s, double v) { s.velocity = (int) v; });
        addRow ("Accent velocity boost", 0, 60, pb.accentBoost, [] (PlaybackSettings& s, double v) { s.accentBoost = (int) v; });
        for (int d = 0; d < (int) Dynamic::Count; ++d)
            addRow (juce::String (dynamicName ((Dynamic) d)) + " velocity", 1, 127, pb.dynamicVelocity[(size_t) d],
                    [d] (PlaybackSettings& s, double v) { s.dynamicVelocity[(size_t) d] = (int) v; }, true);

        for (auto* t : { &allClips, &midiEntry, &midiThru })
        {
            t->setColour (juce::ToggleButton::textColourId, f);
            t->setColour (juce::ToggleButton::tickColourId, f);
            t->setColour (juce::ToggleButton::tickDisabledColourId, f.withAlpha (0.5f));
            addAndMakeVisible (*t);
        }
        midiThru.setToggleState (proc.project.midiThru, juce::dontSendNotification);
        midiThru.onClick = [this] { proc.project.midiThru = midiThru.getToggleState(); changed(); };
        midiEntry.onClick = [this] { if (setMidiEntry) setMidiEntry (midiEntry.getToggleState()); };

        offsetLabel.setText ("Song clip starts at Bitwig bar", juce::dontSendNotification);
        offsetLabel.setColour (juce::Label::textColourId, f);
        offset.setRange (1, 999, 1);
        offset.setValue (proc.project.offsetBars + 1, juce::dontSendNotification);
        offset.setSliderStyle (juce::Slider::IncDecButtons);
        offset.setTextBoxStyle (juce::Slider::TextBoxLeft, false, 60, 24);
        offset.onValueChange = [this] { proc.project.offsetBars = offset.getValue() - 1; changed(); };
        addAndMakeVisible (offsetLabel);
        addAndMakeVisible (offset);

        classical.onClick = [this] {
            const PlaybackSettings d;
            std::vector<double> vals { d.normalLength * 100, d.staccatoLength * 100, (double) d.staccatoVelocity, d.tenutoLength * 100,
                                       d.legatoOverlap * 100, d.slurEndLength * 100, (double) d.slurStartVelocity, (double) d.slurEndVelocity,
                                       (double) d.velocity, (double) d.accentBoost };
            for (int v : d.dynamicVelocity) vals.push_back (v);
            for (size_t i = 0; i < rows.size() && i < vals.size(); ++i) rows[i].slider->setValue (vals[i], juce::sendNotificationSync);
        };
        addAndMakeVisible (classical);
    }

    void refreshToggles() { if (getMidiEntry) midiEntry.setToggleState (getMidiEntry(), juce::dontSendNotification); }

    void changed()
    {
        if (onChanged) onChanged();
    }

    void addRow (const juce::String& name, double lo, double hi, double value, std::function<void (PlaybackSettings&, double)> apply, bool dyn = false)
    {
        Row r;
        r.dynamics = dyn;
        r.label = std::make_unique<juce::Label> ("", name);
        r.label->setColour (juce::Label::textColourId, fg);
        r.slider = std::make_unique<juce::Slider> (juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight);
        r.slider->setRange (lo, hi, 1);
        r.slider->setValue (value, juce::dontSendNotification);
        r.slider->setTextBoxStyle (juce::Slider::TextBoxRight, false, 50, 22);
        auto* s = r.slider.get();
        s->onValueChange = [this, s, apply] {
            if (allClips.getToggleState())
                for (auto& c : proc.project.clips) apply (c.score.playback, s->getValue());
            else
                apply (proc.project.current().score.playback, s->getValue());
            changed();
        };
        addAndMakeVisible (*r.label);
        addAndMakeVisible (*r.slider);
        rows.push_back (std::move (r));
    }

    void resized() override
    {
        Panel::resized();
        auto r = content();
        auto left = r.removeFromLeft (juce::jmin (470, r.getWidth() * 4 / 10));
        r.removeFromLeft (20);
        auto middle = r.removeFromLeft (juce::jmin (300, r.getWidth() / 2));
        r.removeFromLeft (20);
        for (auto& row : rows)
        {
            auto& col = row.dynamics ? middle : left;
            auto line = col.removeFromTop (26);
            row.label->setBounds (line.removeFromLeft (row.dynamics ? 110 : 230));
            row.slider->setBounds (line);
        }
        left.removeFromTop (8);
        classical.setBounds (left.removeFromTop (28).removeFromLeft (170));
        for (auto* t : { &allClips, &midiEntry, &midiThru })
            t->setBounds (r.removeFromTop (28));
        r.removeFromTop (8);
        offsetLabel.setBounds (r.removeFromTop (24));
        offset.setBounds (r.removeFromTop (28).removeFromLeft (160));
    }
};
} // namespace

// --------------------------------------------------------------------------- editor

StaffPaperEditor::StaffPaperEditor (StaffPaperProcessor& p)
    : AudioProcessorEditor (p), proc (p), clips (p.project)
{
    setLookAndFeel (&look);
    setWantsKeyboardFocus (false);

    std::vector<juce::Button*> all { &newClip, &dupClip, &delClip, &loopClip, &undoButton, &redoButton, &hearButton, &exportButton,
                                     &dragButton, &commandsButton, &settingsButton, &zoomOut, &zoomIn, &themeButton, &colourButton,
                                     &tempoSync, &meterSync, &playButton, &stopButton, &rewindButton, &speedyTool, &selectTool,
                                     &insertButton, &clefButton, &grandStaffButton, &staffUpButton, &staffDownButton };
    for (int l = 0; l < kLayers; ++l) all.push_back (&layerButtons[l]);
    for (auto* b : all)
    {
        b->setWantsKeyboardFocus (false);
        b->setMouseClickGrabsKeyboardFocus (false);
        addAndMakeVisible (*b);
    }
    addAndMakeVisible (title);
    addAndMakeVisible (gridBox);
    addAndMakeVisible (tempoSlider);
    addAndMakeVisible (staff);
    addAndMakeVisible (clips);
    clips.toBack(); // the sync buttons sit on the right of the clip strip
    gridBox.setWantsKeyboardFocus (false);
    tempoSlider.setWantsKeyboardFocus (false);

    // header: the clip and what to do with it
    title.onClick = [this] { clips.startRename (proc.project.selectedClip); };
    newClip.setTooltip ("New clip");
    dupClip.setTooltip ("Duplicate this clip");
    delClip.setTooltip ("Delete this clip");
    loopClip.setTooltip ("Loop this clip when it is launched (off: it plays once)");
    loopClip.toggleLook = true;
    newClip.onClick = [this] { clips.addClip(); staff.grabKeyboardFocus(); };
    dupClip.onClick = [this] { clips.duplicateClip (proc.project.selectedClip); staff.grabKeyboardFocus(); };
    delClip.onClick = [this] {
        const int i = proc.project.selectedClip;
        if (proc.project.clips.size() <= 1) return;
        juce::Component::SafePointer<StaffPaperEditor> safe (this);
        juce::AlertWindow::showOkCancelBox (juce::MessageBoxIconType::NoIcon, "Delete clip",
                                            "Delete \"" + clips.displayName (i) + "\"? This can't be undone.", "Delete", "Cancel", this,
                                            juce::ModalCallbackFunction::create ([safe, i] (int r) {
                                                if (safe && r == 1) safe->clips.deleteClip (i);
                                            }));
    };
    loopClip.onClick = [this] { clips.toggleLoop (proc.project.selectedClip); refreshToolbar(); staff.grabKeyboardFocus(); };

    undoButton.setTooltip (tipFor ("Undo", Command::Undo));
    redoButton.setTooltip (tipFor ("Redo", Command::Redo));
    undoButton.onClick = [this] { runCommand (Command::Undo); };
    redoButton.onClick = [this] { runCommand (Command::Redo); };
    hearButton.setClickingTogglesState (true);
    hearButton.setTooltip ("Hear notes: play each note once, short, as you enter it");
    hearButton.onClick = [this] { proc.project.audition = hearButton.getToggleState(); edited(); staff.grabKeyboardFocus(); };
    exportButton.setTooltip ("Export this clip as MusicXML (.musicxml or compressed .mxl) for Finale, Dorico, Sibelius, MuseScore...");
    exportButton.onClick = [this] { exportMusicXml(); };
    dragButton.setTooltip ("Drag this onto a Bitwig track or clip slot to copy the clip as MIDI");
    dragButton.onStateChange = [this] {
        if (dragButton.isDown() && juce::Desktop::getInstance().getMainMouseSource().isDragging())
            startMidiDrag (proc.project.selectedClip);
    };
    commandsButton.setTooltip (tipFor ("Every command and its key", Command::ShowCommands));
    commandsButton.onClick = [this] { if (overlay) { overlay.reset(); staff.grabKeyboardFocus(); } else showCommandsWindow(); };
    settingsButton.setTooltip ("Playback settings");
    settingsButton.onClick = [this] { if (overlay) { overlay.reset(); staff.grabKeyboardFocus(); } else showSettings(); };
    zoomIn.setTooltip ("Zoom in (Cmd + mouse wheel)");
    zoomOut.setTooltip ("Zoom out");
    zoomIn.onClick = [this] { staff.setStaffSpace (staff.getStaffSpace() * 1.15f); proc.zoom = staff.getStaffSpace(); };
    zoomOut.onClick = [this] { staff.setStaffSpace (staff.getStaffSpace() / 1.15f); proc.zoom = staff.getStaffSpace(); };
    themeButton.onClick = [this] { proc.darkMode = ! proc.darkMode; applyTheme(); edited(); };
    colourButton.setTooltip ("Colour: Bitwig's track colour, or your own");
    colourButton.onClick = [this] { showColourMenu(); };
    zoomOut.setConnectedEdges (juce::Button::ConnectedOnRight);
    zoomIn.setConnectedEdges (juce::Button::ConnectedOnLeft);
    undoButton.setConnectedEdges (juce::Button::ConnectedOnRight);
    redoButton.setConnectedEdges (juce::Button::ConnectedOnLeft);

    // transport and sync
    playButton.setTooltip ("Play (own transport)");
    stopButton.setTooltip ("Stop (own transport)");
    rewindButton.setTooltip ("Back to the start");
    playButton.onClick = [this] { proc.ownPlay(); };
    stopButton.onClick = [this] { proc.ownStop(); };
    rewindButton.onClick = [this] { proc.ownRewind(); };
    rewindButton.setConnectedEdges (juce::Button::ConnectedOnRight);
    playButton.setConnectedEdges (juce::Button::ConnectedOnLeft | juce::Button::ConnectedOnRight);
    stopButton.setConnectedEdges (juce::Button::ConnectedOnLeft);
    tempoSlider.setSliderStyle (juce::Slider::LinearBar);
    tempoSlider.setRange (20, 300, 0.1);
    tempoSlider.setValue (proc.ownTempo.load(), juce::dontSendNotification);
    tempoSlider.setTextValueSuffix (" BPM");
    tempoSlider.setNumDecimalPlacesToDisplay (1);
    tempoSlider.setVelocityBasedMode (true);
    tempoSlider.setTooltip ("Tempo: drag, or double-click to type it (set by Bitwig while Tempo is synced)");
    tempoSlider.onValueChange = [this] { proc.ownTempo.store (tempoSlider.getValue()); };
    tempoSync.setClickingTogglesState (true);
    meterSync.setClickingTogglesState (true);
    tempoSync.setTooltip ("On: Bitwig's transport and tempo drive the playback. Off: use the play buttons and your own BPM.");
    meterSync.setTooltip ("On: clips take Bitwig's time signature. Off: each clip keeps its own.");
    tempoSync.onClick = [this] {
        proc.project.followHostTempo = tempoSync.getToggleState();
        if (proc.project.followHostTempo) proc.ownStop();
        edited();
        refreshToolbar();
    };
    meterSync.onClick = [this] {
        proc.project.followHostMeter = meterSync.getToggleState();
        edited();
        refreshToolbar();
    };

    // tools
    speedyTool.setRadioGroupId (55);
    selectTool.setRadioGroupId (55);
    speedyTool.setConnectedEdges (juce::Button::ConnectedOnRight);
    selectTool.setConnectedEdges (juce::Button::ConnectedOnLeft);
    speedyTool.setTooltip ("Speedy entry: click a measure and type (Esc leaves or re-enters the frame)");
    selectTool.setTooltip ("Select: drag over music, then Cmd+C / Cmd+X / Cmd+V");
    speedyTool.onClick = [this] {
        proc.tool = 0;
        staff.setTool (StaffView::Tool::Speedy);
        if (editor && ! editor->frameActive()) editor->setFrameActive (true);
        refreshToolbar();
        staff.grabKeyboardFocus();
    };
    selectTool.onClick = [this] {
        proc.tool = 1;
        staff.setTool (StaffView::Tool::Select);
        if (editor && editor->frameActive()) editor->setFrameActive (false);
        refreshToolbar();
        staff.grabKeyboardFocus();
    };
    insertButton.setClickingTogglesState (true);
    insertButton.setTooltip (tipFor ("Insert mode: new notes push the music after them along", Command::ToggleInsertMode));
    insertButton.onClick = [this] { if (editor) editor->setInsertMode (insertButton.getToggleState()); staff.grabKeyboardFocus(); };
    for (int l = 0; l < kLayers; ++l)
    {
        auto& b = layerButtons[l];
        b.setText (juce::String (l + 1));
        b.setRadioGroupId (77);
        b.setConnectedEdges ((l > 0 ? juce::Button::ConnectedOnLeft : 0) | (l + 1 < kLayers ? juce::Button::ConnectedOnRight : 0));
        b.setTooltip (tipFor ("Layer " + juce::String (l + 1) + (l < 2 ? " (' switches between 1 and 2)" : ""), Command::SetLayer, l + 1));
        b.onClick = [this, l] { runCommand (Command::SetLayer, l + 1); };
    }
    clefButton.setTooltip ("Clef at the insertion bar: treble, alto, tenor or bass");
    clefButton.onClick = [this] { showClefMenu(); };
    grandStaffButton.toggleLook = true;
    grandStaffButton.setTooltip (tipFor ("Grand staff: splits the music at middle C (middle C goes on the top staff). Off: back to one staff.",
                                         Command::GrandStaff));
    grandStaffButton.onClick = [this] { runCommand (Command::GrandStaff); };
    staffUpButton.setTooltip (tipFor ("Move the note (or the selected notes) to the staff above", Command::MoveToStaffAbove));
    staffDownButton.setTooltip (tipFor ("Move the note (or the selected notes) to the staff below", Command::MoveToStaffBelow));
    staffUpButton.onClick = [this] { runCommand (Command::MoveToStaffAbove); };
    staffDownButton.onClick = [this] { runCommand (Command::MoveToStaffBelow); };
    staffUpButton.setConnectedEdges (juce::Button::ConnectedOnRight);
    staffDownButton.setConnectedEdges (juce::Button::ConnectedOnLeft);

    for (int g = 0; g <= 10; ++g)
        gridBox.addItem ("Grid: " + StaffView::gridName (g), g + 1);
    gridBox.setTooltip ("What a drag with the Select tool snaps to");
    gridBox.onChange = [this] {
        proc.grid = gridBox.getSelectedId() - 1;
        staff.setGrid (proc.grid >= 10 ? 11 : proc.grid);
        staff.grabKeyboardFocus();
    };

    buildPalettes();

    staff.setKeyMap (&proc.keyMap);
    staff.onKey = [this] (const juce::String& k, const juce::String& c) {
        lastKey = k;
        lastCommand = c;
        refreshToolbar();
        repaint (getLocalBounds().removeFromBottom (24));
    };
    staff.onScoreEdited = [this] { edited(); };
    staff.onWindowCommand = [this] (Command c) {
        if (c == Command::ShowCommands) { if (overlay) { overlay.reset(); staff.grabKeyboardFocus(); } else showCommandsWindow(); }
        if (c == Command::PrevClip) selectClip (proc.project.selectedClip - 1);
        if (c == Command::NextClip) selectClip (proc.project.selectedClip + 1);
    };

    clips.onLaunch = [this] (int i) { proc.launchClip (i); };
    clips.onStop = [this] { proc.stopClips(); };
    clips.onSong = [this] { proc.returnToSong(); };
    clips.onSelect = [this] (int i) { selectClip (i); };
    clips.onProjectEdited = [this] { bindEditor(); edited(); };
    clips.onDragOut = [this] (int i, const juce::MouseEvent&) { startMidiDrag (i); };

    proc.onProjectReplaced = [this] {
        editorClip = -1;
        bindEditor();
        applyTheme();
        staff.setStaffSpace (proc.zoom);
        tempoSlider.setValue (proc.ownTempo.load(), juce::dontSendNotification);
        refreshToolbar();
        repaint();
    };
    proc.onTrackInfo = [this] { applyTheme(); };

    staff.setStaffSpace (proc.zoom);
    staff.setTool (proc.tool == 1 ? StaffView::Tool::Select : StaffView::Tool::Speedy);
    gridBox.setSelectedId (juce::jlimit (0, 10, proc.grid) + 1, juce::dontSendNotification);
    staff.setGrid (proc.grid >= 10 ? 11 : proc.grid);
    bindEditor();
    applyTheme();
    refreshToolbar();

    setResizable (true, true);
    setResizeLimits (900, 460, 4000, 2400);
    setSize (1280, 720);
    startTimerHz (30);
    staff.grabKeyboardFocus();
}

StaffPaperEditor::~StaffPaperEditor()
{
    proc.onProjectReplaced = nullptr;
    proc.onTrackInfo = nullptr;
    staff.setScore (nullptr, nullptr);
    setLookAndFeel (nullptr);
}

void StaffPaperEditor::bindEditor()
{
    auto& prj = proc.project;
    if (prj.clips.empty()) prj = Project::makeDefault();
    prj.selectedClip = juce::jlimit (0, (int) prj.clips.size() - 1, prj.selectedClip);
    const bool midiEntry = editor ? editor->midiEntry : false;
    const Cursor oldCursor = editor ? editor->cursor() : Cursor {};
    const bool sameClip = editorClip == prj.selectedClip && editor != nullptr;
    editor = std::make_unique<Editor> (prj.current().score);
    if (sameClip) editor->setCursor (oldCursor);
    if (proc.tool == 1) editor->setFrameActive (false);
    editorClip = prj.selectedClip;
    editor->midiEntry = midiEntry;
    editor->heldMidiNotes = [this] { return proc.heldNotes(); };
    editor->onChange = [this] { edited(); };
    editor->onAudition = [this] (const std::vector<int>& notes, int channel) {
        if (proc.project.audition) proc.audition (notes, channel);
    };
    staff.setScore (&prj.current().score, editor.get());
    clips.scrollToSelected();
}

void StaffPaperEditor::selectClip (int index)
{
    auto& prj = proc.project;
    if (index < 0 || index >= (int) prj.clips.size()) return;
    if (index != prj.selectedClip)
    {
        prj.selectedClip = index;
        bindEditor();
        edited();
    }
    clips.scrollToSelected();
    staff.grabKeyboardFocus();
}

void StaffPaperEditor::edited()
{
    proc.projectChanged();
    staff.repaint();
    clips.repaint();
    refreshToolbar();
}

juce::Colour StaffPaperEditor::accentColour() const
{
    const auto track = proc.trackColour.load();
    if (proc.project.useTrackColour && track != 0) return juce::Colour (track).withAlpha (1.0f);
    return juce::Colour (proc.project.manualColour).withAlpha (1.0f);
}

void StaffPaperEditor::applyTheme()
{
    const bool dark = proc.darkMode;
    const auto accent = accentColour();
    shownTrackColour = proc.trackColour.load();
    look.setPalette (dark ? HouseLook::Palette::dark (accent) : HouseLook::Palette::light (accent));
    staff.setTheme (dark ? StaffView::Theme::dark (accent) : StaffView::Theme::light (accent));
    const auto& p = look.palette();
    clips.setTheme ({ p.panel.darker (dark ? 0.12f : 0.03f), p.soft, p.ink, p.muted, p.line, accent, juce::Colour (0xff3ddc97) });
    themeButton.setIcon (dark ? Icon::Sun : Icon::Moon);
    themeButton.setTooltip (dark ? "Light mode" : "Dark mode");
    colourButton.setSwatch (accent);
    sendLookAndFeelChange();
    repaint();
}

void StaffPaperEditor::refreshToolbar()
{
    const bool own = ! proc.project.followHostTempo;
    tempoSync.setToggleState (! own, juce::dontSendNotification);
    tempoSync.setText (own ? "Tempo: own" : "Tempo: Bitwig");
    meterSync.setToggleState (proc.project.followHostMeter, juce::dontSendNotification);
    meterSync.setText (proc.project.followHostMeter ? "Meter: Bitwig" : "Meter: own");
    for (auto* b : { &playButton, &stopButton, &rewindButton })
        b->setVisible (own);
    tempoSlider.setEnabled (own);
    hearButton.setToggleState (proc.project.audition, juce::dontSendNotification);
    speedyTool.setToggleState (proc.tool == 0, juce::dontSendNotification);
    selectTool.setToggleState (proc.tool == 1, juce::dontSendNotification);
    gridBox.setEnabled (proc.tool == 1);

    auto& prj = proc.project;
    const int sel = prj.selectedClip;
    title.number = juce::String (sel + 1);
    title.name = clips.displayName (sel);
    title.colour = clips.clipColour (sel);
    title.ink = look.palette().ink;
    title.muted = look.palette().muted;
    title.repaint();
    loopClip.setToggleState (sel >= 0 && sel < (int) prj.clips.size() && prj.clips[(size_t) sel].loop && prj.songClip != sel,
                             juce::dontSendNotification);
    loopClip.setEnabled (prj.songClip != sel);
    delClip.setEnabled (prj.clips.size() > 1);

    if (editor)
    {
        insertButton.setToggleState (editor->insertMode(), juce::dontSendNotification);
        for (int l = 0; l < kLayers; ++l)
            layerButtons[l].setToggleState (editor->cursor().layer == l, juce::dontSendNotification);
        const auto& sc = prj.current().score;
        const auto& c = editor->cursor();
        const auto clef = sc.clefAtEntry (c.staff, c.measure, c.entry, c.layer);
        juce::juce_wchar g = Glyph::gClef;
        float gy = 2.0f;
        if (clef == Clef::Bass || clef == Clef::Bass8vb || clef == Clef::Bass8va || clef == Clef::Baritone) { g = Glyph::fClef; gy = -3.0f; }
        else if (clef == Clef::Alto || clef == Clef::Tenor || clef == Clef::Soprano || clef == Clef::MezzoSoprano) { g = Glyph::cClef; gy = 0.0f; }
        clefButton.setGlyph (juce::String::charToString (g), 18.0f, gy);
        clefButton.setText (juce::String (clefName (clef)) + juce::String (juce::CharPointer_UTF8 (" \xe2\x96\xbe")));
        grandStaffButton.setToggleState (sc.isGrandStaff(), juce::dontSendNotification);
        staffUpButton.setEnabled (sc.isGrandStaff());
        staffDownButton.setEnabled (sc.isGrandStaff());
        undoButton.setEnabled (editor->canUndo());
        redoButton.setEnabled (editor->canRedo());
    }
    resized();
}

// --------------------------------------------------------------------------- toolbar helpers

juce::String StaffPaperEditor::keysFor (Command c, int arg) const
{
    juce::StringArray out;
    for (const auto& line : juce::StringArray::fromLines (juce::String (proc.keyMap.save())))
    {
        const int eq = line.indexOf (" = ");
        if (eq < 0) continue;
        KeyStroke k;
        if (! parseKey (line.substring (0, eq).toStdString(), k)) continue;
        const auto b = proc.keyMap.lookup (k);
        if (b.command == c && b.arg == arg) out.add (friendlyKey (k));
    }
    return out.joinIntoString ("  or  ");
}

juce::String StaffPaperEditor::tipFor (const juce::String& what, Command c, int arg) const
{
    const auto keys = keysFor (c, arg);
    return keys.isEmpty() ? what : what + "    " + keys;
}

void StaffPaperEditor::runCommand (Command c, int arg)
{
    if (! editor) return;
    staff.runCommand (c, arg);
    staff.grabKeyboardFocus();
    refreshToolbar();
}

void StaffPaperEditor::ClipTitle::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    // a chip in the clip's colour with its number, then the name
    const float h = 24.0f;
    auto chip = juce::Rectangle<float> (r.getX(), r.getCentreY() - h * 0.5f, std::max (h, 14.0f + 8.0f * (float) number.length()), h);
    g.setColour (colour);
    g.fillRoundedRectangle (chip, 6.0f);
    g.setColour (colour.getPerceivedBrightness() > 0.6f ? juce::Colour (0xff171916) : juce::Colour (0xfff2f0e9));
    g.setFont (HouseLook::uiFont (13.0f));
    g.drawText (number, chip, juce::Justification::centred);
    g.setColour (ink);
    g.setFont (HouseLook::uiFont (17.0f));
    auto text = r.withTrimmedLeft (chip.getWidth() + 10.0f);
    g.drawFittedText (name, text.toNearestInt(), juce::Justification::centredLeft, 1, 0.8f);
    if (isMouseOver())
    {
        const float w = std::min (text.getWidth(), juce::GlyphArrangement::getStringWidth (HouseLook::uiFont (17.0f), name));
        g.setColour (muted);
        g.fillRect (text.getX(), text.getCentreY() + 11.0f, w, 1.0f);
    }
}

void StaffPaperEditor::showClefMenu()
{
    if (! editor) return;
    juce::PopupMenu m;
    m.addSectionHeader ("Clef at the insertion bar");
    static const char* names[] = { "Treble", "Bass", "Alto", "Tenor" };
    const auto cur = proc.project.current().score.clefAtEntry (editor->cursor().staff, editor->cursor().measure, editor->cursor().entry,
                                                               editor->cursor().layer);
    static const Clef clefs[] = { Clef::Treble, Clef::Bass, Clef::Alto, Clef::Tenor };
    for (int i : { 0, 2, 3, 1 }) // the usual order: treble, alto, tenor, bass
    {
        const auto keys = keysFor (Command::SetClef, i + 1);
        m.addItem (i + 1, juce::String (names[i]) + (keys.isNotEmpty() ? "      " + keys : juce::String()), true, cur == clefs[i]);
    }
    juce::Component::SafePointer<StaffPaperEditor> safe (this);
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&clefButton).withParentComponent (this), [safe] (int r) {
        if (safe && r > 0) safe->runCommand (Command::SetClef, r);
    });
}

void StaffPaperEditor::buildPalettes()
{
    using C = Command;
    auto u = [] (const char* utf8) { return juce::String (juce::CharPointer_UTF8 (utf8)); };
    auto glyph = [] (juce::juce_wchar c) { return juce::String::charToString (c); };
    // SMuFL single notes: double whole ... 64th (stem up)
    const juce::juce_wchar notes[] = { 0, 0xE1DD, 0xE1DB, 0xE1D9, 0xE1D7, 0xE1D5, 0xE1D3, 0xE1D2, 0xE1D0 };
    const juce::juce_wchar rests[] = { 0, Glyph::rest64th, Glyph::rest32nd, Glyph::rest16th, Glyph::rest8th, Glyph::restQuarter,
                                       Glyph::restHalf, Glyph::restWhole, Glyph::restDoubleWhole };
    static const char* valueNames[] = { "", "64th", "32nd", "16th", "Eighth", "Quarter", "Half", "Whole", "Double whole" };

    auto item = [] (C c, int arg, const juce::String& text, const juce::String& tip, Icon icon = Icon::None,
                    const juce::String& g = {}, float gs = 0, float gy = 0) {
        return PaletteItem { c, arg, text, g, gs, gy, icon, tip };
    };

    Palette notesP { "Notes", {} };
    for (int v = 8; v >= 1; --v)
        notesP.items.push_back (item (C::EnterValue, v, {}, juce::String (valueNames[v]) + " note", Icon::None, glyph (notes[v]), 22.0f, 3.0f));
    notesP.items.push_back (item (C::Dot, 0, {}, "Dot (add or remove)", Icon::None, glyph (Glyph::augmentationDot), 30.0f, 0.0f));
    notesP.items.push_back (item (C::StartTuplet, 3, {}, "Triplet", Icon::None, glyph (0xE883), 22.0f, 8.0f));
    notesP.items.push_back (item (C::TupletDialog, 0, "n:m", "Tuplet with any ratio"));
    notesP.items.push_back (item (C::AddChordNote, 0, u ("+ Chord"), "Add the crossbar's pitch to the chord"));
    notesP.items.push_back (item (C::RemoveChordNote, 0, u ("\xe2\x88\x92 Chord"), "Remove the crossbar's pitch from the chord"));
    notesP.items.push_back (item (C::GraceNote, 0, {}, "Grace note", Icon::None, glyph (0xE562), 22.0f, 3.0f));
    notesP.items.push_back (item (C::SlashGraceNote, 0, {}, "Slashed grace note", Icon::None, glyph (0xE560), 22.0f, 3.0f));
    notesP.items.push_back (item (C::DeleteEntry, 0, {}, "Delete the note or rest", Icon::Trash));
    palettes.push_back (notesP);

    Palette restsP { "Rests", {} };
    for (int v = 8; v >= 1; --v)
        restsP.items.push_back (item (C::EnterRestValue, v, {}, juce::String (valueNames[v]) + " rest", Icon::None, glyph (rests[v]), 22.0f, 0.0f));
    restsP.items.push_back (item (C::ToggleRest, 0, u ("Note \xe2\x86\x94 Rest"), "Turn a note into a rest, or back"));
    palettes.push_back (restsP);

    Palette pitchP { "Pitch", {} };
    pitchP.items.push_back (item (C::RaiseHalfStep, 0, u ("\xe2\x99\xaf  Up"), "Raise a half step"));
    pitchP.items.push_back (item (C::LowerHalfStep, 0, u ("\xe2\x99\xad  Down"), "Lower a half step"));
    pitchP.items.push_back (item (C::RaiseHalfStepMeasure, 0, u ("\xe2\x99\xaf  Measure"), "Raise the whole measure a half step"));
    pitchP.items.push_back (item (C::LowerHalfStepMeasure, 0, u ("\xe2\x99\xad  Measure"), "Lower the whole measure a half step"));
    pitchP.items.push_back (item (C::Enharmonic, 0, "Enharmonic", "Respell the note (e.g. C# to Db)"));
    pitchP.items.push_back (item (C::EnharmonicMeasure, 0, "Enh. measure", "Respell the rest of the measure"));
    pitchP.items.push_back (item (C::ToggleAccidental, 0, "Show / hide acc.", "Show a courtesy accidental, or hide it"));
    pitchP.items.push_back (item (C::AccidentalParens, 0, "( acc )", "Accidental in parentheses"));
    pitchP.items.push_back (item (C::FreezeAccidental, 0, "Freeze acc.", "Freeze the accidental"));
    pitchP.items.push_back (item (C::OctaveUp, 0, "8va up", "Crossbar up an octave"));
    pitchP.items.push_back (item (C::OctaveDown, 0, "8va down", "Crossbar down an octave"));
    palettes.push_back (pitchP);

    Palette tiesP { "Ties & beams", {} };
    tiesP.items.push_back (item (C::TieNext, 0, "Tie", "Tie to the next note"));
    tiesP.items.push_back (item (C::TiePrevious, 0, "Tie back", "Tie to the previous note"));
    tiesP.items.push_back (item (C::BreakJoinBeam, 0, "Break / join beam", "Break the beam before this note, or join it"));
    tiesP.items.push_back (item (C::ResetBeams, 0, "Reset beams", "Beam by the meter again"));
    tiesP.items.push_back (item (C::FlatBeam, 0, "Flat beam", "Flat beam"));
    tiesP.items.push_back (item (C::FreezeStem, 0, u ("Stem \xe2\x86\x95"), "Flip the stem up or down"));
    tiesP.items.push_back (item (C::AutoStem, 0, "Auto stem", "Stem direction back to automatic"));
    tiesP.items.push_back (item (C::HideEntry, 0, "Hide", "Hide or show the note or rest"));
    palettes.push_back (tiesP);

    Palette artP { "Articulations", {} };
    artP.items.push_back (item (C::Staccato, 0, {}, "Staccato (plays short)", Icon::None, glyph (Glyph::staccatoAbove), 26.0f, 6.0f));
    artP.items.push_back (item (C::Tenuto, 0, {}, "Tenuto (plays full length)", Icon::None, glyph (Glyph::tenutoAbove), 26.0f, 6.0f));
    artP.items.push_back (item (C::Accent, 0, {}, "Accent (plays louder)", Icon::None, glyph (Glyph::accentAbove), 26.0f, 6.0f));
    artP.items.push_back (item (C::SlurStart, 0, "Slur (", "Start a slur here (slurred notes connect)"));
    artP.items.push_back (item (C::SlurEnd, 0, ") Slur", "End the slur here (the last note is lifted)"));
    palettes.push_back (artP);

    Palette dynP { "Dynamics", {} };
    for (int d = 0; d < (int) Dynamic::Count; ++d)
        dynP.items.push_back (item (C::SetDynamic, d + 1, {}, juce::String (dynamicName ((Dynamic) d)) + " (a fixed velocity, see Settings)",
                                    Icon::None, glyph (Glyph::dynamics[d]), 24.0f, 1.0f));
    palettes.push_back (dynP);

    Palette selP { "Select", {} };
    selP.items.push_back (item (C::SelectPrev, 0, u ("\xe2\x86\x90 Extend"), "Extend the selection left"));
    selP.items.push_back (item (C::SelectNext, 0, u ("Extend \xe2\x86\x92"), "Extend the selection right"));
    selP.items.push_back (item (C::SelectMeasure, 0, "Measure", "Select the measure"));
    selP.items.push_back (item (C::SelectAll, 0, "All", "Select everything"));
    selP.items.push_back (item (C::Copy, 0, "Copy", "Copy", Icon::Copy));
    selP.items.push_back (item (C::Cut, 0, "Cut", "Cut", Icon::Cut));
    selP.items.push_back (item (C::Paste, 0, "Paste", "Paste at the insertion bar", Icon::Paste));
    selP.items.push_back (item (C::ClearSelection, 0, "Clear", "Clear the selected music (rests remain)", Icon::Erase));
    palettes.push_back (selP);

    Palette navP { "Move", {} };
    navP.items.push_back (item (C::ToggleFrame, 0, "Frame", "Leave the entry frame, or go back into the last measure"));
    navP.items.push_back (item (C::PrevEntry, 0, u ("\xe2\x86\x90 Note"), "Previous note"));
    navP.items.push_back (item (C::NextEntry, 0, u ("Note \xe2\x86\x92"), "Next note"));
    navP.items.push_back (item (C::PrevMeasure, 0, u ("\xe2\x86\x90 Measure"), "Previous measure"));
    navP.items.push_back (item (C::NextMeasure, 0, u ("Measure \xe2\x86\x92"), "Next measure"));
    navP.items.push_back (item (C::StaffUp, 0, u ("\xe2\x86\x91 Staff"), "Cursor to the staff above"));
    navP.items.push_back (item (C::StaffDown, 0, u ("\xe2\x86\x93 Staff"), "Cursor to the staff below"));
    navP.items.push_back (item (C::PrevClip, 0, u ("\xe2\x86\x90 Clip"), "Previous clip"));
    navP.items.push_back (item (C::NextClip, 0, u ("Clip \xe2\x86\x92"), "Next clip"));
    navP.items.push_back (item (C::MeasureMenu, 0, u ("Measure\xe2\x80\xa6"), "Meter, key, barlines, repeats, endings...", Icon::Menu));
    palettes.push_back (navP);

    for (int i = 0; i < (int) palettes.size(); ++i)
    {
        auto b = std::make_unique<IconButton> (Icon::None, palettes[(size_t) i].name);
        b->setRadioGroupId (88);
        b->setWantsKeyboardFocus (false);
        b->setMouseClickGrabsKeyboardFocus (false);
        b->setTooltip (palettes[(size_t) i].name + " palette");
        b->onClick = [this, i] { showPalette (i); };
        addAndMakeVisible (*b);
        paletteTabs.push_back (std::move (b));
    }
    showPalette (juce::jlimit (0, (int) palettes.size() - 1, openPalette));
}

void StaffPaperEditor::showPalette (int index)
{
    openPalette = index;
    for (int i = 0; i < (int) paletteTabs.size(); ++i)
        paletteTabs[(size_t) i]->setToggleState (i == index, juce::dontSendNotification);
    paletteButtons.clear();
    for (const auto& it : palettes[(size_t) index].items)
    {
        auto b = std::make_unique<IconButton> (it.icon, it.text);
        if (it.glyph.isNotEmpty()) b->setGlyph (it.glyph, it.glyphSize, it.glyphY);
        b->setWantsKeyboardFocus (false);
        b->setMouseClickGrabsKeyboardFocus (false);
        b->setTooltip (tipFor (it.tip, it.cmd, it.arg));
        const auto cmd = it.cmd;
        const int arg = it.arg;
        b->onClick = [this, cmd, arg] { runCommand (cmd, arg); };
        addAndMakeVisible (*b);
        paletteButtons.push_back (std::move (b));
    }
    resized();
    repaint();
    if (editor) staff.grabKeyboardFocus();
}

void StaffPaperEditor::showCommandsWindow()
{
    const auto& p = look.palette();
    auto panel = std::make_unique<KeysPanel> (proc, p.panel, p.ink);
    panel->onClose = [this] { overlay.reset(); staff.grabKeyboardFocus(); };
    overlay = std::move (panel);
    addAndMakeVisible (*overlay);
    resized();
}

void StaffPaperEditor::showSettings()
{
    const auto& p = look.palette();
    auto panel = std::make_unique<SettingsPanel> (proc, p.panel, p.ink);
    panel->onClose = [this] { overlay.reset(); staff.grabKeyboardFocus(); };
    panel->onChanged = [this] { edited(); };
    panel->getMidiEntry = [this] { return editor && editor->midiEntry; };
    panel->setMidiEntry = [this] (bool on) { if (editor) editor->midiEntry = on; };
    panel->refreshToggles();
    overlay = std::move (panel);
    addAndMakeVisible (*overlay);
    resized();
}

void StaffPaperEditor::showColourMenu()
{
    static const std::pair<const char*, juce::uint32> picks[] = {
        { "Lime (vii\xc2\xb0/V)", 0xffccff66 }, { "Blue", 0xff4067d6 }, { "Red", 0xffd9534f }, { "Orange", 0xffe0a23b },
        { "Green", 0xff4fbf7a }, { "Teal", 0xff3bb6c4 }, { "Purple", 0xff9b6ddf }, { "Pink", 0xffd96fa8 }, { "Grey", 0xff8a8f98 },
    };
    juce::PopupMenu m;
    const bool hasTrack = proc.trackColour.load() != 0;
    m.addItem (1, hasTrack ? "Follow the Bitwig track's colour" : "Follow the Bitwig track's colour (Bitwig has not sent one yet)",
               true, proc.project.useTrackColour);
    m.addSeparator();
    m.addSectionHeader ("Own colour");
    for (size_t i = 0; i < std::size (picks); ++i)
        m.addColouredItem (10 + (int) i, juce::String (juce::CharPointer_UTF8 (picks[i].first)), juce::Colour (picks[i].second), true,
                           ! proc.project.useTrackColour && proc.project.manualColour == picks[i].second);
    m.addItem (2, "Other colour...");
    juce::Component::SafePointer<StaffPaperEditor> safe (this);
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&colourButton).withParentComponent (this), [safe] (int r) {
        if (! safe || r <= 0) return;
        auto& prj = safe->proc.project;
        if (r == 1) prj.useTrackColour = ! prj.useTrackColour;
        else if (r == 2)
        {
            auto* sel = new juce::ColourSelector (juce::ColourSelector::showColourspace | juce::ColourSelector::showColourAtTop);
            sel->setCurrentColour (juce::Colour (prj.manualColour));
            sel->setSize (280, 260);
            struct Listener : juce::ChangeListener
            {
                juce::Component::SafePointer<StaffPaperEditor> ed;
                void changeListenerCallback (juce::ChangeBroadcaster* b) override
                {
                    if (! ed) return;
                    auto* cs = dynamic_cast<juce::ColourSelector*> (b);
                    ed->proc.project.manualColour = cs->getCurrentColour().withAlpha (1.0f).getARGB();
                    ed->proc.project.useTrackColour = false;
                    ed->applyTheme();
                    ed->edited();
                }
            };
            static Listener listener;
            listener.ed = safe;
            sel->addChangeListener (&listener);
            juce::CallOutBox::launchAsynchronously (std::unique_ptr<juce::Component> (sel), safe->colourButton.getScreenBounds(), nullptr);
            return;
        }
        else
        {
            prj.manualColour = picks[(size_t) (r - 10)].second;
            prj.useTrackColour = false;
        }
        safe->applyTheme();
        safe->edited();
    });
}

bool StaffPaperEditor::keyPressed (const juce::KeyPress& k)
{
    if (overlay && k == juce::KeyPress::escapeKey)
    {
        overlay.reset();
        staff.grabKeyboardFocus();
        return true;
    }
    // keys that reach the window itself go to the staff
    if (! overlay) return staff.keyPressed (k);
    return false;
}

namespace
{
constexpr int kHeaderH = 48, kLauncherH = 66, kToolbarH = 46, kPaletteH = 46, kStatusH = 24;
}

void StaffPaperEditor::paint (juce::Graphics& g)
{
    const auto& p = look.palette();
    const bool dark = look.isDark();
    g.fillAll (p.page);
    auto r = getLocalBounds();

    // header and toolbars share the panel colour, with hairlines between
    auto header = r.removeFromTop (kHeaderH);
    g.setGradientFill (juce::ColourGradient (p.panel.brighter (dark ? 0.04f : 0.0f), 0, 0, p.panel, 0, (float) kHeaderH, false));
    g.fillRect (header);
    r.removeFromTop (kLauncherH); // the clip strip paints itself
    auto bars = r.removeFromTop (kToolbarH + kPaletteH);
    g.setColour (p.panel);
    g.fillRect (bars);
    g.setColour (p.line);
    g.fillRect (header.removeFromBottom (1));
    g.setColour (p.line.withAlpha (0.6f));
    g.fillRect (bars.getX() + 8, bars.getY() + kToolbarH - 1, bars.getWidth() - 16, 1);
    g.setColour (p.line);
    g.fillRect (bars.removeFromBottom (1));
    // a soft shadow onto the staff paper
    g.setGradientFill (juce::ColourGradient (juce::Colours::black.withAlpha (dark ? 0.35f : 0.08f), 0, (float) bars.getBottom() + 1,
                                             juce::Colours::transparentBlack, 0, (float) bars.getBottom() + 7, false));
    g.fillRect (r.getX(), bars.getBottom() + 1, r.getWidth(), 6);

    // group separators and small captions
    g.setColour (p.line);
    for (int x : separators)
        if (x < 0)
            g.fillRect (-x, kHeaderH + kLauncherH + kToolbarH + 10, 1, kPaletteH - 20);
        else
            g.fillRect (x, kHeaderH + kLauncherH + 11, 1, kToolbarH - 22);
    if (layerButtons[0].isVisible())
    {
        g.setColour (p.muted);
        g.setFont (HouseLook::uiFont (10.5f));
        g.drawText ("LAYER", layerButtons[0].getX() - 44, layerButtons[0].getY(), 40, layerButtons[0].getHeight(), juce::Justification::centredRight);
    }

    // status bar
    auto bar = getLocalBounds().removeFromBottom (kStatusH);
    g.setColour (p.panel);
    g.fillRect (bar);
    g.setColour (p.line);
    g.fillRect (bar.removeFromTop (1));
    g.setColour (p.muted);
    g.setFont (HouseLook::uiFont (12.0f, false));
    juce::String status;
    if (lastKey.isNotEmpty())
        status << "Key: " << lastKey << "  " << (lastCommand.isNotEmpty() ? juce::String (juce::CharPointer_UTF8 ("\xe2\x86\x92 ")) + lastCommand : juce::String ("(not a command, passed to Bitwig)"));
    if (editor && ! editor->lastMessage().empty()) status << "    " << juce::String (editor->lastMessage());
    if (editor && editor->midiEntry) status << "    MIDI entry on";
    g.drawText (status, bar.reduced (10, 0), juce::Justification::centredLeft);
    juce::String right;
    if (proc.trackName.isNotEmpty()) right << proc.trackName << "    ";
    if (proc.project.followHostTempo)
        right << "Bitwig " << (proc.hostPlaying.load() ? "playing" : "stopped") << "  " << juce::String (proc.hostTempo.load(), 1) << " BPM";
    else
        right << "Own transport  " << (proc.ownPlaying.load() ? "playing" : "stopped");
    if (proc.hostHasMeter.load()) right << "  " << proc.hostNum.load() << "/" << proc.hostDen.load();
    g.drawText (right, bar.reduced (10, 0), juce::Justification::centredRight);
}

void StaffPaperEditor::resized()
{
    auto r = getLocalBounds();
    separators.clear();
    constexpr int bh = 30; // button height

    auto row = [bh] (juce::Rectangle<int> band) { return band.withSizeKeepingCentre (band.getWidth() - 20, bh); };
    auto placeL = [] (juce::Rectangle<int>& b, juce::Component& c, int w, int gap) {
        c.setBounds (b.removeFromLeft (w));
        b.removeFromLeft (gap);
    };
    auto placeR = [] (juce::Rectangle<int>& b, juce::Component& c, int w, int gap) {
        c.setBounds (b.removeFromRight (w));
        b.removeFromRight (gap);
    };
    auto fit = [] (IconButton& b) { return b.idealWidth(); };

    // header
    {
        auto b = row (r.removeFromTop (kHeaderH));
        placeR (b, colourButton, fit (colourButton), 6);
        placeR (b, themeButton, 32, 6);
        placeR (b, zoomIn, 30, 0);
        placeR (b, zoomOut, 30, 12);
        placeR (b, settingsButton, 34, 6);
        placeR (b, commandsButton, 34, 12);
        placeR (b, dragButton, fit (dragButton), 6);
        placeR (b, exportButton, fit (exportButton), 12);
        placeR (b, hearButton, fit (hearButton), 12);
        placeR (b, redoButton, 32, 0);
        placeR (b, undoButton, 32, 16);
        auto left = b;
        const int wanted = 44 + juce::roundToInt (juce::GlyphArrangement::getStringWidth (HouseLook::uiFont (17.0f), title.name)) + 12;
        const int titleW = juce::jlimit (120, std::max (120, left.getWidth() - 4 * 36 - 40), wanted);
        title.setBounds (left.removeFromLeft (titleW).withSizeKeepingCentre (titleW, 36));
        left.removeFromLeft (8);
        placeL (left, newClip, fit (newClip), 4);
        placeL (left, dupClip, 32, 4);
        placeL (left, loopClip, 32, 4);
        placeL (left, delClip, 32, 4);
        for (auto* c : std::initializer_list<juce::Component*> { &delClip, &loopClip, &dupClip, &newClip })
            if (c->getRight() > undoButton.getX() - 8) c->setBounds ({});
    }

    // launcher row: the clip strip, with sync and transport on the right
    {
        auto band = r.removeFromTop (kLauncherH);
        auto b = band.withSizeKeepingCentre (band.getWidth() - 20, bh);
        const int start = b.getRight();
        placeR (b, meterSync, fit (meterSync), 10);
        placeR (b, tempoSlider, 96, 6);
        if (! proc.project.followHostTempo)
        {
            placeR (b, stopButton, 32, 0);
            placeR (b, playButton, 32, 0);
            placeR (b, rewindButton, 32, 6);
        }
        placeR (b, tempoSync, fit (tempoSync), 0);
        clips.rightReserve = start - b.getRight() + 24;
        clips.setBounds (band);
    }

    // main toolbar: tools, layers, staves and clefs, grid
    {
        auto b = row (r.removeFromTop (kToolbarH));
        placeL (b, speedyTool, fit (speedyTool), 0);
        placeL (b, selectTool, fit (selectTool), 8);
        placeL (b, insertButton, fit (insertButton), 12);
        separators.push_back (b.getX() - 6);
        b.removeFromLeft (50); // "LAYER"
        for (int l = 0; l < kLayers; ++l) placeL (b, layerButtons[l], 30, 0);
        b.removeFromLeft (12);
        separators.push_back (b.getX() - 6);
        placeL (b, clefButton, std::max (96, fit (clefButton)), 6);
        placeL (b, grandStaffButton, fit (grandStaffButton), 6);
        placeL (b, staffUpButton, 34, 0);
        placeL (b, staffDownButton, 34, 12);
        separators.push_back (b.getX() - 6);
        placeL (b, gridBox, 136, 0);
        if (gridBox.getRight() > getWidth() - 10) gridBox.setBounds ({});
    }

    // palette row: tabs, then the open palette's buttons
    {
        auto b = row (r.removeFromTop (kPaletteH));
        for (auto& t : paletteTabs) placeL (b, *t, t->idealWidth() + 2, 0);
        for (size_t i = 0; i < paletteTabs.size(); ++i)
            paletteTabs[i]->setConnectedEdges ((i > 0 ? juce::Button::ConnectedOnLeft : 0)
                                               | (i + 1 < paletteTabs.size() ? juce::Button::ConnectedOnRight : 0));
        b.removeFromLeft (14);
        separators.push_back (-(b.getX() - 7));
        for (auto& pb : paletteButtons)
        {
            const int w = std::max (36, pb->idealWidth());
            if (w > b.getWidth()) { pb->setBounds ({}); continue; }
            placeL (b, *pb, w, 4);
        }
    }

    r.removeFromBottom (kStatusH);
    staff.setBounds (r);
    if (overlay) overlay->setBounds (r);
}

double StaffPaperEditor::writtenTickFromPerformance (int clip, double tick) const
{
    if (clip < 0 || clip >= (int) proc.project.clips.size() || tick < 0) return -1;
    const auto& s = proc.project.clips[(size_t) clip].score;
    double acc = 0;
    for (int mi : playbackOrder (s))
    {
        const double cap = (double) s.measureCapacity (mi);
        if (tick < acc + cap) return (double) s.measureStart (mi) + (tick - acc);
        acc += cap;
    }
    return -1;
}

void StaffPaperEditor::timerCallback()
{
    // follow Bitwig's meter (independently of the tempo sync)
    if (proc.project.followHostMeter && proc.hostHasMeter.load())
    {
        const TimeSig host { proc.hostNum.load(), proc.hostDen.load() };
        bool changed = false;
        for (auto& c : proc.project.clips)
            if (c.score.timeSig != host)
            {
                c.score.timeSig = host;
                changed = true;
            }
        if (changed) edited();
    }
    if (proc.trackColour.load() != shownTrackColour) applyTheme();
    if (proc.project.followHostTempo && std::abs (tempoSlider.getValue() - proc.hostTempo.load()) > 0.05)
        tempoSlider.setValue (proc.hostTempo.load(), juce::dontSendNotification);

    const int pc = proc.playheadClip.load();
    const double t = proc.playheadTick.load();
    staff.setPlayheadTick (pc == proc.project.selectedClip ? writtenTickFromPerformance (pc, t) : -1.0);
    double progress = 0;
    const int playingClip = proc.playingClip.load();
    if (playingClip >= 0 && playingClip < (int) proc.project.clips.size() && pc == playingClip && t >= 0)
    {
        const auto& sc = proc.project.clips[(size_t) playingClip].score;
        double len = 0;
        for (int mi : playbackOrder (sc)) len += (double) sc.measureCapacity (mi);
        progress = len > 0 ? t / len : 0;
    }
    clips.setStatus (playingClip, proc.queuedClip.load(), proc.hostPlaying.load(), progress);
    if (proc.queuedClip.load() >= 0) clips.repaint(); // blink

    static int tick = 0;
    if (++tick % 10 == 0) repaint (getLocalBounds().removeFromBottom (kStatusH));
}

// --------------------------------------------------------------------------- export

void StaffPaperEditor::exportMusicXml()
{
    const auto& clip = proc.project.current();
    auto name = juce::File::createLegalFileName (juce::String (clip.name));
    if (name.isEmpty()) name = "Clip";
    const auto start = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile (name + ".musicxml");
    chooser = std::make_unique<juce::FileChooser> ("Export MusicXML", start, "*.musicxml;*.mxl;*.xml");
    juce::Component::SafePointer<StaffPaperEditor> safe (this);
    chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles
                              | juce::FileBrowserComponent::warnAboutOverwriting,
                          [safe] (const juce::FileChooser& fc) {
                              if (! safe) return;
                              auto f = fc.getResult();
                              if (f == juce::File()) return;
                              if (! f.hasFileExtension ("musicxml;mxl;xml")) f = f.withFileExtension ("musicxml");
                              const auto& c = safe->proc.project.current();
                              const auto xml = toMusicXml (c.score, c.name, PLUGIN_DISPLAY_NAME);
                              bool ok = false;
                              if (f.hasFileExtension ("mxl"))
                              {
                                  // compressed MusicXML: a zip with the score and a container file
                                  juce::ZipFile::Builder zip;
                                  const auto now = juce::Time::getCurrentTime();
                                  zip.addEntry (new juce::MemoryInputStream (juce::MemoryBlock ("application/vnd.recordare.musicxml", 34), false), 0, "mimetype", now);
                                  zip.addEntry (new juce::MemoryInputStream (juce::MemoryBlock (musicXmlContainer ("score.musicxml").data(), musicXmlContainer ("score.musicxml").size()), false),
                                                9, "META-INF/container.xml", now);
                                  zip.addEntry (new juce::MemoryInputStream (juce::MemoryBlock (xml.data(), xml.size()), false), 9, "score.musicxml", now);
                                  f.deleteFile();
                                  juce::FileOutputStream os (f);
                                  ok = os.openedOk() && zip.writeToStream (os, nullptr);
                              }
                              else
                                  ok = f.replaceWithText (juce::String::fromUTF8 (xml.data(), (int) xml.size()));
                              safe->lastKey.clear();
                              safe->lastCommand.clear();
                              safe->repaint();
                              juce::AlertWindow::showMessageBoxAsync (ok ? juce::MessageBoxIconType::NoIcon : juce::MessageBoxIconType::WarningIcon,
                                                                     ok ? "Exported" : "Export failed",
                                                                     ok ? "Saved " + f.getFullPathName() : "Could not write " + f.getFullPathName());
                          });
}

// --------------------------------------------------------------------------- MIDI drag out

juce::File StaffPaperEditor::writeMidiFile (int clipIndex)
{
    const auto& clip = proc.project.clips[(size_t) clipIndex];
    const auto seq = buildSequence (clip.score);
    juce::MidiMessageSequence track;
    const auto ts = clip.score.timeSig;
    track.addEvent (juce::MidiMessage::timeSignatureMetaEvent (ts.num, ts.den), 0);
    track.addEvent (juce::MidiMessage::tempoMetaEvent ((int) (60000000.0 / std::max (1.0, proc.hostTempo.load()))), 0);
    for (const auto& e : seq.events)
    {
        track.addEvent (juce::MidiMessage::noteOn (e.channel, e.note, (juce::uint8) e.velocity), (double) e.start);
        track.addEvent (juce::MidiMessage::noteOff (e.channel, e.note), (double) (e.start + e.length));
    }
    track.addEvent (juce::MidiMessage::endOfTrack(), (double) seq.length);
    track.updateMatchedPairs();
    juce::MidiFile mf;
    mf.setTicksPerQuarterNote ((int) PPQ);
    mf.addTrack (track);

    auto name = juce::File::createLegalFileName (juce::String (clip.name));
    if (name.isEmpty()) name = "Clip";
    auto f = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile (PLUGIN_DISPLAY_NAME).getChildFile (name + ".mid");
    f.getParentDirectory().createDirectory();
    f.deleteFile();
    juce::FileOutputStream os (f);
    if (os.openedOk()) mf.writeTo (os);
    return f;
}

void StaffPaperEditor::startMidiDrag (int clipIndex)
{
    if (clipIndex < 0 || clipIndex >= (int) proc.project.clips.size()) return;
    const auto f = writeMidiFile (clipIndex);
    juce::DragAndDropContainer::performExternalDragDropOfFiles ({ f.getFullPathName() }, false, this);
}
