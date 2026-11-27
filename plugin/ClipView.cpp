#include "ClipView.h"

using namespace staffpaper;

namespace
{
// Clip colours to pick from (0 = follow the track's colour).
const juce::uint32 kClipColours[] = { 0xffd9534f, 0xffe0a23b, 0xffccff66, 0xff4fbf7a, 0xff3bb6c4, 0xff4067d6, 0xff9b6ddf, 0xffd96fa8, 0xff8a8f98 };
const char* kClipColourNames[] = { "Red", "Orange", "Lime", "Green", "Teal", "Blue", "Purple", "Pink", "Grey" };

juce::Colour textOn (juce::Colour c) { return c.getPerceivedBrightness() > 0.6f ? juce::Colour (0xff171916) : juce::Colour (0xfff2f0e9); }
}

ClipView::ClipView (Project& p) : project (p)
{
    theme = { juce::Colour (0xff20231f), juce::Colour (0xff2a2d28), juce::Colour (0xfff2f0e9), juce::Colour (0xffa4a69c),
              juce::Colour (0xff41443d), juce::Colour (0xffccff66), juce::Colour (0xff3ddc97) };
}

void ClipView::setStatus (int p, int q, bool hp, double prog)
{
    if (p == playing && q == queued && hp == hostPlaying && std::abs (prog - progress) < 0.004) return;
    playing = p;
    queued = q;
    hostPlaying = hp;
    progress = prog;
    repaint();
}

juce::Colour ClipView::clipColour (int i) const
{
    if (i < 0 || i >= (int) project.clips.size()) return theme.trackColour;
    const auto c = project.clips[(size_t) i].colour;
    return c == 0 ? theme.trackColour : juce::Colour (c);
}

juce::Rectangle<int> ClipView::stopButton() const { return { 6, 4, 26, getHeight() - 8 }; }
juce::Rectangle<int> ClipView::songButton() const { return { 36, 4, 52, getHeight() - 8 }; }
juce::Rectangle<int> ClipView::quantizeButton() const { return { getWidth() - rightReserve - 74, 4, 68, getHeight() - 8 }; }
juce::Rectangle<int> ClipView::slotsArea() const { return { 94, 0, juce::jmax (0, getWidth() - 94 - 80 - rightReserve), getHeight() }; }

juce::Rectangle<int> ClipView::slotArea (int i) const
{
    const auto a = slotsArea();
    return { a.getX() + i * (kSlot + kGap) - juce::roundToInt (scroll), 4, kSlot, getHeight() - 8 };
}

juce::Rectangle<int> ClipView::launchButton (int i) const
{
    return slotArea (i).removeFromLeft (24);
}

int ClipView::slotAt (juce::Point<int> p) const
{
    if (! slotsArea().contains (p)) return -1;
    for (int i = 0; i <= (int) project.clips.size(); ++i)
        if (slotArea (i).contains (p)) return i;
    return -1;
}

void ClipView::scrollToSelected()
{
    const auto a = slotsArea();
    if (a.getWidth() <= 0) { scroll = 0; return; }
    const auto r = slotArea (project.selectedClip).translated (juce::roundToInt (scroll), 0);
    if (r.getX() - scroll < a.getX()) scroll = (float) (r.getX() - a.getX());
    else if (r.getRight() - scroll > a.getRight()) scroll = (float) (r.getRight() - a.getRight());
    scroll = std::max (0.0f, scroll);
    repaint();
}

void ClipView::paint (juce::Graphics& g)
{
    {
        const float total = (float) ((int) project.clips.size() + 1) * (kSlot + kGap);
        scroll = juce::jlimit (0.0f, std::max (0.0f, total - (float) slotsArea().getWidth()), scroll);
    }
    g.fillAll (theme.bg);
    g.setColour (theme.line);
    g.fillRect (0, getHeight() - 1, getWidth(), 1);

    // stop button: a square, like a launcher track's stop
    {
        auto r = stopButton().toFloat();
        g.setColour (theme.slot);
        g.fillRoundedRectangle (r, 6.0f);
        g.setColour (playing == -2 ? theme.dim : theme.text);
        g.fillRect (r.withSizeKeepingCentre (8, 8));
    }
    // song: the song clip follows Bitwig's timeline
    {
        auto r = songButton().toFloat();
        const bool on = playing == -1 && project.songClip >= 0;
        g.setColour (on ? theme.trackColour.withAlpha (0.18f) : theme.slot);
        g.fillRoundedRectangle (r, 6.0f);
        g.setColour (on ? theme.trackColour : theme.line);
        g.drawRoundedRectangle (r.reduced (0.5f), 6.0f, 1.0f);
        g.setColour (on ? theme.text : theme.dim);
        g.setFont (juce::FontOptions (11.0f, juce::Font::bold));
        g.drawText ("SONG", r, juce::Justification::centred);
    }

    g.saveState();
    g.reduceClipRegion (slotsArea());
    const int n = (int) project.clips.size();
    for (int i = 0; i < n; ++i)
    {
        const auto& c = project.clips[(size_t) i];
        const auto r = slotArea (i).toFloat();
        if (r.getRight() < 0 || r.getX() > (float) getWidth()) continue;
        const auto col = clipColour (i);
        const bool selected = project.selectedClip == i;
        const bool isPlaying = playing == i;
        const bool isQueued = queued == i;

        g.setColour (col.withAlpha (selected ? 1.0f : 0.78f));
        g.fillRoundedRectangle (r, 4.0f);
        if (isPlaying && progress > 0)
        {
            // how far through the clip we are
            g.setColour (juce::Colours::white.withAlpha (0.22f));
            g.fillRoundedRectangle (r.withWidth ((float) (r.getWidth() * juce::jlimit (0.0, 1.0, progress))), 4.0f);
        }
        if (selected)
        {
            g.setColour (theme.text);
            g.drawRoundedRectangle (r.expanded (1.5f), 5.0f, 1.5f);
        }

        // play triangle
        const auto ink = textOn (col);
        auto b = launchButton (i).toFloat().withSizeKeepingCentre (10, 12);
        juce::Path tri;
        tri.addTriangle (b.getX(), b.getY(), b.getX(), b.getBottom(), b.getRight(), b.getCentreY());
        if (isPlaying)
        {
            g.setColour (theme.playing);
            g.fillEllipse (launchButton (i).toFloat().withSizeKeepingCentre (18, 18));
            g.setColour (juce::Colour (0xff171916));
            g.fillPath (tri);
        }
        else if (isQueued)
        {
            g.setColour (ink);
            if ((juce::Time::getMillisecondCounter() / 250) % 2 == 0) g.fillPath (tri);
            else g.strokePath (tri, juce::PathStrokeType (1.2f));
        }
        else
        {
            g.setColour (ink.withAlpha (0.8f));
            g.fillPath (tri);
        }

        auto text = r.withTrimmedLeft (24).reduced (2, 0);
        g.setColour (ink);
        g.setFont (juce::FontOptions (12.0f, selected ? juce::Font::bold : juce::Font::plain));
        juce::String tag;
        if (project.songClip == i) tag = " (song)";
        else if (! c.loop) tag = " (once)";
        g.drawText (juce::String (c.name) + tag, text, juce::Justification::centredLeft, true);
    }
    // new clip slot
    {
        const auto r = slotArea (n).toFloat().withWidth (40);
        g.setColour (theme.slot);
        g.fillRoundedRectangle (r, 4.0f);
        g.setColour (theme.dim);
        g.setFont (juce::FontOptions (16.0f));
        g.drawText ("+", r, juce::Justification::centred);
    }
    g.restoreState();

    // launch quantize
    {
        auto r = quantizeButton().toFloat();
        g.setColour (theme.slot);
        g.fillRoundedRectangle (r, 6.0f);
        g.setColour (theme.dim);
        g.setFont (juce::FontOptions (11.0f));
        g.drawText (juce::String ("Q ") + launchQuantizeName (project.quantize), r, juce::Justification::centred);
    }
}

void ClipView::addClip()
{
    const int n = (int) project.clips.size();
    Clip c;
    c.name = "Clip " + std::to_string (n + 1);
    c.colour = 0; // follows the track
    c.score = Score::makeEmpty (4);
    if (! project.clips.empty())
    {
        const auto& prev = project.clips[(size_t) project.selectedClip].score;
        c.score.timeSig = prev.timeSig;
        c.score.keyFifths = prev.keyFifths;
        if (prev.isGrandStaff()) c.score.setGrandStaff (true);
        for (size_t s = 0; s < c.score.staves.size() && s < prev.staves.size(); ++s)
        {
            c.score.staves[s].clef = prev.staves[s].clef;
            c.score.staves[s].midiChannel = prev.staves[s].midiChannel;
        }
        c.score.playback = prev.playback;
    }
    project.clips.push_back (c);
    if (onSelect) onSelect (n);
    if (onProjectEdited) onProjectEdited();
    scrollToSelected();
}

void ClipView::mouseDown (const juce::MouseEvent& e)
{
    dragFrom = -1;
    dragStarted = false;
    const auto pos = e.getPosition();
    const int n = (int) project.clips.size();

    if (stopButton().contains (pos)) { if (onStop) onStop(); return; }
    if (songButton().contains (pos)) { if (onSong) onSong(); return; }
    if (quantizeButton().contains (pos))
    {
        juce::PopupMenu m;
        m.addSectionHeader ("Launch quantize");
        for (int q = 0; q < (int) LaunchQuantize::Count; ++q)
            m.addItem (launchQuantizeName ((LaunchQuantize) q), true, project.quantize == (LaunchQuantize) q, [this, q] {
                project.quantize = (LaunchQuantize) q;
                if (onProjectEdited) onProjectEdited();
                repaint();
            });
        m.showMenuAsync (juce::PopupMenu::Options().withTargetScreenArea (localAreaToGlobal (quantizeButton())).withParentComponent (getTopLevelComponent()));
        return;
    }

    const int slot = slotAt (pos);
    if (slot < 0) return;
    if (slot == n)
    {
        if (slotArea (n).withWidth (40).contains (pos)) addClip();
        return;
    }
    if (e.mods.isPopupMenu()) { showClipMenu (slot); return; }
    if (launchButton (slot).contains (pos)) { if (onLaunch) onLaunch (slot); return; }
    if (onSelect) onSelect (slot);
    dragFrom = slot;
    repaint();
}

void ClipView::mouseDrag (const juce::MouseEvent& e)
{
    if (dragFrom < 0 || dragStarted || e.getDistanceFromDragStart() < 8) return;
    dragStarted = true;
    if (onDragOut) onDragOut (dragFrom, e);
}

void ClipView::mouseDoubleClick (const juce::MouseEvent& e)
{
    const int slot = slotAt (e.getPosition());
    if (slot >= 0 && slot < (int) project.clips.size() && ! launchButton (slot).contains (e.getPosition()))
        rename (slot);
}

void ClipView::mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails& w)
{
    const float total = (float) ((int) project.clips.size() + 1) * (kSlot + kGap);
    const float d = (std::abs (w.deltaX) > std::abs (w.deltaY) ? w.deltaX : w.deltaY) * 300.0f;
    scroll = juce::jlimit (0.0f, std::max (0.0f, total - (float) slotsArea().getWidth()), scroll - d);
    repaint();
}

void ClipView::rename (int i)
{
    auto* w = new juce::AlertWindow ("Rename clip", {}, juce::MessageBoxIconType::NoIcon, this);
    w->addTextEditor ("name", project.clips[(size_t) i].name);
    w->addButton ("OK", 1, juce::KeyPress (juce::KeyPress::returnKey));
    w->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
    juce::Component::SafePointer<ClipView> safe (this);
    w->enterModalState (true, juce::ModalCallbackFunction::create ([safe, w, i] (int r) {
        if (safe && r == 1 && i < (int) safe->project.clips.size())
        {
            safe->project.clips[(size_t) i].name = w->getTextEditorContents ("name").toStdString();
            if (safe->onProjectEdited) safe->onProjectEdited();
            safe->repaint();
        }
    }), true);
}

void ClipView::showClipMenu (int i)
{
    juce::PopupMenu m;
    auto changed = [this] { if (onProjectEdited) onProjectEdited(); repaint(); };
    m.addItem ("Launch", [this, i] { if (onLaunch) onLaunch (i); });
    m.addItem ("Rename...", [this, i] { rename (i); });
    m.addItem ("Loop", project.songClip != i, project.clips[(size_t) i].loop, [this, i, changed] {
        project.clips[(size_t) i].loop = ! project.clips[(size_t) i].loop;
        changed();
    });
    m.addItem ("Song clip (follows Bitwig's timeline)", true, project.songClip == i, [this, i, changed] {
        project.songClip = project.songClip == i ? -1 : i;
        changed();
    });
    m.addItem ("Duplicate", [this, i, changed] {
        auto c = project.clips[(size_t) i];
        c.name += " copy";
        project.clips.insert (project.clips.begin() + i + 1, c);
        if (project.songClip > i) project.songClip++;
        changed();
    });
    m.addItem ("Move left", i > 0, false, [this, i, changed] {
        std::swap (project.clips[(size_t) i], project.clips[(size_t) i - 1]);
        if (project.selectedClip == i) project.selectedClip--;
        if (project.songClip == i) project.songClip--; else if (project.songClip == i - 1) project.songClip++;
        changed();
    });
    m.addItem ("Move right", i + 1 < (int) project.clips.size(), false, [this, i, changed] {
        std::swap (project.clips[(size_t) i], project.clips[(size_t) i + 1]);
        if (project.selectedClip == i) project.selectedClip++;
        if (project.songClip == i) project.songClip++; else if (project.songClip == i + 1) project.songClip--;
        changed();
    });
    juce::PopupMenu colour;
    colour.addItem ("Track colour", true, project.clips[(size_t) i].colour == 0, [this, i, changed] {
        project.clips[(size_t) i].colour = 0;
        changed();
    });
    for (size_t k = 0; k < std::size (kClipColours); ++k)
    {
        const auto c = kClipColours[k];
        colour.addColouredItem (2000 + (int) k, kClipColourNames[k], juce::Colour (c), true, project.clips[(size_t) i].colour == c);
    }
    m.addSubMenu ("Colour", colour);
    m.addSeparator();
    m.addItem ("Delete", project.clips.size() > 1, false, [this, i, changed] {
        project.clips.erase (project.clips.begin() + i);
        if (project.songClip == i) project.songClip = -1;
        else if (project.songClip > i) project.songClip--;
        project.selectedClip = std::min (project.selectedClip, (int) project.clips.size() - 1);
        if (onSelect) onSelect (project.selectedClip);
        changed();
    });
    juce::Component::SafePointer<ClipView> safe (this);
    m.showMenuAsync (juce::PopupMenu::Options().withTargetScreenArea (localAreaToGlobal (slotArea (i))).withParentComponent (getTopLevelComponent()),
                     [safe, i] (int r) {
                         if (! safe || r < 2000) return;
                         safe->project.clips[(size_t) i].colour = kClipColours[(size_t) (r - 2000)];
                         if (safe->onProjectEdited) safe->onProjectEdited();
                         safe->repaint();
                     });
}
