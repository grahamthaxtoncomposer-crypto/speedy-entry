#include "ClipView.h"

using namespace staffpaper;

namespace
{
const juce::uint32 kClipColours[] = { 0xff5b8def, 0xffe0a23b, 0xff4fbf7a, 0xffd9534f, 0xff9b6ddf, 0xff3bb6c4, 0xffd96fa8, 0xff8a8f98 };
}

ClipView::ClipView (Project& p) : project (p)
{
    theme = { juce::Colour (0xfff2f1ed), juce::Colour (0xffffffff), juce::Colour (0xff1a1a1a), juce::Colour (0xff8a8a8a), juce::Colour (0xff2f6fd6) };
}

void ClipView::setStatus (int p, int q, bool hp)
{
    if (p == playing && q == queued && hp == hostPlaying) return;
    playing = p;
    queued = q;
    hostPlaying = hp;
    repaint();
}

juce::Rectangle<int> ClipView::rowArea (int i) const
{
    return { 12, 44 + i * (kRow + 4), std::min (getWidth() - 24, 520), kRow };
}

juce::Rectangle<int> ClipView::launchButton (int i) const
{
    return rowArea (i).removeFromLeft (kRow);
}

int ClipView::rowAt (juce::Point<int> p) const
{
    for (int i = 0; i <= (int) project.clips.size(); ++i)
        if (rowArea (i).contains (p)) return i;
    return -1;
}

void ClipView::paint (juce::Graphics& g)
{
    g.fillAll (theme.bg);
    g.setColour (theme.dim);
    g.setFont (juce::FontOptions (13.0f));
    g.drawText ("Click ▶ to launch. Double-click a clip to write in it. Drag a clip onto a Bitwig track to copy it as MIDI. Right-click for options.",
                juce::Rectangle<int> (12, 8, getWidth() - 24, 28), juce::Justification::centredLeft, true);

    for (int i = 0; i < (int) project.clips.size(); ++i)
    {
        const auto& c = project.clips[(size_t) i];
        auto r = rowArea (i);
        g.setColour (theme.slot);
        g.fillRoundedRectangle (r.toFloat(), 4.0f);
        const auto col = juce::Colour (c.colour);
        g.setColour (col);
        g.fillRoundedRectangle (r.removeFromLeft (6).toFloat(), 2.0f);

        // launch button
        auto b = launchButton (i).reduced (9).toFloat();
        const bool isPlaying = playing == i;
        const bool isQueued = queued == i;
        g.setColour (isPlaying ? juce::Colour (0xff3ddc97) : (isQueued ? juce::Colour (0xffe0a23b) : theme.dim));
        juce::Path tri;
        tri.addTriangle (b.getX() + 2, b.getY(), b.getX() + 2, b.getBottom(), b.getRight(), b.getCentreY());
        if (isQueued && ! isPlaying && (juce::Time::getMillisecondCounter() / 250) % 2 == 0)
            g.strokePath (tri, juce::PathStrokeType (1.5f));
        else
            g.fillPath (tri);

        auto text = rowArea (i).withTrimmedLeft (kRow + 4);
        g.setColour (project.selectedClip == i ? theme.accent : theme.text);
        g.setFont (juce::FontOptions (14.0f, project.selectedClip == i ? juce::Font::bold : juce::Font::plain));
        g.drawText (c.name, text.removeFromLeft (text.getWidth() - 150), juce::Justification::centredLeft, true);
        g.setColour (theme.dim);
        g.setFont (juce::FontOptions (12.0f));
        juce::String info;
        info << c.score.numMeasures() << " bars";
        if (project.songClip == i) info << "  · song";
        else info << (c.loop ? "  · loop" : "  · one shot");
        g.drawText (info, text.reduced (6, 0), juce::Justification::centredRight);
    }

    // add slot
    auto add = rowArea ((int) project.clips.size());
    g.setColour (theme.dim.withAlpha (0.6f));
    const float dash[] = { 4.0f, 3.0f };
    juce::Path p;
    p.addRoundedRectangle (add.toFloat(), 4.0f);
    juce::PathStrokeType (1.0f).createDashedStroke (p, p, dash, 2);
    g.fillPath (p);
    g.drawText ("+ New clip", add, juce::Justification::centred);

    // stop / song buttons
    auto bottom = rowArea ((int) project.clips.size() + 1);
    auto stopR = bottom.removeFromLeft (120);
    g.setColour (theme.slot);
    g.fillRoundedRectangle (stopR.toFloat(), 4.0f);
    g.setColour (theme.text);
    g.drawText (juce::String (juce::CharPointer_UTF8 ("\xe2\x96\xa0  Stop clips")), stopR, juce::Justification::centred);
    bottom.removeFromLeft (8);
    auto songR = bottom.removeFromLeft (160);
    g.setColour (playing == -1 ? theme.accent.withAlpha (0.2f) : theme.slot);
    g.fillRoundedRectangle (songR.toFloat(), 4.0f);
    g.setColour (theme.text);
    g.drawText ("Follow song timeline", songR, juce::Justification::centred);
    g.setColour (theme.dim);
    g.drawText (juce::String ("Launch quantize: ") + launchQuantizeName (project.quantize), bottom.withTrimmedLeft (8), juce::Justification::centredLeft);
}

void ClipView::mouseDown (const juce::MouseEvent& e)
{
    dragFrom = -1;
    dragStarted = false;
    const auto pos = e.getPosition();
    const int n = (int) project.clips.size();

    auto bottom = rowArea (n + 1);
    if (bottom.removeFromLeft (120).contains (pos)) { if (onStop) onStop(); return; }
    bottom.removeFromLeft (8);
    if (bottom.removeFromLeft (160).contains (pos)) { if (onSong) onSong(); return; }
    if (bottom.withTrimmedLeft (8).contains (pos))
    {
        juce::PopupMenu m;
        for (int q = 0; q < (int) LaunchQuantize::Count; ++q)
            m.addItem (launchQuantizeName ((LaunchQuantize) q), true, project.quantize == (LaunchQuantize) q, [this, q] {
                project.quantize = (LaunchQuantize) q;
                if (onProjectEdited) onProjectEdited();
                repaint();
            });
        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this).withParentComponent (getTopLevelComponent()));
        return;
    }

    const int row = rowAt (pos);
    if (row < 0) return;
    if (row == n)
    {
        Clip c;
        c.name = "Clip " + std::to_string (n + 1);
        c.colour = kClipColours[(size_t) n % 8];
        c.score = Score::makeEmpty (4);
        if (! project.clips.empty())
        {
            const auto& prev = project.clips.back().score;
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
        if (onProjectEdited) onProjectEdited();
        repaint();
        return;
    }
    if (e.mods.isPopupMenu()) { showClipMenu (row); return; }
    if (launchButton (row).contains (pos)) { if (onLaunch) onLaunch (row); return; }
    dragFrom = row;
}

void ClipView::mouseDrag (const juce::MouseEvent& e)
{
    if (dragFrom < 0 || dragStarted || e.getDistanceFromDragStart() < 8) return;
    dragStarted = true;
    if (onDragOut) onDragOut (dragFrom, e);
}

void ClipView::mouseDoubleClick (const juce::MouseEvent& e)
{
    const int row = rowAt (e.getPosition());
    if (row >= 0 && row < (int) project.clips.size() && ! launchButton (row).contains (e.getPosition()))
        if (onEdit) onEdit (row);
}

void ClipView::showClipMenu (int i)
{
    juce::PopupMenu m;
    auto changed = [this] { if (onProjectEdited) onProjectEdited(); repaint(); };
    m.addItem ("Write in this clip", [this, i] { if (onEdit) onEdit (i); });
    m.addItem ("Rename...", [this, i, changed] {
        auto* w = new juce::AlertWindow ("Rename clip", {}, juce::MessageBoxIconType::NoIcon, this);
        w->addTextEditor ("name", project.clips[(size_t) i].name);
        w->addButton ("OK", 1, juce::KeyPress (juce::KeyPress::returnKey));
        w->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
        juce::Component::SafePointer<ClipView> safe (this);
        w->enterModalState (true, juce::ModalCallbackFunction::create ([safe, w, i, changed] (int r) {
            if (safe && r == 1 && i < (int) safe->project.clips.size())
            {
                safe->project.clips[(size_t) i].name = w->getTextEditorContents ("name").toStdString();
                changed();
            }
        }), true);
    });
    m.addItem ("Loop", project.songClip != i, project.clips[(size_t) i].loop, [this, i, changed] {
        project.clips[(size_t) i].loop = ! project.clips[(size_t) i].loop;
        changed();
    });
    m.addItem ("Follows the song timeline", true, project.songClip == i, [this, i, changed] {
        project.songClip = project.songClip == i ? -1 : i;
        changed();
    });
    juce::PopupMenu colours;
    for (auto c : kClipColours)
        colours.addColouredItem (1000 + (int) (c & 0xff), "●", juce::Colour (c), true, project.clips[(size_t) i].colour == c, nullptr);
    m.addItem ("Duplicate", [this, i, changed] {
        auto c = project.clips[(size_t) i];
        c.name += " copy";
        project.clips.insert (project.clips.begin() + i + 1, c);
        if (project.songClip > i) project.songClip++;
        changed();
    });
    m.addItem ("Delete", project.clips.size() > 1, false, [this, i, changed] {
        project.clips.erase (project.clips.begin() + i);
        if (project.songClip == i) project.songClip = -1;
        else if (project.songClip > i) project.songClip--;
        project.selectedClip = std::min (project.selectedClip, (int) project.clips.size() - 1);
        changed();
    });
    juce::PopupMenu colour;
    for (size_t k = 0; k < 8; ++k)
    {
        const auto c = kClipColours[k];
        colour.addItem ("Colour " + juce::String ((int) k + 1), true, project.clips[(size_t) i].colour == c, [this, i, c, changed] {
            project.clips[(size_t) i].colour = c;
            changed();
        });
    }
    m.addSubMenu ("Colour", colour);
    m.showMenuAsync (juce::PopupMenu::Options().withTargetScreenArea (localAreaToGlobal (rowArea (i))).withParentComponent (getTopLevelComponent()));
}
