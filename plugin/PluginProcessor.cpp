#include "PluginProcessor.h"

#include "PluginEditor.h"

#include <cmath>

using namespace staffpaper;

StaffPaperProcessor::StaffPaperProcessor()
    : AudioProcessor (BusesProperties())
{
    project = Project::makeDefault();
    reloadKeyMap();
    projectChanged();
}

StaffPaperProcessor::~StaffPaperProcessor() = default;

// ---------------------------------------------------------------- message thread

juce::File StaffPaperProcessor::keyMapFile() const
{
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
        .getChildFile (PLUGIN_DISPLAY_NAME)
        .getChildFile ("keymap.txt");
}

void StaffPaperProcessor::reloadKeyMap()
{
    keyMap = KeyMap::defaults();
    keyMapProblems.clear();
    const auto f = keyMapFile();
    if (f.existsAsFile())
    {
        // the user's file overrides individual keys of the defaults
        const auto problems = keyMap.load (f.loadFileAsString().toStdString());
        for (const auto& p : problems) keyMapProblems << p << "\n";
    }
}

void StaffPaperProcessor::projectChanged()
{
    auto set = std::make_unique<PlaySet>();
    for (const auto& c : project.clips)
    {
        set->sequences.push_back (buildSequence (c.score));
        set->lengthBeats.push_back ((double) set->sequences.back().length / (double) PPQ);
        set->loop.push_back (c.loop);
    }
    set->songClip = project.songClip;
    set->offsetBars = project.offsetBars;
    set->sync = project.followHostTempo;
    midiThru.store (project.midiThru);

    PlaySet* raw = set.get();
    keepAlive[keepIndex] = std::move (set); // the set that was here is 16 edits old
    keepIndex = (keepIndex + 1) % keepAlive.size();
    current.store (raw, std::memory_order_release);

    juce::XmlElement xml ("StaffPaper");
    xml.setAttribute ("version", 1);
    xml.setAttribute ("dark", darkMode);
    xml.setAttribute ("tool", tool);
    xml.setAttribute ("grid", grid);
    xml.setAttribute ("zoom", zoom);
    xml.setAttribute ("tempo", ownTempo.load());
    xml.createNewChildElement ("project")->addTextElement (juce::String (project.toText()));
    const juce::ScopedLock sl (stateLock);
    cachedState = xml.toString();
}

void StaffPaperProcessor::launchClip (int index) { launchRequest.store (index); }
void StaffPaperProcessor::stopClips() { launchRequest.store (-1); }
void StaffPaperProcessor::returnToSong() { launchRequest.store (-2); }

std::vector<int> StaffPaperProcessor::heldNotes() const
{
    std::vector<int> notes;
    for (int w = 0; w < 4; ++w)
    {
        const uint32_t bits = held[(size_t) w].load();
        for (int b = 0; b < 32; ++b)
            if (bits & (1u << b)) notes.push_back (w * 32 + b);
    }
    return notes;
}

void StaffPaperProcessor::audition (const std::vector<int>& notes, int channel)
{
    for (int n : notes)
    {
        const auto scope = auditionFifo.write (1);
        if (scope.blockSize1 > 0) auditionQueue[(size_t) scope.startIndex1] = { n, channel };
        else if (scope.blockSize2 > 0) auditionQueue[(size_t) scope.startIndex2] = { n, channel };
    }
}

void StaffPaperProcessor::updateTrackProperties (const TrackProperties& props)
{
    trackColour.store (props.colourARGB.value_or (0u));
    const auto name = props.name.value_or (juce::String());
    juce::MessageManager::callAsync ([this, name] {
        trackName = name;
        if (onTrackInfo) onTrackInfo();
    });
}

// ---------------------------------------------------------------- state

void StaffPaperProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    juce::String text;
    {
        const juce::ScopedLock sl (stateLock);
        text = cachedState;
    }
    if (auto xml = juce::parseXML (text))
        copyXmlToBinary (*xml, dest);
}

void StaffPaperProcessor::setStateInformation (const void* data, int size)
{
    auto xml = getXmlFromBinary (data, size);
    if (! xml) return;
    const auto text = xml->toString();
    if (juce::MessageManager::getInstance()->isThisTheMessageThread())
        applyState (text);
    else
        juce::MessageManager::callAsync ([this, text] { applyState (text); });
}

void StaffPaperProcessor::applyState (const juce::String& text)
{
    auto xml = juce::parseXML (text);
    if (! xml || ! xml->hasTagName ("StaffPaper")) return;
    Project p;
    std::string err;
    if (auto* pe = xml->getChildByName ("project"))
        if (! Project::fromText (pe->getAllSubText().toStdString(), p, &err)) return;
    darkMode = xml->getBoolAttribute ("dark", true);
    tool = xml->getIntAttribute ("tool", 0);
    grid = xml->getIntAttribute ("grid", 0);
    zoom = (float) xml->getDoubleAttribute ("zoom", 9.0);
    ownTempo.store (xml->getDoubleAttribute ("tempo", 120.0));
    project = std::move (p);
    projectChanged();
    if (onProjectReplaced) onProjectReplaced();
}

// ---------------------------------------------------------------- audio thread

void StaffPaperProcessor::prepareToPlay (double sr, int)
{
    sampleRate = sr > 0 ? sr : 44100.0;
    out.ensureSize (16384);
    scheduler.reset();
}

void StaffPaperProcessor::processBlock (juce::AudioBuffer<float>& b, juce::MidiBuffer& m) { process (b, m); }
void StaffPaperProcessor::processBlock (juce::AudioBuffer<double>& b, juce::MidiBuffer& m) { process (b, m); }

template <typename Sample>
void StaffPaperProcessor::process (juce::AudioBuffer<Sample>& buffer, juce::MidiBuffer& midi)
{
    const int n = buffer.getNumSamples();
    buffer.clear();
    out.clear();

    // ---- MIDI input: track held keys (for MIDI-keyboard entry), optional thru
    const bool thru = midiThru.load();
    for (const auto meta : midi)
    {
        const auto msg = meta.getMessage();
        if (msg.isNoteOn())
            held[(size_t) (msg.getNoteNumber() / 32)].fetch_or (1u << (msg.getNoteNumber() % 32));
        else if (msg.isNoteOff())
            held[(size_t) (msg.getNoteNumber() / 32)].fetch_and (~(1u << (msg.getNoteNumber() % 32)));
        if (thru) out.addEvent (msg, meta.samplePosition);
    }

    PlaySet* set = current.load (std::memory_order_acquire);
    if (! set || n <= 0)
    {
        midi.swapWith (out);
        return;
    }

    // ---- transport
    bool playing = false, looping = false;
    double beat = 0, bpm = 120, loopStart = 0, loopEnd = 0;
    int num = 4, den = 4;
    // Bitwig's meter is read even when the tempo is not synced: the two follow independently.
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            if (auto ts = pos->getTimeSignature())
            {
                hostNum.store (ts->numerator);
                hostDen.store (ts->denominator);
                hostHasMeter.store (true);
            }
            if (set->sync)
            {
                playing = pos->getIsPlaying();
                beat = pos->getPpqPosition().orFallback (0.0);
                bpm = pos->getBpm().orFallback (120.0);
                if (auto ts = pos->getTimeSignature())
                {
                    num = ts->numerator;
                    den = ts->denominator;
                }
                if (auto lp = pos->getLoopPoints(); lp && pos->getIsLooping())
                {
                    looping = true;
                    loopStart = lp->ppqStart;
                    loopEnd = lp->ppqEnd;
                }
            }
        }
    if (! set->sync)
    {
        if (rewindRequest.exchange (false))
        {
            ownBeat = 0;
            scheduler.allNotesOff (0, [this] (int s, bool, int ch, int note, int) { out.addEvent (juce::MidiMessage::noteOff (ch, note), s); });
            scheduler.reset();
        }
        playing = ownPlaying.load();
        beat = ownBeat;
        bpm = ownTempo.load();
        num = hostHasMeter.load() ? hostNum.load() : 4;
        den = hostHasMeter.load() ? hostDen.load() : 4;
    }
    bpm = juce::jlimit (1.0, 999.0, bpm);
    hostPlaying.store (playing);
    hostTempo.store (bpm);
    const double bps = bpm / 60.0 / sampleRate;
    const double beatsPerBar = (double) num * 4.0 / (double) juce::jmax (1, den);
    const double blockEnd = beat + n * bps;

    // ---- launcher requests
    const int req = launchRequest.exchange (-3);
    if (req != -3)
    {
        double q = 0;
        switch (project.quantize) // read-only use of a plain enum; edits are rare and benign
        {
            case LaunchQuantize::None: q = 0; break;
            case LaunchQuantize::Beat: q = 1; break;
            case LaunchQuantize::Bar: q = beatsPerBar; break;
            case LaunchQuantize::TwoBars: q = 2 * beatsPerBar; break;
            case LaunchQuantize::FourBars: q = 4 * beatsPerBar; break;
            default: q = beatsPerBar; break;
        }
        pendingClip = req;
        pendingAt = (q <= 0 || ! playing) ? beat : std::ceil (beat / q - 1e-9) * q;
        if (! playing && q > 0) pendingAt = std::ceil (beat / q - 1e-9) * q;
        queuedClip.store (req == -2 ? -3 : req);
    }

    auto emit = [this] (int base) {
        return [this, base] (int s, bool on, int ch, int note, int vel) {
            if (on) out.addEvent (juce::MidiMessage::noteOn (ch, note, (juce::uint8) vel), base + s);
            else out.addEvent (juce::MidiMessage::noteOff (ch, note), base + s);
        };
    };

    auto applyPending = [&] (double at, int sample) {
        scheduler.allNotesOff (sample, emit (0));
        if (pendingClip >= 0 && pendingClip < (int) set->sequences.size())
        {
            activeClip = pendingClip;
            clipStartBeat = at;
            stopped = false;
        }
        else if (pendingClip == -1)
        {
            activeClip = -1;
            stopped = true;
        }
        else if (pendingClip == -2)
        {
            activeClip = -1;
            stopped = false;
        }
        pendingClip = -3;
        queuedClip.store (-2);
    };

    auto runSegment = [&] (int start, int count, double segBeat) {
        Scheduler::Block b;
        b.playing = playing;
        b.beat = segBeat;
        b.beatsPerSample = bps;
        b.numSamples = count;
        b.loopActive = looping && activeClip < 0;
        b.loopStart = loopStart;
        b.loopEnd = loopEnd;
        const Sequence* seq = nullptr;
        if (! stopped)
        {
            if (activeClip >= 0 && activeClip < (int) set->sequences.size())
            {
                seq = &set->sequences[(size_t) activeClip];
                b.offsetBeats = clipStartBeat;
                b.loopSequence = set->loop[(size_t) activeClip];
                b.sequenceLoopBeats = set->lengthBeats[(size_t) activeClip];
                b.loopActive = false;
            }
            else if (set->songClip >= 0 && set->songClip < (int) set->sequences.size())
            {
                seq = &set->sequences[(size_t) set->songClip];
                b.offsetBeats = set->offsetBars * beatsPerBar;
            }
        }
        scheduler.process (seq, b, emit (start));
    };

    // a one-shot clip that has finished stops
    if (activeClip >= 0 && activeClip < (int) set->sequences.size() && ! set->loop[(size_t) activeClip]
        && beat >= clipStartBeat + set->lengthBeats[(size_t) activeClip])
    {
        scheduler.allNotesOff (0, emit (0));
        activeClip = -1;
        stopped = true;
    }

    if (pendingClip != -3 && pendingAt <= beat + 1e-9)
    {
        applyPending (beat, 0);
        runSegment (0, n, beat);
    }
    else if (pendingClip != -3 && playing && pendingAt < blockEnd)
    {
        const int split = juce::jlimit (0, n, (int) std::floor ((pendingAt - beat) / bps));
        if (split > 0) runSegment (0, split, beat);
        applyPending (pendingAt, split);
        if (split < n)
        {
            scheduler.reset();
            runSegment (split, n - split, beat + split * bps);
        }
    }
    else
        runSegment (0, n, beat);

    // ---- audition: entered notes sound once, short, whether or not anything plays
    {
        const int toEnd = auditionSamplesLeft;
        if (auditionCount > 0 && toEnd < n)
        {
            for (int i = 0; i < auditionCount; ++i)
                out.addEvent (juce::MidiMessage::noteOff (auditionSounding[(size_t) i].channel, auditionSounding[(size_t) i].note), juce::jmax (0, toEnd));
            auditionCount = 0;
        }
        auditionSamplesLeft = juce::jmax (0, auditionSamplesLeft - n);
        const int ready = auditionFifo.getNumReady();
        if (ready > 0)
        {
            // a new note ends the one before it
            for (int i = 0; i < auditionCount; ++i)
                out.addEvent (juce::MidiMessage::noteOff (auditionSounding[(size_t) i].channel, auditionSounding[(size_t) i].note), 0);
            auditionCount = 0;
            const auto scope = auditionFifo.read (ready);
            auto take = [&] (int start, int size) {
                for (int i = 0; i < size; ++i)
                {
                    const auto a = auditionQueue[(size_t) (start + i)];
                    out.addEvent (juce::MidiMessage::noteOn (a.channel, a.note, (juce::uint8) 90), 0);
                    if (auditionCount < (int) auditionSounding.size()) auditionSounding[(size_t) auditionCount++] = a;
                }
            };
            take (scope.startIndex1, scope.blockSize1);
            take (scope.startIndex2, scope.blockSize2);
            auditionSamplesLeft = (int) (sampleRate * 0.28);
        }
    }

    if (! set->sync && playing) ownBeat = blockEnd;
    else if (! set->sync) ownBeat = beat;

    // ---- report for the UI
    playingClip.store (stopped ? -2 : activeClip);
    if (activeClip >= 0 && activeClip < (int) set->lengthBeats.size())
    {
        const double len = std::max (1e-6, set->lengthBeats[(size_t) activeClip]);
        double rel = beat - clipStartBeat;
        if (set->loop[(size_t) activeClip]) rel = std::fmod (std::max (0.0, rel), len);
        playheadTick.store (playing ? rel * PPQ : -1.0);
        playheadClip.store (activeClip);
    }
    else if (! stopped && set->songClip >= 0)
    {
        const double rel = beat - set->offsetBars * beatsPerBar;
        playheadTick.store (playing && rel >= 0 ? rel * PPQ : -1.0);
        playheadClip.store (set->songClip);
    }
    else
        playheadTick.store (-1.0);

    midi.swapWith (out);
}

juce::AudioProcessorEditor* StaffPaperProcessor::createEditor()
{
    return new StaffPaperEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new StaffPaperProcessor();
}
