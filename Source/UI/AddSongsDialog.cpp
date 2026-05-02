/*
  ==============================================================================
    AddSongsDialog.cpp
  ==============================================================================
*/

#include "AddSongsDialog.h"

//==============================================================================
// Colours
namespace
{
    constexpr uint32_t kBg          = 0xff16213e;
    constexpr uint32_t kCard        = 0xff1a2030;
    constexpr uint32_t kBorder      = 0xff2a3550;
    constexpr uint32_t kAccent      = 0xff30daff;
    constexpr uint32_t kGreen       = 0xff30ee88;
    constexpr uint32_t kGrey        = 0xff888888;
    constexpr uint32_t kBtnNormal   = 0xff253555;
    constexpr uint32_t kBtnImport   = 0xff1a6040;
    constexpr uint32_t kTextPrimary = 0xffe8eaf0;
    constexpr uint32_t kRowHover    = 0xff1e2d48;

    static juce::File settingsFile()
    {
        auto dir = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                       .getChildFile("EncoreKaraoke");
        dir.createDirectory();
        return dir.getChildFile("settings.xml");
    }

    static bool isKaraokeFile(const juce::File& f)
    {
        auto ext = f.getFileExtension().toLowerCase();
        return ext == ".cdg" || ext == ".zip" || ext == ".mp4"
            || ext == ".m4a" || ext == ".mp3" || ext == ".xml";
    }
}

//==============================================================================
// FileListModel — checkbox rows for the setup phase
//==============================================================================
class AddSongsDialog::FileListModel : public juce::ListBoxModel
{
public:
    explicit FileListModel(AddSongsDialog& owner) : owner_(owner) {}

    int getNumRows() override
    {
        return (int) owner_.fileEntries_.size();
    }

    void paintListBoxItem(int row, juce::Graphics& g,
                          int w, int h, bool /*rowSelected*/) override
    {
        if (row < 0 || row >= (int) owner_.fileEntries_.size()) return;
        const auto& entry = owner_.fileEntries_[(size_t) row];

        g.fillAll(juce::Colour(kCard));

        // Hover tint
        const bool hovered = (row == hoveredRow_);
        if (hovered)
            g.fillAll(juce::Colour(kRowHover));

        // Checkbox (16×16 at left edge)
        const int cx = 8, cy = (h - 16) / 2;
        g.setColour(juce::Colour(kAccent));
        g.drawRect(cx, cy, 16, 16, 1);
        if (entry.selected)
        {
            g.fillRect(cx + 3, cy + 3, 10, 10);
        }

        // Filename
        g.setColour(entry.selected ? juce::Colour(kTextPrimary) : juce::Colour(kGrey));
        g.setFont(juce::Font(juce::FontOptions().withHeight(13.5f)));
        g.drawText(entry.displayName, 32, 0, w - 36, h,
                   juce::Justification::centredLeft, true);
    }

    void listBoxItemClicked(int row, const juce::MouseEvent&) override
    {
        if (row < 0 || row >= (int) owner_.fileEntries_.size()) return;
        owner_.fileEntries_[(size_t) row].selected = ! owner_.fileEntries_[(size_t) row].selected;
        owner_.fileListBox_->repaintRow(row);
        owner_.updateSelectAll();
        owner_.updateImportButton();
    }

    void listBoxItemDoubleClicked(int row, const juce::MouseEvent& e) override
    {
        listBoxItemClicked(row, e);
    }

    int hoveredRow_ = -1;

private:
    AddSongsDialog& owner_;
};

//==============================================================================
// StatusListModel — colour-coded per-file status for importing/complete phases
//==============================================================================
class AddSongsDialog::StatusListModel : public juce::ListBoxModel
{
public:
    explicit StatusListModel(AddSongsDialog& owner) : owner_(owner) {}

    int getNumRows() override
    {
        return (int) owner_.importRecords_.size();
    }

    void paintListBoxItem(int row, juce::Graphics& g,
                          int w, int h, bool /*rowSelected*/) override
    {
        if (row < 0 || row >= (int) owner_.importRecords_.size()) return;
        const auto& rec = owner_.importRecords_[(size_t) row];

        g.fillAll(juce::Colour(kCard));

        // Colour dot
        const juce::Colour dot = rec.isNew ? juce::Colour(kGreen) : juce::Colour(kGrey);
        g.setColour(dot);
        g.fillEllipse(8.0f, (h - 8) * 0.5f, 8.0f, 8.0f);

        // Song name
        g.setColour(juce::Colours::white);
        g.setFont(juce::Font(juce::FontOptions().withHeight(13.0f)));
        const int statusW = 110;
        g.drawText(rec.name, 22, 0, w - statusW - 24, h,
                   juce::Justification::centredLeft, true);

        // Status badge
        g.setColour(dot.withAlpha(0.85f));
        g.setFont(juce::Font(juce::FontOptions().withHeight(11.5f)));
        g.drawText(rec.message, w - statusW, 0, statusW - 6, h,
                   juce::Justification::centredRight, false);
    }

private:
    AddSongsDialog& owner_;
};

//==============================================================================
// BorderlessModalWindow (local, same pattern as SongEditDialog)
//==============================================================================
namespace
{
    class AddSongsBorderlessWindow : public juce::DocumentWindow
    {
    public:
        AddSongsBorderlessWindow(juce::Component* content, juce::Colour bg)
            : juce::DocumentWindow({}, bg, 0, true)
        {
            setUsingNativeTitleBar(false);
            setTitleBarHeight(0);
            setDropShadowEnabled(true);
            setResizable(false, false);
            setContentOwned(content, true);
            centreAroundComponent(nullptr, content->getWidth(), content->getHeight());
            setVisible(true);
            enterModalState(true,
                            juce::ModalCallbackFunction::create([this](int) { delete this; }),
                            false);
        }
        void closeButtonPressed() override { exitModalState(0); }
    };
}

//==============================================================================
// Static launch
//==============================================================================
void AddSongsDialog::launch(juce::Component*            parent,
                            const std::vector<CdgSong>& existingSongs,
                            const juce::File&           libraryRootDir,
                            std::function<void(std::vector<CdgSong>, ImportStats)> onComplete)
{
    auto* dlg = new AddSongsDialog(existingSongs, libraryRootDir);
    dlg->onComplete = std::move(onComplete);

    auto* w = new AddSongsBorderlessWindow(dlg, juce::Colour(kBg));
    if (parent != nullptr)
        w->centreAroundComponent(parent, dlg->getWidth(), dlg->getHeight());
}

//==============================================================================
// Constructor
//==============================================================================
AddSongsDialog::AddSongsDialog(const std::vector<CdgSong>& existingSongs,
                               const juce::File& libraryRootDir)
    : existingSongs_(existingSongs),
      libraryRootDir_(libraryRootDir)
{
    // Models
    fileListModel_   = std::make_unique<FileListModel>(*this);
    statusListModel_ = std::make_unique<StatusListModel>(*this);

    //--- Header ---
    titleLabel_ = std::make_unique<juce::Label>("title", "Add Songs to Library");
    titleLabel_->setFont(juce::Font(juce::FontOptions().withHeight(20.0f)).boldened());
    titleLabel_->setColour(juce::Label::textColourId, juce::Colour(kTextPrimary));
    addAndMakeVisible(*titleLabel_);

    closeBtn_ = std::make_unique<juce::TextButton>("X");
    closeBtn_->setColour(juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
    closeBtn_->setColour(juce::TextButton::textColourOffId, juce::Colour(kGrey));
    closeBtn_->onClick = [this] { onCancel(); };
    addAndMakeVisible(*closeBtn_);

    //--- Path row ---
    pathEditor_ = std::make_unique<juce::TextEditor>("path");
    pathEditor_->setColour(juce::TextEditor::backgroundColourId, juce::Colour(kBtnNormal));
    pathEditor_->setColour(juce::TextEditor::textColourId, juce::Colour(kTextPrimary));
    pathEditor_->setColour(juce::TextEditor::outlineColourId, juce::Colour(kBorder));
    pathEditor_->onReturnKey = [this]
    {
        juce::File d(pathEditor_->getText().trim());
        if (d.isDirectory()) { sourcePath_ = d.getFullPathName(); savePrefs(); startFolderScan(d); }
    };
    addAndMakeVisible(*pathEditor_);

    browseBtn_ = std::make_unique<juce::TextButton>("Browse...");
    browseBtn_->setColour(juce::TextButton::buttonColourId, juce::Colour(kBtnNormal));
    browseBtn_->setColour(juce::TextButton::textColourOffId, juce::Colours::white);
    browseBtn_->onClick = [this] { onBrowse(); };
    addAndMakeVisible(*browseBtn_);

    //--- Options ---
    auto makeToggle = [this](std::unique_ptr<juce::ToggleButton>& tb,
                             const juce::String& text, bool initial)
    {
        tb = std::make_unique<juce::ToggleButton>(text);
        tb->setToggleState(initial, juce::dontSendNotification);
        tb->setColour(juce::ToggleButton::textColourId, juce::Colour(kTextPrimary));
        tb->setColour(juce::ToggleButton::tickColourId, juce::Colour(kAccent));
        tb->setColour(juce::ToggleButton::tickDisabledColourId, juce::Colour(kGrey));
        addAndMakeVisible(*tb);
    };

    loadPrefs();
    makeToggle(copySongsToggle_,   "Copy Songs to Library",           copySongs_);
    makeToggle(deleteSongsToggle_, "Delete Songs from Source Folder", deleteSongs_);

    copySongsToggle_->onStateChange = [this]
    {
        copySongs_ = copySongsToggle_->getToggleState();
        savePrefs();
    };
    deleteSongsToggle_->onStateChange = [this]
    {
        deleteSongs_ = deleteSongsToggle_->getToggleState();
        savePrefs();
    };

    //--- Select All toggle ---
    selectAllToggle_ = std::make_unique<juce::ToggleButton>("Select All");
    selectAllToggle_->setToggleState(true, juce::dontSendNotification);
    selectAllToggle_->setColour(juce::ToggleButton::textColourId, juce::Colour(kTextPrimary));
    selectAllToggle_->setColour(juce::ToggleButton::tickColourId, juce::Colour(kAccent));
    selectAllToggle_->setColour(juce::ToggleButton::tickDisabledColourId, juce::Colour(kGrey));
    selectAllToggle_->onClick = [this]
    {
        const bool all = selectAllToggle_->getToggleState();
        for (auto& e : fileEntries_) e.selected = all;
        fileListBox_->repaintRow(-1);
        fileListBox_->updateContent();
        updateImportButton();
    };
    addAndMakeVisible(*selectAllToggle_);

    //--- File ListBox (setup phase) ---
    fileListBox_ = std::make_unique<juce::ListBox>("files", fileListModel_.get());
    fileListBox_->setRowHeight(26);
    fileListBox_->setColour(juce::ListBox::backgroundColourId, juce::Colour(kCard));
    fileListBox_->setColour(juce::ListBox::outlineColourId,    juce::Colour(kBorder));
    fileListBox_->setOutlineThickness(1);
    addAndMakeVisible(*fileListBox_);

    //--- Scanning label (shown while folder scan runs) ---
    scanningLabel_ = std::make_unique<juce::Label>();
    scanningLabel_->setColour(juce::Label::textColourId, juce::Colour(kGrey));
    scanningLabel_->setFont(juce::Font(juce::FontOptions().withHeight(13.5f)));
    scanningLabel_->setJustificationType(juce::Justification::centred);
    addAndMakeVisible(*scanningLabel_);
    scanningLabel_->setVisible(false);

    //--- Progress label ---
    progressLabel_ = std::make_unique<juce::Label>();
    progressLabel_->setColour(juce::Label::textColourId, juce::Colour(kTextPrimary));
    progressLabel_->setFont(juce::Font(juce::FontOptions().withHeight(13.5f)));
    progressLabel_->setText("Analysing files...", juce::dontSendNotification);
    addAndMakeVisible(*progressLabel_);
    progressLabel_->setVisible(false);

    //--- Stats label ---
    statsLabel_ = std::make_unique<juce::Label>();
    statsLabel_->setColour(juce::Label::textColourId, juce::Colour(kAccent));
    statsLabel_->setFont(juce::Font(juce::FontOptions().withHeight(13.0f)));
    addAndMakeVisible(*statsLabel_);
    statsLabel_->setVisible(false);

    //--- Progress bar ---
    progressBar_ = std::make_unique<juce::ProgressBar>(progressValue_);
    progressBar_->setColour(juce::ProgressBar::backgroundColourId, juce::Colour(kBtnNormal));
    progressBar_->setColour(juce::ProgressBar::foregroundColourId, juce::Colour(kAccent));
    addAndMakeVisible(*progressBar_);
    progressBar_->setVisible(false);

    //--- Status ListBox (importing / complete) ---
    statusListBox_ = std::make_unique<juce::ListBox>("status", statusListModel_.get());
    statusListBox_->setRowHeight(24);
    statusListBox_->setColour(juce::ListBox::backgroundColourId, juce::Colour(kCard));
    statusListBox_->setColour(juce::ListBox::outlineColourId,    juce::Colour(kBorder));
    statusListBox_->setOutlineThickness(1);
    addAndMakeVisible(*statusListBox_);
    statusListBox_->setVisible(false);

    //--- Buttons ---
    importBtn_ = std::make_unique<juce::TextButton>("Import 0 Songs");
    importBtn_->setColour(juce::TextButton::buttonColourId,  juce::Colour(kBtnImport));
    importBtn_->setColour(juce::TextButton::textColourOffId, juce::Colours::white);
    importBtn_->setEnabled(false);
    importBtn_->onClick = [this] { onImport(); };
    addAndMakeVisible(*importBtn_);

    cancelBtn_ = std::make_unique<juce::TextButton>("Cancel");
    cancelBtn_->setColour(juce::TextButton::buttonColourId,  juce::Colour(kBtnNormal));
    cancelBtn_->setColour(juce::TextButton::textColourOffId, juce::Colours::white);
    cancelBtn_->onClick = [this] { onCancel(); };
    addAndMakeVisible(*cancelBtn_);

    importMoreBtn_ = std::make_unique<juce::TextButton>("Import More Songs");
    importMoreBtn_->setColour(juce::TextButton::buttonColourId,  juce::Colour(kBtnNormal));
    importMoreBtn_->setColour(juce::TextButton::textColourOffId, juce::Colours::white);
    importMoreBtn_->onClick = [this] { onImportMore(); };
    addAndMakeVisible(*importMoreBtn_);
    importMoreBtn_->setVisible(false);

    // All components are now constructed — safe to call setSize (triggers resized())
    setSize(kWidth, kHeight);

    // If we have a saved path, scan it immediately
    if (sourcePath_.isNotEmpty())
    {
        pathEditor_->setText(sourcePath_, juce::dontSendNotification);
        juce::File savedDir(sourcePath_);
        if (savedDir.isDirectory())
            startFolderScan(savedDir);
    }
}

AddSongsDialog::~AddSongsDialog()
{
    scanner_.stopScan();
}

//==============================================================================
// Preferences
//==============================================================================
void AddSongsDialog::loadPrefs()
{
    juce::PropertiesFile::Options opts;
    juce::PropertiesFile prefs(settingsFile(), opts);
    sourcePath_   = prefs.getValue("addPath",    "");
    copySongs_    = prefs.getBoolValue("copySongs",    false);
    deleteSongs_  = prefs.getBoolValue("deleteSongs",  false);
}

void AddSongsDialog::savePrefs()
{
    juce::PropertiesFile::Options opts;
    juce::PropertiesFile prefs(settingsFile(), opts);
    prefs.setValue("addPath",     sourcePath_);
    prefs.setValue("copySongs",   copySongs_);
    prefs.setValue("deleteSongs", deleteSongs_);
    prefs.save();
}

//==============================================================================
// Async folder scan (pre-import listing)
//==============================================================================
void AddSongsDialog::startFolderScan(const juce::File& dir)
{
    if (! dir.isDirectory()) return;

    // New token — any previous background thread sees its copy go stale
    folderScanToken_ = std::make_shared<std::atomic<bool>>(false);
    auto token = folderScanToken_;

    fileEntries_.clear();
    fileListBox_->setVisible(false);
    selectAllToggle_->setVisible(false);
    scanningLabel_->setText("Scanning folder...  click Cancel to stop",
                            juce::dontSendNotification);
    scanningLabel_->setVisible(true);
    browseBtn_->setEnabled(false);
    updateImportButton();
    resized();

    // Snapshot existing keys on the message thread
    std::set<juce::String> existingKeys;
    for (const auto& s : existingSongs_)
        existingKeys.insert(LibraryScanner::normaliseSongKey(
            juce::String(s.artistName), juce::String(s.songName)));

    juce::Component::SafePointer<AddSongsDialog> safe(this);

    juce::Thread::launch([safe, dir, token, existingKeys = std::move(existingKeys)]()
    {
        // Recurse one level at a time so we can cancel between directories
        std::vector<juce::File> allFiles;

        std::function<void(const juce::File&)> collect = [&](const juce::File& d)
        {
            if (token->load()) return;
            juce::Array<juce::File> children;
            d.findChildFiles(children, juce::File::findFilesAndDirectories, false);
            for (const auto& child : children)
            {
                if (token->load()) return;
                if (child.isDirectory())
                    collect(child);
                else if (isKaraokeFile(child))
                    allFiles.push_back(child);
            }
        };

        collect(dir);

        if (token->load())
        {
            juce::MessageManager::callAsync([safe]()
            {
                if (safe != nullptr) safe->onFolderScanCancelled();
            });
            return;
        }

        // Group by dir + base name (case-insensitive)
        std::map<juce::String, std::vector<juce::File>> groups;
        for (const auto& f : allFiles)
        {
            juce::String key = f.getParentDirectory().getFullPathName() + "/"
                             + f.getFileNameWithoutExtension().toLowerCase();
            groups[key].push_back(f);
        }

        std::vector<AddSongsDialog::FileEntry> entries;
        for (auto& [key, files] : groups)
        {
            if (token->load()) break;

            juce::File primary = files[0];
            for (auto& f : files)
            {
                auto ext = f.getFileExtension().toLowerCase();
                if (ext == ".cdg" || ext == ".zip") { primary = f; break; }
            }

            const juce::String baseName = primary.getFileNameWithoutExtension();
            juce::String artist, song, code, version;
            LibraryScanner::parseFilename(baseName, artist, song, code, version);

            if (existingKeys.count(LibraryScanner::normaliseSongKey(artist, song)) > 0)
                continue;

            AddSongsDialog::FileEntry entry;
            entry.baseName    = baseName;
            entry.displayName = baseName + primary.getFileExtension();
            entry.primaryFile = primary;
            entry.selected    = true;
            entries.push_back(std::move(entry));
        }

        if (token->load())
        {
            juce::MessageManager::callAsync([safe]()
            {
                if (safe != nullptr) safe->onFolderScanCancelled();
            });
            return;
        }

        juce::MessageManager::callAsync([safe, entries = std::move(entries)]() mutable
        {
            if (safe != nullptr) safe->onFolderScanComplete(std::move(entries));
        });
    });
}

void AddSongsDialog::onFolderScanComplete(std::vector<FileEntry> entries)
{
    fileEntries_ = std::move(entries);
    scanningLabel_->setVisible(false);
    fileListBox_->setVisible(true);
    selectAllToggle_->setVisible(true);
    browseBtn_->setEnabled(true);
    fileListBox_->updateContent();
    updateSelectAll();
    updateImportButton();
    resized();
}

void AddSongsDialog::onFolderScanCancelled()
{
    fileEntries_.clear();
    scanningLabel_->setVisible(false);
    fileListBox_->setVisible(true);
    selectAllToggle_->setVisible(true);
    browseBtn_->setEnabled(true);
    fileListBox_->updateContent();
    updateSelectAll();
    updateImportButton();
    resized();
}

//==============================================================================
// Phase management
//==============================================================================
void AddSongsDialog::setPhase(Phase p)
{
    phase_ = p;

    const bool isSetup     = (p == Phase::Setup);
    const bool isImporting = (p == Phase::Importing);
    const bool isComplete  = (p == Phase::Complete);

    // Update title
    if (isSetup)     titleLabel_->setText("Add Songs to Library",  juce::dontSendNotification);
    if (isImporting) titleLabel_->setText("Importing...",          juce::dontSendNotification);
    if (isComplete)  titleLabel_->setText("Import Complete!",      juce::dontSendNotification);

    // Show/hide list area
    fileListBox_->setVisible(isSetup);
    selectAllToggle_->setVisible(isSetup);

    progressLabel_->setVisible(isImporting);
    statsLabel_->setVisible(isImporting || isComplete);
    progressBar_->setVisible(isImporting);
    statusListBox_->setVisible(isImporting || isComplete);

    // Buttons
    importBtn_->setVisible(! isComplete);
    importBtn_->setEnabled(false);
    importMoreBtn_->setVisible(isComplete);
    cancelBtn_->setButtonText(isComplete ? "Close" : "Cancel");

    resized();
    repaint();
}

//==============================================================================
// Helpers
//==============================================================================
int AddSongsDialog::selectedCount() const
{
    int n = 0;
    for (const auto& e : fileEntries_)
        if (e.selected) ++n;
    return n;
}

void AddSongsDialog::updateSelectAll()
{
    const int total = (int) fileEntries_.size();
    const int sel   = selectedCount();
    selectAllToggle_->setToggleState(sel == total && total > 0,
                                     juce::dontSendNotification);
    selectAllToggle_->setButtonText("Select All (" + juce::String(sel)
                                    + " of " + juce::String(total) + " selected)");
}

void AddSongsDialog::updateImportButton()
{
    const int n = selectedCount();
    importBtn_->setButtonText(n == 0 ? "Import Songs" : "Import " + juce::String(n) + " Songs");
    importBtn_->setEnabled(n > 0 && phase_ == Phase::Setup);
}

//==============================================================================
// Actions
//==============================================================================
void AddSongsDialog::onBrowse()
{
    juce::File startDir = sourcePath_.isNotEmpty() ? juce::File(sourcePath_)
                                                   : juce::File::getSpecialLocation(
                                                         juce::File::userMusicDirectory);

    fileChooser_ = std::make_shared<juce::FileChooser>(
        "Select folder containing new songs", startDir);

    fileChooser_->launchAsync(
        juce::FileBrowserComponent::openMode |
        juce::FileBrowserComponent::canSelectDirectories,
        [this](const juce::FileChooser& fc)
        {
            auto result = fc.getResult();
            if (! result.isDirectory()) return;
            sourcePath_ = result.getFullPathName();
            pathEditor_->setText(sourcePath_, juce::dontSendNotification);
            savePrefs();
            startFolderScan(result);
        });
}

void AddSongsDialog::onImport()
{
    const int n = selectedCount();
    if (n == 0) return;

    // Collect selected base names for the scanner filter
    std::vector<juce::String> selectedBaseNames;
    for (const auto& e : fileEntries_)
        if (e.selected) selectedBaseNames.push_back(e.baseName);

    scanner_.setAppendFilter(selectedBaseNames);
    scanner_.setAppendPostOps(copySongs_, deleteSongs_, libraryRootDir_);

    // Reset state
    importRecords_.clear();
    stats_          = {};
    progressValue_  = 0.0;
    mergedSongs_.clear();

    setPhase(Phase::Importing);

    // Wire scanner callbacks
    juce::Component::SafePointer<AddSongsDialog> safe(this);

    scanner_.onProgress = [safe](int cur, int tot, juce::String songName)
    {
        if (safe == nullptr) return;
        if (tot > 0) safe->progressValue_ = (double) cur / (double) tot;
        safe->progressLabel_->setText(
            "Processing " + juce::String(cur) + " of " + juce::String(tot)
            + (songName.isEmpty() ? "" : ":  " + songName),
            juce::dontSendNotification);
        safe->progressBar_->repaint();
    };

    scanner_.onFileImported = [safe](juce::String baseName, bool isNew, juce::String message)
    {
        if (safe == nullptr) return;
        ImportRecord rec;
        rec.name    = baseName;
        rec.isNew   = isNew;
        rec.message = message;
        safe->importRecords_.push_back(std::move(rec));
        safe->statsLabel_->setText(
            "New: " + juce::String(safe->stats_.newSongs)
            + "  |  Already Exists: " + juce::String(safe->stats_.alreadyImported),
            juce::dontSendNotification);
        safe->statusListBox_->updateContent();
        safe->statusListBox_->scrollToEnsureRowIsOnscreen((int) safe->importRecords_.size() - 1);
    };

    scanner_.onComplete = [safe](std::vector<CdgSong> merged, LibraryScanner::ScanStats scanStats)
    {
        if (safe == nullptr) return;
        safe->mergedSongs_           = std::move(merged);
        safe->stats_.newSongs        = scanStats.numNew;
        safe->stats_.alreadyImported = scanStats.numAlreadyImported;
        safe->stats_.total           = scanStats.numNew + scanStats.numAlreadyImported;
        safe->progressValue_         = 1.0;

        safe->statsLabel_->setText(
            "New: " + juce::String(safe->stats_.newSongs)
            + "  |  Already Exists: " + juce::String(safe->stats_.alreadyImported)
            + "  |  Total: " + juce::String(safe->stats_.total),
            juce::dontSendNotification);

        safe->statusListBox_->updateContent();
        safe->setPhase(Phase::Complete);

        // Notify caller (e.g. LibraryPage) to update its song list
        if (safe->onComplete)
            safe->onComplete(safe->mergedSongs_, safe->stats_);
    };

    scanner_.onError = [safe](juce::String err)
    {
        if (safe == nullptr) return;
        safe->progressLabel_->setText("Error: " + err, juce::dontSendNotification);
    };

    juce::File sourceDir(sourcePath_);
    scanner_.startAppendScan(sourceDir, existingSongs_);
}

void AddSongsDialog::onCancel()
{
    folderScanToken_->store(true);   // abort any running folder scan
    scanner_.stopScan();
    if (auto* w = findParentComponentOfClass<juce::DocumentWindow>())
        w->exitModalState(0);
}

void AddSongsDialog::onImportMore()
{
    importRecords_.clear();
    stats_         = {};
    progressValue_ = 0.0;
    mergedSongs_.clear();
    fileEntries_.clear();
    fileListBox_->updateContent();

    // Refresh the file list from the same path (new songs may have been added)
    if (sourcePath_.isNotEmpty())
    {
        // Update existingSongs_ with the latest merged result if available
        if (! mergedSongs_.empty())
            existingSongs_ = mergedSongs_;

        startFolderScan(juce::File(sourcePath_));
    }

    setPhase(Phase::Setup);
}

//==============================================================================
// paint
//==============================================================================
void AddSongsDialog::paint(juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();

    // Background
    g.setColour(juce::Colour(kBg));
    g.fillRoundedRectangle(bounds, 8.0f);

    // Border
    g.setColour(juce::Colour(kBorder));
    g.drawRoundedRectangle(bounds.reduced(0.5f), 8.0f, 1.0f);

    // Header separator
    g.setColour(juce::Colour(kBorder));
    g.drawLine(12.0f, 48.0f, (float) getWidth() - 12.0f, 48.0f, 1.0f);

    if (phase_ == Phase::Complete)
    {
        // Draw 4 stat boxes  (New / Already Exists / Failed / Total)
        const int boxY   = 190;
        const int boxH   = 70;
        const int gap    = 10;
        const int boxW   = (getWidth() - 40 - gap * 3) / 4;
        const int startX = 20;

        struct Box { juce::String value; juce::String label; juce::Colour colour; };
        Box boxes[4] = {
            { juce::String(stats_.newSongs),        "New Songs",      juce::Colour(kGreen) },
            { juce::String(stats_.alreadyImported), "Already Existed",juce::Colour(0xffaaaaaa) },
            { juce::String(stats_.failed),          "Failed",         juce::Colour(0xffee4444) },
            { juce::String(stats_.total),           "Total Processed",juce::Colour(kAccent) },
        };

        for (int i = 0; i < 4; ++i)
        {
            const juce::Rectangle<float> r(
                (float)(startX + i * (boxW + gap)), (float) boxY,
                (float) boxW, (float) boxH);

            g.setColour(juce::Colour(kCard));
            g.fillRoundedRectangle(r, 6.0f);
            g.setColour(boxes[i].colour);
            g.drawRoundedRectangle(r.reduced(0.5f), 6.0f, 1.5f);

            g.setColour(boxes[i].colour);
            g.setFont(juce::Font(juce::FontOptions().withHeight(26.0f)).boldened());
            g.drawText(boxes[i].value, r.withHeight(r.getHeight() * 0.6f),
                       juce::Justification::centred);

            g.setColour(juce::Colour(kGrey));
            g.setFont(juce::Font(juce::FontOptions().withHeight(11.0f)));
            g.drawText(boxes[i].label,
                       r.withY(r.getY() + r.getHeight() * 0.6f)
                        .withHeight(r.getHeight() * 0.4f),
                       juce::Justification::centred);
        }
    }
}

//==============================================================================
// resized
//==============================================================================
void AddSongsDialog::resized()
{
    const int pad  = 16;
    const int bh   = 30;   // standard row height
    const int btnH = 36;
    const int btnW = 160;

    // Header row
    titleLabel_->setBounds(pad, 10, getWidth() - pad * 2 - 40, 30);
    closeBtn_  ->setBounds(getWidth() - pad - 32, 8, 32, 32);

    int y = 56;

    // Path row
    pathEditor_->setBounds(pad, y, getWidth() - pad * 2 - 100, bh);
    browseBtn_ ->setBounds(getWidth() - pad - 96, y, 96, bh);
    y += bh + 8;

    // Options
    copySongsToggle_  ->setBounds(pad, y, getWidth() - pad * 2, bh);  y += bh + 4;
    deleteSongsToggle_->setBounds(pad, y, getWidth() - pad * 2, bh);  y += bh + 8;

    const int btnBarY = getHeight() - btnH - pad;
    const int listH   = btnBarY - y - 4;

    if (phase_ == Phase::Setup)
    {
        selectAllToggle_->setBounds(pad, y, getWidth() - pad * 2, bh);
        fileListBox_->setBounds(pad, y + bh + 4, getWidth() - pad * 2, listH - bh - 4);
        scanningLabel_->setBounds(pad, y + bh + 4, getWidth() - pad * 2, listH - bh - 4);
    }
    else if (phase_ == Phase::Importing)
    {
        progressLabel_->setBounds(pad, y, getWidth() - pad * 2, bh);          y += bh + 4;
        statsLabel_   ->setBounds(pad, y, getWidth() - pad * 2, bh);          y += bh + 4;
        progressBar_  ->setBounds(pad, y, getWidth() - pad * 2, 18);          y += 22;
        statusListBox_->setBounds(pad, y, getWidth() - pad * 2, btnBarY - y - 4);
    }
    else  // Complete
    {
        // Stat boxes are painted directly at y=190; push status list below them
        const int listStartY = 190 + 70 + 12;
        statsLabel_   ->setBounds(pad, y, getWidth() - pad * 2, bh);
        statusListBox_->setBounds(pad, listStartY,
                                  getWidth() - pad * 2, btnBarY - listStartY - 4);
    }

    // Button bar
    const int btnBarCentreX = getWidth() / 2;
    importBtn_   ->setBounds(btnBarCentreX - btnW - 8, btnBarY, btnW, btnH);
    importMoreBtn_->setBounds(btnBarCentreX - btnW - 8, btnBarY, btnW, btnH);
    cancelBtn_   ->setBounds(btnBarCentreX + 8,         btnBarY, btnW, btnH);
}
