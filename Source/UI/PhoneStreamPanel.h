/*
  ==============================================================================

    PhoneStreamPanel.h

    Small popup shown from the "Phone Lyrics" button in BottomBar while
    LyricStreamServer is running: a QR code + URL a singer can scan/open to
    follow the lyric screen on their own phone, a live connected-client
    count, and a Stop button. Closing the panel (X) leaves streaming running
    so the operator doesn't have to keep it open; Stop actually tears the
    server down.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>

class LyricStreamServer;

class PhoneStreamPanel  : public juce::Component,
                          private juce::Timer
{
public:
    explicit PhoneStreamPanel (LyricStreamServer& server);
    ~PhoneStreamPanel() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    static void launch (juce::Component* parent, LyricStreamServer& server);

private:
    void timerCallback() override;
    void refreshStatus();

    LyricStreamServer& server_;

    juce::Label      titleLabel_;
    juce::TextButton closeButton_ { "X" };
    juce::ImageComponent qrImage_;
    juce::Label      noQrLabel_;
    juce::TextEditor urlEditor_;
    juce::Label      statusLabel_;
    juce::TextButton stopButton_ { "Stop Streaming" };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PhoneStreamPanel)
};
