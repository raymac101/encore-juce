/*
  ==============================================================================

    SongEditDialog.cpp

  ==============================================================================
*/

#include "SongEditDialog.h"
#include "../Services/ImageCache.h"

namespace
{
    constexpr int kDialogWidth  = 640;
    constexpr int kDialogHeight = 640;

    constexpr auto kBgColour     = 0xff1a2030;
    constexpr auto kPanelColour  = 0xff0d1527;
    constexpr auto kBorderColour = 0xff2d3a5a;
    constexpr auto kAccentColour = 0xff30daff;
    constexpr auto kTextColour   = 0xfff7f8fa;
    constexpr auto kMutedColour  = 0xffa4b0c4;
    constexpr auto kDangerColour = 0xffe53e3e;
    constexpr auto kStatusOkBg   = 0xff14532d;
    constexpr auto kStatusWarnBg = 0xff7c2d12;
    constexpr auto kStatusErrBg  = 0xff7f1d1d;

    void styleField(juce::Label& lbl, const juce::String& text)
    {
        lbl.setText(text, juce::dontSendNotification);
        lbl.setColour(juce::Label::textColourId, juce::Colour(kMutedColour));
        lbl.setFont(juce::Font(juce::FontOptions().withHeight(13.0f)));
    }

    void styleEditor(juce::TextEditor& ed, const juce::String& placeholder, bool readOnly = false)
    {
        ed.setTextToShowWhenEmpty(placeholder, juce::Colour(kMutedColour));
        ed.setColour(juce::TextEditor::backgroundColourId,        juce::Colour(kPanelColour));
        ed.setColour(juce::TextEditor::textColourId,              juce::Colour(kTextColour));
        ed.setColour(juce::TextEditor::outlineColourId,           juce::Colour(kBorderColour));
        ed.setColour(juce::TextEditor::focusedOutlineColourId,    juce::Colour(kAccentColour));
        ed.setIndents(8, 5);
        ed.setReadOnly(readOnly);
    }

    void styleToggle(juce::ToggleButton& t)
    {
        t.setColour(juce::ToggleButton::textColourId,        juce::Colour(kTextColour));
        t.setColour(juce::ToggleButton::tickColourId,        juce::Colour(kAccentColour));
        t.setColour(juce::ToggleButton::tickDisabledColourId, juce::Colour(kMutedColour));
    }
}

//==============================================================================
SongEditDialog::SongEditDialog(const CdgSong& song, InitialPlaylists pls)
    : song_(song)
    , initial_(pls)
{
    setSize(kDialogWidth, kDialogHeight);

    titleLabel_.setText("Edit Song", juce::dontSendNotification);
    titleLabel_.setColour(juce::Label::textColourId, juce::Colour(kTextColour));
    titleLabel_.setFont(juce::Font(juce::FontOptions().withHeight(22.0f)).boldened());
    addAndMakeVisible(titleLabel_);

    closeButton_.setColour(juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
    closeButton_.setColour(juce::TextButton::textColourOnId,  juce::Colour(kMutedColour));
    closeButton_.setColour(juce::TextButton::textColourOffId, juce::Colour(kMutedColour));
    closeButton_.onClick = [this]() { closeWith(SongEditResult::Action::Cancel); };
    addAndMakeVisible(closeButton_);

    // ── Album artwork ────────────────────────────────────────────────────────
    artworkImage_.setImagePlacement(juce::RectanglePlacement::centred
                                     | juce::RectanglePlacement::onlyReduceInSize);
    addAndMakeVisible(artworkImage_);

    artworkPlaceholder_.setText("No artwork", juce::dontSendNotification);
    artworkPlaceholder_.setColour(juce::Label::textColourId, juce::Colour(kMutedColour));
    artworkPlaceholder_.setFont(juce::Font(juce::FontOptions().withHeight(13.0f)));
    artworkPlaceholder_.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(artworkPlaceholder_);

    refreshArtwork(juce::String(song_.imageUrl));

    // ── Row: Artist [⇄] Song ─────────────────────────────────────────────────
    styleField(artistFieldLabel_, "Artist");
    addAndMakeVisible(artistFieldLabel_);
    styleEditor(artistEditor_, "Artist name");
    artistEditor_.setText(juce::String(song_.artistName), juce::dontSendNotification);
    addAndMakeVisible(artistEditor_);

    swapButton_.setColour(juce::TextButton::buttonColourId, juce::Colour(kPanelColour));
    swapButton_.setColour(juce::TextButton::textColourOnId,  juce::Colour(kAccentColour));
    swapButton_.setColour(juce::TextButton::textColourOffId, juce::Colour(kAccentColour));
    swapButton_.setTooltip("Swap Artist and Song");
    swapButton_.onClick = [this]() { doSwap(); };
    addAndMakeVisible(swapButton_);

    styleField(songFieldLabel_, "Song");
    addAndMakeVisible(songFieldLabel_);
    styleEditor(songEditor_, "Song title");
    songEditor_.setText(juce::String(song_.songName), juce::dontSendNotification);
    addAndMakeVisible(songEditor_);

    // ── Row: Song ID (read-only)   Release Date ──────────────────────────────
    styleField(songIdFieldLabel_, "Song ID");
    addAndMakeVisible(songIdFieldLabel_);
    styleEditor(songIdEditor_, "", /*readOnly*/ true);
    songIdEditor_.setText(juce::String(song_.id), juce::dontSendNotification);
    songIdEditor_.setColour(juce::TextEditor::textColourId, juce::Colour(kMutedColour));
    addAndMakeVisible(songIdEditor_);

    styleField(releaseFieldLabel_, "Release Date");
    addAndMakeVisible(releaseFieldLabel_);
    styleEditor(releaseEditor_, "YYYY-MM-DD or YYYY");
    releaseEditor_.setText(juce::String(song_.releaseDate), juce::dontSendNotification);
    addAndMakeVisible(releaseEditor_);

    // ── Row: Version   Tempo ─────────────────────────────────────────────────
    styleField(versionFieldLabel_, "Version");
    addAndMakeVisible(versionFieldLabel_);
    styleEditor(versionEditor_, "e.g. Male, Female");
    {
        juce::String joined;
        for (size_t i = 0; i < song_.version.size(); ++i)
        {
            if (i > 0) joined << ", ";
            joined << juce::String(song_.version[i]);
        }
        versionEditor_.setText(joined, juce::dontSendNotification);
    }
    addAndMakeVisible(versionEditor_);

    styleField(tempoFieldLabel_, "Tempo (BPM)");
    addAndMakeVisible(tempoFieldLabel_);
    styleEditor(tempoEditor_, "0-300");
    tempoEditor_.setInputRestrictions(4, "0123456789.");
    if (song_.tempo > 0.0)
        tempoEditor_.setText(juce::String(song_.tempo, 1), juce::dontSendNotification);
    addAndMakeVisible(tempoEditor_);

    // ── Row: Key   Duration ──────────────────────────────────────────────────
    styleField(keyFieldLabel_, "Key");
    addAndMakeVisible(keyFieldLabel_);
    styleEditor(keyEditor_, "C, Am, F#");
    keyEditor_.setText(juce::String(song_.keySignature), juce::dontSendNotification);
    addAndMakeVisible(keyEditor_);

    styleField(durationFieldLabel_, "Duration (mm:ss)");
    addAndMakeVisible(durationFieldLabel_);
    styleEditor(durationEditor_, "3:45");
    durationEditor_.setText(formatDuration(song_.durationMS), juce::dontSendNotification);
    addAndMakeVisible(durationEditor_);

    // ── Genres ───────────────────────────────────────────────────────────────
    styleField(genresFieldLabel_, "Genres (comma-separated)");
    addAndMakeVisible(genresFieldLabel_);
    styleEditor(genresEditor_, "Pop, Rock, Country");
    {
        juce::String joined;
        for (size_t i = 0; i < song_.genres.size(); ++i)
        {
            if (i > 0) joined << ", ";
            joined << juce::String(song_.genres[i]);
        }
        genresEditor_.setText(joined, juce::dontSendNotification);
    }
    addAndMakeVisible(genresEditor_);

    // ── Image URL ────────────────────────────────────────────────────────────
    styleField(imageFieldLabel_, "Album Art URL");
    addAndMakeVisible(imageFieldLabel_);
    styleEditor(imageEditor_, "https://...");
    imageEditor_.setText(juce::String(song_.imageUrl), juce::dontSendNotification);
    addAndMakeVisible(imageEditor_);

    // ── Playlist toggles ─────────────────────────────────────────────────────
    styleField(playlistsLabel_, "Add to Playlists");
    addAndMakeVisible(playlistsLabel_);

    styleToggle(addToNewToggle_);
    styleToggle(addToPopularToggle_);
    styleToggle(addToRecommendedToggle_);
    addToNewToggle_.        setToggleState(initial_.inNew,         juce::dontSendNotification);
    addToPopularToggle_.    setToggleState(initial_.inPopular,     juce::dontSendNotification);
    addToRecommendedToggle_.setToggleState(initial_.inRecommended, juce::dontSendNotification);
    addAndMakeVisible(addToNewToggle_);
    addAndMakeVisible(addToPopularToggle_);
    addAndMakeVisible(addToRecommendedToggle_);

    // ── Status + actions ─────────────────────────────────────────────────────
    statusLabel_.setColour(juce::Label::textColourId, juce::Colour(kTextColour));
    statusLabel_.setFont(juce::Font(juce::FontOptions().withHeight(12.0f)));
    statusLabel_.setJustificationType(juce::Justification::centredLeft);
    statusLabel_.setText("", juce::dontSendNotification);
    addAndMakeVisible(statusLabel_);

    getMetadataButton_.setButtonText("Get Metadata");
    getMetadataButton_.setColour(juce::TextButton::buttonColourId,
                                  juce::Colour(kAccentColour).withAlpha(0.18f));
    getMetadataButton_.setColour(juce::TextButton::textColourOnId,  juce::Colour(kAccentColour));
    getMetadataButton_.setColour(juce::TextButton::textColourOffId, juce::Colour(kAccentColour));
    getMetadataButton_.onClick = [this]() { doGetMetadata(); };
    addAndMakeVisible(getMetadataButton_);

    // All four bottom-row buttons share the same dimensions and uppercase label
    // style; only the fill colour distinguishes their semantic role.
    auto styleAction = [](juce::TextButton& b, juce::Colour bg, juce::Colour fg) {
        b.setColour(juce::TextButton::buttonColourId,  bg);
        b.setColour(juce::TextButton::textColourOnId,  fg);
        b.setColour(juce::TextButton::textColourOffId, fg);
    };

    deleteButton_.setButtonText("Delete Song");
    styleAction(deleteButton_, juce::Colour(kDangerColour).withAlpha(0.85f), juce::Colours::white);
    deleteButton_.onClick = [this]() { doDelete(); };
    addAndMakeVisible(deleteButton_);

    cancelButton_.setButtonText("Cancel");
    styleAction(cancelButton_, juce::Colour(kPanelColour), juce::Colour(kTextColour));
    cancelButton_.onClick = [this]() { closeWith(SongEditResult::Action::Cancel); };
    addAndMakeVisible(cancelButton_);

    saveButton_.setButtonText("Save");
    styleAction(saveButton_, juce::Colour(kAccentColour), juce::Colour(0xff0d1527));
    saveButton_.onClick = [this]() { closeWith(SongEditResult::Action::Save); };
    addAndMakeVisible(saveButton_);
}

SongEditDialog::~SongEditDialog() = default;

//==============================================================================
void SongEditDialog::refreshArtwork(const juce::String& url)
{
    if (url.isEmpty())
    {
        artworkImage_.setImage({});
        artworkPlaceholder_.setVisible(true);
        return;
    }

    juce::Component::SafePointer<SongEditDialog> self (this);
    auto img = ArtworkCache::getInstance().getOrFetch(url,
        [self, url]()
        {
            // Fetch completed asynchronously — only honour it if the URL
            // hasn't been replaced again in the meantime, otherwise we'd
            // overwrite the latest selection with the previous load.
            if (self == nullptr) return;
            if (self->imageEditor_.getText().trim() != url) return;

            auto reloaded = ArtworkCache::getInstance().getOrFetch(url, nullptr);
            if (reloaded.isValid())
            {
                self->artworkImage_.setImage(reloaded);
                self->artworkPlaceholder_.setVisible(false);
            }
        });

    if (img.isValid())
    {
        artworkImage_.setImage(img);
        artworkPlaceholder_.setVisible(false);
    }
    else
    {
        // Cleared until the async fetch comes back.
        artworkImage_.setImage({});
        artworkPlaceholder_.setVisible(true);
    }
}

//==============================================================================
void SongEditDialog::doSwap()
{
    auto a = artistEditor_.getText();
    artistEditor_.setText(songEditor_.getText(), juce::dontSendNotification);
    songEditor_.setText(a, juce::dontSendNotification);
}

void SongEditDialog::doGetMetadata()
{
    const auto artist = artistEditor_.getText().trim();
    const auto song   = songEditor_.getText().trim();

    if (artist.isEmpty() || song.isEmpty())
    {
        setStatus("Enter both artist and song name first.", juce::Colour(kStatusWarnBg));
        return;
    }

    if (! onFetchMetadata)
    {
        setStatus("Metadata service not configured (Spotify API not wired up yet).",
                  juce::Colour(kStatusWarnBg));
        return;
    }

    busy_ = true;
    getMetadataButton_.setEnabled(false);
    saveButton_.setEnabled(false);
    setStatus("Fetching metadata...", juce::Colour(kPanelColour));

    juce::Component::SafePointer<SongEditDialog> self (this);
    onFetchMetadata(artist, song,
        [self](bool ok, CdgSong updated, juce::String message)
        {
            juce::MessageManager::callAsync([self, ok, updated, message]()
            {
                if (self == nullptr) return;
                self->busy_ = false;
                self->getMetadataButton_.setEnabled(true);
                self->saveButton_.setEnabled(true);

                if (! ok)
                {
                    self->setStatus(message.isNotEmpty() ? message : juce::String("Metadata lookup failed."),
                                    juce::Colour(kStatusErrBg));
                    return;
                }

                // Merge non-empty fields from `updated` into the form.
                int changed = 0;
                auto setIf = [&](juce::TextEditor& ed, const juce::String& v) {
                    if (v.isNotEmpty()) { ed.setText(v, juce::dontSendNotification); ++changed; }
                };
                setIf(self->artistEditor_,   juce::String(updated.artistName));
                setIf(self->songEditor_,     juce::String(updated.songName));
                setIf(self->releaseEditor_,  juce::String(updated.releaseDate));
                setIf(self->keyEditor_,      juce::String(updated.keySignature));

                // Image URL — also reload the artwork preview when it changed.
                {
                    juce::String newUrl = juce::String(updated.imageUrl);
                    if (newUrl.isNotEmpty()
                        && newUrl != self->imageEditor_.getText().trim())
                    {
                        self->imageEditor_.setText(newUrl, juce::dontSendNotification);
                        self->refreshArtwork(newUrl);
                        ++changed;
                    }
                }
                if (updated.tempo > 0.0)
                {
                    self->tempoEditor_.setText(juce::String(updated.tempo, 1),
                                               juce::dontSendNotification);
                    ++changed;
                }
                if (updated.durationMS > 0)
                {
                    self->durationEditor_.setText(formatDuration(updated.durationMS),
                                                  juce::dontSendNotification);
                    ++changed;
                }
                if (! updated.genres.empty())
                {
                    juce::String joined;
                    for (size_t i = 0; i < updated.genres.size(); ++i)
                    {
                        if (i > 0) joined << ", ";
                        joined << juce::String(updated.genres[i]);
                    }
                    self->genresEditor_.setText(joined, juce::dontSendNotification);
                    ++changed;
                }
                if (! updated.version.empty())
                {
                    juce::String joined;
                    for (size_t i = 0; i < updated.version.size(); ++i)
                    {
                        if (i > 0) joined << ", ";
                        joined << juce::String(updated.version[i]);
                    }
                    self->versionEditor_.setText(joined, juce::dontSendNotification);
                    ++changed;
                }

                self->setStatus(changed > 0
                                    ? juce::String("Updated ") + juce::String(changed) + " field(s)."
                                    : juce::String("No useful metadata returned."),
                                juce::Colour(changed > 0 ? kStatusOkBg : kStatusWarnBg));
            });
        });
}

void SongEditDialog::doDelete()
{
    const juce::String artist = artistEditor_.getText().trim();
    const juce::String title  = songEditor_.getText().trim();
    const juce::String msg = "Delete this song from the library?\n\n"
                             "Artist: " + artist + "\n"
                             "Song: " + title + "\n\n"
                             "This action cannot be undone.";

    juce::AlertWindow::showOkCancelBox(
        juce::MessageBoxIconType::WarningIcon,
        "Delete Song",
        msg,
        "Delete",
        "Cancel",
        nullptr,
        juce::ModalCallbackFunction::create(
            [safe = juce::Component::SafePointer<SongEditDialog>(this)](int result)
            {
                if (safe != nullptr && result == 1)
                    safe->closeWith(SongEditResult::Action::Delete);
            }));
}

//==============================================================================
void SongEditDialog::closeWith(SongEditResult::Action action)
{
    if (busy_) return;

    SongEditResult r;
    r.action = action;

    if (action == SongEditResult::Action::Save)
    {
        r.song              = song_;
        r.song.artistName   = artistEditor_.getText().trim().toStdString();
        r.song.songName     = songEditor_.getText().trim().toStdString();
        r.song.releaseDate  = releaseEditor_.getText().trim().toStdString();
        r.song.keySignature = keyEditor_.getText().trim().toStdString();
        r.song.imageUrl     = imageEditor_.getText().trim().toStdString();
        r.song.tempo        = tempoEditor_.getText().trim().getDoubleValue();
        r.song.durationMS   = parseDuration(durationEditor_.getText());

        // Genres / version: comma-split, trim, drop empties
        auto split = [](const juce::String& s) {
            std::vector<std::string> out;
            juce::StringArray parts;
            parts.addTokens(s, ",", "\"");
            for (auto& p : parts)
            {
                auto t = p.trim();
                if (t.isNotEmpty()) out.push_back(t.toStdString());
            }
            return out;
        };
        r.song.genres  = split(genresEditor_.getText());
        r.song.version = split(versionEditor_.getText());
    }
    else
    {
        // Delete and Cancel pass back the original record for caller use
        // (e.g. id lookup for the delete path).
        r.song = song_;
    }

    r.addToNew         = addToNewToggle_.        getToggleState();
    r.addToPopular     = addToPopularToggle_.    getToggleState();
    r.addToRecommended = addToRecommendedToggle_.getToggleState();

    r.newChanged         = (r.addToNew         != initial_.inNew);
    r.popularChanged     = (r.addToPopular     != initial_.inPopular);
    r.recommendedChanged = (r.addToRecommended != initial_.inRecommended);

    auto cb = onResult;

    if (auto* dw = findParentComponentOfClass<juce::DocumentWindow>())
        dw->exitModalState(0);

    if (cb)
        juce::MessageManager::callAsync([cb, r]() { cb(r); });
}

//==============================================================================
void SongEditDialog::setStatus(const juce::String& msg, juce::Colour bg)
{
    statusLabel_.setText(msg, juce::dontSendNotification);
    statusLabel_.setColour(juce::Label::backgroundColourId, bg);
    statusLabel_.repaint();
}

void SongEditDialog::clearStatus()
{
    statusLabel_.setText("", juce::dontSendNotification);
    statusLabel_.setColour(juce::Label::backgroundColourId, juce::Colours::transparentBlack);
}

//==============================================================================
juce::String SongEditDialog::formatDuration(int durationMS)
{
    if (durationMS <= 0) return {};
    const int totalSec = durationMS / 1000;
    const int min = totalSec / 60;
    const int sec = totalSec % 60;
    return juce::String::formatted("%d:%02d", min, sec);
}

int SongEditDialog::parseDuration(const juce::String& s)
{
    auto trimmed = s.trim();
    if (trimmed.isEmpty()) return 0;
    int colon = trimmed.indexOfChar(':');
    if (colon < 0) return trimmed.getIntValue() * 1000;
    int min = trimmed.substring(0, colon).getIntValue();
    int sec = trimmed.substring(colon + 1).getIntValue();
    return juce::jmax(0, (min * 60 + sec) * 1000);
}

//==============================================================================
void SongEditDialog::paint(juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour(juce::Colour(kBgColour));
    g.fillRoundedRectangle(r, 12.f);
    g.setColour(juce::Colour(kBorderColour));
    g.drawRoundedRectangle(r.reduced(0.5f), 12.f, 1.f);
}

void SongEditDialog::resized()
{
    auto area = getLocalBounds().reduced(20, 14);

    // Header
    auto titleRow = area.removeFromTop(28);
    closeButton_.setBounds(titleRow.removeFromRight(28));
    titleLabel_.setBounds(titleRow);

    area.removeFromTop(8);

    // Album art (centred 110×110 box)
    {
        auto artRow = area.removeFromTop(110);
        auto box = artRow.withSizeKeepingCentre(110, 110);
        artworkImage_.setBounds(box);
        artworkPlaceholder_.setBounds(box);
    }
    area.removeFromTop(10);

    // Helper for two-column rows: label above each editor.
    auto twoColRow = [&](juce::Label& lA, juce::TextEditor& eA,
                         juce::Component* mid,           // optional middle button
                         juce::Label& lB, juce::TextEditor& eB,
                         int fieldHeight = 28)
    {
        auto labelRow = area.removeFromTop(16);
        auto editRow  = area.removeFromTop(fieldHeight + 4);
        const int midW = mid ? 36 : 0;
        const int gap  = 12;
        const int totalGap = gap * 2;
        const int colW = (area.getWidth() - midW - totalGap) / 2;

        auto lARect = labelRow.removeFromLeft(colW); labelRow.removeFromLeft(midW + totalGap);
        auto lBRect = labelRow;
        lA.setBounds(lARect);
        lB.setBounds(lBRect);

        auto eARect = editRow.removeFromLeft(colW); editRow.removeFromLeft(gap);
        if (mid)
        {
            auto midRect = editRow.removeFromLeft(midW); editRow.removeFromLeft(gap);
            mid->setBounds(midRect.withSizeKeepingCentre(midW, fieldHeight));
        }
        auto eBRect = editRow;
        eA.setBounds(eARect.withHeight(fieldHeight));
        eB.setBounds(eBRect.withHeight(fieldHeight));

        area.removeFromTop(8);
    };

    twoColRow(artistFieldLabel_,  artistEditor_,
              &swapButton_,
              songFieldLabel_,    songEditor_);

    twoColRow(songIdFieldLabel_,  songIdEditor_,
              nullptr,
              releaseFieldLabel_, releaseEditor_);

    twoColRow(versionFieldLabel_, versionEditor_,
              nullptr,
              tempoFieldLabel_,   tempoEditor_);

    twoColRow(keyFieldLabel_,     keyEditor_,
              nullptr,
              durationFieldLabel_, durationEditor_);

    // Genres (full width)
    {
        genresFieldLabel_.setBounds(area.removeFromTop(16));
        genresEditor_.setBounds(area.removeFromTop(28));
        area.removeFromTop(8);
    }

    // Image URL (full width)
    {
        imageFieldLabel_.setBounds(area.removeFromTop(16));
        imageEditor_.setBounds(area.removeFromTop(28));
        area.removeFromTop(10);
    }

    // Playlist toggles
    {
        playlistsLabel_.setBounds(area.removeFromTop(16));
        auto row = area.removeFromTop(26);
        const int w = (row.getWidth() - 16) / 3;
        addToNewToggle_.        setBounds(row.removeFromLeft(w));   row.removeFromLeft(8);
        addToPopularToggle_.    setBounds(row.removeFromLeft(w));   row.removeFromLeft(8);
        addToRecommendedToggle_.setBounds(row);
        area.removeFromTop(6);
    }

    // Status (slim, only painted when non-empty)
    statusLabel_.setBounds(area.removeFromTop(20));

    // All four action buttons share one row at the bottom.
    auto actionRow = getLocalBounds().removeFromBottom(56).reduced(20, 12);
    const int gap   = 8;
    const int btnW  = (actionRow.getWidth() - gap * 3) / 4;

    getMetadataButton_.setBounds(actionRow.removeFromLeft(btnW)); actionRow.removeFromLeft(gap);
    deleteButton_     .setBounds(actionRow.removeFromLeft(btnW)); actionRow.removeFromLeft(gap);
    cancelButton_     .setBounds(actionRow.removeFromLeft(btnW)); actionRow.removeFromLeft(gap);
    saveButton_       .setBounds(actionRow);
}

//==============================================================================
namespace
{
    // Borderless modal host — DocumentWindow with the title bar fully
    // suppressed so we don't get a duplicate "Edit Song" header above the
    // dialog's own painted title.
    class BorderlessModalWindow : public juce::DocumentWindow
    {
    public:
        BorderlessModalWindow(juce::Component* contentToOwn, juce::Colour bg)
            : juce::DocumentWindow({}, bg, /*requiredButtons*/ 0, /*addToDesktop*/ true)
        {
            setUsingNativeTitleBar(false);
            setTitleBarHeight(0);                 // hides the chrome strip
            setDropShadowEnabled(true);
            setResizable(false, false);
            setContentOwned(contentToOwn, true);
            centreAroundComponent(nullptr,
                                  contentToOwn->getWidth(),
                                  contentToOwn->getHeight());
            setVisible(true);
            enterModalState(true,
                            juce::ModalCallbackFunction::create(
                                [this](int) { delete this; }),
                            /*deleteWhenDismissed*/ false);
        }

        void closeButtonPressed() override
        {
            exitModalState(0);
        }
    };
}

void SongEditDialog::launch(juce::Component* parent,
                            const CdgSong& song,
                            InitialPlaylists pls,
                            MetadataFetcher fetcher,
                            std::function<void(const SongEditResult&)> onResult)
{
    auto* content = new SongEditDialog(song, pls);
    content->onResult        = std::move(onResult);
    content->onFetchMetadata = std::move(fetcher);

    auto* w = new BorderlessModalWindow(content, juce::Colour(kBgColour));
    if (parent != nullptr)
        w->centreAroundComponent(parent, content->getWidth(), content->getHeight());
}
