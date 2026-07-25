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
#include "../Services/SongDatabase.h"
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
    // Callback — fired with a short phase description ("Scanning Folders...",
    // "Adding Meta Data...", "Uploading Songbook...", etc.) during a scan/
    // metadata/upload sequence, and with an empty string when there's
    // nothing in progress. MainComponent forwards this to the BottomBar's
    // status area (where "Audio Ready" normally shows).
    std::function<void(juce::String)> onStatusMessage;

    //==========================================================================
    // Read-only access to the currently loaded songs
    const std::vector<CdgSong>& getSongs() const { return songs_; }

    //==========================================================================
    // Insert or update a song in the in-memory library, persist the songbook
    // to disk + the SQLite index, and fire onSongbookChanged.  Match priority:
    // by id (when non-empty), otherwise by case-insensitive (artist|song).
    // Returns true if the songbook actually changed.
    bool upsertSong(const CdgSong& song);

    //==========================================================================
    // Remove a song matched by id (preferred) or by case-insensitive
    // (artist|song).  Persists + fires onSongbookChanged.  Returns true if a
    // matching record was found and removed.
    bool deleteSong(const CdgSong& song);

    //==========================================================================
    // Public action: open the folder chooser and run a full ("initial")
    // library scan. Used by the venue-switch flow in LoginWindow → MainComponent
    // when the host picks a venue that wasn't previously configured on this PC.
    void startInitialSongLoad() { onInitialSongLoad(); }

    //==========================================================================
    // Sets the venue whose Firebase Storage copy of songbook.json should be
    // refreshed after each completed scan. Set by MainComponent whenever the
    // active venue changes.
    void setActiveVenueId(const juce::String& venueId) { activeVenueId_ = venueId; }

private:
    //==========================================================================
    // Plain Component with a paint callback -- lets the decorative header
    // panels drawn around the form scroll along with it (Viewport-clipped
    // children are repositioned automatically, but a *parent's* own paint()
    // isn't, so that drawing has to happen on this scrolled child instead
    // of on LibraryPage itself).
    class ContentHolder : public juce::Component
    {
    public:
        std::function<void(juce::Graphics&)> onPaint;
        void paint(juce::Graphics& g) override { if (onPaint) onPaint(g); }
    };

    std::unique_ptr<ContentHolder> contentHolder_;
    juce::Viewport viewport_;

    // Runs the existing fixed-pixel layout against contentHolder_'s current
    // bounds. Called twice from resized() -- once at a starting guess
    // height, then again after correcting contentHolder_ to the height the
    // stats panel actually needed -- so the viewport's scroll range always
    // reaches the last stat row on any window size.
    void layoutContent();

    //==========================================================================
    // juce::Timer — polls scan progress ~10 Hz
    void timerCallback() override;

    //==========================================================================
    // Button actions
    void onInitialSongLoad();   // Full scan of a chosen directory
    void onAddSongs();          // Append scan of a chosen directory
    void onGetMetaData();       // Apply local metadata cache
    void onEditGenres();        // Genre editor (stub)
    void fetchMetadataForImportedSongs(std::vector<size_t> songIndices,
                       bool allowOnlineLookup = true);

    //==========================================================================
    // Helpers
    void startFolderChooser(bool appendMode);

    // Saves songs_ to the local songbook.json/SQLite index (via scanner_)
    // and, if a venue is active, re-uploads the local songbook.json to
    // Firebase Storage so TAGG always sees the current library. Callable
    // from any thread (the Storage upload dispatches its own background
    // thread). Use this instead of calling scanner_.saveSongbook() directly.
    void persistSongbook();

    // Fires onStatusMessage (if set) and bumps statusMessageToken_, so a
    // stale delayed-clear callback from an earlier phase (see persistSongbook)
    // can detect it's no longer current and no-op instead of wiping out a
    // newer message.
    void reportStatus(const juce::String& message);

    void refreshStats();
    void setProgressVisible(bool visible);
    void setScanningState(bool scanning);
    void showMessage(const juce::String& msg, bool isError = false);
    void loadSongbookAsync();

    //==========================================================================
    // UI colours (consistent with the rest of the app)
    static constexpr uint32_t kBg          = 0xff16213e;
    static constexpr uint32_t kPanel       = 0xff1a2a52;
    static constexpr uint32_t kAccent      = 0xff5a8fd8;
    static constexpr uint32_t kBtnNormal   = 0xff2f4b80;
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
    SongDatabase                  songDb_;   // SQLite index; opened in constructor
    std::vector<CdgSong>          songs_;
    LibraryScanner                scanner_;
    LibraryScanner::ScanStats     stats_;
    bool                          lastScanWasAppend_ = false;
    int                           globalScanTaskId_ = 0;
    juce::String                  activeVenueId_;
    int                           statusMessageToken_ = 0;

    // FileChooser must outlive the callback lambda
    std::shared_ptr<juce::FileChooser> fileChooser_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LibraryPage)
};
