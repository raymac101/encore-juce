/*
  ==============================================================================

    BulkMetadataTool.cpp

  ==============================================================================
*/

#include "BulkMetadataTool.h"
#include "SongEditDialog.h"
#include "BorderlessModalWindow.h"
#include "../Services/AiSongNameCleanupService.h"
#include "../Services/MetadataQuotaService.h"
#include "../Services/LibraryScanner.h"
#include "../Services/SongDatabase.h"
#include "../Services/UserPreferences.h"
#include "../Localization/LocalizationManager.h"
#include <set>
#include <algorithm>

namespace
{
    constexpr int kDialogWidth  = 760;
    constexpr int kDialogHeight = 820;

    constexpr auto kBgColour     = 0xff1a2030;
    constexpr auto kPanelColour  = 0xff0d1527;
    constexpr auto kBorderColour = 0xff2d3a5a;
    constexpr auto kAccentColour = 0xff30daff;
    constexpr auto kTextColour   = 0xfff7f8fa;
    constexpr auto kMutedColour  = 0xffa4b0c4;
    constexpr auto kDangerColour = 0xffe53e3e;
    constexpr auto kStatusErrBg  = 0xff7f1d1d;

    void styleField (juce::Label& lbl, const juce::String& text)
    {
        lbl.setText (text, juce::dontSendNotification);
        lbl.setColour (juce::Label::textColourId, juce::Colour (kMutedColour));
        lbl.setFont (juce::Font (juce::FontOptions().withHeight (13.0f)));
    }

    void styleToggleButton (juce::TextButton& b, bool selected)
    {
        b.setColour (juce::TextButton::buttonColourId,
                     selected ? juce::Colour (kAccentColour) : juce::Colour (kPanelColour));
        b.setColour (juce::TextButton::textColourOnId,
                     selected ? juce::Colour (0xff0d1527) : juce::Colour (kTextColour));
        b.setColour (juce::TextButton::textColourOffId,
                     selected ? juce::Colour (0xff0d1527) : juce::Colour (kTextColour));
    }
}

//==============================================================================
void BulkMetadataTool::ReportListModel::paintListBoxItem (int rowNumber, juce::Graphics& g,
                                                           int width, int height, bool)
{
    if (rows == nullptr || rowNumber < 0 || rowNumber >= (int) rows->size())
        return;

    const auto& row = (*rows)[(size_t) rowNumber];

    g.setColour (juce::Colour ((juce::uint32) (rowNumber % 2 == 0 ? 0xff141b2e : 0xff10182a)));
    g.fillRect (0, 0, width, height);

    auto area = juce::Rectangle<int> (0, 0, width, height).reduced (8, 2);
    auto nameArea = area.removeFromTop (height / 2);
    auto errArea  = area;

    g.setColour (juce::Colour (kTextColour));
    g.setFont (juce::Font (juce::FontOptions().withHeight (13.0f)).boldened());
    g.drawFittedText (row.artistName + " - " + row.songName, nameArea, juce::Justification::centredLeft, 1);

    g.setColour (juce::Colour (kDangerColour));
    g.setFont (juce::Font (juce::FontOptions().withHeight (12.0f)));
    g.drawFittedText (row.error, errArea, juce::Justification::centredLeft, 1);
}

//==============================================================================
BulkMetadataTool::BulkMetadataTool()
{
    setSize (kDialogWidth, kDialogHeight);

    titleLabel_.setText ("Bulk Metadata Tool", juce::dontSendNotification);
    titleLabel_.setColour (juce::Label::textColourId, juce::Colour (kTextColour));
    titleLabel_.setFont (juce::Font (juce::FontOptions().withHeight (22.0f)).boldened());
    addAndMakeVisible (titleLabel_);

    closeButton_.setColour (juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
    closeButton_.setColour (juce::TextButton::textColourOnId,  juce::Colour (kMutedColour));
    closeButton_.setColour (juce::TextButton::textColourOffId, juce::Colour (kMutedColour));
    closeButton_.onClick = [this]()
    {
        if (auto* dw = findParentComponentOfClass<juce::DocumentWindow>())
            dw->exitModalState (0);
    };
    addAndMakeVisible (closeButton_);

    //--- Stats -----------------------------------------------------------------
    statsLabel_.setColour (juce::Label::textColourId, juce::Colour (kTextColour));
    statsLabel_.setFont (juce::Font (juce::FontOptions().withHeight (14.0f)));
    statsLabel_.setText ("Loading catalog...", juce::dontSendNotification);
    addAndMakeVisible (statsLabel_);

    refreshCountsButton_.setColour (juce::TextButton::buttonColourId, juce::Colour (kPanelColour));
    refreshCountsButton_.setColour (juce::TextButton::textColourOnId,  juce::Colour (kTextColour));
    refreshCountsButton_.setColour (juce::TextButton::textColourOffId, juce::Colour (kTextColour));
    refreshCountsButton_.onClick = [this]() { refreshCounts(); };
    addAndMakeVisible (refreshCountsButton_);

    reviewButton_.setColour (juce::TextButton::buttonColourId, juce::Colour (kPanelColour));
    reviewButton_.setColour (juce::TextButton::textColourOnId,  juce::Colour (kTextColour));
    reviewButton_.setColour (juce::TextButton::textColourOffId, juce::Colour (kTextColour));
    reviewButton_.onClick = [this]() { showManualReviewList(); };
    reviewButton_.setEnabled (false);
    addAndMakeVisible (reviewButton_);

    //--- Quota -------------------------------------------------------------------
    quotaLabel_.setColour (juce::Label::textColourId, juce::Colour (kTextColour));
    quotaLabel_.setFont (juce::Font (juce::FontOptions().withHeight (14.0f)));
    quotaLabel_.setText ("Loading quota...", juce::dontSendNotification);
    addAndMakeVisible (quotaLabel_);

    refreshQuotaButton_.setColour (juce::TextButton::buttonColourId, juce::Colour (kPanelColour));
    refreshQuotaButton_.setColour (juce::TextButton::textColourOnId,  juce::Colour (kTextColour));
    refreshQuotaButton_.setColour (juce::TextButton::textColourOffId, juce::Colour (kTextColour));
    refreshQuotaButton_.onClick = [this]() { refreshQuota(); };
    addAndMakeVisible (refreshQuotaButton_);

    //--- AI cleanup --------------------------------------------------------------
    aiCleanupToggle_.setColour (juce::ToggleButton::textColourId, juce::Colour (kTextColour));
    aiCleanupToggle_.setColour (juce::ToggleButton::tickColourId, juce::Colour (kAccentColour));
    addAndMakeVisible (aiCleanupToggle_);

    styleField (apiKeyLabel_, "Anthropic API Key");
    addAndMakeVisible (apiKeyLabel_);

    apiKeyEditor_.setTextToShowWhenEmpty ("sk-ant-...", juce::Colour (kMutedColour));
    apiKeyEditor_.setColour (juce::TextEditor::backgroundColourId, juce::Colour (kPanelColour));
    apiKeyEditor_.setColour (juce::TextEditor::textColourId, juce::Colour (kTextColour));
    apiKeyEditor_.setColour (juce::TextEditor::outlineColourId, juce::Colour (kBorderColour));
    apiKeyEditor_.setColour (juce::TextEditor::focusedOutlineColourId, juce::Colour (kAccentColour));
    apiKeyEditor_.setIndents (8, 5);
    apiKeyEditor_.setPasswordCharacter ((juce::juce_wchar) 0x2022);
    apiKeyEditor_.setText (UserPreferences::getInstance().getAnthropicApiKey(), juce::dontSendNotification);
    apiKeyEditor_.onFocusLost = [this]()
    {
        UserPreferences::getInstance().setAnthropicApiKey (apiKeyEditor_.getText().trim());
    };
    addAndMakeVisible (apiKeyEditor_);

    //--- Batch size ----------------------------------------------------------
    styleField (batchSizeLabel_, "Batch size");
    addAndMakeVisible (batchSizeLabel_);

    for (auto* b : { &batch100Button_, &batch250Button_, &batch500Button_, &batch1000Button_ })
        addAndMakeVisible (*b);
    batch100Button_.onClick  = [this]() { selectBatchSize (100); };
    batch250Button_.onClick  = [this]() { selectBatchSize (250); };
    batch500Button_.onClick  = [this]() { selectBatchSize (500); };
    batch1000Button_.onClick = [this]() { selectBatchSize (1000); };

    //--- Mode ------------------------------------------------------------------
    styleField (modeLabel_, "On error");
    addAndMakeVisible (modeLabel_);

    addAndMakeVisible (fixBrokenButton_);
    addAndMakeVisible (notFixButton_);
    fixBrokenButton_.onClick = [this]() { selectMode (true); };
    notFixButton_.onClick    = [this]() { selectMode (false); };

    //--- Run controls ------------------------------------------------------------
    runButton_.setColour (juce::TextButton::buttonColourId, juce::Colour (kAccentColour));
    runButton_.setColour (juce::TextButton::textColourOnId,  juce::Colour (0xff0d1527));
    runButton_.setColour (juce::TextButton::textColourOffId, juce::Colour (0xff0d1527));
    runButton_.onClick = [this]() { startRun(); };
    addAndMakeVisible (runButton_);

    cancelButton_.setColour (juce::TextButton::buttonColourId, juce::Colour (kDangerColour).withAlpha (0.85f));
    cancelButton_.setColour (juce::TextButton::textColourOnId,  juce::Colours::white);
    cancelButton_.setColour (juce::TextButton::textColourOffId, juce::Colours::white);
    cancelButton_.onClick = [this]() { cancelRun(); };
    cancelButton_.setVisible (false);
    addAndMakeVisible (cancelButton_);

    progressLabel_.setColour (juce::Label::textColourId, juce::Colour (kMutedColour));
    progressLabel_.setFont (juce::Font (juce::FontOptions().withHeight (13.0f)));
    addAndMakeVisible (progressLabel_);

    progressBar_.setColour (juce::ProgressBar::backgroundColourId, juce::Colour (kPanelColour));
    progressBar_.setColour (juce::ProgressBar::foregroundColourId, juce::Colour (kAccentColour));
    addAndMakeVisible (progressBar_);

    statusLabel_.setColour (juce::Label::textColourId, juce::Colour (kTextColour));
    statusLabel_.setFont (juce::Font (juce::FontOptions().withHeight (13.0f)));
    statusLabel_.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (statusLabel_);

    //--- Report view -------------------------------------------------------------
    reportSummaryLabel_.setColour (juce::Label::textColourId, juce::Colour (kTextColour));
    reportSummaryLabel_.setFont (juce::Font (juce::FontOptions().withHeight (15.0f)).boldened());
    addChildComponent (reportSummaryLabel_);

    reportModel_.rows = &failures_;
    reportModel_.onRowClicked = [this] (int rowNumber)
    {
        if (reviewMode_)
            openManualReviewFixDialog (rowNumber);
    };
    reportList_.setModel (&reportModel_);
    reportList_.setRowHeight (36);
    reportList_.setColour (juce::ListBox::backgroundColourId, juce::Colour (kPanelColour));
    addChildComponent (reportList_);

    backToToolButton_.setColour (juce::TextButton::buttonColourId, juce::Colour (kAccentColour));
    backToToolButton_.setColour (juce::TextButton::textColourOnId,  juce::Colour (0xff0d1527));
    backToToolButton_.setColour (juce::TextButton::textColourOffId, juce::Colour (0xff0d1527));
    backToToolButton_.onClick = [this]() { showRunView(); };
    addChildComponent (backToToolButton_);

    runNextBatchButton_.setColour (juce::TextButton::buttonColourId, juce::Colour (kAccentColour));
    runNextBatchButton_.setColour (juce::TextButton::textColourOnId,  juce::Colour (0xff0d1527));
    runNextBatchButton_.setColour (juce::TextButton::textColourOffId, juce::Colour (0xff0d1527));
    // Counts/quota were already refreshed at the end of the run that produced
    // this report (see finalizeRun()), so the next batch picks up wherever
    // missingDocIds_ now starts -- whatever just succeeded is excluded,
    // anything that failed stays in the pool and gets retried.
    runNextBatchButton_.onClick = [this]() { showRunView(); startRun(); };
    addChildComponent (runNextBatchButton_);

    updateBatchButtonStates();
    selectMode (false);
    updateRunButtonState();

    refreshCounts();
    refreshQuota();
}

BulkMetadataTool::~BulkMetadataTool() = default;

//==============================================================================
void BulkMetadataTool::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (juce::Colour (kBgColour));
    g.fillRoundedRectangle (r, 12.f);
    g.setColour (juce::Colour (kBorderColour));
    g.drawRoundedRectangle (r.reduced (0.5f), 12.f, 1.f);
}

void BulkMetadataTool::resized()
{
    auto area = getLocalBounds().reduced (20, 14);

    auto titleRow = area.removeFromTop (28);
    closeButton_.setBounds (titleRow.removeFromRight (28));
    titleLabel_.setBounds (titleRow);
    area.removeFromTop (12);

    if (showingReport_)
    {
        reportSummaryLabel_.setBounds (area.removeFromTop (26));
        area.removeFromTop (8);
        auto backRow = area.removeFromBottom (36);
        backToToolButton_.setBounds (backRow.removeFromRight (100));
        backRow.removeFromRight (10);
        runNextBatchButton_.setBounds (backRow.removeFromRight (160));
        area.removeFromBottom (10);
        reportList_.setBounds (area);
        return;
    }

    // Stats row
    {
        auto row = area.removeFromTop (30);
        refreshCountsButton_.setBounds (row.removeFromRight (140));
        row.removeFromRight (10);
        reviewButton_.setBounds (row.removeFromRight (140));
        row.removeFromRight (10);
        statsLabel_.setBounds (row);
    }
    area.removeFromTop (10);

    // Quota row
    {
        auto row = area.removeFromTop (30);
        refreshQuotaButton_.setBounds (row.removeFromRight (140));
        row.removeFromRight (10);
        quotaLabel_.setBounds (row);
    }
    area.removeFromTop (16);

    // AI cleanup
    {
        aiCleanupToggle_.setBounds (area.removeFromTop (26));
        area.removeFromTop (6);
        auto row = area.removeFromTop (28);
        apiKeyLabel_.setBounds (row.removeFromLeft (140));
        apiKeyEditor_.setBounds (row);
    }
    area.removeFromTop (16);

    // Batch size
    {
        batchSizeLabel_.setBounds (area.removeFromTop (16));
        auto row = area.removeFromTop (32);
        const int w = (row.getWidth() - 24) / 4;
        batch100Button_.setBounds  (row.removeFromLeft (w)); row.removeFromLeft (8);
        batch250Button_.setBounds  (row.removeFromLeft (w)); row.removeFromLeft (8);
        batch500Button_.setBounds  (row.removeFromLeft (w)); row.removeFromLeft (8);
        batch1000Button_.setBounds (row);
    }
    area.removeFromTop (16);

    // Mode
    {
        modeLabel_.setBounds (area.removeFromTop (16));
        auto row = area.removeFromTop (32);
        const int w = (row.getWidth() - 8) / 2;
        fixBrokenButton_.setBounds (row.removeFromLeft (w)); row.removeFromLeft (8);
        notFixButton_.setBounds (row);
    }
    area.removeFromTop (20);

    // Run row
    {
        auto row = area.removeFromTop (36);
        runButton_.setBounds (row.removeFromLeft (140));
        row.removeFromLeft (10);
        cancelButton_.setBounds (row.removeFromLeft (100));
    }
    area.removeFromTop (12);

    progressLabel_.setBounds (area.removeFromTop (18));
    progressBar_.setBounds (area.removeFromTop (16));
    area.removeFromTop (12);

    statusLabel_.setBounds (area.removeFromTop (60));
}

//==============================================================================
void BulkMetadataTool::showRunView()
{
    showingReport_ = false;
    reviewMode_ = false;
    reportSummaryLabel_.setVisible (false);
    reportList_.setVisible (false);
    backToToolButton_.setVisible (false);
    runNextBatchButton_.setVisible (false);

    for (juce::Component* c : { (juce::Component*) &statsLabel_, (juce::Component*) &refreshCountsButton_,
                                (juce::Component*) &reviewButton_,
                                (juce::Component*) &quotaLabel_, (juce::Component*) &refreshQuotaButton_,
                                (juce::Component*) &aiCleanupToggle_, (juce::Component*) &apiKeyLabel_,
                                (juce::Component*) &apiKeyEditor_, (juce::Component*) &batchSizeLabel_,
                                (juce::Component*) &batch100Button_, (juce::Component*) &batch250Button_,
                                (juce::Component*) &batch500Button_, (juce::Component*) &batch1000Button_,
                                (juce::Component*) &modeLabel_, (juce::Component*) &fixBrokenButton_,
                                (juce::Component*) &notFixButton_, (juce::Component*) &runButton_,
                                (juce::Component*) &progressLabel_, (juce::Component*) &progressBar_,
                                (juce::Component*) &statusLabel_ })
        c->setVisible (true);
    cancelButton_.setVisible (running_);

    resized();
}

void BulkMetadataTool::showReportView()
{
    showingReport_ = true;
    reviewMode_ = false;

    for (juce::Component* c : { (juce::Component*) &statsLabel_, (juce::Component*) &refreshCountsButton_,
                                (juce::Component*) &reviewButton_,
                                (juce::Component*) &quotaLabel_, (juce::Component*) &refreshQuotaButton_,
                                (juce::Component*) &aiCleanupToggle_, (juce::Component*) &apiKeyLabel_,
                                (juce::Component*) &apiKeyEditor_, (juce::Component*) &batchSizeLabel_,
                                (juce::Component*) &batch100Button_, (juce::Component*) &batch250Button_,
                                (juce::Component*) &batch500Button_, (juce::Component*) &batch1000Button_,
                                (juce::Component*) &modeLabel_, (juce::Component*) &fixBrokenButton_,
                                (juce::Component*) &notFixButton_, (juce::Component*) &runButton_,
                                (juce::Component*) &cancelButton_, (juce::Component*) &progressLabel_,
                                (juce::Component*) &progressBar_, (juce::Component*) &statusLabel_ })
        c->setVisible (false);

    reportSummaryLabel_.setText (
        juce::String (runSucceeded_) + " succeeded, " + juce::String (runFailed_) + " failed (of "
        + juce::String ((int) targetDocIds_.size()) + " attempted).", juce::dontSendNotification);

    reportSummaryLabel_.setVisible (true);
    reportList_.setVisible (true);
    backToToolButton_.setVisible (true);
    runNextBatchButton_.setVisible (countWithoutMetadata_ > 0 && (! quotaLoaded_ || quotaRemaining_ > 0));
    reportList_.updateContent();

    resized();
}

void BulkMetadataTool::showManualReviewList()
{
    auto* rootObj = catalogRoot_.getDynamicObject();
    if (rootObj == nullptr)
        return;

    failures_.clear();
    for (auto& docId : reviewDocIds_)
    {
        auto* entryObj = rootObj->getProperty (docId).getDynamicObject();
        if (entryObj == nullptr) continue;

        FailureRow row;
        row.docId      = docId;
        row.artistName = entryObj->getProperty ("artistName").toString();
        row.songName   = entryObj->getProperty ("songName").toString();
        row.error      = entryObj->getProperty ("lastError").toString();
        if (row.error.isEmpty())
            row.error = "Needs manual review.";
        failures_.push_back (row);
    }

    showingReport_ = true;
    reviewMode_ = true;

    for (juce::Component* c : { (juce::Component*) &statsLabel_, (juce::Component*) &refreshCountsButton_,
                                (juce::Component*) &reviewButton_,
                                (juce::Component*) &quotaLabel_, (juce::Component*) &refreshQuotaButton_,
                                (juce::Component*) &aiCleanupToggle_, (juce::Component*) &apiKeyLabel_,
                                (juce::Component*) &apiKeyEditor_, (juce::Component*) &batchSizeLabel_,
                                (juce::Component*) &batch100Button_, (juce::Component*) &batch250Button_,
                                (juce::Component*) &batch500Button_, (juce::Component*) &batch1000Button_,
                                (juce::Component*) &modeLabel_, (juce::Component*) &fixBrokenButton_,
                                (juce::Component*) &notFixButton_, (juce::Component*) &runButton_,
                                (juce::Component*) &cancelButton_, (juce::Component*) &progressLabel_,
                                (juce::Component*) &progressBar_, (juce::Component*) &statusLabel_ })
        c->setVisible (false);

    reportSummaryLabel_.setText (
        juce::String ((int) failures_.size()) + " song(s) need manual review -- click a row to fix and retry.",
        juce::dontSendNotification);

    reportSummaryLabel_.setVisible (true);
    reportList_.setVisible (true);
    backToToolButton_.setVisible (true);
    runNextBatchButton_.setVisible (false);
    reportList_.updateContent();

    resized();
}

void BulkMetadataTool::openManualReviewFixDialog (int rowNumber)
{
    if (rowNumber < 0 || rowNumber >= (int) failures_.size())
        return;

    const auto row = failures_[(size_t) rowNumber]; // copy -- failures_ may be rebuilt before the dialog closes

    CdgSong song;
    song.id = row.docId.toStdString();
    song.artistName = row.artistName.toStdString();
    song.songName = row.songName.toStdString();

    juce::Component::SafePointer<BulkMetadataTool> safe (this);
    const auto docId = row.docId;
    SongEditDialog::launch (this, song, {}, nullptr,
        [safe, docId, song] (const SongEditResult& r)
        {
            if (safe == nullptr) return;

            if (! r.isSave())
                return; // leave it flagged exactly as it was; back to the review list underneath

            const auto correctedArtist = juce::String (r.song.artistName).trim();
            const auto correctedSong   = juce::String (r.song.songName).trim();

            // Clear the flag before retrying -- beginAttempt()/handleLookupResult()
            // will re-flag it (with a fresh error) if this attempt fails again.
            auto* rootObj = safe->catalogRoot_.getDynamicObject();
            auto* entryObj = rootObj != nullptr ? rootObj->getProperty (docId).getDynamicObject() : nullptr;
            if (entryObj != nullptr)
            {
                entryObj->removeProperty ("needsManualReview");
                entryObj->removeProperty ("lastError");
            }

            // A single manual retry, outside the batch-run machinery -- reuses
            // beginAttempt()'s lookup logic but reports through a one-off
            // handler instead of processNext()/finalizeRun(), since there's no
            // batch in progress here.
            safe->setStatus ("Retrying: " + correctedArtist + " - " + correctedSong + "...");
            ApiService::getInstance().searchArtistAndSong (song, correctedArtist, correctedSong,
                [safe, docId, correctedArtist, correctedSong] (ApiService::Result result)
                {
                    if (safe == nullptr) return;

                    auto* rootObj2 = safe->catalogRoot_.getDynamicObject();
                    auto* entryObj2 = rootObj2 != nullptr ? rootObj2->getProperty (docId).getDynamicObject() : nullptr;

                    // refreshCounts() is async (background thread), so rather than
                    // wait on it to know whether this docId still belongs in the
                    // review list, update reviewDocIds_ here directly and
                    // redisplay immediately; refreshCounts() still runs below to
                    // keep the top-level stats label/button count honest.
                    auto removeFromReview = [safe, docId]()
                    {
                        auto& v = safe->reviewDocIds_;
                        v.erase (std::remove (v.begin(), v.end(), docId), v.end());
                    };

                    if (result.ok && result.song.hasMetadata())
                    {
                        if (entryObj2 != nullptr)
                            applyResultToEntry (entryObj2, result.song);
                        safe->saveCatalog();
                        safe->setStatus ("Fixed: " + correctedArtist + " - " + correctedSong);
                        removeFromReview();
                    }
                    else if (entryObj2 != nullptr)
                    {
                        const auto errorMsg = result.errorMessage.isNotEmpty()
                            ? result.errorMessage : juce::String ("Unknown error.");
                        if (looksTransient (errorMsg))
                        {
                            // Worth a normal retry later rather than staying
                            // parked here -- let it flow back into the regular
                            // missing-metadata pool on the next refresh/run.
                            removeFromReview();
                        }
                        else
                        {
                            entryObj2->setProperty ("needsManualReview", true);
                            entryObj2->setProperty ("lastError", errorMsg);
                        }
                        entryObj2->setProperty ("artistName", correctedArtist);
                        entryObj2->setProperty ("songName", correctedSong);
                        safe->saveCatalog();
                        safe->setStatus ("Still failing: " + errorMsg);
                    }

                    safe->showManualReviewList();
                    safe->refreshCounts();
                });
        });
}

//==============================================================================
void BulkMetadataTool::setStatus (const juce::String& msg)
{
    statusLabel_.setText (msg, juce::dontSendNotification);
}

//==============================================================================
juce::File BulkMetadataTool::catalogFile() const
{
    return LibraryScanner::getDefaultSongbookFile().getSiblingFile ("meta_data.json");
}

bool BulkMetadataTool::entryHasMetadata (juce::DynamicObject* obj)
{
    if (obj == nullptr) return false;

    // Mirrors CdgSong::hasMetadata() -- keySignature/tempo/genres are
    // intentionally NOT required: Spotify deprecated Audio Features for this
    // app in Nov 2024 (tempo/key can never come back from a live lookup
    // anymore), and genres is frequently genuinely blank on Spotify's own
    // side for smaller/tribute artists -- a real final answer, not a sign
    // the song still needs fetching.
    const auto imageUrl    = obj->getProperty ("imageUrl").toString();
    const auto releaseDate = obj->getProperty ("releaseDate").toString();
    const int  durationMS  = (int) obj->getProperty ("durationMS");

    return imageUrl.isNotEmpty() && releaseDate.isNotEmpty() && durationMS > 0;
}

// static
bool BulkMetadataTool::looksTransient (const juce::String& errorMessage)
{
    const auto msg = errorMessage.toLowerCase();
    return msg.contains ("429") || msg.contains ("too many") || msg.contains ("rate")
        || msg.contains ("timeout") || msg.contains ("timed out")
        || msg.contains ("could not connect") || msg.contains ("could not reach")
        || msg.contains ("temporarily") || msg.contains ("unavailable")
        || msg.contains ("503") || msg.contains ("quota");
}

CdgSong BulkMetadataTool::entryToCdgSong (const juce::String& docId, juce::DynamicObject* obj)
{
    CdgSong s = CdgSong::fromJsonObject (obj);
    if (s.id.empty())
        s.id = docId.toStdString();
    return s;
}

void BulkMetadataTool::applyResultToEntry (juce::DynamicObject* obj, const CdgSong& song)
{
    if (obj == nullptr) return;

    obj->setProperty ("artistName",   juce::String (song.artistName));
    obj->setProperty ("songName",     juce::String (song.songName));
    obj->setProperty ("imageUrl",     juce::String (song.imageUrl));
    obj->setProperty ("keySignature", juce::String (song.keySignature));
    obj->setProperty ("releaseDate",  juce::String (song.releaseDate));
    obj->setProperty ("durationMS",   song.durationMS);
    obj->setProperty ("tempo",        song.tempo);

    juce::Array<juce::var> genresArr;
    for (auto& g : song.genres)
        genresArr.add (juce::String (g));
    obj->setProperty ("genres", juce::var (genresArr));
}

bool BulkMetadataTool::loadCatalogIfNeeded (juce::String& outError)
{
    if (catalogLoaded_)
        return true;

    const auto file = catalogFile();
    if (! file.existsAsFile())
    {
        outError = "Catalog file not found (run a library scan on this machine at least once first).";
        return false;
    }

    catalogRoot_ = juce::JSON::parse (file.loadFileAsString());
    if (! catalogRoot_.isObject())
    {
        outError = "Catalog file could not be parsed.";
        return false;
    }

    catalogLoaded_ = true;
    return true;
}

bool BulkMetadataTool::saveCatalog()
{
    const auto file = catalogFile();
    return file.replaceWithText (juce::JSON::toString (catalogRoot_, true));
}

//==============================================================================
void BulkMetadataTool::refreshCounts()
{
    setStatus ("Scanning your library...");
    refreshCountsButton_.setEnabled (false);

    juce::Component::SafePointer<BulkMetadataTool> safe (this);
    juce::Thread::launch (juce::Thread::Priority::low, [safe]()
    {
        if (safe == nullptr) return;

        juce::String error;
        const bool ok = safe->loadCatalogIfNeeded (error);

        if (! ok)
        {
            juce::MessageManager::callAsync ([safe, error]()
            {
                if (safe == nullptr) return;
                safe->refreshCountsButton_.setEnabled (true);
                safe->statsLabel_.setText (error, juce::dontSendNotification);
                safe->setStatus (error);
            });
            return;
        }

        // Scope the count/target list to THIS venue's actual scanned library,
        // not the whole shared bootstrap catalog (~48.5k songs spanning every
        // venue that's ever contributed to it). Without this, "songs with
        // metadata" answers "how complete is the shared catalog" rather than
        // "how many of my songs have metadata" -- wildly inflating the number
        // a single venue sees, since most catalog rows belong to nobody's
        // local library here.
        SongDatabase db;
        std::vector<CdgSong> localSongs;
        if (db.open())
            localSongs = db.getAll();

        std::set<juce::String> localKeys;
        for (auto& s : localSongs)
            localKeys.insert (LibraryScanner::normaliseSongKey (
                juce::String (s.artistName), juce::String (s.songName)));

        int withMeta = 0, withoutMeta = 0, needsReview = 0;
        std::vector<juce::String> missingDocIds;
        std::vector<juce::String> reviewDocIds;
        std::set<juce::String> matchedKeys;

        auto* rootObj = safe->catalogRoot_.getDynamicObject();
        auto& props = rootObj->getProperties();
        for (int i = 0; i < props.size(); ++i)
        {
            auto* entryObj = props.getValueAt (i).getDynamicObject();
            if (entryObj == nullptr) continue;

            const auto key = LibraryScanner::normaliseSongKey (
                entryObj->getProperty ("artistName").toString(),
                entryObj->getProperty ("songName").toString());

            if (localKeys.find (key) == localKeys.end())
                continue; // catalog row not present in this venue's library -- not ours to count

            matchedKeys.insert (key);

            if (entryHasMetadata (entryObj))
                ++withMeta;
            else if ((bool) entryObj->getProperty ("needsManualReview"))
            {
                // Flagged by a previous failed attempt (see recordFailure()) --
                // excluded from the auto-retry pool so it doesn't keep getting
                // attempted every single run; surfaced instead via the
                // "Manual Review" list for the admin to fix or ignore.
                ++needsReview;
                reviewDocIds.push_back (props.getName (i).toString());
            }
            else
            {
                ++withoutMeta;
                missingDocIds.push_back (props.getName (i).toString());
            }
        }

        // Local songs with no catalog row at all definitely have no metadata.
        // Synthesize a bare catalog entry (artist/song only) for each so the
        // existing docId-keyed run/save machinery can fetch and persist their
        // metadata like any other catalog entry; only written to disk once a
        // run against it actually succeeds (see finalizeRun -- saveCatalog()
        // only runs when runSucceeded_ > 0).
        for (auto& s : localSongs)
        {
            const auto key = LibraryScanner::normaliseSongKey (
                juce::String (s.artistName), juce::String (s.songName));
            if (matchedKeys.find (key) != matchedKeys.end())
                continue;
            matchedKeys.insert (key); // dedupe multiple local files mapping to the same key

            const auto newDocId = "local-" + juce::Uuid().toString();
            juce::DynamicObject::Ptr newEntry = new juce::DynamicObject();
            newEntry->setProperty ("artistName", juce::String (s.artistName));
            newEntry->setProperty ("songName",   juce::String (s.songName));
            rootObj->setProperty (juce::Identifier (newDocId), juce::var (newEntry.get()));

            ++withoutMeta;
            missingDocIds.push_back (newDocId);
        }

        juce::MessageManager::callAsync ([safe, withMeta, withoutMeta, needsReview,
                                          missing = std::move (missingDocIds),
                                          review = std::move (reviewDocIds)]() mutable
        {
            if (safe == nullptr) return;

            safe->countWithMetadata_ = withMeta;
            safe->countWithoutMetadata_ = withoutMeta;
            safe->countNeedsReview_ = needsReview;
            safe->missingDocIds_ = std::move (missing);
            safe->reviewDocIds_ = std::move (review);

            const int total = withMeta + withoutMeta + needsReview;
            const int percent = total > 0 ? juce::roundToInt (100.0 * withMeta / total) : 0;
            juce::String text = juce::String (withMeta) + " of " + juce::String (total)
                + " (" + juce::String (percent) + "%) songs in your library have metadata";
            if (needsReview > 0)
                text += "; " + juce::String (needsReview) + " need manual review";
            safe->statsLabel_.setText (text, juce::dontSendNotification);

            safe->reviewButton_.setButtonText (needsReview > 0
                ? "Manual Review (" + juce::String (needsReview) + ")" : "Manual Review");
            safe->reviewButton_.setEnabled (needsReview > 0);

            safe->refreshCountsButton_.setEnabled (true);
            safe->setStatus ({});
            safe->updateBatchButtonStates();
            safe->updateRunButtonState();
        });
    });
}

void BulkMetadataTool::refreshQuota()
{
    refreshQuotaButton_.setEnabled (false);
    quotaLabel_.setText ("Loading quota...", juce::dontSendNotification);

    juce::Component::SafePointer<BulkMetadataTool> safe (this);
    MetadataQuotaService::getInstance().getStatus ([safe] (MetadataQuotaService::Status status)
    {
        if (safe == nullptr) return;

        safe->refreshQuotaButton_.setEnabled (true);

        if (! status.ok)
        {
            safe->quotaLabel_.setText ("Quota unavailable: " + status.errorMessage, juce::dontSendNotification);
            safe->quotaLoaded_ = false;
            safe->updateBatchButtonStates();
            safe->updateRunButtonState();
            return;
        }

        safe->quotaLoaded_ = true;
        safe->quotaRemaining_ = status.remaining;
        safe->quotaCap_ = status.cap;
        safe->quotaLabel_.setText (
            "Spotify quota today: used " + juce::String (status.usedCalls) + " / " + juce::String (status.cap)
            + " (" + juce::String (status.remaining) + " remaining)", juce::dontSendNotification);

        safe->updateBatchButtonStates();
        safe->updateRunButtonState();
    });
}

//==============================================================================
void BulkMetadataTool::selectBatchSize (int size)
{
    selectedBatchSize_ = size;
    updateBatchButtonStates();
    updateRunButtonState();
}

void BulkMetadataTool::updateBatchButtonStates()
{
    const int available = quotaLoaded_ ? juce::jmin (countWithoutMetadata_, quotaRemaining_) : countWithoutMetadata_;

    struct Entry { juce::TextButton* button; int size; };
    const Entry entries[] = {
        { &batch100Button_, 100 }, { &batch250Button_, 250 },
        { &batch500Button_, 500 }, { &batch1000Button_, 1000 }
    };

    for (auto& e : entries)
    {
        const bool enabled = available > 0; // still allow picking a size larger than what's left -- run() clamps it
        e.button->setEnabled (enabled);
        styleToggleButton (*e.button, e.size == selectedBatchSize_);
    }
}

void BulkMetadataTool::selectMode (bool fixBroken)
{
    fixBrokenMode_ = fixBroken;
    styleToggleButton (fixBrokenButton_, fixBrokenMode_);
    styleToggleButton (notFixButton_, ! fixBrokenMode_);
}

void BulkMetadataTool::updateRunButtonState()
{
    const bool haveWork = countWithoutMetadata_ > 0 && (! quotaLoaded_ || quotaRemaining_ > 0);
    runButton_.setEnabled (haveWork && ! running_);
}

//==============================================================================
void BulkMetadataTool::startRun()
{
    if (running_) return;

    juce::String error;
    if (! loadCatalogIfNeeded (error))
    {
        setStatus (error);
        return;
    }

    const int available = quotaLoaded_ ? juce::jmin (countWithoutMetadata_, quotaRemaining_) : countWithoutMetadata_;
    const int n = juce::jmin (selectedBatchSize_, available, (int) missingDocIds_.size());

    if (n <= 0)
    {
        setStatus ("Nothing to run (no songs missing metadata, or no quota remaining today).");
        return;
    }

    targetDocIds_.assign (missingDocIds_.begin(), missingDocIds_.begin() + n);
    runPosition_ = 0;
    runSucceeded_ = 0;
    runFailed_ = 0;
    failures_.clear();
    cancelRequested_ = false;
    running_ = true;

    runButton_.setEnabled (false);
    cancelButton_.setVisible (true);
    refreshCountsButton_.setEnabled (false);
    progressValue_ = 0.0;

    setStatus ("Running " + juce::String (n) + " song(s)...");
    processNext();
}

void BulkMetadataTool::cancelRun()
{
    cancelRequested_ = true;
    setStatus ("Cancelling after the current song...");
}

void BulkMetadataTool::processNext()
{
    if (cancelRequested_ || runPosition_ >= targetDocIds_.size())
    {
        finalizeRun();
        return;
    }

    const auto docId = targetDocIds_[runPosition_++];
    progressValue_ = (double) (runPosition_ - 1) / (double) targetDocIds_.size();
    progressLabel_.setText ("[" + juce::String ((int) runPosition_) + "/"
                            + juce::String ((int) targetDocIds_.size()) + "]", juce::dontSendNotification);

    beginAttempt (docId, false, {}, {});
}

void BulkMetadataTool::beginAttempt (const juce::String& docId, bool isManualRetry,
                                    const juce::String& forcedArtist, const juce::String& forcedSong)
{
    auto* rootObj = catalogRoot_.getDynamicObject();
    auto* entryObj = rootObj != nullptr ? rootObj->getProperty (docId).getDynamicObject() : nullptr;
    if (entryObj == nullptr)
    {
        recordFailure (docId, {}, {}, "Catalog entry vanished.");
        processNext();
        return;
    }

    const auto current = entryToCdgSong (docId, entryObj);
    const juce::String baseArtist = forcedArtist.isNotEmpty() ? forcedArtist : juce::String (current.artistName);
    const juce::String baseSong   = forcedSong.isNotEmpty()   ? forcedSong   : juce::String (current.songName);

    juce::Component::SafePointer<BulkMetadataTool> safe (this);

    auto runLookup = [safe, docId, current] (const juce::String& artist, const juce::String& song)
    {
        if (safe == nullptr) return;
        ApiService::getInstance().searchArtistAndSong (current, artist, song,
            [safe, docId, artist, song] (ApiService::Result result)
            {
                if (safe != nullptr)
                    safe->handleLookupResult (docId, artist, song, result);
            });
    };

    // AI cleanup only runs on the first attempt at a song -- once the admin
    // has manually corrected the spelling in Fix Broken mode, we retry with
    // exactly what they typed, no further AI involvement.
    if (! isManualRetry && aiCleanupToggle_.getToggleState())
    {
        AiSongNameCleanupService::getInstance().cleanup (baseArtist, baseSong,
            [safe, runLookup, baseArtist, baseSong] (AiSongNameCleanupService::Result cleanup)
            {
                if (safe == nullptr) return;
                const auto artist = cleanup.ok ? cleanup.artistName : baseArtist;
                const auto song   = cleanup.ok ? cleanup.songName   : baseSong;
                runLookup (artist, song);
            });
        return;
    }

    runLookup (baseArtist, baseSong);
}

void BulkMetadataTool::handleLookupResult (const juce::String& docId, const juce::String& searchArtist,
                                           const juce::String& searchSong, ApiService::Result result)
{
    if (result.ok && result.song.hasMetadata())
    {
        auto* rootObj = catalogRoot_.getDynamicObject();
        auto* entryObj = rootObj != nullptr ? rootObj->getProperty (docId).getDynamicObject() : nullptr;
        if (entryObj != nullptr)
            applyResultToEntry (entryObj, result.song);

        ++runSucceeded_;
        processNext();
        return;
    }

    // ApiService now only reports ok=true when the result actually has
    // complete metadata, so result.errorMessage is always populated here.
    const auto errorMsg = result.errorMessage.isNotEmpty() ? result.errorMessage : juce::String ("Unknown error.");

    if (fixBrokenMode_)
    {
        openFixDialog (docId, searchArtist, searchSong, errorMsg);
        return;
    }

    recordFailure (docId, searchArtist, searchSong, errorMsg);
    processNext();
}

void BulkMetadataTool::openFixDialog (const juce::String& docId, const juce::String& searchArtist,
                                      const juce::String& searchSong, const juce::String& errorMsg)
{
    setStatus ("Fix needed: " + searchArtist + " - " + searchSong);

    CdgSong song;
    song.id = docId.toStdString();
    song.artistName = searchArtist.toStdString();
    song.songName = searchSong.toStdString();

    juce::Component::SafePointer<BulkMetadataTool> safe (this);
    SongEditDialog::launch (this, song, {}, nullptr,
        [safe, docId] (const SongEditResult& r)
        {
            if (safe == nullptr) return;

            if (! r.isSave())
            {
                safe->recordFailure (docId, juce::String (r.song.artistName), juce::String (r.song.songName),
                                     "Skipped by admin.");
                safe->processNext();
                return;
            }

            const auto correctedArtist = juce::String (r.song.artistName).trim();
            const auto correctedSong   = juce::String (r.song.songName).trim();
            safe->beginAttempt (docId, true, correctedArtist, correctedSong);
        },
        "Spotify lookup failed for \"" + searchArtist + " - " + searchSong + "\": " + errorMsg
            + "\nCorrect the spelling below and Save to retry.");
}

void BulkMetadataTool::recordFailure (const juce::String& docId, const juce::String& artist,
                                      const juce::String& song, const juce::String& error)
{
    FailureRow row;
    row.docId = docId;
    row.artistName = artist;
    row.songName = song;
    row.error = error;
    failures_.push_back (row);
    ++runFailed_;

    // A rate-limit/timeout/quota-shaped error is worth a normal retry next
    // run (transient); anything else (bad catalog entry, genuinely no
    // Spotify match) will fail identically forever, so flag it for manual
    // review instead of re-attempting it every single run -- see
    // refreshCounts()'s use of "needsManualReview" to exclude these from
    // missingDocIds_.
    if (! looksTransient (error))
    {
        auto* rootObj = catalogRoot_.getDynamicObject();
        auto* entryObj = rootObj != nullptr ? rootObj->getProperty (docId).getDynamicObject() : nullptr;
        if (entryObj != nullptr)
        {
            entryObj->setProperty ("needsManualReview", true);
            entryObj->setProperty ("lastError", error);
            catalogDirty_ = true;
        }
    }
}

void BulkMetadataTool::finalizeRun()
{
    running_ = false;
    cancelButton_.setVisible (false);
    refreshCountsButton_.setEnabled (true);
    updateRunButtonState();

    if (runSucceeded_ > 0)
        catalogDirty_ = true;
    if (catalogDirty_)
    {
        saveCatalog();
        catalogDirty_ = false;
    }

    setStatus ("Done: " + juce::String (runSucceeded_) + " succeeded, "
              + juce::String (runFailed_) + " failed.");

    // Recount so the stats/quota reflect what just happened.
    refreshCounts();
    refreshQuota();

    showReportView();
}

//==============================================================================
void BulkMetadataTool::launch (juce::Component* parent)
{
    auto* content = new BulkMetadataTool();
    BorderlessModalWindow::launch (parent, content, juce::Colour (kBgColour));
}
