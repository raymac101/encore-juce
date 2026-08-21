/*
  ==============================================================================

    RoomEqWizard.cpp

  ==============================================================================
*/

#include "RoomEqWizard.h"
#include "../Services/UserPreferences.h"
#include "../Localization/LocalizationManager.h"

namespace
{
    constexpr uint32_t kBg          = 0xff141b26;
    constexpr uint32_t kPanel       = 0xff1c2735;
    constexpr uint32_t kAccent      = 0xff2dd4bf;
    constexpr uint32_t kBtnNormal   = 0xff253555;
    constexpr uint32_t kTextPrimary = 0xffd3dce9;
    constexpr uint32_t kTextDim     = 0xff7f8b9c;
    constexpr uint32_t kDanger      = 0xffff6b6b;

    juce::String micTypeKey (RoomEqMeasurementService::MicType t)
    {
        switch (t)
        {
            case RoomEqMeasurementService::MicType::Dynamic:   return "roomeq.mictype.dynamic";
            case RoomEqMeasurementService::MicType::Condenser: return "roomeq.mictype.condenser";
            case RoomEqMeasurementService::MicType::Flat:      return "roomeq.mictype.flat";
        }
        return {};
    }

    //==========================================================================
    class RoomEqWizardBorderlessWindow : public juce::DocumentWindow
    {
    public:
        RoomEqWizardBorderlessWindow (juce::Component* content, juce::Colour bg)
            : juce::DocumentWindow ({}, bg, 0, true)
        {
            setUsingNativeTitleBar (false);
            setTitleBarHeight (0);
            setDropShadowEnabled (true);
            setResizable (false, false);
            setContentOwned (content, true);
            setVisible (true);
            enterModalState (true,
                             juce::ModalCallbackFunction::create ([this] (int) { delete this; }),
                             false);
        }
        void closeButtonPressed() override { exitModalState (0); }
    };
}

//==============================================================================
void RoomEqWizard::launch (juce::Component* parent, AudioEngine& engine,
                           const juce::String& activeVenueId, const juce::String& activeVenueName,
                           std::function<void()> onProfileSaved)
{
    auto* dlg = new RoomEqWizard (engine, activeVenueId, activeVenueName);
    dlg->onProfileSaved = std::move (onProfileSaved);

    auto* w = new RoomEqWizardBorderlessWindow (dlg, juce::Colour (kBg));
    if (parent != nullptr)
        w->centreAroundComponent (parent, dlg->getWidth(), dlg->getHeight());
}

//==============================================================================
RoomEqWizard::RoomEqWizard (AudioEngine& engine, const juce::String& activeVenueId, const juce::String& activeVenueName)
    : engine_ (engine), activeVenueId_ (activeVenueId), activeVenueName_ (activeVenueName)
{
    auto& lm = LocalizationManager::getInstance();

    //--- Header ---
    titleLabel_ = std::make_unique<juce::Label> ("title", lm.getText ("roomeq.wizard.title"));
    titleLabel_->setFont (juce::Font (juce::FontOptions().withHeight (19.0f)).boldened());
    titleLabel_->setColour (juce::Label::textColourId, juce::Colour (kTextPrimary));
    addAndMakeVisible (*titleLabel_);

    closeBtn_ = std::make_unique<juce::TextButton> ("X");
    closeBtn_->setColour (juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
    closeBtn_->setColour (juce::TextButton::textColourOffId, juce::Colour (kTextDim));
    closeBtn_->onClick = [this]
    {
        if (auto* dw = findParentComponentOfClass<juce::DocumentWindow>())
            dw->exitModalState (0);
    };
    addAndMakeVisible (*closeBtn_);

    //--- Preflight ---
    preflightMessage_ = std::make_unique<juce::Label> ("preflight", lm.getText ("roomeq.wizard.preflight.message"));
    preflightMessage_->setColour (juce::Label::textColourId, juce::Colour (kTextPrimary));
    preflightMessage_->setJustificationType (juce::Justification::centred);
    preflightMessage_->setFont (juce::Font (juce::FontOptions().withHeight (15.0f)));
    addAndMakeVisible (*preflightMessage_);

    //--- Setup ---
    micLabel_ = std::make_unique<juce::Label> ("micLabel", lm.getText ("roomeq.wizard.setup.mic"));
    micLabel_->setColour (juce::Label::textColourId, juce::Colour (kTextDim));
    addAndMakeVisible (*micLabel_);

    micCombo_ = std::make_unique<juce::ComboBox> ("micCombo");
    micCombo_->addItem (lm.getText ("roomeq.mic.vocal1"), 1);
    micCombo_->addItem (lm.getText ("roomeq.mic.vocal2"), 2);
    micCombo_->setSelectedId (1, juce::dontSendNotification);
    micCombo_->onChange = [this] { micIndex_ = micCombo_->getSelectedId(); };
    addAndMakeVisible (*micCombo_);

    micTypeLabel_ = std::make_unique<juce::Label> ("micTypeLabel", lm.getText ("roomeq.wizard.setup.mictype"));
    micTypeLabel_->setColour (juce::Label::textColourId, juce::Colour (kTextDim));
    addAndMakeVisible (*micTypeLabel_);

    micTypeCombo_ = std::make_unique<juce::ComboBox> ("micTypeCombo");
    micTypeCombo_->addItem (lm.getText ("roomeq.mictype.dynamic"),   1);
    micTypeCombo_->addItem (lm.getText ("roomeq.mictype.condenser"), 2);
    micTypeCombo_->addItem (lm.getText ("roomeq.mictype.flat"),      3);
    micTypeCombo_->setSelectedId (1, juce::dontSendNotification);
    micTypeCombo_->onChange = [this]
    {
        switch (micTypeCombo_->getSelectedId())
        {
            case 1: micType_ = RoomEqMeasurementService::MicType::Dynamic;   break;
            case 2: micType_ = RoomEqMeasurementService::MicType::Condenser; break;
            case 3: micType_ = RoomEqMeasurementService::MicType::Flat;      break;
            default: break;
        }
    };
    addAndMakeVisible (*micTypeCombo_);

    setupInstructions_ = std::make_unique<juce::Label> ("setupInstructions", lm.getText ("roomeq.wizard.setup.instructions"));
    setupInstructions_->setColour (juce::Label::textColourId, juce::Colour (kTextDim));
    setupInstructions_->setJustificationType (juce::Justification::topLeft);
    setupInstructions_->setFont (juce::Font (juce::FontOptions().withHeight (13.5f)));
    addAndMakeVisible (*setupInstructions_);

    startMeasuringBtn_ = std::make_unique<juce::TextButton> (lm.getText ("roomeq.wizard.setup.start"));
    startMeasuringBtn_->setColour (juce::TextButton::buttonColourId, juce::Colour (kAccent));
    startMeasuringBtn_->setColour (juce::TextButton::textColourOffId, juce::Colour (kBg));
    startMeasuringBtn_->onClick = [this] { setPhase (Phase::Measuring); };
    addAndMakeVisible (*startMeasuringBtn_);

    //--- Measuring ---
    positionLabel_ = std::make_unique<juce::Label> ("positionLabel");
    positionLabel_->setColour (juce::Label::textColourId, juce::Colour (kTextPrimary));
    positionLabel_->setJustificationType (juce::Justification::centred);
    positionLabel_->setFont (juce::Font (juce::FontOptions().withHeight (15.0f)));
    addAndMakeVisible (*positionLabel_);

    measureBtn_ = std::make_unique<juce::TextButton> (lm.getText ("roomeq.wizard.measure.button"));
    measureBtn_->setColour (juce::TextButton::buttonColourId, juce::Colour (kAccent));
    measureBtn_->setColour (juce::TextButton::textColourOffId, juce::Colour (kBg));
    measureBtn_->onClick = [this] { startMeasurement(); };
    addAndMakeVisible (*measureBtn_);

    measuringStatusLabel_ = std::make_unique<juce::Label> ("measuringStatus");
    measuringStatusLabel_->setColour (juce::Label::textColourId, juce::Colour (kTextDim));
    measuringStatusLabel_->setJustificationType (juce::Justification::centred);
    addAndMakeVisible (*measuringStatusLabel_);

    doneMeasuringBtn_ = std::make_unique<juce::TextButton> (lm.getText ("roomeq.wizard.measure.done"));
    doneMeasuringBtn_->setColour (juce::TextButton::buttonColourId, juce::Colour (kBtnNormal));
    doneMeasuringBtn_->setColour (juce::TextButton::textColourOffId, juce::Colour (kTextPrimary));
    doneMeasuringBtn_->setEnabled (false);
    doneMeasuringBtn_->onClick = [this] { beginComputing(); };
    addAndMakeVisible (*doneMeasuringBtn_);

    //--- Computing ---
    computingLabel_ = std::make_unique<juce::Label> ("computingLabel", lm.getText ("roomeq.wizard.result.computing"));
    computingLabel_->setColour (juce::Label::textColourId, juce::Colour (kTextPrimary));
    computingLabel_->setJustificationType (juce::Justification::centred);
    addAndMakeVisible (*computingLabel_);

    //--- Result ---
    nameLabel_ = std::make_unique<juce::Label> ("nameLabel", lm.getText ("roomeq.wizard.result.namelabel"));
    nameLabel_->setColour (juce::Label::textColourId, juce::Colour (kTextDim));
    addAndMakeVisible (*nameLabel_);

    nameEditor_ = std::make_unique<juce::TextEditor> ("nameEditor");
    nameEditor_->setColour (juce::TextEditor::backgroundColourId, juce::Colour (kBtnNormal));
    nameEditor_->setColour (juce::TextEditor::textColourId, juce::Colour (kTextPrimary));
    nameEditor_->setText (activeVenueName_.isNotEmpty() ? activeVenueName_
                                                         : lm.getText ("roomeq.wizard.result.defaultname"),
                          juce::dontSendNotification);
    addAndMakeVisible (*nameEditor_);

    saveBtn_ = std::make_unique<juce::TextButton> (lm.getText ("roomeq.wizard.result.save"));
    saveBtn_->setColour (juce::TextButton::buttonColourId, juce::Colour (kAccent));
    saveBtn_->setColour (juce::TextButton::textColourOffId, juce::Colour (kBg));
    saveBtn_->onClick = [this] { onSave(); };
    addAndMakeVisible (*saveBtn_);

    savedConfirmationLabel_ = std::make_unique<juce::Label> ("savedConfirmation");
    savedConfirmationLabel_->setColour (juce::Label::textColourId, juce::Colour (kAccent));
    savedConfirmationLabel_->setJustificationType (juce::Justification::centred);
    savedConfirmationLabel_->setVisible (false);
    addAndMakeVisible (*savedConfirmationLabel_);

    closeAfterSaveBtn_ = std::make_unique<juce::TextButton> (lm.getText ("roomeq.wizard.result.close"));
    closeAfterSaveBtn_->setColour (juce::TextButton::buttonColourId, juce::Colour (kBtnNormal));
    closeAfterSaveBtn_->setColour (juce::TextButton::textColourOffId, juce::Colour (kTextPrimary));
    closeAfterSaveBtn_->setVisible (false);
    closeAfterSaveBtn_->onClick = [this]
    {
        if (auto* dw = findParentComponentOfClass<juce::DocumentWindow>())
            dw->exitModalState (0);
    };
    addAndMakeVisible (*closeAfterSaveBtn_);

    // Sized last, once every child component above exists -- setSize()
    // synchronously triggers resized(), which lays out those children.
    setSize (kWidth, kHeight);
    setPhase (engine_.isLiveVocalInputActive() ? Phase::Setup : Phase::PreflightBlocked);
}

RoomEqWizard::~RoomEqWizard() = default;

//==============================================================================
void RoomEqWizard::setPhase (Phase p)
{
    phase_ = p;
    auto& lm = LocalizationManager::getInstance();

    preflightMessage_->setVisible (p == Phase::PreflightBlocked);

    const bool setup = (p == Phase::Setup);
    micLabel_->setVisible (setup);
    micCombo_->setVisible (setup);
    micTypeLabel_->setVisible (setup);
    micTypeCombo_->setVisible (setup);
    setupInstructions_->setVisible (setup);
    startMeasuringBtn_->setVisible (setup);

    const bool measuring = (p == Phase::Measuring);
    positionLabel_->setVisible (measuring);
    measureBtn_->setVisible (measuring);
    measuringStatusLabel_->setVisible (measuring);
    doneMeasuringBtn_->setVisible (measuring);
    if (measuring)
    {
        positionLabel_->setText (
            lm.getTextWithParams ("roomeq.wizard.measure.position",
                                  juce::StringArray (juce::String ((int) recordedPositions_.size() + 1))),
            juce::dontSendNotification);
        measuringStatusLabel_->setText (
            lm.getTextWithParams ("roomeq.wizard.measure.recorded",
                                  juce::StringArray (juce::String ((int) recordedPositions_.size()))),
            juce::dontSendNotification);
        doneMeasuringBtn_->setEnabled (recordedPositions_.size() >= 2 && ! measurementInFlight_);
        measureBtn_->setEnabled (! measurementInFlight_);
    }

    computingLabel_->setVisible (p == Phase::Computing);

    const bool result = (p == Phase::Result);
    nameLabel_->setVisible (result);
    nameEditor_->setVisible (result);
    saveBtn_->setVisible (result && ! savedConfirmationLabel_->isVisible());

    resized();
}

//==============================================================================
void RoomEqWizard::startMeasurement()
{
    if (measurementInFlight_)
        return;

    measurementInFlight_ = true;
    measureBtn_->setEnabled (false);
    doneMeasuringBtn_->setEnabled (false);
    measuringStatusLabel_->setText (LocalizationManager::getInstance().getText ("roomeq.wizard.measure.inprogress"),
                                    juce::dontSendNotification);

    const double sampleRate = engine_.getCurrentSampleRate();
    auto sweep = RoomEqMeasurementService::generateSweep (sampleRate);

    juce::Component::SafePointer<RoomEqWizard> safe (this);
    const bool started = engine_.playAndRecordSweep (sweep, micIndex_, [safe] (juce::AudioBuffer<float> recorded)
    {
        if (safe != nullptr)
            safe->onSweepRecorded (std::move (recorded));
    });

    if (! started)
    {
        // playAndRecordSweep still invokes the callback (with an empty
        // buffer) on failure, via callAsync -- nothing further to do here.
    }
}

void RoomEqWizard::onSweepRecorded (juce::AudioBuffer<float> recorded)
{
    measurementInFlight_ = false;

    if (recorded.getNumSamples() > 0)
        recordedPositions_.push_back (std::move (recorded));

    setPhase (Phase::Measuring);
}

//==============================================================================
void RoomEqWizard::beginComputing()
{
    if (recordedPositions_.size() < 2)
        return;

    setPhase (Phase::Computing);

    juce::Component::SafePointer<RoomEqWizard> safe (this);
    auto positions = recordedPositions_;
    const double sampleRate = engine_.getCurrentSampleRate();
    const auto micType = micType_;

    juce::Thread::launch ([safe, positions, sampleRate, micType]()
    {
        auto bands = RoomEqMeasurementService::computeCorrectionBands (positions, sampleRate, micType);

        juce::MessageManager::callAsync ([safe, bands]() mutable
        {
            if (safe != nullptr)
                safe->onBandsComputed (std::move (bands));
        });
    });
}

void RoomEqWizard::onBandsComputed (std::vector<RoomEqBand> bands)
{
    computedBands_ = std::move (bands);
    setPhase (Phase::Result);
}

//==============================================================================
void RoomEqWizard::onSave()
{
    auto& lm = LocalizationManager::getInstance();
    const juce::String label = nameEditor_->getText().trim().isNotEmpty()
        ? nameEditor_->getText().trim()
        : lm.getText ("roomeq.wizard.result.defaultname");

    UserPreferences::RoomEqProfile profile;
    profile.id          = juce::String (juce::Time::currentTimeMillis());
    profile.label        = label;
    profile.venueId      = activeVenueId_;
    profile.micType      = micType_ == RoomEqMeasurementService::MicType::Dynamic   ? "dynamic"
                          : micType_ == RoomEqMeasurementService::MicType::Condenser ? "condenser"
                                                                                      : "flat";
    profile.bands        = RoomEqMeasurementService::toPrefBands (computedBands_);
    profile.createdAtMs  = juce::Time::currentTimeMillis();

    auto& prefs = UserPreferences::getInstance();
    prefs.addRoomEqProfile (profile);
    prefs.setSelectedRoomEqProfileId (profile.id);
    prefs.setRoomEqBands (profile.bands);
    prefs.setRoomEqEnabled (true);

    engine_.setRoomEqBands (computedBands_);
    engine_.setRoomEqEnabled (true);

    saveBtn_->setVisible (false);
    nameEditor_->setEnabled (false);
    savedConfirmationLabel_->setText (
        lm.getTextWithParams ("roomeq.wizard.result.saved", juce::StringArray (label)),
        juce::dontSendNotification);
    savedConfirmationLabel_->setVisible (true);
    closeAfterSaveBtn_->setVisible (true);
    resized();

    if (onProfileSaved)
        onProfileSaved();
}

//==============================================================================
void RoomEqWizard::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (kBg));
    g.setColour (juce::Colour (kPanel));
    g.fillRoundedRectangle (getLocalBounds().toFloat().reduced (8.0f), 10.0f);
}

void RoomEqWizard::resized()
{
    auto area = getLocalBounds().reduced (24);

    auto header = area.removeFromTop (32);
    closeBtn_->setBounds (header.removeFromRight (28));
    titleLabel_->setBounds (header);

    area.removeFromTop (16);

    switch (phase_)
    {
        case Phase::PreflightBlocked:
        {
            preflightMessage_->setBounds (area.removeFromTop (120));
            break;
        }

        case Phase::Setup:
        {
            auto row1 = area.removeFromTop (28);
            micLabel_->setBounds (row1.removeFromLeft (140));
            micCombo_->setBounds (row1.removeFromLeft (220));
            area.removeFromTop (12);

            auto row2 = area.removeFromTop (28);
            micTypeLabel_->setBounds (row2.removeFromLeft (140));
            micTypeCombo_->setBounds (row2.removeFromLeft (220));
            area.removeFromTop (16);

            setupInstructions_->setBounds (area.removeFromTop (100));
            area.removeFromTop (16);
            startMeasuringBtn_->setBounds (area.removeFromTop (40).withSizeKeepingCentre (200, 40));
            break;
        }

        case Phase::Measuring:
        {
            positionLabel_->setBounds (area.removeFromTop (60));
            area.removeFromTop (12);
            measureBtn_->setBounds (area.removeFromTop (44).withSizeKeepingCentre (180, 44));
            area.removeFromTop (16);
            measuringStatusLabel_->setBounds (area.removeFromTop (28));
            area.removeFromTop (20);
            doneMeasuringBtn_->setBounds (area.removeFromTop (36).withSizeKeepingCentre (220, 36));
            break;
        }

        case Phase::Computing:
        {
            computingLabel_->setBounds (area.removeFromTop (80));
            break;
        }

        case Phase::Result:
        {
            if (! savedConfirmationLabel_->isVisible())
            {
                nameLabel_->setBounds (area.removeFromTop (24));
                nameEditor_->setBounds (area.removeFromTop (32));
                area.removeFromTop (20);
                saveBtn_->setBounds (area.removeFromTop (40).withSizeKeepingCentre (200, 40));
            }
            else
            {
                nameLabel_->setBounds (area.removeFromTop (24));
                nameEditor_->setBounds (area.removeFromTop (32));
                area.removeFromTop (20);
                savedConfirmationLabel_->setBounds (area.removeFromTop (28));
                area.removeFromTop (16);
                closeAfterSaveBtn_->setBounds (area.removeFromTop (36).withSizeKeepingCentre (160, 36));
            }
            break;
        }
    }
}
