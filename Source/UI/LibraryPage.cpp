/*
  ==============================================================================

    LibraryPage.cpp
    Created: 22 Apr 2026
    Author:  GitHub Copilot

  ==============================================================================
*/

#include "LibraryPage.h"
#include "AddSongsDialog.h"
#include "MenuTheme.h"
#include "../Services/ApiService.h"
#include "../Services/UserPreferences.h"
#include "../Audio/KeyBpmAnalyzer.h"
#include "../Services/GlobalProgressService.h"
#include "../Services/SongbookStorageService.h"
#include "../Localization/LocalizationManager.h"

#include <unordered_set>
#include <unordered_map>
#include <atomic>
#include <cmath>

//==============================================================================
// Helper: style a stat label
//==============================================================================
static void styleStatLabel(juce::Label* lbl, uint32_t textColour)
{
    lbl->setFont(juce::Font(juce::FontOptions().withHeight(13.f)));
    lbl->setColour(juce::Label::textColourId, juce::Colour(textColour));
    lbl->setJustificationType(juce::Justification::centredLeft);
}

static bool needsRemoteMetadata(const CdgSong& song)
{
    return ! song.hasMetadata();
}

// Gated on durationVerified ALONE, not on tempo/keySignature being non-empty.
// runLocalAudioAnalysis() always sets durationVerified together with (but
// unconditionally of) the tempo/key attempt in the same completion step --
// for a real chunk of songs, KeyBpmAnalyzer::analyze() legitimately can't
// produce a confident tempo/key estimate (quiet intros, sparse arrangements,
// etc.), which is a deterministic, repeatable outcome for that file, not a
// transient failure. Gating on tempo/keySignature too meant those songs
// could never satisfy this check no matter how many times they were
// (successfully) reprocessed, so they got permanently re-queued on every
// single session forever. durationVerified reflects "this song has been
// through a real analysis attempt" regardless of whether tempo/key came
// back with an answer, which is what should stop the re-queueing.
static bool needsAudioAnalysis(const CdgSong& song)
{
    return ! song.durationVerified;
}

namespace
{
    struct AudioAnalysisState
    {
        juce::Component::SafePointer<LibraryPage> owner;
        std::vector<size_t> targets;
        size_t position = 0;
        int updatedCount = 0;
        int generation = 0; // captured at start; see audioAnalysisGeneration_
        std::vector<CdgSong> catalogBuffer; // flushed periodically -- see kCatalogFlushInterval
    };

    constexpr int kAudioAnalysisCheckpointInterval = 200;
    constexpr int kAudioAnalysisTimeoutMs = 30000; // see the watchdog in runLocalAudioAnalysis()
}

//==============================================================================
LibraryPage::LibraryPage()
    : juce::Timer()
    , contentHolder_ (std::make_unique<ContentHolder>())
{
    setOpaque(true);
    addAndMakeVisible(viewport_);
    viewport_.setViewedComponent(contentHolder_.get(), false);
    // Horizontal too: contentHolder_ has a minimum width floor (see
    // resized()) that can exceed a narrow viewport, clipping content with
    // no way to reach it otherwise.
    viewport_.setScrollBarsShown(true, true);

    auto& lm = LocalizationManager::getInstance();

    //--------------------------------------------------------------------------
    // Title
    titleLabel_ = std::make_unique<juce::Label>();
    titleLabel_->setText(lm.getText("library.title"), juce::dontSendNotification);
    titleLabel_->setFont(juce::Font(juce::FontOptions().withHeight(22.f)).boldened());
    titleLabel_->setColour(juce::Label::textColourId, juce::Colours::white);
    titleLabel_->setJustificationType(juce::Justification::centredLeft);
    contentHolder_->addAndMakeVisible(*titleLabel_);

    //--------------------------------------------------------------------------
    // Path section
    pathLabel_ = std::make_unique<juce::Label>();
    pathLabel_->setText(lm.getText("library.path_label"), juce::dontSendNotification);
    pathLabel_->setFont(juce::Font(juce::FontOptions().withHeight(13.f)));
    pathLabel_->setColour(juce::Label::textColourId, juce::Colour(kTextSecond));
    contentHolder_->addAndMakeVisible(*pathLabel_);

    pathEditor_ = std::make_unique<juce::TextEditor>();
    pathEditor_->setMultiLine(false);
    pathEditor_->setFont(juce::Font(juce::FontOptions().withHeight(13.f)));
    pathEditor_->setColour(juce::TextEditor::backgroundColourId, juce::Colour(0xff0d1527));
    pathEditor_->setColour(juce::TextEditor::textColourId, juce::Colour(kTextPrimary));
    pathEditor_->setColour(juce::TextEditor::outlineColourId, juce::Colour(kAccent).withAlpha(0.4f));
    pathEditor_->setColour(juce::TextEditor::focusedOutlineColourId, juce::Colour(kAccent));
    contentHolder_->addAndMakeVisible(*pathEditor_);

    browseBtn_ = std::make_unique<juce::TextButton>("...");
    browseBtn_->setColour(juce::TextButton::buttonColourId, juce::Colour(kBtnNormal));
    browseBtn_->setColour(juce::TextButton::textColourOffId, juce::Colours::white);
    browseBtn_->onClick = [this]() { onInitialSongLoad(); };
    contentHolder_->addAndMakeVisible(*browseBtn_);

    //--------------------------------------------------------------------------
    // Progress section (hidden until a scan starts)
    progressLabel_ = std::make_unique<juce::Label>();
    progressLabel_->setText(lm.getText("library.scanning"), juce::dontSendNotification);
    progressLabel_->setFont(juce::Font(juce::FontOptions().withHeight(13.f)));
    progressLabel_->setColour(juce::Label::textColourId, juce::Colour(kTextSecond));
    progressLabel_->setVisible(false);
    contentHolder_->addAndMakeVisible(*progressLabel_);

    progressBar_ = std::make_unique<juce::ProgressBar>(progressValue_);
    progressBar_->setColour(juce::ProgressBar::backgroundColourId, juce::Colour(0xff0d1527));
    progressBar_->setColour(juce::ProgressBar::foregroundColourId, juce::Colour(kAccent));
    progressBar_->setVisible(false);
    contentHolder_->addAndMakeVisible(*progressBar_);

    currentSongLabel_ = std::make_unique<juce::Label>();
    currentSongLabel_->setFont(juce::Font(juce::FontOptions().withHeight(11.f)));
    currentSongLabel_->setColour(juce::Label::textColourId, juce::Colour(kTextSecond));
    currentSongLabel_->setVisible(false);
    contentHolder_->addAndMakeVisible(*currentSongLabel_);

    //--------------------------------------------------------------------------
    // Status message label
    messageLabel_ = std::make_unique<juce::Label>();
    messageLabel_->setFont(juce::Font(juce::FontOptions().withHeight(12.f)));
    messageLabel_->setColour(juce::Label::textColourId, juce::Colour(kTextSecond));
    messageLabel_->setVisible(false);
    contentHolder_->addAndMakeVisible(*messageLabel_);

    //--------------------------------------------------------------------------
    // Buttons
    auto makeBtn = [this](const juce::String& text,
                          std::unique_ptr<juce::TextButton>& btn,
                          std::function<void()> action)
    {
        btn = std::make_unique<juce::TextButton>(text);
        btn->setColour(juce::TextButton::buttonColourId,  juce::Colour(kBtnNormal));
        btn->setColour(juce::TextButton::textColourOffId, juce::Colours::white);
        btn->onClick = std::move(action);
        contentHolder_->addAndMakeVisible(*btn);
    };

    makeBtn(lm.getText("library.btn_initial_load"), initialSongLoadBtn_,
            [this]() { onInitialSongLoad(); });
    makeBtn(lm.getText("library.btn_add_songs"),     addSongsBtn_,
            [this]() { onAddSongs(); });
    makeBtn(lm.getText("library.btn_get_metadata"),  getMetaDataBtn_,
            [this]() { onGetMetaData(); });
    makeBtn(lm.getText("library.btn_edit_genres"),   editGenresBtn_,
            [this]() { onEditGenres(); });

    //--------------------------------------------------------------------------
    // Stats labels
    auto makeStatLabel = [this](std::unique_ptr<juce::Label>& lbl)
    {
        lbl = std::make_unique<juce::Label>();
        styleStatLabel(lbl.get(), kTextPrimary);
        contentHolder_->addAndMakeVisible(*lbl);
    };

    makeStatLabel(statsTotalLabel_);
    makeStatLabel(statsMetaLabel_);
    makeStatLabel(statsAnalyzedLabel_);
    makeStatLabel(statsCDGLabel_);
    makeStatLabel(statsZipLabel_);
    makeStatLabel(statsMP4Label_);
    makeStatLabel(statsM4ALabel_);
    makeStatLabel(statsXMLLabel_);
    makeStatLabel(statsUnknownLabel_);

    //--------------------------------------------------------------------------
    // Background audio analysis status row (hidden until there's work pending)
    audioAnalysisStatusLabel_ = std::make_unique<juce::Label>();
    styleStatLabel(audioAnalysisStatusLabel_.get(), kTextPrimary);
    contentHolder_->addChildComponent(*audioAnalysisStatusLabel_); // added but stays hidden until updateAudioAnalysisUI() shows it

    audioAnalysisPaused_ = UserPreferences::getInstance().getAudioAnalysisPaused();
    audioAnalysisPauseBtn_ = std::make_unique<juce::TextButton>(audioAnalysisPaused_ ? "Resume" : "Pause");
    audioAnalysisPauseBtn_->setColour(juce::TextButton::buttonColourId,  juce::Colour(kBtnNormal));
    audioAnalysisPauseBtn_->setColour(juce::TextButton::textColourOffId, juce::Colours::white);
    audioAnalysisPauseBtn_->onClick = [this]()
    {
        audioAnalysisPaused_ = ! audioAnalysisPaused_;
        UserPreferences::getInstance().setAudioAnalysisPaused(audioAnalysisPaused_);
        updateAudioAnalysisUI();
        if (! audioAnalysisPaused_ && audioAnalysisResume_ != nullptr)
            (*audioAnalysisResume_)();
    };
    contentHolder_->addChildComponent(*audioAnalysisPauseBtn_);

    //--------------------------------------------------------------------------
    // Open SQLite index and give the scanner a pointer to it so scan results
    // are persisted to the database (and loaded from it on startup).
    if (songDb_.open())
        scanner_.setSongDatabase(&songDb_);

    //--------------------------------------------------------------------------
    // Wire up scanner callbacks
    scanner_.onProgress = [this](int cur, int tot, juce::String song) {
        // Already on message thread (dispatched by LibraryScanner)
        if (tot > 0)
            progressValue_ = (double)cur / (double)tot;
        currentSongLabel_->setText(song, juce::dontSendNotification);
        progressBar_->repaint();
    };

    scanner_.onComplete = [this](std::vector<CdgSong> scannedSongs,
                                 LibraryScanner::ScanStats scannedStats)
    {
        // Already on message thread
        songs_ = std::move(scannedSongs);
        stats_ = scannedStats;

        // Persist to disk + Storage
        persistSongbook();

        // Persist the selected library root so startup can restore it and
        // avoid prompting again once the user has fixed a path mismatch.
        const auto selectedRoot = pathEditor_->getText().trim();
        if (selectedRoot.isNotEmpty())
            UserPreferences::getInstance().setLibraryPath(selectedRoot);

        // Path editor intentionally left unchanged — it shows the scan root
        // that the user selected, not an individual song's subdirectory.

        refreshStats();
        setScanningState(false);
        showMessage(LocalizationManager::getInstance().getText("library.songs_loaded").replace("{n}", juce::String((int)songs_.size())), false);

        if (onSongbookChanged) onSongbookChanged(songs_);

        std::vector<size_t> needsMetadata;
        needsMetadata.reserve(songs_.size());
        for (size_t i = 0; i < songs_.size(); ++i)
            if (needsRemoteMetadata(songs_[i]))
                needsMetadata.push_back(i);

        if (! needsMetadata.empty())
            fetchMetadataForImportedSongs(std::move(needsMetadata), lastScanWasAppend_);

        std::vector<size_t> needsAudio;
        needsAudio.reserve(songs_.size());
        for (size_t i = 0; i < songs_.size(); ++i)
            if (needsAudioAnalysis(songs_[i]))
                needsAudio.push_back(i);

        if (! needsAudio.empty())
            runLocalAudioAnalysis(std::move(needsAudio));
    };

    scanner_.onError = [this](juce::String err) {
        setScanningState(false);
        showMessage(err, true);
        reportStatus({});
    };

    //--------------------------------------------------------------------------
    // Decorative header panels -- drawn against contentHolder_'s own bounds
    // (not LibraryPage's) so they scroll along with the form instead of
    // staying fixed while the text/buttons scroll past them.
    contentHolder_->onPaint = [this](juce::Graphics& g)
    {
        auto bounds  = contentHolder_->getLocalBounds();
        auto reduced = bounds.reduced(20, 16);

        // Header panel height tracks its actual content (title/path/progress-or-message)
        // rather than a fixed constant, so it doesn't overlap the button row when the
        // progress bar is hidden and the content above the buttons is shorter.
        int headerTop    = reduced.getY();
        int headerHeight = juce::jmax(80, initialSongLoadBtn_->getY() - 20 - headerTop);
        MenuTheme::drawHeaderPanel(g, juce::Rectangle<int>(reduced.getX(), headerTop, reduced.getWidth(), headerHeight));

        // Stats panel starts below the buttons, with spacing
        int panelY = initialSongLoadBtn_->getBottom() + 20;
        int panelH = bounds.getHeight() - panelY - 16;
        if (panelH > 0)
            MenuTheme::drawHeaderPanel(g, juce::Rectangle<int>(20, panelY, bounds.getWidth() - 40, panelH));
    };

    //--------------------------------------------------------------------------
    // Load existing songbook from disk without blocking first paint.
    loadSongbookAsync();
}

//==============================================================================
void LibraryPage::reportStatus(const juce::String& message)
{
    // persistSongbook() (and hence this) can be called from background
    // threads (e.g. onGetMetaData's juce::Thread::launch), but onStatusMessage
    // ultimately mutates BottomBar UI state, so always marshal onto the
    // message thread. The token bump itself happens synchronously so an
    // immediately-following call from any thread still invalidates any
    // already-queued stale callback.
    ++statusMessageToken_;
    if (! onStatusMessage)
        return;

    if (juce::MessageManager::getInstance()->isThisTheMessageThread())
    {
        onStatusMessage(message);
    }
    else
    {
        juce::Component::SafePointer<LibraryPage> safeThis(this);
        juce::MessageManager::callAsync([safeThis, message]
        {
            if (safeThis != nullptr && safeThis->onStatusMessage)
                safeThis->onStatusMessage(message);
        });
    }
}

//==============================================================================
void LibraryPage::persistSongbook()
{
    scanner_.saveSongbook(songs_);

    if (activeVenueId_.isEmpty())
        return;

    reportStatus("Uploading Songbook...");
    const int myToken = statusMessageToken_;

    juce::Component::SafePointer<LibraryPage> safeThis(this);
    SongbookStorageService::getInstance().uploadLocalSongbook(activeVenueId_,
        [safeThis, myToken](bool ok, juce::String error)
        {
            if (! ok)
                DBG ("[Songbook] Storage upload failed: " << error);

            if (safeThis == nullptr || safeThis->statusMessageToken_ != myToken)
                return;

            safeThis->reportStatus(ok ? "Song Library Updated!" : "Songbook Sync Failed");
            const int doneToken = safeThis->statusMessageToken_;

            juce::Timer::callAfterDelay(2500, [safeThis, doneToken]
            {
                if (safeThis != nullptr && safeThis->statusMessageToken_ == doneToken)
                    safeThis->reportStatus({});
            });
        });
}

//==============================================================================
LibraryPage::~LibraryPage()
{
    stopTimer();
    scanner_.stopScan();
    if (globalScanTaskId_ != 0)
    {
        GlobalProgressService::getInstance().endTask(globalScanTaskId_);
        globalScanTaskId_ = 0;
    }
    scanner_.setSongDatabase(nullptr);
    songDb_.close();
}

//==============================================================================
void LibraryPage::loadSongbook()
{
    songs_ = scanner_.loadSongbook();
    stats_ = LibraryScanner::computeStats(songs_);
    refreshStats();

    // Restore the scan root from the saved file (written by saveSongbook)
    {
        juce::File rootFile = scanner_.getSongbookFile().getSiblingFile("scanRoot.txt");
        if (rootFile.existsAsFile())
        {
            juce::String savedPath = rootFile.loadFileAsString().trim();
            if (savedPath.isNotEmpty())
            {
                pathEditor_->setText(savedPath, juce::dontSendNotification);
                UserPreferences::getInstance().setLibraryPath(savedPath);
            }
        }
        else if (! songs_.empty() && ! songs_[0].filePath.empty())
        {
            // Legacy fallback — walk up until we reach a directory that looks like the root
            juce::File dir(juce::String(songs_[0].filePath[0]));
            pathEditor_->setText(dir.getFullPathName(), juce::dontSendNotification);
            UserPreferences::getInstance().setLibraryPath(dir.getFullPathName());
        }
        else
        {
            // Final fallback: previously persisted preference.
            const auto preferredPath = UserPreferences::getInstance().getLibraryPath().trim();
            if (preferredPath.isNotEmpty())
                pathEditor_->setText(preferredPath, juce::dontSendNotification);
        }
    }
}

void LibraryPage::loadSongbookAsync()
{
    juce::Component::SafePointer<LibraryPage> safeThis(this);
    juce::Thread::launch(juce::Thread::Priority::low, [safeThis]()
    {
        if (safeThis == nullptr)
            return;

        auto loadedSongs = safeThis->scanner_.loadSongbook();
        auto loadedStats = LibraryScanner::computeStats(loadedSongs);

        juce::MessageManager::callAsync([safeThis, loadedSongs = std::move(loadedSongs), loadedStats]() mutable
        {
            if (safeThis == nullptr)
                return;

            safeThis->songs_ = std::move(loadedSongs);
            safeThis->stats_ = loadedStats;
            safeThis->refreshStats();

            // Restore scan-root path when available.
            juce::File rootFile = safeThis->scanner_.getSongbookFile().getSiblingFile("scanRoot.txt");
            if (rootFile.existsAsFile())
            {
                juce::String savedPath = rootFile.loadFileAsString().trim();
                if (savedPath.isNotEmpty())
                {
                    safeThis->pathEditor_->setText(savedPath, juce::dontSendNotification);
                    UserPreferences::getInstance().setLibraryPath(savedPath);
                }
            }
            else if (! safeThis->songs_.empty() && ! safeThis->songs_[0].filePath.empty())
            {
                juce::File dir(juce::String(safeThis->songs_[0].filePath[0]));
                safeThis->pathEditor_->setText(dir.getFullPathName(), juce::dontSendNotification);
                UserPreferences::getInstance().setLibraryPath(dir.getFullPathName());
            }
            else
            {
                const auto preferredPath = UserPreferences::getInstance().getLibraryPath().trim();
                if (preferredPath.isNotEmpty())
                    safeThis->pathEditor_->setText(preferredPath, juce::dontSendNotification);
            }

            if (safeThis->onSongbookChanged)
                safeThis->onSongbookChanged(safeThis->songs_);

            // Unlike scanner_.onComplete (fired only after an explicit scan/
            // import), a plain startup load otherwise never re-checks for
            // pending audio analysis -- without this, songs left unverified
            // from before this feature existed (or from an interrupted prior
            // sweep) would only ever get corrected by manually re-running a
            // full scan.
            std::vector<size_t> needsAudio;
            needsAudio.reserve(safeThis->songs_.size());
            for (size_t i = 0; i < safeThis->songs_.size(); ++i)
                if (needsAudioAnalysis(safeThis->songs_[i]))
                    needsAudio.push_back(i);

            if (! needsAudio.empty())
                safeThis->runLocalAudioAnalysis(std::move(needsAudio));
        });
    });
}

//==============================================================================
// Match a candidate against `target`:
//   1. By non-empty id (exact match), else
//   2. By case-insensitive (artistName + "|" + songName).
static int findSongIndex(const std::vector<CdgSong>& list, const CdgSong& target)
{
    if (! target.id.empty())
    {
        for (size_t i = 0; i < list.size(); ++i)
            if (list[i].id == target.id)
                return (int) i;
    }
    auto lcA = juce::String(target.artistName).toLowerCase();
    auto lcS = juce::String(target.songName).toLowerCase();
    for (size_t i = 0; i < list.size(); ++i)
    {
        if (juce::String(list[i].artistName).toLowerCase() == lcA
         && juce::String(list[i].songName).toLowerCase()   == lcS)
            return (int) i;
    }
    return -1;
}

bool LibraryPage::upsertSong(const CdgSong& song)
{
    int idx = findSongIndex(songs_, song);

    CdgSong merged = song;
    if (idx >= 0)
    {
        // Preserve file-related fields the dialog never touches.
        const auto& existing = songs_[(size_t) idx];
        if (merged.id.empty())       merged.id       = existing.id;
        if (merged.fullPath.empty()) merged.fullPath  = existing.fullPath;
        if (merged.fileName.empty()) merged.fileName  = existing.fileName;
        if (merged.filePath.empty()) merged.filePath  = existing.filePath;
        if (merged.fileType.empty()) merged.fileType  = existing.fileType;
        if (merged.fileDate == 0)    merged.fileDate  = existing.fileDate;
        if (merged.fileSize == 0)    merged.fileSize  = existing.fileSize;
        if (merged.code.empty())     merged.code      = existing.code;
        // Always preserve the original addedAt so edits don't reset the timestamp.
        if (existing.addedAt > 0)    merged.addedAt   = existing.addedAt;

        songs_[(size_t) idx] = merged;
    }
    else
    {
        // First time this song enters the library — stamp it.
        if (merged.addedAt == 0)
            merged.addedAt = juce::Time::currentTimeMillis();
        songs_.push_back(merged);
    }

    // Persist to songbook.json + SQLite index.
    persistSongbook();
    if (songDb_.isOpen())
        songDb_.insertOrReplace(merged);

    refreshStats();
    if (onSongbookChanged) onSongbookChanged(songs_);
    return true;
}

bool LibraryPage::deleteSong(const CdgSong& song)
{
    int idx = findSongIndex(songs_, song);
    if (idx < 0) return false;

    auto removedId = songs_[(size_t) idx].id;
    songs_.erase(songs_.begin() + idx);

    persistSongbook();
    if (songDb_.isOpen() && ! removedId.empty())
        songDb_.remove(juce::String(removedId));

    refreshStats();
    if (onSongbookChanged) onSongbookChanged(songs_);
    return true;
}

//==============================================================================
void LibraryPage::updateAllText()
{
    auto& lm = LocalizationManager::getInstance();
    titleLabel_->setText(lm.getText("library.title"),      juce::dontSendNotification);
    pathLabel_ ->setText(lm.getText("library.path_label"), juce::dontSendNotification);
    initialSongLoadBtn_->setButtonText(lm.getText("library.btn_initial_load"));
    addSongsBtn_       ->setButtonText(lm.getText("library.btn_add_songs"));
    getMetaDataBtn_    ->setButtonText(lm.getText("library.btn_get_metadata"));
    editGenresBtn_     ->setButtonText(lm.getText("library.btn_edit_genres"));
    refreshStats();
}

//==============================================================================
// juce::Timer
//==============================================================================
void LibraryPage::timerCallback()
{
    if (! scanner_.isScanning.load()) return;

    int cur = scanner_.progressCurrent.load();
    int tot = scanner_.progressTotal.load();

    if (tot > 0)
        progressValue_ = (double)cur / (double)tot;
    else
        progressValue_ = -1.0; // indeterminate spinner

    juce::String songName;
    {
        juce::ScopedLock sl(scanner_.currentSongLock);
        songName = scanner_.currentSong;
    }
    currentSongLabel_->setText(songName, juce::dontSendNotification);
    progressBar_->repaint();
}

//==============================================================================
// paint / resized
//==============================================================================
void LibraryPage::paint(juce::Graphics& g)
{
    MenuTheme::drawPageBackground(g, getLocalBounds());
}

void LibraryPage::resized()
{
    viewport_.setBounds(getLocalBounds());

    const int startingWidth  = juce::jmax(600, viewport_.getWidth() - viewport_.getScrollBarThickness());
    const int startingHeight = juce::jmax(contentHolder_->getHeight(), 500);
    contentHolder_->setSize(startingWidth, startingHeight);
    layoutContent();

    // Grow to fit the last stats row -- lets the viewport's scrollbar reach
    // it on any window size. layoutContent() only depends on width, so a
    // second pass at the corrected height reproduces the same positions.
    int contentBottom = statsUnknownLabel_->getBottom();
    int bottomMargin = 16;
    if (audioAnalysisPauseBtn_->isVisible())
    {
        contentBottom = juce::jmax(contentBottom, audioAnalysisPauseBtn_->getBottom());
        bottomMargin = 28; // extra breathing room below the Pause row specifically
    }
    const int neededHeight = contentBottom + bottomMargin;
    if (neededHeight != contentHolder_->getHeight())
    {
        contentHolder_->setSize(startingWidth, neededHeight);
        layoutContent();
    }
}

void LibraryPage::layoutContent()
{
    auto area   = contentHolder_->getLocalBounds().reduced(20, 16);

    // Inset all content from the decorative panel borders drawn in onPaint
    // (which sit at the `area` edges) so text/controls never touch the box.
    const int innerPad = 16;
    int  contentX = area.getX() + innerPad;
    int  w        = area.getWidth() - innerPad * 2;
    int  lineH  = 24;
    int  btnH   = 32;
    int  gap    = 12;
    int  y      = area.getY();

    // Title
    titleLabel_->setBounds(contentX, y, w, 30);
    y += 34;

    // Path label
    pathLabel_->setBounds(contentX, y, w, lineH);
    y += lineH + 4;

    // Path editor + browse button
    int browseW = 36;
    pathEditor_->setBounds(contentX, y, w - browseW - 4, lineH);
    browseBtn_->setBounds(contentX + w - browseW, y, browseW, lineH);
    y += lineH + gap;

    // Progress section
    if (progressBar_->isVisible())
    {
        progressLabel_->setBounds(contentX, y, 100, lineH);
        progressBar_->setBounds(contentX + 104, y, w - 104, lineH);
        y += lineH + 6;
        currentSongLabel_->setBounds(contentX, y, w, lineH - 4);
        y += lineH + gap;
    }

    // Message label. The trailing gap here must match the "20" the header
    // panel's height calc (see contentHolder_->onPaint) assumes sits below
    // this label -- otherwise, whenever the progress bar is hidden (so this
    // becomes the last thing above the button row), the panel is drawn too
    // short and its border cuts across the message text instead of sitting
    // below it.
    messageLabel_->setBounds(contentX, y, w, lineH - 4);
    y += messageLabel_->isVisible() ? (lineH - 4 + 20) : 8;

    // Button bar (4 buttons equally spaced)
    int btnGap  = 8;
    int numBtns = 4;
    int btnW    = (w - btnGap * (numBtns - 1)) / numBtns;
    int bx      = contentX;

    initialSongLoadBtn_->setBounds(bx, y, btnW, btnH); bx += btnW + btnGap;
    addSongsBtn_       ->setBounds(bx, y, btnW, btnH); bx += btnW + btnGap;
    getMetaDataBtn_    ->setBounds(bx, y, btnW, btnH); bx += btnW + btnGap;
    editGenresBtn_     ->setBounds(bx, y, btnW, btnH);
    y += btnH + gap + 8;

    // Stats panel — two columns
    int statsX    = contentX;
    int statsW    = (w - 16) / 2;
    int statsLineH = 22;

    auto placeStat = [&](juce::Label* lbl, int col) {
        int cx = statsX + col * (statsW + 16);
        lbl->setBounds(cx, y, statsW, statsLineH);
        if (col == 1) y += statsLineH + 4;
    };

    placeStat(statsTotalLabel_.get(),    0);
    placeStat(statsMetaLabel_.get(),     1);
    placeStat(statsCDGLabel_.get(),      0);
    placeStat(statsAnalyzedLabel_.get(), 1); // directly under Metadata Available
    placeStat(statsZipLabel_.get(),      0);
    placeStat(statsMP4Label_.get(),      1);
    placeStat(statsM4ALabel_.get(),      0);
    placeStat(statsXMLLabel_.get(),      1);
    placeStat(statsUnknownLabel_.get(),  0);
    y += statsLineH + 4; // odd number of stat rows -- last one lands alone in col 0, so placeStat's col==1 auto-advance never fires for it

    if (audioAnalysisStatusLabel_->isVisible())
    {
        audioAnalysisStatusLabel_->setBounds(statsX, y, statsW, statsLineH);
        audioAnalysisPauseBtn_->setBounds(statsX + statsW + 16, y, 90, statsLineH);
        y += statsLineH + 4;
    }
}

//==============================================================================
// Button actions
//==============================================================================
void LibraryPage::onInitialSongLoad()
{
    startFolderChooser(false /* full scan */);
}

void LibraryPage::onAddSongs()
{
    juce::File libRoot(pathEditor_->getText().trim());
    std::unordered_set<std::string> preImportIds;
    preImportIds.reserve(songs_.size());
    for (const auto& song : songs_)
        preImportIds.insert(song.id);

    AddSongsDialog::launch(this, songs_, libRoot,
        [this, preImportIds = std::move(preImportIds)](std::vector<CdgSong> mergedSongs, AddSongsDialog::ImportStats)
        {
            songs_ = std::move(mergedSongs);
            if (songDb_.isOpen())
                for (auto& s : songs_)
                    songDb_.insertOrReplace(s);
            persistSongbook();
            refreshStats();
            if (onSongbookChanged) onSongbookChanged(songs_);

            std::vector<size_t> newlyImported;
            newlyImported.reserve(songs_.size());
            for (size_t i = 0; i < songs_.size(); ++i)
            {
                if (preImportIds.find(songs_[i].id) == preImportIds.end())
                    newlyImported.push_back(i);
            }

            if (! newlyImported.empty())
            {
                if (onSongsAddedViaAddSongs)
                {
                    std::vector<CdgSong> added;
                    added.reserve(newlyImported.size());
                    for (auto index : newlyImported)
                        added.push_back(songs_[index]);
                    onSongsAddedViaAddSongs(added);
                }

                auto newlyImportedForAudio = newlyImported;
                fetchMetadataForImportedSongs(std::move(newlyImported), true);
                runLocalAudioAnalysis(std::move(newlyImportedForAudio));
            }
        });
}

        void LibraryPage::fetchMetadataForImportedSongs(std::vector<size_t> songIndices,
                                bool allowOnlineLookup)
{
    if (songIndices.empty())
        return;

    reportStatus("Adding Meta Data...");

    // Show an immediate wait state while we build/check local metadata.
    setScanningState(true);
    progressLabel_->setText("Preparing local metadata...", juce::dontSendNotification);
    currentSongLabel_->setText("Checking local metadata file. Please wait...", juce::dontSendNotification);
    progressValue_ = -1.0;
    progressBar_->repaint();
    showMessage("Checking local metadata file before online lookup...", false);
    repaint();

    // Always run a local metadata pre-pass first so previous scans in
    // meta_data.json are applied before any online lookup starts.
    const int localPrePassMatched = scanner_.applyLocalMetadata(songs_);

    std::vector<size_t> targets;
    targets.reserve(songIndices.size());
    for (auto index : songIndices)
    {
        if (index < songs_.size() && needsRemoteMetadata(songs_[index]))
            targets.push_back(index);
    }

    if (targets.empty())
    {
        persistSongbook();
        stats_ = LibraryScanner::computeStats(songs_);
        refreshStats();
        setScanningState(false);
        if (onSongbookChanged)
            onSongbookChanged(songs_);
        showMessage("Local metadata matched " + juce::String(localPrePassMatched)
                    + " songs. No online lookup needed.", false);
        return;
    }

    if (! allowOnlineLookup)
    {
        persistSongbook();
        stats_ = LibraryScanner::computeStats(songs_);
        refreshStats();
        setScanningState(false);
        if (onSongbookChanged)
            onSongbookChanged(songs_);

        showMessage("Initial load is local-only: matched "
                    + juce::String(localPrePassMatched)
                    + " songs from meta_data.json; checking "
                    + juce::String((int) targets.size())
                    + " remaining song(s) against Firebase (no Spotify calls)...", false);

        // Still skip Spotify for a full initial scan (could be tens of
        // thousands of songs), but a Firestore/shared-cache-only check is
        // cheap and quota-free -- cheaper than waiting for the next
        // 6-hour maybeSyncSharedMetadata() sweep to pick up the same gap.
        syncSharedMetadataForSongs(targets, true);
        return;
    }

    setScanningState(true);
    progressLabel_->setText(LocalizationManager::getInstance().getText("library.applying_metadata"), juce::dontSendNotification);
    progressValue_ = 0.0;
    progressBar_->repaint();
    showMessage("Local metadata matched " + juce::String(localPrePassMatched)
                + " songs. Fetching online metadata for "
                + juce::String((int) targets.size()) + " remaining songs...", false);

    constexpr int kMaxTransientRetries = 2;
    constexpr int kQueuePollMaxRounds = 4;
    constexpr int kQueuePollDelayMs = 4000;

    auto isTransientError = [](const juce::String& err) -> bool
    {
        auto msg = err.toLowerCase();
        return msg.contains("429")
            || msg.contains("too many")
            || msg.contains("rate")
            || msg.contains("timeout")
            || msg.contains("timed out")
            || msg.contains("could not connect")
            || msg.contains("temporarily")
            || msg.contains("unavailable")
            || msg.contains("503");
    };

    struct State
    {
        juce::Component::SafePointer<LibraryPage> owner;
        std::vector<size_t> targets;
        size_t position = 0;
        int updatedCount = 0;
        std::atomic<int> inFlightTimerToken { 0 };

        int localCacheHits = 0;
        int firestoreHits = 0;
        int legacyApiHits = 0;
        int queuedCount = 0;
        int failedCount = 0;
        int retryCount = 0;
        int queueRound = 0;

        std::unordered_map<size_t, int> retriesBySongIndex;
        std::unordered_set<size_t> queuedSongIndices;
    };

    auto state = std::make_shared<State>();
    state->owner   = juce::Component::SafePointer<LibraryPage>(this);
    state->targets = std::move(targets);

    auto persistRunReport = [state](int pendingQueued)
    {
        auto reportFile = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
            .getChildFile("EncoreKaraoke")
            .getChildFile("metadata_run_report.json");
        reportFile.getParentDirectory().createDirectory();

        juce::var root;
        if (reportFile.existsAsFile())
            root = juce::JSON::parse(reportFile);
        if (! root.isObject())
            root = juce::var(new juce::DynamicObject());

        auto* rootObj = root.getDynamicObject();

        juce::Array<juce::var> runs;
        auto existingRuns = rootObj->getProperty("runs");
        if (auto* arr = existingRuns.getArray())
            runs = *arr;

        juce::DynamicObject::Ptr run = new juce::DynamicObject();
        run->setProperty("timestampMs", juce::Time::currentTimeMillis());
        run->setProperty("totalTargets", (int) state->targets.size());
        run->setProperty("updated", state->updatedCount);
        run->setProperty("cache", state->localCacheHits);
        run->setProperty("firestore", state->firestoreHits);
        run->setProperty("legacyApi", state->legacyApiHits);
        run->setProperty("queued", state->queuedCount);
        run->setProperty("retries", state->retryCount);
        run->setProperty("failed", state->failedCount);
        run->setProperty("pendingQueued", pendingQueued);
        run->setProperty("queuePollRounds", state->queueRound);
        runs.add(juce::var(run.get()));

        constexpr int kMaxRuns = 25;
        while (runs.size() > kMaxRuns)
            runs.remove(0);

        rootObj->setProperty("runs", juce::var(runs));
        rootObj->setProperty("lastUpdatedMs", juce::Time::currentTimeMillis());

        reportFile.replaceWithText(juce::JSON::toString(root, true));
    };

    auto statusSummary = [state]() -> juce::String
    {
        return "Metadata: cache=" + juce::String(state->localCacheHits)
             + " firestore=" + juce::String(state->firestoreHits)
             + " api=" + juce::String(state->legacyApiHits)
             + " queued=" + juce::String(state->queuedCount)
             + " retries=" + juce::String(state->retryCount)
             + " failed=" + juce::String(state->failedCount);
    };

    auto finalize = [state, statusSummary, persistRunReport](const juce::String& suffix)
    {
        auto* owner = state->owner.getComponent();
        if (owner == nullptr)
            return;

        persistRunReport((int) state->queuedSongIndices.size());

        owner->persistSongbook();
        owner->stats_ = LibraryScanner::computeStats(owner->songs_);
        owner->refreshStats();
        owner->setScanningState(false);
        owner->showMessage(statusSummary() + suffix, false);
        if (owner->onSongbookChanged)
            owner->onSongbookChanged(owner->songs_);
    };

    auto fetchNext = std::make_shared<std::function<void(int)>>();
    auto scheduleNext = [state, fetchNext](int delayMs)
    {
        auto token = ++state->inFlightTimerToken;
        juce::Timer::callAfterDelay(juce::jmax(0, delayMs), [state, fetchNext, token]()
        {
            if (state->owner == nullptr)
                return;
            if (token != state->inFlightTimerToken.load())
                return;
            (*fetchNext)(0);
        });
    };

    auto pollQueued = std::make_shared<std::function<void()>>();
    *pollQueued = [state, statusSummary, finalize, pollQueued, kQueuePollMaxRounds, kQueuePollDelayMs]()
    {
        auto* owner = state->owner.getComponent();
        if (owner == nullptr)
            return;

        if (state->queuedSongIndices.empty())
        {
            finalize(". Queued metadata resolved.");
            return;
        }

        if (state->queueRound >= kQueuePollMaxRounds)
        {
            finalize(". " + juce::String((int) state->queuedSongIndices.size())
                     + " queued song(s) still pending in backend.");
            return;
        }

        ++state->queueRound;
        owner->progressLabel_->setText("Polling queued metadata (round "
                                       + juce::String(state->queueRound)
                                       + "/" + juce::String(kQueuePollMaxRounds)
                                       + ")",
                                       juce::dontSendNotification);
        owner->progressValue_ = -1.0;
        owner->progressBar_->repaint();
        owner->showMessage(statusSummary() + " - waiting for queued metadata...", false);

        auto pending = std::make_shared<std::vector<size_t>>();
        pending->reserve(state->queuedSongIndices.size());
        for (auto idx : state->queuedSongIndices)
            pending->push_back(idx);

        auto pollIndex = std::make_shared<size_t>(0);
        auto pollNextOne = std::make_shared<std::function<void()>>();
        *pollNextOne = [state, pending, pollIndex, pollNextOne, pollQueued, kQueuePollDelayMs]()
        {
            auto* owner = state->owner.getComponent();
            if (owner == nullptr)
                return;

            if (*pollIndex >= pending->size())
            {
                juce::Timer::callAfterDelay(kQueuePollDelayMs, [pollQueued]() { (*pollQueued)(); });
                return;
            }

            const size_t songIndex = (*pending)[(*pollIndex)++];
            if (songIndex >= owner->songs_.size())
            {
                (*pollNextOne)();
                return;
            }

            const auto current = owner->songs_[songIndex];
            ApiService::getInstance().lookupSharedMetadataOnly(
                current,
                juce::String(current.artistName),
                juce::String(current.songName),
                [state, pollNextOne, songIndex](ApiService::Result result)
                {
                    auto* ownerInner = state->owner.getComponent();
                    if (ownerInner == nullptr)
                        return;

                    if (result.ok)
                    {
                        ownerInner->songs_[songIndex] = result.song;
                        ++state->updatedCount;

                        if (result.source == ApiService::Result::Source::localCache)
                            ++state->localCacheHits;
                        else if (result.source == ApiService::Result::Source::firestore)
                            ++state->firestoreHits;

                        state->queuedSongIndices.erase(songIndex);
                    }

                    (*pollNextOne)();
                });
        };

        (*pollNextOne)();
    };

    *fetchNext = [state, fetchNext, scheduleNext, statusSummary, finalize, pollQueued, isTransientError, kMaxTransientRetries](int)
    {
        if (state->owner == nullptr)
            return;

        if (state->position >= state->targets.size())
        {
            if (! state->queuedSongIndices.empty())
                (*pollQueued)();
            else
                finalize(".");
            return;
        }

        const size_t songIndex = state->targets[state->position++];
        auto current = state->owner->songs_[(size_t) songIndex];
        auto songLabel = juce::String(current.artistName) + " - " + juce::String(current.songName);
        state->owner->currentSongLabel_->setText(
            juce::String("[") + juce::String((int) state->position) + "/"
            + juce::String((int) state->targets.size()) + "] "
            + songLabel,
            juce::dontSendNotification);
        state->owner->progressValue_ = (double) (state->position - 1) / (double) state->targets.size();
        state->owner->progressBar_->repaint();

        auto handleResult = [state, scheduleNext, songIndex, songLabel, statusSummary, isTransientError, kMaxTransientRetries](ApiService::Result result)
        {
            if (state->owner == nullptr)
                return;

            if (result.ok)
            {
                state->owner->songs_[(size_t) songIndex] = result.song;
                ++state->updatedCount;

                if (result.source == ApiService::Result::Source::localCache)
                    ++state->localCacheHits;
                else if (result.source == ApiService::Result::Source::firestore)
                    ++state->firestoreHits;
                else if (result.source == ApiService::Result::Source::legacyApi)
                    ++state->legacyApiHits;

                state->queuedSongIndices.erase(songIndex);
            }
            else
            {
                if (result.queued)
                {
                    auto inserted = state->queuedSongIndices.insert(songIndex).second;
                    if (inserted)
                        ++state->queuedCount;
                }

                const bool transient = isTransientError(result.errorMessage);
                const int retriesSoFar = state->retriesBySongIndex[songIndex];
                if (transient && retriesSoFar < kMaxTransientRetries)
                {
                    state->retriesBySongIndex[songIndex] = retriesSoFar + 1;
                    ++state->retryCount;

                    // Re-try this item by rewinding position.
                    if (state->position > 0)
                        --state->position;

                    state->owner->showMessage(statusSummary()
                        + " - transient error, retrying "
                        + songLabel, false);

                    const int retryDelayMs = 400 + (retriesSoFar * 350);
                    scheduleNext(retryDelayMs);
                    return;
                }

                ++state->failedCount;
            }

            state->owner->progressValue_ = (double) state->position / (double) state->targets.size();
            state->owner->progressBar_->repaint();
            state->owner->showMessage(statusSummary(), false);

            // Gentle pacing to avoid bursty endpoint traffic.
            int nextDelayMs = 120;

            // If we queued but did not get immediate metadata, back off.
            if (! result.ok && result.queued)
                nextDelayMs = 400;

            // Retry pressure signals from legacy API path.
            if (! result.ok)
            {
                auto msg = result.errorMessage.toLowerCase();
                if (msg.contains("429") || msg.contains("too many") || msg.contains("rate"))
                    nextDelayMs = 900;
                else if (msg.contains("could not connect") || msg.contains("timeout"))
                    nextDelayMs = 600;
            }

            scheduleNext(nextDelayMs);
        };

        // Always check local/shared metadata first; only use online path on miss.
        ApiService::getInstance().lookupSharedMetadataOnly(
            current,
            juce::String(current.artistName),
            juce::String(current.songName),
            [state, handleResult, current](ApiService::Result localResult)
            {
                if (state->owner == nullptr)
                    return;

                if (localResult.ok)
                {
                    handleResult(localResult);
                    return;
                }

                ApiService::getInstance().searchArtistAndSong(
                    current,
                    juce::String(current.artistName),
                    juce::String(current.songName),
                    [handleResult](ApiService::Result remoteResult)
                    {
                        handleResult(remoteResult);
                    });
            });
    };

    scheduleNext(0);
}

//==============================================================================
void LibraryPage::runLocalAudioAnalysis(std::vector<size_t> songIndices)
{
    std::vector<size_t> targets;
    targets.reserve(songIndices.size());
    for (auto index : songIndices)
        if (index < songs_.size() && needsAudioAnalysis(songs_[index]))
            targets.push_back(index);

    if (targets.empty())
        return;

    // Deliberately NOT reportStatus()/GlobalProgressService here -- this
    // pass can run for hours over a large library, and both of those drive
    // app-wide "busy" UI (BottomBar's status text and its spinner) that
    // would otherwise get stuck showing "analyzing" indefinitely instead of
    // the normal "Audio Ready" state. The dedicated status row + Pause
    // button on this page (see updateAudioAnalysisUI()) is where this
    // pass's progress belongs.
    audioAnalysisRunning_ = true;
    audioAnalysisTotal_ = (int) targets.size();
    audioAnalysisDone_ = 0;
    updateAudioAnalysisUI();

    auto state = std::make_shared<AudioAnalysisState>();
    state->owner = juce::Component::SafePointer<LibraryPage>(this);
    state->targets = std::move(targets);
    state->generation = audioAnalysisGeneration_;

    auto processNext = std::make_shared<std::function<void()>>();
    audioAnalysisResume_ = processNext; // lets the Pause/Resume button continue this exact pass
    *processNext = [state, processNext]()
    {
        auto* owner = state->owner.getComponent();
        if (owner == nullptr)
            return;

        // A scan/add/get-metadata operation started since this pass began --
        // songs_ may have been wholly replaced (Initial Load) or is being
        // concurrently written elsewhere, so this pass's captured indices
        // can no longer be trusted. Stop cleanly without touching songs_ or
        // persisting; the operation that bumped the generation will trigger
        // a fresh, correctly-indexed pass when it finishes.
        if (state->generation != owner->audioAnalysisGeneration_)
        {
            owner->audioAnalysisRunning_ = false;
            owner->audioAnalysisResume_.reset();
            owner->updateAudioAnalysisUI();
            return;
        }

        if (owner->audioAnalysisPaused_)
        {
            // Don't dispatch another song -- just reflect the paused state.
            // The Pause/Resume button calls (*audioAnalysisResume_)() (this
            // same processNext) to continue exactly where this left off.
            owner->updateAudioAnalysisUI();
            return;
        }

        if (state->position >= state->targets.size())
        {
            if (state->updatedCount > 0)
            {
                owner->persistSongbook();
                owner->stats_ = LibraryScanner::computeStats(owner->songs_);
                owner->refreshStats();
                if (owner->onSongbookChanged)
                    owner->onSongbookChanged(owner->songs_);
            }

            if (! state->catalogBuffer.empty())
            {
                owner->scanner_.updateLocalCatalogEntries(state->catalogBuffer);
                state->catalogBuffer.clear();
            }

            owner->audioAnalysisRunning_ = false;
            owner->audioAnalysisResume_.reset();
            owner->updateAudioAnalysisUI();

            owner->showMessage("Analyzed tempo/key/duration for " + juce::String(state->updatedCount) + " song(s).", false);
            return;
        }

        const size_t songIndex = state->targets[state->position++];
        if (songIndex >= owner->songs_.size())
        {
            (*processNext)();
            return;
        }

        // Copy off the message thread before handing to the background
        // thread -- songs_ itself is only ever read/written here, never
        // from the analysis thread, so there's no data race to reason about.
        const CdgSong current = owner->songs_[songIndex];

        owner->audioAnalysisCurrentSong_ = juce::String(current.artistName) + " - " + juce::String(current.songName);
        owner->updateAudioAnalysisUI();

        // Guards against double-handling one song: whichever of the timeout
        // below or the real completion (inside the background thread) fires
        // first wins and advances the pass; the other becomes a no-op.
        auto handled = std::make_shared<std::atomic<bool>>(false);

        // Watchdog: some libraries live on slow or unreliable network-mounted
        // volumes where a single stuck file read can hang indefinitely with
        // no error and no timeout of its own. Since this pass processes one
        // song at a time, a single hang like that would otherwise block
        // every song behind it forever -- which looks exactly like "the
        // same N songs never finish analyzing no matter how many times I
        // run it" (they never even get a chance to run). If the real
        // completion hasn't fired within kAudioAnalysisTimeoutMs, give up on
        // this song and move on; the original background thread is left to
        // finish (or keep hanging) harmlessly on its own.
        juce::Timer::callAfterDelay(kAudioAnalysisTimeoutMs, [state, processNext, songIndex, handled]()
        {
            if (handled->exchange(true))
                return;

            auto* ownerInner = state->owner.getComponent();
            if (ownerInner != nullptr && songIndex < ownerInner->songs_.size())
            {
                ownerInner->songs_[songIndex].durationVerified = true;
                ownerInner->audioAnalysisDone_ = (int) state->position;
                ownerInner->updateAudioAnalysisUI();
            }
            (*processNext)();
        });

        // Low priority (efficiency cores where available) -- this decodes
        // audio files for BPM/key/duration analysis, the same kind of work
        // as live playback, so it must never contend with the audio thread
        // if a venue is mid-show.
        juce::Thread::launch(juce::Thread::Priority::low, [state, processNext, songIndex, current, handled]()
        {
            juce::File tempFile;
            auto audioFile = KeyBpmAnalyzer::resolvePlayableAudioFile(current, 0, tempFile);

            KeyBpmAnalyzer::Result analysis;
            int measuredDurationMS = 0;
            if (audioFile.existsAsFile())
            {
                analysis = KeyBpmAnalyzer::analyze(audioFile);

                // Real duration, read from the same resolved file tempo/key
                // analysis already paid the zip-extraction cost for -- karaoke
                // edits routinely trim/extend vs. the commercial track, so
                // this is preferred over Spotify's/the catalog's durationMS.
                juce::AudioFormatManager formatManager;
                formatManager.registerBasicFormats();
                std::unique_ptr<juce::AudioFormatReader> reader (formatManager.createReaderFor (audioFile));
                if (reader != nullptr && reader->sampleRate > 0.0 && reader->lengthInSamples > 0)
                    measuredDurationMS = (int) std::round ((double) reader->lengthInSamples / reader->sampleRate * 1000.0);
            }

            if (tempFile.existsAsFile())
                tempFile.deleteFile();

            juce::MessageManager::callAsync([state, processNext, songIndex, analysis, measuredDurationMS, handled]()
            {
                if (handled->exchange(true))
                    return; // the watchdog already gave up on this song and moved on

                auto* ownerInner = state->owner.getComponent();
                const bool stale = ownerInner != nullptr && state->generation != ownerInner->audioAnalysisGeneration_;
                if (ownerInner != nullptr && ! stale && songIndex < ownerInner->songs_.size())
                {
                    bool updated = false;

                    if (analysis.ok)
                    {
                        ownerInner->songs_[songIndex].tempo = (double) analysis.bpm;
                        ownerInner->songs_[songIndex].keySignature = analysis.keySignature.toStdString();
                        updated = true;
                    }

                    // Mark verified even when the read failed (file missing/
                    // unsupported) so this song isn't retried forever -- the
                    // catalog placeholder, if any, remains as the best
                    // available answer.
                    if (measuredDurationMS > 0)
                        ownerInner->songs_[songIndex].durationMS = measuredDurationMS;
                    ownerInner->songs_[songIndex].durationVerified = true;
                    updated = true;

                    if (updated)
                    {
                        ++state->updatedCount;

                        const auto& song = ownerInner->songs_[songIndex];
                        state->catalogBuffer.push_back(song);

                        // Share with the Firebase master list too -- most
                        // venues use the same handful of karaoke vendors, so
                        // one venue's real analysis saves every other one
                        // from redoing the same work. Fire-and-forget.
                        ApiService::getInstance().submitLocalAudioAnalysis(
                            juce::String(song.artistName), juce::String(song.songName),
                            song.tempo, juce::String(song.keySignature), song.durationMS);
                    }

                    // Checkpoint locally every so often -- this pass can run
                    // for a very long time over a large library (each song
                    // costs a real decode), so persisting only once at the
                    // very end would lose all of it if the app closes
                    // mid-sweep. Local-only (no cloud upload, unlike
                    // persistSongbook()) since a full save is cheap (single
                    // SQLite transaction) but the network round-trip isn't
                    // something we want firing every 200 songs.
                    if (state->position % kAudioAnalysisCheckpointInterval == 0)
                    {
                        ownerInner->scanner_.saveSongbook(ownerInner->songs_);

                        if (! state->catalogBuffer.empty())
                        {
                            ownerInner->scanner_.updateLocalCatalogEntries(state->catalogBuffer);
                            state->catalogBuffer.clear();
                        }
                    }

                    ownerInner->audioAnalysisDone_ = (int) state->position;
                    ownerInner->updateAudioAnalysisUI();
                }

                (*processNext)();
            });
        });
    };

    (*processNext)();
}

//==============================================================================
namespace
{
    constexpr juce::int64 kMetadataSyncCooldownMs = (juce::int64) 6 * 60 * 60 * 1000; // 6 hours

    // Local state for syncSharedMetadataForSongs()'s sequential shared-cache
    // sweep -- deliberately separate from fetchMetadataForImportedSongs'
    // State struct above, since this pass never touches Spotify/the queue
    // and never shows scan-progress UI.
    struct SharedSyncState
    {
        juce::Component::SafePointer<LibraryPage> owner;
        std::vector<size_t> targets;
        size_t position = 0;
        int updatedCount = 0;
        bool updateLocalCatalog = false;
        std::vector<CdgSong> updatedSongs; // only collected when updateLocalCatalog is true
        int generation = 0; // captured at start; see audioAnalysisGeneration_
    };
}

void LibraryPage::maybeSyncSharedMetadata()
{
    auto& prefs = UserPreferences::getInstance();
    const auto now = juce::Time::currentTimeMillis();
    const auto last = prefs.getLastMetadataSyncAtMs();
    if (last > 0 && (now - last) < kMetadataSyncCooldownMs)
        return;

    std::vector<size_t> targets;
    targets.reserve(songs_.size());
    for (size_t i = 0; i < songs_.size(); ++i)
        if (needsRemoteMetadata(songs_[i]))
            targets.push_back(i);

    // Record the attempt regardless of outcome, so a library with nothing
    // left to fix doesn't get rechecked on every relaunch/venue-switch.
    prefs.setLastMetadataSyncAtMs(now);

    if (targets.empty())
        return;

    DBG ("[Metadata] startup sync: checking " << (int) targets.size()
         << " song(s) missing metadata against the shared cache");

    syncSharedMetadataForSongs(std::move(targets), true);
}

void LibraryPage::syncSharedMetadataForSongs(std::vector<size_t> songIndices, bool updateLocalCatalog)
{
    if (songIndices.empty())
        return;

    auto state = std::make_shared<SharedSyncState>();
    state->owner = juce::Component::SafePointer<LibraryPage>(this);
    state->targets = std::move(songIndices);
    state->updateLocalCatalog = updateLocalCatalog;
    state->generation = audioAnalysisGeneration_;

    auto checkNext = std::make_shared<std::function<void()>>();
    *checkNext = [state, checkNext]()
    {
        auto* owner = state->owner.getComponent();
        if (owner == nullptr)
            return;

        // See runLocalAudioAnalysis()'s identical check -- a scan/add/get-
        // metadata operation started since this sweep began, so songs_ may
        // have moved on from under it. Stop without persisting; whichever
        // operation bumped the generation will recompute and resync once
        // it's done.
        if (state->generation != owner->audioAnalysisGeneration_)
            return;

        if (state->position >= state->targets.size())
        {
            if (state->updatedCount > 0)
            {
                owner->persistSongbook();
                owner->stats_ = LibraryScanner::computeStats(owner->songs_);
                owner->refreshStats();
                if (owner->onSongbookChanged)
                    owner->onSongbookChanged(owner->songs_);

                juce::String msg = "Synced " + juce::String(state->updatedCount)
                                   + " song(s) with shared metadata from Firebase.";

                if (state->updateLocalCatalog && ! state->updatedSongs.empty())
                {
                    const bool catalogOk = owner->scanner_.updateLocalCatalogEntries(state->updatedSongs);
                    msg += catalogOk ? " Local catalog file updated."
                                      : " (Local catalog file could not be updated.)";
                }

                owner->showMessage(msg, false);
            }
            return;
        }

        const size_t songIndex = state->targets[state->position++];
        if (songIndex >= owner->songs_.size())
        {
            (*checkNext)();
            return;
        }

        const auto current = owner->songs_[songIndex];
        ApiService::getInstance().lookupSharedMetadataOnly(current,
            juce::String(current.artistName), juce::String(current.songName),
            [state, checkNext, songIndex](ApiService::Result result)
            {
                auto* ownerInner = state->owner.getComponent();
                if (ownerInner == nullptr)
                    return;

                const bool stale = state->generation != ownerInner->audioAnalysisGeneration_;
                if (! stale && result.ok && songIndex < ownerInner->songs_.size())
                {
                    ownerInner->songs_[songIndex] = result.song;
                    ++state->updatedCount;
                    if (state->updateLocalCatalog)
                        state->updatedSongs.push_back(result.song);
                }

                (*checkNext)();
            });
    };

    (*checkNext)();
}

void LibraryPage::startFolderChooser(bool appendMode)
{
    if (scanner_.isScanning.load()) return;

    juce::File startDir = juce::File::getSpecialLocation(juce::File::userMusicDirectory);
    if (! pathEditor_->getText().isEmpty())
    {
        juce::File candidate(pathEditor_->getText());
        if (candidate.isDirectory()) startDir = candidate;
    }

    fileChooser_ = std::make_shared<juce::FileChooser>(
        appendMode ? LocalizationManager::getInstance().getText("library.chooser_append")
                   : LocalizationManager::getInstance().getText("library.chooser_root"),
        startDir);

    fileChooser_->launchAsync(
        juce::FileBrowserComponent::openMode |
        juce::FileBrowserComponent::canSelectDirectories,
        [this, appendMode](const juce::FileChooser& fc)
        {
            auto result = fc.getResult();
            if (! result.isDirectory()) return;

            pathEditor_->setText(result.getFullPathName(), juce::dontSendNotification);
            UserPreferences::getInstance().setLibraryPath(result.getFullPathName());
            lastScanWasAppend_ = appendMode;
            setScanningState(true);
            progressValue_ = -1.0; // indeterminate while collecting files
            reportStatus("Scanning Folders...");

            // Invalidate any in-flight background analysis/sync pass --
            // Initial Load replaces songs_ outright, and even Add Songs
            // shouldn't race a concurrent writer. A fresh, correctly-indexed
            // pass gets kicked off once this scan's onComplete runs.
            ++audioAnalysisGeneration_;

            if (appendMode)
                scanner_.startAppendScan(result, songs_);
            else
                scanner_.startInitialScan(result);

            startTimerHz(10);
        });
}

//==============================================================================
void LibraryPage::onGetMetaData()
{
    if (songs_.empty())
    {
        showMessage(LocalizationManager::getInstance().getText("library.no_songs"), true);
        return;
    }

    setScanningState(true);
    progressValue_ = -1.0;
    progressBar_->repaint();
    progressLabel_->setText(LocalizationManager::getInstance().getText("library.applying_metadata"), juce::dontSendNotification);
    reportStatus("Adding Meta Data...");

    // Invalidate any in-flight background analysis/sync pass -- this is
    // about to mutate songs_ from a background thread (applyLocalMetadata)
    // and then from the message thread (syncSharedMetadataForSongs below),
    // and an analysis pass writing to the same songs_ concurrently would
    // race it. A fresh pass gets kicked off once this finishes.
    ++audioAnalysisGeneration_;

    // Run on a background thread to avoid blocking the UI, low priority so
    // it never competes with live playback if a show is in progress.
    juce::Thread::launch(juce::Thread::Priority::low, [this]() {
        int matched = scanner_.applyLocalMetadata(songs_);
        stats_ = LibraryScanner::computeStats(songs_);
        persistSongbook();

        juce::MessageManager::callAsync([this, matched]() {
            stats_.numMeta = matched;
            refreshStats();
            setScanningState(false);
            if (onSongbookChanged) onSongbookChanged(songs_);

            std::vector<size_t> targets;
            targets.reserve(songs_.size());
            for (size_t i = 0; i < songs_.size(); ++i)
                if (needsRemoteMetadata(songs_[i]))
                    targets.push_back(i);

            if (targets.empty())
            {
                showMessage(juce::String(matched) + " songs matched with metadata.", false);
                return;
            }

            // Local catalog file is current for everything it has -- for
            // whatever's still missing, check the shared Firebase master
            // list too (cache + Firestore only, never Spotify) and, on any
            // hit, write it back into the local catalog file so it's there
            // for next time even if this venue is offline.
            showMessage(juce::String(matched) + " songs matched locally. Checking "
                        + juce::String((int) targets.size())
                        + " remaining song(s) against Firebase...", false);
            syncSharedMetadataForSongs(std::move(targets), true);
        });
    });
}

//==============================================================================
void LibraryPage::onEditGenres()
{
    auto& lm = LocalizationManager::getInstance();
    juce::AlertWindow::showMessageBoxAsync(
        juce::MessageBoxIconType::InfoIcon,
        lm.getText("library.edit_genres_title"),
        lm.getText("library.edit_genres_body"),
        lm.getText("button.ok"));
}

//==============================================================================
// Helpers
//==============================================================================
void LibraryPage::refreshStats()
{
    auto fmt = [](const char* label, int value) -> juce::String {
        return juce::String(label) + juce::String(value);
    };

    auto& lm = LocalizationManager::getInstance();
    statsTotalLabel_  ->setText(fmt(lm.getText("library.stats_total")  .toRawUTF8(), stats_.numSongs),   juce::dontSendNotification);
    statsMetaLabel_   ->setText(fmt(lm.getText("library.stats_meta")   .toRawUTF8(), stats_.numMeta),    juce::dontSendNotification);
    statsAnalyzedLabel_->setText(fmt(lm.getText("library.stats_analyzed").toRawUTF8(), stats_.numAnalyzed), juce::dontSendNotification);
    statsCDGLabel_    ->setText(fmt(lm.getText("library.stats_cdg")    .toRawUTF8(), stats_.numCDG),     juce::dontSendNotification);
    statsZipLabel_    ->setText(fmt(lm.getText("library.stats_zip")    .toRawUTF8(), stats_.numZip),     juce::dontSendNotification);
    statsMP4Label_    ->setText(fmt(lm.getText("library.stats_mp4")    .toRawUTF8(), stats_.numMP4),     juce::dontSendNotification);
    statsM4ALabel_    ->setText(fmt(lm.getText("library.stats_m4a")    .toRawUTF8(), stats_.numM4A),     juce::dontSendNotification);
    statsXMLLabel_    ->setText(fmt(lm.getText("library.stats_xml")    .toRawUTF8(), stats_.numXML),     juce::dontSendNotification);
    statsUnknownLabel_->setText(fmt(lm.getText("library.stats_unknown").toRawUTF8(), stats_.numUnknown), juce::dontSendNotification);

    repaint();
}

void LibraryPage::updateAudioAnalysisUI()
{
    const bool show = audioAnalysisRunning_ && audioAnalysisTotal_ > 0;
    const bool visibilityChanged = audioAnalysisStatusLabel_->isVisible() != show;

    if (show)
    {
        juce::String text = (audioAnalysisPaused_ ? "Paused -- audio analysis: " : "Analyzing audio (tempo/key/duration): ")
            + juce::String(audioAnalysisDone_) + " / " + juce::String(audioAnalysisTotal_);
        if (! audioAnalysisPaused_ && audioAnalysisCurrentSong_.isNotEmpty())
            text += "  (" + audioAnalysisCurrentSong_ + ")";
        audioAnalysisStatusLabel_->setText(text, juce::dontSendNotification);
        audioAnalysisPauseBtn_->setButtonText(audioAnalysisPaused_ ? "Resume" : "Pause");
    }

    audioAnalysisStatusLabel_->setVisible(show);
    audioAnalysisPauseBtn_->setVisible(show);

    if (visibilityChanged)
        resized();
}

void LibraryPage::setProgressVisible(bool visible)
{
    progressLabel_    ->setVisible(visible);
    progressBar_      ->setVisible(visible);
    currentSongLabel_ ->setVisible(visible);
    resized();
    repaint();
}

void LibraryPage::setScanningState(bool scanning)
{
    initialSongLoadBtn_->setEnabled(! scanning);
    addSongsBtn_       ->setEnabled(! scanning);
    getMetaDataBtn_    ->setEnabled(! scanning);
    editGenresBtn_     ->setEnabled(! scanning);
    browseBtn_         ->setEnabled(! scanning);

    setProgressVisible(scanning);

    if (scanning)
    {
        if (globalScanTaskId_ == 0)
        {
            juce::String msg = progressLabel_ != nullptr ? progressLabel_->getText().trim() : juce::String();
            if (msg.isEmpty())
                msg = "Scanning library...";
            globalScanTaskId_ = GlobalProgressService::getInstance().beginTask(msg);
        }

        progressLabel_->setText(LocalizationManager::getInstance().getText("library.scanning"), juce::dontSendNotification);
        messageLabel_->setVisible(false);
        startTimerHz(10);
    }
    else
    {
        if (globalScanTaskId_ != 0)
        {
            GlobalProgressService::getInstance().endTask(globalScanTaskId_);
            globalScanTaskId_ = 0;
        }

        stopTimer();
        progressValue_ = 1.0;
        progressBar_->repaint();
    }
}

void LibraryPage::showMessage(const juce::String& msg, bool isError)
{
    messageLabel_->setText(msg, juce::dontSendNotification);
    messageLabel_->setColour(juce::Label::textColourId,
                             isError ? juce::Colours::tomato
                                     : juce::Colour(kTextSecond));
    messageLabel_->setVisible(true);
    resized();
    repaint();
}
