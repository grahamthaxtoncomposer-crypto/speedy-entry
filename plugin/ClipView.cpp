#include "ClipView.h"

#include "HouseLook.h"

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
    stopAll.setTooltip ("Stop the launched clips");
    song.setTooltip ("Back to the song clip, which follows Bitwig's timeline");
    song.toggleLook = true;
    quantize.setTooltip ("Launch quantize: when a launched clip starts");
    stopAll.onClick = [this] { if (onStop) onStop(); };
    song.onClick = [this] { if (onSong) onSong(); };
    quantize.onClick = [this] { showQuantizeMenu(); };
    for (auto* b : { &stopAll, &song, &quantize }) addAndMakeVisible (*b);
    addChildComponent (nameEditor);
    nameEditor.addListener (this);
    nameEditor.setJustification (juce::Justification::centredLeft);
    nameEditor.setIndents (4, 2);
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

juce::String ClipView::displayName (int i) const
{
    if (i < 0 || i >= (int) project.clips.size()) return {};
    const juce::String n (project.clips[(size_t) i].name);
    return hasOwnName (i) ? n : "Clip " + juce::String (i + 1);
}

bool ClipView::hasOwnName (int i) const
{
    if (i < 0 || i >= (int) project.clips.size()) return false;
    const juce::String n = juce::String (project.clips[(size_t) i].name).trim();
    if (n.isEmpty() || n == "Clip") return false;
    // "Clip 3" and the like are the automatic names
    return ! (n.startsWith ("Clip ") && n.substring (5).containsOnly ("0123456789"));
}

void ClipView::resized()
{
    const int h = getHeight() - 14;
    const int y = 7;
    stopAll.setBounds (8, y, 30, h);
    song.setBounds (42, y, 54, h);
    quantize.setBounds (getWidth() - rightReserve - 62, y + (h - 28) / 2, 54, 28);
    nameEditor.setVisible (false);
    renaming = -1;
}

juce::Rectangle<int> ClipView::slotsArea() const
{
    return { 104, 0, juce::jmax (0, getWidth() - 104 - 74 - rightReserve), getHeight() };
}

juce::Rectangle<int> ClipView::slotArea (int i) const
{
    const auto a = slotsArea();
    return { a.getX() + i * (kSlot + kGap) - juce::roundToInt (scroll), 6, kSlot, getHeight() - 12 };
}

juce::Rectangle<int> ClipView::launchButton (int i) const
{
    return slotArea (i).withSize (22, 22);
}

int ClipView::visibleSlots() const
{
    // every clip, then empty slots out to the edge (at least one)
    const int fit = (int) std::ceil ((slotsArea().getWidth() + scroll) / (float) (kSlot + kGap));
    return std::max ((int) project.clips.size() + 1, fit);
}

int ClipView::slotAt (juce::Point<int> p) const
{
    if (! slotsArea().contains (p)) return -1;
    for (int i = 0; i < visibleSlots(); ++i)
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
    const bool dark = theme.bg.getPerceivedBrightness() < 0.5f;
    g.fillAll (theme.bg);
    g.setColour (theme.line);
    g.fillRect (0, getHeight() - 1, getWidth(), 1);

    song.setToggleState (playing == -1 && project.songClip >= 0, juce::dontSendNotification);
    quantize.setButtonText (juce::String ("Q  ") + launchQuantizeName (project.quantize));

    // the launcher lane: a recessed strip the slots sit in
    const auto lane = slotsArea().toFloat().reduced (0, 3).expanded (3, 0);
    g.setColour (dark ? juce::Colours::black.withAlpha (0.25f) : juce::Colours::black.withAlpha (0.05f));
    g.fillRoundedRectangle (lane, 8.0f);

    g.saveState();
    g.reduceClipRegion (slotsArea());
    const int n = (int) project.clips.size();
    const int shown = visibleSlots();
    for (int i = 0; i < shown; ++i)
    {
        const auto r = slotArea (i).toFloat();
        if (r.getRight() < 0 || r.getX() > (float) getWidth()) continue;
        const bool hot = hover == i;

        if (i >= n)
        {
            // an empty slot: dark cell with a small stop square, like an empty launcher slot
            g.setColour (dark ? theme.slot.darker (0.25f) : theme.slot.brighter (0.4f));
            g.fillRoundedRectangle (r, 5.0f);
            g.setColour (theme.line.withAlpha (hot ? 1.0f : 0.6f));
            g.drawRoundedRectangle (r.reduced (0.5f), 5.0f, 1.0f);
            if (i == n || hot)
            {
                g.setColour (hot ? theme.text : theme.dim);
                drawIcon (g, Icon::Plus, r.withSizeKeepingCentre (14, 14), 1.6f);
            }
            else
            {
                g.setColour (theme.line);
                g.drawRect (r.withSizeKeepingCentre (7, 7), 1.0f);
            }
            continue;
        }

        const auto& c = project.clips[(size_t) i];
        auto col = clipColour (i);
        const bool selected = project.selectedClip == i;
        const bool isPlaying = playing == i;
        const bool isQueued = queued == i;
        if (! selected) col = col.withMultipliedSaturation (0.85f).withMultipliedBrightness (dark ? 0.82f : 0.95f);
        if (hot) col = col.brighter (0.08f);

        // shadow, body, and a soft top light
        g.setColour (juce::Colours::black.withAlpha (dark ? 0.4f : 0.15f));
        g.fillRoundedRectangle (r.translated (0, 1), 5.0f);
        g.setGradientFill (juce::ColourGradient (col.brighter (0.12f), 0, r.getY(), col.darker (0.08f), 0, r.getBottom(), false));
        g.fillRoundedRectangle (r, 5.0f);
        g.setColour (juce::Colours::white.withAlpha (0.18f));
        g.drawHorizontalLine ((int) r.getY() + 1, r.getX() + 4, r.getRight() - 4);

        const auto ink = textOn (col);
        // progress through the playing clip, along the bottom
        if (isPlaying)
        {
            auto bar = r.reduced (4, 0).removeFromBottom (6).withTrimmedBottom (3);
            g.setColour (ink.withAlpha (0.2f));
            g.fillRoundedRectangle (bar, 1.5f);
            g.setColour (ink.withAlpha (0.85f));
            g.fillRoundedRectangle (bar.withWidth ((float) (bar.getWidth() * juce::jlimit (0.0, 1.0, progress))), 1.5f);
        }

        // play triangle, top left
        auto lb = launchButton (i).toFloat();
        auto b = lb.withSizeKeepingCentre (8, 10).translated (1, 0);
        juce::Path tri;
        tri.addTriangle (b.getX(), b.getY(), b.getX(), b.getBottom(), b.getRight(), b.getCentreY());
        if (isPlaying)
        {
            g.setColour (theme.playing);
            g.fillEllipse (lb.withSizeKeepingCentre (16, 16));
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
            g.setColour (ink.withAlpha (hot ? 0.95f : 0.7f));
            g.fillPath (tri);
        }

        // number, top right
        g.setColour (ink.withAlpha (0.9f));
        g.setFont (juce::FontOptions (HouseLook::uiFontName(), 12.0f, juce::Font::bold));
        g.drawText (juce::String (i + 1), r.withTrimmedRight (6).withHeight (22), juce::Justification::centredRight);

        // own name along the bottom, then small marks for song / one-shot
        auto textArea = r.reduced (5, 0).withTrimmedTop (21).withTrimmedBottom (isPlaying ? 7 : 3);
        juce::String marks;
        if (project.songClip == i) marks << "song";
        else if (! c.loop) marks << "once";
        if (hasOwnName (i))
        {
            g.setColour (ink);
            g.setFont (juce::FontOptions (HouseLook::uiFontName(), 10.5f, juce::Font::bold));
            g.drawFittedText (displayName (i), textArea.toNearestInt(), juce::Justification::bottomLeft, 2, 0.85f);
        }
        else if (marks.isNotEmpty())
        {
            g.setColour (ink.withAlpha (0.7f));
            g.setFont (juce::FontOptions (HouseLook::uiFontName(), 9.5f, juce::Font::plain));
            g.drawText (marks, textArea, juce::Justification::bottomLeft);
        }

        if (selected)
        {
            g.setColour (dark ? juce::Colours::white : juce::Colour (0xff171916));
            g.drawRoundedRectangle (r.expanded (1.5f), 6.5f, 2.0f);
        }
    }
    g.restoreState();
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

void ClipView::duplicateClip (int i)
{
    if (i < 0 || i >= (int) project.clips.size()) return;
    auto c = project.clips[(size_t) i];
    if (hasOwnName (i)) c.name += " copy";
    else c.name = "Clip " + std::to_string (i + 2);
    project.clips.insert (project.clips.begin() + i + 1, c);
    if (project.songClip > i) project.songClip++;
    if (onSelect) onSelect (i + 1);
    if (onProjectEdited) onProjectEdited();
    scrollToSelected();
    repaint();
}

void ClipView::deleteClip (int i)
{
    if (i < 0 || i >= (int) project.clips.size() || project.clips.size() <= 1) return;
    project.clips.erase (project.clips.begin() + i);
    if (project.songClip == i) project.songClip = -1;
    else if (project.songClip > i) project.songClip--;
    project.selectedClip = std::min (std::max (0, i - (i == (int) project.clips.size() ? 1 : 0)), (int) project.clips.size() - 1);
    if (onSelect) onSelect (project.selectedClip);
    if (onProjectEdited) onProjectEdited();
    repaint();
}

void ClipView::toggleLoop (int i)
{
    if (i < 0 || i >= (int) project.clips.size() || project.songClip == i) return;
    project.clips[(size_t) i].loop = ! project.clips[(size_t) i].loop;
    if (onProjectEdited) onProjectEdited();
    repaint();
}

void ClipView::showQuantizeMenu()
{
    juce::PopupMenu m;
    m.addSectionHeader ("Launch quantize");
    for (int q = 0; q < (int) LaunchQuantize::Count; ++q)
        m.addItem (launchQuantizeName ((LaunchQuantize) q), true, project.quantize == (LaunchQuantize) q, [this, q] {
            project.quantize = (LaunchQuantize) q;
            if (onProjectEdited) onProjectEdited();
            repaint();
        });
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&quantize).withParentComponent (getTopLevelComponent()));
}

void ClipView::mouseMove (const juce::MouseEvent& e)
{
    const int h = slotAt (e.getPosition());
    if (h != hover)
    {
        hover = h;
        setMouseCursor (h >= 0 ? juce::MouseCursor::PointingHandCursor : juce::MouseCursor::NormalCursor);
        repaint();
    }
}

void ClipView::mouseExit (const juce::MouseEvent&)
{
    if (hover != -1) { hover = -1; repaint(); }
}

void ClipView::mouseDown (const juce::MouseEvent& e)
{
    dragFrom = -1;
    dragStarted = false;
    const auto pos = e.getPosition();
    const int n = (int) project.clips.size();
    const int slot = slotAt (pos);
    if (slot < 0) return;
    if (slot >= n)
    {
        if (! e.mods.isPopupMenu()) addClip();
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
        startRename (slot);
}

void ClipView::mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails& w)
{
    const float total = (float) ((int) project.clips.size() + 1) * (kSlot + kGap);
    const float d = (std::abs (w.deltaX) > std::abs (w.deltaY) ? w.deltaX : w.deltaY) * 300.0f;
    scroll = juce::jlimit (0.0f, std::max (0.0f, total - (float) slotsArea().getWidth()), scroll - d);
    repaint();
}

void ClipView::startRename (int i)
{
    if (i < 0 || i >= (int) project.clips.size()) return;
    project.selectedClip = i;
    scrollToSelected();
    renaming = i;
    auto r = slotArea (i);
    nameEditor.setBounds (r.getX() - 4, r.getY() + 8, std::max (150, r.getWidth() + 8), 26);
    nameEditor.setFont (juce::FontOptions (HouseLook::uiFontName(), 13.0f, juce::Font::plain));
    nameEditor.setText (hasOwnName (i) ? juce::String (project.clips[(size_t) i].name) : juce::String(), false);
    nameEditor.setTextToShowWhenEmpty ("Clip " + juce::String (i + 1), theme.dim);
    nameEditor.setVisible (true);
    nameEditor.toFront (true);
    nameEditor.selectAll();
    nameEditor.grabKeyboardFocus();
}

void ClipView::finishRename (bool keep)
{
    if (renaming < 0) return;
    const int i = renaming;
    renaming = -1;
    nameEditor.setVisible (false);
    if (keep && i < (int) project.clips.size())
    {
        auto t = nameEditor.getText().trim();
        project.clips[(size_t) i].name = (t.isEmpty() ? "Clip " + juce::String (i + 1) : t).toStdString();
        if (onProjectEdited) onProjectEdited();
    }
    if (onSelect) onSelect (project.selectedClip);
    repaint();
}

void ClipView::showClipMenu (int i)
{
    juce::PopupMenu m;
    auto changed = [this] { if (onProjectEdited) onProjectEdited(); repaint(); };
    m.addItem ("Launch", [this, i] { if (onLaunch) onLaunch (i); });
    m.addItem ("Rename", [this, i] { startRename (i); });
    m.addItem ("Loop", project.songClip != i, project.clips[(size_t) i].loop, [this, i, changed] {
        project.clips[(size_t) i].loop = ! project.clips[(size_t) i].loop;
        changed();
    });
    m.addItem ("Song clip (follows Bitwig's timeline)", true, project.songClip == i, [this, i, changed] {
        project.songClip = project.songClip == i ? -1 : i;
        changed();
    });
    m.addItem ("Duplicate", [this, i] { duplicateClip (i); });
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
    m.addItem ("Delete", project.clips.size() > 1, false, [this, i] { deleteClip (i); });
    juce::Component::SafePointer<ClipView> safe (this);
    m.showMenuAsync (juce::PopupMenu::Options().withTargetScreenArea (localAreaToGlobal (slotArea (i))).withParentComponent (getTopLevelComponent()),
                     [safe, i] (int r) {
                         if (! safe || r < 2000) return;
                         safe->project.clips[(size_t) i].colour = kClipColours[(size_t) (r - 2000)];
                         if (safe->onProjectEdited) safe->onProjectEdited();
                         safe->repaint();
                     });
}
