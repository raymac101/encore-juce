/*
  ==============================================================================

    AdEditDialog.cpp

  ==============================================================================
*/

#include "AdEditDialog.h"
#include "BorderlessModalWindow.h"
#include "../Localization/LocalizationManager.h"

namespace
{
    constexpr int kDialogWidth  = 440;
    constexpr int kDialogHeight = 430;

    constexpr auto kBgColour     = 0xff1a2030;
    constexpr auto kPanelColour  = 0xff0d1527;
    constexpr auto kBorderColour = 0xff2d3a5a;
    constexpr auto kAccentColour = 0xff30daff;
    constexpr auto kTextColour   = 0xfff7f8fa;
    constexpr auto kMutedColour  = 0xffa4b0c4;
    constexpr auto kStatusErrBg  = 0xff7f1d1d;

    void styleField (juce::Label& lbl, const juce::String& text)
    {
        lbl.setText (text, juce::dontSendNotification);
        lbl.setColour (juce::Label::textColourId, juce::Colour (kMutedColour));
        lbl.setFont (juce::Font (juce::FontOptions().withHeight (13.0f)));
    }

    void styleEditor (juce::TextEditor& ed, const juce::String& placeholder, bool readOnly = false)
    {
        ed.setTextToShowWhenEmpty (placeholder, juce::Colour (kMutedColour));
        ed.setColour (juce::TextEditor::backgroundColourId,     juce::Colour (kPanelColour));
        ed.setColour (juce::TextEditor::textColourId,           juce::Colour (kTextColour));
        ed.setColour (juce::TextEditor::outlineColourId,        juce::Colour (kBorderColour));
        ed.setColour (juce::TextEditor::focusedOutlineColourId, juce::Colour (kAccentColour));
        ed.setIndents (8, 5);
        ed.setReadOnly (readOnly);
    }

    // Blank text -> *out = 0 (unbounded), returns true. Non-blank text that
    // fails to parse -> returns false, *out untouched. Mirrors ChartsPage's
    // custom-range date validation (YYYY-MM-DD via juce::Time::fromISO8601).
    bool parseOptionalDate (const juce::String& text, const juce::String& timeSuffix, juce::int64& out)
    {
        const auto t = text.trim();
        if (t.isEmpty())
        {
            out = 0;
            return true;
        }

        const auto time = juce::Time::fromISO8601 (t + timeSuffix);
        if (time.toMilliseconds() <= 0)
            return false;

        out = time.toMilliseconds();
        return true;
    }

    juce::String formatDateField (juce::int64 ms)
    {
        if (ms <= 0)
            return {};
        return juce::Time (ms).formatted ("%Y-%m-%d");
    }
}

//==============================================================================
AdEditDialog::AdEditDialog (const AdMetadata& ad)
    : meta_ (ad)
{
    auto& lm = LocalizationManager::getInstance();

    setSize (kDialogWidth, kDialogHeight);

    titleLabel_.setText (lm.getText ("ads.edit.title"), juce::dontSendNotification);
    titleLabel_.setColour (juce::Label::textColourId, juce::Colour (kTextColour));
    titleLabel_.setFont (juce::Font (juce::FontOptions().withHeight (20.0f)).boldened());
    addAndMakeVisible (titleLabel_);

    closeButton_.setColour (juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
    closeButton_.setColour (juce::TextButton::textColourOnId,  juce::Colour (kMutedColour));
    closeButton_.setColour (juce::TextButton::textColourOffId, juce::Colour (kMutedColour));
    closeButton_.onClick = [this]() { closeWith (AdEditResult::Action::Cancel); };
    addAndMakeVisible (closeButton_);

    styleField (nameLabel_, lm.getText ("ads.edit.name"));
    addAndMakeVisible (nameLabel_);
    nameValueLabel_.setText (meta_.name, juce::dontSendNotification);
    nameValueLabel_.setColour (juce::Label::textColourId, juce::Colour (kTextColour));
    nameValueLabel_.setFont (juce::Font (juce::FontOptions().withHeight (14.0f)).boldened());
    addAndMakeVisible (nameValueLabel_);

    styleField (mediaTypeLabel_, lm.getText ("ads.edit.media_type"));
    addAndMakeVisible (mediaTypeLabel_);
    mediaTypeBox_.addItem (lm.getText ("ads.edit.type_image"), 1);
    mediaTypeBox_.addItem (lm.getText ("ads.edit.type_video"), 2);
    mediaTypeBox_.setColour (juce::ComboBox::backgroundColourId, juce::Colour (kPanelColour));
    mediaTypeBox_.setColour (juce::ComboBox::textColourId, juce::Colour (kTextColour));
    mediaTypeBox_.setColour (juce::ComboBox::outlineColourId, juce::Colour (kBorderColour));
    mediaTypeBox_.setSelectedId (meta_.isVideo() ? 2 : 1, juce::dontSendNotification);
    mediaTypeBox_.onChange = [this]() { updateDurationEnablement(); };
    addAndMakeVisible (mediaTypeBox_);

    styleField (durationLabel_, lm.getText ("ads.edit.duration"));
    addAndMakeVisible (durationLabel_);
    styleEditor (durationEditor_, "10");
    durationEditor_.setInputRestrictions (4, "0123456789");
    durationEditor_.setText (juce::String (juce::jmax (1, meta_.durationSec)), juce::dontSendNotification);
    addAndMakeVisible (durationEditor_);

    durationHintLabel_.setText (lm.getText ("ads.edit.duration_video_hint"), juce::dontSendNotification);
    durationHintLabel_.setColour (juce::Label::textColourId, juce::Colour (kMutedColour));
    durationHintLabel_.setFont (juce::Font (juce::FontOptions().withHeight (11.0f)));
    addAndMakeVisible (durationHintLabel_);

    styleField (frequencyLabel_, lm.getText ("ads.edit.frequency"));
    addAndMakeVisible (frequencyLabel_);
    styleEditor (frequencyEditor_, "1");
    frequencyEditor_.setInputRestrictions (3, "0123456789");
    frequencyEditor_.setText (juce::String (juce::jmax (1, meta_.frequency)), juce::dontSendNotification);
    addAndMakeVisible (frequencyEditor_);

    styleField (startDateLabel_, lm.getText ("ads.edit.start_date"));
    addAndMakeVisible (startDateLabel_);
    styleEditor (startDateEditor_, lm.getText ("ads.edit.date_hint"));
    startDateEditor_.setText (formatDateField (meta_.startDateMs), juce::dontSendNotification);
    addAndMakeVisible (startDateEditor_);

    styleField (endDateLabel_, lm.getText ("ads.edit.end_date"));
    addAndMakeVisible (endDateLabel_);
    styleEditor (endDateEditor_, lm.getText ("ads.edit.date_hint"));
    endDateEditor_.setText (formatDateField (meta_.endDateMs), juce::dontSendNotification);
    addAndMakeVisible (endDateEditor_);

    statusLabel_.setColour (juce::Label::textColourId, juce::Colour (kTextColour));
    statusLabel_.setFont (juce::Font (juce::FontOptions().withHeight (12.0f)));
    statusLabel_.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (statusLabel_);

    cancelButton_.setButtonText (lm.getText ("button.cancel"));
    cancelButton_.setColour (juce::TextButton::buttonColourId, juce::Colour (kPanelColour));
    cancelButton_.setColour (juce::TextButton::textColourOnId,  juce::Colour (kTextColour));
    cancelButton_.setColour (juce::TextButton::textColourOffId, juce::Colour (kTextColour));
    cancelButton_.onClick = [this]() { closeWith (AdEditResult::Action::Cancel); };
    addAndMakeVisible (cancelButton_);

    saveButton_.setButtonText (lm.getText ("button.save"));
    saveButton_.setColour (juce::TextButton::buttonColourId, juce::Colour (kAccentColour));
    saveButton_.setColour (juce::TextButton::textColourOnId,  juce::Colour (0xff0d1527));
    saveButton_.setColour (juce::TextButton::textColourOffId, juce::Colour (0xff0d1527));
    saveButton_.onClick = [this]() { closeWith (AdEditResult::Action::Save); };
    addAndMakeVisible (saveButton_);

    updateDurationEnablement();
}

AdEditDialog::~AdEditDialog() = default;

//==============================================================================
void AdEditDialog::updateDurationEnablement()
{
    const bool isVideo = mediaTypeBox_.getSelectedId() == 2;
    durationEditor_.setEnabled (! isVideo);
    durationEditor_.setAlpha (isVideo ? 0.5f : 1.0f);
    durationHintLabel_.setVisible (isVideo);
}

void AdEditDialog::setStatus (const juce::String& message)
{
    statusLabel_.setText (message, juce::dontSendNotification);
    statusLabel_.setColour (juce::Label::backgroundColourId, juce::Colour (kStatusErrBg));
}

bool AdEditDialog::validateAndApply()
{
    auto& lm = LocalizationManager::getInstance();
    const bool isVideo = mediaTypeBox_.getSelectedId() == 2;

    int duration = durationEditor_.getText().trim().getIntValue();
    if (! isVideo && duration < 1)
    {
        setStatus (lm.getText ("ads.edit.duration_required"));
        return false;
    }
    duration = juce::jmax (1, duration);

    const int frequency = frequencyEditor_.getText().trim().getIntValue();
    if (frequency < 1)
    {
        setStatus (lm.getText ("ads.edit.frequency_required"));
        return false;
    }

    juce::int64 startMs = 0, endMs = 0;
    if (! parseOptionalDate (startDateEditor_.getText(), "T00:00:00Z", startMs)
        || ! parseOptionalDate (endDateEditor_.getText(), "T23:59:59Z", endMs))
    {
        setStatus (lm.getText ("ads.edit.invalid_date"));
        return false;
    }

    if (startMs > 0 && endMs > 0 && startMs > endMs)
    {
        setStatus (lm.getText ("ads.edit.end_before_start"));
        return false;
    }

    meta_.mediaType   = isVideo ? "video" : "image";
    meta_.durationSec = isVideo ? juce::jmax (1, meta_.durationSec) : duration;
    meta_.frequency   = frequency;
    meta_.startDateMs = startMs;
    meta_.endDateMs   = endMs;
    return true;
}

//==============================================================================
void AdEditDialog::closeWith (AdEditResult::Action action)
{
    if (action == AdEditResult::Action::Save && ! validateAndApply())
        return;

    AdEditResult r;
    r.action = action;
    r.meta = meta_;

    auto cb = onResult;

    if (auto* dw = findParentComponentOfClass<juce::DocumentWindow>())
        dw->exitModalState (0);

    if (cb)
        juce::MessageManager::callAsync ([cb, r]() { cb (r); });
}

//==============================================================================
void AdEditDialog::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (juce::Colour (kBgColour));
    g.fillRoundedRectangle (r, 12.f);
    g.setColour (juce::Colour (kBorderColour));
    g.drawRoundedRectangle (r.reduced (0.5f), 12.f, 1.f);
}

void AdEditDialog::resized()
{
    auto area = getLocalBounds().reduced (20, 14);

    auto titleRow = area.removeFromTop (28);
    closeButton_.setBounds (titleRow.removeFromRight (28));
    titleLabel_.setBounds (titleRow);
    area.removeFromTop (10);

    nameLabel_.setBounds (area.removeFromTop (14));
    nameValueLabel_.setBounds (area.removeFromTop (20));
    area.removeFromTop (10);

    auto typeRow = area.removeFromTop (16);
    mediaTypeLabel_.setBounds (typeRow);
    mediaTypeBox_.setBounds (area.removeFromTop (28));
    area.removeFromTop (10);

    {
        auto row = area.removeFromTop (16);
        const int half = (row.getWidth() - 12) / 2;
        durationLabel_.setBounds (row.removeFromLeft (half));
        row.removeFromLeft (12);
        frequencyLabel_.setBounds (row);

        auto editRow = area.removeFromTop (28);
        durationEditor_.setBounds (editRow.removeFromLeft (half));
        editRow.removeFromLeft (12);
        frequencyEditor_.setBounds (editRow);

        durationHintLabel_.setBounds (area.removeFromTop (14));
    }
    area.removeFromTop (6);

    {
        auto row = area.removeFromTop (16);
        const int half = (row.getWidth() - 12) / 2;
        startDateLabel_.setBounds (row.removeFromLeft (half));
        row.removeFromLeft (12);
        endDateLabel_.setBounds (row);

        auto editRow = area.removeFromTop (28);
        startDateEditor_.setBounds (editRow.removeFromLeft (half));
        editRow.removeFromLeft (12);
        endDateEditor_.setBounds (editRow);
    }
    area.removeFromTop (10);

    statusLabel_.setBounds (area.removeFromTop (20));

    auto actionRow = getLocalBounds().removeFromBottom (56).reduced (20, 12);
    const int gap  = 10;
    const int btnW = (actionRow.getWidth() - gap) / 2;
    cancelButton_.setBounds (actionRow.removeFromLeft (btnW));
    actionRow.removeFromLeft (gap);
    saveButton_.setBounds (actionRow);
}

//==============================================================================
void AdEditDialog::launch (juce::Component* parent,
                           const AdMetadata& ad,
                           std::function<void (const AdEditResult&)> onResult)
{
    auto* content = new AdEditDialog (ad);
    content->onResult = std::move (onResult);
    BorderlessModalWindow::launch (parent, content, juce::Colour (kBgColour));
}
