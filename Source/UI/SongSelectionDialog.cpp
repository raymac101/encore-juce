/*
  ==============================================================================

    SongSelectionDialog.cpp

  ==============================================================================
*/

#include "SongSelectionDialog.h"
#include "../Services/ImageCache.h"

namespace
{
    constexpr int kDialogWidth  = 560;
    constexpr int kDialogHeight = 560;

    constexpr auto kBgColour     = 0xff1a2030;
    constexpr auto kPanelColour  = 0xff0d1527;
    constexpr auto kBorderColour = 0xff2d3a5a;
    constexpr auto kAccentColour = 0xff30daff;   // cyan (matches SearchPage)
    constexpr auto kTextColour   = 0xfff7f8fa;
    constexpr auto kMutedColour  = 0xffa4b0c4;

    void styleField(juce::Label& lbl, const juce::String& text)
    {
        lbl.setText(text, juce::dontSendNotification);
        lbl.setColour(juce::Label::textColourId, juce::Colour(kMutedColour));
        lbl.setFont(juce::Font(juce::FontOptions().withHeight(14.0f)));
    }

    void styleValue(juce::Label& lbl, const juce::String& text)
    {
        lbl.setText(text, juce::dontSendNotification);
        lbl.setColour(juce::Label::textColourId, juce::Colour(kTextColour));
        lbl.setFont(juce::Font(juce::FontOptions().withHeight(16.0f)).boldened());
    }

    void styleActionButton(juce::TextButton& btn, juce::Colour bg, juce::Colour fg)
    {
        btn.setColour(juce::TextButton::buttonColourId, bg);
        btn.setColour(juce::TextButton::textColourOnId, fg);
        btn.setColour(juce::TextButton::textColourOffId, fg);
    }
}

//==============================================================================
SongSelectionDialog::SongSelectionDialog(const CdgSong& song)
    : song_(song)
{
    setSize(kDialogWidth, kDialogHeight);

    // Title
    titleLabel_.setText("Song Selection", juce::dontSendNotification);
    titleLabel_.setColour(juce::Label::textColourId, juce::Colour(kTextColour));
    titleLabel_.setFont(juce::Font(juce::FontOptions().withHeight(22.0f)).boldened());
    addAndMakeVisible(titleLabel_);

    // Close (X)
    closeButton_.setColour(juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
    closeButton_.setColour(juce::TextButton::textColourOnId,  juce::Colour(kMutedColour));
    closeButton_.setColour(juce::TextButton::textColourOffId, juce::Colour(kMutedColour));
    closeButton_.onClick = [this]() { closeWithResult(SongSelectionResult::Action::Cancelled); };
    addAndMakeVisible(closeButton_);

    // Song / artist
    styleField(songFieldLabel_,    "Song:");
    styleValue(songValueLabel_,    juce::String(song_.songName));
    styleField(artistFieldLabel_,  "Artist:");
    styleValue(artistValueLabel_,  juce::String(song_.artistName));
    addAndMakeVisible(songFieldLabel_);
    addAndMakeVisible(songValueLabel_);
    addAndMakeVisible(artistFieldLabel_);
    addAndMakeVisible(artistValueLabel_);

    // Singer
    styleField(singerFieldLabel_, "Singer's Name:");
    addAndMakeVisible(singerFieldLabel_);

    singerEditor_.setTextToShowWhenEmpty("Unknown (default)", juce::Colour(kMutedColour));
    singerEditor_.setColour(juce::TextEditor::backgroundColourId, juce::Colour(kPanelColour));
    singerEditor_.setColour(juce::TextEditor::textColourId,       juce::Colour(kTextColour));
    singerEditor_.setColour(juce::TextEditor::outlineColourId,    juce::Colour(kBorderColour));
    singerEditor_.setColour(juce::TextEditor::focusedOutlineColourId, juce::Colour(kAccentColour));
    singerEditor_.setIndents(10, 6);
    addAndMakeVisible(singerEditor_);

    // Version dropdown
    styleField(versionFieldLabel_, "Song Version:");
    addAndMakeVisible(versionFieldLabel_);

    versionBox_.setColour(juce::ComboBox::backgroundColourId, juce::Colour(kPanelColour));
    versionBox_.setColour(juce::ComboBox::textColourId,       juce::Colour(kTextColour));
    versionBox_.setColour(juce::ComboBox::outlineColourId,    juce::Colour(kBorderColour));
    versionBox_.setColour(juce::ComboBox::arrowColourId,      juce::Colour(kAccentColour));

    const auto& versions = song_.version;
    const auto& codes    = song_.code;
    int count = (int) std::max(versions.size(), codes.size());
    if (count == 0)
    {
        versionBox_.addItem("Default", 1);
    }
    else
    {
        for (int i = 0; i < count; ++i)
        {
            juce::String code = (i < (int)codes.size())    ? juce::String(codes[(size_t)i])    : juce::String();
            juce::String ver  = (i < (int)versions.size()) ? juce::String(versions[(size_t)i]) : juce::String();
            juce::String text = code.isNotEmpty() && ver.isNotEmpty()
                                    ? code + " - " + ver
                                    : (code.isNotEmpty() ? code : ver);
            if (text.isEmpty()) text = "Version " + juce::String(i + 1);
            versionBox_.addItem(text, i + 1);
        }
    }
    versionBox_.setSelectedId(1, juce::dontSendNotification);
    addAndMakeVisible(versionBox_);

    // Pitch
    styleField(pitchFieldLabel_, "Change Pitch:");
    addAndMakeVisible(pitchFieldLabel_);

    auto stylePitchBtn = [](juce::TextButton& b) {
        b.setColour(juce::TextButton::buttonColourId, juce::Colour(kPanelColour));
        b.setColour(juce::TextButton::textColourOnId,  juce::Colour(kAccentColour));
        b.setColour(juce::TextButton::textColourOffId, juce::Colour(kAccentColour));
    };
    stylePitchBtn(pitchMinusButton_);
    stylePitchBtn(pitchPlusButton_);
    pitchMinusButton_.onClick = [this]() {
        if (pitchSemitones_ > -12) { --pitchSemitones_; updatePitchLabel(); }
    };
    pitchPlusButton_.onClick = [this]() {
        if (pitchSemitones_ <  12) { ++pitchSemitones_; updatePitchLabel(); }
    };
    addAndMakeVisible(pitchMinusButton_);
    addAndMakeVisible(pitchPlusButton_);

    pitchValueLabel_.setJustificationType(juce::Justification::centred);
    pitchValueLabel_.setColour(juce::Label::textColourId, juce::Colour(kTextColour));
    pitchValueLabel_.setFont(juce::Font(juce::FontOptions().withHeight(18.0f)).boldened());
    addAndMakeVisible(pitchValueLabel_);

    pitchUnitLabel_.setText("semitones", juce::dontSendNotification);
    pitchUnitLabel_.setColour(juce::Label::textColourId, juce::Colour(kMutedColour));
    pitchUnitLabel_.setFont(juce::Font(juce::FontOptions().withHeight(13.0f)));
    addAndMakeVisible(pitchUnitLabel_);

    updatePitchLabel();

    // Action buttons (order matches Angular modal)
    cancelButton_.setButtonText("Cancel");
    styleActionButton(cancelButton_, juce::Colour(kPanelColour), juce::Colour(kTextColour));
    cancelButton_.onClick = [this]() { closeWithResult(SongSelectionResult::Action::Cancelled); };
    addAndMakeVisible(cancelButton_);

    playNextButton_.setButtonText("Play Next");
    styleActionButton(playNextButton_, juce::Colour(kPanelColour), juce::Colour(kAccentColour));
    playNextButton_.onClick = [this]() { closeWithResult(SongSelectionResult::Action::PlayNext); };
    addAndMakeVisible(playNextButton_);

    playNowButton_.setButtonText("Play Now");
    styleActionButton(playNowButton_, juce::Colour(kAccentColour).withAlpha(0.25f), juce::Colour(kAccentColour));
    playNowButton_.onClick = [this]() { closeWithResult(SongSelectionResult::Action::PlayNow); };
    addAndMakeVisible(playNowButton_);

    addToQueueButton_.setButtonText("Add to Queue");
    styleActionButton(addToQueueButton_, juce::Colour(kAccentColour), juce::Colour(0xff0d1527));
    addToQueueButton_.onClick = [this]() { closeWithResult(SongSelectionResult::Action::AddToQueue); };
    addAndMakeVisible(addToQueueButton_);

    // Kick off artwork load
    if (! song_.imageUrl.empty())
    {
        juce::Component::SafePointer<SongSelectionDialog> self (this);
        artwork_ = ArtworkCache::getInstance().getOrFetch(juce::String(song_.imageUrl),
                                                          [self]() { if (self) self->repaint(); });
    }
}

SongSelectionDialog::~SongSelectionDialog() = default;

//==============================================================================
void SongSelectionDialog::updatePitchLabel()
{
    juce::String text = (pitchSemitones_ > 0 ? "+" : "") + juce::String(pitchSemitones_);
    pitchValueLabel_.setText(text, juce::dontSendNotification);
}

void SongSelectionDialog::closeWithResult(SongSelectionResult::Action action)
{
    SongSelectionResult r;
    r.action         = action;
    r.song           = song_;
    r.singerName     = singerEditor_.getText().trim();
    r.versionIndex   = juce::jmax(0, versionBox_.getSelectedId() - 1);
    r.pitchSemitones = pitchSemitones_;

    // Copy the callback locally — closing the window will delete `this`.
    auto cb = onResult;

    if (auto* dw = findParentComponentOfClass<juce::DialogWindow>())
        dw->exitModalState(0);

    // Defer the callback so it runs AFTER JUCE finishes tearing down the
    // modal dialog. Otherwise any follow-up UI work (alert windows, queue
    // refresh) can race with the dialog's destruction.
    if (cb)
        juce::MessageManager::callAsync([cb, r]() { cb(r); });
}

//==============================================================================
void SongSelectionDialog::paint(juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();

    // Background card
    g.setColour(juce::Colour(kBgColour));
    g.fillRoundedRectangle(r, 12.f);

    g.setColour(juce::Colour(kBorderColour));
    g.drawRoundedRectangle(r.reduced(0.5f), 12.f, 1.f);

    // Artwork area background
    auto art = getLocalBounds().withTrimmedTop(60).withHeight(160).reduced(20, 0);
    g.setColour(juce::Colour(kPanelColour));
    g.fillRoundedRectangle(art.toFloat(), 8.f);

    if (artwork_.isValid())
    {
        // Fit image centred inside the art rect, preserving aspect ratio.
        int iw = artwork_.getWidth();
        int ih = artwork_.getHeight();
        if (iw > 0 && ih > 0)
        {
            float scale = juce::jmin((float)art.getWidth() / iw, (float)art.getHeight() / ih);
            int dw = (int)(iw * scale);
            int dh = (int)(ih * scale);
            int dx = art.getX() + (art.getWidth()  - dw) / 2;
            int dy = art.getY() + (art.getHeight() - dh) / 2;
            g.drawImage(artwork_, dx, dy, dw, dh, 0, 0, iw, ih);
        }
    }
    else
    {
        g.setColour(juce::Colour(kMutedColour));
        g.setFont(juce::Font(juce::FontOptions().withHeight(13.0f)));
        g.drawText("No artwork", art, juce::Justification::centred);
    }
}

void SongSelectionDialog::resized()
{
    auto area = getLocalBounds().reduced(20, 16);

    // Title row
    auto titleRow = area.removeFromTop(32);
    closeButton_.setBounds(titleRow.removeFromRight(32));
    titleLabel_.setBounds(titleRow);

    // Artwork (painted, but reserve space)
    area.removeFromTop(8);
    area.removeFromTop(160);
    area.removeFromTop(10);

    auto labelRow = [&](juce::Label& field, juce::Label& value) {
        auto row = area.removeFromTop(24);
        field.setBounds(row.removeFromLeft(90));
        value.setBounds(row);
    };
    labelRow(songFieldLabel_,   songValueLabel_);
    labelRow(artistFieldLabel_, artistValueLabel_);

    area.removeFromTop(12);

    // Singer row
    {
        auto row = area.removeFromTop(32);
        singerFieldLabel_.setBounds(row.removeFromLeft(130));
        singerEditor_.setBounds(row);
    }

    area.removeFromTop(10);

    // Version row
    {
        auto row = area.removeFromTop(32);
        versionFieldLabel_.setBounds(row.removeFromLeft(130));
        versionBox_.setBounds(row);
    }

    area.removeFromTop(10);

    // Pitch row
    {
        auto row = area.removeFromTop(32);
        pitchFieldLabel_.setBounds(row.removeFromLeft(130));
        pitchMinusButton_.setBounds(row.removeFromLeft(36));
        row.removeFromLeft(6);
        pitchValueLabel_.setBounds(row.removeFromLeft(50));
        row.removeFromLeft(6);
        pitchPlusButton_.setBounds(row.removeFromLeft(36));
        row.removeFromLeft(10);
        pitchUnitLabel_.setBounds(row);
    }

    // Button bar at bottom
    auto btnRow = getLocalBounds().removeFromBottom(56).reduced(20, 12);
    int gap = 8;
    int w = (btnRow.getWidth() - gap * 3) / 4;
    cancelButton_    .setBounds(btnRow.removeFromLeft(w)); btnRow.removeFromLeft(gap);
    playNextButton_  .setBounds(btnRow.removeFromLeft(w)); btnRow.removeFromLeft(gap);
    playNowButton_   .setBounds(btnRow.removeFromLeft(w)); btnRow.removeFromLeft(gap);
    addToQueueButton_.setBounds(btnRow);
}

//==============================================================================
void SongSelectionDialog::launch(juce::Component* parent,
                                 const CdgSong& song,
                                 std::function<void(const SongSelectionResult&)> onResult)
{
    auto content = std::make_unique<SongSelectionDialog>(song);
    content->onResult = std::move(onResult);

    juce::DialogWindow::LaunchOptions opts;
    opts.content.setOwned(content.release());
    opts.dialogTitle                   = "Song Selection";
    opts.dialogBackgroundColour        = juce::Colour(kBgColour);
    opts.componentToCentreAround       = parent;
    opts.escapeKeyTriggersCloseButton  = true;
    opts.useNativeTitleBar             = false;
    opts.resizable                     = false;
    opts.launchAsync();
}
