/*
  ==============================================================================

    LyricDisplayComponent.cpp

  ==============================================================================
*/

#include "LyricDisplayComponent.h"
#include "../Audio/AudioEngine.h"

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
}

LyricDisplayComponent::~LyricDisplayComponent()
{
    stopTimer();
}

//==============================================================================
void LyricDisplayComponent::setAudioEngine (AudioEngine* engine)
{
    audioEngine_ = engine;
}

void LyricDisplayComponent::loadCDG (const juce::File& cdgFile)
{
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
    if (! decoder_.isLoaded() || audioEngine_ == nullptr)
        return;

    const double pos = audioEngine_->getCurrentPosition();
    decoder_.renderAt (pos);
    repaint();
}

//==============================================================================
void LyricDisplayComponent::resized() {}

void LyricDisplayComponent::paint (juce::Graphics& g)
{
    auto area = getLocalBounds();

    // Solid backdrop — matches the Angular theme's --background-color (#393939).
    g.fillAll (juce::Colour (0xff393939));

    if (decoder_.isLoaded())
        paintCdg (g, area);
    else
        paintIdle (g, area);

    paintOverlay (g, area);
}

void LyricDisplayComponent::paintIdle (juce::Graphics& g, juce::Rectangle<int> area)
{
    // Subtle radial vignette for visual interest.
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff4a4a4a),
                                             (float) area.getCentreX(),
                                             (float) area.getCentreY(),
                                             juce::Colour (0xff2a2a2a),
                                             0.0f, 0.0f, true));
    g.fillRect (area);

    if (logoImage_.isValid())
    {
        const int maxW = juce::jmin (600, area.getWidth()  / 2);
        const int maxH = juce::jmin (400, area.getHeight() / 2);
        auto logoArea = juce::Rectangle<float> (0, 0, (float) maxW, (float) maxH)
                            .withCentre (area.toFloat().getCentre());
        g.drawImage (logoImage_, logoArea,
                     juce::RectanglePlacement::centred
                   | juce::RectanglePlacement::onlyReduceInSize);
    }
    else
    {
        g.setColour (juce::Colour (0xff30daff));
        g.setFont (juce::Font (juce::FontOptions()
                                  .withHeight (juce::jmax (48.0f,
                                                          area.getHeight() * 0.08f)))
                       .boldened());
        g.drawFittedText ("Encore",
                          area.reduced (40),
                          juce::Justification::centred,
                          1);
    }

    g.setColour (juce::Colours::white.withAlpha (0.6f));
    g.setFont (juce::Font (juce::FontOptions().withHeight (22.0f)));
    g.drawFittedText ("Waiting for next singer...",
                      area.withTop (area.getBottom() - 60),
                      juce::Justification::centred,
                      1);
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

    // --- Venue code (bottom-centre lower-third) ---
    if (venueCode_.isNotEmpty())
    {
        const int codeH = 56;
        auto stripe = area.withTop (area.getBottom() - codeH);
        g.setColour (juce::Colour (0xcc000000));
        g.fillRect (stripe);

        g.setColour (juce::Colour (0xff30daff));
        g.setFont (juce::Font (juce::FontOptions().withHeight (24.0f)).boldened());
        g.drawText ("Join with code: " + venueCode_, stripe,
                    juce::Justification::centred);
    }
}
