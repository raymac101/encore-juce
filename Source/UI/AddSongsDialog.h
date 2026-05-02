/*
  ==============================================================================

    AddSongsDialog.h

    Modal dialog that mirrors the Angular add-modal-component.

    Three phases:
      Setup    — path input, options, scrollable file list with checkboxes
                 showing only files NOT already in the local library.
      Importing — live progress bar + per-file status list.
      Complete  — summary stat boxes + full processed-file list.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include <atomic>
#include <memory>
#include <set>
#include "../Models/CdgSong.h"
#include "../Services/LibraryScanner.h"

class AddSongsDialog : public juce::Component
{
public:
    //==========================================================================
    struct ImportStats
    {
        int newSongs        = 0;
        int alreadyImported = 0;
        int failed          = 0;
        int total           = 0;
    };

    struct ImportRecord
    {
        juce::String name;
        bool         isNew = false;
        juce::String message;
    };

    //==========================================================================
    /** Launch the dialog modally.  `libraryRootDir` is used as the copy
        destination when the user checks "Copy Songs to Library". */
    static void launch(juce::Component*              parent,
                       const std::vector<CdgSong>&   existingSongs,
                       const juce::File&             libraryRootDir,
                       std::function<void(std::vector<CdgSong>, ImportStats)> onComplete);

    AddSongsDialog(const std::vector<CdgSong>& existingSongs,
                   const juce::File& libraryRootDir);
    ~AddSongsDialog() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    /** Fires on the message thread when the import finishes. */
    std::function<void(std::vector<CdgSong>, ImportStats)> onComplete;

    static constexpr int kWidth  = 680;
    static constexpr int kHeight = 630;

private:
    //==========================================================================
    enum class Phase { Setup, Importing, Complete };
    Phase phase_ = Phase::Setup;

    std::vector<CdgSong>    existingSongs_;
    juce::File              libraryRootDir_;
    std::vector<ImportRecord> importRecords_;
    ImportStats             stats_;
    std::vector<CdgSong>    mergedSongs_;

    juce::String sourcePath_;
    bool         copySongs_   = false;
    bool         deleteSongs_ = false;

    LibraryScanner scanner_;

    //==========================================================================
    // File entry (setup phase)
    struct FileEntry
    {
        juce::String baseName;      // filename without extension
        juce::String displayName;   // baseName + primary extension
        juce::File   primaryFile;
        bool         selected = true;
    };
    std::vector<FileEntry> fileEntries_;

    //==========================================================================
    // Inner ListBox models — forward-declared; defined in .cpp
    class FileListModel;
    class StatusListModel;
    std::unique_ptr<FileListModel>   fileListModel_;
    std::unique_ptr<StatusListModel> statusListModel_;

    //==========================================================================
    // Header
    std::unique_ptr<juce::Label>      titleLabel_;
    std::unique_ptr<juce::TextButton> closeBtn_;

    // Path row
    std::unique_ptr<juce::TextEditor> pathEditor_;
    std::unique_ptr<juce::TextButton> browseBtn_;

    // Options
    std::unique_ptr<juce::ToggleButton> copySongsToggle_;
    std::unique_ptr<juce::ToggleButton> deleteSongsToggle_;

    // File list (setup)
    std::unique_ptr<juce::ToggleButton> selectAllToggle_;
    std::unique_ptr<juce::ListBox>      fileListBox_;
    std::unique_ptr<juce::Label>        scanningLabel_;   // shown while folder scan runs

    // Shared cancel token — set to true to abort a running folder scan
    std::shared_ptr<std::atomic<bool>>  folderScanToken_ =
        std::make_shared<std::atomic<bool>>(false);

    // Progress area (importing)
    std::unique_ptr<juce::Label>        progressLabel_;
    std::unique_ptr<juce::Label>        statsLabel_;
    std::unique_ptr<juce::ProgressBar>  progressBar_;
    double                              progressValue_ = 0.0;
    std::unique_ptr<juce::ListBox>      statusListBox_;

    // Buttons
    std::unique_ptr<juce::TextButton>   importBtn_;
    std::unique_ptr<juce::TextButton>   cancelBtn_;
    std::unique_ptr<juce::TextButton>   importMoreBtn_;

    // File chooser
    std::shared_ptr<juce::FileChooser>  fileChooser_;

    //==========================================================================
    void loadPrefs();
    void savePrefs();
    void startFolderScan(const juce::File& dir);   // async — replaces populateFileList
    void onFolderScanComplete(std::vector<FileEntry> entries);
    void onFolderScanCancelled();
    void setPhase(Phase p);
    void updateSelectAll();
    void updateImportButton();
    int  selectedCount() const;

    void onBrowse();
    void onImport();
    void onCancel();
    void onImportMore();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AddSongsDialog)
};
