/*
  ==============================================================================

    RoomEqWizard.h

    Modal dialog that walks a KJ through measuring their PA/room and
    deriving a correction curve -- a much simpler cousin of REW (Room EQ
    Wizard). Mirrors the AddSongsDialog/OnboardingWizard modal shape:
    static launch() entry point, SafePointer-guarded async callbacks.

    Phases:
      Preflight  -- blocks with a clear message if Live Vocal Input isn't
                    already enabled (AudioEngine::isLiveVocalInputActive()),
                    since this whole feature rides on that capture path.
      Setup      -- pick which mic (Vocal 1/2) and a generic mic-type
                    preset (dynamic/condenser/flat measurement mic).
      Measuring  -- "place the mic, click Measure" repeated for as many
                    positions as the host wants (minimum 2), one fixed
                    sweep level, no ramp-up.
      Result     -- computes bands via RoomEqMeasurementService, names +
                    saves the profile (UserPreferences::addRoomEqProfile),
                    applies it immediately (AudioEngine::setRoomEqBands +
                    setRoomEqEnabled(true)).

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include <vector>
#include "../Audio/AudioEngine.h"
#include "../Services/RoomEqMeasurementService.h"

class RoomEqWizard : public juce::Component
{
public:
    static constexpr int kWidth  = 560;
    static constexpr int kHeight = 460;

    /** Launches the wizard modally. `activeVenueName` pre-fills the saved
        profile's name (editable); `activeVenueId` (may be empty if no
        venue is active) tags the profile for MainComponent's per-venue
        auto-load. `onProfileSaved` fires once, on the message thread, only
        if the wizard actually reaches Save -- lets the launcher (MixerPage)
        know to refresh its saved-profile picker. */
    static void launch (juce::Component* parent, AudioEngine& engine,
                        const juce::String& activeVenueId, const juce::String& activeVenueName,
                        std::function<void()> onProfileSaved);

    RoomEqWizard (AudioEngine& engine, const juce::String& activeVenueId, const juce::String& activeVenueName);
    ~RoomEqWizard() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    std::function<void()> onProfileSaved;

private:
    enum class Phase { PreflightBlocked, Setup, Measuring, Computing, Result };
    Phase phase_ = Phase::Setup;

    AudioEngine& engine_;
    juce::String activeVenueId_;
    juce::String activeVenueName_;

    int micIndex_ = 1;
    RoomEqMeasurementService::MicType micType_ = RoomEqMeasurementService::MicType::Dynamic;
    std::vector<juce::AudioBuffer<float>> recordedPositions_;
    bool measurementInFlight_ = false;
    std::vector<RoomEqBand> computedBands_;

    //--- Header ---
    std::unique_ptr<juce::Label>      titleLabel_;
    std::unique_ptr<juce::TextButton> closeBtn_;

    //--- Preflight ---
    std::unique_ptr<juce::Label>      preflightMessage_;

    //--- Setup ---
    std::unique_ptr<juce::Label>      micLabel_;
    std::unique_ptr<juce::ComboBox>   micCombo_;
    std::unique_ptr<juce::Label>      micTypeLabel_;
    std::unique_ptr<juce::ComboBox>   micTypeCombo_;
    std::unique_ptr<juce::Label>      setupInstructions_;
    std::unique_ptr<juce::TextButton> startMeasuringBtn_;

    //--- Measuring ---
    std::unique_ptr<juce::Label>      positionLabel_;
    std::unique_ptr<juce::TextButton> measureBtn_;
    std::unique_ptr<juce::Label>      measuringStatusLabel_;
    std::unique_ptr<juce::TextButton> doneMeasuringBtn_;

    //--- Computing ---
    std::unique_ptr<juce::Label>      computingLabel_;

    //--- Result ---
    std::unique_ptr<juce::Label>      nameLabel_;
    std::unique_ptr<juce::TextEditor> nameEditor_;
    std::unique_ptr<juce::TextButton> saveBtn_;
    std::unique_ptr<juce::Label>      savedConfirmationLabel_;
    std::unique_ptr<juce::TextButton> closeAfterSaveBtn_;

    void setPhase (Phase p);
    void startMeasurement();
    void onSweepRecorded (juce::AudioBuffer<float> recorded);
    void beginComputing();
    void onBandsComputed (std::vector<RoomEqBand> bands);
    void onSave();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RoomEqWizard)
};
