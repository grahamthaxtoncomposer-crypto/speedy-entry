// Staff Paper — the audio-thread side: plays the score as MIDI in step with
// the host (or its own transport when sync is off) and runs the clip launcher.
#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "staffpaper/KeyMap.h"
#include "staffpaper/Playback.h"
#include "staffpaper/Project.h"

#include <array>
#include <atomic>
#include <memory>

class StaffPaperProcessor : public juce::AudioProcessor
{
public:
    StaffPaperProcessor();
    ~StaffPaperProcessor() override;

    // ---- AudioProcessor
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override { return true; }
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    void processBlock (juce::AudioBuffer<double>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return PLUGIN_DISPLAY_NAME; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return true; }
    bool isMidiEffect() const override { return true; }
    double getTailLengthSeconds() const override { return 0.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}
    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    // ---- message-thread API (editor)
    staffpaper::Project project; // edited on the message thread only
    staffpaper::KeyMap keyMap;
    juce::String keyMapProblems;
    bool darkMode = true;
    float zoom = 9.0f;
    int tool = 0;      // 0 = Speedy entry, 1 = select
    int grid = 0;      // selection grid, see StaffView::setGrid

    void projectChanged();     // rebuild playback after an edit
    void reloadKeyMap();
    juce::File keyMapFile() const;

    void launchClip (int index); // queue a clip on the launcher
    void stopClips();            // queue stop
    void returnToSong();         // back to the song clip following Bitwig's timeline

    // own transport (sync off)
    void ownPlay() { ownPlaying.store (true); }
    void ownStop() { ownPlaying.store (false); }
    void ownRewind() { rewindRequest.store (true); }
    std::atomic<double> ownTempo { 120.0 };

    // ---- status for the UI (written by the audio thread)
    std::atomic<bool> hostPlaying { false };
    std::atomic<bool> ownPlaying { false };
    std::atomic<double> hostTempo { 120.0 };
    std::atomic<int> hostNum { 4 }, hostDen { 4 };
    std::atomic<bool> hostHasMeter { false };
    std::atomic<int> playingClip { -1 }; // -1 = song clip (or nothing)
    std::atomic<int> queuedClip { -2 };  // -2 = nothing queued, -1 = stop queued
    std::atomic<double> playheadTick { -1.0 };
    std::atomic<int> playheadClip { -1 };
    std::atomic<bool> silent { false };   // launcher stopped

    // MIDI-keyboard entry: notes currently held on the input
    std::vector<int> heldNotes() const;

    // Play notes once, short (message thread -> audio thread).
    void audition (const std::vector<int>& notes, int channel);

    // Bitwig's track colour (ARGB), 0 when the host has not told us.
    std::atomic<uint32_t> trackColour { 0 };
    juce::String trackName;
    std::function<void()> onTrackInfo; // message thread
    void updateTrackProperties (const TrackProperties&) override;

    // Called on the message thread when the host loaded a new state.
    std::function<void()> onProjectReplaced;

private:
    struct PlaySet
    {
        std::vector<staffpaper::Sequence> sequences; // one per clip
        std::vector<double> lengthBeats;
        std::vector<bool> loop;
        int songClip = -1;
        double offsetBars = 0;
        bool sync = true;
    };
    std::atomic<PlaySet*> current { nullptr };
    std::array<std::unique_ptr<PlaySet>, 16> keepAlive; // recent sets stay valid for the audio thread
    size_t keepIndex = 0;

    staffpaper::Scheduler scheduler;
    juce::MidiBuffer out;
    double sampleRate = 44100.0;

    // launcher state (audio thread)
    std::atomic<int> launchRequest { -3 }; // -3 none, -2 back to song, -1 stop, >= 0 clip
    int activeClip = -1;      // -1 = song mode
    bool stopped = false;
    double clipStartBeat = 0;
    int pendingClip = -3;
    double pendingAt = 0;

    // own transport
    std::atomic<bool> rewindRequest { false };
    double ownBeat = 0;

    std::array<std::atomic<uint32_t>, 4> held {};

    // audition notes: a small lock-free queue, then note-offs counted down in samples
    struct AuditionNote { int note = 0, channel = 1; };
    static constexpr int kAuditionQueue = 128;
    juce::AbstractFifo auditionFifo { kAuditionQueue };
    std::array<AuditionNote, kAuditionQueue> auditionQueue {};
    std::array<AuditionNote, 32> auditionSounding {};
    int auditionCount = 0;
    int auditionSamplesLeft = 0;
    std::atomic<bool> midiThru { false };

    juce::CriticalSection stateLock;
    juce::String cachedState;
    void applyState (const juce::String& xmlText);

    template <typename Sample>
    void process (juce::AudioBuffer<Sample>&, juce::MidiBuffer&);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (StaffPaperProcessor)
};
