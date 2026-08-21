#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

class AdVideoPreviewComponent : public juce::Component,
                                private juce::Timer
{
public:
    AdVideoPreviewComponent();
    ~AdVideoPreviewComponent() override;

    void load (const juce::File& file, std::function<void (juce::Result)> callback);
    void play();
    void pause();

    void paint (juce::Graphics& g) override;
    void resized() override;
    void mouseUp (const juce::MouseEvent&) override;

private:
    void timerCallback() override;

    struct Pimpl;
    std::shared_ptr<Pimpl> pimpl_;
    juce::Image frame_;
    std::function<void (juce::Result)> loadCallback_;
    bool loadReported_ = false;
    bool frameRequestPending_ = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AdVideoPreviewComponent)
};