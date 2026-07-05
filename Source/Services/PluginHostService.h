/*
  ==============================================================================

    PluginHostService.h

    Discovers installed VST3 plugins for the Mixer page's plugin-chain
    pickers (see ChannelPluginChain). Two responsibilities:

      1. Scanning — background scan of the standard OS VST3 folders. Each
         candidate plugin file's metadata is read in an isolated
         re-invocation of this same executable as a child process
         (--scan-plugin=<path> --scan-output=<path>), so a crashing or
         malformed plugin can only kill that disposable scan child, never
         the running app. This mirrors JUCE's own AudioPluginHost example.
         Results are cached to disk (plugin-cache.xml) so a rescan isn't
         needed on every launch.

      2. Instantiation — exposes the shared juce::AudioPluginFormatManager
         that ChannelPluginChain uses to actually load a chosen plugin into
         a mixer channel (a separate step from scanning; scanning never
         instantiates a plugin for real, only reads its metadata).

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include <functional>

class PluginHostService
{
public:
    static PluginHostService& getInstance();

    using ProgressCallback   = std::function<void(float progress01, juce::String currentItem)>;
    using CompletionCallback = std::function<void(int numFound)>;

    /** Starts a background scan of the standard OS VST3 folders. No-op if a
        scan is already running. onProgress/onComplete are always called on
        the message thread. */
    void scanForPlugins(ProgressCallback onProgress, CompletionCallback onComplete);

    bool isScanning() const noexcept { return scanning_.load(); }
    juce::Time getLastScanTime() const noexcept { return lastScanTime_; }

    /** The plugins found by the most recent scan (or loaded from cache at
        startup). */
    juce::Array<juce::PluginDescription> getAvailablePlugins() const;

    /** Shared format manager used to actually load a plugin into a
        ChannelPluginChain slot. */
    juce::AudioPluginFormatManager& getFormatManager() noexcept { return formatManager_; }

    //==========================================================================
    /** Call from Main.cpp's initialise(), as the very first thing, before
        any login/venue/network bootstrap. If this process was launched with
        --scan-plugin=<path> --scan-output=<path>, reads that one plugin
        file's metadata (no audio instantiation — just findAllTypesForFile,
        which itself is the risky part that might crash for a malformed
        plugin) and writes the result as XML to the output path. Returns
        true if this was a scan-child invocation (the caller should quit()
        immediately without touching normal startup), false for a normal
        launch. */
    static bool handleScanCommandLineIfPresent();

private:
    PluginHostService();
    ~PluginHostService() = default;

    void loadCache();
    void saveCache() const;
    static juce::File getCacheFile();
    static juce::File getScanTempDirectory();

    juce::AudioPluginFormatManager formatManager_;
    juce::KnownPluginList knownPlugins_;
    std::atomic<bool> scanning_ { false };
    juce::Time lastScanTime_;

    JUCE_DECLARE_NON_COPYABLE(PluginHostService)
};
