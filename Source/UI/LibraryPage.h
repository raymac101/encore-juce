/*
  ==============================================================================

    LibraryPage.h
    Created: 22 Apr 2026
    Author:  GitHub Copilot

    Library management page — migrated from the Angular LibraryComponent.

    Lets the user:
      • Set the root path to their karaoke file collection
      • Run an "Initial Song Load" (full scan → replaces the songbook)
      • "Add Songs" (incremental scan → appended to the songbook)
      • "Get Meta Data" (match against a local metadata cache)
      • View live scan progress with a progress bar and current-song label
      • See file-type statistics for the loaded collection

    Uses LibraryScanner (a juce::Thread subclass) for background work and
    polls it via juce::Timer to keep the UI updated.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "../Services/LibraryScanner.h"
#include <vector>
#include <functional>

//==============================================================================
class LibraryPage : public juce::Component,
                    public juce::Timer
{
public:
    //==========================================================================
    LibraryPage();
    ~LibraryPage() override;

    //==========================================================================
    // Component overrides
    void paint(juce::Graphics& g) override;
    void resized() override;

    //==========================================================================
    // Load the current songbook from disk and refresh stats.
    // Called once at startup, and after every scan completes.
    void loadSongbook();

    //==========================================================================
    // Localization
    void updateAllText();

    //==========================================================================
    // Callback — fired whenever the songbook changes so the Search page
    // (and other consumers) can refresh their song list.
    std::function<void(const std::vector<CdgSong>&)> onSongbookChanged;

    //==========================================================================
    // Read-only access to the currently loaded songs
    const std::vector<CdgSong>& getSongs() const { return songs_; }

private:
    //==========================================================================
    // juce::Timer — polls scan progress ~10 Hz
    void timerCallback() override;

    //==========================================================================
    // Button actions
    void onInitialSongLoad();   // Full scan of a chosen directory
    void onAddSongs();          // Append scan of a chosen directory
    void onGetMetaData();       // Apply local metadata cache
    void onEditGenres();        // Genre editor (stub)

    //==========================================================================
    // Helpers
    void startFolderChooser(bool appendMode);
    void refreshStats();
    void setProgressVisible(bool visible);
    void setScanningState(bool scanning);
    void showMessage(const juce::String& msg, bool isError = false);

    //==========================================================================
    // UI colours (consistent with the rest of the app)
    static constexpr uint32_t kBg          = 0xff16213e;
    static constexpr uint32_t kPanel       = 0xff1a2550;
    static constexpr uint32_t kAccent      = 0xff7b5ea7;
    static constexpr uint32_t kBtnNormal   = 0xff2d2d3a;
    static constexpr uint32_t kTextPrimary = 0xffe4e4e4;
    static constexpr uint32_t kTextSecond  = 0xffa3a6a8;

    //==========================================================================
    // Header
    std::unique_ptr<juce::Label>      titleLabel_;

    // Path section
    std::unique_ptr<juce::Label>      pathLabel_;
    std::unique_ptr<juce::TextEditor> pathEditor_;
    std::unique_ptr<juce::TextButton> browseBtn_;

    // Progress section (hidden when idle)
    std::unique_ptr<juce::Label>      progressLabel_;
    std::unique_ptr<juce::ProgressBar> progressBar_;
    std::unique_ptr<juce::Label>      currentSongLabel_;
    double progressValue_ = 0.0;   // owned by ProgressBar

    // Message / status
    std::unique_ptr<juce::Label> messageLabel_;

    // Button bar
    std::unique_ptr<juce::TextButton> initialSongLoadBtn_;
    std::unique_ptr<juce::TextButton> addSongsBtn_;
    std::unique_ptr<juce::TextButton> getMetaDataBtn_;
    std::unique_ptr<juce::TextButton> editGenresBtn_;

    // Stats panel
    std::unique_ptr<juce::Label> statsTotalLabel_;
    std::unique_ptr<juce::Label> statsMetaLabel_;
    std::unique_ptr<juce::Label> statsCDGLabel_;
    std::unique_ptr<juce::Label> statsZipLabel_;
    std::unique_ptr<juce::Label> statsMP4Label_;
    std::unique_ptr<juce::Label> statsM4ALabel_;
    std::unique_ptr<juce::Label> statsXMLLabel_;
    std::unique_ptr<juce::Label> statsUnknownLabel_;
    std::unique_ptr<juce::Label> statsGroupsLabel_;

    //==========================================================================
    // Data
    std::vector<CdgSong>          songs_;
    LibraryScanner                scanner_;
    LibraryScanner::ScanStats     stats_;

    // FileChooser must outlive the callback lambda
    std::shared_ptr<juce::FileChooser> fileChooser_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LibraryPage)
};
