/*
  ==============================================================================

    ChannelPluginChain.h

    Hosts up to 2 third-party VST3 plugin instances for one mixer channel.

    Named "plugin chain" / "plugin slot" deliberately, not "insert chain" —
    AudioEngine already has an unrelated built-in `applyMasterInsert`
    (saturation, driven by `masterInsertDrive`), and MixerPage's Master strip
    has separate decorative `insertA`/`insertB` combo boxes tied to the
    always-on compressor/limiter. This class is a third, unrelated concept;
    keeping the name distinct avoids confusing the three at the code level.

    Real-time safety contract:
      - loadPlugin() / unloadPlugin() must be called from the message thread
        (or any non-audio thread). Plugin construction/destruction is not
        realtime-safe and must never happen on, or be blocked by, the audio
        thread.
      - process() must only be called from the audio thread.
      - The two sides communicate via a lock-free atomic pointer swap per
        slot: loadPlugin() builds the new juce::AudioPluginInstance off the
        audio thread, atomically swaps it in, and defers destruction of the
        previous instance to the message thread (juce::MessageManager::
        callAsync) — process() never locks a mutex and never deletes
        anything.

    One instance of this class is owned per real mixer channel: Music,
    Vocal 1, Vocal 2, Effects, Master (all inside AudioEngine), and
    Background Music (inside BackgroundMusicPlayer, on its own independent
    audio device).

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include <array>
#include <atomic>
#include <functional>

class ChannelPluginChain
{
public:
    static constexpr int numSlots = 2;

    ChannelPluginChain() = default;
    ~ChannelPluginChain();

    /** Call once before audio starts, and again if the device's sample rate
        or block size changes. Message thread only. */
    void prepare(double sampleRate, int maxBlockSize, int numChannels);

    using LoadCallback = std::function<void(bool success, juce::String error)>;

    /** Asynchronously instantiate `desc` into `slotIndex`, replacing
        whatever was there. Message thread only. `onDone` is called on the
        message thread once instantiation completes (success or failure).
        If `initialState` is non-null, it's applied via
        setStateInformation() immediately after creation, before the plugin
        is swapped in — used to restore a previously-saved plugin at
        startup. */
    void loadPlugin(int slotIndex,
                    const juce::PluginDescription& desc,
                    juce::AudioPluginFormatManager& formatManager,
                    LoadCallback onDone,
                    const juce::MemoryBlock* initialState = nullptr);

    /** Clear a slot. Message thread only. */
    void unloadPlugin(int slotIndex);

    /** Run whatever's loaded, in slot order. Audio thread only. No-ops
        (and does not touch the buffer) if bypassed or a slot is empty. */
    void process(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi);

    //==========================================================================
    // Bypass — a single realtime-safe bool flag, not plugin unloading, so it
    // can be toggled instantly from the audio thread's perspective (e.g. a
    // Mixer-wide "Bypass All Plugins" panic control) with zero risk.
    void setBypassed(bool shouldBypass) noexcept { bypassed_ = shouldBypass; }
    bool isBypassed() const noexcept { return bypassed_.load(); }

    /** True if the most recent process() call took longer than the
        callback's available time budget (bufferSize / sampleRate).
        Informational only in v1 — no automatic unloading. */
    bool isOverBudget() const noexcept { return overBudget_.load(); }

    //==========================================================================
    bool hasPluginInSlot(int slotIndex) const noexcept;
    juce::PluginDescription getDescriptionForSlot(int slotIndex) const;

    /** Returns the plugin's editor, creating it if needed. Message thread
        only. Returns nullptr if the slot is empty. */
    juce::AudioProcessorEditor* getEditorForSlot(int slotIndex);

    /** Sum of both slots' reported latency. Informational only — no PDC. */
    int getLatencySamples() const;

    /** Snapshot of the loaded plugin's current parameter state, for
        persistence. Message thread only. Empty if the slot has no plugin.
        Note: like most lightweight hosts, this doesn't synchronize against
        a concurrent audio-thread processBlock() call — acceptable for the
        infrequent, user-driven save points this is used at (see
        MixerPage), not a general-purpose thread-safe API. */
    juce::MemoryBlock getStateForSlot(int slotIndex) const;

private:
    struct Slot
    {
        std::atomic<juce::AudioPluginInstance*> active { nullptr };
        juce::PluginDescription description;
    };

    std::array<Slot, (size_t) numSlots> slots_;
    std::atomic<bool> bypassed_ { false };
    std::atomic<bool> overBudget_ { false };

    double sampleRate_    = 44100.0;
    int    maxBlockSize_  = 512;
    int    numChannels_   = 2;

    static void deleteOnMessageThread(juce::AudioPluginInstance* instance);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ChannelPluginChain)
};
