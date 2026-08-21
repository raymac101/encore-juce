#include "AdVideoPreviewComponent.h"
#include <juce_video/juce_video.h>

struct AdVideoPreviewComponent::Pimpl
{
    explicit Pimpl (AdVideoPreviewComponent& owner)
    {
        owner.addAndMakeVisible (video);
    }

    juce::VideoComponent video { true };
};

AdVideoPreviewComponent::AdVideoPreviewComponent()
    : pimpl_ (std::make_shared<Pimpl> (*this))
{
}
AdVideoPreviewComponent::~AdVideoPreviewComponent() = default;

void AdVideoPreviewComponent::load (const juce::File& file, std::function<void (juce::Result)> callback)
{
    if (callback)
        callback (pimpl_->video.load (file));
}

void AdVideoPreviewComponent::play() { pimpl_->video.play(); }
void AdVideoPreviewComponent::pause() { pimpl_->video.stop(); }
void AdVideoPreviewComponent::paint (juce::Graphics& g) { g.fillAll (juce::Colours::black); }
void AdVideoPreviewComponent::resized() { pimpl_->video.setBounds (getLocalBounds()); }
void AdVideoPreviewComponent::mouseUp (const juce::MouseEvent&) {}
void AdVideoPreviewComponent::timerCallback() {}