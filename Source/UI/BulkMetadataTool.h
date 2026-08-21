/*
  ==============================================================================

    BulkMetadataTool.h

    Viracicom Admin > Bulk Metadata Tool. Operates on the bundled master
    catalog (assets/data/meta_data.json, copied to the writable app-data dir
    by LibraryScanner -- ~48,500 songs), NOT any one venue's library: shows
    how many catalog entries have full metadata vs. are still missing it,
    lets the admin run a batch of 100/250/500/1000 through
    ApiService::searchArtistAndSong() (optionally AI-precleaned first via
    AiSongNameCleanupService), respects the shared daily Spotify quota
    (MetadataQuotaService), and either:

      Fix Broken  -- stops on every failure, shows SongEditDialog pre-filled
                     with the Spotify error so the admin can correct the
                     artist/song spelling and retry once, then continues.
      Not Fix     -- runs straight through every song in the batch, records
                     failures, never stops for input.

    Either way, every successful hit is written back into the SAME catalog
    entry (preserving fields this tool doesn't touch, e.g. fileName/fileDate)
    and the file is re-saved; the Cloud Function side already upserts into
    the shared metadataSongs collection on every successful Spotify hit (see
    firebase/functions/index.js), so this also seeds the shared cache for
    every venue.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "../Models/CdgSong.h"
#include "../Services/ApiService.h"
#include <vector>

//==============================================================================
class BulkMetadataTool : public juce::Component
{
public:
    BulkMetadataTool();
    ~BulkMetadataTool() override;

    void paint (juce::Graphics& g) override;
    void resized() override;

    static void launch (juce::Component* parent);

private:
    //==========================================================================
    struct FailureRow
    {
        juce::String docId, artistName, songName, error;
    };

    class ReportListModel : public juce::ListBoxModel
    {
    public:
        std::vector<FailureRow>* rows = nullptr;

        int getNumRows() override { return rows != nullptr ? (int) rows->size() : 0; }
        void paintListBoxItem (int rowNumber, juce::Graphics& g, int width, int height, bool rowIsSelected) override;
    };

    //==========================================================================
    void refreshCounts();
    void refreshQuota();
    void selectBatchSize (int size);
    void selectMode (bool fixBroken);
    void updateBatchButtonStates();
    void updateRunButtonState();

    void startRun();
    void cancelRun();
    void processNext();
    void beginAttempt (const juce::String& docId, bool isManualRetry,
                       const juce::String& forcedArtist, const juce::String& forcedSong);
    void handleLookupResult (const juce::String& docId, const juce::String& searchArtist,
                             const juce::String& searchSong, ApiService::Result result);
    void openFixDialog (const juce::String& docId, const juce::String& searchArtist,
                        const juce::String& searchSong, const juce::String& errorMsg);
    void recordFailure (const juce::String& docId, const juce::String& artist,
                        const juce::String& song, const juce::String& error);
    void finalizeRun();

    void setStatus (const juce::String& msg);
    void showRunView();
    void showReportView();

    juce::File catalogFile() const;
    bool loadCatalogIfNeeded (juce::String& outError);
    bool saveCatalog();
    static CdgSong entryToCdgSong (const juce::String& docId, juce::DynamicObject* obj);
    static void applyResultToEntry (juce::DynamicObject* obj, const CdgSong& song);
    static bool entryHasMetadata (juce::DynamicObject* obj);

    //==========================================================================
    // Header
    juce::Label      titleLabel_;
    juce::TextButton closeButton_ { "X" };

    // Stats
    juce::Label      statsLabel_;
    juce::TextButton refreshCountsButton_ { "Refresh Counts" };

    // Quota
    juce::Label      quotaLabel_;
    juce::TextButton refreshQuotaButton_ { "Refresh Quota" };

    // AI cleanup
    juce::ToggleButton aiCleanupToggle_ { "Use AI to pre-clean song/artist text before Spotify" };
    juce::Label      apiKeyLabel_;
    juce::TextEditor apiKeyEditor_;

    // Batch size
    juce::Label      batchSizeLabel_;
    juce::TextButton batch100Button_  { "100" };
    juce::TextButton batch250Button_  { "250" };
    juce::TextButton batch500Button_  { "500" };
    juce::TextButton batch1000Button_ { "1000" };
    int selectedBatchSize_ = 100;

    // Mode
    juce::Label      modeLabel_;
    juce::TextButton fixBrokenButton_ { "Fix Broken" };
    juce::TextButton notFixButton_    { "Not Fix" };
    bool fixBrokenMode_ = false;

    // Run controls
    juce::TextButton runButton_    { "Run" };
    juce::TextButton cancelButton_ { "Cancel" };
    juce::Label      progressLabel_;
    double progressValue_ = 0.0;
    juce::ProgressBar progressBar_ { progressValue_ };
    juce::Label      statusLabel_;

    // Report view
    bool showingReport_ = false;
    juce::Label      reportSummaryLabel_;
    ReportListModel  reportModel_;
    juce::ListBox    reportList_ { "bulkMetaReport", &reportModel_ };
    juce::TextButton backToToolButton_ { "Back" };
    juce::TextButton runNextBatchButton_ { "Run Next Batch" };

    //==========================================================================
    // Catalog state
    juce::var   catalogRoot_;
    bool        catalogLoaded_ = false;
    int         countWithMetadata_ = 0;
    int         countWithoutMetadata_ = 0;
    std::vector<juce::String> missingDocIds_;

    // Quota state
    bool quotaLoaded_ = false;
    int  quotaRemaining_ = 0;
    int  quotaCap_ = 1000;

    // Run state
    bool running_ = false;
    bool cancelRequested_ = false;
    std::vector<juce::String> targetDocIds_;
    size_t runPosition_ = 0;
    int runSucceeded_ = 0;
    int runFailed_ = 0;
    std::vector<FailureRow> failures_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BulkMetadataTool)
};
