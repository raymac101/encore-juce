#import <AVFoundation/AVFoundation.h>
#import <QuartzCore/QuartzCore.h>

#include "AdVideoPreviewComponent.h"

struct AdVideoPreviewComponent::Pimpl
{
    enum class Readiness
    {
        pending,
        ready,
        failed
    };

    explicit Pimpl (const juce::File& file)
    {
        const auto path = file.getFullPathName();
        auto* pathString = [NSString stringWithUTF8String:path.toRawUTF8()];
        if (pathString == nil)
            return;

        auto* asset = [AVURLAsset URLAssetWithURL:[NSURL fileURLWithPath:pathString] options:nil];
        item = [[AVPlayerItem alloc] initWithAsset:asset];
        generator = [[AVAssetImageGenerator alloc] initWithAsset:asset];
        generator.appliesPreferredTrackTransform = YES;
        generator.maximumSize = CGSizeMake (1280.0, 720.0);
        generator.requestedTimeToleranceBefore = CMTimeMake (1, 30);
        generator.requestedTimeToleranceAfter = CMTimeMake (1, 30);

        NSDictionary* attributes = @{
            (NSString*) kCVPixelBufferPixelFormatTypeKey: @(kCVPixelFormatType_32BGRA)
        };
        output = [[AVPlayerItemVideoOutput alloc] initWithPixelBufferAttributes:attributes];
        [item addOutput:output];
        [output requestNotificationOfMediaDataChangeWithAdvanceInterval:0.03];
        player = [[AVPlayer alloc] initWithPlayerItem:item];
    }

    ~Pimpl()
    {
        [player pause];
        [player release];
        [output release];
        [generator release];
        [item release];
    }

    Readiness readiness (juce::String& error) const
    {
        if (item == nil)
        {
            error = "Could not create the video player.";
            return Readiness::failed;
        }
        if (item.status == AVPlayerItemStatusFailed)
        {
            error = item.error != nil
                        ? juce::String::fromUTF8 (item.error.localizedDescription.UTF8String)
                        : "Could not decode the video.";
            return Readiness::failed;
        }
        if (item.status == AVPlayerItemStatusReadyToPlay)
            return Readiness::ready;
        return Readiness::pending;
    }

    juce::Image generateFrame (double seconds) const
    {
        if (generator == nil)
            return {};

        const auto itemTime = CMTimeMakeWithSeconds (juce::jmax (0.0, seconds), 600);
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
        CGImageRef generatedFrame = [generator copyCGImageAtTime:itemTime actualTime:nil error:nil];
#pragma clang diagnostic pop
        if (generatedFrame == nullptr)
            return {};

        const int width = (int) CGImageGetWidth (generatedFrame);
        const int height = (int) CGImageGetHeight (generatedFrame);
        juce::Image image (juce::Image::ARGB, width, height, true);
        juce::Image::BitmapData pixels (image, juce::Image::BitmapData::writeOnly);
        auto colourSpace = CGColorSpaceCreateDeviceRGB();
        auto context = CGBitmapContextCreate (pixels.getLinePointer (0), (size_t) width, (size_t) height,
                                              8, (size_t) pixels.lineStride, colourSpace,
                                              kCGImageAlphaPremultipliedFirst | kCGBitmapByteOrder32Little);
        if (context != nullptr)
        {
            CGContextDrawImage (context, CGRectMake (0, 0, width, height), generatedFrame);
            CGContextRelease (context);
        }
        CGColorSpaceRelease (colourSpace);
        CGImageRelease (generatedFrame);
        return image;
    }

    void play()  { if (player != nil) [player play]; }
    void pause() { if (player != nil) [player pause]; }
    bool isPlaying() const { return player != nil && player.rate != 0.0f; }

    double position() const
    {
        const auto seconds = CMTimeGetSeconds (player.currentTime);
        return std::isfinite (seconds) ? seconds : 0.0;
    }

    double duration() const
    {
        const auto seconds = CMTimeGetSeconds (item.duration);
        return std::isfinite (seconds) ? seconds : 0.0;
    }

    AVPlayerItem* item = nil;
    AVPlayerItemVideoOutput* output = nil;
    AVAssetImageGenerator* generator = nil;
    AVPlayer* player = nil;
};

AdVideoPreviewComponent::AdVideoPreviewComponent() = default;

AdVideoPreviewComponent::~AdVideoPreviewComponent()
{
    stopTimer();
    pimpl_.reset();
}

void AdVideoPreviewComponent::load (const juce::File& file,
                                    std::function<void (juce::Result)> callback)
{
    stopTimer();
    frame_ = {};
    loadReported_ = false;
    loadCallback_ = std::move (callback);
    pimpl_ = std::make_shared<Pimpl> (file);
    startTimerHz (30);
}

void AdVideoPreviewComponent::play()
{
    if (pimpl_ != nullptr)
        pimpl_->play();
}

void AdVideoPreviewComponent::pause()
{
    if (pimpl_ != nullptr)
        pimpl_->pause();
}

void AdVideoPreviewComponent::timerCallback()
{
    if (pimpl_ == nullptr)
        return;

    if (! loadReported_)
    {
        juce::String error;
        const auto readiness = pimpl_->readiness (error);
        if (readiness != Pimpl::Readiness::pending)
        {
            loadReported_ = true;
            if (loadCallback_)
                loadCallback_ (readiness == Pimpl::Readiness::ready
                                   ? juce::Result::ok()
                                   : juce::Result::fail (error));
        }
    }

    if (! loadReported_ || frameRequestPending_)
        return;

    frameRequestPending_ = true;
    const auto requestedTime = pimpl_->position();
    auto pimpl = pimpl_;
    juce::Component::SafePointer<AdVideoPreviewComponent> safe (this);
    juce::Thread::launch ([safe, pimpl, requestedTime]
    {
        auto frameHolder = std::make_shared<juce::Image> (pimpl->generateFrame (requestedTime));
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wshadow-uncaptured-local"
        juce::MessageManager::callAsync ([safe, frameHolder]
        {
            if (safe == nullptr)
                return;

            safe->frameRequestPending_ = false;
            if (frameHolder->isValid())
            {
                safe->frame_ = std::move (*frameHolder);
                safe->repaint();
            }
        });
#pragma clang diagnostic pop
    });
}

void AdVideoPreviewComponent::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colours::black);
    if (frame_.isValid())
        g.drawImageWithin (frame_, 0, 0, getWidth(), getHeight(),
                           juce::RectanglePlacement::centred | juce::RectanglePlacement::onlyReduceInSize,
                           false);

    if (pimpl_ == nullptr)
        return;

    auto controls = getLocalBounds().removeFromBottom (42).toFloat();
    g.setColour (juce::Colours::black.withAlpha (0.62f));
    g.fillRect (controls);

    juce::Path icon;
    if (pimpl_->isPlaying())
    {
        icon.addRectangle (16.0f, controls.getY() + 12.0f, 5.0f, 18.0f);
        icon.addRectangle (25.0f, controls.getY() + 12.0f, 5.0f, 18.0f);
    }
    else
    {
        icon.addTriangle (16.0f, controls.getY() + 10.0f,
                          16.0f, controls.getBottom() - 10.0f,
                          32.0f, controls.getCentreY());
    }
    g.setColour (juce::Colours::white);
    g.fillPath (icon);

    const auto duration = pimpl_->duration();
    if (duration > 0.0)
    {
        auto track = controls.reduced (48.0f, 18.0f);
        g.setColour (juce::Colours::white.withAlpha (0.3f));
        g.fillRect (track);
        track.setWidth (track.getWidth() * (float) juce::jlimit (0.0, 1.0, pimpl_->position() / duration));
        g.setColour (juce::Colours::white);
        g.fillRect (track);
    }
}

void AdVideoPreviewComponent::resized() {}

void AdVideoPreviewComponent::mouseUp (const juce::MouseEvent&)
{
    if (pimpl_ == nullptr)
        return;
    if (pimpl_->isPlaying())
        pause();
    else
        play();
    repaint();
}