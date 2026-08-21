/*
  ==============================================================================

    AdEditDialog.h

    Modal dialog for setting one ad's scheduling/rotation metadata --
    media type, duration (images only), rotation frequency (weight), and an
    optional start/end date range. Opened both right after picking a file to
    upload (pre-filled with defaults) and from the Edit button on an
    existing ad tile in AdsPage.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "../Models/AdMetadata.h"

//==============================================================================
struct AdEditResult
{
    enum class Action { Save, Cancel };

    Action      action = Action::Cancel;
    AdMetadata  meta;   // Updated (Save) or original (Cancel).
};

//==============================================================================
class AdEditDialog : public juce::Component
{
public:
    explicit AdEditDialog (const AdMetadata& ad);
    ~AdEditDialog() override;

    void paint (juce::Graphics& g) override;
    void resized() override;

    std::function<void (const AdEditResult&)> onResult;

    static void launch (juce::Component* parent,
                        const AdMetadata& ad,
                        std::function<void (const AdEditResult&)> onResult);

private:
    void closeWith (AdEditResult::Action action);
    bool validateAndApply();
    void setStatus (const juce::String& message);
    void updateDurationEnablement();

    AdMetadata meta_;

    juce::Label      titleLabel_;
    juce::TextButton closeButton_ { "X" };

    juce::Label      nameLabel_;
    juce::Label      nameValueLabel_;

    juce::Label      mediaTypeLabel_;
    juce::ComboBox   mediaTypeBox_;

    juce::Label      durationLabel_;
    juce::TextEditor durationEditor_;
    juce::Label      durationHintLabel_;

    juce::Label      frequencyLabel_;
    juce::TextEditor frequencyEditor_;

    juce::Label      startDateLabel_;
    juce::TextEditor startDateEditor_;
    juce::Label      endDateLabel_;
    juce::TextEditor endDateEditor_;

    juce::Label      statusLabel_;
    juce::TextButton cancelButton_;
    juce::TextButton saveButton_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AdEditDialog)
};
