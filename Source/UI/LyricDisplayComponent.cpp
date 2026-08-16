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
#include "../Services/VenueService.h"

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

        juce::StringArray headers;
        const auto token = FirestoreClient::getInstance().getFreshIdToken();
        if (token.isNotEmpty())
            headers.add ("Authorization: Bearer " + token);

        auto appendFromPrefix = [&] (const juce::String& prefix)
        {
            const juce::String listUrl = "https://firebasestorage.googleapis.com/v0/b/"
                                       + FirebaseConfig::storageBucket
                                       + "/o?prefix="
                                       + juce::URL::addEscapeChars (prefix, true);

            int status = 0;
            const auto listJson = httpGetJson (juce::URL (listUrl), headers, &status);
            if (! (status >= 200 && status < 300) || ! listJson.isObject())
            {
                DBG ("LyricDisplay: ad list failed for prefix " + prefix + " (HTTP " + juce::String (status) + ")");
                return;
            }

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
        };

        // Canonical source: Firebase Storage folder Venues/<venueId>/Ads/.
        // Also scan lowercase "ads" for venues created with legacy casing.
        appendFromPrefix ("Venues/" + venueId + "/Ads/");
        appendFromPrefix ("Venues/" + venueId + "/ads/");

        std::sort (out.begin(), out.end(), [] (const LyricDisplayComponent::AdEntry& a,
                                               const LyricDisplayComponent::AdEntry& b)
        {
            return a.name.compareIgnoreCase (b.name) < 0;
        });

        return out;
    }

    // Avatar string -> image resolution (preset:<id>, legacy relative path,
    // or a Firebase Storage URL for a user-uploaded photo) is centralized in
    // ArtworkCache::resolveAvatar().
}

namespace
{
    // Scales saturation only (hue/brightness/alpha untouched) so Color
    // Intensity mutes every theme's accent uniformly, including Classic's
    // cyan -- at 100% (default) this is a no-op and Classic renders exactly
    // as it always has.
    juce::Colour scaleSaturation (juce::Colour c, float intensity01)
    {
        return c.withSaturation (c.getSaturation() * juce::jlimit (0.0f, 1.0f, intensity01));
    }

    // One "100% intensity" design value per theme, mirroring the CSS custom
    // properties (--t-accent, --t-ink, --t-ink-dim, --t-card-bg, --t-card-line,
    // --t-avatar) validated in the approved mockup. Confetti Pop and
    // Bubblegum Pop intentionally use light cards + dark ink -- the opposite
    // polarity from the other six -- which is why every card/text colour is
    // palette-driven rather than just the accent.
    const std::array<LyricThemePalette, 8>& lyricThemeBaseTable()
    {
        static const std::array<LyricThemePalette, 8> table = { {
            // Classic -- matches today's app exactly at Color Intensity 100%.
            { juce::Colour (0xff30daff), juce::Colours::white, juce::Colours::white.withAlpha (0.80f),
              juce::Colours::black.withAlpha (0.35f), juce::Colour (0xff30daff).withAlpha (0.18f),
              juce::Colour (0xff30daff).withAlpha (0.22f), juce::Colours::black.withAlpha (0.70f) },
            // Neon Pulse -- magenta/purple/cyan.
            { juce::Colour (0xffff2ec4), juce::Colours::white, juce::Colours::white.withAlpha (0.82f),
              juce::Colour (0xff2a1240).withAlpha (0.55f), juce::Colour (0xffff2ec4).withAlpha (0.35f),
              juce::Colour (0xff30daff).withAlpha (0.35f), juce::Colours::black.withAlpha (0.55f) },
            // Confetti Pop -- light cream cards, dark ink (inverted).
            { juce::Colour (0xffff3d81), juce::Colour (0xff3a2a1a), juce::Colour (0xff3a2a1a).withAlpha (0.75f),
              juce::Colour (0xfffff3e6).withAlpha (0.92f), juce::Colour (0xffff3d81).withAlpha (0.45f),
              juce::Colour (0xffff3d81).withAlpha (0.35f), juce::Colours::white.withAlpha (0.55f) },
            // Disco Sweep -- near-black, warm gold accent.
            { juce::Colour (0xffffc94d), juce::Colours::white, juce::Colours::white.withAlpha (0.78f),
              juce::Colours::black.withAlpha (0.45f), juce::Colour (0xffffc94d).withAlpha (0.30f),
              juce::Colour (0xffffc94d).withAlpha (0.30f), juce::Colours::black.withAlpha (0.65f) },
            // Retro Marquee -- deep burgundy, warm gold accent.
            { juce::Colour (0xffffcf70), juce::Colours::white, juce::Colours::white.withAlpha (0.80f),
              juce::Colour (0xff2a0810).withAlpha (0.55f), juce::Colour (0xffffcf70).withAlpha (0.30f),
              juce::Colour (0xffffcf70).withAlpha (0.30f), juce::Colours::black.withAlpha (0.55f) },
            // Laser Grid -- synthwave purple/pink, hot pink accent.
            { juce::Colour (0xffff4fd8), juce::Colours::white, juce::Colours::white.withAlpha (0.82f),
              juce::Colour (0xff2a1250).withAlpha (0.55f), juce::Colour (0xff30daff).withAlpha (0.35f),
              juce::Colour (0xffff4fd8).withAlpha (0.35f), juce::Colours::black.withAlpha (0.55f) },
            // Bubblegum Pop -- pastel, light cards, dark ink (inverted).
            { juce::Colour (0xffff6fb0), juce::Colour (0xff44304a), juce::Colour (0xff44304a).withAlpha (0.72f),
              juce::Colour (0xfffff0f8).withAlpha (0.90f), juce::Colour (0xffff6fb0).withAlpha (0.40f),
              juce::Colour (0xffff6fb0).withAlpha (0.35f), juce::Colours::white.withAlpha (0.55f) },
            // VU Meter Pulse -- near-black, signal-green accent.
            { juce::Colour (0xff39ff6a), juce::Colours::white, juce::Colours::white.withAlpha (0.78f),
              juce::Colours::black.withAlpha (0.45f), juce::Colour (0xff39ff6a).withAlpha (0.30f),
              juce::Colour (0xff39ff6a).withAlpha (0.30f), juce::Colours::black.withAlpha (0.65f) }
        } };
        return table;
    }

    LyricThemePalette resolveLyricThemePalette (LyricTheme theme, float colorIntensity01)
    {
        const auto& base = lyricThemeBaseTable()[(size_t) theme];
        LyricThemePalette out = base;
        out.accent         = scaleSaturation (base.accent,         colorIntensity01);
        out.ink             = scaleSaturation (base.ink,             colorIntensity01);
        out.inkDim          = scaleSaturation (base.inkDim,          colorIntensity01);
        out.cardFill        = scaleSaturation (base.cardFill,        colorIntensity01);
        out.cardBorder      = scaleSaturation (base.cardBorder,      colorIntensity01);
        out.avatarRing      = scaleSaturation (base.avatarRing,      colorIntensity01);
        out.avatarFallback  = scaleSaturation (base.avatarFallback,  colorIntensity01);
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

    seedThemeAnimations();
    refreshAdsAsync (true);
}

void LyricDisplayComponent::seedThemeAnimations()
{
    juce::Random rng (juce::Time::currentTimeMillis());

    for (auto& s : confettiSeeds_)
    {
        s.x0        = rng.nextFloat();
        s.fallSpeed = 0.12f + rng.nextFloat() * 0.18f;
        s.size      = 6.0f + rng.nextFloat() * 8.0f;
        s.rotSpeed  = 0.5f + rng.nextFloat() * 1.5f;
        s.rotOffset = rng.nextFloat() * juce::MathConstants<float>::twoPi;
        s.colourIdx = rng.nextInt (4);
    }

    for (auto& s : bubbleSeeds_)
    {
        s.x0          = rng.nextFloat();
        s.y0          = rng.nextFloat();
        s.radius      = 24.0f + rng.nextFloat() * 40.0f;
        s.bobSpeed    = 0.4f + rng.nextFloat() * 0.5f;
        s.bobPhase    = rng.nextFloat() * juce::MathConstants<float>::twoPi;
        s.driftSpeed  = 0.15f + rng.nextFloat() * 0.2f;
        s.driftPhase  = rng.nextFloat() * juce::MathConstants<float>::twoPi;
    }

    // Bulbs/bars read x as a fractional position across the strip -- lay
    // them out evenly rather than randomly so the row reads as an
    // intentional strip, not scattered noise.
    const int halfBulbs = (int) marqueeBulbSeeds_.size() / 2;
    for (int i = 0; i < (int) marqueeBulbSeeds_.size(); ++i)
    {
        auto& s = marqueeBulbSeeds_[(size_t) i];
        const bool top = i < halfBulbs;
        const int idxInRow = top ? i : (i - halfBulbs);
        s.x           = (float) idxInRow / (float) (halfBulbs - 1);
        s.topEdge     = top;
        s.phaseOffset = (float) idxInRow * 0.35f;
    }

    for (int i = 0; i < (int) vuBarSeeds_.size(); ++i)
    {
        auto& s = vuBarSeeds_[(size_t) i];
        s.x           = (float) i / (float) (vuBarSeeds_.size() - 1);
        s.speedMul    = 0.8f + rng.nextFloat() * 1.6f;
        s.phaseOffset = rng.nextFloat() * juce::MathConstants<float>::twoPi;
    }
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
    for (const auto& entry : queuePreview_)
    {
        const auto key = entry.avatarPath.trim().toStdString();
        if (key.empty() || queueAvatarCache_.find (key) != queueAvatarCache_.end())
            continue;

        resolveAndCacheAvatar (entry.avatarPath);
    }
    repaint();
}

void LyricDisplayComponent::setLowerThirdNextUpSinger (const juce::String& singerName)
{
    lowerThirdNextUpSinger_ = singerName.trim();
    repaint();
}

juce::Image LyricDisplayComponent::getQueuePreviewAvatar (const juce::String& avatarPath)
{
    return resolveAndCacheAvatar (avatarPath);
}

juce::Image LyricDisplayComponent::resolveAndCacheAvatar (const juce::String& avatarPath)
{
    const auto key = avatarPath.trim().toStdString();
    if (key.empty())
        return {};

    const auto cached = queueAvatarCache_.find (key);
    if (cached != queueAvatarCache_.end())
        return cached->second;

    auto image = ArtworkCache::getInstance().resolveAvatar (avatarPath,
        [safe = juce::Component::SafePointer<LyricDisplayComponent> (this), avatarPath]
        {
            if (safe == nullptr)
                return;
            const auto k = avatarPath.trim().toStdString();
            safe->queueAvatarCache_[k] = ArtworkCache::getInstance().resolveAvatar (avatarPath);
            safe->repaint();
        });
    queueAvatarCache_[key] = image;
    return image;
}

void LyricDisplayComponent::setNowSingingInfo (const juce::String& singerName,
                                               const juce::String& songName,
                                               const juce::String& artistName,
                                               const juce::String& avatarPath)
{
    nowSingingName_ = singerName;
    nowSingingSong_ = songName;
    nowSingingArtist_ = artistName;
    nowSingingAvatarPath_ = avatarPath;

    const auto key = nowSingingAvatarPath_.trim().toStdString();
    if (! key.empty() && queueAvatarCache_.find (key) == queueAvatarCache_.end())
        resolveAndCacheAvatar (nowSingingAvatarPath_);

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
    // Advance emoji reactions every tick regardless of idle/CDG/video mode,
    // so any already in flight when the idle screen kicks in keep quietly
    // progressing to natural completion in the background (they just won't
    // be painted — see paint()) rather than being frozen mid-animation.
    {
        const double nowMs = juce::Time::getMillisecondCounterHiRes();
        const double dt = (lastEmojiTickMs_ > 0.0)
                             ? juce::jlimit (0.0, 0.25, (nowMs - lastEmojiTickMs_) / 1000.0)
                             : (1.0 / 30.0);
        lastEmojiTickMs_ = nowMs;
        updateEmojis (dt);
    }

    // Advance the shared lyric-theme animation clock. Every themed
    // decoration (particles, beams, chase bulbs, VU bars...) reads a value
    // as a pure function of animPhase_ plus a fixed per-element seed -- see
    // paintThemeBackdrop() -- so multiplying the phase advance by
    // motion01 uniformly is enough to make Motion Intensity = 0 freeze
    // every theme with no per-theme branching, and there is no simulation
    // state to reset when the host switches themes.
    {
        const double nowMs = juce::Time::getMillisecondCounterHiRes();
        const double dt = (lastThemeTickMs_ > 0.0)
                             ? juce::jlimit (0.0, 0.25, (nowMs - lastThemeTickMs_) / 1000.0)
                             : (1.0 / 30.0);
        lastThemeTickMs_ = nowMs;
        const float motion01 = UserPreferences::getInstance().getLyricMotionIntensityPercent() / 100.0f;
        animPhase_ += dt * motion01;
    }

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

    // Keep lower-third overlays full-width in all modes.
    paintOverlay (g, area);

    // Emoji cheer reactions only ever show over an active karaoke song —
    // never on the idle/ad screen between singers.
    if (! idleMode)
        paintEmojis (g, area);
}

void LyricDisplayComponent::paintIdle (juce::Graphics& g, juce::Rectangle<int> area)
{
    const auto fullArea = area;
    auto left = area.removeFromLeft (area.getWidth() / 2);
    auto right = area;

    auto& prefs = UserPreferences::getInstance();
    const auto theme = static_cast<LyricTheme> (juce::jlimit (0, 7, prefs.getLyricThemeIndex()));
    const float colorIntensity01  = prefs.getLyricColorIntensityPercent()  / 100.0f;
    const float motionIntensity01 = prefs.getLyricMotionIntensityPercent() / 100.0f;
    const auto pal = resolveLyricThemePalette (theme, colorIntensity01);

    paintThemeBackdrop (g, fullArea, theme, pal, animPhase_, motionIntensity01);

    g.setColour (juce::Colours::black.withAlpha (0.22f));
    g.fillRect (right);

    const float logoScale         = prefs.getLyricLogoScalePercent()          / 100.0f;
    const float brandTextScale    = prefs.getLyricBrandTextScalePercent()     / 100.0f;
    const float nowTextScale      = prefs.getLyricNowSingingTextScalePercent() / 100.0f;
    const float nowInfoScale      = prefs.getLyricNowSingingInfoScalePercent() / 100.0f;
    const float upNextTextScale   = prefs.getLyricUpNextTextScalePercent()    / 100.0f;
    const float upNextInfoScale   = prefs.getLyricUpNextInfoScalePercent()    / 100.0f;

    // Keep branding compact so the queue panel has enough height for 3 rows.
    auto brandArea = left.removeFromTop ((int) (left.getHeight() * 0.38f)).reduced (24, 16);
    auto queueArea = left.reduced (24, 10);

    if (logoImage_.isValid())
    {
        auto logoArea = brandArea.removeFromTop ((int) (brandArea.getHeight() * 0.56f * logoScale));

        // The logo artwork has semi-transparent shading, so the busier/
        // brighter theme backdrops show through it and wash it out. A soft
        // dark halo behind it keeps it legible on top of any theme. Classic
        // is left untouched (pixel parity with the pre-theme app).
        if (theme != LyricTheme::Classic)
        {
            const auto glow = logoArea.toFloat().expanded (logoArea.getWidth() * 0.22f);
            const auto centre = glow.getCentre();
            juce::ColourGradient glowGrad (juce::Colours::black.withAlpha (0.55f), centre.x, centre.y,
                                           juce::Colours::black.withAlpha (0.0f), glow.getRight(), centre.y, true);
            g.setGradientFill (glowGrad);
            g.fillEllipse (glow);
        }

        // No onlyReduceInSize here (unlike the pre-scale-slider version) --
        // the slider needs to be able to grow the logo past its native
        // pixel size, not just shrink it.
        g.drawImage (logoImage_, logoArea.toFloat(), juce::RectanglePlacement::centred);
    }

    const auto venueLabel = venueName_.isNotEmpty() ? venueName_ : juce::String ("Encore Karaoke");
    g.setColour (pal.ink);
    g.setFont (juce::Font (juce::FontOptions().withHeight (juce::jmax (24.0f, brandArea.getHeight() * 0.16f) * brandTextScale)).boldened());
    g.drawFittedText (venueLabel, brandArea, juce::Justification::centredTop, 2);

    if (nowSingingName_.isNotEmpty())
    {
        g.setColour (pal.accent);
        g.setFont (juce::Font (juce::FontOptions().withHeight (20.0f * nowTextScale)).boldened());
        auto nowTitle = queueArea.removeFromTop ((int) (30 * nowTextScale));
        g.drawText ("Now Singing", nowTitle, juce::Justification::centredLeft, true);

        const int nowCardH = (int) (72 * juce::jmax (nowInfoScale, nowTextScale));
        auto nowCard = queueArea.removeFromTop (nowCardH).reduced (0, 6);
        g.setColour (pal.cardFill);
        g.fillRoundedRectangle (nowCard.toFloat(), 8.0f);
        g.setColour (pal.cardBorder);
        g.drawRoundedRectangle (nowCard.toFloat().reduced (0.5f), 8.0f, 1.2f);
        paintNowCardDecoration (g, nowCard, theme, pal, animPhase_, motionIntensity01);

        auto nowText = nowCard.reduced (12, 10);
        auto nowAvatarBox = nowText.removeFromLeft ((int) (48 * nowInfoScale));
        const int nowAvatarSize = juce::jmax (18, juce::jmin (nowAvatarBox.getWidth(), nowAvatarBox.getHeight()) - 8);
        auto nowAvatarArea = juce::Rectangle<int> (
            nowAvatarBox.getX() + (nowAvatarBox.getWidth()  - nowAvatarSize) / 2,
            nowAvatarBox.getY() + (nowAvatarBox.getHeight() - nowAvatarSize) / 2,
            nowAvatarSize,
            nowAvatarSize);

        // Prefer the same avatar source used by the Next Up list for this
        // singer so both cards render consistently.
        juce::String resolvedNowAvatarPath;
        for (const auto& entry : queuePreview_)
        {
            if (entry.singerName.trim().equalsIgnoreCase (nowSingingName_.trim())
                && entry.avatarPath.trim().isNotEmpty())
            {
                resolvedNowAvatarPath = entry.avatarPath;
                break;
            }
        }

        if (resolvedNowAvatarPath.isEmpty())
            resolvedNowAvatarPath = nowSingingAvatarPath_;

        const auto nowAvatar = getQueuePreviewAvatar (resolvedNowAvatarPath);
        if (! nowAvatar.isValid())
        {
            g.setColour (pal.avatarFallback);
            g.fillEllipse (nowAvatarArea.toFloat());
        }

        if (nowAvatar.isValid())
        {
            juce::Graphics::ScopedSaveState state (g);
            juce::Path clipPath;
            clipPath.addEllipse (nowAvatarArea.toFloat());
            g.reduceClipRegion (clipPath);
            g.drawImage (nowAvatar, nowAvatarArea.toFloat(), juce::RectanglePlacement::fillDestination);
        }
        g.setColour (pal.avatarRing);
        g.drawEllipse (nowAvatarArea.toFloat().reduced (0.5f), 1.2f);

        nowText.removeFromLeft (6);
        g.setColour (pal.ink);
        g.setFont (juce::Font (juce::FontOptions().withHeight (20.0f * nowTextScale)).boldened());
        g.drawText (nowSingingName_, nowText.removeFromTop ((int) (24 * nowTextScale)), juce::Justification::centredLeft, true);

        juce::String nowSongLine = nowSingingSong_;
        if (nowSingingArtist_.isNotEmpty())
            nowSongLine << " - " << nowSingingArtist_;

        g.setColour (pal.inkDim);
        g.setFont (juce::Font (juce::FontOptions().withHeight (16.0f * nowTextScale)));
        g.drawFittedText (nowSongLine, nowText, juce::Justification::centredLeft, 1);

        queueArea.removeFromTop (4);
    }

    g.setColour (pal.accent);
    g.setFont (juce::Font (juce::FontOptions().withHeight (22.0f * upNextTextScale)).boldened());
    if (! queuePreview_.empty())
    {
        auto queueTitle = queueArea.removeFromTop ((int) (36 * upNextTextScale));
        g.drawText ("Next Up", queueTitle, juce::Justification::centredLeft, true);
    }

    if (queuePreview_.empty())
    {
        g.setColour (pal.inkDim);
        g.setFont (juce::Font (juce::FontOptions().withHeight (20.0f * upNextTextScale)));
        g.drawFittedText ("Queue is waiting for singers", queueArea, juce::Justification::centred, 2);
    }
    else
    {
        // Keep queue rows visually stable (similar to the Now Singing card)
        // so avatar badges stay circular and text rhythm is consistent.
        const int rowH = (int) (72 * juce::jmax (upNextInfoScale, upNextTextScale));
        for (int i = 0; i < (int) queuePreview_.size() && i < 3; ++i)
        {
            if (queueArea.getHeight() < rowH)
                break;

            auto row = queueArea.removeFromTop (rowH).reduced (0, 6);

            g.setColour (pal.cardFill);
            g.fillRoundedRectangle (row.toFloat(), 8.0f);
            g.setColour (pal.cardBorder);
            g.drawRoundedRectangle (row.toFloat().reduced (0.5f), 8.0f, 1.0f);

            auto text = row.reduced (12, 10);
            g.setColour (pal.accent);
            g.setFont (juce::Font (juce::FontOptions().withHeight (16.0f * upNextTextScale)).boldened());
            g.drawText (juce::String (i + 1), text.removeFromLeft ((int) (26 * upNextTextScale)), juce::Justification::centred);

            auto avatarBox = text.removeFromLeft ((int) (44 * upNextInfoScale));
            const int avatarSize = juce::jmax (18, juce::jmin (avatarBox.getWidth(), avatarBox.getHeight()) - 8);
            auto avatarArea = juce::Rectangle<int> (
                avatarBox.getX() + (avatarBox.getWidth()  - avatarSize) / 2,
                avatarBox.getY() + (avatarBox.getHeight() - avatarSize) / 2,
                avatarSize,
                avatarSize);

            const auto avatar = getQueuePreviewAvatar (queuePreview_[(size_t) i].avatarPath);

            // Keep real avatars un-tinted. Draw fallback backing only when missing.
            if (! avatar.isValid())
            {
                g.setColour (pal.avatarFallback);
                g.fillEllipse (avatarArea.toFloat());
            }

            if (avatar.isValid())
            {
                juce::Graphics::ScopedSaveState state (g);
                juce::Path clipPath;
                clipPath.addEllipse (avatarArea.toFloat());
                g.reduceClipRegion (clipPath);
                g.drawImage (avatar, avatarArea.toFloat(), juce::RectanglePlacement::fillDestination);
            }

            // Subtle ring for separation from row background.
            g.setColour (pal.avatarRing);
            g.drawEllipse (avatarArea.toFloat().reduced (0.5f), 1.2f);

            // Breathing room between avatar and singer name.
            text.removeFromLeft (8);

            g.setColour (pal.ink);
            g.setFont (juce::Font (juce::FontOptions().withHeight (20.0f * upNextTextScale)).boldened());
            g.drawText (queuePreview_[(size_t) i].singerName, text.removeFromTop ((int) (24 * upNextTextScale)), juce::Justification::centredLeft, true);

            juce::String songLine = queuePreview_[(size_t) i].songName;
            if (queuePreview_[(size_t) i].artistName.isNotEmpty())
                songLine << " - " << queuePreview_[(size_t) i].artistName;

            g.setColour (pal.inkDim);
            g.setFont (juce::Font (juce::FontOptions().withHeight (16.0f * upNextTextScale)));
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

    // Opaque, not translucent: an image ad is drawn into this same 2D
    // canvas right after this fill (below), so any transparency in the ad
    // artwork or letterboxing from an aspect-ratio mismatch lets this
    // backdrop show/blend through, reading as "faded". A video ad never
    // showed this because idleAdVideoComponent_ is a native VideoComponent
    // layered fully opaque on top of this whole area regardless of what's
    // painted underneath -- this fill was invisible for video the entire
    // time, so making it opaque only changes the image case.
    g.setColour (juce::Colours::black);
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

void LyricDisplayComponent::paintThemeBackdrop (juce::Graphics& g, juce::Rectangle<int> area, LyricTheme theme,
                                                const LyricThemePalette& pal, double animPhase, float motion01)
{
    juce::ignoreUnused (pal);

    const float w  = (float) area.getWidth();
    const float h  = (float) area.getHeight();
    const float cx = (float) area.getCentreX();
    const float cy = (float) area.getCentreY();

    switch (theme)
    {
        case LyricTheme::Classic:
        {
            // Byte-identical to the pre-theme app -- guarantees true pixel
            // parity at Color Intensity 100%.
            auto left = area.removeFromLeft (area.getWidth() / 2);
            g.setGradientFill (juce::ColourGradient (juce::Colour (0xff4c4c4c),
                                                     (float) left.getX(), (float) left.getY(),
                                                     juce::Colour (0xff2b2b2b),
                                                     (float) left.getRight(), (float) left.getBottom(),
                                                     false));
            g.fillRect (left);
            break;
        }

        case LyricTheme::NeonPulse:
        {
            const float angle = (float) animPhase * 0.15f;
            const float dx = std::cos (angle) * w * 0.5f;
            const float dy = std::sin (angle) * h * 0.5f;
            juce::ColourGradient grad (juce::Colour (0xffb400ff), cx - dx, cy - dy,
                                       juce::Colour (0xff00e0ff), cx + dx, cy + dy, false);
            grad.addColour (0.5, juce::Colour (0xff5a1fd0));
            g.setGradientFill (grad);
            g.fillRect (area);
            break;
        }

        case LyricTheme::ConfettiPop:
        {
            juce::ColourGradient grad (juce::Colour (0xffffd23f), (float) area.getX(), (float) area.getY(),
                                       juce::Colour (0xffff5e7e), (float) area.getRight(), (float) area.getBottom(), false);
            grad.addColour (0.5, juce::Colour (0xffff9c3f));
            g.setGradientFill (grad);
            g.fillRect (area);

            static const juce::Colour confettiColours[4] = {
                juce::Colours::white, juce::Colour (0xff30daff),
                juce::Colour (0xff39ff6a), juce::Colour (0xffffe14d)
            };

            for (const auto& s : confettiSeeds_)
            {
                const float yFrac = std::fmod ((float) animPhase * s.fallSpeed + s.x0 * 3.0f, 1.0f);
                const float x = area.getX() + s.x0 * w;
                const float y = area.getY() + yFrac * h;
                const float rot = s.rotOffset + (float) animPhase * s.rotSpeed;

                juce::Path quad;
                quad.addRectangle (-s.size * 0.5f, -s.size * 0.35f, s.size, s.size * 0.7f);
                quad.applyTransform (juce::AffineTransform::rotation (rot).translated (x, y));

                g.setColour (confettiColours[(size_t) s.colourIdx].withAlpha (0.9f));
                g.fillPath (quad);
            }
            break;
        }

        case LyricTheme::DiscoSweep:
        {
            g.setColour (juce::Colour (0xff0c0c10));
            g.fillRect (area);

            static const juce::Colour beamColours[3] = {
                juce::Colour (0xffffc94d), juce::Colour (0xff30daff), juce::Colour (0xffff4fd8)
            };

            for (int i = 0; i < 3; ++i)
            {
                const float speed = 0.25f + (float) i * 0.12f;
                const float angle = (float) animPhase * speed + (float) i * 2.1f;

                juce::Path beam;
                const float len = juce::jmax (w, h) * 1.4f;
                beam.addRectangle (-len * 0.5f, -18.0f, len, 36.0f);
                beam.applyTransform (juce::AffineTransform::rotation (angle).translated (cx, cy));

                // No true "screen" blend mode in JUCE's 2D context -- modest
                // alpha over near-black approximates the mockup's additive
                // glow, though overlapping beams will darken/mix rather than
                // truly brighten.
                juce::ColourGradient beamGrad (beamColours[(size_t) i].withAlpha (0.28f), cx, cy,
                                               beamColours[(size_t) i].withAlpha (0.0f), cx + len * 0.5f, cy, false);
                g.setGradientFill (beamGrad);
                g.fillPath (beam);
            }
            break;
        }

        case LyricTheme::RetroMarquee:
        {
            g.setGradientFill (juce::ColourGradient (juce::Colour (0xff3a0812), (float) area.getX(), (float) area.getY(),
                                                     juce::Colour (0xff1c0509), (float) area.getX(), (float) area.getBottom(), false));
            g.fillRect (area);

            const float edgeMargin = 14.0f;
            for (const auto& s : marqueeBulbSeeds_)
            {
                const float x = area.getX() + s.x * w;
                const float y = s.topEdge ? area.getY() + edgeMargin : area.getBottom() - edgeMargin;
                // motion01 scales the chase amplitude directly so Motion=0
                // settles every bulb to the same dim rest brightness instead
                // of freezing on an arbitrary per-bulb phase.
                const float brightness = 0.35f + 0.65f * motion01 * std::abs (std::sin ((float) animPhase * 2.2f + s.phaseOffset));

                g.setColour (juce::Colour (0xffffcf70).withAlpha (brightness * 0.35f));
                g.fillEllipse (x - 7.0f, y - 7.0f, 14.0f, 14.0f);
                g.setColour (juce::Colour (0xffffe9b8).withAlpha (0.5f + brightness * 0.5f));
                g.fillEllipse (x - 3.5f, y - 3.5f, 7.0f, 7.0f);
            }
            break;
        }

        case LyricTheme::LaserGrid:
        {
            g.setGradientFill (juce::ColourGradient (juce::Colour (0xff230832), (float) area.getX(), (float) area.getY(),
                                                     juce::Colour (0xff5e1a4a), (float) area.getX(), (float) area.getBottom(), false));
            g.fillRect (area);

            const float horizonY = area.getY() + h * 0.55f;
            const float scroll = std::fmod ((float) animPhase * 0.35f, 1.0f);

            // Horizontal perspective lines, spaced quadratically so they
            // bunch up near the horizon like a receding floor grid.
            for (int i = 0; i < 10; ++i)
            {
                const float t = ((float) i + scroll) / 10.0f;
                const float y = horizonY + t * t * (area.getBottom() - horizonY);
                const float alpha = juce::jmax (0.0f, 0.5f - t * 0.4f);
                g.setColour (juce::Colour (0xff30daff).withAlpha (alpha * 0.35f));
                g.drawLine ((float) area.getX(), y, (float) area.getRight(), y, 3.0f);
                g.setColour (juce::Colour (0xffff4fd8).withAlpha (alpha));
                g.drawLine ((float) area.getX(), y, (float) area.getRight(), y, 1.2f);
            }

            // Converging diagonals from the horizon out to the bottom edge.
            for (int i = -5; i <= 5; ++i)
            {
                const float xTop = cx + (float) i * (w * 0.05f);
                const float xBottom = cx + (float) i * (w * 0.5f);
                g.setColour (juce::Colour (0xffff4fd8).withAlpha (0.25f));
                g.drawLine (xTop, horizonY, xBottom, (float) area.getBottom(), 1.2f);
            }
            break;
        }

        case LyricTheme::BubblegumPop:
        {
            juce::ColourGradient grad (juce::Colour (0xffffd6ec), (float) area.getX(), (float) area.getY(),
                                       juce::Colour (0xffd6ecff), (float) area.getRight(), (float) area.getBottom(), false);
            grad.addColour (0.5, juce::Colour (0xffe9d6ff));
            g.setGradientFill (grad);
            g.fillRect (area);

            // Radial gradient fill fakes a soft blur -- a true blur would
            // need an offscreen image + convolution, unnecessary for v1.
            for (const auto& s : bubbleSeeds_)
            {
                const float x = area.getX() + s.x0 * w + std::sin ((float) animPhase * s.driftSpeed + s.driftPhase) * 14.0f;
                const float y = area.getY() + s.y0 * h + std::sin ((float) animPhase * s.bobSpeed + s.bobPhase) * 18.0f;

                juce::ColourGradient bubbleGrad (juce::Colours::white.withAlpha (0.55f), x, y,
                                                 juce::Colour (0xffff9fd0).withAlpha (0.05f), x + s.radius, y + s.radius, false);
                g.setGradientFill (bubbleGrad);
                g.fillEllipse (x - s.radius, y - s.radius, s.radius * 2.0f, s.radius * 2.0f);
            }
            break;
        }

        case LyricTheme::VuMeterPulse:
        {
            g.setColour (juce::Colour (0xff0a0a0a));
            g.fillRect (area);

            const float barAreaH = h * 0.4f;
            const float barBaseY = (float) area.getBottom() - 8.0f;
            const float barW = juce::jmax (3.0f, w / (float) vuBarSeeds_.size() * 0.6f);

            for (const auto& s : vuBarSeeds_)
            {
                const float x = area.getX() + s.x * w;
                const float level = std::abs (std::sin ((float) animPhase * s.speedMul + s.phaseOffset));
                // motion01 scales the swing amplitude directly, so Motion=0
                // settles every bar to the same flat rest height rather than
                // freezing mid-bounce at an arbitrary per-bar phase.
                const float barH = 6.0f + level * barAreaH * motion01;

                g.setColour (juce::Colour (0xff39ff6a).withAlpha (0.85f));
                g.fillRoundedRectangle (x - barW * 0.5f, barBaseY - barH, barW, barH, barW * 0.3f);
            }
            break;
        }
    }
}

void LyricDisplayComponent::paintNowCardDecoration (juce::Graphics& g, juce::Rectangle<int> nowCard, LyricTheme theme,
                                                     const LyricThemePalette& pal, double animPhase, float motion01)
{
    // Only Neon Pulse gets a breathing glow around the Now Singing card;
    // every other theme is a no-op here.
    if (theme != LyricTheme::NeonPulse)
        return;

    const float pulse = 0.5f + 0.5f * motion01 * std::sin ((float) animPhase * 1.8f);
    for (int ring = 0; ring < 3; ++ring)
    {
        const float expand = (float) ring * 3.0f + pulse * 4.0f;
        g.setColour (pal.accent.withAlpha (0.20f - (float) ring * 0.06f));
        g.drawRoundedRectangle (nowCard.toFloat().expanded (expand), 8.0f + expand, 1.5f);
    }
}

void LyricDisplayComponent::paintOverlay (juce::Graphics& g, juce::Rectangle<int> area)
{
    auto& prefs = UserPreferences::getInstance();
    const auto theme = static_cast<LyricTheme> (juce::jlimit (0, 7, prefs.getLyricThemeIndex()));
    const float colorIntensity01 = prefs.getLyricColorIntensityPercent() / 100.0f;
    const auto pal = resolveLyricThemePalette (theme, colorIntensity01);

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
        g.setColour (pal.accent);
        g.drawRoundedRectangle (banner.toFloat().reduced (0.5f), 8.0f, 1.5f);

        auto text = banner.reduced (14, 10);

        g.setColour (pal.accent);
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

    // --- Bottom lower-third: left Next Up, right Venue Code ---
    if (venueCode_.isNotEmpty() || lowerThirdNextUpSinger_.isNotEmpty())
    {
        // Use the same height getContentRenderArea() reserves space for, so
        // the "Lyric Code Bar Height" slider actually resizes the bar that's
        // drawn here rather than just changing how content is clipped above it.
        const int codeH = getVenueCodeBarHeight (area);
        auto stripe = area.withTop (area.getBottom() - codeH);

        // Solid black bar across the full width of the lyric screen.
        g.setColour (juce::Colours::black);
        g.fillRect (stripe);

        auto inner = stripe.reduced (24, 10);
        auto leftHalf  = inner.removeFromLeft (inner.getWidth() / 2).reduced (8, 0);
        auto rightHalf = inner.reduced (8, 0);

        // Divider line between left/right halves.
        g.setColour (juce::Colours::white.withAlpha (0.12f));
        g.fillRect (leftHalf.getRight() + 7, stripe.getY() + 12, 1, stripe.getHeight() - 24);

        const float bottomBarTextScale = UserPreferences::getInstance().getLyricBottomBarTextScalePercent() / 100.0f;

        if (lowerThirdNextUpSinger_.isNotEmpty())
        {
            const float titleSize = juce::jmax (16.0f, leftHalf.getHeight() * 0.28f) * bottomBarTextScale;
            const float singerSize = juce::jmax (24.0f, leftHalf.getHeight() * 0.48f) * bottomBarTextScale;

            g.setColour (pal.accent);
            g.setFont (juce::Font (juce::FontOptions().withHeight (titleSize)).boldened());
            auto titleArea = leftHalf.removeFromTop ((int) (titleSize * 1.15f));
            g.drawFittedText ("Next Up", titleArea, juce::Justification::centredLeft, 1);

            g.setColour (juce::Colours::white);
            g.setFont (juce::Font (juce::FontOptions().withHeight (singerSize)).boldened());
            g.drawFittedText (lowerThirdNextUpSinger_, leftHalf, juce::Justification::centredLeft, 1);
        }

        if (venueCode_.isNotEmpty())
        {
            const float promptSize = juce::jmax (16.0f, rightHalf.getHeight() * 0.28f) * bottomBarTextScale;
            const float codeSize   = juce::jmax (28.0f, rightHalf.getHeight() * 0.54f) * bottomBarTextScale;

            g.setColour (juce::Colours::white);
            g.setFont (juce::Font (juce::FontOptions().withHeight (promptSize)));
            auto promptArea = rightHalf.removeFromTop ((int) (promptSize * 1.15f));
            g.drawFittedText ("Enter this code to join the karaoke queue",
                              promptArea, juce::Justification::centred, 1);

            g.setColour (pal.accent);
            g.setFont (juce::Font (juce::FontOptions().withHeight (codeSize)).boldened());
            g.drawFittedText (venueCode_, rightHalf, juce::Justification::centred, 1);
        }
    }
}

void LyricDisplayComponent::setVenueLogo (const juce::Image& logo)
{
    logoImage_ = logo;
    repaint();
}

//==============================================================================
// Emoji cheer reactions
juce::Image LyricDisplayComponent::getOrLoadEmojiSheet (const juce::String& emojiName, int& outTotalFrames)
{
    const auto key = emojiName.toStdString();

    auto cachedImage = emojiSheetCache_.find (key);
    if (cachedImage != emojiSheetCache_.end())
    {
        outTotalFrames = emojiFrameCountCache_[key];
        return cachedImage->second;
    }

    const auto appDir = juce::File::getSpecialLocation (juce::File::currentExecutableFile)
                            .getParentDirectory();
    const auto file = appDir.getChildFile ("assets/emojis/" + emojiName + "-sheet.png");

    juce::Image image;
    int frames = 0;
    if (file.existsAsFile())
    {
        image = juce::ImageFileFormat::loadFrom (file);
        if (image.isValid())
            frames = juce::jmax (1, image.getWidth() / kEmojiFrameSize);
    }
    else
    {
        DBG ("LyricDisplay: emoji sprite sheet not found: " + file.getFullPathName());
    }

    emojiSheetCache_[key] = image;
    emojiFrameCountCache_[key] = frames;
    outTotalFrames = frames;
    return image;
}

void LyricDisplayComponent::addEmoji (const Emoji& request)
{
    // Never display cheer reactions over the idle/ad screen — only during
    // an active karaoke song.
    const bool idleMode = forceIdleScreen_ || (! isVideoActive() && ! decoder_.isLoaded());
    if (idleMode)
        return;

    if ((int) activeEmojis_.size() >= kMaxConcurrentEmojis)
        return;  // spam guard — extra reactions are silently dropped

    const juce::String name = juce::String (request.emojiName).trim();
    if (name.isEmpty())
        return;

    int totalFrames = 0;
    auto sheet = getOrLoadEmojiSheet (name, totalFrames);
    if (! sheet.isValid() || totalFrames <= 0)
        return;

    Emoji e = request;
    e.img = sheet;
    e.totalFrames = totalFrames;
    e.width = kEmojiFrameSize;
    e.height = kEmojiFrameSize;
    e.current = 0;
    e.alpha = 1.0f;
    e.isDeleting = false;
    e.isLoading = false;
    e.frameAccumulatorSeconds = 0.0f;

    // Random spawn: left-hand column, near the bottom, random rise speed —
    // mirrors the legacy Angular lyric-display cheer animation. Sizes and
    // thresholds scale with the window so the effect looks right whether
    // this is a small preview or a full external display.
    auto bounds = getLocalBounds();
    juce::Random& rng = juce::Random::getSystemRandom();

    const float leftBand = juce::jmax (60.0f, (float) bounds.getWidth() * 0.08f);
    e.xPos = rng.nextFloat() * leftBand;
    e.yPos = (float) bounds.getHeight() - 20.0f;
    e.speed = 40.0f + rng.nextFloat() * 90.0f;              // px/second
    e.fadeStartYPos = (float) bounds.getHeight() * 0.2f;
    e.drawSize = juce::jmax (60.0f, (float) bounds.getHeight() * 0.09f);

    activeEmojis_.push_back (std::move (e));
}

void LyricDisplayComponent::updateEmojis (double dtSeconds)
{
    if (activeEmojis_.empty())
        return;

    for (auto& e : activeEmojis_)
        e.update (dtSeconds);

    for (auto it = activeEmojis_.begin(); it != activeEmojis_.end(); )
    {
        if (it->isFinished())
        {
            if (venueId_.isNotEmpty() && ! it->id.empty())
                VenueService::getInstance().deleteEmoji (venueId_, juce::String (it->id));
            it = activeEmojis_.erase (it);
        }
        else
        {
            ++it;
        }
    }
}

void LyricDisplayComponent::paintEmojis (juce::Graphics& g, juce::Rectangle<int> area)
{
    for (const auto& e : activeEmojis_)
    {
        if (! e.img.isValid())
            continue;

        const int srcX = e.current * kEmojiFrameSize;
        const int destSize = (int) e.drawSize;
        const int destX = area.getX() + (int) e.xPos;
        const int destY = area.getY() + (int) e.yPos;

        g.setOpacity (juce::jlimit (0.0f, 1.0f, e.alpha));
        g.drawImage (e.img,
                     destX, destY, destSize, destSize,
                     srcX, 0, kEmojiFrameSize, kEmojiFrameSize);
    }

    g.setOpacity (1.0f);
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
    if (venueCode_.isEmpty() && lowerThirdNextUpSinger_.isEmpty())
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
