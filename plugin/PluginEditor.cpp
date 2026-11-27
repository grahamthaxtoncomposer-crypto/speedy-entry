#include "PluginEditor.h"

#include "staffpaper/Layout.h"

using namespace staffpaper;

namespace
{
const char* kGroups[][2] = {
    { "Entering notes", "EnterValue InsertValue EnterRestValue Dot StartTuplet TupletDialog AddChordNote RemoveChordNote ToggleRest DeleteEntry GraceNote" },
    { "Pitch and accidentals", "RaiseHalfStep LowerHalfStep RaiseHalfStepMeasure LowerHalfStepMeasure Enharmonic EnharmonicMeasure ToggleAccidental AccidentalParens" },
    { "Ties, beams, stems", "TieNext TiePrevious BreakJoinBeam ResetBeams FlatBeam FreezeStem AutoStem HideEntry" },
    { "Articulations and slurs (these change playback)", "Staccato Tenuto Accent SlurStart SlurEnd" },
    { "Moving around", "CursorUp CursorDown OctaveUp OctaveDown PrevEntry NextEntry PrevMeasure NextMeasure StaffUp StaffDown ToggleInsertMode" },
    { "General", "Undo Redo MeasureMenu" },
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
        g.setFont (juce::FontOptions (16.0f, juce::Font::bold));
        g.drawText (title, 16, 8, getWidth() - 140, 28, juce::Justification::centredLeft);
    }
    juce::Rectangle<int> content() const { return getLocalBounds().withTrimmedTop (44).reduced (16, 8); }
    void resized() override { close.setBounds (getWidth() - 96, 10, 80, 26); }
};

struct KeysPanel : public Panel
{
    juce::TextEditor text;
    juce::TextButton openFile { "Edit key map file" }, reload { "Reload key map" };

    KeysPanel (StaffPaperProcessor& p, juce::Colour b, juce::Colour f) : Panel ("Keyboard commands", b, f)
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
        out << "Right-click a measure (or press " << friendlyKey ({ 'm', ModAlt }) << ") for time and key signatures, clefs, barlines, repeats and endings.\n";
        out << "Cmd + mouse wheel zooms. Space is passed to Bitwig (play / stop).\n";
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
    };
    std::vector<Row> rows;
    juce::ToggleButton allClips { "Apply to all clips" }, midiEntry { "Enter pitches from a MIDI keyboard" },
        midiThru { "Pass MIDI input through to the instrument" }, followMeter { "Follow Bitwig's time signature" };
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

        for (auto* t : { &allClips, &midiEntry, &midiThru, &followMeter })
        {
            t->setColour (juce::ToggleButton::textColourId, f);
            t->setColour (juce::ToggleButton::tickColourId, f);
            t->setColour (juce::ToggleButton::tickDisabledColourId, f.withAlpha (0.5f));
            addAndMakeVisible (*t);
        }
        midiThru.setToggleState (proc.project.midiThru, juce::dontSendNotification);
        followMeter.setToggleState (proc.project.followHostMeter, juce::dontSendNotification);
        midiThru.onClick = [this] { proc.project.midiThru = midiThru.getToggleState(); changed(); };
        followMeter.onClick = [this] { proc.project.followHostMeter = followMeter.getToggleState(); changed(); };
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
            const double vals[] = { d.normalLength * 100, d.staccatoLength * 100, (double) d.staccatoVelocity, d.tenutoLength * 100,
                                    d.legatoOverlap * 100, d.slurEndLength * 100, (double) d.slurStartVelocity, (double) d.slurEndVelocity,
                                    (double) d.velocity, (double) d.accentBoost };
            for (size_t i = 0; i < rows.size(); ++i) rows[i].slider->setValue (vals[i], juce::sendNotificationSync);
        };
        addAndMakeVisible (classical);
    }

    void refreshToggles() { if (getMidiEntry) midiEntry.setToggleState (getMidiEntry(), juce::dontSendNotification); }

    void changed()
    {
        if (onChanged) onChanged();
    }

    void addRow (const juce::String& name, double lo, double hi, double value, std::function<void (PlaybackSettings&, double)> apply)
    {
        Row r;
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
        auto left = r.removeFromLeft (juce::jmin (520, r.getWidth() * 3 / 5));
        for (auto& row : rows)
        {
            auto line = left.removeFromTop (28);
            row.label->setBounds (line.removeFromLeft (250));
            row.slider->setBounds (line);
        }
        left.removeFromTop (8);
        classical.setBounds (left.removeFromTop (28).removeFromLeft (170));
        r.removeFromLeft (24);
        for (auto* t : { &allClips, &midiEntry, &midiThru, &followMeter })
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
    setLookAndFeel (&lnf);
    setWantsKeyboardFocus (false);

    for (auto* b : { &notationTab, &clipsTab, &playButton, &stopButton, &rewindButton, &viewButton, &zoomOut, &zoomIn,
                     &themeButton, &keysButton, &settingsButton, &dragButton })
        addAndMakeVisible (*b);
    addAndMakeVisible (clipName);
    addAndMakeVisible (syncToggle);
    addChildComponent (tempoSlider);
    addAndMakeVisible (staff);
    clipViewport.setViewedComponent (&clips, false);
    clipViewport.setScrollBarsShown (true, false);
    addChildComponent (clipViewport);

    playButton.setButtonText (juce::String (juce::CharPointer_UTF8 ("\xe2\x96\xb6")));
    stopButton.setButtonText (juce::String (juce::CharPointer_UTF8 ("\xe2\x96\xa0")));
    rewindButton.setButtonText (juce::String (juce::CharPointer_UTF8 ("\xe2\x8f\xae")));
    playButton.onClick = [this] { proc.ownPlay(); };
    stopButton.onClick = [this] { proc.ownStop(); };
    rewindButton.onClick = [this] { proc.ownRewind(); };
    tempoSlider.setRange (20, 300, 0.1);
    tempoSlider.setValue (proc.ownTempo.load(), juce::dontSendNotification);
    tempoSlider.setSliderStyle (juce::Slider::IncDecButtons);
    tempoSlider.setTextBoxStyle (juce::Slider::TextBoxLeft, false, 56, 24);
    tempoSlider.setTextValueSuffix (" bpm");
    tempoSlider.onValueChange = [this] { proc.ownTempo.store (tempoSlider.getValue()); };

    syncToggle.setToggleState (proc.project.followHostTempo, juce::dontSendNotification);
    syncToggle.onClick = [this] {
        proc.project.followHostTempo = syncToggle.getToggleState();
        if (proc.project.followHostTempo) proc.ownStop();
        edited();
        updateTransportButtons();
    };

    notationTab.onClick = [this] { showNotation(); };
    clipsTab.onClick = [this] { showClips(); };
    viewButton.onClick = [this] {
        proc.pageView = ! proc.pageView;
        staff.setPageView (proc.pageView);
        viewButton.setButtonText (proc.pageView ? "Page view" : "Scroll view");
        edited();
    };
    zoomIn.onClick = [this] { staff.setStaffSpace (staff.getStaffSpace() * 1.15f); proc.zoom = staff.getStaffSpace(); };
    zoomOut.onClick = [this] { staff.setStaffSpace (staff.getStaffSpace() / 1.15f); proc.zoom = staff.getStaffSpace(); };
    themeButton.onClick = [this] { proc.darkMode = ! proc.darkMode; applyTheme(); edited(); };
    keysButton.onClick = [this] { showKeysWindow(); };
    settingsButton.onClick = [this] { showSettings(); };
    dragButton.setTooltip ("Drag this onto a Bitwig track or clip slot to copy the clip as MIDI");
    dragButton.addMouseListener (this, false);
    dragButton.onStateChange = [this] {
        if (dragButton.isDown() && juce::Desktop::getInstance().getMainMouseSource().isDragging())
            startMidiDrag (proc.project.selectedClip);
    };

    staff.setKeyMap (&proc.keyMap);
    staff.onKey = [this] (const juce::String& k, const juce::String& c) {
        lastKey = k;
        lastCommand = c;
        repaint (getLocalBounds().removeFromBottom (24));
    };
    staff.onScoreEdited = [this] { edited(); };

    clips.onLaunch = [this] (int i) { proc.launchClip (i); };
    clips.onStop = [this] { proc.stopClips(); };
    clips.onSong = [this] { proc.returnToSong(); };
    clips.onEdit = [this] (int i) {
        proc.project.selectedClip = i;
        bindEditor();
        showNotation();
    };
    clips.onProjectEdited = [this] { bindEditor(); edited(); };
    clips.onDragOut = [this] (int i, const juce::MouseEvent&) { startMidiDrag (i); };

    proc.onProjectReplaced = [this] {
        editorClip = -1;
        bindEditor();
        applyTheme();
        staff.setStaffSpace (proc.zoom);
        staff.setPageView (proc.pageView);
        syncToggle.setToggleState (proc.project.followHostTempo, juce::dontSendNotification);
        tempoSlider.setValue (proc.ownTempo.load(), juce::dontSendNotification);
        updateTransportButtons();
        repaint();
    };

    staff.setStaffSpace (proc.zoom);
    staff.setPageView (proc.pageView);
    viewButton.setButtonText (proc.pageView ? "Page view" : "Scroll view");
    bindEditor();
    applyTheme();
    updateTransportButtons();

    setResizable (true, true);
    setResizeLimits (640, 320, 4000, 2400);
    setSize (1100, 460);
    startTimerHz (30);
    showNotation();
}

StaffPaperEditor::~StaffPaperEditor()
{
    proc.onProjectReplaced = nullptr;
    staff.setScore (nullptr, nullptr);
    setLookAndFeel (nullptr);
}

void StaffPaperEditor::bindEditor()
{
    auto& prj = proc.project;
    if (prj.clips.empty()) prj = Project::makeDefault();
    prj.selectedClip = juce::jlimit (0, (int) prj.clips.size() - 1, prj.selectedClip);
    const bool midiEntry = editor ? editor->midiEntry : false;
    editor = std::make_unique<Editor> (prj.current().score);
    editorClip = prj.selectedClip;
    editor->midiEntry = midiEntry;
    editor->heldMidiNotes = [this] { return proc.heldNotes(); };
    editor->onChange = [this] { edited(); };
    staff.setScore (&prj.current().score, editor.get());
    clipName.setText (prj.current().name, juce::dontSendNotification);
}

void StaffPaperEditor::edited()
{
    proc.projectChanged();
    clipName.setText (proc.project.current().name, juce::dontSendNotification);
    staff.repaint();
    clips.repaint();
}

void StaffPaperEditor::applyTheme()
{
    const bool dark = proc.darkMode;
    staff.setDark (dark);
    const auto& t = staff.getTheme();
    const auto chrome = dark ? juce::Colour (0xff141518) : juce::Colour (0xffeeede8);
    const auto text = t.ink;
    lnf.setColourScheme (dark ? juce::LookAndFeel_V4::getMidnightColourScheme() : juce::LookAndFeel_V4::getLightColourScheme());
    lnf.setColour (juce::ResizableWindow::backgroundColourId, chrome);
    lnf.setColour (juce::TextButton::buttonColourId, chrome);
    lnf.setColour (juce::TextButton::buttonOnColourId, t.frame.withAlpha (0.25f));
    lnf.setColour (juce::TextButton::textColourOffId, text);
    lnf.setColour (juce::TextButton::textColourOnId, text);
    lnf.setColour (juce::ComboBox::outlineColourId, chrome);
    lnf.setColour (juce::Label::textColourId, text);
    lnf.setColour (juce::ToggleButton::textColourId, text);
    lnf.setColour (juce::ToggleButton::tickColourId, text);
    lnf.setColour (juce::ToggleButton::tickDisabledColourId, text.withAlpha (0.5f));
    lnf.setColour (juce::Slider::textBoxTextColourId, text);
    lnf.setColour (juce::Slider::textBoxOutlineColourId, chrome);
    lnf.setColour (juce::PopupMenu::backgroundColourId, dark ? juce::Colour (0xff222327) : juce::Colours::white);
    lnf.setColour (juce::PopupMenu::textColourId, text);
    themeButton.setButtonText (dark ? "Light" : "Dark");
    ClipView::Theme ct { dark ? juce::Colour (0xff1c1d20) : juce::Colour (0xfff7f6f2), dark ? juce::Colour (0xff26282c) : juce::Colours::white,
                         text, text.withAlpha (0.5f), t.frame };
    clips.setTheme (ct);
    sendLookAndFeelChange();
    repaint();
}

void StaffPaperEditor::showNotation()
{
    showingClips = false;
    clipViewport.setVisible (false);
    staff.setVisible (true);
    notationTab.setToggleState (true, juce::dontSendNotification);
    clipsTab.setToggleState (false, juce::dontSendNotification);
    staff.grabKeyboardFocus();
    resized();
}

void StaffPaperEditor::showClips()
{
    showingClips = true;
    staff.setVisible (false);
    clipViewport.setVisible (true);
    notationTab.setToggleState (false, juce::dontSendNotification);
    clipsTab.setToggleState (true, juce::dontSendNotification);
    resized();
}

void StaffPaperEditor::showKeysWindow()
{
    const auto& t = staff.getTheme();
    auto panel = std::make_unique<KeysPanel> (proc, t.paper, t.ink);
    panel->onClose = [this] { overlay.reset(); staff.grabKeyboardFocus(); };
    overlay = std::move (panel);
    addAndMakeVisible (*overlay);
    resized();
}

void StaffPaperEditor::showSettings()
{
    const auto& t = staff.getTheme();
    auto panel = std::make_unique<SettingsPanel> (proc, t.paper, t.ink);
    panel->onClose = [this] { overlay.reset(); staff.grabKeyboardFocus(); };
    panel->onChanged = [this] { edited(); };
    panel->getMidiEntry = [this] { return editor && editor->midiEntry; };
    panel->setMidiEntry = [this] (bool on) { if (editor) editor->midiEntry = on; };
    panel->refreshToggles();
    overlay = std::move (panel);
    addAndMakeVisible (*overlay);
    resized();
}

bool StaffPaperEditor::keyPressed (const juce::KeyPress& k)
{
    // keys that reach the window itself go to the staff
    if (! showingClips && ! overlay) return staff.keyPressed (k);
    if (overlay && k == juce::KeyPress::escapeKey)
    {
        overlay.reset();
        staff.grabKeyboardFocus();
        return true;
    }
    return false;
}

void StaffPaperEditor::paint (juce::Graphics& g)
{
    g.fillAll (findColour (juce::ResizableWindow::backgroundColourId));
    auto bar = getLocalBounds().removeFromBottom (24);
    g.setColour (staff.getTheme().ink.withAlpha (0.7f));
    g.setFont (juce::FontOptions (13.0f));
    juce::String status;
    if (lastKey.isNotEmpty())
        status << "Key: " << lastKey << "  " << (lastCommand.isNotEmpty() ? juce::String (juce::CharPointer_UTF8 ("\xe2\x86\x92 ")) + lastCommand : juce::String ("(not a command, passed to Bitwig)"));
    if (editor && ! editor->lastMessage().empty()) status << "    " << juce::String (editor->lastMessage());
    if (editor && editor->midiEntry) status << "    MIDI entry on";
    g.drawText (status, bar.reduced (10, 0), juce::Justification::centredLeft);
    juce::String right;
    if (proc.project.followHostTempo)
        right << "Bitwig " << (proc.hostPlaying.load() ? "playing" : "stopped") << "  " << juce::String (proc.hostTempo.load(), 1) << " bpm";
    else
        right << "Own transport  " << (proc.ownPlaying.load() ? "playing" : "stopped");
    g.drawText (right, bar.reduced (10, 0), juce::Justification::centredRight);
}

void StaffPaperEditor::resized()
{
    auto r = getLocalBounds();
    auto top = r.removeFromTop (38).reduced (8, 6);
    r.removeFromBottom (24);

    notationTab.setBounds (top.removeFromLeft (84));
    clipsTab.setBounds (top.removeFromLeft (64));
    top.removeFromLeft (10);
    clipName.setBounds (top.removeFromLeft (140));
    syncToggle.setBounds (top.removeFromLeft (130));
    if (! proc.project.followHostTempo)
    {
        rewindButton.setBounds (top.removeFromLeft (30));
        playButton.setBounds (top.removeFromLeft (30));
        stopButton.setBounds (top.removeFromLeft (30));
        top.removeFromLeft (4);
        tempoSlider.setBounds (top.removeFromLeft (150));
    }
    dragButton.setBounds (top.removeFromRight (84));
    top.removeFromRight (6);
    settingsButton.setBounds (top.removeFromRight (74));
    keysButton.setBounds (top.removeFromRight (50));
    themeButton.setBounds (top.removeFromRight (54));
    top.removeFromRight (6);
    zoomIn.setBounds (top.removeFromRight (26));
    zoomOut.setBounds (top.removeFromRight (26));
    viewButton.setBounds (top.removeFromRight (90));

    staff.setBounds (r);
    clipViewport.setBounds (r);
    clips.setSize (juce::jmax (r.getWidth() - 12, 300), 44 + ((int) proc.project.clips.size() + 3) * 38);
    if (overlay) overlay->setBounds (r);
}

void StaffPaperEditor::updateTransportButtons()
{
    const bool own = ! proc.project.followHostTempo;
    for (auto* b : { &playButton, &stopButton, &rewindButton })
        b->setVisible (own);
    tempoSlider.setVisible (own);
    resized();
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
    // follow Bitwig's meter
    if (proc.project.followHostMeter && proc.project.followHostTempo && proc.hostHasMeter.load())
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

    const int pc = proc.playheadClip.load();
    const double t = proc.playheadTick.load();
    staff.setPlayheadTick (pc == proc.project.selectedClip ? writtenTickFromPerformance (pc, t) : -1.0);
    clips.setStatus (proc.playingClip.load(), proc.queuedClip.load(), proc.hostPlaying.load());
    if (proc.queuedClip.load() >= 0) clips.repaint(); // blink

    static int tick = 0;
    if (++tick % 10 == 0) repaint (getLocalBounds().removeFromBottom (24));
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
