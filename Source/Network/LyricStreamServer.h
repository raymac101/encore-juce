/*
  ==============================================================================

    LyricStreamServer.h

    Serves the singer-facing lyric screen to phones/tablets on the venue WiFi
    as a plain web page, so any device with a browser -- iPad, iPhone,
    Android -- can follow along without a companion app or OS-level screen
    mirroring (which only really works Apple-to-Apple). LyricDisplayComponent
    periodically hands this server a snapshot of what it's currently
    painting; connected phones poll GET /frame for the latest image.

    Off by default -- only runs while explicitly started from the "Phone
    Lyrics" control in BottomBar. This is an unauthenticated, LAN-only HTTP
    server: acceptable here because it only ever serves read-only lyric
    images (no state-changing endpoints), but it is reachable by anyone on
    the same venue WiFi while active.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include <atomic>
#include <map>

class LyricStreamServer : private juce::Thread
{
public:
    LyricStreamServer();
    ~LyricStreamServer() override;

    /** Starts listening on a background thread. Safe to call if already
        running (no-op). Returns true if a listening socket was bound. */
    bool start();

    /** Stops listening and lets any in-flight connections finish. */
    void stop();

    bool isRunning() const noexcept { return listening_.load(); }

    /** The URL phones should open, e.g. "http://192.168.1.42:8791/".
        Empty if not running or no suitable LAN address could be found. */
    juce::String getUrl() const;

    /** Distinct client addresses that have polled /frame in the last few
        seconds -- a proxy for "how many phones are currently watching",
        since each poll is a short-lived connection rather than something
        that stays open. */
    int getConnectedClientCount() const;

    /** Called from the message thread with the latest rendered lyric-screen
        image. PNG-encodes and caches it once, regardless of how many phones
        are currently polling. */
    void pushFrame (const juce::Image& image);

    /** Same as pushFrame(), but renders a centred text message instead of a
        screenshot -- used while nothing is loaded yet, or while a video-based
        song is playing (not supported for phone streaming; see CLAUDE.md's
        CDG-only v1 scope). */
    void pushPlaceholder (const juce::String& message);

private:
    void run() override; // accept loop
    void handleConnection (juce::StreamingSocket* rawSocket);

    static juce::String readRequestLine (juce::StreamingSocket& socket);
    static void writeResponse (juce::StreamingSocket& socket, int statusCode, const juce::String& statusText,
                                const juce::String& contentType, const void* body, size_t bodyBytes);
    void noteClientSeen (const juce::String& clientKey);

    std::unique_ptr<juce::StreamingSocket> listener_;
    std::atomic<bool> listening_ { false };
    int boundPort_ = 0;

    juce::CriticalSection frameLock_;
    juce::MemoryBlock latestFramePng_;

    mutable juce::CriticalSection recentClientsLock_;
    std::map<juce::String, juce::int64> recentClientLastSeenMs_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LyricStreamServer)
};
