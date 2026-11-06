/*
  ==============================================================================

    SongSelectionDialog.cpp

  ==============================================================================
*/

#include "SongSelectionDialog.h"
#include "BorderlessModalWindow.h"
#include "AppLookAndFeel.h"
#include "../Services/ImageCache.h"
#include "../Localization/LocalizationManager.h"

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

    constexpr auto  kStarColour      = 0xffffc533;   // gold
    constexpr auto  kStarEmptyColour = 0x40a4b0c4;
    constexpr float kStarSize        = 13.0f;
    constexpr float kStarGap         = 2.0f;
    constexpr int   kStarsWidth      = (int) (5 * kStarSize + 4 * kStarGap) + 10;

    juce::Path makeStar(juce::Rectangle<float> r)
    {
        juce::Path p;
        p.addStar(r.getCentre(), 5, r.getWidth() * 0.22f, r.getWidth() * 0.5f);
        return p;
    }

    /** Five stars left-aligned in `area`, filled to `rating` (0-5) so
        quarter ratings like 4.25 show as a partly filled last star. */
    void drawStars(juce::Graphics& g, juce::Rectangle<float> area, double rating)
    {
        for (int i = 0; i < 5; ++i)
        {
            juce::Rectangle<float> star(area.getX() + (float) i * (kStarSize + kStarGap),
                                        area.getCentreY() - kStarSize * 0.5f,
                                        kStarSize, kStarSize);
            auto path = makeStar(star);

            g.setColour(juce::Colour(kStarEmptyColour));
            g.fillPath(path);

            const float fill = (float) juce::jlimit(0.0, 1.0, rating - i);
            if (fill > 0.0f)
            {
                juce::Graphics::ScopedSaveState save(g);
                g.reduceClipRegion(star.withWidth(star.getWidth() * fill).toNearestIntEdges());
                g.setColour(juce::Colour(kStarColour));
                g.fillPath(path);
            }
        }
    }
}

//==============================================================================
/** Draws a star rating in front of each version, both in the dropdown list
    and in the closed box. Ratings are looked up by item text, since
    drawPopupMenuItem doesn't receive the item id. */
class SongSelectionDialog::VersionLookAndFeel : public AppLookAndFeel
{
public:
    std::map<juce::String, double> ratings;

    void drawPopupMenuItem(juce::Graphics& g, const juce::Rectangle<int>& area,
                           bool isSeparator, bool isActive, bool isHighlighted, bool isTicked,
                           bool hasSubMenu, const juce::String& text, const juce::String& shortcutKeyText,
                           const juce::Drawable* icon, const juce::Colour* textColour) override
    {
        auto it = ratings.find(text);
        if (isSeparator || it == ratings.end())
        {
            AppLookAndFeel::drawPopupMenuItem(g, area, isSeparator, isActive, isHighlighted, isTicked,
                                              hasSubMenu, text, shortcutKeyText, icon, textColour);
            return;
        }

        auto r = area.reduced(1);
        if (isHighlighted && isActive)
        {
            g.setColour(findColour(juce::PopupMenu::highlightedBackgroundColourId));
            g.fillRect(r);
            g.setColour(findColour(juce::PopupMenu::highlightedTextColourId));
        }
        else
        {
            g.setColour(textColour != nullptr ? *textColour : findColour(juce::PopupMenu::textColourId));
        }

        auto textArea = r.reduced(12, 0);
        auto starArea = textArea.removeFromLeft(kStarsWidth);
        auto font = getPopupMenuFont();
        if (isTicked) font = font.boldened();
        g.setFont(font);
        g.drawFittedText(text, textArea, juce::Justification::centredLeft, 1);

        drawStars(g, starArea.toFloat(), it->second);
    }

    void getIdealPopupMenuItemSize(const juce::String& text, bool isSeparator, int standardMenuItemHeight,
                                   int& idealWidth, int& idealHeight) override
    {
        AppLookAndFeel::getIdealPopupMenuItemSize(text, isSeparator, standardMenuItemHeight,
                                                  idealWidth, idealHeight);
        if (! isSeparator)
        {
            idealWidth  += kStarsWidth;
            idealHeight  = juce::jmax(idealHeight, 30);
        }
    }

    void drawComboBox(juce::Graphics& g, int width, int height, bool isButtonDown,
                      int buttonX, int buttonY, int buttonW, int buttonH, juce::ComboBox& box) override
    {
        AppLookAndFeel::drawComboBox(g, width, height, isButtonDown, buttonX, buttonY, buttonW, buttonH, box);

        auto it = ratings.find(box.getText());
        if (it != ratings.end())
            drawStars(g, juce::Rectangle<float>(10.0f, 0.0f, (float) kStarsWidth, (float) height), it->second);
    }

    void positionComboBoxText(juce::ComboBox& box, juce::Label& label) override
    {
        AppLookAndFeel::positionComboBoxText(box, label);
        label.setBounds(label.getBounds().withTrimmedLeft(kStarsWidth));
    }
};

//==============================================================================
SongSelectionDialog::SongSelectionDialog(const CdgSong& song)
    : song_(song),
      versionLookAndFeel_(std::make_unique<VersionLookAndFeel>())
{
    auto& lm = LocalizationManager::getInstance();

    setSize(kDialogWidth, kDialogHeight);

    // Title
    titleLabel_.setText(lm.getText("song_selection.title"), juce::dontSendNotification);
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
    styleField(songFieldLabel_,    lm.getText("song_selection.song"));
    styleValue(songValueLabel_,    juce::String(song_.songName));
    styleField(artistFieldLabel_,  lm.getText("song_selection.artist"));
    styleValue(artistValueLabel_,  juce::String(song_.artistName));
    addAndMakeVisible(songFieldLabel_);
    addAndMakeVisible(songValueLabel_);
    addAndMakeVisible(artistFieldLabel_);
    addAndMakeVisible(artistValueLabel_);

    // Singer
    styleField(singerFieldLabel_, lm.getText("song_selection.singer_name"));
    addAndMakeVisible(singerFieldLabel_);

    singerEditor_.setTextToShowWhenEmpty(lm.getText("song_selection.unknown_default"), juce::Colour(kMutedColour));
    singerEditor_.setColour(juce::TextEditor::backgroundColourId, juce::Colour(kPanelColour));
    singerEditor_.setColour(juce::TextEditor::textColourId,       juce::Colour(kTextColour));
    singerEditor_.setColour(juce::TextEditor::outlineColourId,    juce::Colour(kBorderColour));
    singerEditor_.setColour(juce::TextEditor::focusedOutlineColourId, juce::Colour(kAccentColour));
    singerEditor_.setIndents(10, 6);
    addAndMakeVisible(singerEditor_);

    // Version dropdown
    styleField(versionFieldLabel_, lm.getText("song_selection.song_version"));
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
        versionBox_.addItem(lm.getText("song_selection.default_version"), 1);
    }
    else
    {
        // Highest-rated first; unrated versions sort as 0 (last). Item ids
        // stay index + 1 so closeWithResult maps back to the original slot.
        auto ratingAt = [this](int i) {
            return i < (int) song_.rating.size() ? song_.rating[(size_t) i] : 0.0;
        };
        std::vector<int> order((size_t) count);
        for (int i = 0; i < count; ++i) order[(size_t) i] = i;
        std::stable_sort(order.begin(), order.end(),
                         [&](int a, int b) { return ratingAt(a) > ratingAt(b); });

        bool anyRated = false;
        for (int i : order)
        {
            juce::String code = (i < (int)codes.size())    ? juce::String(codes[(size_t)i])    : juce::String();
            juce::String ver  = (i < (int)versions.size()) ? juce::String(versions[(size_t)i]) : juce::String();
            juce::String text = code.isNotEmpty() && ver.isNotEmpty()
                                    ? code + " - " + ver
                                    : (code.isNotEmpty() ? code : ver);
            if (text.isEmpty()) text = lm.getText("song_selection.version_prefix") + juce::String(i + 1);
            versionBox_.addItem(text, i + 1);

            // Library ratings are 0-5; anything larger (e.g. a 0-100 online
            // popularity score) isn't a star rating, so leave it out.
            const double r = ratingAt(i);
            if (r > 0.0 && r <= 5.0) anyRated = true;
            versionLookAndFeel_->ratings[text] = r <= 5.0 ? r : 0.0;
        }

        if (anyRated)
            versionBox_.setLookAndFeel(versionLookAndFeel_.get());
        versionBox_.setSelectedId(order.front() + 1, juce::dontSendNotification);
    }
    if (versionBox_.getSelectedId() == 0)
        versionBox_.setSelectedId(1, juce::dontSendNotification);
    addAndMakeVisible(versionBox_);

    // Pitch
    styleField(pitchFieldLabel_, lm.getText("song_selection.change_pitch"));
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

    pitchUnitLabel_.setText(lm.getText("song_selection.semitones"), juce::dontSendNotification);
    pitchUnitLabel_.setColour(juce::Label::textColourId, juce::Colour(kMutedColour));
    pitchUnitLabel_.setFont(juce::Font(juce::FontOptions().withHeight(13.0f)));
    addAndMakeVisible(pitchUnitLabel_);

    updatePitchLabel();

    // Action buttons (order matches Angular modal)
    cancelButton_.setButtonText(lm.getText("button.cancel"));
    styleActionButton(cancelButton_, juce::Colour(kPanelColour), juce::Colour(kTextColour));
    cancelButton_.onClick = [this]() { closeWithResult(SongSelectionResult::Action::Cancelled); };
    addAndMakeVisible(cancelButton_);

    playNextButton_.setButtonText(lm.getText("song_selection.play_next"));
    styleActionButton(playNextButton_, juce::Colour(kPanelColour), juce::Colour(kAccentColour));
    playNextButton_.onClick = [this]() { closeWithResult(SongSelectionResult::Action::PlayNext); };
    addAndMakeVisible(playNextButton_);

    playNowButton_.setButtonText(lm.getText("song_selection.play_now"));
    styleActionButton(playNowButton_, juce::Colour(kAccentColour).withAlpha(0.25f), juce::Colour(kAccentColour));
    playNowButton_.onClick = [this]() { closeWithResult(SongSelectionResult::Action::PlayNow); };
    addAndMakeVisible(playNowButton_);

    addToQueueButton_.setButtonText(lm.getText("song_selection.add_to_queue"));
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

SongSelectionDialog::~SongSelectionDialog()
{
    versionBox_.setLookAndFeel(nullptr);
}

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

    if (auto* dw = findParentComponentOfClass<juce::DocumentWindow>())
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
    auto& lm = LocalizationManager::getInstance();
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
        g.drawText(lm.getText("song_selection.no_artwork"), art, juce::Justification::centred);
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
    auto* content = new SongSelectionDialog(song);
    content->onResult = std::move(onResult);
    BorderlessModalWindow::launch(parent, content, juce::Colour(kBgColour));
}
