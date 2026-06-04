/*
  ==============================================================================

    LibraryPage.cpp
    Created: 22 Apr 2026
    Author:  GitHub Copilot

  ==============================================================================
*/

#include "LibraryPage.h"
#include "AddSongsDialog.h"
#include "../Services/ApiService.h"
#include "../Services/UserPreferences.h"
#include "../Localization/LocalizationManager.h"

#include <unordered_set>
#include <unordered_map>
#include <atomic>

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
    return song.imageUrl.empty()
        || song.durationMS <= 0
        || song.keySignature.empty()
        || song.tempo <= 0.0
        || song.releaseDate.empty()
        || song.genres.empty();
}

//==============================================================================
LibraryPage::LibraryPage()
    : juce::Timer()
{
    auto& lm = LocalizationManager::getInstance();

    //--------------------------------------------------------------------------
    // Title
    titleLabel_ = std::make_unique<juce::Label>();
    titleLabel_->setText(lm.getText("library.title"), juce::dontSendNotification);
    titleLabel_->setFont(juce::Font(juce::FontOptions().withHeight(22.f)).boldened());
    titleLabel_->setColour(juce::Label::textColourId, juce::Colours::white);
    titleLabel_->setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(*titleLabel_);

    //--------------------------------------------------------------------------
    // Path section
    pathLabel_ = std::make_unique<juce::Label>();
    pathLabel_->setText(lm.getText("library.path_label"), juce::dontSendNotification);
    pathLabel_->setFont(juce::Font(juce::FontOptions().withHeight(13.f)));
    pathLabel_->setColour(juce::Label::textColourId, juce::Colour(kTextSecond));
    addAndMakeVisible(*pathLabel_);

    pathEditor_ = std::make_unique<juce::TextEditor>();
    pathEditor_->setMultiLine(false);
    pathEditor_->setFont(juce::Font(juce::FontOptions().withHeight(13.f)));
    pathEditor_->setColour(juce::TextEditor::backgroundColourId, juce::Colour(0xff0d1527));
    pathEditor_->setColour(juce::TextEditor::textColourId, juce::Colour(kTextPrimary));
    pathEditor_->setColour(juce::TextEditor::outlineColourId, juce::Colour(kAccent).withAlpha(0.4f));
    pathEditor_->setColour(juce::TextEditor::focusedOutlineColourId, juce::Colour(kAccent));
    addAndMakeVisible(*pathEditor_);

    browseBtn_ = std::make_unique<juce::TextButton>("...");
    browseBtn_->setColour(juce::TextButton::buttonColourId, juce::Colour(kBtnNormal));
    browseBtn_->setColour(juce::TextButton::textColourOffId, juce::Colours::white);
    browseBtn_->onClick = [this]() { onInitialSongLoad(); };
    addAndMakeVisible(*browseBtn_);

    //--------------------------------------------------------------------------
    // Progress section (hidden until a scan starts)
    progressLabel_ = std::make_unique<juce::Label>();
    progressLabel_->setText(lm.getText("library.scanning"), juce::dontSendNotification);
    progressLabel_->setFont(juce::Font(juce::FontOptions().withHeight(13.f)));
    progressLabel_->setColour(juce::Label::textColourId, juce::Colour(kTextSecond));
    progressLabel_->setVisible(false);
    addAndMakeVisible(*progressLabel_);

    progressBar_ = std::make_unique<juce::ProgressBar>(progressValue_);
    progressBar_->setColour(juce::ProgressBar::backgroundColourId, juce::Colour(0xff0d1527));
    progressBar_->setColour(juce::ProgressBar::foregroundColourId, juce::Colour(kAccent));
    progressBar_->setVisible(false);
    addAndMakeVisible(*progressBar_);

    currentSongLabel_ = std::make_unique<juce::Label>();
    currentSongLabel_->setFont(juce::Font(juce::FontOptions().withHeight(11.f)));
    currentSongLabel_->setColour(juce::Label::textColourId, juce::Colour(kTextSecond));
    currentSongLabel_->setVisible(false);
    addAndMakeVisible(*currentSongLabel_);

    //--------------------------------------------------------------------------
    // Status message label
    messageLabel_ = std::make_unique<juce::Label>();
    messageLabel_->setFont(juce::Font(juce::FontOptions().withHeight(12.f)));
    messageLabel_->setColour(juce::Label::textColourId, juce::Colour(kTextSecond));
    messageLabel_->setVisible(false);
    addAndMakeVisible(*messageLabel_);

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
        addAndMakeVisible(*btn);
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
        addAndMakeVisible(*lbl);
    };

    makeStatLabel(statsTotalLabel_);
    makeStatLabel(statsMetaLabel_);
    makeStatLabel(statsCDGLabel_);
    makeStatLabel(statsZipLabel_);
    makeStatLabel(statsMP4Label_);
    makeStatLabel(statsM4ALabel_);
    makeStatLabel(statsXMLLabel_);
    makeStatLabel(statsUnknownLabel_);
    makeStatLabel(statsGroupsLabel_);

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

        // Persist to disk
        scanner_.saveSongbook(songs_);

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
    };

    scanner_.onError = [this](juce::String err) {
        setScanningState(false);
        showMessage(err, true);
    };

    //--------------------------------------------------------------------------
    // Load existing songbook from disk without blocking first paint.
    loadSongbookAsync();
}

//==============================================================================
LibraryPage::~LibraryPage()
{
    stopTimer();
    scanner_.stopScan();
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
    juce::Thread::launch([safeThis]()
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
    scanner_.saveSongbook(songs_);
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

    scanner_.saveSongbook(songs_);
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
    // Semi-transparent dark grey panel behind the stats area so the textured
    // tile background from MainArea still shows through subtly.
    auto bounds = getLocalBounds();
    int panelY = titleLabel_->getBottom() + 8
               + pathLabel_->getHeight() + 4
               + pathEditor_->getHeight() + 8
               + (progressBar_->isVisible() ? progressBar_->getHeight() + 32 : 0)
               + 8   // message label
               + initialSongLoadBtn_->getHeight() + 12;

    int panelH = bounds.getHeight() - panelY - 12;
    if (panelH > 0)
    {
        g.setColour(juce::Colour(0xff202428).withAlpha(0.55f));
        g.fillRoundedRectangle(juce::Rectangle<int>(12, panelY, bounds.getWidth() - 24, panelH).toFloat(), 6.f);
    }
}

void LibraryPage::resized()
{
    auto area   = getLocalBounds().reduced(16, 12);
    int  w      = area.getWidth();
    int  lineH  = 24;
    int  btnH   = 32;
    int  gap    = 8;
    int  y      = area.getY();

    // Title
    titleLabel_->setBounds(area.getX(), y, w, 30);
    y += 34;

    // Path label
    pathLabel_->setBounds(area.getX(), y, w, lineH);
    y += lineH + 2;

    // Path editor + browse button
    int browseW = 36;
    pathEditor_->setBounds(area.getX(), y, w - browseW - 4, lineH);
    browseBtn_->setBounds(area.getX() + w - browseW, y, browseW, lineH);
    y += lineH + gap;

    // Progress section
    int progressSectionH = 0;
    if (progressBar_->isVisible())
    {
        progressLabel_->setBounds(area.getX(), y, 100, lineH);
        progressBar_->setBounds(area.getX() + 104, y, w - 104, lineH);
        y += lineH + 2;
        currentSongLabel_->setBounds(area.getX(), y, w, lineH - 4);
        y += lineH + gap;
        progressSectionH = 2 * lineH + gap + 2;
    }

    // Message label
    messageLabel_->setBounds(area.getX(), y, w, lineH - 4);
    y += messageLabel_->isVisible() ? (lineH - 4 + 4) : 4;

    // Button bar (4 buttons equally spaced)
    int btnGap  = 8;
    int numBtns = 4;
    int btnW    = (w - btnGap * (numBtns - 1)) / numBtns;
    int bx      = area.getX();

    initialSongLoadBtn_->setBounds(bx, y, btnW, btnH); bx += btnW + btnGap;
    addSongsBtn_       ->setBounds(bx, y, btnW, btnH); bx += btnW + btnGap;
    getMetaDataBtn_    ->setBounds(bx, y, btnW, btnH); bx += btnW + btnGap;
    editGenresBtn_     ->setBounds(bx, y, btnW, btnH);
    y += btnH + gap + 4;

    // Stats panel — two columns
    int statsX    = area.getX() + 12;
    int statsW    = (w - 36) / 2;
    int statsLineH = 22;

    auto placeStat = [&](juce::Label* lbl, int col) {
        int cx = statsX + col * (statsW + 12);
        lbl->setBounds(cx, y, statsW, statsLineH);
        if (col == 1) y += statsLineH + 2;
    };

    placeStat(statsTotalLabel_.get(),   0);
    placeStat(statsMetaLabel_.get(),    1);
    placeStat(statsCDGLabel_.get(),     0);
    placeStat(statsZipLabel_.get(),     1);
    placeStat(statsMP4Label_.get(),     0);
    placeStat(statsM4ALabel_.get(),     1);
    placeStat(statsXMLLabel_.get(),     0);
    placeStat(statsUnknownLabel_.get(), 1);
    placeStat(statsGroupsLabel_.get(),  0);
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
            scanner_.saveSongbook(songs_);
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
                fetchMetadataForImportedSongs(std::move(newlyImported), true);
        });
}

        void LibraryPage::fetchMetadataForImportedSongs(std::vector<size_t> songIndices,
                                bool allowOnlineLookup)
{
    if (songIndices.empty())
        return;

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
        scanner_.saveSongbook(songs_);
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
        scanner_.saveSongbook(songs_);
        stats_ = LibraryScanner::computeStats(songs_);
        refreshStats();
        setScanningState(false);
        if (onSongbookChanged)
            onSongbookChanged(songs_);

        showMessage("Initial load is local-only: matched "
                    + juce::String(localPrePassMatched)
                    + " songs from meta_data.json; "
                    + juce::String((int) targets.size())
                    + " songs remain without local metadata.", false);
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

        owner->scanner_.saveSongbook(owner->songs_);
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

    // Run on a background thread to avoid blocking the UI
    juce::Thread::launch([this]() {
        int matched = scanner_.applyLocalMetadata(songs_);
        stats_ = LibraryScanner::computeStats(songs_);
        scanner_.saveSongbook(songs_);

        juce::MessageManager::callAsync([this, matched]() {
            stats_.numMeta = matched;
            refreshStats();
            setScanningState(false);
            showMessage(juce::String(matched) + " songs matched with metadata.", false);
            if (onSongbookChanged) onSongbookChanged(songs_);
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
    statsCDGLabel_    ->setText(fmt(lm.getText("library.stats_cdg")    .toRawUTF8(), stats_.numCDG),     juce::dontSendNotification);
    statsZipLabel_    ->setText(fmt(lm.getText("library.stats_zip")    .toRawUTF8(), stats_.numZip),     juce::dontSendNotification);
    statsMP4Label_    ->setText(fmt(lm.getText("library.stats_mp4")    .toRawUTF8(), stats_.numMP4),     juce::dontSendNotification);
    statsM4ALabel_    ->setText(fmt(lm.getText("library.stats_m4a")    .toRawUTF8(), stats_.numM4A),     juce::dontSendNotification);
    statsXMLLabel_    ->setText(fmt(lm.getText("library.stats_xml")    .toRawUTF8(), stats_.numXML),     juce::dontSendNotification);
    statsUnknownLabel_->setText(fmt(lm.getText("library.stats_unknown").toRawUTF8(), stats_.numUnknown), juce::dontSendNotification);
    statsGroupsLabel_ ->setText(fmt(lm.getText("library.stats_groups") .toRawUTF8(), stats_.numGroups),  juce::dontSendNotification);

    repaint();
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
        progressLabel_->setText(LocalizationManager::getInstance().getText("library.scanning"), juce::dontSendNotification);
        messageLabel_->setVisible(false);
        startTimerHz(10);
    }
    else
    {
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
