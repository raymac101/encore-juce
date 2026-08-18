/*
  ==============================================================================

    AdPreviewDialog.h

    Read-only modal preview of one ad, opened by clicking its tile in
    AdsPage. Images render via ArtworkCache; videos use a JUCE-painted
    AVPlayer pixel stream on macOS so the preview remains reliable inside
    the modal while retaining audio and play/pause controls.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "../Models/AdMetadata.h"
#include "AdVideoPreviewComponent.h"

//==============================================================================
class AdPreviewDialog : public juce::Component
{
public:
    explicit AdPreviewDialog (const AdMetadata& ad);
    ~AdPreviewDialog() override;

    void paint (juce::Graphics& g) override;
    void resized() override;

    static void launch (juce::Component* parent, const AdMetadata& ad);

private:
    AdMetadata ad_;

    juce::Label      titleLabel_;
    juce::Label      statusLabel_;
    juce::TextButton closeButton_ { "X" };

    juce::ImageComponent imageComponent_;
    std::unique_ptr<AdVideoPreviewComponent> videoComponent_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AdPreviewDialog)
};
