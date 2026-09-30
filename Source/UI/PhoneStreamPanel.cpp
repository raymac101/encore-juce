/*
  ==============================================================================

    PhoneStreamPanel.cpp

  ==============================================================================
*/

#include "PhoneStreamPanel.h"
#include "BorderlessModalWindow.h"
#include "../Network/LyricStreamServer.h"
#include "../Network/QrCodeGenerator.h"

namespace
{
    constexpr int kDialogWidth  = 360;
    constexpr int kDialogHeight = 480;
    constexpr int kQrSize       = 260;

    constexpr auto kBgColour     = 0xff1a2030;
    constexpr auto kBorderColour = 0xff2d3a5a;
    constexpr auto kTextColour   = 0xfff7f8fa;
    constexpr auto kMutedColour  = 0xffa4b0c4;
}

//==============================================================================
PhoneStreamPanel::PhoneStreamPanel (LyricStreamServer& server)
    : server_ (server)
{
    setSize (kDialogWidth, kDialogHeight);

    titleLabel_.setText ("Phone Lyrics", juce::dontSendNotification);
    titleLabel_.setColour (juce::Label::textColourId, juce::Colour (kTextColour));
    titleLabel_.setFont (juce::Font (juce::FontOptions().withHeight (18.0f)).boldened());
    addAndMakeVisible (titleLabel_);

    closeButton_.setColour (juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
    closeButton_.setColour (juce::TextButton::textColourOnId,  juce::Colour (kMutedColour));
    closeButton_.setColour (juce::TextButton::textColourOffId, juce::Colour (kMutedColour));
    closeButton_.onClick = [this]()
    {
        if (auto* dw = findParentComponentOfClass<juce::DocumentWindow>())
            dw->exitModalState (0);
    };
    addAndMakeVisible (closeButton_);

    const auto url = server_.getUrl();

    auto qr = QrCodeGenerator::generateQrCodeImage (url, 8, 3);
    if (qr.isValid())
    {
        qrImage_.setImage (qr);
        qrImage_.setImagePlacement (juce::RectanglePlacement::centred | juce::RectanglePlacement::onlyReduceInSize);
        addAndMakeVisible (qrImage_);
    }
    else
    {
        noQrLabel_.setText ("Open this URL on your phone's browser:", juce::dontSendNotification);
        noQrLabel_.setColour (juce::Label::textColourId, juce::Colour (kMutedColour));
        noQrLabel_.setJustificationType (juce::Justification::centred);
        addAndMakeVisible (noQrLabel_);
    }

    urlEditor_.setText (url.isEmpty() ? "Could not start server" : url, juce::dontSendNotification);
    urlEditor_.setReadOnly (true);
    urlEditor_.setMultiLine (false);
    urlEditor_.setJustification (juce::Justification::centred);
    urlEditor_.setColour (juce::TextEditor::backgroundColourId, juce::Colour (0xff0f1420));
    urlEditor_.setColour (juce::TextEditor::textColourId, juce::Colour (kTextColour));
    urlEditor_.setColour (juce::TextEditor::outlineColourId, juce::Colour (kBorderColour));
    addAndMakeVisible (urlEditor_);

    statusLabel_.setJustificationType (juce::Justification::centred);
    statusLabel_.setColour (juce::Label::textColourId, juce::Colour (kMutedColour));
    addAndMakeVisible (statusLabel_);

    stopButton_.onClick = [this]()
    {
        server_.stop();
        if (auto* dw = findParentComponentOfClass<juce::DocumentWindow>())
            dw->exitModalState (0);
    };
    addAndMakeVisible (stopButton_);

    refreshStatus();
    startTimer (1000);
    resized();
}

PhoneStreamPanel::~PhoneStreamPanel() = default;

//==============================================================================
void PhoneStreamPanel::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (juce::Colour (kBgColour));
    g.fillRoundedRectangle (r, 12.f);
    g.setColour (juce::Colour (kBorderColour));
    g.drawRoundedRectangle (r.reduced (0.5f), 12.f, 1.f);
}

void PhoneStreamPanel::resized()
{
    auto area = getLocalBounds().reduced (20, 14);

    auto titleRow = area.removeFromTop (28);
    closeButton_.setBounds (titleRow.removeFromRight (28));
    titleLabel_.setBounds (titleRow);
    area.removeFromTop (10);

    auto qrArea = area.removeFromTop (kQrSize);
    qrImage_.setBounds (qrArea);
    noQrLabel_.setBounds (qrArea);

    area.removeFromTop (14);
    urlEditor_.setBounds (area.removeFromTop (32));

    area.removeFromTop (10);
    statusLabel_.setBounds (area.removeFromTop (24));

    area.removeFromTop (14);
    stopButton_.setBounds (area.removeFromTop (36));
}

//==============================================================================
void PhoneStreamPanel::timerCallback()
{
    refreshStatus();
}

void PhoneStreamPanel::refreshStatus()
{
    if (! server_.isRunning())
    {
        statusLabel_.setText ("Stopped", juce::dontSendNotification);
        return;
    }

    const auto count = server_.getConnectedClientCount();
    statusLabel_.setText (count > 0
                             ? juce::String (count) + (count == 1 ? " phone connected" : " phones connected")
                             : "Waiting for a phone to connect...",
                          juce::dontSendNotification);
}

//==============================================================================
void PhoneStreamPanel::launch (juce::Component* parent, LyricStreamServer& server)
{
    auto* content = new PhoneStreamPanel (server);
    BorderlessModalWindow::launch (parent, content, juce::Colour (kBgColour));
}
