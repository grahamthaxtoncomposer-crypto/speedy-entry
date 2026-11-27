#include "PluginEditor.h"

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

    for (auto* b : { &speedyTool, &selectTool, &insertButton, &tempoSync, &meterSync, &playButton, &stopButton, &rewindButton,
                     &hearButton, &exportButton, &dragButton, &commandsButton, &settingsButton, &zoomOut, &zoomIn, &themeButton, &colourButton })
        addAndMakeVisible (*b);
    for (int l = 0; l < kLayers; ++l)
    {
        auto& b = layerButtons[l];
        b.setButtonText (juce::String (l + 1));
        b.setRadioGroupId (77);
        b.setTooltip ("Layer " + juce::String (l + 1) + (l == 0 ? "" : " (Cmd+Option+" + juce::String (l + 1) + ")"));
        b.onClick = [this, l] { if (editor) { editor->perform (Command::SetLayer, l + 1); staff.grabKeyboardFocus(); } };
        addAndMakeVisible (b);
    }
    addAndMakeVisible (gridBox);
    addChildComponent (tempoSlider);
    addAndMakeVisible (staff);
    addAndMakeVisible (clips);
    clips.toBack(); // the window's buttons sit on the right of the clip strip

    // tools
    speedyTool.setRadioGroupId (55);
    selectTool.setRadioGroupId (55);
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
    insertButton.setTooltip ("Insert mode: new notes push the music after them along (Shift+I)");
    insertButton.onClick = [this] { if (editor) editor->setInsertMode (insertButton.getToggleState()); staff.grabKeyboardFocus(); };

    for (int g = 0; g <= 10; ++g)
        gridBox.addItem ("Grid: " + StaffView::gridName (g), g + 1);
    gridBox.setTooltip ("What a drag with the Select tool snaps to");
    gridBox.onChange = [this] {
        proc.grid = gridBox.getSelectedId() - 1;
        staff.setGrid (proc.grid >= 10 ? 11 : proc.grid);
        staff.grabKeyboardFocus();
    };

    // transport and sync
    playButton.setButtonText (juce::String (juce::CharPointer_UTF8 ("\xe2\x96\xb6")));
    stopButton.setButtonText (juce::String (juce::CharPointer_UTF8 ("\xe2\x96\xa0")));
    rewindButton.setButtonText (juce::String (juce::CharPointer_UTF8 ("\xe2\x8f\xae")));
    playButton.onClick = [this] { proc.ownPlay(); };
    stopButton.onClick = [this] { proc.ownStop(); };
    rewindButton.onClick = [this] { proc.ownRewind(); };
    tempoSlider.setSliderStyle (juce::Slider::LinearBar);
    tempoSlider.setRange (20, 300, 0.1);
    tempoSlider.setValue (proc.ownTempo.load(), juce::dontSendNotification);
    tempoSlider.setTextValueSuffix (" BPM");
    tempoSlider.setNumDecimalPlacesToDisplay (1);
    tempoSlider.setVelocityBasedMode (true);
    tempoSlider.setTooltip ("Drag or double-click to type the tempo");
    tempoSlider.onValueChange = [this] { proc.ownTempo.store (tempoSlider.getValue()); };
    tempoSlider.setVisible (true);

    tempoSync.setClickingTogglesState (true);
    meterSync.setClickingTogglesState (true);
    tempoSync.setTooltip ("On: Bitwig's transport and tempo drive the playback. Off: use the play buttons and your own BPM.");
    meterSync.setTooltip ("On: clips take Bitwig's time signature");
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

    hearButton.setClickingTogglesState (true);
    hearButton.setTooltip ("Play each note once, short, as you enter it");
    hearButton.onClick = [this] { proc.project.audition = hearButton.getToggleState(); edited(); staff.grabKeyboardFocus(); };

    exportButton.setTooltip ("Save this clip as MusicXML (.musicxml or compressed .mxl) for Finale, Dorico, Sibelius, MuseScore...");
    exportButton.onClick = [this] { exportMusicXml(); };
    dragButton.setTooltip ("Drag this onto a Bitwig track or clip slot to copy the clip as MIDI");
    dragButton.addMouseListener (this, false);
    dragButton.onStateChange = [this] {
        if (dragButton.isDown() && juce::Desktop::getInstance().getMainMouseSource().isDragging())
            startMidiDrag (proc.project.selectedClip);
    };
    commandsButton.setTooltip ("Every key command (Cmd+K)");
    commandsButton.onClick = [this] { showCommandsWindow(); };
    settingsButton.onClick = [this] { showSettings(); };
    zoomIn.onClick = [this] { staff.setStaffSpace (staff.getStaffSpace() * 1.15f); proc.zoom = staff.getStaffSpace(); };
    zoomOut.onClick = [this] { staff.setStaffSpace (staff.getStaffSpace() / 1.15f); proc.zoom = staff.getStaffSpace(); };
    themeButton.onClick = [this] { proc.darkMode = ! proc.darkMode; applyTheme(); edited(); };
    colourButton.setTooltip ("Colour: Bitwig's track colour, or your own");
    colourButton.onClick = [this] { showColourMenu(); };

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
    setResizeLimits (760, 380, 4000, 2400);
    setSize (1280, 600);
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
    clips.setTheme ({ p.panel, p.soft, p.ink, p.muted, p.line, accent, juce::Colour (0xff3ddc97) });
    themeButton.setButtonText (dark ? "Light" : "Dark");
    sendLookAndFeelChange();
    repaint();
}

void StaffPaperEditor::refreshToolbar()
{
    const bool own = ! proc.project.followHostTempo;
    tempoSync.setToggleState (! own, juce::dontSendNotification);
    tempoSync.setButtonText (own ? "Tempo: own" : "Tempo: Bitwig");
    meterSync.setToggleState (proc.project.followHostMeter, juce::dontSendNotification);
    meterSync.setButtonText (proc.project.followHostMeter ? "Meter: Bitwig" : "Meter: own");
    for (auto* b : { &playButton, &stopButton, &rewindButton })
        b->setVisible (own);
    tempoSlider.setEnabled (own);
    hearButton.setToggleState (proc.project.audition, juce::dontSendNotification);
    speedyTool.setToggleState (proc.tool == 0, juce::dontSendNotification);
    selectTool.setToggleState (proc.tool == 1, juce::dontSendNotification);
    if (editor)
    {
        insertButton.setToggleState (editor->insertMode(), juce::dontSendNotification);
        for (int l = 0; l < kLayers; ++l)
            layerButtons[l].setToggleState (editor->cursor().layer == l, juce::dontSendNotification);
    }
    colourButton.setButtonText (proc.project.useTrackColour ? "Colour: track" : "Colour: own");
    resized();
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

void StaffPaperEditor::paint (juce::Graphics& g)
{
    const auto& p = look.palette();
    g.fillAll (p.page);
    auto bar = getLocalBounds().removeFromBottom (24);
    g.setColour (p.panel);
    g.fillRect (bar);
    g.setColour (p.line);
    g.fillRect (bar.removeFromTop (1));
    // toolbar background
    auto tb = getLocalBounds().withTrimmedTop (36).removeFromTop (44);
    g.setColour (p.panel);
    g.fillRect (tb);
    g.setColour (p.line);
    g.fillRect (tb.removeFromBottom (1));

    g.setColour (p.muted);
    g.setFont (juce::FontOptions (HouseLook::uiFontName(), 12.0f, juce::Font::plain));
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

    // swatch on the colour button
    if (! colourButton.getBounds().isEmpty())
    {
        g.setColour (accentColour());
        g.fillEllipse (colourButton.getBounds().toFloat().withTrimmedLeft (colourButton.getWidth() - 16.0f).withSizeKeepingCentre (8, 8).translated (-4, 0));
    }
}

void StaffPaperEditor::resized()
{
    auto r = getLocalBounds();
    // row 1: clips, then the window's own buttons on the right
    auto row1 = r.removeFromTop (36);
    clips.rightReserve = 8 + 48 + 4 + 26 + 2 + 26 + 6 + 70 + 4 + 82 + 4 + 110 + 4;
    clips.setBounds (row1);
    {
        auto b = row1.reduced (0, 5).withTrimmedRight (8);
        auto placeR = [&b] (juce::Component& c, int w, int gap = 4) {
            c.setBounds (b.removeFromRight (w));
            b.removeFromRight (gap);
        };
        placeR (themeButton, 48);
        placeR (zoomIn, 26, 2);
        placeR (zoomOut, 26, 6);
        placeR (settingsButton, 70);
        placeR (commandsButton, 82);
        placeR (colourButton, 110);
    }

    // row 2: entry tools, sync, hearing, export
    auto top = r.removeFromTop (44).reduced (8, 7);
    r.removeFromBottom (24);
    auto place = [&top] (juce::Component& c, int w, int gap = 4) {
        c.setBounds (top.removeFromLeft (w));
        top.removeFromLeft (gap);
    };
    place (speedyTool, 64, 2);
    place (selectTool, 58, 8);
    place (insertButton, 56, 8);
    for (int l = 0; l < kLayers; ++l) place (layerButtons[l], 24, l + 1 == kLayers ? 8 : 2);
    place (gridBox, 128, 14);
    place (tempoSync, 98, 4);
    if (! proc.project.followHostTempo)
    {
        place (rewindButton, 28, 2);
        place (playButton, 28, 2);
        place (stopButton, 28, 4);
    }
    place (tempoSlider, 92, 4);
    place (meterSync, 98, 14);
    place (hearButton, 84, 4);

    auto placeR = [&top] (juce::Component& c, int w, int gap = 4) {
        c.setBounds (top.removeFromRight (w));
        top.removeFromRight (gap);
    };
    placeR (dragButton, 80);
    placeR (exportButton, 120);
    // on a narrow window these go first
    for (auto* c : std::initializer_list<juce::Component*> { &dragButton, &exportButton })
        if (c->getX() < hearButton.getRight() + 4) c->setBounds ({});

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
    if (++tick % 10 == 0) repaint (getLocalBounds().removeFromBottom (24));
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
