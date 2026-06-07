/*
  ==============================================================================

    MainComponent.cpp
    Created: 15 Apr 2026 7:04:12pm
    Author:  GitHub Copilot

    Main application component implementation

  ==============================================================================
*/

#include "MainComponent.h"
#include "BottomBar.h"
#include "../Services/WaveformGenerator.h"
#include "../Services/VenueService.h"
#include "../Services/QueueService.h"
#include "../Services/RequestService.h"
#include "../Services/HostService.h"
#include "../Services/SongDatabase.h"
#include "../Services/ImageCache.h"
#include "../Services/FirestoreClient.h"
#include "../Services/ArchiveService.h"
#include "../Services/ApiService.h"
#include "EditSingerModal.h"
#include <cmath>
#include <cstring>
#include <algorithm>
#include <condition_variable>
#include <mutex>
#include <numeric>
#include <random>

namespace
{
bool appendSongSync(const juce::String& venueId, const QueueItem& item, juce::String& errorOut)
{
    std::mutex mutex;
    std::condition_variable cv;
    bool done = false;
    bool ok = false;

    QueueService::getInstance().appendSong(venueId, item,
        [&](bool success, juce::String error)
        {
            std::lock_guard<std::mutex> lock(mutex);
            ok = success;
            errorOut = std::move(error);
            done = true;
            cv.notify_one();
        });

    std::unique_lock<std::mutex> lock(mutex);
    cv.wait(lock, [&]() { return done; });
    return ok;
}

bool createRequestedSong(const juce::String& venueId, const QueueItem& item, juce::String& errorOut)
{
    auto fields = FirestoreClient::makeFields({
        {"id",          FirestoreClient::stringValue(juce::String(item.id))},
        {"profileId",   FirestoreClient::stringValue(juce::String(item.profileId))},
        {"foxId",       FirestoreClient::stringValue(juce::String(item.foxId))},
        {"deviceId",    FirestoreClient::stringValue(juce::String(item.deviceId))},
        {"singerName",  FirestoreClient::stringValue(juce::String(item.singerName))},
        {"avatar",      FirestoreClient::stringValue(juce::String(item.singerAvatar))},
        {"song",        FirestoreClient::stringValue(juce::String(item.songName))},
        {"songId",      FirestoreClient::stringValue(juce::String(item.songId))},
        {"songVersion", FirestoreClient::stringValue(juce::String(item.songVersion))},
        {"artist",      FirestoreClient::stringValue(juce::String(item.songArtist))},
        {"duration",    FirestoreClient::integerValue(item.duration)},
        {"pitch",       FirestoreClient::doubleValue(item.pitch)},
        {"status",      FirestoreClient::stringValue(juce::String(item.status))},
        {"order",       FirestoreClient::integerValue(item.order)},
        {"songOrder",   FirestoreClient::integerValue(item.songOrder)},
        {"time",        FirestoreClient::stringValue(juce::String(item.time))},
        {"reason",      FirestoreClient::stringValue(juce::String(item.reason))},
        {"action",      FirestoreClient::stringValue(juce::String(item.action))},
        {"addedAlert",  FirestoreClient::booleanValue(false)},
        {"singingAlert",FirestoreClient::booleanValue(false)},
        {"nextAlert",   FirestoreClient::booleanValue(false)}
    });

    auto resp = FirestoreClient::getInstance().createDocument("venues/" + venueId + "/requested", fields);
    const bool ok = resp.isObject();
    if (!ok)
        errorOut = "createDocument failed";
    return ok;
}

int randomPitchSemitones(std::mt19937& rng, bool enabled)
{
    if (!enabled)
        return 0;
    std::uniform_int_distribution<int> dist(-6, 6);
    return dist(rng);
}

int findHostIndexInQueue(const std::vector<Singers>& singers)
{
    for (int i = 0; i < (int) singers.size(); ++i)
        if (singers[(size_t) i].isHost)
            return i;
    return -1;
}

void reindexQueueWithHostRoundAnchor(std::vector<Singers>& singers)
{
    for (int i = 0; i < (int) singers.size(); ++i)
        singers[(size_t) i].order = i;

    const int hostIndex = findHostIndexInQueue(singers);
    if (hostIndex < 0)
    {
        int rot = 0;
        for (int i = 0; i < (int) singers.size(); ++i)
            singers[(size_t) i].rotationOrder = rot++;
        return;
    }

    singers[(size_t) hostIndex].rotationOrder = -1;

    int rot = 0;
    const int n = (int) singers.size();
    for (int step = 1; step < n; ++step)
    {
        const int i = (hostIndex + step) % n;
        if (! singers[(size_t) i].isHost)
            singers[(size_t) i].rotationOrder = rot++;
    }
}
}

//==============================================================================
// Simple semi-transparent overlay shown while a song is loading. It paints a
// dim backdrop, a rounded card and a label, plus an indeterminate activity
// indicator. It intercepts all mouse input so the user can't fire another
// Play Now while the first one is still resolving.
//==============================================================================
class MainComponent::LoadingOverlay : public juce::Component,
                                      private juce::Timer
{
public:
    LoadingOverlay()
    {
        setInterceptsMouseClicks(true, false);
        setAlwaysOnTop(true);
        startTimerHz(30);
    }

    void setState(const juce::String& msg, double p)
    {
        message_ = msg;
        progress_ = p;
        repaint();
    }

    void paint(juce::Graphics& g) override
    {
        // Dimmed backdrop
        g.fillAll(juce::Colour(0xcc000000));

        // Centred card
        const int cardW = 380;
        const int cardH = 150;
        auto card = juce::Rectangle<int>(cardW, cardH)
                        .withCentre(getLocalBounds().getCentre());

        g.setColour(juce::Colour(0xff1a2030));
        g.fillRoundedRectangle(card.toFloat(), 10.0f);
        g.setColour(juce::Colour(0xff30daff));
        g.drawRoundedRectangle(card.toFloat().reduced(0.5f), 10.0f, 1.5f);

        // Spinner
        auto spinnerBox = card.removeFromLeft(56).reduced(12).toFloat();
        drawSpinner(g, spinnerBox);

        // Message
        g.setColour(juce::Colours::white);
        g.setFont(juce::Font(juce::FontOptions().withHeight(18.0f)).boldened());
        auto textArea = card.reduced(12, 8);
        g.drawFittedText(message_, textArea.removeFromTop(52), juce::Justification::centredLeft, 2);

        if (progress_ >= 0.0)
        {
            auto barArea = textArea.removeFromTop(16).reduced(0, 2);
            g.setColour(juce::Colour(0xff0d1527));
            g.fillRoundedRectangle(barArea.toFloat(), 5.0f);

            auto fill = barArea.withWidth((int) std::round(barArea.getWidth() * juce::jlimit(0.0, 1.0, progress_)));
            g.setColour(juce::Colour(0xff30daff));
            g.fillRoundedRectangle(fill.toFloat(), 5.0f);

            g.setColour(juce::Colour(0xffb9c3d5));
            g.setFont(juce::Font(juce::FontOptions().withHeight(12.0f)));
            g.drawText(juce::String((int) std::round(progress_ * 100.0)) + "%",
                       textArea.removeFromTop(18), juce::Justification::centredLeft);
        }
    }

private:
    void timerCallback() override
    {
        phase_ += 0.12f;
        if (phase_ > juce::MathConstants<float>::twoPi)
            phase_ -= juce::MathConstants<float>::twoPi;
        repaint();
    }

    void drawSpinner(juce::Graphics& g, juce::Rectangle<float> r) const
    {
        const auto centre = r.getCentre();
        const float radius = juce::jmin(r.getWidth(), r.getHeight()) * 0.5f - 2.0f;
        const int segments = 12;
        for (int i = 0; i < segments; ++i)
        {
            float angle = phase_ + (juce::MathConstants<float>::twoPi * i) / (float) segments;
            float alpha = 0.15f + 0.85f * ((float) i / (float) segments);
            g.setColour(juce::Colour(0xff30daff).withAlpha(alpha));
            juce::Point<float> p1(centre.x + std::cos(angle) * radius * 0.55f,
                                  centre.y + std::sin(angle) * radius * 0.55f);
            juce::Point<float> p2(centre.x + std::cos(angle) * radius,
                                  centre.y + std::sin(angle) * radius);
            g.drawLine({ p1, p2 }, 2.6f);
        }
    }

    juce::String message_ { "Loading song..." };
    double progress_ = -1.0;
    float phase_ = 0.0f;
};

//==============================================================================
MainComponent::MainComponent()
{
    const auto ctorStartMs = juce::Time::getMillisecondCounterHiRes();
    audioStartupInProgress_ = false;
    audioStartupComplete_ = false;

    // Set initial size (will be adjusted by responsive system)
    setSize(1200, 800);

    // Create the audio engine object, but defer actual device initialisation
    // so a slow driver/device probe does not block first paint.
    audioEngine = std::make_unique<AudioEngine>();
    DBG("[Startup] AudioEngine object created: "
        + juce::String(juce::Time::getMillisecondCounterHiRes() - ctorStartMs, 1) + " ms");

    try
    {
        // Setup UI components carefully
        setupUI();
        
        // Force initial layout after components are created
        resized();
        
        // Ensure components are visible and painted
        repaint();
        
        // Start timer for periodic updates - disabled until safer implementation
        // startTimer(2000);
        
        // Initial screen configuration - simplified
        // detectAndConfigureScreens(); // Commenting out temporarily
        
        DBG("MainComponent initialized successfully");
        DBG("[Startup] MainComponent total ctor: "
            + juce::String(juce::Time::getMillisecondCounterHiRes() - ctorStartMs, 1) + " ms");
    }
    catch (const std::exception& e)
    {
        DBG("Error in MainComponent constructor: " + juce::String(e.what()));
        // Fallback: create a simple label
        titleLabel = std::make_unique<juce::Label>("title", "Encore Karaoke"); 
        titleLabel->setFont(juce::Font(juce::FontOptions().withHeight(28.0f)).boldened());
        titleLabel->setJustificationType(juce::Justification::centred);
        addAndMakeVisible(titleLabel.get());
    }
}

MainComponent::~MainComponent()
{
    stopTimer();
    // Close the secondary display before the audio engine goes away — its
    // timer may be trying to poll the engine's position.
    lyricWindow_.reset();
    if (audioEngine) audioEngine->shutdown();
}

void MainComponent::setupUI()
{
    // Setup UI with LocalizationManager integration
    DBG("Starting setupUI with LocalizationManager");
    
    // Create TopBar first
    topBar = std::make_unique<TopBar>();
    addAndMakeVisible(topBar.get());

    // Create BottomBar (music transport and waveform controls)
    bottomBar = std::make_unique<BottomBar>();
    addAndMakeVisible(bottomBar.get());
    
    // Set up TopBar callbacks
    topBar->onLogoClicked = [this]() {
        DBG("Encore logo clicked");
        // TODO: Implement logo click action (about dialog, etc.)
    };
    
    topBar->onUserButtonClicked = [this]() {
        DBG("User button clicked");
        // TODO: Implement user menu/account access
    };
    
    topBar->onHeightChanged = [this](int newHeight) {
        DBG("TopBar height changed to: " + juce::String(newHeight));
        resized(); // Re-layout when TopBar height changes
    };

    bottomBar->onHeightChanged = [this](int newHeight) {
        DBG("BottomBar height changed to: " + juce::String(newHeight));
        resized(); // Re-layout when BottomBar height changes
    };
    
    // Set initial TopBar identity from signed-in host profile.
    topBar->setOnlineStatus(true); // Start with online status
    applyCurrentIdentityToUi();

    // Shared transport-start helper.
    // allowQueueFallback=true keeps the existing bottom-bar behavior
    // (load next singer when nothing is loaded). Now-playing uses false.
    auto startTransportPlayback = [this](bool allowQueueFallback)
    {
        if (! audioEngine)
            return;

        if (! audioEngine->isInitialized())
            audioEngine->initialize();

        if (! audioEngine->isInitialized())
        {
            if (bottomBar != nullptr)
                bottomBar->setPlaying(false);
            if (queueBar != nullptr)
                queueBar->setPlaying(false);
            updateAudioStatusIndicator();
            return;
        }

        const bool hasLoadedMedia = (currentSongDuration > 0.01)
                                 || (audioEngine->getTotalLength() > 0.01);
        const bool videoLoaded = (lyricWindow_ != nullptr && lyricWindow_->isVideoActive());

        if (! hasLoadedMedia && ! videoLoaded)
        {
            if (allowQueueFallback)
            {
                // First press with no loaded media should preload only.
                const bool queued = queueAndLoadNextSingerSong(false);

                if (bottomBar != nullptr)
                    bottomBar->setPlaying(false);
                if (queueBar != nullptr)
                    queueBar->setPlaying(false);

                juce::ignoreUnused(queued);
            }
            else
            {
                if (bottomBar != nullptr)
                    bottomBar->setPlaying(false);
                if (queueBar != nullptr)
                    queueBar->setPlaying(false);
            }
            return;
        }

        if (playStartTimeMs_ == 0)
            playStartTimeMs_ = juce::Time::currentTimeMillis();

        if (videoLoaded)
            lyricWindow_->playVideo();
        else
            audioEngine->play();

        if (lyricWindow_ != nullptr)
            lyricWindow_->setForceIdleScreen(false);

        if (bottomBar != nullptr)
            bottomBar->setPlaying(true);
        if (queueBar != nullptr)
            queueBar->setPlaying(true);
    };

    // BottomBar callbacks — drive the AudioEngine
    bottomBar->onReturnToZero = [this]() {
        if (lyricWindow_ != nullptr && lyricWindow_->isVideoActive())
            lyricWindow_->seekVideo(0.0);
        else if (audioEngine)
            audioEngine->seekToPosition(0.0);
        bottomBar->setProgress(0.0f);
    };

    bottomBar->onStopAndReturnToZero = [this]() {
        logPlayHistoryIfNeeded(false); // logs only if played > 30 s
        if (lyricWindow_ != nullptr && lyricWindow_->isVideoActive())
        {
            lyricWindow_->pauseVideo();
            lyricWindow_->seekVideo(0.0);
        }
        else if (audioEngine)
            audioEngine->stop();
        bottomBar->setProgress(0.0f);
        bottomBar->setPlaying(false);
        if (queueBar != nullptr)
            queueBar->setPlaying(false);
    };

    bottomBar->onPlayPause = [this, startTransportPlayback](bool isNowPlaying) {
        if (! audioEngine) return;

        if (isNowPlaying)
        {
            startTransportPlayback(true);
        }
        else
        {
            if (lyricWindow_ != nullptr && lyricWindow_->isVideoActive())
                lyricWindow_->pauseVideo();
            else
                audioEngine->pause();
            if (queueBar != nullptr)
                queueBar->setPlaying(false);
        }
    };

    bottomBar->onJumpToEnd = [this]() {
        if (lyricWindow_ != nullptr && lyricWindow_->isVideoActive() && lyricWindow_->getVideoDuration() > 0.25)
            lyricWindow_->seekVideo(lyricWindow_->getVideoDuration() - 0.25);
        else if (audioEngine && audioEngine->getTotalLength() > 0.25)
            audioEngine->seekToPosition(audioEngine->getTotalLength() - 0.25);
    };

    bottomBar->onSeek = [this](float newProgress) {
        if (! audioEngine && (lyricWindow_ == nullptr || ! lyricWindow_->isVideoActive())) return;
        double total = (lyricWindow_ != nullptr && lyricWindow_->isVideoActive())
            ? lyricWindow_->getVideoDuration()
            : audioEngine->getTotalLength();
        if (total > 0.0)
        {
            const auto pos = total * (double) juce::jlimit(0.0f, 1.0f, newProgress);
            if (lyricWindow_ != nullptr && lyricWindow_->isVideoActive())
                lyricWindow_->seekVideo(pos);
            else
                audioEngine->seekToPosition(pos);
        }
    };

    bottomBar->onPitchChanged = [this](int semitones) {
        if (audioEngine) audioEngine->setPitchShift((float) semitones);
    };

    bottomBar->onVolumeChanged = [this](int volumeStep) {
        if (! audioEngine) return;
        // volumeStep is 0..10 in the slider -> map to 0..1.
        float v = juce::jlimit(0.0f, 1.0f, (float) volumeStep / 10.0f);
        audioEngine->setMasterVolume(v);
    };

    // The BottomBar's own 30Hz timer will no longer auto-advance progress —
    // our 100ms timer below polls the real AudioEngine position instead.
    bottomBar->setExternalProgressControl(true);

    // Start timer to poll audio engine + update VU / progress UI
    startTimer(50);
    
    DBG("TopBar created and configured");
    
    // Create NavBar (left navigation)
    navBar = std::make_unique<NavBar>();
    addAndMakeVisible(navBar.get());

    // Create MainArea (central content area)
    mainArea = std::make_unique<MainArea>();
    mainArea->setAudioEngine(audioEngine.get());
    addAndMakeVisible(mainArea.get());
    wireTestingPageCallbacks();

    // Wire NavBar page selection to MainArea
    navBar->onPageSelected = [this](NavPage page) {
        DBG("NavBar: page selected -> " + juce::String(static_cast<int>(page)));
        mainArea->setCurrentPage(page);
    };

    // Handle Song Selection dialog result from Home / Search
    mainArea->onSongSelectionResult = [this](const SongSelectionResult& r)
    {
        if (r.action == SongSelectionResult::Action::PlayNow)
        {
            loadAndPlaySong(r.song, r.versionIndex, r.pitchSemitones);
            return;
        }

        if (r.action == SongSelectionResult::Action::Cancelled)
            return;

        // AddToQueue and PlayNext both create a QueueItem and call appendSong.
        const juce::String venueId = activeVenueId_;
        if (venueId.isEmpty())
        {
            DBG ("[SongSelect] cannot add to queue: no active venue");
            juce::AlertWindow::showMessageBoxAsync(
                juce::MessageBoxIconType::WarningIcon,
                "No Active Venue",
                "Please select a venue from Settings before adding songs to the queue.");
            return;
        }

        // Default singer name when the field is left blank.
        const juce::String singerName = r.singerName.isNotEmpty()
                                            ? r.singerName
                                            : "Unknown";

        // Pick the selected version label, falling back to the first version
        // or an empty string if the song has no version info.
        juce::String versionLabel;
        if (r.versionIndex < (int) r.song.version.size())
            versionLabel = juce::String(r.song.version[(size_t) r.versionIndex]);
        else if (!r.song.version.empty())
            versionLabel = juce::String(r.song.version[0]);

        // Build the QueueItem from the dialog result.  For KJ-added singers
        // we deliberately leave profileId empty — using the host's own UID
        // would cause a 409 Conflict on the second add, since Firestore won't
        // create two docs at the same path.  Mobile requests, in contrast,
        // carry the singer's real auth UID in profileId.
        QueueItem item;
        item.id          = juce::Uuid().toString().toStdString();
        item.deviceId    = "local";
        item.profileId   = "";   // empty -> Firestore auto-generates doc ID
        item.singerName  = singerName.toStdString();
        item.songId      = r.song.id;
        item.songName    = r.song.songName;
        item.songArtist  = r.song.artistName;
        item.songVersion = versionLabel.toStdString();
        item.duration    = r.song.durationMS / 1000;
        item.pitch       = 1.0f + (float) r.pitchSemitones / 12.0f;
        item.key         = r.pitchSemitones;
        item.status      = "queued";
        item.dateAdded   = juce::Time::getCurrentTime().toMilliseconds();

        const bool playNext = (r.action == SongSelectionResult::Action::PlayNext);

        DBG ("[SongSelect] " << (playNext ? "PlayNext" : "AddToQueue")
             << " singer='" << singerName << "'"
             << " song='" << juce::String(item.songName) << "'");

        juce::Component::SafePointer<MainComponent> safe (this);
        QueueService::getInstance().appendSong (venueId, item,
            [safe, venueId, singerName, playNext](bool ok, juce::String err)
            {
                if (!ok)
                {
                    DBG ("[SongSelect] appendSong FAILED: " << err);
                    juce::AlertWindow::showMessageBoxAsync(
                        juce::MessageBoxIconType::WarningIcon,
                        "Could Not Add Song",
                        "Failed to add the song to the queue: " + err);
                    return;
                }

                DBG ("[SongSelect] appendSong OK for '" << singerName << "'");

                if (playNext)
                {
                    // Move the newly-added singer to the front of the rotation
                    // (position 1, right after the pinned host at 0).  We do
                    // this by bumping every non-host singer's order up by 1,
                    // then setting the new singer's order to 1.
                    juce::Thread::launch([venueId, singerName, safe]()
                    {
                        const auto collPath = "venues/" + venueId + "/queue";
                        auto docs = FirestoreClient::getInstance().listCollection(collPath, 200);

                        for (auto& d : docs)
                        {
                            auto fields   = d.getProperty("fields", juce::var());
                            auto nameVar  = fields.getProperty("name", juce::var());
                            juce::String docName = d.getProperty("name", "").toString();
                            juce::String rel = docName.fromLastOccurrenceOf("/documents/", false, false);
                            if (rel.isEmpty()) continue;

                            // Parse name and order
                            auto nameVal = nameVar.hasProperty("stringValue")
                                               ? nameVar.getProperty("stringValue", "").toString()
                                               : juce::String();
                            auto orderVar = fields.getProperty("order", juce::var());
                            int currentOrder = orderVar.hasProperty("integerValue")
                                                   ? (int) orderVar.getProperty("integerValue", "0")
                                                         .toString().getLargeIntValue()
                                                   : 0;

                            // Skip the host (order 0 or isHost flag)
                            auto isHostVar = fields.getProperty("isHost", juce::var());
                            bool isHost = isHostVar.hasProperty("booleanValue")
                                              && (bool) isHostVar.getProperty("booleanValue", false);
                            if (isHost || currentOrder == 0)
                                continue;

                            int newOrder = (nameVal.equalsIgnoreCase(singerName)) ? 1
                                                                                  : currentOrder + 1;

                            juce::DynamicObject::Ptr f = new juce::DynamicObject();
                            f->setProperty("order", FirestoreClient::integerValue(newOrder));
                            FirestoreClient::getInstance()
                                .patchDocument(rel + "?updateMask.fieldPaths=order",
                                               juce::var(f.get()));
                        }

                        // Reload on the message thread once all patches are done.
                        juce::MessageManager::callAsync([venueId, safe]() mutable
                        {
                            if (safe != nullptr)
                                safe->reloadQueueFromFirestore(venueId);
                        });
                    });
                }
                else
                {
                    safe->reloadQueueFromFirestore(venueId);
                }
            });
    };

    // Persist Song-Edit dialog results.  Save also patches the venue
    // playlists (New / Popular / Recommended) in Firestore based on which
    // checkboxes the user actually changed, then refreshes the membership
    // cache + the home-page rows.  Delete additionally removes the song
    // from every venue playlist it appeared in.
    mainArea->onSongEditResult = [this](const SongEditResult& r)
    {
        auto* lib = mainArea ? mainArea->getLibraryPage() : nullptr;
        if (lib == nullptr)
        {
            DBG ("[SongEdit] no LibraryPage available — skipping persist");
            return;
        }

        auto& v = VenueService::getInstance();

        if (r.isSave())
        {
            const bool changed = lib->upsertSong(r.song);
            DBG ("[SongEdit] SAVE id='" << juce::String(r.song.id)
                 << "' title='" << juce::String(r.song.songName)
                 << "' artist='" << juce::String(r.song.artistName)
                 << "' changed=" << (int) changed
                 << " playlists: new="    << (int) r.addToNew
                 << " popular="           << (int) r.addToPopular
                 << " recommended="       << (int) r.addToRecommended);

            // Mirror the saved record into the shared metadata cache so the
            // next "Get Metadata" lookup serves the user's edits without
            // hitting the cloud function again.
            juce::Thread::launch([song = r.song]() {
                ApiService::getInstance().saveSharedMetadata(song);
            });

            // Patch venue playlists for any toggle that actually changed.
            const auto songId = juce::String(r.song.id);

            if (r.newChanged)
            {
                if (r.addToNew) v.addSongToNewSongs(r.song);
                else            v.deleteSongFromNewSongs(songId);
            }
            // Existing entries in "new" should also pick up updated metadata.
            else if (newSongIds_.count(r.song.id) > 0)
            {
                v.updateSongInNewSongs(r.song);
            }

            if (r.popularChanged)
            {
                if (r.addToPopular)
                    v.addSongToLists(r.song, juce::StringArray { "Popular" });
                else
                    v.deleteSongFromPlaylist("Popular", songId);
            }

            if (r.recommendedChanged)
            {
                if (r.addToRecommended)
                    v.addSongToLists(r.song, juce::StringArray { "Recommended" });
                else
                    v.deleteSongFromPlaylist("Recommended", songId);
            }

            // Refresh the membership cache + Home-page rows after a short
            // delay so the writes have time to land in Firestore.
            juce::Component::SafePointer<MainComponent> safe (this);
            juce::Timer::callAfterDelay(800, [safe]() {
                if (safe != nullptr) safe->loadVenuePlaylists();
            });
        }
        else if (r.isDelete())
        {
            const bool removed = lib->deleteSong(r.song);
            DBG ("[SongEdit] DELETE id='" << juce::String(r.song.id)
                 << "' title='" << juce::String(r.song.songName)
                 << "' removed=" << (int) removed);

            // Best-effort: drop the song from every venue playlist it was in.
            const auto songId = juce::String(r.song.id);
            v.deleteSongFromNewSongs    (songId);
            v.deleteSongFromPlaylist    ("Popular",     songId);
            v.deleteSongFromPlaylist    ("Recommended", songId);

            juce::Component::SafePointer<MainComponent> safe (this);
            juce::Timer::callAfterDelay(800, [safe]() {
                if (safe != nullptr) safe->loadVenuePlaylists();
            });
        }
    };

    // Initial playlist membership for the Edit dialog — reads the cached
    // sets that loadVenuePlaylists() refreshes from Firestore.
    mainArea->onSongEditPlaylistQuery = [this](const CdgSong& song,
                                               SongEditDialog::InitialPlaylists& out)
    {
        out.inNew         = newSongIds_        .count(song.id) > 0;
        out.inPopular     = popularSongIds_    .count(song.id) > 0;
        out.inRecommended = recommendedSongIds_.count(song.id) > 0;
    };

    // Spotify-style metadata lookup via the TAGG cloud function.
    // ApiService handles caching to shared_metadata.json on success.
    mainArea->onSongEditFetchMetadata =
        [](juce::String artist, juce::String song,
           std::function<void(bool ok, CdgSong updated, juce::String message)> done)
        {
            CdgSong stub;
            stub.artistName = artist.toStdString();
            stub.songName   = song.toStdString();
            ApiService::getInstance().searchArtistAndSong(stub, artist, song,
                [done](ApiService::Result r)
                {
                    if (! done) return;
                    juce::String msg;
                    if (r.ok)
                    {
                        switch (r.source)
                        {
                            case ApiService::Result::Source::localCache:
                                msg = "Loaded from local cache.";
                                break;
                            case ApiService::Result::Source::firestore:
                                msg = "Loaded from shared Firestore metadata.";
                                break;
                            case ApiService::Result::Source::legacyApi:
                                msg = "Updated from metadata API.";
                                break;
                            default:
                                msg = "Metadata updated.";
                                break;
                        }
                    }
                    else
                    {
                        msg = r.errorMessage;
                        if (r.queued)
                            msg << " Request queued for background metadata processing.";
                    }
                    done(r.ok, r.song, msg);
                });
        };

    // Push settings-page edits back to Firestore. On success the queue bar
    // and lyric display also pick up any name / code changes.
    mainArea->onVenueSettingsChanged = [this](const VenueItem& updated)
    {
        if (activeVenueId_.isEmpty())
            return;

        juce::Component::SafePointer<MainComponent> safe (this);
        const auto venueId = activeVenueId_;
        VenueService::getInstance().saveVenue (venueId, updated,
            [safe, updated](bool ok, juce::String error)
            {
                if (safe == nullptr) return;
                if (! ok)
                {
                    DBG ("[Venue] save failed: " << error);
                    return;
                }

                if (safe->queueBar != nullptr)
                    safe->queueBar->setVenueInfo (juce::String(updated.name),
                                                  juce::String(updated.code));
                if (safe->lyricWindow_ != nullptr)
                    if (auto* d = safe->lyricWindow_->getDisplay())
                        d->setVenueCode (juce::String(updated.code));
            });
    };

    // Wire the "End Session & Archive" button to ArchiveService. The button
    // also runs as part of the nightly cleanup timer started in setVenueId.
    if (auto* sp = mainArea->getSettingsPage())
    {
        auto normalizeRoleForFirestore = [](const juce::String& roleLabel)
        {
            auto role = roleLabel.trim();
            if (role.isEmpty())
                return juce::String("Basic");

            if (role.equalsIgnoreCase("Host"))
                return juce::String("Host");
            if (role.equalsIgnoreCase("Admin"))
                return juce::String("Admin");
            if (role.equalsIgnoreCase("Tester"))
                return juce::String("Tester");
            if (role.equalsIgnoreCase("EnterpriseAdmin") || role.equalsIgnoreCase("Enterprise Admin"))
                return juce::String("EnterpriseAdmin");
            if (role.equalsIgnoreCase("Basic") || role.equalsIgnoreCase("Basic User"))
                return juce::String("Basic");

            return juce::String("Basic");
        };

        auto isPrivilegedRole = [](const juce::String& role)
        {
            return role.equalsIgnoreCase("Admin")
                || role.equalsIgnoreCase("Tester")
                || role.equalsIgnoreCase("EnterpriseAdmin")
                || role.equalsIgnoreCase("Enterprise Admin");
        };

        struct AssociationLookupResult
        {
            juce::String path;
            juce::String role;
            juce::String status;
        };

        auto findAssociationByEmail = [this](const juce::String& rawEmail) -> AssociationLookupResult
        {
            AssociationLookupResult out;
            if (activeVenueId_.isEmpty())
                return out;

            const auto targetEmail = rawEmail.trim().toLowerCase();
            auto& fc = FirestoreClient::getInstance();
            auto docs = fc.listCollection("user-venue-lookup", 1000);
            for (auto& d : docs)
            {
                if (FirestoreClient::readString(d, "venueId") != activeVenueId_)
                    continue;
                if (FirestoreClient::readString(d, "userEmail").toLowerCase() != targetEmail)
                    continue;

                const auto fullName = d.getProperty("name", juce::var()).toString();
                const auto marker = "/documents/";
                const auto idx = fullName.indexOf(marker);
                if (idx < 0)
                    continue;

                out.path = fullName.substring(idx + (int) std::strlen(marker));
                out.role = FirestoreClient::readString(d, "role");
                out.status = FirestoreClient::readString(d, "status");
                return out;
            }
            return out;
        };

        auto countActivePrivilegedUsers = [this, isPrivilegedRole]() -> int
        {
            if (activeVenueId_.isEmpty())
                return 0;

            int count = 0;
            auto docs = FirestoreClient::getInstance().listCollection("user-venue-lookup", 1000);
            for (auto& d : docs)
            {
                if (FirestoreClient::readString(d, "venueId") != activeVenueId_)
                    continue;
                if (! FirestoreClient::readString(d, "status").equalsIgnoreCase("active"))
                    continue;
                if (! isPrivilegedRole(FirestoreClient::readString(d, "role")))
                    continue;
                ++count;
            }
            return count;
        };

        sp->onSetVenueCode = [this](const juce::String& code)
        {
            if (activeVenueId_.isEmpty())
                return;

            juce::Component::SafePointer<MainComponent> safe(this);
            const auto venueId = activeVenueId_;
            VenueService::getInstance().updateVenueCode(venueId, code,
                [safe, venueId](bool ok, juce::String err)
                {
                    if (safe == nullptr)
                        return;
                    if (! ok)
                    {
                        DBG("[Settings] set venue code failed: " << err);
                        return;
                    }

                    VenueService::getInstance().loadVenue(venueId,
                        [safe](bool loaded, VenueItem v, juce::String loadErr)
                        {
                            if (safe == nullptr) return;
                            if (! loaded)
                            {
                                DBG("[Settings] reload venue after set code failed: " << loadErr);
                                return;
                            }
                            if (safe->mainArea != nullptr)
                                safe->mainArea->setVenueData(v);
                            if (safe->queueBar != nullptr)
                                safe->queueBar->setVenueInfo(juce::String(v.name), juce::String(v.code));
                            if (safe->lyricWindow_ != nullptr)
                                if (auto* d = safe->lyricWindow_->getDisplay())
                                    d->setVenueCode(juce::String(v.code));
                        });
                });
        };

        sp->onGenerateVenueCode = [this]()
        {
            if (activeVenueId_.isEmpty())
                return;

            juce::Component::SafePointer<MainComponent> safe(this);
            const auto venueId = activeVenueId_;
            VenueService::getInstance().addCode(venueId,
                [safe, venueId](bool ok, juce::String err)
                {
                    if (safe == nullptr)
                        return;
                    if (! ok)
                    {
                        DBG("[Settings] generate venue code failed: " << err);
                        return;
                    }

                    VenueService::getInstance().loadVenue(venueId,
                        [safe](bool loaded, VenueItem v, juce::String loadErr)
                        {
                            if (safe == nullptr) return;
                            if (! loaded)
                            {
                                DBG("[Settings] reload venue after generate code failed: " << loadErr);
                                return;
                            }

                            if (safe->mainArea != nullptr)
                                safe->mainArea->setVenueData(v);

                            if (safe->queueBar != nullptr)
                                safe->queueBar->setVenueInfo(juce::String(v.name), juce::String(v.code));

                            if (safe->lyricWindow_ != nullptr)
                                if (auto* d = safe->lyricWindow_->getDisplay())
                                    d->setVenueCode(juce::String(v.code));
                        });
                });
        };

        sp->onSetEmergencyCode = [this](const juce::String& emergencyCode)
        {
            if (activeVenueId_.isEmpty())
                return;

            juce::Component::SafePointer<MainComponent> safe(this);
            const auto venueId = activeVenueId_;
            juce::Thread::launch([safe, venueId, emergencyCode]
            {
                auto& fc = FirestoreClient::getInstance();
                const auto path = "venues/" + venueId + "?updateMask.fieldPaths=codePlus";
                const auto fields = FirestoreClient::makeFields({
                    { "codePlus", FirestoreClient::stringValue(emergencyCode) }
                });
                const bool ok = fc.patchDocument(path, fields);

                juce::MessageManager::callAsync([safe, ok, venueId]
                {
                    if (safe == nullptr)
                        return;
                    if (! ok)
                    {
                        DBG("[Settings] set emergency code failed");
                        return;
                    }

                    VenueService::getInstance().loadVenue(venueId,
                        [safe](bool loaded, VenueItem v, juce::String loadErr)
                        {
                            if (safe == nullptr) return;
                            if (! loaded)
                            {
                                DBG("[Settings] reload venue after set emergency code failed: " << loadErr);
                                return;
                            }
                            if (safe->mainArea != nullptr)
                                safe->mainArea->setVenueData(v);
                        });
                });
            });
        };

        sp->onGenerateEmergencyCode = [this]()
        {
            const auto generated = VenueService::getInstance().generateCode();
            if (auto* settings = mainArea != nullptr ? mainArea->getSettingsPage() : nullptr)
                if (settings->onSetEmergencyCode)
                    settings->onSetEmergencyCode(generated);
        };

        sp->onInviteUser = [this, normalizeRoleForFirestore](const juce::String& email, const juce::String& roleLabel)
        {
            if (activeVenueId_.isEmpty())
                return;

            juce::Component::SafePointer<MainComponent> safe(this);
            const auto venueId = activeVenueId_;
            const auto venueName = mainArea != nullptr && mainArea->getSettingsPage() != nullptr
                                 ? juce::String(mainArea->getSettingsPage()->getVenueData().name)
                                 : juce::String();

            juce::Thread::launch([safe, venueId, venueName, email = email.trim(), role = normalizeRoleForFirestore(roleLabel)]
            {
                auto& fc = FirestoreClient::getInstance();
                const auto lowerEmail = email.toLowerCase();

                bool hasPendingInvite = false;
                auto invites = fc.listCollection("venueInvitations", 500);
                for (auto& inv : invites)
                {
                    if (FirestoreClient::readString(inv, "venueId") != venueId)
                        continue;
                    if (FirestoreClient::readString(inv, "invitedUserEmail").toLowerCase() != lowerEmail)
                        continue;
                    if (FirestoreClient::readBool(inv, "isAccepted", false))
                        continue;
                    if (FirestoreClient::readBool(inv, "isExpired", false))
                        continue;
                    hasPendingInvite = true;
                    break;
                }

                if (! hasPendingInvite)
                {
                    const auto inviterEmail = fc.getEmail();
                    const auto now = juce::Time::getCurrentTime();
                    const auto expiry = now + juce::RelativeTime::days(30.0);
                    auto fields = FirestoreClient::makeFields({
                        { "venueId",          FirestoreClient::stringValue(venueId) },
                        { "venueName",        FirestoreClient::stringValue(venueName) },
                        { "invitedUserEmail", FirestoreClient::stringValue(lowerEmail) },
                        { "invitedByEmail",   FirestoreClient::stringValue(inviterEmail) },
                        { "invitedByName",    FirestoreClient::stringValue(inviterEmail) },
                        { "role",             FirestoreClient::stringValue(role) },
                        { "invitationDate",   FirestoreClient::timestampValue(now) },
                        { "expirationDate",   FirestoreClient::timestampValue(expiry) },
                        { "isAccepted",       FirestoreClient::booleanValue(false) },
                        { "isExpired",        FirestoreClient::booleanValue(false) }
                    });
                    fc.createDocument("venueInvitations", fields);
                }

                juce::MessageManager::callAsync([safe]
                {
                    if (safe == nullptr) return;
                    safe->refreshSettingsUsers();
                    safe->refreshSettingsInvitations();
                });
            });
        };

        sp->onChangeUserRole = [this, normalizeRoleForFirestore, findAssociationByEmail, countActivePrivilegedUsers, isPrivilegedRole](const juce::String& email, const juce::String& roleLabel)
        {
            if (activeVenueId_.isEmpty())
                return;

            juce::Component::SafePointer<MainComponent> safe(this);
            const auto role = normalizeRoleForFirestore(roleLabel);
            juce::Thread::launch([safe, role, email, findAssociationByEmail, countActivePrivilegedUsers, isPrivilegedRole]
            {
                auto& fc = FirestoreClient::getInstance();
                const auto assoc = findAssociationByEmail(email);
                bool blocked = false;

                if (assoc.path.isNotEmpty()
                    && assoc.status.equalsIgnoreCase("active")
                    && isPrivilegedRole(assoc.role)
                    && ! isPrivilegedRole(role)
                    && countActivePrivilegedUsers() <= 1)
                {
                    blocked = true;
                }

                if (! blocked && assoc.path.isNotEmpty())
                {
                    auto patchPath = assoc.path + "?updateMask.fieldPaths=role";
                    auto fields = FirestoreClient::makeFields({
                        { "role", FirestoreClient::stringValue(role) }
                    });
                    fc.patchDocument(patchPath, fields);
                }

                juce::MessageManager::callAsync([safe, blocked]
                {
                    if (safe == nullptr) return;
                    if (blocked)
                    {
                        juce::AlertWindow::showMessageBoxAsync(
                            juce::MessageBoxIconType::WarningIcon,
                            "Role Change Blocked",
                            "Cannot remove the last privileged venue user.");
                    }
                    safe->refreshSettingsUsers();
                });
            });
        };

        sp->onDeactivateUser = [this, findAssociationByEmail, countActivePrivilegedUsers, isPrivilegedRole](const juce::String& email)
        {
            if (activeVenueId_.isEmpty())
                return;

            juce::Component::SafePointer<MainComponent> safe(this);
            juce::Thread::launch([safe, email, findAssociationByEmail, countActivePrivilegedUsers, isPrivilegedRole]
            {
                auto& fc = FirestoreClient::getInstance();
                const auto assoc = findAssociationByEmail(email);
                bool blocked = false;

                if (assoc.path.isNotEmpty()
                    && assoc.status.equalsIgnoreCase("active")
                    && isPrivilegedRole(assoc.role)
                    && countActivePrivilegedUsers() <= 1)
                {
                    blocked = true;
                }

                if (! blocked && assoc.path.isNotEmpty())
                {
                    auto patchPath = assoc.path + "?updateMask.fieldPaths=status";
                    auto fields = FirestoreClient::makeFields({
                        { "status", FirestoreClient::stringValue("inactive") }
                    });
                    fc.patchDocument(patchPath, fields);
                }

                juce::MessageManager::callAsync([safe, blocked]
                {
                    if (safe == nullptr) return;
                    if (blocked)
                    {
                        juce::AlertWindow::showMessageBoxAsync(
                            juce::MessageBoxIconType::WarningIcon,
                            "Deactivation Blocked",
                            "Cannot deactivate the last privileged venue user.");
                    }
                    safe->refreshSettingsUsers();
                });
            });
        };

        sp->onRemoveUser = [this, findAssociationByEmail, countActivePrivilegedUsers, isPrivilegedRole](const juce::String& email)
        {
            if (activeVenueId_.isEmpty())
                return;

            juce::Component::SafePointer<MainComponent> safe(this);
            juce::Thread::launch([safe, email, findAssociationByEmail, countActivePrivilegedUsers, isPrivilegedRole]
            {
                auto& fc = FirestoreClient::getInstance();
                const auto assoc = findAssociationByEmail(email);
                bool blocked = false;

                if (assoc.path.isNotEmpty()
                    && assoc.status.equalsIgnoreCase("active")
                    && isPrivilegedRole(assoc.role)
                    && countActivePrivilegedUsers() <= 1)
                {
                    blocked = true;
                }

                if (! blocked && assoc.path.isNotEmpty())
                    fc.deleteDocument(assoc.path);

                juce::MessageManager::callAsync([safe, blocked]
                {
                    if (safe == nullptr) return;
                    if (blocked)
                    {
                        juce::AlertWindow::showMessageBoxAsync(
                            juce::MessageBoxIconType::WarningIcon,
                            "Removal Blocked",
                            "Cannot remove the last privileged venue user.");
                    }
                    safe->refreshSettingsUsers();
                    safe->refreshSettingsInvitations();
                    safe->refreshSettingsSessionStats();
                });
            });
        };

        sp->onRevokeInvitation = [this](const juce::String& email)
        {
            if (activeVenueId_.isEmpty())
                return;

            juce::Component::SafePointer<MainComponent> safe(this);
            const auto venueId = activeVenueId_;
            juce::Thread::launch([safe, venueId, email = email.trim().toLowerCase()]
            {
                auto& fc = FirestoreClient::getInstance();
                auto invites = fc.listCollection("venueInvitations", 1000);
                for (auto& inv : invites)
                {
                    if (FirestoreClient::readString(inv, "venueId") != venueId)
                        continue;
                    if (FirestoreClient::readString(inv, "invitedUserEmail").toLowerCase() != email)
                        continue;
                    if (FirestoreClient::readBool(inv, "isAccepted", false))
                        continue;

                    const auto fullName = inv.getProperty("name", juce::var()).toString();
                    const auto marker = "/documents/";
                    const auto idx = fullName.indexOf(marker);
                    if (idx < 0)
                        continue;
                    const auto relPath = fullName.substring(idx + (int) std::strlen(marker));
                    fc.deleteDocument(relPath);
                }

                juce::MessageManager::callAsync([safe]
                {
                    if (safe == nullptr) return;
                    safe->refreshSettingsInvitations();
                });
            });
        };

        sp->onEndSession = [this](std::function<void(bool)> done)
        {
            if (activeVenueId_.isEmpty())
            {
                if (done) done(false);
                return;
            }
            const auto venueId   = activeVenueId_;
            const auto venueName = mainArea ? juce::String(mainArea->getSettingsPage()
                                              ? mainArea->getSettingsPage()->getVenueData().name
                                              : std::string())
                                            : juce::String();

            juce::Component::SafePointer<MainComponent> safe (this);
            ArchiveService::getInstance().archiveAndClearSession(venueId, venueName,
                [safe, done = std::move(done)] (bool ok, juce::String /*sessionId*/, juce::String /*error*/)
                {
                    if (safe != nullptr && ok)
                        safe->reloadQueueFromFirestore(safe->activeVenueId_);
                    if (done) done(ok);
                });
        };
    }

    // When NavBar width changes via drag, re-layout
    navBar->onWidthChanged = [this](int /*newWidth*/) {
        resized();
    };

    // Sample genre list for the bottom half of NavBar
    navBar->setGenreList({ "Pop", "Rock", "Country", "R&B", "Hip Hop",
                           "Dance", "Latin", "Jazz", "Classical", "Oldies" });

    DBG("NavBar and MainArea created and configured");

    // Create QueueBar (right-side singer queue)
    queueBar = std::make_unique<QueueBar>();
    addAndMakeVisible(queueBar.get());

    queueBar->onWidthChanged = [this](int /*newWidth*/) {
        resized();
    };

    queueBar->onExpandToggled = [this](bool expanded)
    {
        queueExpanded_ = expanded;
        resized();
    };

    // Persist queue ordering whenever the UI reorders singers locally.
    queueBar->onReorder = [this](int /*from*/, int /*to*/)
    {
        if (queueBar == nullptr || activeVenueId_.isEmpty())
            return;

        const auto venueId = activeVenueId_;
        const auto singersSnapshot = queueBar->getSingers();

        QueueService::getInstance().persistSingerOrder(venueId, singersSnapshot,
            [](bool ok, juce::String err)
            {
                if (! ok)
                    DBG("[Queue] persistSingerOrder failed: " << err);
            });
    };

    queueBar->onClearQueue = [this]()
    {
        auto& lm = LocalizationManager::getInstance();
        const auto noVenueTitle = lm.getText("queue.clear.no_venue_title");
        const auto noVenueBody = lm.getText("queue.clear.no_venue_body");
        const auto confirmTitle = lm.getText("queue.clear.confirm_title");
        const auto confirmBody = lm.getText("queue.clear.confirm_body");
        const auto deleteButton = lm.getText("queue.clear.confirm_delete");
        const auto cancelButton = lm.getText("button.cancel");
        const auto clearingText = lm.getText("queue.clear.clearing");
        const auto failedTitle = lm.getText("queue.clear.failed_title");
        const auto failedSingersBody = lm.getText("queue.clear.failed_singers_body");
        const auto partialTitle = lm.getText("queue.clear.partial_title");
        const auto partialBody = lm.getText("queue.clear.partial_body");
        const auto clearedText = lm.getText("queue.clear.cleared");

        if (activeVenueId_.isEmpty())
        {
            juce::AlertWindow::showMessageBoxAsync(
                juce::MessageBoxIconType::WarningIcon,
                noVenueTitle,
                noVenueBody);
            return;
        }

        juce::AlertWindow::showOkCancelBox(
            juce::AlertWindow::WarningIcon,
            confirmTitle,
            confirmBody,
            deleteButton,
            cancelButton,
            this,
            juce::ModalCallbackFunction::create(
                [safe = juce::Component::SafePointer<MainComponent>(this),
                 clearingText,
                 failedTitle,
                 failedSingersBody,
                 partialTitle,
                 partialBody,
                 clearedText](int result)
                {
                    if (safe == nullptr || result == 0)
                        return;
                    if (safe->activeVenueId_.isEmpty())
                        return;

                    const auto venueId = safe->activeVenueId_;
                    safe->showMaintenanceToast(clearingText);

                    VenueService::getInstance().deleteAllSingersFromQueue(
                        venueId,
                        [safe, venueId, failedTitle, failedSingersBody, partialTitle, partialBody, clearedText](bool queueOk, juce::String queueErr)
                        {
                            if (safe == nullptr)
                                return;

                            if (! queueOk)
                            {
                                juce::AlertWindow::showMessageBoxAsync(
                                    juce::MessageBoxIconType::WarningIcon,
                                    failedTitle,
                                    failedSingersBody + queueErr);
                                return;
                            }

                            VenueService::getInstance().deleteAllSongsFromRequested(
                                venueId,
                                [safe, venueId, partialTitle, partialBody, clearedText](bool reqOk, juce::String reqErr)
                                {
                                    if (safe == nullptr)
                                        return;

                                    if (safe->audioEngine)
                                        safe->audioEngine->stop();

                                    safe->localNowPlaying_ = {};
                                    safe->hasLocalNowPlaying_ = false;

                                    if (safe->queueBar != nullptr)
                                    {
                                        safe->queueBar->clearNowPlaying();
                                        safe->queueBar->setPlaying(false);
                                        auto resetQueue = safe->composeQueueWithHost({});
                                        safe->queueBar->setSingers(resetQueue);
                                        safe->syncLyricIdlePreview(resetQueue);
                                    }

                                    safe->reloadQueueFromFirestore(venueId);

                                    if (! reqOk)
                                    {
                                        juce::AlertWindow::showMessageBoxAsync(
                                            juce::MessageBoxIconType::WarningIcon,
                                            partialTitle,
                                            partialBody + reqErr);
                                        return;
                                    }

                                    safe->showMaintenanceToast(clearedText);
                                });
                        });
                }));
    };

    queueBar->onPlaySinger = [this](int singerIndex) {
        DBG("QueueBar: Play singer at index " + juce::String(singerIndex));
        if (queueBar == nullptr) return;
        const auto& list = queueBar->getSingers();
        if (singerIndex < 0 || singerIndex >= (int) list.size()) return;

        const auto singer = list[(size_t) singerIndex];
        if (singer.songs.empty())
        {
            DBG("Play singer: no songs in queue");
            return;
        }
        const auto firstSong = singer.songs.front();

        // 1. Show the singer on the now-singing card. Also store a local
        //    copy as the source of truth — Firestore has no concept of
        //    "now playing" on this side, so the watcher poll would
        //    otherwise clear the card on its next tick.
        queueBar->setNowPlaying(singer);
        localNowPlaying_    = singer;
        hasLocalNowPlaying_ = true;

        // 2. Resolve the song in the local library and load it WITHOUT
        //    auto-starting playback. The host presses play on the bottom
        //    bar transport or the now-playing avatar to start the track.
        if (mainArea == nullptr) return;
        const auto& library = mainArea->getLibrarySongs();
        const juce::String wantId     = juce::String(firstSong.songId).trim();
        const juce::String wantName   = juce::String(firstSong.songName).trim().toLowerCase();
        const juce::String wantArtist = juce::String(firstSong.songArtist).trim().toLowerCase();

        const CdgSong* match = nullptr;
        for (const auto& s : library)
        {
            if (! wantId.isEmpty() && juce::String(s.id).trim() == wantId)
            {
                match = &s;
                break;
            }
        }

        // Fallback: queue songId may be a catalog code, not the local DB id.
        if (match == nullptr && wantId.isNotEmpty())
        {
            for (const auto& s : library)
            {
                for (const auto& code : s.code)
                {
                    if (juce::String(code).trim().equalsIgnoreCase(wantId))
                    {
                        match = &s;
                        break;
                    }
                }
                if (match != nullptr)
                    break;
            }
        }
        if (match == nullptr)
        {
            for (const auto& s : library)
            {
                const juce::String libName = juce::String(s.songName).trim().toLowerCase();
                const juce::String libArtist = juce::String(s.artistName).trim().toLowerCase();

                const bool nameMatches = (libName == wantName);
                const bool artistMatches = wantArtist.isEmpty() || (libArtist == wantArtist);

                if (nameMatches && artistMatches)
                {
                    match = &s;
                    break;
                }
            }
        }

        // Last-resort fuzzy fallback: song title contains/contained-by
        // when metadata variants differ slightly.
        if (match == nullptr && wantName.isNotEmpty())
        {
            for (const auto& s : library)
            {
                const juce::String libName = juce::String(s.songName).trim().toLowerCase();
                if (libName.contains(wantName) || wantName.contains(libName))
                {
                    match = &s;
                    break;
                }
            }
        }

        // Database fallback for multi-computer venues:
        // queue item may come from another machine (different library cache
        // lifetime), so resolve against this machine's local song DB.
        CdgSong dbMatch;
        if (match == nullptr)
        {
            SongDatabase db;
            if (db.open())
            {
                if (wantId.isNotEmpty())
                    dbMatch = db.getById(wantId);

                if (dbMatch.id.empty() && wantName.isNotEmpty())
                {
                    const juce::String query = wantArtist.isNotEmpty()
                        ? (wantName + " " + wantArtist)
                        : wantName;

                    auto hits = db.searchPrefix(query, 40);
                    if (!hits.empty())
                        dbMatch = hits.front();
                }
            }

            if (!dbMatch.id.empty())
                match = &dbMatch;
        }
        if (match == nullptr)
        {
            DBG("Play singer: no library match for '" << juce::String(firstSong.songName)
                << "' by '" << juce::String(firstSong.songArtist) << "'");
            showSongUnavailableMessage(firstSong);
            return;
        }

        int resolvedVersionIndex = 0;
        const juce::String wantedVersion = juce::String(firstSong.songVersion).trim();
        if (wantedVersion.isNotEmpty() && !match->version.empty())
        {
            for (size_t vi = 0; vi < match->version.size(); ++vi)
            {
                if (juce::String(match->version[vi]).trim().equalsIgnoreCase(wantedVersion))
                {
                    resolvedVersionIndex = (int) vi;
                    break;
                }
            }
        }

        const int pitchSemis = juce::roundToInt(firstSong.pitch);
        const bool autoStartSelectedSinger = queueAutoStartRequested_;
        queueAutoStartRequested_ = false;
        loadAndPlaySong(*match, resolvedVersionIndex, pitchSemis, autoStartSelectedSinger);
    };

    queueBar->onPlayCurrent = [this, startTransportPlayback]() {
        // Now-playing play button should only start transport for the
        // currently loaded song and never advance/load the queue.
        startTransportPlayback(false);
    };

    queueBar->onSongClicked = [this](int singerIdx, int /*songIdx*/) {
        if (queueBar == nullptr) return;
        const auto& singers = queueBar->getSingers();
        if (singerIdx < 0 || singerIdx >= (int) singers.size()) return;

        const auto& singer = singers[(size_t) singerIdx];
        const juce::String singerName = juce::String(singer.name);
        const auto songsCopy = singer.songs;
        const auto venueId = activeVenueId_;

        EditSingerModal::show(this, singerName, songsCopy,
            [this, venueId, singerName](const std::vector<QueueItem>& updated)
            {
                if (venueId.isEmpty()) return;
                QueueService::getInstance().patchSingerSongs(
                    venueId, singerName, updated,
                    [](bool ok, const juce::String& err)
                    {
                        DBG("[Queue] patchSingerSongs ok=" << (ok ? 1 : 0)
                            << " err=" << err);
                    });
            });
    };

    // ── Remove singer ──────────────────────────────────────────────────────────
    queueBar->onRemoveSinger = [this](int singerIndex)
    {
        if (queueBar == nullptr) return;
        const auto& singers = queueBar->getSingers();
        if (singerIndex < 0 || singerIndex >= (int) singers.size()) return;
        if (singers[(size_t) singerIndex].isHost) return;

        const juce::String docId   = juce::String(singers[(size_t) singerIndex].id);
        const juce::String venueId = activeVenueId_;

        queueBar->removeSinger(singerIndex);

        if (venueId.isEmpty() || docId.isEmpty()) return;
        juce::Thread::launch([venueId, docId]()
        {
            QueueService::getInstance().deleteSinger(
                venueId, docId, [](bool, const juce::String&) {});
        });
    };

    // ── Move up / down (persist via existing onReorder logic) ─────────────────
    queueBar->onMoveSingerUp = [this](int singerIndex)
    {
        if (queueBar == nullptr) return;
        const auto& singers = queueBar->getSingers();
        const int hostIndex = findHostIndexInQueue(singers);
        if (hostIndex >= 0 && singerIndex == hostIndex)
            return;
        int targetIndex = singerIndex - 1;
        if (hostIndex >= 0 && singerIndex > hostIndex)
            targetIndex = juce::jmax(targetIndex, hostIndex + 1);
        if (targetIndex < 0 || targetIndex >= singerIndex) return;
        queueBar->moveSinger(singerIndex, targetIndex);
        if (queueBar->onReorder) queueBar->onReorder(singerIndex, targetIndex);
    };

    queueBar->onMoveSingerDown = [this](int singerIndex)
    {
        if (queueBar == nullptr) return;
        const auto& singers = queueBar->getSingers();
        const int hostIndex = findHostIndexInQueue(singers);
        if (hostIndex >= 0 && singerIndex == hostIndex)
            return;
        int targetIndex = singerIndex + 1;
        if (hostIndex >= 0 && singerIndex < hostIndex)
            targetIndex = juce::jmin(targetIndex, hostIndex - 1);
        if (targetIndex >= (int) queueBar->getSingers().size()) return;
        queueBar->moveSinger(singerIndex, targetIndex);
        if (queueBar->onReorder) queueBar->onReorder(singerIndex, targetIndex);
    };

    // ── Return now-playing to queue ────────────────────────────────────────────
    // Shared helper: re-insert the current singer at the given position and
    // clear the now-playing card, without stopping the audio (the host may
    // want to fade out manually).
    auto returnCurrentToQueue = [this](bool toFront)
    {
        if (queueBar == nullptr) return;
        Singers cs = localNowPlaying_;

        if (cs.id.empty()) return;

        queueBar->clearNowPlaying();
        if (audioEngine) audioEngine->stop();

        const auto venueId = activeVenueId_;
        const juce::String docId = juce::String(cs.id);

        // Re-insert locally at front or back
        auto singers = queueBar->getSingers();
        const int hostIndex = findHostIndexInQueue(singers);
        if (toFront)
            singers.insert(singers.begin() + (hostIndex >= 0 ? hostIndex + 1 : 0), cs);
        else
            singers.push_back(cs);
        reindexQueueWithHostRoundAnchor(singers);
        queueBar->setSingers(singers);

        // Persist new order
        if (queueBar->onReorder)
        {
            for (int i = 0; i < (int) singers.size(); ++i)
                if (singers[(size_t) i].id == cs.id)
                    { queueBar->onReorder(0, i); break; }
        }
    };

    queueBar->onReturnCurrentToQueueNext = [returnCurrentToQueue]()  { returnCurrentToQueue(true);  };
    queueBar->onReturnCurrentToQueueEnd  = [returnCurrentToQueue]()  { returnCurrentToQueue(false); };

    // ── Skip current singer ────────────────────────────────────────────────────
    queueBar->onSkipCurrentSinger = [this]()
    {
        logPlayHistoryIfNeeded(false); // logs only if played > 30 s
        if (audioEngine) audioEngine->stop();
        if (queueBar)    queueBar->clearNowPlaying();
        if (queueBar)    queueBar->setPlaying(false);
    };

    // ── Add singer manually (KJ action) ───────────────────────────────────────
    // Shows an alert to get the singer name, then adds a placeholder row
    // locally with isNewlyAdded=true. The QueueService watcher will merge
    // the real document once the KJ uses the search page to add a song for
    // them (appendSong creates the singer doc automatically).
    queueBar->onAddSinger = [this]()
    {
        auto* alertWindow = new juce::AlertWindow("Add Singer",
                                                   "Enter the singer's name:",
                                                   juce::MessageBoxIconType::QuestionIcon);
        alertWindow->addTextEditor("singerName", "", "Name:");
        alertWindow->addButton("Add",    1, juce::KeyPress(juce::KeyPress::returnKey));
        alertWindow->addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));

        alertWindow->enterModalState(true,
            juce::ModalCallbackFunction::create(
                [this, alertWindow](int result)
                {
                    if (result == 1)
                    {
                        juce::String name = alertWindow->getTextEditorContents("singerName").trim();
                        if (name.isNotEmpty() && queueBar != nullptr)
                        {
                            Singers newSinger;
                            newSinger.id           = ("local-" + juce::Uuid().toString()).toStdString();
                            newSinger.name         = name.toStdString();
                            newSinger.deviceId     = "local";
                            newSinger.isNewlyAdded = true;
                            newSinger.order        = (int) queueBar->getSingers().size();
                            queueBar->addSinger(newSinger);
                        }
                    }
                    delete alertWindow;
                }),
            true);
    };

    // ── Song-finished → auto-advance ──────────────────────────────────────────
    audioEngine->onSongFinished = [this]()
    {
        logPlayHistoryIfNeeded(true);

        if (lyricWindow_ != nullptr)
            lyricWindow_->setForceIdleScreen (true);

        if (queueBar == nullptr) return;
        const auto venueId = activeVenueId_;

        // Remove the finished song from the singer's queue item now that it
        // has actually completed, then move the singer to the end of the
        // round with any remaining songs.
        if (hasLocalNowPlaying_ && !localNowPlaying_.songs.empty())
        {
            auto singers = queueBar->getSingers();
            int singerIndex = -1;
            for (int i = 0; i < (int) singers.size(); ++i)
            {
                if (juce::String(singers[(size_t) i].id).trim() == juce::String(localNowPlaying_.id).trim()
                    || juce::String(singers[(size_t) i].name).trim().equalsIgnoreCase(juce::String(localNowPlaying_.name).trim()))
                {
                    singerIndex = i;
                    break;
                }
            }

            if (singerIndex >= 0)
            {
                auto finishedSinger = singers[(size_t) singerIndex];
                if (! finishedSinger.songs.empty())
                {
                    QueueItem finishedItem;
                    finishedItem.id          = currentSong.id;
                    finishedItem.songId      = currentSong.id;
                    finishedItem.songName    = currentSong.songName;
                    finishedItem.songArtist  = currentSong.artistName;
                    finishedItem.singerName  = finishedSinger.name;
                    finishedItem.singerAvatar= finishedSinger.avatar;

                    finishedSinger.songs.erase(finishedSinger.songs.begin());
                    finishedSinger.songsPerformed += 1;

                    if (! venueId.isEmpty())
                        QueueService::getInstance().removeSong(venueId, finishedItem, nullptr);
                }

                singers.erase(singers.begin() + singerIndex);
                singers.push_back(finishedSinger);
                reindexQueueWithHostRoundAnchor(singers);
                queueBar->setSingers(singers);

                if (queueBar->onReorder)
                    queueBar->onReorder(singerIndex, (int) singers.size() - 1);
            }
        }

        if (bottomBar != nullptr)
            bottomBar->setPlaying(false);
        queueBar->setPlaying(false);

        // Always advance the round when a song ends so the next singer/song
        // is ready. Auto Play controls whether it starts automatically.
        if (! queueBar->isAutoPlayEnabled())
        {
            queueBar->stopCountdown();
            queueAndLoadNextSingerSong(false);
            return;
        }

        if (queueBar->isAutoPlayEnabled())
        {
            int delay = queueBar->getDelaySec();
            if (delay > 0)
                queueBar->startCountdown(delay);
            else if (queueBar->onCountdownFinished)
                queueBar->onCountdownFinished();
        }
    };

    queueBar->onCountdownFinished = [this]()
    {
        if (queueBar == nullptr) return;
        // Advance to the next playable singer in the rotation.
        queueAndLoadNextSingerSong(true);
    };

    // Queue starts empty until a venue is loaded; setVenueId() fetches
    // the live queue from Firestore at venues/<venueId>/queue and
    // populates the bar via QueueService.
    queueBar->clearNowPlaying();
    queueBar->setSingers({});

    DBG("QueueBar created (waiting on venue queue load)");

    // Title label - using LocalizationManager
    titleLabel = std::make_unique<juce::Label>("title", LocalizationManager::getInstance().getText("app.name"));
    titleLabel->setFont(juce::Font(juce::FontOptions().withHeight(28.0f)).boldened());
    titleLabel->setJustificationType(juce::Justification::centred);
    titleLabel->setColour(juce::Label::textColourId, juce::Colours::darkblue);
    addAndMakeVisible(titleLabel.get());
    DBG("Title label created with localized text");

    // Language selection button - using LocalizationManager
    languageButton = std::make_unique<juce::TextButton>(LocalizationManager::getInstance().getText("language.english"));
    languageButton->onClick = [this]() { 
        DBG("Language button clicked");
        showLanguageSelector(); 
    };
    addAndMakeVisible(languageButton.get());
    DBG("Language button created with localized text");
    
    // Status display - using LocalizationManager
    statusLabel = std::make_unique<juce::Label>("status", LocalizationManager::getInstance().getText("status.ready"));
    statusLabel->setFont(juce::Font(juce::FontOptions().withHeight(14.0f)));
    statusLabel->setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(statusLabel.get());
    DBG("Status label created with localized text");

    updateAudioStatusIndicator();
    
    // Debug information (always visible for now)
    debugLabel = std::make_unique<juce::Label>("debug", "Debug: Application Running");
    debugLabel->setFont(juce::Font(juce::FontOptions().withHeight(12.0f)));
    debugLabel->setJustificationType(juce::Justification::bottomLeft);
    debugLabel->setColour(juce::Label::textColourId, juce::Colours::grey);
    addAndMakeVisible(debugLabel.get());
    DBG("Debug label created");
    
    // Force initial bounds for all components to ensure visibility
    auto tempBounds = juce::Rectangle<int>(20, 70, 1160, 710);  // Reserve top 50px for TopBar
    if (titleLabel) titleLabel->setBounds(tempBounds.removeFromTop(60));
    if (languageButton) languageButton->setBounds(tempBounds.removeFromTop(40).withWidth(200));
    if (statusLabel) statusLabel->setBounds(tempBounds.removeFromTop(30));
    if (debugLabel) debugLabel->setBounds(tempBounds.removeFromTop(30));

    maintenanceToastLabel_ = std::make_unique<juce::Label>("maintenanceToast", "");
    maintenanceToastLabel_->setJustificationType(juce::Justification::centredLeft);
    maintenanceToastLabel_->setColour(juce::Label::textColourId, juce::Colours::white);
    // Keep this label transparent so it never paints as a solid bar.
    maintenanceToastLabel_->setColour(juce::Label::backgroundColourId, juce::Colours::transparentBlack);
    maintenanceToastLabel_->setFont(juce::Font(juce::FontOptions().withHeight(13.0f)).boldened());
    maintenanceToastLabel_->setBorderSize(juce::BorderSize<int>(4, 12, 4, 12));
    maintenanceToastLabel_->setVisible(false);
    addAndMakeVisible(maintenanceToastLabel_.get());
    
    DBG("setupUI completed successfully with initial bounds set");
    
    // Load default background tile
    loadBackgroundTile();
}

//==============================================================================
void MainComponent::loadBackgroundTile(const juce::String& path)
{
    auto appDir = juce::File::getSpecialLocation(juce::File::currentExecutableFile).getParentDirectory();
    juce::String tilePath = path.isEmpty() ? "assets/images/backgrounds/background1.png" : path;
    juce::File tileFile = appDir.getChildFile(tilePath);
    if (tileFile.existsAsFile())
    {
        backgroundTileImage_ = juce::ImageFileFormat::loadFrom(tileFile);
        if (backgroundTileImage_.isValid())
            repaint();
    }
}

//==============================================================================
void MainComponent::paint(juce::Graphics& g)
{
    if (backgroundTileImage_.isValid())
    {
        // Scale tile to backgroundTileSize_ width, maintain aspect ratio (like CSS background-size: Npx auto)
        float aspectRatio = (float)backgroundTileImage_.getHeight() / (float)backgroundTileImage_.getWidth();
        int tileW = backgroundTileSize_;
        int tileH = juce::roundToInt(tileW * aspectRatio);
        if (tileH < 1) tileH = tileW;

        for (int y = 0; y < getHeight(); y += tileH)
            for (int x = 0; x < getWidth(); x += tileW)
                g.drawImage(backgroundTileImage_, x, y, tileW, tileH,
                            0, 0, backgroundTileImage_.getWidth(), backgroundTileImage_.getHeight());
    }
    else
    {
        // Fallback solid fill when no tile image is loaded
        g.fillAll(juce::Colour(0xff16213e));
    }
    
    // Draw responsive layout debug info in debug builds
    #if JUCE_DEBUG
    g.setColour(juce::Colours::yellow.withAlpha(0.3f));
    g.drawRect(getLocalBounds(), 2);
    
    // Simplified debug info to avoid getCurrentScreenSize() issues
    juce::String debugInfo = "Debug Mode - " + juce::String(getWidth()) + "x" + juce::String(getHeight());
    g.setColour(juce::Colours::yellow);
    g.setFont(juce::Font(juce::FontOptions().withHeight(10.0f)));
    g.drawText(debugInfo, getLocalBounds().reduced(5), 
               juce::Justification::topRight, true);
    #endif
}

//==============================================================================
void MainComponent::resized()
{
    // Basic responsive resized method
    auto bounds = getLocalBounds();

    // Non-mac: embedded menu bar at the very top.
   #if ! JUCE_MAC
    if (menuBar_ != nullptr && menuBar_->isVisible())
    {
        auto menuBounds = bounds.removeFromTop (24);
        menuBar_->setBounds (menuBounds);
    }
   #endif

    // Reserve bottom area for BottomBar
    if (bottomBar)
    {
        auto bottomBarBounds = bounds.removeFromBottom(bottomBar->getBarHeight());
        bottomBar->setBounds(bottomBarBounds);
    }
    
    // Reserve top area for TopBar
    if (topBar)
    {
        auto topBarBounds = bounds.removeFromTop(topBar->getBarHeight());
        topBar->setBounds(topBarBounds);
    }
    
    // NavBar on the left (resizable width)
    if (navBar)
    {
        navBar->setVisible(!queueExpanded_);
        if (!queueExpanded_)
        {
            auto navBounds = bounds.removeFromLeft(navBar->getBarWidth());
            navBar->setBounds(navBounds);
        }
    }

    // QueueBar on the right (resizable width), or expanded to fill the workspace.
    if (queueBar)
    {
        auto queueBounds = queueExpanded_
            ? bounds
            : bounds.removeFromRight(queueBar->getBarWidth());
        queueBar->setBounds(queueBounds);
    }

    // MainArea fills the remaining centre space
    if (mainArea)
    {
        mainArea->setVisible(!queueExpanded_);
        if (!queueExpanded_)
            mainArea->setBounds(bounds);
    }

    // Loading overlay always covers the full window when visible.
    if (loadingOverlay_)
        loadingOverlay_->setBounds(getLocalBounds());

    if (maintenanceToastLabel_)
    {
        int topOffset = 8;
       #if ! JUCE_MAC
        if (menuBar_ != nullptr && menuBar_->isVisible())
            topOffset += 24;
       #endif
        if (topBar != nullptr)
            topOffset += topBar->getBarHeight();

        const int toastW = juce::jmin(520, juce::jmax(280, getWidth() - 280));
        const int toastH = 34;
        maintenanceToastLabel_->setBounds(getWidth() - toastW - 16, topOffset + 10, toastW, toastH);
        if (maintenanceToastLabel_->getText().trim().isEmpty())
            maintenanceToastLabel_->setVisible(false);
        maintenanceToastLabel_->toFront(false);
    }

    // The old placeholder labels are no longer laid out in the centre;
    // they can be hidden or removed entirely later.
    if (titleLabel)     titleLabel->setVisible(false);
    if (languageButton) languageButton->setVisible(false);
    if (statusLabel)    statusLabel->setVisible(false);
    if (debugLabel)     debugLabel->setVisible(false);
    
    // Basic responsive features (commented out until safer)
    // updateUIForScreenSize();
}

// Simple implementations to satisfy linker requirements
void MainComponent::showLanguageSelector()
{
    // Enhanced language selector using LocalizationManager
    DBG("Language selector clicked - using LocalizationManager");
    
    auto& lm = LocalizationManager::getInstance();
    
    // Create popup menu with available languages
    juce::PopupMenu languageMenu;
    juce::StringArray languages = lm.getAvailableLanguages();
    
    // Add fallback languages if none loaded from files
    if (languages.isEmpty())
    {
        languageMenu.addItem(1, "English (English)", true, lm.getCurrentLanguage() == "en_US");
        languageMenu.addItem(2, "Español (Spanish)", true, lm.getCurrentLanguage() == "es_ES");  
        languageMenu.addItem(3, "Français (French)", true, lm.getCurrentLanguage() == "fr_FR");
        languageMenu.addItem(4, "Deutsch (German)", true, lm.getCurrentLanguage() == "de_DE");
    }
    else
    {
        // Use languages from LocalizationManager
        for (int i = 0; i < languages.size(); ++i)
        {
            juce::String langInfo = languages[i];
            languageMenu.addItem(i + 1, langInfo, true, false);
        }
    }
    
    languageMenu.showMenuAsync(
        juce::PopupMenu::Options()
            .withTargetComponent(languageButton.get())
            .withStandardItemHeight(30),
        [this, &lm](int result)
        {
            if (result > 0)
            {
                juce::String selectedLang;
                
                // Map menu result to language codes
                switch (result)
                {
                    case 1: selectedLang = "en_US"; break;
                    case 2: selectedLang = "es_ES"; break; 
                    case 3: selectedLang = "fr_FR"; break;
                    case 4: selectedLang = "de_DE"; break;
                    default: selectedLang = "en_US"; break;
                }
                
                // Change language using LocalizationManager
                lm.setLanguage(selectedLang);
                
                // Update UI text
                updateAllText();
                
                DBG("Language changed to: " + selectedLang);
            }
        }
    );
}

void MainComponent::updateUIForScreenSize()
{
    // Basic UI updates for different screen sizes
    DBG("updateUIForScreenSize called");
    
    // Simple font scaling based on window size  
    float baseScale = juce::jmin(getWidth() / 1200.0f, getHeight() / 800.0f);
    baseScale = juce::jmax(0.7f, juce::jmin(baseScale, 2.0f)); // Clamp between 0.7 and 2.0
    
    if (titleLabel != nullptr)
    {
        auto scaledFont = juce::Font(juce::FontOptions().withHeight(28.0f * baseScale)).boldened();
        titleLabel->setFont(scaledFont);
    }
    
    if (statusLabel != nullptr)
    {
        auto scaledFont = juce::Font(juce::FontOptions().withHeight(14.0f * baseScale));
        statusLabel->setFont(scaledFont);
    }
    
    #if JUCE_DEBUG
    if (debugLabel != nullptr)
    {
        auto scaledFont = juce::Font(juce::FontOptions().withHeight(12.0f * baseScale));
        debugLabel->setFont(scaledFont);
    }
    #endif
    
    resized(); // Update layout
}

void MainComponent::timerCallback()
{
    updateConnectionStatus();
    updateDebugInfo();
    updateAudioStatusIndicator();
    runSongbookHealthCheckIfReady();

    if (audioEngine == nullptr)
        return;

    // Feed real audio level into the VU meter.
    if (topBar != nullptr)
        topBar->setAudioLevel(juce::jlimit(0.0f, 1.0f, audioEngine->getCurrentLevel() * 4.0f));

    // Feed real playback position into the BottomBar progress/time labels.
    if (bottomBar != nullptr)
    {
        // Prefer the lyric-window video's position when an MP4 is playing.
        const bool videoActive = (lyricWindow_ != nullptr && lyricWindow_->isVideoActive());

        double total = videoActive ? lyricWindow_->getVideoDuration()
                                   : audioEngine->getTotalLength();
        if (total > 0.0)
        {
            double pos = videoActive ? lyricWindow_->getVideoPosition()
                                     : audioEngine->getCurrentPosition();
            pos = juce::jlimit (0.0, total, pos);
            bottomBar->setProgress((float) (pos / total));
        }
        bottomBar->setPlaying (videoActive ? true : audioEngine->isPlaying());
        bottomBar->setVolume (juce::roundToInt(audioEngine->getMasterVolume() * 10.0f));
    }
}

void MainComponent::showSongUnavailableMessage(const QueueItem& item)
{
    juce::String msg = "Song not found on this computer";
    const juce::String name = juce::String(item.songName).trim();
    if (name.isNotEmpty())
        msg = "Song unavailable on this computer: " + name;

    showLoadingOverlay(msg, -1.0);
    if (bottomBar != nullptr)
        bottomBar->setWaveformStatusMessage("Missing local file. Run Library scan to relink this venue.");

    juce::Timer::callAfterDelay(1800, [safe = juce::Component::SafePointer<MainComponent>(this)]()
    {
        if (safe != nullptr)
            safe->hideLoadingOverlay();
    });
}

void MainComponent::showMaintenanceToast(const juce::String& message)
{
    if (message.isEmpty() || maintenanceToastLabel_ == nullptr)
        return;

    const int token = ++maintenanceToastToken_;
    maintenanceToastLabel_->setText(message, juce::dontSendNotification);
    maintenanceToastLabel_->setVisible(true);
    maintenanceToastLabel_->toFront(false);

    juce::Timer::callAfterDelay(3500, [safe = juce::Component::SafePointer<MainComponent>(this), token]()
    {
        if (safe == nullptr || safe->maintenanceToastLabel_ == nullptr)
            return;
        if (safe->maintenanceToastToken_ != token)
            return;
        safe->maintenanceToastLabel_->setVisible(false);
    });
}

void MainComponent::runSongbookHealthCheckIfReady()
{
    if (!pendingSongbookHealthCheck_ || songbookHealthPromptShown_)
        return;
    if (mainArea == nullptr)
        return;

    auto* libraryPage = mainArea->getLibraryPage();
    if (libraryPage == nullptr)
        return;

    const auto& songs = mainArea->getLibrarySongs();
    if (songs.empty())
        return;

    pendingSongbookHealthCheck_ = false;

    int checked = 0;
    int missing = 0;
    int unixStyle = 0;
    int windowsStyle = 0;

    for (const auto& s : songs)
    {
        juce::String path;
        if (!s.fullPath.empty())
            path = juce::String(s.fullPath.front()).trim();

        if (path.isEmpty() && !s.filePath.empty() && !s.fileName.empty())
            path = juce::File(juce::String(s.filePath.front())).getChildFile(juce::String(s.fileName.front())).getFullPathName();

        if (path.isEmpty())
            continue;

        ++checked;
        if (!juce::File(path).existsAsFile())
            ++missing;

        if (path.startsWithChar('/') || path.startsWithIgnoreCase("~"))
            ++unixStyle;

        // Match common Windows absolute path formats (e.g. C:\foo, D:/bar)
        if ((path.length() >= 3 && juce::CharacterFunctions::isLetter(path[0])
                && path[1] == ':' && (path[2] == '\\' || path[2] == '/'))
            || path.startsWith("\\\\"))
            ++windowsStyle;

        if (checked >= 120)
            break;
    }

    if (checked < 10)
        return;

    const int missingPct = (missing * 100) / checked;
    const int unixPct = (unixStyle * 100) / checked;
    const int windowsPct = (windowsStyle * 100) / checked;
    const bool severeMissing = missingPct >= 70;
    bool likelyForeignPaths = false;

   #if JUCE_WINDOWS
    likelyForeignPaths = unixPct >= 30;
   #else
    likelyForeignPaths = windowsPct >= 30;
   #endif

    if (!severeMissing && !likelyForeignPaths)
        return;

    songbookHealthPromptShown_ = true;

    juce::String detail = "Songbook paths appear to be from another computer.\n"
                         "Detected " + juce::String(missingPct) + "% missing files in sampled songs.\n\n"
                         "Run Initial Song Load now to relink this machine?";

    juce::AlertWindow::showOkCancelBox(
        juce::AlertWindow::WarningIcon,
        "Library Path Mismatch Detected",
        detail,
        "Run Scan",
        "Later",
        this,
        juce::ModalCallbackFunction::create([safe = juce::Component::SafePointer<MainComponent>(this)](int result)
        {
            if (safe == nullptr || result == 0 || safe->mainArea == nullptr)
                return;

            if (auto* lp = safe->mainArea->getLibraryPage())
            {
                safe->mainArea->setCurrentPage(NavPage::Library);
                lp->startInitialSongLoad();
            }
        }));
}

void MainComponent::updateAudioStatusIndicator()
{
    if (bottomBar == nullptr)
        return;

    auto& lm = LocalizationManager::getInstance();
    juce::String text;

    if (audioStartupInProgress_)
    {
        text = lm.getText("audio.feedback.starting");
    }
    else if (audioEngine != nullptr && audioEngine->isInitialized())
    {
        text = lm.getText("audio.feedback.ready");
    }
    else
    {
        text = lm.getText("audio.feedback.unavailable");
    }

    bottomBar->setWaveformStatusMessage(text);
}

void MainComponent::updateConnectionStatus()
{
    // Simple status updates without LocalizationManager
    static int statusCounter = 0;
    statusCounter++;
    
    juce::String statusText = "Status: Active (Update #" + juce::String(statusCounter) + ")";
    
    if (statusLabel != nullptr)
        statusLabel->setText(statusText, juce::dontSendNotification);
}

void MainComponent::updateDebugInfo()
{
    // Simple debug info updates
    juce::String debugText = "Size: " + juce::String(getWidth()) + "x" + juce::String(getHeight());
    debugText += " | Timer Active | Memory: " + juce::String(juce::SystemStats::getMemorySizeInMegabytes()) + "MB";
    
    if (debugLabel != nullptr)
        debugLabel->setText(debugText, juce::dontSendNotification);
}

// Configuration methods temporarily disabled for debugging
/*
void MainComponent::configureForMobile()
{
    auto bounds = getLocalBounds();
    auto margin = getScaledMargin(12); // Smaller margin for mobile
    bounds.reduce(margin, margin);
    
    // Vertical stack layout for mobile
    auto titleHeight = 60;
    auto buttonHeight = 44;
    auto spacing = 16;
    
    titleLabel->setBounds(bounds.removeFromTop(titleHeight));
    bounds.removeFromTop(spacing);
    
    languageButton->setBounds(bounds.removeFromTop(buttonHeight));
    bounds.removeFromTop(spacing);
    
    statusLabel->setBounds(bounds.removeFromTop(30));
    
    #if JUCE_DEBUG
    debugLabel->setBounds(bounds.removeFromBottom(40));
    #endif
}

void MainComponent::configureForStandard()
{
    auto bounds = getLocalBounds();
    auto margin = getScaledMargin(16); // Standard margin
    bounds.reduce(margin, margin);
    
    // Standard laptop/desktop layout (HD, WXGA, etc.)
    auto titleArea = bounds.removeFromTop(80);
    titleLabel->setBounds(titleArea);
    
    auto topControls = bounds.removeFromTop(50);
    languageButton->setBounds(topControls.removeFromRight(150));
    
    bounds.removeFromTop(20);
    statusLabel->setBounds(bounds.removeFromTop(30));
    
    #if JUCE_DEBUG
    debugLabel->setBounds(bounds.removeFromBottom(40));
    #endif
}
// Configuration methods temporarily disabled for debugging
/*void MainComponent::configureForWide()
{
    auto bounds = getLocalBounds();
    auto margin = getScaledMargin(20); // Larger margin for wide screens
    bounds.reduce(margin, margin);
    
    // Wide screen layout (Full HD, QHD, etc.)
    auto header = bounds.removeFromTop(100);
    titleLabel->setBounds(header.removeFromTop(60));
    
    auto controls = header.removeFromTop(40);
    languageButton->setBounds(controls.removeFromRight(150));
    
    // More space available, adjust accordingly
    bounds.removeFromTop(20);
    statusLabel->setBounds(bounds.removeFromTop(25));
    
    #if JUCE_DEBUG
    debugLabel->setBounds(bounds.removeFromBottom(30));
    #endif
}

void MainComponent::configureForUltraWide()
{
    auto bounds = getLocalBounds();
    auto margin = getScaledMargin(24); // Even larger margin for ultra-wide
    bounds.reduce(margin, margin);
    
    // Ultra-wide layout (21:9, 32:9 aspect ratios)
    // Use horizontal space more efficiently
    auto header = bounds.removeFromTop(80);
    
    // Split header horizontally for ultra-wide
    auto titleArea = header.removeFromLeft(header.getWidth() * 0.6f);
    titleLabel->setBounds(titleArea);
    
    auto controlArea = header.reduced(20, 0);
    languageButton->setBounds(controlArea.removeFromRight(150));
    
    bounds.removeFromTop(20);
    
    // Status area - use left portion of ultra-wide screen
    auto statusArea = bounds.removeFromTop(30);
    statusLabel->setBounds(statusArea.removeFromLeft(statusArea.getWidth() * 0.7f));
    
    #if JUCE_DEBUG
    auto debugArea = bounds.removeFromBottom(30);
    debugLabel->setBounds(debugArea.removeFromLeft(debugArea.getWidth() * 0.8f));
    #endif
}

/*
void MainComponent::configureForHighResolution()
{
    // 4K/5K/8K displays - use desktop layout with automatic font scaling applied
    configureForWide();
    
    // Additional adjustments for very high resolution displays
    if (getCurrentScreenSize() == ScreenSize::UHD_8K)
    {
        // For 8K displays, we might want even more spacing
        auto bounds = getLocalBounds();
        auto extraMargin = bounds.getWidth() * 0.05f; // 5% extra margin on 8K
        
        // Apply extra margin to all components (this is illustrative)
        if (titleLabel)
        {
            auto titleBounds = titleLabel->getBounds();
            titleBounds.reduce(static_cast<int>(extraMargin * 0.5f), 0);
            titleLabel->setBounds(titleBounds);
        }
    }
}

//==============================================================================
void MainComponent::updateUIForScreenSize()
{
    ResponsiveLayout::updateUIForScreenSize();
    
    // Apply font scaling based on screen size
    if (titleLabel != nullptr)
    {
        auto baseFont = 28.0f * getScaleFactor();
        titleLabel->setFont(juce::Font(juce::FontOptions().withHeight(baseFont)).boldened());
    }
    
    if (languageButton != nullptr)
    {
        // TextButton font is handled by LookAndFeel, not setFont
    }
    
    if (statusLabel != nullptr)
    {
        auto baseFont = 14.0f * getScaleFactor();
        statusLabel->setFont(juce::Font(juce::FontOptions().withHeight(baseFont)));
    }
    
    #if JUCE_DEBUG
    if (debugLabel != nullptr)
    {
        auto baseFont = 12.0f * getScaleFactor();
        debugLabel->setFont(juce::Font(juce::FontOptions().withHeight(baseFont)));
    }
    #endif
}
*/

//==============================================================================
void MainComponent::updateAllText()
{
    // Update all UI text using LocalizationManager 
    DBG("updateAllText called - refreshing localized text");
    
    auto& lm = LocalizationManager::getInstance();
    
    try
    {
        // Update title
        if (titleLabel != nullptr)
        {
            titleLabel->setText(lm.getText("app.name"), juce::dontSendNotification);
        }
        
        // Update language button with current language display name  
        if (languageButton != nullptr)
        {
            languageButton->setButtonText(lm.getCurrentLanguageDisplayName());
        }
        
        // Update status
        if (statusLabel != nullptr)
        {
            statusLabel->setText(lm.getText("status.ready"), juce::dontSendNotification);
        }
        
        // Update debug info
        if (debugLabel != nullptr)
        {
            juce::String debugText = "Debug: " + lm.getCurrentLanguage() + " Active";
            debugLabel->setText(debugText, juce::dontSendNotification);
        }
        
        // Propagate to child bar components
        if (topBar)    topBar->updateAllText();
        if (bottomBar) bottomBar->updateAllText();
        if (navBar)    navBar->updateAllText();
        if (mainArea)  mainArea->updateAllText();
        if (queueBar)  queueBar->updateAllText();
        
        // Trigger repaint to update any drawn text
        repaint();
        
        DBG("All UI text updated successfully");
    }
    catch (const std::exception& e)
    {
        DBG("Error updating UI text: " + juce::String(e.what()));
    }
    catch (...)
    {
        DBG("Unknown error updating UI text");
    }
}

// Methods temporarily disabled for debugging
/*
//==============================================================================
void MainComponent::changeLanguage(const juce::String& languageCode)
{
    auto& lm = LocalizationManager::getInstance();
    
    lm.setLanguage(languageCode);
    updateAllText();
    repaint();
    
    DBG("Language changed to: " + languageCode);
        
    // Save preference (in a real app, this would go to settings)
    // For now, just log it
    auto message = lm.getText("status.language_changed");
    message = message.replace("{language}", lm.getCurrentLanguageDisplayName());
    statusLabel->setText(message, juce::dontSendNotification);
}

void MainComponent::showLanguageSelector()
{
    // Simplified language selector - no LocalizationManager for now
    DBG("Language selector clicked - functionality temporarily disabled");
    
    // Show a simple popup menu with basic options
    juce::PopupMenu languageMenu;
    languageMenu.addItem(1, "English", true, true);
    languageMenu.addItem(2, "Español", true, false);
    languageMenu.addItem(3, "Français", true, false);
    
    languageMenu.showMenuAsync(
        juce::PopupMenu::Options()
            .withTargetComponent(languageButton.get())
            .withStandardItemHeight(30),
        [this](int result)
        {
            DBG("Selected language option: " + juce::String(result));
            // Language change functionality disabled for now
        }
    );
}

//==============================================================================
void MainComponent::detectAndConfigureScreens()
{
    auto& desktop = juce::Desktop::getInstance();
    auto primaryDisplay = desktop.getDisplays().getPrimaryDisplay();
    
    // Simple implementation - just detect if there might be multiple displays
    // by checking if the primary display is not the main screen boundary
    auto mainScreenBounds = desktop.getDisplays().getTotalBounds(true);
    bool hasMultipleDisplays = mainScreenBounds.getWidth() > primaryDisplay->totalArea.getWidth() || 
                               mainScreenBounds.getHeight() > primaryDisplay->totalArea.getHeight();
    
    DBG("Primary display: " + 
        juce::String(primaryDisplay->totalArea.getWidth()) + "x" + 
        juce::String(primaryDisplay->totalArea.getHeight()));
        
    if (hasMultipleDisplays)
    {
        setupDualScreenLayout();
        DBG("Multiple displays detected");
    }
    else
    {
        DBG("Single display detected");
    }
}

void MainComponent::setupDualScreenLayout()
{
    // In a full karaoke system, this would setup:
    // - Primary display: DJ/operator interface
    // - Secondary display: Public song display with lyrics
    
    DBG("Setting up dual-screen karaoke layout");
    
    // For now, just log that we detected multiple screens
    auto& lm = LocalizationManager::getInstance();
    statusLabel->setText(lm.getText("status.dual_screen_detected"), juce::dontSendNotification);
}

//==============================================================================
void MainComponent::timerCallback()
{
    updateConnectionStatus();
    updateDebugInfo();
}

void MainComponent::updateConnectionStatus()
{
    // Simplified - no LocalizationManager for now
    static int statusCounter = 0;
    statusCounter++;
    
    juce::String statusText = "Status Update #" + juce::String(statusCounter);
    
    if (statusLabel != nullptr)
        statusLabel->setText(statusText, juce::dontSendNotification);
}

void MainComponent::updateDebugInfo()
{
    // Simplified debug info - no LocalizationManager calls
    juce::String debugText = "Size: " + juce::String(getWidth()) + "x" + juce::String(getHeight());
    debugText += " | Debug Mode Active";
    
    if (debugLabel != nullptr)
        debugLabel->setText(debugText, juce::dontSendNotification);
}

//==============================================================================
void MainComponent::setHighContrastMode(bool enabled)
{
    highContrastMode = enabled;
    
    // Apply high contrast colors
    if (enabled)
    {
        titleLabel->setColour(juce::Label::textColourId, juce::Colours::white);
        statusLabel->setColour(juce::Label::textColourId, juce::Colours::lightgrey);
    }
    else
    {
        titleLabel->setColour(juce::Label::textColourId, juce::Colours::darkblue);
        statusLabel->setColour(juce::Label::textColourId, juce::Colours::black);
    }
    
    repaint();
}

void MainComponent::setLargeTextMode(bool enabled)
{
    largeTextMode = enabled;
    
    // Increase font sizes for accessibility
    float textScale = enabled ? 1.25f : 1.0f;
    
    titleLabel->setFont(juce::Font(juce::FontOptions().withHeight(28.0f * textScale * getScaleFactor())).boldened());
    // TextButton font is handled by LookAndFeel, not setFont
    statusLabel->setFont(juce::Font(juce::FontOptions().withHeight(14.0f * textScale * getScaleFactor())));
    
    #if JUCE_DEBUG
    debugLabel->setFont(juce::Font(juce::FontOptions().withHeight(12.0f * textScale * getScaleFactor())));
    #endif
    
    resized(); // Relayout with new font sizes
}
*/
//==============================================================================
// Song playback
//==============================================================================
void MainComponent::loadAndPlaySong(const CdgSong& song, int versionIndex, int pitchSemitones, bool autoStart)
{
    if (! audioEngine)
        return;

    if (! audioEngine->isInitialized())
    {
        DBG("loadAndPlaySong: audio engine not ready, attempting immediate init");
        audioEngine->initialize();

        if (! audioEngine->isInitialized())
        {
            auto& lm = LocalizationManager::getInstance();
            showLoadingOverlay(audioStartupInProgress_ ? lm.getText("audio.feedback.engine_starting")
                                                       : lm.getText("audio.feedback.engine_unavailable"));
            juce::Timer::callAfterDelay(1200, [safe = juce::Component::SafePointer<MainComponent>(this)]()
            {
                if (safe != nullptr)
                    safe->hideLoadingOverlay();
            });
            return;
        }
    }

    // Show the loading overlay immediately so the user gets instant feedback
    // instead of the OS beach-ball that the synchronous load would otherwise
    // produce.
    showLoadingOverlay("Loading \"" + juce::String(song.songName) + "\"...");

    // Defer the actual load work to the next message-loop tick so the overlay
    // has a chance to paint before we block the UI thread on file I/O.
    juce::Component::SafePointer<MainComponent> self(this);
    juce::MessageManager::callAsync([self, song, versionIndex, pitchSemitones, autoStart]()
    {
        if (! self || ! self->audioEngine)
            return;

    // Pick the file path for the chosen version (fall back to the first path).
    juce::String path;
    if (versionIndex >= 0 && versionIndex < (int) song.fullPath.size())
        path = juce::String(song.fullPath[(size_t) versionIndex]);
    else if (! song.fullPath.empty())
        path = juce::String(song.fullPath.front());

    if (path.isEmpty())
    {
        DBG("loadAndPlaySong: no file path on song \"" + juce::String(song.songName) + "\"");
        self->hideLoadingOverlay();
        return;
    }

    juce::File audioFile(path);

    // Some karaoke libraries store a ZIP containing CDG + MP3, or a .cdg file
    // beside a .mp3 — try a sibling MP3 if the direct path isn't playable.
    auto ext = audioFile.getFileExtension().toLowerCase();
    if (ext == ".cdg" || ext == ".zip")
    {
        auto sibling = audioFile.withFileExtension("mp3");
        if (sibling.existsAsFile())
            audioFile = sibling;
        ext = audioFile.getFileExtension().toLowerCase();
    }

    if (! audioFile.existsAsFile())
    {
        // Fallback: if the requested version path is unavailable, try every
        // known version path and pick the first playable file.
        for (const auto& fp : song.fullPath)
        {
            juce::File candidate { juce::String(fp) };
            auto cext = candidate.getFileExtension().toLowerCase();
            if (cext == ".cdg" || cext == ".zip")
            {
                auto sibling = candidate.withFileExtension("mp3");
                if (sibling.existsAsFile())
                    candidate = sibling;
            }

            if (candidate.existsAsFile())
            {
                audioFile = candidate;
                ext = audioFile.getFileExtension().toLowerCase();
                DBG("loadAndPlaySong: fallback picked playable version: " + audioFile.getFullPathName());
                break;
            }
        }
    }

    if (! audioFile.existsAsFile())
    {
        DBG("loadAndPlaySong: file not found: " + audioFile.getFullPathName());
        self->hideLoadingOverlay();
        return;
    }

    // ─── MP4 / video branch ────────────────────────────────────────────────
    // The lyric display owns its own video player (juce::VideoComponent) which
    // handles audio + video together. We bypass AudioEngine entirely for
    // these formats so the two pipelines don't double-up the audio.
    const bool isVideo = (ext == ".mp4" || ext == ".m4v" || ext == ".mov");

    if (isVideo)
    {
        // Stop any currently-playing audio song so we don't overlap.
        self->audioEngine->stop();

        if (self->lyricWindow_ == nullptr
            || ! self->lyricWindow_->loadVideo (audioFile, autoStart))
        {
            DBG("loadAndPlaySong: video load failed: " + audioFile.getFullPathName());
            self->hideLoadingOverlay();
            return;
        }

        self->currentSong         = song;
        self->currentSongImageUrl = juce::String (song.imageUrl);
        self->currentSongDuration = self->lyricWindow_->getVideoDuration();

        if (self->topBar)
        {
            juce::String version;
            if (versionIndex >= 0 && versionIndex < (int) song.version.size())
                version = juce::String (song.version[(size_t) versionIndex]);

            self->topBar->setTrackInfo (juce::String (song.songName),
                                        juce::String (song.artistName),
                                        version);
            self->topBar->setMusicInfo (juce::String (song.keySignature),
                                        (int) std::round (song.tempo));

            if (self->currentSongImageUrl.isNotEmpty())
            {
                juce::Component::SafePointer<TopBar> safeTop (self->topBar.get());
                juce::String url = self->currentSongImageUrl;
                juce::Image img = ArtworkCache::getInstance().getOrFetch (url, [safeTop, url]() {
                    if (! safeTop) return;
                    auto cached = ArtworkCache::getInstance().getOrFetch (url, nullptr);
                    if (cached.isValid()) safeTop->setCoverArt (cached);
                });
                if (img.isValid())
                    self->topBar->setCoverArt (img);
            }
            else
            {
                self->topBar->setCoverArt ({});
            }
        }

        if (self->bottomBar)
        {
            self->bottomBar->setDurationSeconds (self->currentSongDuration);
            self->bottomBar->setProgress (0.0f);
            self->bottomBar->setPlaying (autoStart);
            self->bottomBar->setPitch (pitchSemitones);
            self->bottomBar->setWaveformSamples ({}); // No waveform for video.
        }

        if (self->queueBar) self->queueBar->setPlaying (autoStart);

        self->hideLoadingOverlay();
        return;
    }

    if (! self->audioEngine->loadSong(audioFile))
    {
        DBG("loadAndPlaySong: AudioEngine failed to load " + audioFile.getFullPathName());
        self->hideLoadingOverlay();
        return;
    }

    // Push the matching CDG file to the secondary lyric display. If no CDG
    // companion exists the window will simply fall back to its idle screen.
    if (self->lyricWindow_)
    {
        const auto cdgFile = self->resolveCdgFileFor (audioFile);
        self->lyricWindow_->loadCDG (cdgFile);
    }

    self->currentSong          = song;
    self->currentSongImageUrl  = juce::String(song.imageUrl);
    self->currentSongDuration  = self->audioEngine->getTotalLength();

    // Apply pitch (the dialog's semitone adjustment)
    self->audioEngine->setPitchShift((float) pitchSemitones);

    // TopBar — song info
    if (self->topBar)
    {
        juce::String version;
        if (versionIndex >= 0 && versionIndex < (int) song.version.size())
            version = juce::String(song.version[(size_t) versionIndex]);

        self->topBar->setTrackInfo(juce::String(song.songName),
                             juce::String(song.artistName),
                             version);

        int bpm = (int) std::round(song.tempo);
        self->topBar->setMusicInfo(juce::String(song.keySignature), bpm);

        // Artwork (async via ArtworkCache)
        if (self->currentSongImageUrl.isNotEmpty())
        {
            juce::Component::SafePointer<TopBar> safeTop(self->topBar.get());
            juce::String url = self->currentSongImageUrl;
            juce::Image img = ArtworkCache::getInstance().getOrFetch(url, [safeTop, url]() {
                if (! safeTop) return;
                auto cached = ArtworkCache::getInstance().getOrFetch(url, nullptr);
                if (cached.isValid()) safeTop->setCoverArt(cached);
            });
            if (img.isValid())
                self->topBar->setCoverArt(img);
        }
        else
        {
            self->topBar->setCoverArt({}); // reset
        }
    }

    // BottomBar — duration + waveform
    if (self->bottomBar)
    {
        self->bottomBar->setDurationSeconds(self->currentSongDuration);
        self->bottomBar->setProgress(0.0f);
        self->bottomBar->setPlaying(autoStart);
        self->bottomBar->setPitch(pitchSemitones);
        self->bottomBar->setWaveformSamples({});

        // Build a real waveform asynchronously.
        juce::Component::SafePointer<BottomBar> safeBottom(self->bottomBar.get());
        WaveformGenerator::generateAsync(audioFile, 240, [safeBottom](std::vector<float> peaks) {
            if (safeBottom) safeBottom->setWaveformSamples(peaks);
        });
    }

    if (self->queueBar) self->queueBar->setPlaying (autoStart);

    if (autoStart)
    {
        self->playStartTimeMs_ = juce::Time::currentTimeMillis();
        self->audioEngine->play();
    }
    self->hideLoadingOverlay();
    });
}

//==============================================================================
void MainComponent::showLoadingOverlay(const juce::String& message, double progress)
{
    if (! loadingOverlay_)
    {
        loadingOverlay_ = std::make_unique<LoadingOverlay>();
        addAndMakeVisible(loadingOverlay_.get());
    }
    loadingOverlay_->setState(message, progress);
    loadingOverlay_->setBounds(getLocalBounds());
    loadingOverlay_->toFront(false);
    loadingOverlay_->setVisible(true);
    loadingOverlay_->repaint();
}

bool MainComponent::queueAndLoadNextSingerSong(bool autoStartAfterLoad)
{
    if (queueBar == nullptr || !queueBar->onPlaySinger)
        return false;

    const auto venueId = activeVenueId_;
    auto current = queueBar->getSingers();
    if (current.empty())
        return false;

    // Rotate past singers with no songs and stop at the first playable singer.
    // Process from the actual top of queue so queue order stays in sync with
    // round order, including host/empty singers being moved to the tail.
    const int maxAttempts = (int) current.size();
    for (int attempt = 0; attempt < maxAttempts; ++attempt)
    {
        current = queueBar->getSingers();
        if (current.empty())
            return false;

        const int frontIndex = 0;

        if (! current[(size_t) frontIndex].songs.empty())
        {
            queueAutoStartRequested_ = autoStartAfterLoad;
            queueBar->onPlaySinger(frontIndex);
            return true;
        }

        const int endIndex = (int) current.size() - 1;

        auto rotated = current;
        auto moved = rotated[(size_t) frontIndex];
        rotated.erase(rotated.begin() + frontIndex);

        bool removedForStrikes = false;
        if (! moved.isHost)
        {
            const int currentStrikes = juce::jmax(0, moved.strikes);
            const int nextStrikes = currentStrikes + 1;

            if (activeVenueNumStrikes_ > 0 && nextStrikes >= activeVenueNumStrikes_)
            {
                removedForStrikes = true;
                if (! venueId.isEmpty())
                {
                    QueueService::getInstance().deleteSinger(
                        venueId,
                        juce::String(moved.name),
                        nullptr);
                }
            }
            else
            {
                moved.strikes = nextStrikes;
            }
        }

        if (! removedForStrikes)
            rotated.push_back(std::move(moved));

        reindexQueueWithHostRoundAnchor(rotated);

        queueBar->setSingers(rotated);
        if (queueBar->onReorder)
            queueBar->onReorder(frontIndex, removedForStrikes ? frontIndex : endIndex);
    }

    queueAutoStartRequested_ = false;
    return false;
}

std::vector<Singers> MainComponent::composeQueueWithHost(const std::vector<Singers>& queueSingers) const
{
    std::vector<Singers> merged = queueSingers;

    if (!HostService::getInstance().hasCurrent())
        return merged;

    const auto h = HostService::getInstance().getCurrent();
    Singers hostSinger;
    hostSinger.id     = h.userId.empty() ? h.profileId : h.userId;
    hostSinger.name   = ! h.stageName.empty() ? h.stageName
                      : ! h.fullName.empty()  ? h.fullName
                      : "Host";
    hostSinger.avatar = h.avatarUrl;
    hostSinger.isHost = true;
    hostSinger.order  = -1;
    hostSinger.rotationOrder = -1;

    int preferredIndex = -1;
    if (queueBar != nullptr)
    {
        const auto& current = queueBar->getSingers();
        for (int i = 0; i < (int) current.size(); ++i)
        {
            if (!current[(size_t) i].isHost)
                continue;

            const auto existingHostId = juce::String(current[(size_t) i].id).trim();
            if (existingHostId.isEmpty() || existingHostId == juce::String(hostSinger.id).trim())
            {
                preferredIndex = i;
                break;
            }
        }
    }

    // First load path (no host currently in the visible queue): place the
    // host at the start of the current round, not necessarily at queue index 0.
    if (preferredIndex < 0)
    {
        if (! merged.empty())
        {
            int minRotation = INT_MAX;
            int minRotationIndex = 0;
            for (int i = 0; i < (int) merged.size(); ++i)
            {
                if (merged[(size_t) i].rotationOrder < minRotation)
                {
                    minRotation = merged[(size_t) i].rotationOrder;
                    minRotationIndex = i;
                }
            }
            preferredIndex = minRotationIndex;
        }
        else
        {
            preferredIndex = 0;
        }
    }

    preferredIndex = juce::jlimit(0, (int) merged.size(), preferredIndex);
    merged.insert(merged.begin() + preferredIndex, std::move(hostSinger));

    return merged;
}

std::vector<LyricDisplayComponent::QueuePreviewEntry>
MainComponent::buildLyricQueuePreview(const std::vector<Singers>& singers) const
{
    std::vector<LyricDisplayComponent::QueuePreviewEntry> preview;
    preview.reserve (3);

    for (const auto& singer : singers)
    {
        if (singer.isHost)
            continue;

        LyricDisplayComponent::QueuePreviewEntry entry;
        entry.singerName = juce::String (singer.name);

        if (! singer.songs.empty())
        {
            const auto& song = singer.songs.front();
            entry.songName = juce::String (song.songName);
            entry.artistName = juce::String (song.songArtist);
        }

        preview.push_back (std::move (entry));
        if ((int) preview.size() >= 3)
            break;
    }

    return preview;
}

void MainComponent::syncLyricIdlePreview(const std::vector<Singers>& singers)
{
    if (lyricWindow_ == nullptr)
        return;

    lyricWindow_->setVenueContext (activeVenueId_, activeVenueName_);
    lyricWindow_->setQueuePreview (buildLyricQueuePreview (singers));
}

void MainComponent::updateLoadingOverlay(const juce::String& message, double progress)
{
    if (loadingOverlay_)
        loadingOverlay_->setState(message, progress);
}

void MainComponent::hideLoadingOverlay()
{
    if (loadingOverlay_)
        loadingOverlay_->setVisible(false);
}

void MainComponent::startDeferredAudioServices(const juce::String& venueId, int startupToken)
{
    juce::Component::SafePointer<MainComponent> safeThis(this);
    updateLoadingOverlay("Starting audio engine...", 0.92);
    audioStartupInProgress_ = true;
    audioStartupComplete_ = false;
    if (bottomBar != nullptr)
        bottomBar->setPlayEnabled(false);

    juce::MessageManager::callAsync([safeThis, venueId, startupToken]()
    {
        if (safeThis == nullptr || safeThis->startupLoadToken_ != startupToken || safeThis->activeVenueId_ != venueId)
            return;

        safeThis->hideLoadingOverlay();
    });

    juce::Thread::launch([safeThis, venueId, startupToken]()
    {
        if (safeThis == nullptr || safeThis->audioEngine == nullptr)
            return;

        const auto startMs = juce::Time::getMillisecondCounterHiRes();
        DBG("[AudioStartup] Background audio startup begin");
        if (! safeThis->audioEngine->isInitialized())
            safeThis->audioEngine->initialize();
        const auto initMs = juce::Time::getMillisecondCounterHiRes() - startMs;

        juce::MessageManager::callAsync([safeThis, venueId, startupToken, initMs]()
        {
            if (safeThis == nullptr || safeThis->startupLoadToken_ != startupToken || safeThis->activeVenueId_ != venueId)
                return;

            const auto lyricStartMs = juce::Time::getMillisecondCounterHiRes();

            if (safeThis->lyricWindow_ == nullptr)
                safeThis->lyricWindow_ = std::make_unique<LyricDisplayWindow> (safeThis->audioEngine.get());

            const auto lyricMs = juce::Time::getMillisecondCounterHiRes() - lyricStartMs;

            if (auto* d = safeThis->lyricWindow_ != nullptr ? safeThis->lyricWindow_->getDisplay() : nullptr)
            {
                d->setVenueContext (safeThis->activeVenueId_, safeThis->activeVenueName_);
                if (safeThis->pendingVenueCode_.isNotEmpty())
                    d->setVenueCode(safeThis->pendingVenueCode_);
                if (safeThis->pendingVenueLogo_.isValid())
                    d->setVenueLogo(safeThis->pendingVenueLogo_);

                if (safeThis->queueBar != nullptr)
                    safeThis->syncLyricIdlePreview (safeThis->queueBar->getSingers());
            }

            safeThis->audioStartupInProgress_ = false;
            safeThis->audioStartupComplete_ = safeThis->audioEngine != nullptr
                                            && safeThis->audioEngine->isInitialized();
            if (safeThis->bottomBar != nullptr)
                safeThis->bottomBar->setPlayEnabled(safeThis->audioStartupComplete_);

            DBG("[AudioStartup] Deferred audio init finished in " + juce::String(initMs, 1)
                + " ms; lyric window setup in " + juce::String(lyricMs, 1) + " ms");
            safeThis->hideLoadingOverlay();
        });
    });
}

//==============================================================================
juce::File MainComponent::resolveCdgFileFor (const juce::File& audioFile) const
{
    if (! audioFile.existsAsFile())
        return {};

    // If the audio file is itself a .cdg, that's our target.
    if (audioFile.getFileExtension().equalsIgnoreCase (".cdg"))
        return audioFile;

    // Common case: the audio file is an .mp3/.m4a and the CDG graphics sit
    // in a sibling file with the same stem. Probe a couple of common casings.
    for (const juce::String& ext : { ".cdg", ".CDG", ".Cdg" })
    {
        auto sibling = audioFile.withFileExtension (ext);
        if (sibling.existsAsFile())
            return sibling;
    }

    return {};
}

//==============================================================================
void MainComponent::installMenuBarModel (juce::MenuBarModel* model)
{
   #if JUCE_MAC
    juce::ignoreUnused (model);
    // macOS uses the system menu bar, managed directly by EncoreApplication
    // via juce::MenuBarModel::setMacMainMenu. Nothing to do here.
   #else
    if (model == nullptr)
    {
        menuBar_.reset();
    }
    else
    {
        if (menuBar_ == nullptr)
        {
            menuBar_ = std::make_unique<juce::MenuBarComponent> (model);
            addAndMakeVisible (menuBar_.get());
        }
        else
        {
            menuBar_->setModel (model);
        }
    }
    resized();
   #endif
}

//==============================================================================
void MainComponent::loadVenuePlaylists()
{
    const auto venueId = activeVenueId_;
    if (venueId.isEmpty()) return;

    juce::Component::SafePointer<MainComponent> safe (this);
    auto& v = VenueService::getInstance();

    auto applyToHome = [safe](const std::vector<Playlist>& list,
                              std::unordered_set<std::string>& cache,
                              const char* logTag,
                              std::function<void(HomePage&, const std::vector<Playlist>&)> setter)
    {
        if (safe == nullptr) return;

        cache.clear();
        for (auto& p : list)
            if (! p.id.empty()) cache.insert(p.id);

        DBG ("[Playlists] " << logTag << " count=" << (int) list.size());

        if (auto* hp = safe->mainArea ? safe->mainArea->getHomePage() : nullptr)
            setter(*hp, list);
    };

    // The home page "New Songs" row is driven by local addedAt tracking
    // (setSongsFromLibrary). We still fetch the Firebase "new" playlist to
    // keep newSongIds_ in sync for the SongEditDialog checkbox state.
    v.getNewSongs(venueId,
        [safe](bool ok, std::vector<Playlist> list, juce::String err)
        {
            if (safe == nullptr) return;
            if (! ok) { DBG ("[Playlists] new load failed: " << err); return; }
            safe->newSongIds_.clear();
            for (auto& p : list)
                if (! p.id.empty()) safe->newSongIds_.insert(p.id);
            DBG ("[Playlists] new (for edit-dialog) count=" << (int) list.size());
        });

    v.getPlaylists(venueId, "Popular",
        [safe, applyToHome](bool ok, std::vector<Playlist> list, juce::String err)
        {
            if (safe == nullptr) return;
            if (! ok) { DBG ("[Playlists] Popular load failed: " << err); return; }
            applyToHome(list, safe->popularSongIds_, "Popular",
                [](HomePage& hp, const std::vector<Playlist>& l) { hp.setPopularSongs(l); });
        });

    v.getPlaylists(venueId, "Recommended",
        [safe, applyToHome](bool ok, std::vector<Playlist> list, juce::String err)
        {
            if (safe == nullptr) return;
            if (! ok) { DBG ("[Playlists] Recommended load failed: " << err); return; }
            applyToHome(list, safe->recommendedSongIds_, "Recommended",
                [](HomePage& hp, const std::vector<Playlist>& l) { hp.setRecommendedSongs(l); });
        });

    v.getRecentlyPlayed(venueId,
        [safe](bool ok, std::vector<Playlist> list, juce::String err)
        {
            if (safe == nullptr) return;
            if (! ok) { DBG ("[Playlists] RecentlyPlayed load failed: " << err); return; }
            DBG ("[Playlists] RecentlyPlayed count=" << (int) list.size());
            if (auto* hp = safe->mainArea ? safe->mainArea->getHomePage() : nullptr)
                hp->setRecentlyPlayedFromHistory(list);
        });
}

void MainComponent::setVenueId (const juce::String& venueId, bool requestInitialScan)
{
    const int startupToken = ++startupLoadToken_;
    showLoadingOverlay(requestInitialScan ? "Preparing venue and scanning songs..."
                                          : "Loading venue...",
                       0.05);

    activeVenueId_ = venueId;
    applyCurrentIdentityToUi();
    applyNavRoleForActiveVenue();

    if (venueId.isEmpty())
    {
        activeVenueName_.clear();
        pendingVenueCode_.clear();
        pendingVenueLogo_ = {};
        if (queueBar != nullptr)
            queueBar->setVenueInfo ("No Venue", "");
        if (lyricWindow_ != nullptr)
            lyricWindow_->setVenueContext ({}, {});
        ArchiveService::getInstance().stopNightlyCleanup();
        hideLoadingOverlay();
        return;
    }

    // Provisional state until the doc loads.
    if (queueBar != nullptr)
        queueBar->setVenueInfo ("Loading...", "");

    juce::Component::SafePointer<MainComponent> safe (this);

    VenueService::getInstance().loadVenue (venueId,
        [safe, requestInitialScan, venueId, startupToken] (bool ok, VenueItem v, juce::String error)
        {
            if (safe == nullptr || safe->startupLoadToken_ != startupToken || safe->activeVenueId_ != venueId)
                return;

            if (! ok)
            {
                DBG ("[Venue] load failed: " << error);
                if (safe->queueBar != nullptr)
                    safe->queueBar->setVenueInfo ("Venue unavailable", "");
                safe->hideLoadingOverlay();
                return;
            }

            safe->updateLoadingOverlay("Venue loaded. Loading playlists...", 0.20);

            const juce::String name (v.name);
            const juce::String code (v.code);
            const juce::String logoUrl (v.logoUrl);

            safe->activeVenueName_ = name;
            safe->pendingVenueCode_ = code;

            safe->activeVenueNumStrikes_ = v.numStrikes;

            auto sharedProgress = std::make_shared<int>(0);
            auto advanceProgress = [safe, venueId, startupToken, sharedProgress](const juce::String& step)
            {
                if (safe == nullptr || safe->startupLoadToken_ != startupToken || safe->activeVenueId_ != venueId)
                    return;

                ++(*sharedProgress);
                const double progress = 0.20 + 0.50 * ((double) *sharedProgress / 5.0);
                safe->updateLoadingOverlay(step, progress);
            };

            if (safe->queueBar != nullptr)
                safe->queueBar->setVenueInfo (name, code);

            if (safe->lyricWindow_ != nullptr)
            {
                safe->lyricWindow_->setVenueContext (venueId, name);
                if (auto* d = safe->lyricWindow_->getDisplay())
                    d->setVenueCode (code);
            }

            // Push the full venue snapshot into the Settings page so the admin
            // can edit any field. Saves are routed through MainArea's
            // onVenueSettingsChanged callback wired in the constructor.
            if (safe->mainArea != nullptr)
                safe->mainArea->setVenueData (v);

            safe->refreshSettingsUsers();
            safe->refreshSettingsInvitations();
            safe->refreshSettingsSessionStats();

            // Start the nightly archive + clear timer for this venue. The
            // service reads the configured cleanup hour from UserPreferences
            // each minute, so a settings change takes effect immediately.
            ArchiveService::getInstance().startNightlyCleanup(
                juce::String(v.id), juce::String(v.name));

            // Fetch the venue logo asynchronously and push it into the lyric
            // display once it arrives. ArtworkCache invokes the callback on
            // the message thread.
            if (logoUrl.isNotEmpty())
            {
                auto img = ArtworkCache::getInstance().getOrFetch (logoUrl,
                    [safe, logoUrl]
                    {
                        if (safe == nullptr) return;
                        auto loaded = ArtworkCache::getInstance().getOrFetch (logoUrl);
                        if (loaded.isValid())
                            safe->pendingVenueLogo_ = loaded;
                        if (loaded.isValid() && safe->lyricWindow_ != nullptr)
                            if (auto* d = safe->lyricWindow_->getDisplay())
                                d->setVenueLogo (loaded);
                    });

                if (img.isValid() && safe->lyricWindow_ != nullptr)
                    if (auto* d = safe->lyricWindow_->getDisplay())
                        d->setVenueLogo (img);
                if (img.isValid())
                    safe->pendingVenueLogo_ = img;
            }

            // Pull venue playlists (New / Popular / Recommended / history) into the
            // home page + membership caches, updating staged progress as each returns.
            auto& vs = VenueService::getInstance();
            const auto vid = juce::String(v.id);

            vs.getNewSongs(vid,
                [safe, venueId, startupToken, advanceProgress](bool ok, std::vector<Playlist> list, juce::String err) mutable
                {
                    if (safe == nullptr || safe->startupLoadToken_ != startupToken || safe->activeVenueId_ != venueId)
                        return;
                    if (! ok) DBG ("[Playlists] new load failed: " << err);
                    safe->newSongIds_.clear();
                    for (auto& p : list)
                        if (! p.id.empty()) safe->newSongIds_.insert(p.id);
                    advanceProgress("Loaded New Songs");
                });

            vs.getPlaylists(vid, "Popular",
                [safe, venueId, startupToken, advanceProgress](bool ok, std::vector<Playlist> list, juce::String err) mutable
                {
                    if (safe == nullptr || safe->startupLoadToken_ != startupToken || safe->activeVenueId_ != venueId)
                        return;
                    if (! ok) DBG ("[Playlists] Popular load failed: " << err);
                    safe->popularSongIds_.clear();
                    for (auto& p : list)
                        if (! p.id.empty()) safe->popularSongIds_.insert(p.id);
                    if (auto* hp = safe->mainArea ? safe->mainArea->getHomePage() : nullptr)
                        hp->setPopularSongs(list);
                    advanceProgress("Loaded Popular Songs");
                });

            vs.getPlaylists(vid, "Recommended",
                [safe, venueId, startupToken, advanceProgress](bool ok, std::vector<Playlist> list, juce::String err) mutable
                {
                    if (safe == nullptr || safe->startupLoadToken_ != startupToken || safe->activeVenueId_ != venueId)
                        return;
                    if (! ok) DBG ("[Playlists] Recommended load failed: " << err);
                    safe->recommendedSongIds_.clear();
                    for (auto& p : list)
                        if (! p.id.empty()) safe->recommendedSongIds_.insert(p.id);
                    if (auto* hp = safe->mainArea ? safe->mainArea->getHomePage() : nullptr)
                        hp->setRecommendedSongs(list);
                    advanceProgress("Loaded Recommended Songs");
                });

            vs.getRecentlyPlayed(vid,
                [safe, venueId, startupToken, advanceProgress](bool ok, std::vector<Playlist> list, juce::String err) mutable
                {
                    if (safe == nullptr || safe->startupLoadToken_ != startupToken || safe->activeVenueId_ != venueId)
                        return;
                    if (! ok) DBG ("[Playlists] RecentlyPlayed load failed: " << err);
                    if (auto* hp = safe->mainArea ? safe->mainArea->getHomePage() : nullptr)
                        hp->setRecentlyPlayedFromHistory(list);
                    advanceProgress("Loaded Recently Played");
                });

            // Venue switch path: if this load was triggered by the user
            // picking a different venue than the one configured on this PC,
            // navigate to the Library page and kick off the full song scan.
            if (requestInitialScan && safe->mainArea != nullptr)
                safe->mainArea->triggerInitialSongLoad();

            // Load the live queue for this venue from Firestore and push it
            // into the QueueBar (replaces the placeholder/empty state).
            QueueService::getInstance().loadQueue (vid,
                [safe, vid, venueId, startupToken, advanceProgress] (bool qok, QueueService::Snapshot snap, juce::String qerr) mutable
                {
                    if (safe == nullptr || safe->queueBar == nullptr || safe->startupLoadToken_ != startupToken || safe->activeVenueId_ != venueId)
                        return;

                    if (! qok)
                    {
                        DBG ("[Queue] load failed: " << qerr);
                        safe->queueBar->clearNowPlaying();
                        safe->queueBar->setSingers ({});
                        advanceProgress("Queue unavailable");
                        safe->startDeferredAudioServices(venueId, startupToken);
                        return;
                    }

                    if (snap.hasNowPlaying)
                    {
                        safe->queueBar->setNowPlaying (snap.nowPlaying);
                        safe->localNowPlaying_    = snap.nowPlaying;
                        safe->hasLocalNowPlaying_ = true;
                    }
                    else if (safe->hasLocalNowPlaying_)
                    {
                        safe->queueBar->setNowPlaying (safe->localNowPlaying_);
                    }
                    else
                    {
                        safe->queueBar->clearNowPlaying();
                    }

                    safe->queueBar->setSingers (safe->composeQueueWithHost(snap.singers));

                    // Ensure persisted queue order is canonical at startup so
                    // relaunches and mobile clients resume the same round state.
                    QueueService::getInstance().persistSingerOrder(vid, snap.singers,
                        [](bool ok, juce::String err)
                        {
                            if (! ok)
                                DBG("[Queue] startup persistSingerOrder failed: " << err);
                        });

                    // Start (or restart) the /requested polling pipeline so
                    // we route TAGG requests through autoApprove and into
                    // /queue.
                    safe->startRequestPipelineFor (vid);
                        advanceProgress("Loaded Queue");
                        safe->startDeferredAudioServices(venueId, startupToken);
                });
        });
}

void MainComponent::applyCurrentIdentityToUi()
{
    if (topBar == nullptr)
        return;

    juce::String displayName;
    UserRole hostRole = UserRole::Host;

    if (HostService::getInstance().hasCurrent())
    {
        const auto host = HostService::getInstance().getCurrent();
        hostRole = host.role;

        displayName = juce::String(host.stageName).trim();
        if (displayName.isEmpty())
            displayName = juce::String(host.fullName).trim();
    }

    if (displayName.isEmpty())
    {
        const auto& fc = FirestoreClient::getInstance();
        displayName = fc.getDisplayName().trim();
        if (displayName.isEmpty())
            displayName = fc.getEmail().trim();
    }

    topBar->setUserInfo(displayName, juce::Image());

    // If no venue is active yet, at least expose host-level rights.
    if (navBar != nullptr && activeVenueId_.isEmpty())
        navBar->setUserRole(hostRole);
}

void MainComponent::applyNavRoleForActiveVenue()
{
    if (navBar == nullptr)
        return;

    UserRole fallbackRole = UserRole::Host;
    if (HostService::getInstance().hasCurrent())
        fallbackRole = HostService::getInstance().getCurrent().role;

    if (activeVenueId_.isEmpty())
    {
        navBar->setUserRole(fallbackRole);
        return;
    }

    const auto venueId = activeVenueId_;
    const auto userId = FirestoreClient::getInstance().getUserId().trim();
    const auto userEmail = FirestoreClient::getInstance().getEmail().trim().toLowerCase();

    if (userId.isEmpty() && userEmail.isEmpty())
    {
        navBar->setUserRole(fallbackRole);
        return;
    }

    juce::Component::SafePointer<MainComponent> safe(this);
    juce::Thread::launch([safe, venueId, userId, userEmail, fallbackRole]
    {
        UserRole resolvedRole = fallbackRole;
        bool matchedAssociation = false;

        auto docs = FirestoreClient::getInstance().listCollection("user-venue-lookup", 1000);
        for (auto& d : docs)
        {
            if (FirestoreClient::readString(d, "venueId") != venueId)
                continue;

            const auto status = FirestoreClient::readString(d, "status").trim();
            if (status.equalsIgnoreCase("removed"))
                continue;
            if (status.isNotEmpty() && !status.equalsIgnoreCase("active"))
                continue;

            const auto docUserId = FirestoreClient::readString(d, "userId").trim();
            const auto email = FirestoreClient::readString(d, "userEmail").trim().toLowerCase();
            const bool idMatch = userId.isNotEmpty() && docUserId == userId;
            const bool emailMatch = userEmail.isNotEmpty() && email == userEmail;
            if (!idMatch && !emailMatch)
                continue;

            auto role = FirestoreClient::readString(d, "role").trim();
            if (role.equalsIgnoreCase("Enterprise Admin"))
                role = "EnterpriseAdmin";

            resolvedRole = AccessRightsUtil::stringToUserRole(role.toStdString());
            matchedAssociation = true;
            break;
        }

        juce::MessageManager::callAsync([safe, venueId, resolvedRole, matchedAssociation]()
        {
            if (safe == nullptr || safe->navBar == nullptr)
                return;
            if (safe->activeVenueId_ != venueId)
                return;

            safe->navBar->setUserRole(resolvedRole);
        });
    });
}

void MainComponent::refreshSettingsUsers()
{
    if (activeVenueId_.isEmpty())
        return;
    if (mainArea == nullptr || mainArea->getSettingsPage() == nullptr)
        return;

    juce::Component::SafePointer<MainComponent> safe(this);
    const auto venueId = activeVenueId_;
    juce::Thread::launch([safe, venueId]
    {
        std::vector<SettingsPage::VenueUser> users;
        auto docs = FirestoreClient::getInstance().listCollection("user-venue-lookup", 1000);
        for (auto& d : docs)
        {
            if (FirestoreClient::readString(d, "venueId") != venueId)
                continue;

            const auto status = FirestoreClient::readString(d, "status");
            if (status == "removed")
                continue;

            SettingsPage::VenueUser u;
            u.email = FirestoreClient::readString(d, "userEmail");
            if (u.email.isEmpty())
                continue;

            const auto role = FirestoreClient::readString(d, "role");
            if (role.equalsIgnoreCase("Host"))
                u.role = "Host";
            else if (role.equalsIgnoreCase("Admin"))
                u.role = "Admin";
            else if (role.equalsIgnoreCase("Tester"))
                u.role = "Tester";
            else if (role.equalsIgnoreCase("EnterpriseAdmin") || role.equalsIgnoreCase("Enterprise Admin"))
                u.role = "EnterpriseAdmin";
            else
                u.role = "Basic";
            u.active = (status == "active");
            users.push_back(std::move(u));
        }

        std::sort(users.begin(), users.end(), [](const SettingsPage::VenueUser& a, const SettingsPage::VenueUser& b)
        {
            return a.email.compareIgnoreCase(b.email) < 0;
        });

        juce::MessageManager::callAsync([safe, users = std::move(users)]() mutable
        {
            if (safe == nullptr || safe->mainArea == nullptr)
                return;
            if (auto* sp = safe->mainArea->getSettingsPage())
                sp->setUserList(users);
        });
    });
}

void MainComponent::refreshSettingsInvitations()
{
    if (activeVenueId_.isEmpty())
        return;
    if (mainArea == nullptr || mainArea->getSettingsPage() == nullptr)
        return;

    juce::Component::SafePointer<MainComponent> safe(this);
    const auto venueId = activeVenueId_;
    juce::Thread::launch([safe, venueId]
    {
        std::vector<SettingsPage::PendingInvitation> invitesOut;
        auto& fc = FirestoreClient::getInstance();
        auto invites = fc.listCollection("venueInvitations", 1000);
        auto associations = fc.listCollection("user-venue-lookup", 1000);
        const auto now = juce::Time::getCurrentTime();
        int autoAcceptedCount = 0;
        int autoExpiredCount = 0;

        std::unordered_set<std::string> activeEmails;
        for (auto& d : associations)
        {
            if (FirestoreClient::readString(d, "venueId") != venueId)
                continue;
            if (!FirestoreClient::readString(d, "status").equalsIgnoreCase("active"))
                continue;

            const auto email = FirestoreClient::readString(d, "userEmail").trim().toLowerCase();
            if (email.isNotEmpty())
                activeEmails.insert(email.toStdString());
        }

        auto getRelPathFromName = [](const juce::var& doc) -> juce::String
        {
            const auto fullName = doc.getProperty("name", juce::var()).toString();
            const auto marker = "/documents/";
            const auto idx = fullName.indexOf(marker);
            if (idx < 0)
                return {};
            return fullName.substring(idx + (int) std::strlen(marker));
        };

        for (auto& inv : invites)
        {
            if (FirestoreClient::readString(inv, "venueId") != venueId)
                continue;

            const auto email = FirestoreClient::readString(inv, "invitedUserEmail").trim().toLowerCase();
            if (email.isEmpty())
                continue;

            bool isAccepted = FirestoreClient::readBool(inv, "isAccepted", false);
            bool isExpired = FirestoreClient::readBool(inv, "isExpired", false);

            const auto expirationDate = FirestoreClient::readTime(inv, "expirationDate");
            const bool expiredByTime = (expirationDate > juce::Time() && expirationDate < now);
            const bool hasActiveAssociation = activeEmails.count(email.toStdString()) > 0;

            const auto relPath = getRelPathFromName(inv);

            if (! isAccepted && hasActiveAssociation && relPath.isNotEmpty())
            {
                const auto path = relPath
                    + "?updateMask.fieldPaths=isAccepted"
                    + "&updateMask.fieldPaths=acceptedDate";
                const auto fields = FirestoreClient::makeFields({
                    { "isAccepted",  FirestoreClient::booleanValue(true) },
                    { "acceptedDate", FirestoreClient::timestampValue(now) }
                });
                fc.patchDocument(path, fields);
                isAccepted = true;
                ++autoAcceptedCount;
            }

            if (! isAccepted && ! isExpired && expiredByTime && relPath.isNotEmpty())
            {
                const auto path = relPath + "?updateMask.fieldPaths=isExpired";
                const auto fields = FirestoreClient::makeFields({
                    { "isExpired", FirestoreClient::booleanValue(true) }
                });
                fc.patchDocument(path, fields);
                isExpired = true;
                ++autoExpiredCount;
            }

            if (isAccepted)
                continue;

            SettingsPage::PendingInvitation pending;
            pending.email = email;
            pending.role = FirestoreClient::readString(inv, "role");
            pending.expirationDate = expirationDate;
            pending.expired = isExpired || expiredByTime;

            if (pending.role.equalsIgnoreCase("Enterprise Admin"))
                pending.role = "EnterpriseAdmin";
            if (pending.role.isEmpty())
                pending.role = "Basic";

            invitesOut.push_back(std::move(pending));
        }

        std::sort(invitesOut.begin(), invitesOut.end(), [](const SettingsPage::PendingInvitation& a,
                                                           const SettingsPage::PendingInvitation& b)
        {
            return a.email.compareIgnoreCase(b.email) < 0;
        });

        juce::MessageManager::callAsync([safe, invitesOut = std::move(invitesOut), autoAcceptedCount, autoExpiredCount]() mutable
        {
            if (safe == nullptr || safe->mainArea == nullptr)
                return;
            if (auto* sp = safe->mainArea->getSettingsPage())
                sp->setPendingInvitations(invitesOut);

            if (autoAcceptedCount > 0 || autoExpiredCount > 0)
            {
                juce::StringArray parts;
                if (autoAcceptedCount > 0)
                    parts.add(juce::String(autoAcceptedCount) + " invite" + (autoAcceptedCount == 1 ? "" : "s") + " accepted");
                if (autoExpiredCount > 0)
                    parts.add(juce::String(autoExpiredCount) + " invite" + (autoExpiredCount == 1 ? "" : "s") + " expired");
                safe->showMaintenanceToast("Invitation maintenance: " + parts.joinIntoString(", "));
            }
        });
    });
}

void MainComponent::refreshSettingsSessionStats()
{
    if (activeVenueId_.isEmpty())
        return;
    if (mainArea == nullptr || mainArea->getSettingsPage() == nullptr)
        return;

    juce::Component::SafePointer<MainComponent> safe(this);
    VenueService::getInstance().checkExistingSessionData(activeVenueId_,
        [safe](bool ok, VenueService::SessionCounts counts, juce::String /*error*/)
        {
            if (safe == nullptr || ! ok || safe->mainArea == nullptr)
                return;

            const auto venueId = safe->activeVenueId_;
            juce::Thread::launch([safe, counts, venueId]
            {
                int songsToday = 0;
                int activeMembers = 0;
                auto docs = FirestoreClient::getInstance().listCollection("venues/" + venueId + "/playHistory", 500);
                auto associationDocs = FirestoreClient::getInstance().listCollection("user-venue-lookup", 1000);

                auto now = juce::Time::getCurrentTime();
                auto startOfDay = juce::Time(now.getYear(), now.getMonth(), now.getDayOfMonth(), 0, 0, 0, 0, false);
                const auto startMs = startOfDay.toMilliseconds();
                const auto endMs = startMs + 24LL * 60LL * 60LL * 1000LL;

                for (auto& d : docs)
                {
                    auto fields = d.getProperty("fields", juce::var());
                    auto playedAtField = fields.getProperty("playedAt", juce::var());
                    const auto playedAt = playedAtField.getProperty("integerValue", "0").toString().getLargeIntValue();
                    if (playedAt >= startMs && playedAt < endMs)
                        ++songsToday;
                }

                for (auto& d : associationDocs)
                {
                    if (FirestoreClient::readString(d, "venueId") != venueId)
                        continue;
                    if (!FirestoreClient::readString(d, "status").equalsIgnoreCase("active"))
                        continue;
                    ++activeMembers;
                }

                juce::MessageManager::callAsync([safe, counts, songsToday, activeMembers]
                {
                    if (safe == nullptr || safe->mainArea == nullptr)
                        return;

                    SettingsPage::SessionStats stats;
                    stats.songsPlayedToday = songsToday;
                    stats.activeMembers = activeMembers;
                    stats.singersInQueue = counts.queueCount;
                    stats.requestedSongs = counts.requestedCount;

                    if (auto* sp = safe->mainArea->getSettingsPage())
                        sp->setSessionStats(stats);
                });
            });
        });
}

//==============================================================================
// /requested pipeline
void MainComponent::startRequestPipelineFor (const juce::String& venueId)
{
    activeVenueId_ = venueId;
    juce::Component::SafePointer<MainComponent> safe (this);

    auto& rs = RequestService::getInstance();
    rs.onNewRequest      = [safe](const QueueItem& item) { if (safe != nullptr) safe->onIncomingNewRequest(item); };
    rs.onApprovedRequest = [safe](const QueueItem& item) { if (safe != nullptr) safe->onIncomingApprovedRequest(item); };
    rs.onRejectedRequest = [safe](const QueueItem& item) { if (safe != nullptr) safe->onIncomingRejectedRequest(item); };
    rs.onDeleteRequest   = [safe](const QueueItem& item) { if (safe != nullptr) safe->onIncomingDeleteRequest(item); };
    rs.start (venueId);

    // Watch /queue itself for changes pushed by other clients (e.g. TAGG
    // mobile app reordering songs, marking a song as playing, etc.). The
    // watcher uses a fingerprint to repaint only when something actually
    // changed.
    QueueService::getInstance().startWatching (venueId,
        [safe] (QueueService::Snapshot snap)
        {
            if (safe == nullptr || safe->queueBar == nullptr)
                return;

            if (snap.hasNowPlaying)
            {
                safe->queueBar->setNowPlaying (snap.nowPlaying);
                safe->localNowPlaying_    = snap.nowPlaying;
                safe->hasLocalNowPlaying_ = true;
            }
            else if (safe->hasLocalNowPlaying_)
            {
                safe->queueBar->setNowPlaying (safe->localNowPlaying_);
            }
            else
            {
                safe->queueBar->clearNowPlaying();
            }

            auto composed = safe->composeQueueWithHost (snap.singers);
            safe->queueBar->setSingers (composed);
            safe->syncLyricIdlePreview (composed);
        });
}

void MainComponent::reloadQueueFromFirestore (const juce::String& venueId)
{
    if (venueId.isEmpty() || queueBar == nullptr)
        return;

    juce::Component::SafePointer<MainComponent> safe (this);
    QueueService::getInstance().loadQueue (venueId,
        [safe] (bool ok, QueueService::Snapshot snap, juce::String /*err*/)
        {
            if (safe == nullptr || safe->queueBar == nullptr || ! ok)
                return;

            if (snap.hasNowPlaying)
            {
                safe->queueBar->setNowPlaying (snap.nowPlaying);
                safe->localNowPlaying_    = snap.nowPlaying;
                safe->hasLocalNowPlaying_ = true;
            }
            else if (safe->hasLocalNowPlaying_)
            {
                safe->queueBar->setNowPlaying (safe->localNowPlaying_);
            }
            else
            {
                safe->queueBar->clearNowPlaying();
            }

            auto composed = safe->composeQueueWithHost (snap.singers);
            safe->queueBar->setSingers (composed);
            safe->syncLyricIdlePreview (composed);
        });
}

void MainComponent::onIncomingNewRequest (const QueueItem& item)
{
    // Mirrors autoApproveSong() in queue-bar.component.ts.
    const auto venue = VenueService::getInstance().getCurrent();
    const juce::String venueId = activeVenueId_;
    if (venueId.isEmpty())
        return;

    // Local desktop request — bypass checks and add straight to queue.
    if (juce::String(item.deviceId).equalsIgnoreCase("local"))
    {
        DBG ("[Pipeline] new(local) -> approve & enqueue: " << juce::String(item.songName));
        QueueItem approved = item;
        approved.status = "approved";
        juce::Component::SafePointer<MainComponent> safe (this);
        QueueService::getInstance().appendSong (venueId, approved,
            [safe, id = juce::String(item.id), venueId](bool ok, juce::String /*err*/)
            {
                RequestService::getInstance().deleteRequested (venueId, id);
                if (safe != nullptr && ok)
                    safe->reloadQueueFromFirestore (venueId);
            });
        return;
    }

    // Queue closed?
    if (queueBar != nullptr && queueBar->isQueueClosed())
    {
        DBG ("[Pipeline] new -> reject (queue closed): " << juce::String(item.songName));
        RequestService::getInstance().patchStatus (venueId, juce::String(item.id),
            "rejected", "No longer accepting song requests.  Please come back next time!");
        return;
    }

    // Max songs per singer.
    const int  maxSongs    = juce::jmax (1, venue.numSongs);
    const bool allowRepeat = venue.repeatSongs;

    if (queueBar != nullptr)
    {
        const auto& singers = queueBar->getSingers();
        const auto wantSinger = juce::String(item.singerName).toLowerCase();
        const auto wantSong   = juce::String(item.songName).toLowerCase();
        const auto wantArtist = juce::String(item.songArtist).toLowerCase();

        for (auto& s : singers)
        {
            if (juce::String(s.name).toLowerCase() != wantSinger)
                continue;
            if ((int) s.songs.size() >= maxSongs)
            {
                DBG ("[Pipeline] new -> reject (too many songs): " << juce::String(item.songName));
                RequestService::getInstance().patchStatus (venueId, juce::String(item.id),
                    "rejected", "You have too many songs in the queue.  Sing or delete one before adding more");
                return;
            }
            break;
        }

        if (! allowRepeat)
        {
            for (auto& s : singers)
            {
                for (auto& q : s.songs)
                {
                    if (juce::String(q.songName).toLowerCase()   == wantSong
                     && juce::String(q.songArtist).toLowerCase() == wantArtist)
                    {
                        DBG ("[Pipeline] new -> reject (duplicate in queue): " << juce::String(item.songName));
                        RequestService::getInstance().patchStatus (venueId, juce::String(item.id),
                            "rejected", "This song is already in the queue.  Please choose another song");
                        return;
                    }
                }
            }
        }
    }

    // All checks passed — add to queue now and mark the /requested doc
    // "approved" so the mobile app receives its confirmation notification.
    QueueItem approved = item;
    approved.status = "approved";

    DBG ("[Pipeline] new -> approved -> enqueue: " << juce::String(item.songName)
         << " (singer=" << juce::String(item.singerName) << ")");

    juce::Component::SafePointer<MainComponent> safe (this);
    QueueService::getInstance().appendSong (venueId, approved,
        [safe, id = juce::String(item.id), venueId](bool ok, juce::String err)
        {
            if (ok)
            {
                // Delete the /requested doc so the RequestService poll does NOT
                // see it flip to "approved" and fire onApprovedRequest again
                // (which would call appendSong a second time and create a
                // duplicate).  The mobile app confirms approval by watching the
                // queue collection directly.
                RequestService::getInstance().deleteRequested (venueId, id);
                if (safe != nullptr)
                    safe->reloadQueueFromFirestore (venueId);
            }
            else
            {
                DBG ("[Pipeline] appendSong failed: " << err);
                RequestService::getInstance().patchStatus (venueId, id,
                    "rejected", "Server error \xe2\x80\x94 please try again.");
            }
        });
}

void MainComponent::onIncomingApprovedRequest (const QueueItem& item)
{
    const juce::String venueId = activeVenueId_;
    if (venueId.isEmpty())
        return;

    DBG ("[Pipeline] approved -> enqueue: " << juce::String(item.songName)
         << " (singer=" << juce::String(item.singerName) << ")");

    juce::Component::SafePointer<MainComponent> safe (this);
    QueueService::getInstance().appendSong (venueId, item,
        [safe, id = juce::String(item.id), venueId](bool ok, juce::String /*err*/)
        {
            RequestService::getInstance().deleteRequested (venueId, id);
            if (safe != nullptr && ok)
                safe->reloadQueueFromFirestore (venueId);
        });
}

void MainComponent::onIncomingRejectedRequest (const QueueItem& item)
{
    const juce::String venueId = activeVenueId_;
    if (venueId.isEmpty())
        return;

    DBG ("[Pipeline] rejected -> remove from /requested: " << juce::String(item.songName));
    RequestService::getInstance().deleteRequested (venueId, juce::String(item.id));
}

void MainComponent::onIncomingDeleteRequest (const QueueItem& item)
{
    const juce::String venueId = activeVenueId_;
    if (venueId.isEmpty())
        return;

    DBG ("[Pipeline] delete -> remove from /queue + /requested: " << juce::String(item.songName));

    juce::Component::SafePointer<MainComponent> safe (this);
    QueueService::getInstance().removeSong (venueId, item,
        [safe, id = juce::String(item.id), venueId](bool /*ok*/, juce::String /*err*/)
        {
            RequestService::getInstance().deleteRequested (venueId, id);
            if (safe != nullptr)
                safe->reloadQueueFromFirestore (venueId);
        });
}

void MainComponent::wireTestingPageCallbacks()
{
    if (mainArea == nullptr)
        return;

    auto* testing = mainArea->getTestingPage();
    if (testing == nullptr)
        return;

    testing->onApplyResolution = [this](int width, int height)
    {
        if (auto* topLevel = findParentComponentOfClass<juce::TopLevelWindow>())
        {
            topLevel->setSize(width, height);
            topLevel->centreWithSize(width, height);
        }
    };

    testing->onCreateQueue = [this](const TestingPage::SeedOptions& options,
                                    std::function<void(float)> onProgress,
                                    std::function<void(bool, juce::String)> onDone)
    {
        seedTestingQueue(options, std::move(onProgress), std::move(onDone));
    };
}

void MainComponent::seedTestingQueue(const TestingPage::SeedOptions& options,
                                     std::function<void(float)> progressCallback,
                                     std::function<void(bool, juce::String)> doneCallback)
{
    const auto venueId = activeVenueId_;
    if (venueId.isEmpty())
    {
        if (doneCallback) doneCallback(false, "No active venue. Select a venue first.");
        return;
    }

    const auto songs = mainArea != nullptr ? mainArea->getLibrarySongs()
                                           : std::vector<CdgSong>{};
    if (songs.empty())
    {
        if (doneCallback) doneCallback(false, "No library songs found. Import songs before creating test queue.");
        return;
    }

    juce::Thread::launch([venueId, songs, options,
                         progressCb = std::move(progressCallback),
                         doneCb = std::move(doneCallback)]() mutable
    {
        std::mt19937 rng((unsigned int) juce::Time::getMillisecondCounter());

        std::vector<int> singerTypes;
        singerTypes.reserve((size_t) (options.numEncoreSingers + options.numMobileSingers));
        singerTypes.insert(singerTypes.end(), (size_t) options.numEncoreSingers, 0); // Encore/manual
        singerTypes.insert(singerTypes.end(), (size_t) options.numMobileSingers, 1); // Mobile/tagg

        if (singerTypes.empty())
        {
            if (doneCb) doneCb(false, "Nothing to create. Increase singer counts above zero.");
            return;
        }

        std::shuffle(singerTypes.begin(), singerTypes.end(), rng);

        std::uniform_int_distribution<int> songsPerSingerDist(options.numSongsMin, options.numSongsMax);
        std::uniform_int_distribution<int> songIndexDist(0, juce::jmax(0, (int) songs.size() - 1));

        static const char* kIcons[] = {
            "assets/icon/1064391.png", "assets/icon/1082581.png", "assets/icon/2015468.png",
            "assets/icon/2345434.png", "assets/icon/2587741.png", "assets/icon/3214567.png",
            "assets/icon/3457912.png", "assets/icon/3852556.png", "assets/icon/4568752.png",
            "assets/icon/5238382.png", "assets/icon/6319385.png", "assets/icon/7463548.png",
            "assets/icon/8032015.png", "assets/icon/8745632.png", "assets/icon/9517532.png"
        };
        constexpr int kIconCount = (int) (sizeof(kIcons) / sizeof(kIcons[0]));
        std::uniform_int_distribution<int> iconIndexDist(0, kIconCount - 1);

        int encoreCreated = 0;
        int mobileCreated = 0;
        int writeFailures = 0;

        for (size_t i = 0; i < singerTypes.size(); ++i)
        {
            const bool isMobile = singerTypes[i] == 1;
            const juce::String singerName = isMobile
                ? ("MobileSinger" + juce::String(mobileCreated++))
                : ("EncoreSinger" + juce::String(encoreCreated++));

            const juce::String avatar = kIcons[iconIndexDist(rng)];
            const int songCount = songsPerSingerDist(rng);

            for (int s = 0; s < songCount; ++s)
            {
                const auto& song = songs[(size_t) songIndexDist(rng)];

                juce::String version;
                if (!song.version.empty())
                    version = juce::String(song.version[0]);

                const int semitones = randomPitchSemitones(rng, options.randomPitch);

                QueueItem item;
                item.id          = juce::Uuid().toString().toStdString();
                item.profileId   = "";
                item.foxId       = "";
                item.deviceId    = isMobile ? "mobiletest" : "local";
                item.singerName  = singerName.toStdString();
                item.singerAvatar= avatar.toStdString();
                item.songId      = song.id;
                item.songName    = song.songName;
                item.songArtist  = song.artistName;
                item.songVersion = version.toStdString();
                item.duration    = song.durationMS > 0 ? song.durationMS / 1000 : 240;
                item.pitch       = (float) semitones;
                item.key         = semitones;
                item.status      = isMobile ? "new" : "approved";
                item.order       = 0;
                item.songOrder   = 0;
                item.time        = "0:00";
                item.reason      = "";
                item.action      = "new";
                item.dateAdded   = juce::Time::getCurrentTime().toMilliseconds();

                juce::String err;
                const bool ok = isMobile
                    ? createRequestedSong(venueId, item, err)
                    : appendSongSync(venueId, item, err);

                if (!ok)
                {
                    ++writeFailures;
                    DBG("[TestingPage] failed to write test queue item: " << err);
                }
            }

            if (progressCb)
                progressCb((float) (i + 1) / (float) singerTypes.size());
        }

        if (doneCb)
        {
            if (writeFailures == 0)
            {
                doneCb(true, "Created test singers and songs successfully.");
            }
            else
            {
                doneCb(false, "Finished with " + juce::String(writeFailures)
                               + " write failures. Check logs for details.");
            }
        }
    });
}

//==============================================================================
void MainComponent::logPlayHistoryIfNeeded(bool naturalEnd)
{
    if (playStartTimeMs_ == 0) return;

    if (currentSong.songName.empty() && currentSong.artistName.empty()) return;

    if (! naturalEnd)
    {
        const auto elapsedMs = juce::Time::currentTimeMillis() - playStartTimeMs_;
        if (elapsedMs < 30000) return;
    }

    const auto venueId = activeVenueId_;
    if (venueId.isEmpty()) return;

    VenueService::PlayHistoryEntry entry;
    entry.songId     = currentSong.id;
    entry.songName   = currentSong.songName;
    entry.artistName = currentSong.artistName;
    entry.imageUrl   = currentSong.imageUrl;
    entry.singerName = hasLocalNowPlaying_ ? localNowPlaying_.name : "Unknown";

    playStartTimeMs_ = 0; // Reset before the async write to prevent duplicate entries.

    VenueService::getInstance().addPlayHistory(entry,
        [venueId](bool ok, juce::String err)
        {
            if (! ok) DBG("[History] addPlayHistory failed: " << err);
        });
}
