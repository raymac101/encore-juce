/*
  ==============================================================================

    LibVlcVideoView.cpp

  ==============================================================================
*/

#include "LibVlcVideoView.h"

#if JUCE_WINDOWS
 #include <windows.h>
 #include <vlc/vlc.h>
#endif

#if JUCE_WINDOWS

namespace
{
    constexpr wchar_t kWindowClassName[] = L"EncoreLibVlcHost";

    // Registered once per process; DefWindowProc is all that's needed here --
    // libvlc's "drawable" video output paints into this window itself once
    // libvlc_media_player_set_hwnd() is called.
    ATOM registerWindowClassOnce()
    {
        static const ATOM atom = []() -> ATOM
        {
            WNDCLASSEXW wc{};
            wc.cbSize        = sizeof (wc);
            wc.style         = CS_HREDRAW | CS_VREDRAW;
            wc.lpfnWndProc   = DefWindowProcW;
            wc.hInstance     = (HINSTANCE) juce::Process::getCurrentModuleInstanceHandle();
            wc.hbrBackground = (HBRUSH) GetStockObject (BLACK_BRUSH);
            wc.lpszClassName = kWindowClassName;
            return RegisterClassExW (&wc);
        }();

        return atom;
    }

    // One instance for the whole process, created lazily and never released
    // (same "create once, let the OS reclaim at process exit" precedent used
    // for other always-on media subsystems in this codebase). libvlc finds
    // its plugins/ directory automatically next to libvlc.dll.
    libvlc_instance_t* sharedInstance()
    {
        static libvlc_instance_t* instance = []() -> libvlc_instance_t*
        {
            const char* args[] = { "--quiet", "--no-video-title-show", "--no-osd" };
            return libvlc_new ((int) juce::numElementsInArray (args), args);
        }();

        return instance;
    }
}

//==============================================================================
class LibVlcVideoView::Impl
{
public:
    explicit Impl (LibVlcVideoView& owner) : owner_ (owner)
    {
        auto* vlc = sharedInstance();
        if (vlc == nullptr)
            return;

        if (registerWindowClassOnce() == 0)
            return;

        hwnd_ = CreateWindowExW (0, kWindowClassName, L"", WS_POPUP | WS_CLIPCHILDREN,
                                  0, 0, 16, 16, nullptr, nullptr,
                                  (HINSTANCE) juce::Process::getCurrentModuleInstanceHandle(),
                                  nullptr);
        if (hwnd_ == nullptr)
            return;

        hwndHost_.setHWND (hwnd_);
        owner_.addAndMakeVisible (hwndHost_);

        player_ = libvlc_media_player_new (vlc);
        if (player_ == nullptr)
            return;

        libvlc_media_player_set_hwnd (player_, hwnd_);
        libvlc_audio_set_mute (player_, 1);

        eventContext_ = std::make_unique<EventContext>();
        eventContext_->owner = this;

        if (auto* mgr = libvlc_media_player_event_manager (player_))
            libvlc_event_attach (mgr, libvlc_MediaPlayerEncounteredError, &staticEventCallback, eventContext_.get());
    }

    ~Impl()
    {
        if (player_ != nullptr && eventContext_ != nullptr)
        {
            {
                // Neuter the callback before touching anything it might race
                // with -- detach() itself doesn't guarantee a callback isn't
                // already mid-flight on a libvlc thread, so this lock is what
                // actually makes it safe to free eventContext_ afterward.
                const juce::ScopedLock sl (eventContext_->lock);
                eventContext_->owner = nullptr;
            }

            if (auto* mgr = libvlc_media_player_event_manager (player_))
                libvlc_event_detach (mgr, libvlc_MediaPlayerEncounteredError, &staticEventCallback, eventContext_.get());
        }

        if (player_ != nullptr)
        {
            libvlc_media_player_stop (player_);
            libvlc_media_player_release (player_);
        }
    }

    bool isAvailable() const noexcept { return player_ != nullptr; }

    void play (const juce::File& file)
    {
        if (player_ == nullptr)
            return;

        auto* media = libvlc_media_new_path (sharedInstance(), file.getFullPathName().toRawUTF8());
        if (media == nullptr)
            return;

        // The standard libvlc idiom for looping a single item -- there is no
        // simple "loop forever" flag on the player itself.
        libvlc_media_add_option (media, ":input-repeat=65535");

        // Swapping the source on a live player is libvlc's own supported
        // pattern for exactly this "change what's playing in the same
        // window" use case -- unlike IMFMediaEngine, this doesn't leak.
        libvlc_media_player_set_media (player_, media);
        libvlc_media_release (media); // player_ holds its own reference now

        libvlc_media_player_play (player_);
    }

    void stop()
    {
        if (player_ != nullptr)
            libvlc_media_player_stop (player_);
    }

    void resized (juce::Rectangle<int> bounds)
    {
        hwndHost_.setBounds (bounds);
    }

private:
    struct EventContext
    {
        juce::CriticalSection lock;
        Impl* owner = nullptr; // guarded by lock
    };

    static void staticEventCallback (const libvlc_event_t*, void* data)
    {
        auto* ctx = static_cast<EventContext*> (data);
        const auto* raw = libvlc_errmsg();
        juce::String message (raw != nullptr ? raw : "unknown error");

        const juce::ScopedLock sl (ctx->lock);
        if (ctx->owner != nullptr)
            ctx->owner->notifyErrorAsync (message);
    }

    // Called on a libvlc-internal thread; marshals to the message thread
    // before touching any UI state, same background-thread-to-UI-thread
    // pattern used throughout this codebase.
    void notifyErrorAsync (juce::String message)
    {
        juce::Component::SafePointer<LibVlcVideoView> safe (&owner_);

        juce::MessageManager::callAsync ([safe, message]
        {
            if (safe != nullptr)
                juce::Logger::writeToLog ("[LibVlcVideoView] playback error: " + message);
        });
    }

    LibVlcVideoView& owner_;
    juce::HWNDComponent hwndHost_;
    HWND hwnd_ = nullptr;
    libvlc_media_player_t* player_ = nullptr;
    std::unique_ptr<EventContext> eventContext_;
};

#else

class LibVlcVideoView::Impl
{
public:
    explicit Impl (LibVlcVideoView&) {}

    bool isAvailable() const noexcept { return false; }
    void play (const juce::File&) {}
    void stop() {}
    void resized (juce::Rectangle<int>) {}
};

#endif

//==============================================================================
LibVlcVideoView::LibVlcVideoView()
    : impl_ (std::make_unique<Impl> (*this))
{
    setInterceptsMouseClicks (false, false);
    setOpaque (true);
}

LibVlcVideoView::~LibVlcVideoView() = default;

bool LibVlcVideoView::isAvailable() const noexcept
{
    return impl_->isAvailable();
}

void LibVlcVideoView::play (const juce::File& mediaFile)
{
    if (! mediaFile.existsAsFile())
    {
        stop();
        return;
    }

    current_ = mediaFile;
    impl_->play (mediaFile);
}

void LibVlcVideoView::stop()
{
    current_ = juce::File{};
    impl_->stop();
}

void LibVlcVideoView::resized()
{
    impl_->resized (getLocalBounds());
}
