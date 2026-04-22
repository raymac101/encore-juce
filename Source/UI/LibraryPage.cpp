/*
  ==============================================================================

    LibraryPage.cpp
    Created: 22 Apr 2026
    Author:  GitHub Copilot

  ==============================================================================
*/

#include "LibraryPage.h"
#include "../Localization/LocalizationManager.h"

//==============================================================================
// Helper: style a stat label
//==============================================================================
static void styleStatLabel(juce::Label* lbl, uint32_t textColour)
{
    lbl->setFont(juce::Font(juce::FontOptions().withHeight(13.f)));
    lbl->setColour(juce::Label::textColourId, juce::Colour(textColour));
    lbl->setJustificationType(juce::Justification::centredLeft);
}

//==============================================================================
LibraryPage::LibraryPage()
    : juce::Timer()
{
    auto& lm = LocalizationManager::getInstance();

    //--------------------------------------------------------------------------
    // Title
    titleLabel_ = std::make_unique<juce::Label>();
    titleLabel_->setText("LIBRARY", juce::dontSendNotification);
    titleLabel_->setFont(juce::Font(juce::FontOptions().withHeight(22.f)).boldened());
    titleLabel_->setColour(juce::Label::textColourId, juce::Colours::white);
    titleLabel_->setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(*titleLabel_);

    //--------------------------------------------------------------------------
    // Path section
    pathLabel_ = std::make_unique<juce::Label>();
    pathLabel_->setText("Path to Karaoke Song Files:", juce::dontSendNotification);
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
    progressLabel_->setText("Scanning:", juce::dontSendNotification);
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

    makeBtn("Initial Song Load", initialSongLoadBtn_,
            [this]() { onInitialSongLoad(); });
    makeBtn("Add Songs",         addSongsBtn_,
            [this]() { onAddSongs(); });
    makeBtn("Get Meta Data",     getMetaDataBtn_,
            [this]() { onGetMetaData(); });
    makeBtn("Edit Genres",       editGenresBtn_,
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

        // Path editor intentionally left unchanged — it shows the scan root
        // that the user selected, not an individual song's subdirectory.

        refreshStats();
        setScanningState(false);
        showMessage(juce::String(songs_.size()) + " songs loaded.", false);

        if (onSongbookChanged) onSongbookChanged(songs_);
    };

    scanner_.onError = [this](juce::String err) {
        setScanningState(false);
        showMessage(err, true);
    };

    //--------------------------------------------------------------------------
    // Load existing songbook from disk
    loadSongbook();
}

//==============================================================================
LibraryPage::~LibraryPage()
{
    stopTimer();
    scanner_.stopScan();
}

//==============================================================================
void LibraryPage::loadSongbook()
{
    songs_ = scanner_.loadSongbook();
    stats_ = LibraryScanner::computeStats(songs_);
    refreshStats();

    // Pre-populate path from saved prefs / first song
    if (! songs_.empty() && ! songs_[0].filePath.empty())
    {
        juce::File dir(juce::String(songs_[0].filePath[0]));
        pathEditor_->setText(dir.getFullPathName(), juce::dontSendNotification);
    }
}

//==============================================================================
void LibraryPage::updateAllText()
{
    // Re-read any localized strings if/when localization covers library text
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
    g.fillAll(juce::Colour(kBg));

    // Stats panel background
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
        g.setColour(juce::Colour(kPanel));
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
    startFolderChooser(true /* append */);
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
        appendMode ? "Select Directory to Add Songs From"
                   : "Select Karaoke Collection Root Directory",
        startDir);

    fileChooser_->launchAsync(
        juce::FileBrowserComponent::openMode |
        juce::FileBrowserComponent::canSelectDirectories,
        [this, appendMode](const juce::FileChooser& fc)
        {
            auto result = fc.getResult();
            if (! result.isDirectory()) return;

            pathEditor_->setText(result.getFullPathName(), juce::dontSendNotification);
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
        showMessage("No songs loaded. Run an Initial Song Load first.", true);
        return;
    }

    setScanningState(true);
    progressValue_ = -1.0;
    progressBar_->repaint();
    progressLabel_->setText("Applying metadata:", juce::dontSendNotification);

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
    juce::AlertWindow::showMessageBoxAsync(
        juce::MessageBoxIconType::InfoIcon,
        "Edit Genres",
        "Genre editor coming soon.\n\nGenres are automatically extracted from your CDG collection folders and metadata.",
        "OK");
}

//==============================================================================
// Helpers
//==============================================================================
void LibraryPage::refreshStats()
{
    auto fmt = [](const char* label, int value) -> juce::String {
        return juce::String(label) + juce::String(value);
    };

    statsTotalLabel_  ->setText(fmt("Total Songs:        ", stats_.numSongs),   juce::dontSendNotification);
    statsMetaLabel_   ->setText(fmt("Metadata Available: ", stats_.numMeta),    juce::dontSendNotification);
    statsCDGLabel_    ->setText(fmt("CDG Files:          ", stats_.numCDG),     juce::dontSendNotification);
    statsZipLabel_    ->setText(fmt("ZIP Files:          ", stats_.numZip),     juce::dontSendNotification);
    statsMP4Label_    ->setText(fmt("MP4 Files:          ", stats_.numMP4),     juce::dontSendNotification);
    statsM4ALabel_    ->setText(fmt("M4A Files:          ", stats_.numM4A),     juce::dontSendNotification);
    statsXMLLabel_    ->setText(fmt("XML Files:          ", stats_.numXML),     juce::dontSendNotification);
    statsUnknownLabel_->setText(fmt("Unknown Files:      ", stats_.numUnknown), juce::dontSendNotification);
    statsGroupsLabel_ ->setText(fmt("Folders/Groups:     ", stats_.numGroups),  juce::dontSendNotification);

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
        progressLabel_->setText("Scanning:", juce::dontSendNotification);
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
