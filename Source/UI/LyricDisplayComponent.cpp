/*
  ==============================================================================

    LyricDisplayComponent.cpp

  ==============================================================================
*/

#include "LyricDisplayComponent.h"
#include "../Audio/AudioEngine.h"
#include "../Firebase/FirebaseConfig.h"
#include "../Services/FirestoreClient.h"
#include "../Services/ImageCache.h"
#include "../Services/UserPreferences.h"

#include <algorithm>
#include <cmath>
#include <map>

namespace
{
    juce::String trimTrailingSlash (juce::String s)
    {
        while (s.endsWithChar ('/'))
            s = s.dropLastCharacters (1);
        return s;
    }

    juce::var httpGetJson (const juce::URL& url,
                           const juce::StringArray& headers,
                           int* httpStatus)
    {
        juce::StringPairArray responseHeaders;
        auto options = juce::URL::InputStreamOptions (juce::URL::ParameterHandling::inAddress)
                           .withConnectionTimeoutMs (10000)
                           .withNumRedirectsToFollow (5)
                           .withExtraHeaders (headers.joinIntoString ("\r\n"))
                           .withResponseHeaders (&responseHeaders)
                           .withStatusCode (httpStatus);

        if (auto stream = url.createInputStream (options))
            return juce::JSON::parse (stream->readEntireStreamAsString());

        return {};
    }

    void addDurationHint (std::map<juce::String, int>& hints,
                          const juce::String& key,
                          int durationSec)
    {
        auto k = key.trim().toLowerCase();
        if (k.isNotEmpty() && durationSec > 0)
            hints[k] = durationSec;
    }

    std::vector<LyricDisplayComponent::AdEntry> fetchVenueAds (const juce::String& venueId)
    {
        std::vector<LyricDisplayComponent::AdEntry> out;
        if (venueId.isEmpty())
            return out;

        std::map<juce::String, int> durationHints;

        // Optional metadata: venues/<venueId>/Ads (duration overrides, direct URLs).
        {
            const auto docs = FirestoreClient::getInstance().listCollection ("venues/" + venueId + "/Ads", 200);
            for (const auto& doc : docs)
            {
                const int duration = (int) FirestoreClient::readInt (doc, "duration", 0);
                addDurationHint (durationHints, FirestoreClient::readString (doc, "name"), duration);
                addDurationHint (durationHints, FirestoreClient::readString (doc, "fileName"), duration);
                addDurationHint (durationHints, FirestoreClient::readString (doc, "path"), duration);
                addDurationHint (durationHints, FirestoreClient::readString (doc, "mediaUrl"), duration);
                addDurationHint (durationHints, FirestoreClient::readString (doc, "url"), duration);

                const auto mediaUrl = FirestoreClient::readString (doc, "mediaUrl");
                if (mediaUrl.isEmpty())
                    continue;

                LyricDisplayComponent::AdEntry ad;
                ad.name = FirestoreClient::readString (doc, "name");
                ad.url = mediaUrl;
                ad.mimeType = FirestoreClient::readString (doc, "mediaType");
                if (ad.mimeType.isEmpty())
                    ad.mimeType = FirestoreClient::readString (doc, "type");
                ad.durationSec = juce::jmax (1, duration > 0 ? duration : 10);
                out.push_back (ad);
            }
        }

        // Canonical source: Firebase Storage folder Venues/<venueId>/Ads/.
        const juce::String prefix = "Venues/" + venueId + "/Ads/";
        const juce::String listUrl = "https://firebasestorage.googleapis.com/v0/b/"
                                   + FirebaseConfig::storageBucket
                                   + "/o?prefix="
                                   + juce::URL::addEscapeChars (prefix, true);

        juce::StringArray headers;
        const auto token = FirestoreClient::getInstance().getIdToken();
        if (token.isNotEmpty())
            headers.add ("Authorization: Bearer " + token);

        int status = 0;
        const auto listJson = httpGetJson (juce::URL (listUrl), headers, &status);

        if (status >= 200 && status < 300 && listJson.isObject())
        {
            const auto items = listJson.getProperty ("items", juce::var());
            if (auto* arr = items.getArray())
            {
                for (const auto& item : *arr)
                {
                    const auto objectPath = item.getProperty ("name", juce::String()).toString();
                    if (objectPath.isEmpty() || objectPath.endsWithChar ('/'))
                        continue;

                    LyricDisplayComponent::AdEntry ad;
                    ad.name = juce::File (objectPath).getFileName();
                    ad.mimeType = item.getProperty ("contentType", juce::String()).toString();

                    juce::String tokenValue;
                    auto tokenList = item.getProperty ("downloadTokens", juce::String()).toString();
                    if (tokenList.isNotEmpty())
                        tokenValue = tokenList.upToFirstOccurrenceOf (",", false, false);

                    ad.url = "https://firebasestorage.googleapis.com/v0/b/"
                           + FirebaseConfig::storageBucket
                           + "/o/"
                           + juce::URL::addEscapeChars (objectPath, true)
                           + "?alt=media";
                    if (tokenValue.isNotEmpty())
                        ad.url << "&token=" << juce::URL::addEscapeChars (tokenValue, true);

                    const auto hintByName = durationHints.find (ad.name.toLowerCase());
                    const auto hintByPath = durationHints.find (objectPath.toLowerCase());
                    const auto hintByUrl  = durationHints.find (ad.url.toLowerCase());
                    int duration = 0;
                    if (hintByName != durationHints.end()) duration = hintByName->second;
                    if (hintByPath != durationHints.end()) duration = hintByPath->second;
                    if (hintByUrl  != durationHints.end()) duration = hintByUrl->second;

                    ad.durationSec = juce::jmax (1, duration > 0 ? duration : (ad.isVideo() ? 8 : 10));

                    const bool already = std::any_of (out.begin(), out.end(),
                                                      [&] (const LyricDisplayComponent::AdEntry& existing)
                                                      {
                                                          return trimTrailingSlash (existing.url) == trimTrailingSlash (ad.url);
                                                      });
                    if (! already)
                        out.push_back (ad);
                }
            }
        }

        std::sort (out.begin(), out.end(), [] (const LyricDisplayComponent::AdEntry& a,
                                               const LyricDisplayComponent::AdEntry& b)
        {
            return a.name.compareIgnoreCase (b.name) < 0;
        });

        return out;
    }
}

//==============================================================================
LyricDisplayComponent::LyricDisplayComponent()
{
    setOpaque (true);

    // Try to load the Encore logo for the idle screen. It is optional — the
    // component falls back to a text-only idle display if the file is
    // missing.
    const auto appDir = juce::File::getSpecialLocation (juce::File::currentExecutableFile)
                            .getParentDirectory();
    const auto logoFile = appDir.getChildFile ("assets/images/encore-logo-white.png");
    if (logoFile.existsAsFile())
        logoImage_ = juce::ImageFileFormat::loadFrom (logoFile);

    // 30 Hz is plenty — CDG updates at most at 300 packets/sec but the
    // perceivable lyric motion is much slower.
    startTimerHz (30);

    refreshAdsAsync (true);
}

LyricDisplayComponent::~LyricDisplayComponent()
{
    stopIdleAdVideo();
    stopTimer();
}

//==============================================================================
void LyricDisplayComponent::setAudioEngine (AudioEngine* engine)
{
    audioEngine_ = engine;
}

void LyricDisplayComponent::loadCDG (const juce::File& cdgFile)
{
    // Loading a CDG implies we're not playing a video any more.
    stopVideo();

    loadedFile_ = cdgFile;

    if (cdgFile == juce::File{} || ! cdgFile.existsAsFile())
    {
        decoder_.clear();
        repaint();
        return;
    }

    if (! decoder_.loadFile (cdgFile))
    {
        DBG ("LyricDisplay: failed to load CDG: " + cdgFile.getFullPathName());
        decoder_.clear();
    }

    repaint();
}

void LyricDisplayComponent::setVenueContext (const juce::String& venueId,
                                             const juce::String& venueName)
{
    const bool changed = (venueId_ != venueId || venueName_ != venueName);
    venueId_ = venueId;
    venueName_ = venueName;

    if (changed)
        refreshAdsAsync (true);

    repaint();
}

void LyricDisplayComponent::setQueuePreview (const std::vector<QueuePreviewEntry>& entries)
{
    queuePreview_ = entries;
    repaint();
}

void LyricDisplayComponent::setForceIdleScreen (bool shouldForce)
{
    forceIdleScreen_ = shouldForce;

    if (videoComponent_ != nullptr && videoComponent_->isVideoOpen())
    {
        videoComponent_->setVisible (! shouldForce);
        if (! shouldForce)
            layoutVideoBounds();
    }

    repaint();
}

//==============================================================================
bool LyricDisplayComponent::loadVideo (const juce::File& videoFile, bool autoPlay)
{
    // Switching to video clears any CDG that was active.
    decoder_.clear();
    loadedFile_ = juce::File{};

    if (videoFile == juce::File{} || ! videoFile.existsAsFile())
    {
        stopVideo();
        return false;
    }

    if (videoComponent_ == nullptr)
    {
        // useNativeControlsIfAvailable=false: we drive playback ourselves so
        // the singer-facing screen stays clean (no pause bar/scrubber).
        videoComponent_ = std::make_unique<juce::VideoComponent> (false);
        addAndMakeVisible (videoComponent_.get());
    }

    auto result = videoComponent_->load (videoFile);
    if (result.failed())
    {
        DBG ("LyricDisplay: failed to load video: " + videoFile.getFullPathName()
              + " (" + result.getErrorMessage() + ")");
        stopVideo();
        return false;
    }

    layoutVideoBounds();
    videoComponent_->setVisible (true);
    if (autoPlay)
        videoComponent_->play();
    else
    {
        videoComponent_->stop();
        videoComponent_->setPlayPosition (0.0);
    }
    repaint();
    return true;
}

void LyricDisplayComponent::stopVideo()
{
    if (videoComponent_ != nullptr)
    {
        videoComponent_->stop();
        videoComponent_->closeVideo();
        videoComponent_->setVisible (false);
    }
    repaint();
}

void LyricDisplayComponent::playVideo()
{
    if (videoComponent_ != nullptr && videoComponent_->isVideoOpen())
    {
        videoComponent_->setVisible (true);
        layoutVideoBounds();
        videoComponent_->play();
    }
}

void LyricDisplayComponent::pauseVideo()
{
    if (videoComponent_ != nullptr && videoComponent_->isVideoOpen())
        videoComponent_->stop();
}

void LyricDisplayComponent::seekVideo (double positionSeconds)
{
    if (videoComponent_ != nullptr && videoComponent_->isVideoOpen())
        videoComponent_->setPlayPosition (juce::jmax (0.0, positionSeconds));
}

bool LyricDisplayComponent::isVideoActive() const
{
    return videoComponent_ != nullptr && videoComponent_->isVideoOpen();
}

double LyricDisplayComponent::getVideoPosition() const
{
    return isVideoActive() ? videoComponent_->getPlayPosition() : 0.0;
}

double LyricDisplayComponent::getVideoDuration() const
{
    return isVideoActive() ? videoComponent_->getVideoDuration() : 0.0;
}

void LyricDisplayComponent::setNextSinger (const juce::String& singerName,
                                           const juce::String& songName,
                                           const juce::String& artist)
{
    nextSinger_ = singerName;
    nextSong_   = songName;
    nextArtist_ = artist;
    repaint();
}

//==============================================================================
void LyricDisplayComponent::timerCallback()
{
    if (forceIdleScreen_)
    {
        // The next song may be preloaded while transport is stopped. Keep the
        // idle screen visible until playback actually starts again.
        if (audioEngine_ != nullptr && audioEngine_->isPlaying())
            forceIdleScreen_ = false;
    }

    const bool idleMode = forceIdleScreen_ || (! isVideoActive() && ! decoder_.isLoaded());
    updateAdPanelAnimation (idleMode);

    if (++adRefreshCountdownFrames_ >= 900)
    {
        adRefreshCountdownFrames_ = 0;
        refreshAdsAsync();
    }

    if (! ads_.empty())
    {
        if (currentAdIndex_ < 0 || currentAdIndex_ >= (int) ads_.size())
        {
            advanceIdleAd (true);
        }
        else
        {
            const auto& current = ads_[(size_t) currentAdIndex_];

            if (current.isVideo())
            {
                if (idleAdVideoComponent_ != nullptr && idleAdVideoComponent_->isVideoOpen())
                {
                    // Some backends can reset volume when media state changes,
                    // so enforce visual-only ads during playback.
                    idleAdVideoComponent_->setAudioVolume (0.0f);

                    const auto dur = idleAdVideoComponent_->getVideoDuration();
                    const auto pos = idleAdVideoComponent_->getPlayPosition();
                    if (dur > 0.2 && pos >= dur - 0.1)
                        advanceIdleAd();
                }
            }
            else
            {
                adRemainingMs_ -= 33;
                if (adRemainingMs_ <= 0)
                    advanceIdleAd();
            }
        }
    }

    // While a video is playing the VideoComponent paints itself; we only
    // need to keep the overlays current.
    if (isVideoActive())
    {
        repaint();
        return;
    }

    if (forceIdleScreen_ || ! decoder_.isLoaded() || audioEngine_ == nullptr)
    {
        repaint();
        return;
    }

    const double pos = audioEngine_->getCurrentPosition();
    decoder_.renderAt (pos);
    repaint();
}

//==============================================================================
void LyricDisplayComponent::resized()
{
    layoutVideoBounds();
    layoutIdleAdVideoBounds (getLocalBounds());
}

void LyricDisplayComponent::layoutVideoBounds()
{
    if (videoComponent_ == nullptr)
        return;

    auto area = getContentRenderArea (getPrimaryRenderArea (getLocalBounds(), false));

    if (! videoComponent_->isVideoOpen())
    {
        videoComponent_->setBounds (area);
        return;
    }

    // Letterbox the video inside our bounds, preserving its native aspect.
    auto native = videoComponent_->getVideoNativeSize();
    if (native.getWidth() <= 0 || native.getHeight() <= 0)
    {
        videoComponent_->setBounds (area);
        return;
    }

    const float srcW  = (float) native.getWidth();
    const float srcH  = (float) native.getHeight();
    const float scale = juce::jmin ((float) area.getWidth()  / srcW,
                                    (float) area.getHeight() / srcH);
    const int   w = (int) std::round (srcW * scale);
    const int   h = (int) std::round (srcH * scale);

    videoComponent_->setBounds (area.withSizeKeepingCentre (w, h));
}

void LyricDisplayComponent::paint (juce::Graphics& g)
{
    auto area = getLocalBounds();
    const bool idleMode = forceIdleScreen_ || (! isVideoActive() && ! decoder_.isLoaded());

    // Solid backdrop — matches the Angular theme's --background-color (#393939).
    g.fillAll (juce::Colour (0xff393939));

    // The VideoComponent (when active) paints itself as a child component, so
    // skip CDG/idle painting and only draw the overlays on top.
    if (forceIdleScreen_)
    {
        paintIdle (g, getContentRenderArea (area));
    }
    else if (! isVideoActive())
    {
        if (decoder_.isLoaded())
            paintCdg (g, getContentRenderArea (getPrimaryRenderArea (area, false)));
        else
            paintIdle (g, getContentRenderArea (area));
    }

    if (! idleMode && adPanelVisibility_ > 0.01f)
        paintAdPanel (g, getContentRenderArea (getAdRenderArea (area, false)), true);

    paintOverlay (g, idleMode ? area : getPrimaryRenderArea (area, false));
}

void LyricDisplayComponent::paintIdle (juce::Graphics& g, juce::Rectangle<int> area)
{
    auto left = area.removeFromLeft (area.getWidth() / 2);
    auto right = area;

    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff4c4c4c),
                                             (float) left.getX(), (float) left.getY(),
                                             juce::Colour (0xff2b2b2b),
                                             (float) left.getRight(), (float) left.getBottom(),
                                             false));
    g.fillRect (left);

    g.setColour (juce::Colours::black.withAlpha (0.22f));
    g.fillRect (right);

    auto brandArea = left.removeFromTop ((int) (left.getHeight() * 0.52f)).reduced (28, 24);
    auto queueArea = left.reduced (28, 14);

    if (logoImage_.isValid())
    {
        auto logoArea = brandArea.removeFromTop ((int) (brandArea.getHeight() * 0.70f));
        g.drawImage (logoImage_, logoArea.toFloat(),
                     juce::RectanglePlacement::centred | juce::RectanglePlacement::onlyReduceInSize);
    }

    const auto venueLabel = venueName_.isNotEmpty() ? venueName_ : juce::String ("Encore Karaoke");
    g.setColour (juce::Colours::white);
    g.setFont (juce::Font (juce::FontOptions().withHeight (juce::jmax (30.0f, brandArea.getHeight() * 0.20f))).boldened());
    g.drawFittedText (venueLabel, brandArea, juce::Justification::centredTop, 2);

    g.setColour (juce::Colour (0xff30daff));
    g.setFont (juce::Font (juce::FontOptions().withHeight (22.0f)).boldened());
    auto queueTitle = queueArea.removeFromTop (36);
    g.drawText ("Next Up", queueTitle, juce::Justification::centredLeft, true);

    if (queuePreview_.empty())
    {
        g.setColour (juce::Colours::white.withAlpha (0.65f));
        g.setFont (juce::Font (juce::FontOptions().withHeight (20.0f)));
        g.drawFittedText ("Queue is waiting for singers", queueArea, juce::Justification::centred, 2);
    }
    else
    {
        const int rowH = juce::jmax (72, queueArea.getHeight() / 3);
        for (int i = 0; i < (int) queuePreview_.size() && i < 3; ++i)
        {
            auto row = queueArea.removeFromTop (rowH).reduced (0, 6);

            g.setColour (juce::Colours::black.withAlpha (0.35f));
            g.fillRoundedRectangle (row.toFloat(), 8.0f);

            auto text = row.reduced (12, 10);
            g.setColour (juce::Colour (0xff30daff));
            g.setFont (juce::Font (juce::FontOptions().withHeight (16.0f)).boldened());
            g.drawText (juce::String (i + 1), text.removeFromLeft (26), juce::Justification::centred);

            g.setColour (juce::Colours::white);
            g.setFont (juce::Font (juce::FontOptions().withHeight (20.0f)).boldened());
            g.drawText (queuePreview_[(size_t) i].singerName, text.removeFromTop (24), juce::Justification::centredLeft, true);

            juce::String songLine = queuePreview_[(size_t) i].songName;
            if (queuePreview_[(size_t) i].artistName.isNotEmpty())
                songLine << " - " << queuePreview_[(size_t) i].artistName;

            g.setColour (juce::Colours::white.withAlpha (0.78f));
            g.setFont (juce::Font (juce::FontOptions().withHeight (16.0f)));
            g.drawFittedText (songLine, text, juce::Justification::centredLeft, 1);
        }
    }

    paintAdPanel (g, right, false);
}

void LyricDisplayComponent::paintAdPanel (juce::Graphics& g, juce::Rectangle<int> area, bool addFrame)
{
    if (area.getWidth() <= 8 || area.getHeight() <= 8)
        return;

    auto adArea = area.reduced (18);
    g.setColour (juce::Colours::black.withAlpha (0.42f));
    g.fillRoundedRectangle (adArea.toFloat(), 12.0f);

    if (addFrame)
    {
        g.setColour (juce::Colours::white.withAlpha (0.10f));
        g.drawRoundedRectangle (adArea.toFloat().reduced (0.5f), 12.0f, 1.0f);
    }

    if (currentAdIndex_ >= 0 && currentAdIndex_ < (int) ads_.size())
    {
        const auto& currentAd = ads_[(size_t) currentAdIndex_];
        if (! currentAd.isVideo() && currentAdImage_.isValid())
        {
            g.drawImage (currentAdImage_, adArea.toFloat().reduced (10.0f),
                         juce::RectanglePlacement::centred | juce::RectanglePlacement::onlyReduceInSize);
        }

        g.setColour (juce::Colours::white.withAlpha (0.75f));
        g.setFont (juce::Font (juce::FontOptions().withHeight (16.0f)));
        g.drawText (currentAd.name.isNotEmpty() ? currentAd.name : "Sponsored", adArea.reduced (14),
                    juce::Justification::bottomLeft, true);
    }
    else
    {
        g.setColour (juce::Colours::white.withAlpha (0.60f));
        g.setFont (juce::Font (juce::FontOptions().withHeight (22.0f)));
        g.drawFittedText ("Ads will appear here", adArea.reduced (24), juce::Justification::centred, 2);
    }
}

void LyricDisplayComponent::paintCdg (juce::Graphics& g, juce::Rectangle<int> area)
{
    const auto& img = decoder_.getImage();
    if (! img.isValid())
        return;

    // CD+G is 300x216 which matches a 25:18 ≈ 1.39:1 aspect ratio. Scale to
    // fit while preserving aspect ratio, centred in the window.
    const float srcW = (float) CDGDecoder::kScreenWidth;
    const float srcH = (float) CDGDecoder::kScreenHeight;
    const float dstScale = juce::jmin ((float) area.getWidth()  / srcW,
                                       (float) area.getHeight() / srcH);

    const float w = srcW * dstScale;
    const float h = srcH * dstScale;
    const float x = area.getX() + (area.getWidth()  - w) * 0.5f;
    const float y = area.getY() + (area.getHeight() - h) * 0.5f;

    // Apply CDG smooth-scroll offset (in source pixels, scaled to dst).
    const float hOff = (float) decoder_.getHorizontalOffset() * dstScale;
    const float vOff = (float) decoder_.getVerticalOffset()   * dstScale;

    g.drawImage (img,
                 x - hOff, y - vOff, w, h,
                 0, 0, (int) srcW, (int) srcH,
                 false);
}

void LyricDisplayComponent::paintOverlay (juce::Graphics& g, juce::Rectangle<int> area)
{
    // --- Next singer banner (top-right) ---
    if (nextSinger_.isNotEmpty())
    {
        const int bannerW = juce::jmin (area.getWidth() / 3, 520);
        const int bannerH = 110;
        auto banner = juce::Rectangle<int> (area.getRight() - bannerW - 24,
                                            area.getY() + 24,
                                            bannerW, bannerH);

        g.setColour (juce::Colour (0xdd000000));
        g.fillRoundedRectangle (banner.toFloat(), 8.0f);
        g.setColour (juce::Colour (0xff30daff));
        g.drawRoundedRectangle (banner.toFloat().reduced (0.5f), 8.0f, 1.5f);

        auto text = banner.reduced (14, 10);

        g.setColour (juce::Colour (0xff30daff));
        g.setFont (juce::Font (juce::FontOptions().withHeight (16.0f)).boldened());
        g.drawText ("NEXT UP", text.removeFromTop (20),
                    juce::Justification::centredLeft);

        g.setColour (juce::Colours::white);
        g.setFont (juce::Font (juce::FontOptions().withHeight (22.0f)).boldened());
        g.drawText (nextSinger_, text.removeFromTop (28),
                    juce::Justification::centredLeft);

        if (nextSong_.isNotEmpty() || nextArtist_.isNotEmpty())
        {
            g.setColour (juce::Colours::white.withAlpha (0.85f));
            g.setFont (juce::Font (juce::FontOptions().withHeight (18.0f)));
            juce::String line = nextSong_;
            if (nextArtist_.isNotEmpty())
                line += (line.isNotEmpty() ? "  —  " : "") + nextArtist_;
            g.drawText (line, text, juce::Justification::centredLeft);
        }
    }

    // --- Venue code (bottom-centre lower-third, full-width black bar) ---
    if (venueCode_.isNotEmpty())
    {
        const int codeH = juce::jmax (90, area.getHeight() / 9);
        auto stripe = area.withTop (area.getBottom() - codeH);

        // Solid black bar across the full width of the lyric screen.
        g.setColour (juce::Colours::black);
        g.fillRect (stripe);

        auto inner = stripe.reduced (24, 10);

        // Auto-scale text to the stripe height.
        const float promptSize = juce::jmax (18.0f, inner.getHeight() * 0.32f);
        const float codeSize   = juce::jmax (32.0f, inner.getHeight() * 0.62f);

        // Top line: prompt text in white.
        g.setColour (juce::Colours::white);
        g.setFont (juce::Font (juce::FontOptions().withHeight (promptSize)));
        auto promptArea = inner.removeFromTop ((int) (promptSize * 1.15f));
        g.drawFittedText ("Enter this code to join the karaoke queue",
                          promptArea, juce::Justification::centred, 1);

        // Bottom line: the venue code in the bright highlight colour.
        g.setColour (juce::Colour (0xff30daff));
        g.setFont (juce::Font (juce::FontOptions().withHeight (codeSize)).boldened());
        g.drawFittedText (venueCode_, inner, juce::Justification::centred, 1);
    }
}

void LyricDisplayComponent::setVenueLogo (const juce::Image& logo)
{
    logoImage_ = logo;
    repaint();
}

void LyricDisplayComponent::layoutIdleAdVideoBounds (juce::Rectangle<int> area)
{
    if (idleAdVideoComponent_ == nullptr)
        return;

    const bool idleMode = forceIdleScreen_ || (! isVideoActive() && ! decoder_.isLoaded());
    auto right = getContentRenderArea (getAdRenderArea (area, idleMode)).reduced (28);
    idleAdVideoComponent_->setBounds (right);
    idleAdVideoComponent_->setVisible (right.getWidth() > 60 && right.getHeight() > 60);
}

void LyricDisplayComponent::updateAdPanelAnimation (bool idleMode)
{
    if (idleMode)
    {
        adPanelTarget_ = 1.0f;
    }
    else
    {
        const auto pos = getPlaybackPositionSeconds();
        const auto dur = getPlaybackDurationSeconds();
        const auto leadSec = (double) UserPreferences::getInstance().getLyricAdTransitionLeadSeconds();

        bool showPanel = true;
        if (dur > 0.5)
        {
            const auto showNearEndAt = juce::jmax (0.0, dur - leadSec);
            if (pos > 7.0 && pos < showNearEndAt)
                showPanel = false;
        }

        adPanelTarget_ = showPanel ? 1.0f : 0.0f;
    }

    const float before = adPanelVisibility_;
    adPanelVisibility_ += (adPanelTarget_ - adPanelVisibility_) * 0.18f;
    if (std::abs (adPanelTarget_ - adPanelVisibility_) < 0.01f)
        adPanelVisibility_ = adPanelTarget_;

    if (std::abs (adPanelVisibility_ - before) > 0.001f)
    {
        layoutVideoBounds();
        layoutIdleAdVideoBounds (getLocalBounds());
    }
}

double LyricDisplayComponent::getPlaybackPositionSeconds() const
{
    if (isVideoActive())
        return getVideoPosition();

    if (audioEngine_ != nullptr)
        return audioEngine_->getCurrentPosition();

    return 0.0;
}

double LyricDisplayComponent::getPlaybackDurationSeconds() const
{
    if (isVideoActive())
        return getVideoDuration();

    if (audioEngine_ != nullptr)
    {
        const auto audibleEnd = audioEngine_->getAudibleEndPosition();
        if (audibleEnd > 0.0)
            return audibleEnd;
        return audioEngine_->getTotalLength();
    }

    return 0.0;
}

int LyricDisplayComponent::getAdPanelWidth (juce::Rectangle<int> area, bool idleMode) const
{
    const int fullPanel = area.getWidth() / 2;
    if (idleMode)
        return fullPanel;

    return juce::roundToInt ((float) fullPanel * juce::jlimit (0.0f, 1.0f, adPanelVisibility_));
}

int LyricDisplayComponent::getVenueCodeBarHeight (juce::Rectangle<int> area) const
{
    if (venueCode_.isEmpty())
        return 0;

    const int percent = UserPreferences::getInstance().getLyricVenueCodeBarHeightPercent();
    return juce::jmax (90, juce::roundToInt ((float) area.getHeight() * (float) percent / 100.0f));
}

juce::Rectangle<int> LyricDisplayComponent::getContentRenderArea (juce::Rectangle<int> area) const
{
    const int barH = getVenueCodeBarHeight (area);
    if (barH > 0 && area.getHeight() > barH)
        area.removeFromBottom (barH);
    return area;
}

juce::Rectangle<int> LyricDisplayComponent::getPrimaryRenderArea (juce::Rectangle<int> area, bool idleMode) const
{
    const int adW = getAdPanelWidth (area, idleMode);
    if (adW > 0)
        return area.removeFromLeft (area.getWidth() - adW);
    return area;
}

juce::Rectangle<int> LyricDisplayComponent::getAdRenderArea (juce::Rectangle<int> area, bool idleMode) const
{
    const int adW = getAdPanelWidth (area, idleMode);
    if (adW <= 0)
        return {};
    return area.removeFromRight (adW);
}

void LyricDisplayComponent::refreshAdsAsync (bool force)
{
    if (venueId_.isEmpty())
    {
        applyAds ({});
        return;
    }

    if (! force && ! ads_.empty())
        return;

    juce::Component::SafePointer<LyricDisplayComponent> safe (this);
    const auto venueId = venueId_;

    juce::Thread::launch ([safe, venueId]()
    {
        auto ads = fetchVenueAds (venueId);
        juce::MessageManager::callAsync ([safe, ads = std::move (ads)]() mutable
        {
            if (safe != nullptr)
                safe->applyAds (std::move (ads));
        });
    });
}

void LyricDisplayComponent::applyAds (std::vector<AdEntry> ads)
{
    ads_ = std::move (ads);
    currentAdIndex_ = -1;
    adRemainingMs_ = 0;
    currentAdImage_ = {};
    stopIdleAdVideo();

    if (! ads_.empty())
        advanceIdleAd (true);

    repaint();
}

void LyricDisplayComponent::advanceIdleAd (bool force)
{
    if (ads_.empty())
    {
        currentAdIndex_ = -1;
        adRemainingMs_ = 0;
        currentAdImage_ = {};
        stopIdleAdVideo();
        repaint();
        return;
    }

    currentAdIndex_ = force
        ? juce::jlimit (0, (int) ads_.size() - 1, currentAdIndex_ < 0 ? 0 : currentAdIndex_)
        : (currentAdIndex_ + 1) % (int) ads_.size();

    auto& ad = ads_[(size_t) currentAdIndex_];
    adRemainingMs_ = juce::jmax (1000, ad.durationSec * 1000);
    currentAdImage_ = {};

    if (ad.isVideo())
    {
        showIdleAdVideo (ad);
    }
    else
    {
        stopIdleAdVideo();
        currentAdImage_ = ArtworkCache::getInstance().getOrFetch (ad.url,
            [safe = juce::Component::SafePointer<LyricDisplayComponent> (this)]()
            {
                if (safe == nullptr || safe->currentAdIndex_ < 0 || safe->currentAdIndex_ >= (int) safe->ads_.size())
                    return;

                const auto& current = safe->ads_[(size_t) safe->currentAdIndex_];
                if (current.isVideo())
                    return;

                auto loaded = ArtworkCache::getInstance().getOrFetch (current.url, nullptr);
                if (loaded.isValid())
                    safe->currentAdImage_ = loaded;
                safe->repaint();
            });
    }

    repaint();
}

void LyricDisplayComponent::stopIdleAdVideo()
{
    if (idleAdVideoComponent_ != nullptr)
    {
        idleAdVideoComponent_->stop();
        idleAdVideoComponent_->closeVideo();
        idleAdVideoComponent_->setVisible (false);
    }
}

void LyricDisplayComponent::showIdleAdVideo (const AdEntry& ad)
{
    if (idleAdVideoComponent_ == nullptr)
    {
        idleAdVideoComponent_ = std::make_unique<juce::VideoComponent> (false);
        addAndMakeVisible (idleAdVideoComponent_.get());
    }

    // Ad videos are visual-only on lyric display.
    idleAdVideoComponent_->setAudioVolume (0.0f);

    auto r = idleAdVideoComponent_->load (juce::URL (ad.url));
    if (r.failed())
    {
        DBG ("LyricDisplay: failed to load idle ad video: " + ad.url + " (" + r.getErrorMessage() + ")");
        stopIdleAdVideo();
        return;
    }

    // Re-apply mute after load in case decoder initialization reset it.
    idleAdVideoComponent_->setAudioVolume (0.0f);

    layoutIdleAdVideoBounds (getLocalBounds());
    idleAdVideoComponent_->setVisible (true);
    idleAdVideoComponent_->play();
}
