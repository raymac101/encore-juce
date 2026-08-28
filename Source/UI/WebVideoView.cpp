/*
  ==============================================================================

    WebVideoView.cpp

  ==============================================================================
*/

#include "WebVideoView.h"

#if JUCE_WINDOWS
 #include <windows.h>
#endif

namespace
{
    juce::File webViewUserDataFolder()
    {
        auto dir = juce::File::getSpecialLocation (juce::File::tempDirectory)
                       .getChildFile ("EncoreKaraoke")
                       .getChildFile ("webview2");
        dir.createDirectory();
        return dir;
    }

    void enableEmbeddedAutoplayOnce()
    {
       #if JUCE_WINDOWS
        // WebView2's default autoplay policy is document-user-activation-required,
        // which blocks even muted autoplay -- a lyric screen never gets a click.
        // WebView2 reads this env var at environment-creation time when the
        // options object doesn't already carry AdditionalBrowserArguments (JUCE's
        // WinWebView2 options don't expose them).
        static bool done = false;
        if (! done)
        {
            SetEnvironmentVariableW (L"WEBVIEW2_ADDITIONAL_BROWSER_ARGUMENTS",
                                     L"--autoplay-policy=no-user-gesture-required");
            done = true;
        }
       #endif
    }

    juce::WebBrowserComponent::Options makeOptions()
    {
        using Options = juce::WebBrowserComponent::Options;

        return Options{}
           #if JUCE_WINDOWS
            .withBackend (Options::Backend::webview2)
            .withWinWebView2Options (Options::WinWebView2{}
                                         .withUserDataFolder (webViewUserDataFolder())
                                         .withBackgroundColour (juce::Colours::black)
                                         .withStatusBarDisabled()
                                         .withBuiltInErrorPageDisabled())
           #endif
            .withKeepPageLoadedWhenBrowserIsHidden();
    }

    // A relative filename, percent-escaped so it is a valid <video src> in the
    // player page that sits in the same directory as the clip.
    juce::String escapedRelativeSrc (const juce::File& mediaFile)
    {
        return juce::URL::addEscapeChars (mediaFile.getFileName(), false)
                   .replace ("&", "%26")
                   .replace ("\"", "%22")
                   .replace ("'", "%27");
    }

    juce::String playerPageHtml (const juce::String& escapedSrc)
    {
        return R"(<!doctype html><html><head><meta charset="utf-8"><style>
 html,body{margin:0;padding:0;width:100%;height:100%;background:#000;overflow:hidden}
 #v{position:fixed;inset:0;width:100%;height:100%;object-fit:contain;background:#000}
</style></head><body>
<video id="v" src=")" + escapedSrc + R"(" autoplay muted loop playsinline preload="auto"></video>
<script>
 var v=document.getElementById('v');
 function go(){try{var p=v.play();if(p&&p.catch){p.catch(function(){setTimeout(go,250);});}}catch(e){setTimeout(go,250);}}
 v.addEventListener('canplay',go);
 v.addEventListener('ended',function(){v.currentTime=0;go();});
 v.addEventListener('error',function(){document.title='advideo-error-'+(v.error?v.error.code:'?');});
 go();
</script></body></html>)";
    }

    //==============================================================================
    class DiagnosticWebBrowser : public juce::WebBrowserComponent
    {
    public:
        explicit DiagnosticWebBrowser (const Options& o) : juce::WebBrowserComponent (o) {}

        void pageFinishedLoading (const juce::String& url) override
        {
            juce::Logger::writeToLog ("[WebVideoView] page loaded: " + url);
        }

        bool pageLoadHadNetworkError (const juce::String& errorInfo) override
        {
            juce::Logger::writeToLog ("[WebVideoView] page load ERROR: " + errorInfo);
            return true;
        }
    };
}

//==============================================================================
WebVideoView::WebVideoView()
{
    setInterceptsMouseClicks (false, false);
    setOpaque (true);

    enableEmbeddedAutoplayOnce();

    const auto options = makeOptions();

    if (! juce::WebBrowserComponent::areOptionsSupported (options))
    {
        juce::Logger::writeToLog ("[WebVideoView] WebView2 backend not supported on this machine");
        return;
    }

    browser_ = std::make_unique<DiagnosticWebBrowser> (options);
    browser_->setWantsKeyboardFocus (false);
    addAndMakeVisible (*browser_);
}

WebVideoView::~WebVideoView() = default;

void WebVideoView::play (const juce::File& mediaFile)
{
    if (browser_ == nullptr)
        return;

    if (! mediaFile.existsAsFile())
    {
        stop();
        return;
    }

    current_ = mediaFile;

    // The player page lives beside the clip so a file:// origin can load it as
    // a same-directory subresource. Rewritten per clip; a nav-count query
    // param defeats WebView2's page cache so re-selecting the same ad restarts
    // it cleanly.
    auto page = mediaFile.getParentDirectory().getChildFile ("_adplayer.html");
    page.replaceWithText (playerPageHtml (escapedRelativeSrc (mediaFile)));

    const auto url = juce::URL (page).toString (true) + "?_=" + juce::String (++navCounter_);
    juce::Logger::writeToLog ("[WebVideoView] navigating to: " + url);
    browser_->goToURL (url);
}

void WebVideoView::stop()
{
    current_ = juce::File{};

    if (browser_ != nullptr)
        browser_->goToURL ("about:blank");
}

void WebVideoView::resized()
{
    if (browser_ != nullptr)
        browser_->setBounds (getLocalBounds());
}
