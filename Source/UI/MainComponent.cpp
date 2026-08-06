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
#include "../Services/QueueRotation.h"
#include "../Services/RequestService.h"
#include "../Services/EmojiService.h"
#include "../Services/HostService.h"
#include "../Services/SongDatabase.h"
#include "../Services/ImageCache.h"
#include "../Services/FirestoreClient.h"
#include "../Services/SongbookStorageService.h"
#include "../Services/ArchiveService.h"
#include "../Services/VenueSessionService.h"
#include "../Services/ApiService.h"
#include "../Services/UpdateService.h"
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
bool writeZipEntryToFile(juce::ZipFile& zip, int index, const juce::File& target)
{
    auto input = zip.createStreamForEntry(index);
    if (input == nullptr)
        return false;

    target.getParentDirectory().createDirectory();
    auto output = target.createOutputStream();
    if (output == nullptr)
        return false;

    output->writeFromInputStream(*input, -1);
    output->flush();
    return true;
}

bool extractZipMediaFiles(const juce::File& zipFile,
                          juce::File& audioOut,
                          juce::File& cdgOut,
                          juce::String& errorOut)
{
    if (! zipFile.existsAsFile())
    {
        errorOut = "ZIP archive not found.";
        return false;
    }

    juce::ZipFile zip(zipFile);
    if (zip.getNumEntries() <= 0)
    {
        errorOut = "ZIP archive is empty or unreadable.";
        return false;
    }

    static const juce::StringArray audioExts { ".mp3", ".wav", ".ogg", ".flac", ".aac", ".m4a" };

    int audioIndex = -1;
    int audioRank = std::numeric_limits<int>::max();
    juce::String audioStem;
    int firstCdgIndex = -1;
    int matchingCdgIndex = -1;

    for (int i = 0; i < zip.getNumEntries(); ++i)
    {
        auto* entry = zip.getEntry(i);
        if (entry == nullptr)
            continue;

        const auto entryName = juce::File(entry->filename).getFileName();
        if (entryName.isEmpty())
            continue;

        const auto ext = juce::File(entryName).getFileExtension().toLowerCase();
        const auto stem = juce::File(entryName).getFileNameWithoutExtension().toLowerCase();

        if (ext == ".cdg")
        {
            if (firstCdgIndex < 0)
                firstCdgIndex = i;
            if (audioStem.isNotEmpty() && stem == audioStem && matchingCdgIndex < 0)
                matchingCdgIndex = i;
            continue;
        }

        const int rank = audioExts.indexOf(ext);
        if (rank >= 0 && rank < audioRank)
        {
            audioIndex = i;
            audioRank = rank;
            audioStem = stem;
            if (firstCdgIndex >= 0)
            {
                auto* firstCdgEntry = zip.getEntry(firstCdgIndex);
                if (firstCdgEntry != nullptr)
                {
                    const auto firstCdgStem = juce::File(firstCdgEntry->filename).getFileNameWithoutExtension().toLowerCase();
                    if (firstCdgStem == audioStem)
                        matchingCdgIndex = firstCdgIndex;
                }
            }
        }
    }

    if (audioIndex < 0)
    {
        errorOut = "ZIP archive does not contain a supported audio file.";
        return false;
    }

    const auto cacheRoot = juce::File::getSpecialLocation(juce::File::tempDirectory)
        .getChildFile("EncoreKaraoke")
        .getChildFile("zip-cache")
        .getChildFile(zipFile.getFileNameWithoutExtension() + "-" + juce::String::toHexString(zipFile.getFullPathName().hashCode()));
    cacheRoot.createDirectory();

    auto* audioEntry = zip.getEntry(audioIndex);
    if (audioEntry == nullptr)
    {
        errorOut = "ZIP archive audio entry is unreadable.";
        return false;
    }

    audioOut = cacheRoot.getChildFile(juce::File(audioEntry->filename).getFileName());
    if (! writeZipEntryToFile(zip, audioIndex, audioOut))
    {
        errorOut = "Failed to extract audio file from ZIP archive.";
        return false;
    }

    const int cdgIndex = matchingCdgIndex >= 0 ? matchingCdgIndex : firstCdgIndex;
    if (cdgIndex >= 0)
    {
        if (auto* cdgEntry = zip.getEntry(cdgIndex))
        {
            cdgOut = cacheRoot.getChildFile(juce::File(cdgEntry->filename).getFileName());
            if (! writeZipEntryToFile(zip, cdgIndex, cdgOut))
                cdgOut = juce::File{};
        }
    }

    return true;
}

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

juce::File resolveAssetFile(const juce::String& relativePath)
{
    auto exeDir = juce::File::getSpecialLocation(juce::File::currentExecutableFile).getParentDirectory();
    auto cwd = juce::File::getCurrentWorkingDirectory();

    const juce::Array<juce::File> roots {
        cwd,
        exeDir,
        exeDir.getParentDirectory(),
        exeDir.getParentDirectory().getParentDirectory(),
        exeDir.getParentDirectory().getParentDirectory().getParentDirectory()
    };

    for (const auto& root : roots)
    {
        auto candidate = root.getChildFile(relativePath);
        if (candidate.existsAsFile())
            return candidate;
    }

    return {};
}

int findHostIndexInQueue(const std::vector<Singers>& singers)
{
    for (int i = 0; i < (int) singers.size(); ++i)
        if (singers[(size_t) i].isHost)
            return i;
    return -1;
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
// Small persistent, dismissible banner ("Update available — Restart Now /
// Later"). Deliberately not modal and not auto-dismissing like
// showMaintenanceToast()'s transient label — an update notice needs to stay
// visible with actionable buttons until the KJ chooses, but must never block
// input to the rest of the app.
class MainComponent::UpdateBanner : public juce::Component
{
public:
    UpdateBanner()
    {
        messageLabel_.setJustificationType(juce::Justification::centredLeft);
        messageLabel_.setColour(juce::Label::textColourId, juce::Colours::white);
        messageLabel_.setFont(juce::Font(juce::FontOptions().withHeight(13.0f)));
        addAndMakeVisible(messageLabel_);

        restartButton_.setButtonText(LocalizationManager::getInstance().getText("update.btn_restart_now"));
        restartButton_.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff2f6fed));
        restartButton_.onClick = [this] { if (onRestartClicked) onRestartClicked(); };
        addAndMakeVisible(restartButton_);

        laterButton_.setButtonText(LocalizationManager::getInstance().getText("update.btn_later"));
        laterButton_.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff2a3445));
        laterButton_.onClick = [this] { if (onLaterClicked) onLaterClicked(); };
        addAndMakeVisible(laterButton_);

        setVisible(false);
    }

    void setMessage(const juce::String& msg) { messageLabel_.setText(msg, juce::dontSendNotification); }

    void paint(juce::Graphics& g) override
    {
        auto area = getLocalBounds().toFloat();
        g.setColour(juce::Colour(0xff1a2030));
        g.fillRoundedRectangle(area, 8.0f);
        g.setColour(juce::Colour(0xff30daff));
        g.drawRoundedRectangle(area.reduced(0.5f), 8.0f, 1.2f);
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced(8, 4);
        laterButton_.setBounds(area.removeFromRight(70));
        area.removeFromRight(6);
        restartButton_.setBounds(area.removeFromRight(120));
        area.removeFromRight(10);
        messageLabel_.setBounds(area);
    }

    std::function<void()> onRestartClicked;
    std::function<void()> onLaterClicked;

private:
    juce::Label messageLabel_;
    juce::TextButton restartButton_;
    juce::TextButton laterButton_;
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

    bgPlayer_ = std::make_unique<BackgroundMusicPlayer>();
    bgPlayer_->initialize();
    DBG("[Startup] BackgroundMusicPlayer initialized: "
        + juce::String(juce::Time::getMillisecondCounterHiRes() - ctorStartMs, 1) + " ms");

    bgPlayer_->onTrackChanged = [this]()
    {
        juce::Component::SafePointer<MainComponent> safe (this);
        juce::MessageManager::callAsync ([safe]()
        {
            if (safe == nullptr) return;
            if (safe->ribbonMenu != nullptr && safe->bgPlayer_ != nullptr)
            {
                safe->ribbonMenu->setBackgroundTrackInfo (safe->bgPlayer_->getCurrentTrackName(),
                                                    safe->bgPlayer_->getCurrentPosition(),
                                                    safe->bgPlayer_->getTotalLength());
                safe->ribbonMenu->setBackgroundState (safe->bgPlayer_->isPlaying(), safe->bgPlayer_->getVolume());
            }
        });
    };

    bgPlayer_->onPlayStateChanged = [this]()
    {
        juce::Component::SafePointer<MainComponent> safe (this);
        juce::MessageManager::callAsync ([safe]()
        {
            if (safe == nullptr) return;
            if (safe->ribbonMenu != nullptr && safe->bgPlayer_ != nullptr)
                safe->ribbonMenu->setBackgroundState (safe->bgPlayer_->isPlaying(), safe->bgPlayer_->getVolume());
        });
    };

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
    if (bgPlayer_) bgPlayer_->shutdown();
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
        auto& lm = LocalizationManager::getInstance();

        juce::PopupMenu menu;
        menu.addItem (1, lm.getText ("topbar.menu_edit_profile"));
        menu.addItem (2, lm.getText ("topbar.menu_sign_out"));
        menu.addSeparator();
        menu.addItem (3, lm.getText ("topbar.menu_close_app"));

        juce::Component::SafePointer<MainComponent> safe (this);
        menu.showMenuAsync (juce::PopupMenu::Options(), [safe] (int result)
        {
            if (safe == nullptr || result == 0)
                return;

            if (result == 1)
            {
                // "Edit Profile" -- not a sidebar nav item, only reachable
                // here (see NavPage::Profile's declaration).
                if (safe->mainArea != nullptr)
                {
                    if (auto* profilePage = safe->mainArea->getProfilePage())
                        profilePage->loadFromCurrentHost();
                    safe->mainArea->setCurrentPage (NavPage::Profile);
                }
            }
            else if (result == 2)
            {
                // "Sign Out" -- stop every venue-scoped watcher and clear
                // cached session state BEFORE handing off to Main.cpp's
                // signOutAndReturnToLogin(), since that tears down this
                // whole MainComponent but the underlying *Service singletons
                // would otherwise keep running/polling for the now-signed-out
                // session (they outlive MainComponent -- see
                // RequestService/QueueService/VenueService, all singletons).
                RequestService::getInstance().stop();
                QueueService::getInstance().stopWatching();
                EmojiService::getInstance().stop();
                VenueService::getInstance().stopWatchingPlaying();
                VenueService::getInstance().clear();
                HostService::getInstance().clear();
                FirestoreClient::getInstance().signOut();

                if (safe->onSignOutRequested)
                    safe->onSignOutRequested();
            }
            else if (result == 3)
            {
                // "Close App" -- the same graceful-quit path the window's
                // own close button uses.
                juce::JUCEApplication::getInstance()->systemRequestedQuit();
            }
        });
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

    auto loadSingerIntoNowPlaying = [this](const Singers& singer, bool autoStart) -> bool
    {
        if (singer.songs.empty())
        {
            DBG("Play singer: no songs in queue");
            return false;
        }

        const auto firstSong = singer.songs.front();

        if (mainArea == nullptr)
            return false;

        const auto& library = mainArea->getLibrarySongs();
        const juce::String wantId     = juce::String(firstSong.songId).trim();
        const juce::String wantName   = juce::String(firstSong.songName).trim().toLowerCase();
        const juce::String wantArtist = juce::String(firstSong.songArtist).trim().toLowerCase();

        const CdgSong* match = nullptr;

        // Exact name+artist match first, preferring whichever library row has
        // the most manufacturer versions. The library can contain duplicate
        // rows for the same song/artist when a scan didn't merge every
        // version into one record (seen with "House Of The Rising Sun" --
        // one row left with only ["Unknown"], another with 12 real
        // versions). Matching on the queue item's stored songId first (as
        // this used to) can land on the thin row and hide every other
        // version, so the grouping key (name+artist) takes priority.
        for (const auto& s : library)
        {
            const juce::String libName = juce::String(s.songName).trim().toLowerCase();
            const juce::String libArtist = juce::String(s.artistName).trim().toLowerCase();

            const bool nameMatches = (libName == wantName);
            const bool artistMatches = wantArtist.isEmpty() || (libArtist == wantArtist);

            if (nameMatches && artistMatches)
            {
                if (match == nullptr || s.version.size() > match->version.size())
                    match = &s;
            }
        }

        if (match == nullptr && wantId.isNotEmpty())
        {
            for (const auto& s : library)
            {
                if (juce::String(s.id).trim() == wantId)
                {
                    match = &s;
                    break;
                }
            }
        }

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

        CdgSong dbMatch;
        if (match == nullptr)
        {
            SongDatabase db;
            if (db.open())
            {
                // findByNameAndArtist() first, not searchPrefix() -- searchPrefix()
                // depends on the songs_fts FTS5 virtual table, which silently
                // fails to get created on Windows (the vcpkg sqlite3 port
                // doesn't enable FTS5 by default), so it always returned zero
                // hits here. See SongDatabase::findByNameAndArtist()'s doc
                // comment for the full story. It's also already the "richest"
                // duplicate row (ORDER BY LENGTH(versions) DESC), so try it
                // before falling back to a raw id match that could land on a
                // thin duplicate row.
                if (wantName.isNotEmpty())
                    dbMatch = db.findByNameAndArtist(wantName, wantArtist);

                if (dbMatch.id.empty() && wantId.isNotEmpty())
                    dbMatch = db.getById(wantId);
            }

            if (!dbMatch.id.empty())
                match = &dbMatch;
        }

        if (match == nullptr)
        {
            DBG("Play singer: no library match for '" << juce::String(firstSong.songName)
                << "' by '" << juce::String(firstSong.songArtist) << "'");

            if (queueBar != nullptr)
                queueBar->clearNowPlaying();
            localNowPlaying_ = {};
            hasLocalNowPlaying_ = false;

            if (bottomBar != nullptr)
                bottomBar->setPlaying(false);

            showSongUnavailableMessage(firstSong);
            return false;
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

        // Loading a singer into the Now Singing card should not switch the
        // lyric screen out of idle mode until transport actually starts.
        if (lyricWindow_ != nullptr)
            lyricWindow_->setForceIdleScreen(! autoStart);

        loadAndPlaySong(*match, resolvedVersionIndex, pitchSemis, autoStart,
                        [this, singer, autoStart](bool ok)
                        {
                            if (! ok)
                            {
                                if (queueBar != nullptr)
                                    queueBar->clearNowPlaying();
                                localNowPlaying_ = {};
                                hasLocalNowPlaying_ = false;
                                lyricLowerThirdHoldNowSinging_ = false;

                                if (bottomBar != nullptr)
                                    bottomBar->setPlaying(false);
                                if (queueBar != nullptr)
                                    queueBar->setPlaying(false);
                                return;
                            }

                            if (queueBar != nullptr)
                                queueBar->setNowPlaying(singer);
                            localNowPlaying_ = singer;
                            hasLocalNowPlaying_ = true;
                            lyricLowerThirdHoldNowSinging_ = ! autoStart;
                            syncLyricNowSingingSummary();
                            if (queueBar != nullptr)
                                syncLyricLowerThirdNextUp(queueBar->getSingers());
                        });
        return true;
    };

    auto clearLocalNowPlayingState = [this]()
    {
        localNowPlaying_ = {};
        hasLocalNowPlaying_ = false;
        lyricLowerThirdHoldNowSinging_ = false;
        syncLyricNowSingingSummary();
        if (queueBar != nullptr)
            syncLyricLowerThirdNextUp(queueBar->getSingers());
        else if (lyricWindow_ != nullptr)
            lyricWindow_->setLowerThirdNextUpSinger({});
    };

    auto clearLoadedPlaybackState = [this]()
    {
        // The song is being abandoned (skipped / returned to queue), not
        // paused — remove its now-playing doc so it doesn't linger.
        clearPlayingDoc();

        currentSong = {};
        currentSongImageUrl.clear();
        currentSongDuration = 0.0;
        currentRibbonCdgFile_ = juce::File();

        if (lyricWindow_ != nullptr)
            lyricWindow_->stopVideo();

        if (audioEngine != nullptr)
            audioEngine->unloadSong();

        if (bottomBar != nullptr)
        {
            bottomBar->setDurationSeconds(0.0);
            bottomBar->setProgress(0.0f);
            bottomBar->setWaveformSamples({});
        }

        refreshRibbonState();
    };

    // Shared transport-start helper.
    // allowQueueFallback=true keeps the existing bottom-bar behavior
    // (load next singer when nothing is loaded). Now-playing uses false.
    auto startTransportPlayback = [this, loadSingerIntoNowPlaying](bool allowQueueFallback)
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
            if (hasLocalNowPlaying_ && ! localNowPlaying_.songs.empty())
            {
                if (loadSingerIntoNowPlaying(localNowPlaying_, true))
                    return;
            }

            if (allowQueueFallback)
            {
                // First press with no loaded media should preload only.
                const bool queued = queueAndLoadNextSingerSong(false, true);

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

        // Fade out background music so it doesn't overlap karaoke -- covers
        // the "song was merely preloaded, not autostarted" case (Auto Play
        // off) and plain resume-from-pause, both of which reach this direct
        // play path instead of loadAndPlaySong(). Harmless no-op if the
        // background music isn't currently playing/fading.
        if (bgPlayer_ != nullptr)
            bgPlayer_->fadeOut (1.5f);

        if (videoLoaded)
            lyricWindow_->playVideo();
        else
            audioEngine->play();

        // Fresh play on an already-loaded song (e.g. Play pressed after a
        // preload-without-autostart), or resuming from pause — resuming is
        // a no-op here since writePlayingDocIfNeeded() only ever writes once
        // per loaded song (the doc is left untouched across pause/resume).
        writePlayingDocIfNeeded();

        if (lyricWindow_ != nullptr)
            lyricWindow_->setForceIdleScreen(false);

        lyricLowerThirdHoldNowSinging_ = false;
        if (queueBar != nullptr)
            syncLyricLowerThirdNextUp(queueBar->getSingers());

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

        if (lyricWindow_ != nullptr)
            lyricWindow_->setForceIdleScreen(true);

        // Manual stop is a distinct event from pause — remove the
        // now-playing doc (pause deliberately leaves it in place).
        clearPlayingDoc();

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

            if (lyricWindow_ != nullptr)
                lyricWindow_->setForceIdleScreen(true);

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

    bottomBar->onExpandMainScreenClicked = [this]() {
        if (onToggleMainFullscreenRequested)
            onToggleMainFullscreenRequested();
    };

    bottomBar->onExpandLyricScreenClicked = [this]() {
        if (lyricWindow_ == nullptr)
            lyricWindow_ = std::make_unique<LyricDisplayWindow>(audioEngine.get());

        if (lyricWindow_ != nullptr)
            lyricWindow_->toggleFullScreen();

        refreshRibbonState();
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
    if (auto* settingsPage = mainArea->getSettingsPage())
        settingsPage->setAudioEngine(audioEngine.get());
    mainArea->setBackgroundMusicPlayer(bgPlayer_.get());
    addAndMakeVisible(mainArea.get());

    if (auto* profilePage = mainArea->getProfilePage())
        profilePage->onProfileSaved = [this] { applyCurrentIdentityToUi(); };

    // Create Ribbon menu (quick-access control surface)
    ribbonMenu = std::make_unique<RibbonMenu>();
    addAndMakeVisible(ribbonMenu.get());
    ribbonMenu->onLayoutChanged = [this]() { resized(); };
    ribbonMenu->setAudioEngine(audioEngine.get());

    ribbonMenu->onBackgroundPlayPause = [this](bool shouldPlay)
    {
        if (bgPlayer_ == nullptr) return;
        if (shouldPlay)
            bgPlayer_->play();
        else
            bgPlayer_->pause();
    };

    ribbonMenu->onBackgroundVolumeChanged = [this](float volume01)
    {
        if (bgPlayer_ != nullptr)
            bgPlayer_->setVolume(volume01);
    };

    ribbonMenu->onBackgroundSeekRequested = [this](double positionSeconds)
    {
        if (bgPlayer_ != nullptr)
            bgPlayer_->seekToPosition(positionSeconds);
    };

    ribbonMenu->onBackgroundNextTrack = [this]()
    {
        if (bgPlayer_ != nullptr)
            bgPlayer_->skipToNext();
    };

    ribbonMenu->onBackgroundPrevTrack = [this]()
    {
        if (bgPlayer_ != nullptr)
            bgPlayer_->skipToPrev();
    };

    ribbonMenu->onLyricToggleWindow = [this]()
    {
        if (lyricWindow_ == nullptr)
            lyricWindow_ = std::make_unique<LyricDisplayWindow>(audioEngine.get());

        if (lyricWindow_ != nullptr)
            lyricWindow_->setVisible(! lyricWindow_->isVisible());

        refreshRibbonState();
    };

    ribbonMenu->onLyricToggleFullscreen = [this]()
    {
        if (lyricWindow_ == nullptr)
            lyricWindow_ = std::make_unique<LyricDisplayWindow>(audioEngine.get());

        if (lyricWindow_ != nullptr)
            lyricWindow_->toggleFullScreen();

        refreshRibbonState();
    };

    ribbonMenu->onPlayNextSinger = [this]()
    {
        const bool ok = queueAndLoadNextSingerSong(true, true);
        if (! ok)
            showMaintenanceToast("No queued singer available.");
    };

    ribbonMenu->onSfxVolumeChanged = [this](float volume01)
    {
        if (audioEngine != nullptr)
            audioEngine->setSfxVolume(volume01);
    };

    ribbonMenu->setSfxVolume(audioEngine != nullptr ? audioEngine->getSfxVolume() : 0.85f);

    ribbonMenu->onTriggerSfx = [this](const juce::String& effectName)
    {
        juce::String soundPath;
        if (effectName.equalsIgnoreCase("Are You Ready"))
            soundPath = "assets/sounds/Are You Ready.wav";
        else if (effectName.equalsIgnoreCase("Chicken"))
            soundPath = "assets/sounds/Chicken.wav";
        else if (effectName.equalsIgnoreCase("Burp"))
            soundPath = "assets/sounds/Burp.wav";
        else if (effectName.equalsIgnoreCase("Bruh"))
            soundPath = "assets/sounds/Bruh.wav";
        else if (effectName.equalsIgnoreCase("Buzzer"))
            soundPath = "assets/sounds/Buzzer.wav";
        else if (effectName.equalsIgnoreCase("Drum Fill"))
            soundPath = "assets/sounds/Drum Fill.wav";
        else if (effectName.equalsIgnoreCase("Drum Roll"))
            soundPath = "assets/sounds/Drum Roll.wav";
        else if (effectName.equalsIgnoreCase("WooHoo"))
            soundPath = "assets/sounds/WooHoo.wav";
        else
            return;

        auto soundFile = resolveAssetFile(soundPath);
        if (! soundFile.existsAsFile())
        {
            showMaintenanceToast("Missing sound file: " + soundPath);
            return;
        }

        if (audioEngine == nullptr || ! audioEngine->triggerOneShotSfx(soundFile))
        {
            showMaintenanceToast("Unable to play sound effect: " + effectName);
            return;
        }

        showMaintenanceToast("SFX: " + effectName);
    };

    refreshRibbonState();

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
            if (currentSong.isValid())
                logPlayHistoryIfNeeded(false); // logs only if played > 30 s
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

        // Build the QueueItem from the dialog result.
        // QueueService applies a canonical doc-ID policy:
        // - auth singers use auth UID (`profileId`)
        // - manual singers use deterministic `manual-*` IDs.
        // When the signed-in host adds songs for themselves, pass their auth UID.
        const juce::String authUid = FirestoreClient::getInstance().getUserId().trim();

        juce::String hostStageName;
        juce::String hostFullName;
        juce::String hostAvatarUrl;
        if (HostService::getInstance().hasCurrent())
        {
            const auto h = HostService::getInstance().getCurrent();
            hostStageName = juce::String(h.stageName).trim();
            hostFullName = juce::String(h.fullName).trim();
            hostAvatarUrl = juce::String(h.avatarUrl).trim();
        }

        const juce::String trimmedSingerName = singerName.trim();
        const bool singerIsHostKeyword = trimmedSingerName.equalsIgnoreCase("host");
        const bool singerLooksLikeHost = authUid.isNotEmpty()
            && (singerIsHostKeyword
             || trimmedSingerName.equalsIgnoreCase(hostStageName)
             || trimmedSingerName.equalsIgnoreCase(hostFullName));

        // If the user typed the keyword "host", resolve it to the host's real
        // stage name so QueueService's case-insensitive name lookup finds the
        // pinned host slot rather than creating a new "host" singer entry.
        const juce::String resolvedSingerName = (singerIsHostKeyword && singerLooksLikeHost)
            ? (hostStageName.isNotEmpty() ? hostStageName
               : hostFullName.isNotEmpty() ? hostFullName
               : singerName)
            : singerName;

        QueueItem item;
        item.id          = juce::Uuid().toString().toStdString();
        item.deviceId    = "local";
        item.profileId   = singerLooksLikeHost ? authUid.toStdString() : "";
        item.singerName  = resolvedSingerName.toStdString();
        item.songId      = r.song.id;
        item.songName    = r.song.songName;
        item.songArtist  = r.song.artistName;
        item.songVersion = versionLabel.toStdString();
        item.duration    = r.song.durationMS / 1000;
        item.pitch       = (float) r.pitchSemitones; // semitones, 0.0 = normal -- see QueueItem.h
        item.key         = r.pitchSemitones;
        item.status      = "queued";
        item.dateAdded   = juce::Time::getCurrentTime().toMilliseconds();

        const bool playNext = (r.action == SongSelectionResult::Action::PlayNext);

        DBG ("[SongSelect] " << (playNext ? "PlayNext" : "AddToQueue")
             << " singer='" << resolvedSingerName << "'"
             << " song='" << juce::String(item.songName) << "'");

        juce::Component::SafePointer<MainComponent> safe (this);
        auto appendSelectedSong = [safe, venueId, resolvedSingerName, playNext, item]()
        {
            QueueService::getInstance().appendSong (venueId, item,
                [safe, venueId, resolvedSingerName, playNext](bool ok, juce::String err)
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

                    DBG ("[SongSelect] appendSong OK for '" << resolvedSingerName << "'");

                    if (playNext)
                    {
                        // "Play Next" is purely a rotation-anchor change --
                        // the RR itself (`order`) is untouched. appendSong
                        // already placed the new singer at the bottom of
                        // the RR; here we just point the anchor at them.
                        QueueService::getInstance().loadQueue(venueId,
                            [venueId, resolvedSingerName, safe](bool loadedOk, QueueService::Snapshot snap, juce::String loadErr)
                            {
                                if (! loadedOk)
                                {
                                    DBG ("[SongSelect] PlayNext: loadQueue failed: " << loadErr);
                                    if (safe != nullptr) safe->reloadQueueFromFirestore(venueId);
                                    return;
                                }

                                auto rr = QueueRotation::sortByStableOrder(snap.singers);
                                juce::String newAnchorId;
                                for (auto& s : rr)
                                {
                                    if (! s.isHost && juce::String(s.name).trim().equalsIgnoreCase(resolvedSingerName))
                                    {
                                        newAnchorId = juce::String(s.id).trim();
                                        break;
                                    }
                                }

                                if (newAnchorId.isEmpty())
                                {
                                    if (safe != nullptr) safe->reloadQueueFromFirestore(venueId);
                                    return;
                                }

                                QueueRotation::stampDerivedRanks(rr, newAnchorId);
                                QueueService::getInstance().persistSingerOrder(venueId, rr,
                                    [venueId, safe](bool ok, juce::String err)
                                    {
                                        if (! ok)
                                            DBG("[Queue] PlayNext persistSingerOrder failed: " << err);
                                        if (safe != nullptr)
                                            safe->reloadQueueFromFirestore(venueId);
                                    });
                            });
                    }
                    else if (safe != nullptr)
                    {
                        safe->reloadQueueFromFirestore(venueId);
                    }
                });
        };

        if (singerLooksLikeHost && authUid.isNotEmpty())
        {
            QueueService::getInstance().ensureHostQueueDoc(
                venueId,
                authUid,
                hostStageName.isNotEmpty() ? hostStageName : resolvedSingerName,
                hostAvatarUrl,
                [appendSelectedSong](bool ok, juce::String err)
                {
                    if (! ok)
                    {
                        DBG ("[SongSelect] ensureHostQueueDoc FAILED: " << err);
                        juce::AlertWindow::showMessageBoxAsync(
                            juce::MessageBoxIconType::WarningIcon,
                            "Could Not Add Song",
                            "Failed to prepare the host queue slot: " + err);
                        return;
                    }

                    appendSelectedSong();
                });
        }
        else
        {
            appendSelectedSong();
        }
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

    // Reflect Library scan/metadata/upload progress in the BottomBar's
    // status area (where "Audio Ready" normally shows).
    mainArea->onLibraryStatusMessage = [this](const juce::String& msg)
    {
        setLibrarySyncStatusMessage(msg);
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

        sp->onUploadLogo = [this](const juce::File& logoFile)
        {
            if (activeVenueId_.isEmpty())
                return;

            juce::Component::SafePointer<MainComponent> safe(this);
            const auto venueId = activeVenueId_;
            VenueService::getInstance().uploadLogo(venueId, logoFile,
                [safe, venueId](bool ok, juce::String logoUrl, juce::String error)
                {
                    if (safe == nullptr)
                        return;

                    if (auto* settings = safe->mainArea != nullptr ? safe->mainArea->getSettingsPage() : nullptr)
                        settings->setLogoStatus(ok ? LocalizationManager::getInstance().getText("settings.logo_upload_success")
                                                    : error,
                                                 ! ok);

                    if (! ok)
                        return;

                    VenueService::getInstance().loadVenue(venueId,
                        [safe, logoUrl](bool loaded, VenueItem v, juce::String loadErr)
                        {
                            if (safe == nullptr) return;
                            if (! loaded)
                            {
                                DBG("[Settings] reload venue after logo upload failed: " << loadErr);
                                return;
                            }
                            if (safe->mainArea != nullptr)
                                safe->mainArea->setVenueData(v);

                            // Push the new logo straight into the lyric display
                            // without waiting for the next full venue reload.
                            if (safe->lyricWindow_ != nullptr)
                            {
                                auto img = ArtworkCache::getInstance().getOrFetch(logoUrl,
                                    [safe, logoUrl]
                                    {
                                        if (safe == nullptr || safe->lyricWindow_ == nullptr) return;
                                        auto loaded2 = ArtworkCache::getInstance().getOrFetch(logoUrl);
                                        if (loaded2.isValid())
                                            if (auto* d = safe->lyricWindow_->getDisplay())
                                                d->setVenueLogo(loaded2);
                                    });
                                if (img.isValid())
                                {
                                    safe->pendingVenueLogo_ = img;
                                    if (auto* d = safe->lyricWindow_->getDisplay())
                                        d->setVenueLogo(img);
                                }
                            }
                        });
                });
        };

        sp->onResetLogo = [this]()
        {
            if (activeVenueId_.isEmpty())
                return;

            juce::Component::SafePointer<MainComponent> safe(this);
            const auto venueId = activeVenueId_;
            VenueService::getInstance().resetLogo(venueId,
                [safe, venueId](bool ok, juce::String error)
                {
                    if (safe == nullptr)
                        return;

                    if (auto* settings = safe->mainArea != nullptr ? safe->mainArea->getSettingsPage() : nullptr)
                        settings->setLogoStatus(ok ? LocalizationManager::getInstance().getText("settings.logo_reset_success")
                                                    : error,
                                                 ! ok);

                    if (! ok)
                        return;

                    safe->pendingVenueLogo_ = {};
                    if (safe->lyricWindow_ != nullptr)
                        if (auto* d = safe->lyricWindow_->getDisplay())
                            d->setVenueLogo ({});

                    VenueService::getInstance().loadVenue(venueId,
                        [safe](bool loaded, VenueItem v, juce::String loadErr)
                        {
                            if (safe == nullptr) return;
                            if (! loaded)
                            {
                                DBG("[Settings] reload venue after logo reset failed: " << loadErr);
                                return;
                            }
                            if (safe->mainArea != nullptr)
                                safe->mainArea->setVenueData(v);
                        });
                });
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
        // queueBar->getSingers() is the DISPLAYED (anchor-rotated) order, not
        // the stable RR -- recover the RR by sorting on each singer's own
        // .order field (already correctly set by whichever caller triggered
        // this, e.g. QueueBar::moveSinger via QueueRotation::remapFromDisplayDrag)
        // before persisting, so we never write display position as RR order.
        const auto rr = QueueRotation::sortByStableOrder(queueBar->getSingers());

        QueueService::getInstance().persistSingerOrder(venueId, rr,
            [](bool ok, juce::String err)
            {
                if (! ok)
                    DBG("[Queue] persistSingerOrder failed: " << err);
            });
    };

    queueBar->onClearQueue = [this]()
    {
        auto& lm = LocalizationManager::getInstance();
        auto safe = juce::Component::SafePointer<MainComponent>(this);
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
                [safe,
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

    queueBar->onPlaySinger = [this, loadSingerIntoNowPlaying](int singerIndex) {
        DBG("QueueBar: Play singer at index " + juce::String(singerIndex));
        if (queueBar == nullptr) return;

        auto singers = queueBar->getSingers();
        if (singerIndex < 0 || singerIndex >= (int) singers.size()) return;

        auto singer = singers[(size_t) singerIndex];
        const bool singerHasQueuedSongs = ! singer.songs.empty();

        // Starting a performance never touches the Round Robin or the
        // rotation anchor -- only finishing (handleSongFinished) or being
        // skipped for having no songs (queueAndLoadNextSingerSong) does.
        // If this row has nothing to play, only the front-of-queue (the
        // anchor) can be meaningfully skipped/struck and advanced past;
        // there's nothing sensible to do for a non-front row with no songs.
        if (! singerHasQueuedSongs)
        {
            if (singerIndex == 0)
            {
                const bool autoStart = queueAutoStartRequested_;
                queueAutoStartRequested_ = false;
                const bool queued = queueAndLoadNextSingerSong(autoStart, true);
                if (! queued)
                {
                    if (bottomBar != nullptr) bottomBar->setPlaying(false);
                    if (queueBar != nullptr) queueBar->setPlaying(false);
                }
            }
            return;
        }

        auto singerForPlayback = singer; // keeps the full songs list for loadSingerIntoNowPlaying
        QueueItem nowSingingItem = singer.songs.front();
        nowSingingItem.singerName = singer.name;
        singerForPlayback.isNewlyAdded = false;

        // Promoting a singer into Now Singing cycles the round robin
        // immediately, so the top of the queue always shows the NEXT
        // person to sing -- never whoever is currently performing. (The
        // RR itself, i.e. `order`, is still untouched -- only the anchor
        // advances past this singer's slot.)
        auto rr = QueueRotation::sortByStableOrder(singers);
        const int promotedOrder = singer.order;
        const auto newAnchorId = QueueRotation::advanceAnchor(rr, promotedOrder);

        for (auto& s : rr)
        {
            if (s.order == promotedOrder)
            {
                s.isNewlyAdded = false;
                s.songs.erase(s.songs.begin());
                break;
            }
        }

        QueueRotation::stampDerivedRanks(rr, newAnchorId);
        queueBar->setSingers(QueueRotation::deriveDisplayQueue(rr, newAnchorId));

        const auto venueId = activeVenueId_.trim();
        if (venueId.isNotEmpty())
        {
            // Remove it from the singer's persisted queue immediately so
            // it does not remain queued.
            QueueService::getInstance().removeSong(venueId, nowSingingItem,
                [](bool ok, juce::String err)
                {
                    if (! ok)
                        DBG("[Queue] onPlaySinger removeSong failed: " << err);
                });

            QueueService::getInstance().persistSingerOrder(venueId, rr,
                [](bool ok, juce::String err)
                {
                    if (! ok)
                        DBG("[Queue] onPlaySinger persistSingerOrder failed: " << err);
                });
        }

        const bool autoStartSelectedSinger = queueAutoStartRequested_;
        queueAutoStartRequested_ = false;

        juce::ignoreUnused(loadSingerIntoNowPlaying(singerForPlayback, autoStartSelectedSinger));
    };

    queueBar->onPlayCurrent = [this, startTransportPlayback]() {
        // Now-playing play button should only start transport for the
        // currently loaded song and never advance/load the queue.
        startTransportPlayback(false);
    };

    queueBar->onPauseCurrent = [this]() {
        if (lyricWindow_ != nullptr && lyricWindow_->isVideoActive())
            lyricWindow_->pauseVideo();
        else if (audioEngine != nullptr)
            audioEngine->pause();

        if (lyricWindow_ != nullptr)
            lyricWindow_->setForceIdleScreen(true);

        if (bottomBar != nullptr)
            bottomBar->setPlaying(false);
        if (queueBar != nullptr)
            queueBar->setPlaying(false);
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
        const auto singers = queueBar->getSingers();
        if (singerIndex < 0 || singerIndex >= (int) singers.size()) return;
        if (singers[(size_t) singerIndex].isHost) return;

        const auto removedSinger  = singers[(size_t) singerIndex];
        const juce::String docId  = juce::String(removedSinger.id);
        const juce::String venueId = activeVenueId_;
        const bool wasAnchor = (singerIndex == 0); // display index 0 is always the anchor

        queueBar->removeSinger(singerIndex);

        // If the removed singer was up next, advance the anchor to whoever
        // comes after them in the RR and persist the restamped ranks for
        // everyone who's left. If they weren't the anchor, nothing else
        // needs to change -- gaps in `order` are harmless (display is
        // always re-derived by sorting ascending), and the anchor is
        // unaffected.
        if (wasAnchor && ! venueId.isEmpty())
        {
            auto rr = QueueRotation::sortByStableOrder(singers); // pre-removal RR
            const auto newAnchorId = QueueRotation::advanceAnchor(rr, removedSinger.order);

            for (int i = 0; i < (int) rr.size(); ++i)
            {
                if (rr[(size_t) i].order == removedSinger.order)
                {
                    rr.erase(rr.begin() + i);
                    break;
                }
            }

            QueueRotation::stampDerivedRanks(rr, newAnchorId);
            QueueService::getInstance().persistSingerOrder(venueId, rr,
                [](bool ok, juce::String err)
                {
                    if (! ok)
                        DBG("[Queue] onRemoveSinger persistSingerOrder failed: " << err);
                });
        }

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
    auto returnCurrentToQueue = [this, clearLocalNowPlayingState, clearLoadedPlaybackState](bool toFront)
    {
        if (queueBar == nullptr) return;
        Singers cs = localNowPlaying_;

        if (cs.id.empty()) return;

        queueBar->clearNowPlaying();
    clearLocalNowPlayingState();
    clearLoadedPlaybackState();

        const auto venueId = activeVenueId_;

        // The singer never left the RR (their slot, `order`, is untouched
        // by starting a performance -- see onPlaySinger), so there's no
        // re-insertion needed. The anchor already advanced past them the
        // moment they were promoted to Now Singing, though, so: "Next"
        // explicitly moves it back to them; "End" leaves it exactly where
        // it already is (already past them -- advancing it again here
        // would wrongly skip whoever's genuinely up next).
        auto rr = QueueRotation::sortByStableOrder(queueBar->getSingers());
        for (auto& s : rr)
        {
            if (juce::String(s.id).trim() == juce::String(cs.id).trim()
                || juce::String(s.name).trim().equalsIgnoreCase(juce::String(cs.name).trim()))
            {
                s.songs = cs.songs; // restore the full list -- they never actually sang it
                break;
            }
        }

        const auto newAnchorId = toFront
            ? juce::String(cs.id).trim()
            : QueueRotation::findAnchorId(rr);

        QueueRotation::stampDerivedRanks(rr, newAnchorId);
        queueBar->setSingers(QueueRotation::deriveDisplayQueue(rr, newAnchorId));

        if (! venueId.isEmpty())
            QueueService::getInstance().persistSingerOrder(venueId, rr,
                [](bool ok, juce::String err)
                {
                    if (! ok)
                        DBG("[Queue] returnCurrentToQueue persistSingerOrder failed: " << err);
                });
    };

    queueBar->onReturnCurrentToQueueNext = [returnCurrentToQueue]()  { returnCurrentToQueue(true);  };
    queueBar->onReturnCurrentToQueueEnd  = [returnCurrentToQueue]()  { returnCurrentToQueue(false); };

    // ── Skip current singer ────────────────────────────────────────────────────
    queueBar->onSkipCurrentSinger = [this, clearLocalNowPlayingState, clearLoadedPlaybackState]()
    {
        logPlayHistoryIfNeeded(false); // logs only if played > 30 s
        if (queueBar)    queueBar->clearNowPlaying();
        clearLocalNowPlayingState();
        clearLoadedPlaybackState();
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

                            // Bottom of the stable RR (max existing order + 1) --
                            // NOT display-vector length, which can collide with an
                            // existing order once gaps exist (e.g. after a removal).
                            int maxOrder = -1;
                            for (auto& s : queueBar->getSingers())
                                maxOrder = juce::jmax(maxOrder, s.order);
                            newSinger.order = maxOrder + 1;

                            queueBar->addSinger(newSinger);
                        }
                    }
                    delete alertWindow;
                }),
            true);
    };

    // ── Song dropped from Search results onto a singer row ────────────────────
    // Bypasses SongSelectionDialog entirely (no version/pitch picker) --
    // defaults to the song's first version and no pitch shift, matching a
    // quick drag-and-drop interaction. The target is an existing, already-
    // loaded singer row, so (unlike onSongSelectionResult's typed-name path)
    // there's no name-resolution guessing or ensureHostQueueDoc dance needed
    // -- targetSinger.isHost tells us directly, and their doc already exists
    // since they're already visible in the queue.
    queueBar->onSongDroppedOnSinger = [this](const CdgSong& song, int singerIndex)
    {
        if (queueBar == nullptr) return;
        const auto& singersList = queueBar->getSingers();
        if (singerIndex < 0 || singerIndex >= (int) singersList.size()) return;

        const auto targetSinger = singersList[(size_t) singerIndex];
        const juce::String venueId = activeVenueId_;
        if (venueId.isEmpty())
        {
            DBG ("[SongDrop] cannot add to queue: no active venue");
            return;
        }

        const juce::String authUid = FirestoreClient::getInstance().getUserId().trim();
        const juce::String versionLabel = ! song.version.empty()
            ? juce::String (song.version[0]) : juce::String();

        QueueItem item;
        item.id          = juce::Uuid().toString().toStdString();
        item.deviceId    = "local";
        item.profileId   = targetSinger.isHost ? authUid.toStdString() : "";
        item.singerName  = targetSinger.name;
        item.songId      = song.id;
        item.songName    = song.songName;
        item.songArtist  = song.artistName;
        item.songVersion = versionLabel.toStdString();
        item.duration    = song.durationMS / 1000;
        item.pitch       = 0.0f; // 0.0 = normal -- matches .key (semitones), not the
        item.key         = 0;    // 1.0-ratio convention SongSelectionDialog's flow uses.
        item.status      = "queued";
        item.dateAdded   = juce::Time::getCurrentTime().toMilliseconds();

        DBG ("[SongDrop] AddToQueue singer='" << juce::String (targetSinger.name)
             << "' song='" << juce::String (item.songName) << "'");

        juce::Component::SafePointer<MainComponent> safe (this);
        QueueService::getInstance().appendSong (venueId, item,
            [safe, venueId] (bool ok, juce::String err)
            {
                if (! ok)
                {
                    DBG ("[SongDrop] appendSong FAILED: " << err);
                    juce::AlertWindow::showMessageBoxAsync(
                        juce::MessageBoxIconType::WarningIcon,
                        "Could Not Add Song",
                        "Failed to add the song to the queue: " + err);
                    return;
                }

                if (safe != nullptr)
                    safe->reloadQueueFromFirestore (venueId);
            });
    };

    // ── Now Playing: click the song to change its version ─────────────────────
    // Purely a live playback change, same as the BottomBar pitch knob
    // (bottomBar->onPitchChanged above) -- not written back to the singer's
    // Firestore queue doc. currentSong is already the full library record
    // (with every version[]/rating[] entry) since loadAndPlaySong sets it.
    queueBar->onNowPlayingSongClicked = [this]()
    {
        if (! currentSong.isValid())
            return;

        if (currentSong.version.empty())
        {
            juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::InfoIcon,
                "No Other Versions",
                "No other manufacturer versions of this song were found in the library.");
            return;
        }

        const juce::String currentVersion = currentSongVersion_.trim();
        const auto orderedIndices = currentSong.getVersionIndicesByRating();

        juce::PopupMenu menu;
        for (int vi : orderedIndices)
        {
            const bool isCurrent = juce::String (currentSong.version[(size_t) vi]).trim()
                                       .equalsIgnoreCase (currentVersion);
            menu.addItem (vi + 1, currentSong.getVersionLabel (vi), true, isCurrent);
        }

        juce::Component::SafePointer<MainComponent> safe (this);
        const CdgSong songToReload = currentSong;
        const int pitchSemis = juce::roundToInt (currentPitchSemitones_);

        menu.showMenuAsync (juce::PopupMenu::Options(),
            [safe, songToReload, pitchSemis] (int result)
            {
                if (safe == nullptr || result <= 0)
                    return;

                const int vi = result - 1;
                if (vi < 0 || (size_t) vi >= songToReload.version.size())
                    return;

                // Same partial-play logging as any other song change (e.g.
                // SongSelectionDialog's "Play Now").
                if (safe->currentSong.isValid())
                    safe->logPlayHistoryIfNeeded (false);

                safe->loadAndPlaySong (songToReload, vi, pitchSemis, /*autoStart*/ true);
            });
    };

    // ── Song-finished → auto-advance ──────────────────────────────────────────
    // Shared by AudioEngine's onSongFinished (audio/CDG) and the video
    // natural-end check in timerCallback() (MP4/M4V/MOV, which bypasses
    // AudioEngine entirely) — see MainComponent::handleSongFinished().
    audioEngine->onSongFinished = [this]()
    {
        handleSongFinished();
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

    updateBanner_ = std::make_unique<UpdateBanner>();
    updateBanner_->onRestartClicked = [] { UpdateService::getInstance().restartAndInstall(); };
    updateBanner_->onLaterClicked = [this] { if (updateBanner_) updateBanner_->setVisible(false); };
    // addChildComponent (not addAndMakeVisible) -- the banner must stay
    // hidden until showUpdateAvailableBanner() actually has something to
    // show; addAndMakeVisible would force it visible immediately regardless
    // of the UpdateBanner constructor's own setVisible(false).
    addChildComponent(updateBanner_.get());

    // Covers the case where the download (kicked off at launch, before
    // login) already finished while the user was still on the login/venue
    // screen — this constructor runs after that, so check directly rather
    // than relying only on the launch-time callback firing again.
    if (UpdateService::getInstance().isUpdateReadyToInstall())
        showUpdateAvailableBanner (UpdateService::getInstance().getPendingVersion());

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

    // Non-mac: embedded menu bar at the very top, with the app icon to its
    // left (macOS shows the app icon in the system menu bar itself, so no
    // equivalent is needed there).
   #if ! JUCE_MAC
    if (menuBar_ != nullptr && menuBar_->isVisible())
    {
        auto menuBounds = bounds.removeFromTop (24);

        if (menuBarIcon_ != nullptr)
            menuBarIcon_->setBounds (menuBounds.removeFromLeft (24).reduced (4));

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

    // Ribbon + MainArea fill the remaining centre space.
    if (mainArea)
    {
        const bool workspaceVisible = ! queueExpanded_;

        if (ribbonMenu)
        {
            ribbonMenu->setVisible(workspaceVisible);

            if (workspaceVisible)
            {
                if (ribbonMenu->isPanelExpanded())
                {
                    ribbonMenu->setBounds(bounds);
                    mainArea->setVisible(false);
                }
                else
                {
                    const int ribbonHeight = ribbonMenu->isHidden()
                        ? ribbonMenu->getHiddenHeight()
                        : ribbonMenu->getCollapsedHeight();

                    mainArea->setVisible(true);
                    auto ribbonBounds = bounds.removeFromBottom(juce::jmin(ribbonHeight, bounds.getHeight()));
                    mainArea->setBounds(bounds);
                    ribbonMenu->setBounds(ribbonBounds);
                }
            }
            else
            {
                mainArea->setVisible(false);
            }
        }
        else
        {
            mainArea->setVisible(workspaceVisible);
            if (workspaceVisible)
                mainArea->setBounds(bounds);
        }
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

    if (updateBanner_)
    {
        int topOffset = 8;
       #if ! JUCE_MAC
        if (menuBar_ != nullptr && menuBar_->isVisible())
            topOffset += 24;
       #endif
        if (topBar != nullptr)
            topOffset += topBar->getBarHeight();

        const int bannerW = juce::jmin(460, juce::jmax(340, getWidth() - 280));
        const int bannerH = 40;
        updateBanner_->setBounds(getWidth() - bannerW - 16, topOffset + 10, bannerW, bannerH);
        updateBanner_->toFront(false);
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
        languageMenu.addItem(2, juce::String(juce::CharPointer_UTF8("Español (Spanish)")), true, lm.getCurrentLanguage() == "es_ES");
        languageMenu.addItem(3, juce::String(juce::CharPointer_UTF8("Français (French)")), true, lm.getCurrentLanguage() == "fr_FR");
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

    // Video (MP4/M4V/MOV) bypasses AudioEngine entirely, so it has no
    // equivalent of AudioEngine::onSongFinished. Detect natural end here by
    // polling position vs. duration. Guarded so it only fires once per
    // loaded video, and only while position is actively advancing (not
    // merely sitting near the end while paused).
    if (lyricWindow_ != nullptr && lyricWindow_->isVideoActive() && ! videoFinishedFired_)
    {
        const double total = lyricWindow_->getVideoDuration();
        const double pos   = lyricWindow_->getVideoPosition();

        if (total > 0.25 && pos >= total - 0.15 && pos > lastVideoPositionSec_)
        {
            videoFinishedFired_ = true;
            handleSongFinished();
        }

        lastVideoPositionSec_ = pos;
    }

    refreshRibbonState();
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

void MainComponent::showSongLoadFailedMessage(const juce::String& songName,
                                              const juce::String& reason,
                                              const juce::String& path)
{
    juce::String title = "Song Load Failed";
    juce::String message = reason;

    const auto trimmedSong = songName.trim();
    if (trimmedSong.isNotEmpty())
        message = "Could not load \"" + trimmedSong + "\".\n\n" + reason;

    const auto trimmedPath = path.trim();
    if (trimmedPath.isNotEmpty())
        message << "\n\nPath: " << trimmedPath;

    if (bottomBar != nullptr)
        bottomBar->setWaveformStatusMessage(reason);

    juce::AlertWindow::showMessageBoxAsync(
        juce::MessageBoxIconType::WarningIcon,
        title,
        message);
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

void MainComponent::checkSongbookSyncAndPromptIfNeeded(const juce::String& venueId)
{
    auto safe = juce::Component::SafePointer<MainComponent>(this);

    // Never invite a local overwrite of a venue's live songbook from a
    // second machine -- if another device's session heartbeat is still
    // fresh, skip the out-of-sync prompt entirely (silently, matching this
    // feature's existing fail-safe style) rather than warn-and-allow, since
    // there's no urgent need for this PC to sync while someone else is live.
    VenueSessionService::getInstance().checkForOtherActiveSessions(venueId,
        [safe, venueId](bool otherActive, juce::String otherDeviceLabel, juce::String /*error*/)
        {
            if (safe == nullptr || safe->activeVenueId_ != venueId)
                return;

            if (otherActive)
            {
                DBG("[Songbook] Skipping out-of-sync check for venue " << venueId
                     << " -- appears live on " << otherDeviceLabel);
                return;
            }

            safe->runSongbookSyncCheck(venueId);
        });
}

void MainComponent::runSongbookSyncCheck(const juce::String& venueId)
{
    auto safe = juce::Component::SafePointer<MainComponent>(this);

    SongbookStorageService::getInstance().checkSongbookInSync(venueId,
        [safe, venueId](bool inSync, juce::String error)
        {
            if (safe == nullptr || safe->activeVenueId_ != venueId)
                return;

            if (inSync)
            {
                if (error.isNotEmpty())
                    DBG("[Songbook] Sync check couldn't complete for venue " << venueId << " (" << error << ") -- skipping");
                return;
            }

            auto& lm = LocalizationManager::getInstance();
            juce::AlertWindow::showOkCancelBox(
                juce::AlertWindow::WarningIcon,
                lm.getText("songbook.sync_title"),
                lm.getText("songbook.sync_body"),
                lm.getText("songbook.btn_sync_now"),
                lm.getText("songbook.btn_not_now"),
                safe.getComponent(),
                juce::ModalCallbackFunction::create([safe, venueId](int result)
                {
                    if (safe == nullptr || result != 1 || safe->activeVenueId_ != venueId)
                        return;

                    safe->showMaintenanceToast(LocalizationManager::getInstance().getText("songbook.syncing_toast"));

                    SongbookStorageService::getInstance().uploadLocalSongbook(venueId,
                        [safe, venueId](bool ok, juce::String uploadError)
                        {
                            if (safe == nullptr || safe->activeVenueId_ != venueId)
                                return;

                            auto& lm2 = LocalizationManager::getInstance();
                            if (ok)
                                safe->showMaintenanceToast(lm2.getText("songbook.sync_success_toast"));
                            else
                                juce::AlertWindow::showMessageBoxAsync(
                                    juce::AlertWindow::WarningIcon,
                                    lm2.getText("songbook.sync_failed_title"),
                                    lm2.getText("songbook.sync_failed_body") + " " + uploadError);
                        });
                }));
        });
}

void MainComponent::showUpdateAvailableBanner(const juce::String& version)
{
    if (updateBanner_ == nullptr)
        return;

    auto& lm = LocalizationManager::getInstance();
    updateBanner_->setMessage(lm.getText("update.banner_available") + " " + version);
    updateBanner_->setVisible(true);
    updateBanner_->toFront(false);
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

    // A Library scan/metadata/upload phase in progress takes priority over
    // the normal audio-engine status text.
    if (librarySyncStatusMessage_.isNotEmpty())
    {
        bottomBar->setWaveformStatusMessage(librarySyncStatusMessage_);
        return;
    }

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

void MainComponent::setLibrarySyncStatusMessage(const juce::String& message)
{
    librarySyncStatusMessage_ = message;
    updateAudioStatusIndicator();
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
        if (ribbonMenu) ribbonMenu->updateAllText();
        
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
void MainComponent::loadAndPlaySong(const CdgSong& song,
                                    int versionIndex,
                                    int pitchSemitones,
                                    bool autoStart,
                                    std::function<void(bool)> onDone)
{
    if (! audioEngine)
    {
        if (onDone)
            onDone(false);
        return;
    }

    if (! audioEngine->isInitialized())
    {
        DBG("loadAndPlaySong: audio engine not ready, attempting immediate init");
        audioEngine->initialize();

        if (! audioEngine->isInitialized())
        {
            auto& lm = LocalizationManager::getInstance();
            if (bottomBar != nullptr)
            {
                bottomBar->setWaveformSamples({});
                bottomBar->setWaveformStatusMessage(audioStartupInProgress_
                    ? lm.getText("audio.feedback.engine_starting")
                    : lm.getText("audio.feedback.engine_unavailable"));
            }
            showSongLoadFailedMessage(song.songName,
                                      "Audio engine is unavailable.");
            if (onDone)
                onDone(false);
            return;
        }
    }

    if (bottomBar != nullptr)
    {
        bottomBar->setWaveformSamples({});
        bottomBar->setWaveformStatusMessage("Loading \"" + juce::String(song.songName) + "\"...");
    }

    // Defer the actual load work to the next message-loop tick so the loading
    // message paints in the waveform area before any file I/O starts.
    juce::Component::SafePointer<MainComponent> self(this);
    juce::MessageManager::callAsync([self, song, versionIndex, pitchSemitones, autoStart, onDone = std::move(onDone)]()
    {
        if (! self || ! self->audioEngine)
        {
            if (onDone)
                onDone(false);
            return;
        }

    auto resolvePlayableMediaFile = [] (juce::File candidate) -> juce::File
    {
        auto ext = candidate.getFileExtension().toLowerCase();
        if (ext == ".cdg" || ext == ".xml")
        {
            static const juce::StringArray sidecarExts { ".mp3", ".wav", ".ogg", ".flac", ".aac", ".m4a" };
            for (const auto& sidecarExt : sidecarExts)
            {
                auto sibling = candidate.withFileExtension(sidecarExt);
                if (sibling.existsAsFile())
                    return sibling;
            }
        }

        return candidate;
    };

    auto resolveSongFilesForLoad = [&resolvePlayableMediaFile](const juce::File& sourceFile,
                                                               juce::File& audioOut,
                                                               juce::File& cdgOut,
                                                               juce::String& errorOut) -> bool
    {
        audioOut = juce::File{};
        cdgOut = juce::File{};
        errorOut.clear();

        const auto ext = sourceFile.getFileExtension().toLowerCase();
        if (ext == ".zip")
            return extractZipMediaFiles(sourceFile, audioOut, cdgOut, errorOut);

        audioOut = resolvePlayableMediaFile(sourceFile);
        return true;
    };

    auto buildVersionPath = [&song](int index) -> juce::String
    {
        if (index >= 0 && index < (int) song.fullPath.size())
        {
            const auto path = juce::String(song.fullPath[(size_t) index]).trim();
            if (path.isNotEmpty())
                return path;
        }

        if (index >= 0
            && index < (int) song.filePath.size()
            && index < (int) song.fileName.size())
        {
            const auto dir = juce::String(song.filePath[(size_t) index]).trim();
            const auto name = juce::String(song.fileName[(size_t) index]).trim();
            if (dir.isNotEmpty() && name.isNotEmpty())
                return juce::File(dir).getChildFile(name).getFullPathName();
        }

        return {};
    };

    // Pick the file path for the chosen version (fall back to the first path).
    juce::String path = buildVersionPath(versionIndex);
    if (path.isEmpty())
        path = buildVersionPath(0);

    if (path.isEmpty())
    {
        DBG("loadAndPlaySong: no file path on song \"" + juce::String(song.songName) + "\"");
        if (self->bottomBar != nullptr)
            self->bottomBar->setWaveformStatusMessage("Load failed.");
        self->showSongLoadFailedMessage(song.songName,
                                        "No playable file path is recorded for this song.");
        if (onDone)
            onDone(false);
        return;
    }

    juce::File audioFile;
    juce::File extractedCdgFile;
    juce::String resolveError;
    const juce::File sourceFile(path);
    if (! resolveSongFilesForLoad(sourceFile, audioFile, extractedCdgFile, resolveError))
    {
        if (self->bottomBar != nullptr)
            self->bottomBar->setWaveformStatusMessage("Load failed.");
        self->showSongLoadFailedMessage(song.songName,
                                        resolveError.isNotEmpty() ? resolveError : "Could not read ZIP archive.",
                                        sourceFile.getFullPathName());
        if (onDone)
            onDone(false);
        return;
    }

    auto ext = audioFile.getFileExtension().toLowerCase();

    if (! audioFile.existsAsFile())
    {
        // Fallback: if the requested version path is unavailable, try every
        // known version path and pick the first playable file.
        const int versionCount = juce::jmax((int) song.fullPath.size(),
                                            juce::jmin((int) song.filePath.size(), (int) song.fileName.size()));
        for (int i = 0; i < versionCount; ++i)
        {
            auto candidatePath = buildVersionPath(i);
            if (candidatePath.isEmpty())
                continue;

            juce::File candidate;
            juce::File candidateCdg;
            juce::String candidateError;
            if (! resolveSongFilesForLoad(juce::File(candidatePath), candidate, candidateCdg, candidateError))
                continue;

            if (candidate.existsAsFile())
            {
                audioFile = candidate;
                extractedCdgFile = candidateCdg;
                ext = audioFile.getFileExtension().toLowerCase();
                DBG("loadAndPlaySong: fallback picked playable version: " + audioFile.getFullPathName());
                break;
            }
        }
    }

    if (! audioFile.existsAsFile())
    {
        DBG("loadAndPlaySong: file not found: " + audioFile.getFullPathName());
        if (self->bottomBar != nullptr)
            self->bottomBar->setWaveformStatusMessage("Load failed.");
        self->showSongLoadFailedMessage(song.songName,
                                        "The song file was not found on this computer.",
                                        audioFile.getFullPathName());
        if (onDone)
            onDone(false);
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
            if (self->bottomBar != nullptr)
                self->bottomBar->setWaveformStatusMessage("Load failed.");
            self->showSongLoadFailedMessage(song.songName,
                                            "The video file could not be opened for playback.",
                                            audioFile.getFullPathName());
            if (onDone)
                onDone(false);
            return;
        }

        self->currentSong         = song;
        self->currentSongImageUrl = juce::String (song.imageUrl);
        self->currentSongDuration = self->lyricWindow_->getVideoDuration();
        self->playingDocWritten_  = false;
        self->videoFinishedFired_ = false;
        self->lastVideoPositionSec_ = -1.0;
        self->currentPitchSemitones_ = (float) pitchSemitones;
        self->currentSongVersion_ = (versionIndex >= 0 && versionIndex < (int) song.version.size())
                                        ? juce::String (song.version[(size_t) versionIndex])
                                        : juce::String();

        if (self->topBar)
        {
            juce::String version = self->currentSongVersion_;

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
            self->bottomBar->setWaveformStatusMessage("Video loaded.");
        }

        if (self->queueBar) self->queueBar->setPlaying (autoStart);
        self->currentRibbonCdgFile_ = juce::File();
        self->refreshRibbonState();

        if (autoStart)
        {
            self->playStartTimeMs_ = juce::Time::currentTimeMillis();
            // Fade out background music so it doesn't overlap karaoke.
            if (self->bgPlayer_ != nullptr)
                self->bgPlayer_->fadeOut (1.5f);
            self->writePlayingDocIfNeeded();
        }

        if (onDone)
            onDone(true);
        return;
    }

    if (! self->audioEngine->loadSong(audioFile))
    {
        DBG("loadAndPlaySong: AudioEngine failed to load " + audioFile.getFullPathName());
        if (self->bottomBar != nullptr)
            self->bottomBar->setWaveformStatusMessage("Load failed.");
        self->showSongLoadFailedMessage(song.songName,
                                        "The audio file format is unsupported or the file could not be opened.",
                                        audioFile.getFullPathName());
        if (onDone)
            onDone(false);
        return;
    }

    // Push the matching CDG file to the secondary lyric display. If no CDG
    // companion exists the window will simply fall back to its idle screen.
    const auto cdgFile = extractedCdgFile.existsAsFile()
        ? extractedCdgFile
        : self->resolveCdgFileFor (audioFile);
    self->currentRibbonCdgFile_ = cdgFile;

    if (self->lyricWindow_)
    {
        self->lyricWindow_->loadCDG (cdgFile);
    }

    self->currentSong          = song;
    self->currentSongImageUrl  = juce::String(song.imageUrl);
    self->currentSongDuration  = self->audioEngine->getTotalLength();
    self->playStartTimeMs_     = 0;
    self->playingDocWritten_   = false;
    self->currentSongVersion_  = (versionIndex >= 0 && versionIndex < (int) song.version.size())
                                    ? juce::String (song.version[(size_t) versionIndex])
                                    : juce::String();
    self->refreshRibbonState();

    // Apply pitch (the dialog's semitone adjustment)
    self->audioEngine->setPitchShift((float) pitchSemitones);
    self->currentPitchSemitones_ = (float) pitchSemitones;

    // TopBar — song info
    if (self->topBar)
    {
        juce::String version = self->currentSongVersion_;

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
        self->bottomBar->setWaveformStatusMessage("Analyzing waveform...");

        // Build a real waveform asynchronously.
        juce::Component::SafePointer<BottomBar> safeBottom(self->bottomBar.get());
        WaveformGenerator::generateAsync(audioFile, 240, [safeBottom](std::vector<float> peaks) {
            if (safeBottom)
            {
                safeBottom->setWaveformSamples(peaks);
                safeBottom->setWaveformStatusMessage(peaks.empty() ? "Waveform unavailable." : juce::String());
            }
        });
    }

    if (self->queueBar) self->queueBar->setPlaying (autoStart);

    if (autoStart)
    {
        self->playStartTimeMs_ = juce::Time::currentTimeMillis();
        // Fade out background music so it doesn't overlap karaoke.
        if (self->bgPlayer_ != nullptr)
            self->bgPlayer_->fadeOut (1.5f);
        self->audioEngine->play();
        self->writePlayingDocIfNeeded();
    }
    if (onDone)
        onDone(true);
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

bool MainComponent::queueAndLoadNextSingerSong(bool autoStartAfterLoad,
                                               bool showNoSongsMessage)
{
    if (queueBar == nullptr || !queueBar->onPlaySinger)
        return false;

    const auto venueId = activeVenueId_;
    auto current = queueBar->getSingers();
    if (current.empty())
        return false;

    bool anyPlayableSinger = false;
    for (const auto& s : current)
    {
        if (! s.songs.empty())
        {
            anyPlayableSinger = true;
            break;
        }
    }

    // If no singer has any queued songs, stop auto-rotation and keep the
    // transport idle when Play is pressed.
    if (! anyPlayableSinger)
    {
        queueAutoStartRequested_ = false;
        if (showNoSongsMessage)
            showMaintenanceToast("No queued songs available.");
        return false;
    }

    // Rotate past singers with no songs and stop at the first playable
    // singer. This is a "turn ends without singing" primitive, just like a
    // song finishing: the RR itself (`order`) is untouched -- only the
    // rotation anchor advances (and the skipped singer is dropped from the
    // RR entirely if they've now exhausted their strikes).
    const int maxAttempts = (int) current.size();
    for (int attempt = 0; attempt < maxAttempts; ++attempt)
    {
        auto rr = QueueRotation::sortByStableOrder(queueBar->getSingers());
        if (rr.empty())
            return false;

        auto anchorId = QueueRotation::findAnchorId(rr);
        QueueRotation::stampDerivedRanks(rr, anchorId);

        int frontIdx = -1;
        for (int i = 0; i < (int) rr.size(); ++i)
        {
            if (rr[(size_t) i].rotationOrder == 0)
            {
                frontIdx = i;
                break;
            }
        }
        if (frontIdx < 0)
            return false;

        if (! rr[(size_t) frontIdx].songs.empty())
        {
            // The anchor is always display index 0 (see deriveDisplayQueue).
            queueAutoStartRequested_ = autoStartAfterLoad;
            queueBar->onPlaySinger(0);
            return true;
        }

        auto& frontSinger = rr[(size_t) frontIdx];
        const int frontOrder = frontSinger.order;
        bool removedForStrikes = false;

        if (! frontSinger.isHost)
        {
            const int currentStrikes = juce::jmax(0, frontSinger.strikes);
            const int nextStrikes = currentStrikes + 1;

            if (activeVenueNumStrikes_ > 0 && nextStrikes >= activeVenueNumStrikes_)
            {
                removedForStrikes = true;
                if (! venueId.isEmpty())
                    QueueService::getInstance().deleteSinger(venueId, juce::String(frontSinger.id), nullptr);
            }
            else
            {
                frontSinger.strikes = nextStrikes;
            }
        }

        const auto newAnchorId = QueueRotation::advanceAnchor(rr, frontOrder);
        if (removedForStrikes)
            rr.erase(rr.begin() + frontIdx);
        QueueRotation::stampDerivedRanks(rr, newAnchorId);

        queueBar->setSingers(QueueRotation::deriveDisplayQueue(rr, newAnchorId));

        if (! venueId.isEmpty())
            QueueService::getInstance().persistSingerOrder(venueId, rr,
                [](bool ok, juce::String err)
                {
                    if (! ok)
                        DBG("[Queue] queueAndLoadNextSingerSong persistSingerOrder failed: " << err);
                });
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
    const juce::String authUid = FirestoreClient::getInstance().getUserId().trim();
    const juce::String hostId = authUid.isNotEmpty()
        ? authUid
        : juce::String(! h.profileId.empty() ? h.profileId : h.userId).trim();

    int existingHostIndex = -1;
    if (hostId.isNotEmpty())
    {
        for (int i = 0; i < (int) merged.size(); ++i)
        {
            auto& s = merged[(size_t) i];
            if (s.isHost || juce::String(s.id).trim() == hostId)
            {
                existingHostIndex = i;
                s.isHost = true;
                // Keep Firestore songs; update display name/avatar from credentials.
                if (! h.stageName.empty()) s.name = h.stageName;
                else if (! h.fullName.empty()) s.name = h.fullName;
                if (! h.avatarUrl.empty()) s.avatar = h.avatarUrl;
                break;
            }
        }
    }

    Singers hostSinger;
    hostSinger.id     = hostId.toStdString();
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

    if (existingHostIndex >= 0)
    {
        // No synthetic insertion needed when a real host doc is already present.
    }
    else
    {
        preferredIndex = juce::jlimit(0, (int) merged.size(), preferredIndex);
        merged.insert(merged.begin() + preferredIndex, std::move(hostSinger));
    }

    // Preserve/assign "new singer" highlight across watcher refreshes:
    // - Existing highlighted singers remain highlighted until they've had a turn.
    // - Newly discovered non-host singers with queued songs are highlighted.
    std::unordered_map<std::string, bool> existingNewFlags;
    if (queueBar != nullptr)
    {
        for (const auto& existing : queueBar->getSingers())
        {
            if (existing.isHost)
                continue;

            juce::String key = juce::String(existing.id).trim();
            if (key.isEmpty())
                key = "name:" + juce::String(existing.name).trim().toLowerCase();

            existingNewFlags[key.toStdString()] = existing.isNewlyAdded;
        }
    }

    for (auto& s : merged)
    {
        if (s.isHost)
        {
            s.isNewlyAdded = false;
            continue;
        }

        juce::String key = juce::String(s.id).trim();
        if (key.isEmpty())
            key = "name:" + juce::String(s.name).trim().toLowerCase();

        const auto it = existingNewFlags.find(key.toStdString());
        if (it != existingNewFlags.end())
            s.isNewlyAdded = false; // already present last cycle: highlight window has elapsed
        else
            s.isNewlyAdded = ! s.songs.empty();
    }

    // Derive the actual displayed Queue from the stable Round Robin: sort
    // by RR position (host forced first), find who's currently "up next"
    // (the anchor), and rotate to start there. This is read-only -- it does
    // not persist anything back to Firestore; only genuine mutations
    // (finish/remove/drag/move) do that, via QueueRotation + persistSingerOrder.
    const auto rr = QueueRotation::sortByStableOrder(merged);
    const auto anchorId = QueueRotation::findAnchorId(rr);
    return QueueRotation::deriveDisplayQueue(rr, anchorId);
}

std::vector<LyricDisplayComponent::QueuePreviewEntry>
MainComponent::buildLyricQueuePreview(const std::vector<Singers>& singers) const
{
    std::vector<LyricDisplayComponent::QueuePreviewEntry> preview;
    preview.reserve (3);

    for (const auto& singer : singers)
    {
        if (singer.isHost || singer.songs.empty())
            continue;

        LyricDisplayComponent::QueuePreviewEntry entry;
        entry.singerName = juce::String (singer.name);
        entry.avatarPath = juce::String (singer.avatar);

        const auto& song = singer.songs.front();
        entry.songName = juce::String (song.songName);
        entry.artistName = juce::String (song.songArtist);

        preview.push_back (std::move (entry));
        if ((int) preview.size() >= 3)
            break;
    }

    return preview;
}

void MainComponent::syncLyricIdlePreview(const std::vector<Singers>& singers)
{
    if (lyricWindow_ == nullptr)
    {
        refreshRibbonState();
        return;
    }

    lyricWindow_->setVenueContext (activeVenueId_, activeVenueName_);
    lyricWindow_->setQueuePreview (buildLyricQueuePreview (singers));
    syncLyricNowSingingSummary();
    syncLyricLowerThirdNextUp(singers);
    refreshRibbonState();
}

void MainComponent::refreshRibbonState()
{
    if (ribbonMenu == nullptr)
        return;

    // Background music state comes from the independent bgPlayer_, not the karaoke engine.
    if (bgPlayer_ != nullptr)
    {
        ribbonMenu->setBackgroundState(bgPlayer_->isPlaying(), bgPlayer_->getVolume());
        ribbonMenu->setBackgroundTrackInfo(bgPlayer_->getCurrentTrackName(),
                                           bgPlayer_->getCurrentPosition(),
                                           bgPlayer_->getTotalLength());
    }
    else
    {
        ribbonMenu->setBackgroundState(false, 0.5f);
        ribbonMenu->setBackgroundTrackInfo({}, 0.0, 0.0);
    }

    ribbonMenu->setLyricPreviewFile(currentRibbonCdgFile_);

    const bool lyricVisible = lyricWindow_ != nullptr && lyricWindow_->isVisible();
    const bool lyricFull = lyricWindow_ != nullptr && lyricWindow_->isTrulyFullScreen();
    ribbonMenu->setLyricWindowVisible(lyricVisible);
    ribbonMenu->setLyricWindowFullScreen(lyricFull);

    if (bottomBar != nullptr)
        bottomBar->setLyricScreenExpanded(lyricFull);

    juce::String nextSingerName;
    juce::String nextSongName;
    if (queueBar != nullptr)
    {
        for (const auto& singer : queueBar->getSingers())
        {
            if (singer.isHost || singer.songs.empty())
                continue;

            nextSingerName = juce::String(singer.name).trim();
            nextSongName = juce::String(singer.songs.front().songName).trim();
            break;
        }
    }

    ribbonMenu->setNextSinger(nextSingerName, nextSongName);
}

juce::String MainComponent::buildLyricLowerThirdNextUpSinger(const std::vector<Singers>& singers) const
{
    for (const auto& singer : singers)
    {
        if (singer.isHost || singer.songs.empty())
            continue;

        const auto name = juce::String(singer.name).trim();
        if (name.isNotEmpty())
            return name;
    }

    return {};
}

void MainComponent::syncLyricLowerThirdNextUp(const std::vector<Singers>& singers)
{
    if (lyricWindow_ == nullptr)
        return;

    if (lyricLowerThirdHoldNowSinging_ && hasLocalNowPlaying_)
    {
        const auto nowName = juce::String(localNowPlaying_.name).trim();
        if (nowName.isNotEmpty())
        {
            lyricWindow_->setLowerThirdNextUpSinger(nowName);
            return;
        }
    }

    lyricWindow_->setLowerThirdNextUpSinger(buildLyricLowerThirdNextUpSinger(singers));
}

void MainComponent::syncLyricNowSingingSummary()
{
    if (lyricWindow_ == nullptr)
        return;

    if (! hasLocalNowPlaying_)
    {
        lyricWindow_->setNowSingingInfo ({}, {}, {}, {});
        return;
    }

    juce::String singerName = juce::String (localNowPlaying_.name).trim();
    juce::String songName;
    juce::String artistName;
    juce::String avatarPath = juce::String (localNowPlaying_.avatar).trim();

    if (! localNowPlaying_.songs.empty())
    {
        const auto& nowSong = localNowPlaying_.songs.front();
        songName = juce::String (nowSong.songName).trim();
        artistName = juce::String (nowSong.songArtist).trim();
        if (avatarPath.isEmpty())
            avatarPath = juce::String (nowSong.singerAvatar).trim();
    }
    else
    {
        songName = juce::String (currentSong.songName).trim();
        artistName = juce::String (currentSong.artistName).trim();
    }

    lyricWindow_->setNowSingingInfo (singerName, songName, artistName, avatarPath);
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
        menuBarIcon_.reset();
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

        if (menuBarIcon_ == nullptr)
        {
            auto appDir = juce::File::getSpecialLocation (juce::File::currentExecutableFile).getParentDirectory();
            auto iconFile = appDir.getChildFile ("assets/images/Encore-E.png");
            auto icon = juce::ImageFileFormat::loadFrom (iconFile);

            if (icon.isValid())
            {
                menuBarIcon_ = std::make_unique<juce::ImageComponent>();
                menuBarIcon_->setImage (icon, juce::RectanglePlacement::centred);
                addAndMakeVisible (menuBarIcon_.get());
            }
        }
    }
    resized();
   #endif
}

//==============================================================================
void MainComponent::setMenuBarVisible (bool visible)
{
   #if ! JUCE_MAC
    if (menuBar_ != nullptr)
        menuBar_->setVisible (visible);
    if (menuBarIcon_ != nullptr)
        menuBarIcon_->setVisible (visible);
    resized();
   #else
    juce::ignoreUnused (visible);
   #endif
}

//==============================================================================
void MainComponent::setMainScreenFullscreenIcon (bool expanded)
{
    if (bottomBar != nullptr)
        bottomBar->setMainScreenExpanded (expanded);
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
        VenueSessionService::getInstance().stopHeartbeat();
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

            // Defensive cleanup: wipe any stale now-playing doc left behind
            // by a previous session that crashed/closed mid-song, so it
            // doesn't confuse the mobile app on this fresh load.
            safe->clearPlayingDoc();

            // Push the full venue snapshot into the Settings page so the admin
            // can edit any field. Saves are routed through MainArea's
            // onVenueSettingsChanged callback wired in the constructor.
            if (safe->mainArea != nullptr)
            {
                safe->mainArea->setVenueData (v);
                safe->mainArea->setActiveVenueId (venueId);
            }

            // Make sure this venue has a current songbook.json in Firebase
            // Storage -- the TAGG mobile app reads it directly, and a venue
            // that's never been scanned from any PC (or whose Storage
            // folder was never created) leaves TAGG showing an error when a
            // customer tries to connect. If there's no local songbook to
            // fall back on either, kick off the same initial-scan flow used
            // for a first-time venue switch (unless that's already about to
            // run below via requestInitialScan).
            if (! requestInitialScan)
            {
                SongbookStorageService::getInstance().ensureSongbookExists (venueId,
                    [safe, venueId] (bool exists, bool justUploaded, juce::String error)
                    {
                        if (safe == nullptr || safe->activeVenueId_ != venueId)
                            return;

                        if (justUploaded)
                        {
                            // We just uploaded the local file as-is, so it's
                            // definitionally in sync -- no need to check.
                            DBG ("[Songbook] Uploaded local songbook.json to Storage for venue " << venueId);
                        }
                        else if (! exists)
                        {
                            DBG ("[Songbook] No songbook.json in Storage and nothing local to fall back on"
                                 << (error.isNotEmpty() ? (" (" + error + ")") : juce::String())
                                 << " -- starting initial scan for venue " << venueId);
                            if (safe->mainArea != nullptr)
                                safe->mainArea->triggerInitialSongLoad();
                        }
                        else
                        {
                            // Storage already had a songbook untouched by the
                            // call above -- verify it actually matches what
                            // this PC has scanned locally. TAGG reads the
                            // Storage copy directly to know what it can
                            // request, so drift here means TAGG can offer
                            // songs the queue doesn't have (or miss ones it
                            // does), silently, until someone notices.
                            safe->checkSongbookSyncAndPromptIfNeeded (venueId);
                        }
                    });
            }

            safe->refreshSettingsUsers();
            safe->refreshSettingsInvitations();
            safe->refreshSettingsSessionStats();

            // Start the nightly archive + clear timer for this venue. The
            // service reads the configured cleanup hour from UserPreferences
            // each minute, so a settings change takes effect immediately.
            ArchiveService::getInstance().startNightlyCleanup(
                juce::String(v.id), juce::String(v.name));

            // Presence heartbeat so other devices logging into this same
            // venue can be warned "this appears to be live elsewhere" --
            // see VenueSessionService and the login-time checks in
            // LoginWindow.cpp. startHeartbeat() stops any previous venue's
            // heartbeat internally, so this is safe on every venue switch.
            VenueSessionService::getInstance().startHeartbeat(venueId);

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

                    safe->syncLyricNowSingingSummary();

                    safe->queueBar->setSingers (safe->composeQueueWithHost(snap.singers));
                    safe->syncLyricLowerThirdNextUp(safe->queueBar->getSingers());

                    // Ensure the host always has a real Firestore queue doc
                    // (created on first venue load, no-op thereafter).
                    if (HostService::getInstance().hasCurrent())
                    {
                        const auto hostInfo = HostService::getInstance().getCurrent();
                        const juce::String hostUid = FirestoreClient::getInstance().getUserId().trim();
                        if (hostUid.isNotEmpty())
                        {
                            QueueService::getInstance().ensureHostQueueDoc(
                                vid,
                                hostUid,
                                juce::String(hostInfo.stageName),
                                juce::String(hostInfo.avatarUrl),
                                [](bool ok, juce::String err)
                                {
                                    if (! ok)
                                        DBG("[Queue] ensureHostQueueDoc failed: " << err);
                                    // No reload needed — composeQueueWithHost already
                                    // showed the correct queue; the watcher will pick
                                    // up the new doc on its next poll cycle.
                                });
                        }
                    }

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
    juce::Image avatarImage;
    UserRole hostRole = UserRole::Host;

    if (HostService::getInstance().hasCurrent())
    {
        const auto host = HostService::getInstance().getCurrent();
        hostRole = host.role;

        displayName = juce::String(host.stageName).trim();
        if (displayName.isEmpty())
            displayName = juce::String(host.fullName).trim();

        const auto avatarUrl = juce::String(host.avatarUrl).trim();
        if (avatarUrl.isNotEmpty())
        {
            avatarImage = ArtworkCache::getInstance().resolveAvatar(avatarUrl,
                [safe = juce::Component::SafePointer<MainComponent>(this)]
                {
                    // Simplest safe way to pick up a just-finished avatar
                    // download (now cached) is to re-run the whole identity
                    // refresh -- it's cheap and idempotent.
                    if (safe != nullptr)
                        safe->applyCurrentIdentityToUi();
                });
        }
    }

    if (displayName.isEmpty())
    {
        const auto& fc = FirestoreClient::getInstance();
        displayName = fc.getDisplayName().trim();
        if (displayName.isEmpty())
            displayName = fc.getEmail().trim();
    }

    topBar->setUserInfo(displayName, avatarImage);

    if (navBar != nullptr)
    {
        // Gates enterpriseAdminOnly nav items (Customer Admin) on the
        // account's real global role -- independent of the venue-scoped
        // role applyNavRoleForActiveVenue() resolves below, so a venue
        // association can never mask a genuine EnterpriseAdmin account.
        navBar->setGlobalHostRole(hostRole);

        // If no venue is active yet, at least expose host-level rights for
        // everything else (the normal AccessRights-table-driven items).
        if (activeVenueId_.isEmpty())
            navBar->setUserRole(hostRole);
    }
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

    // Watch /emojis for cheer reactions sent by the TAGG mobile app while a
    // singer is performing, and animate each one on the lyric display.
    auto& es = EmojiService::getInstance();
    es.onNewEmoji = [safe] (const std::vector<Emoji>& newEmojis)
    {
        if (safe == nullptr || safe->lyricWindow_ == nullptr)
            return;
        for (const auto& e : newEmojis)
            safe->lyricWindow_->addEmoji (e);
    };
    es.start (venueId);
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
                    "rejected", juce::String(juce::CharPointer_UTF8("Server error \xe2\x80\x94 please try again.")));
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

//==============================================================================
// venues/<id>/playing — "now playing" doc
Playing MainComponent::buildPlayingFromCurrentState() const
{
    Playing p;
    p.artists      = currentSong.artistName;
    p.durationMS   = (int) std::round (currentSongDuration * 1000.0);
    p.genres       = currentSong.genres;
    p.imageUrl     = currentSongImageUrl.toStdString();
    p.keySignature = currentSong.keySignature;
    p.songName     = currentSong.songName;
    p.songId       = currentSong.id;
    p.songVersion  = currentSongVersion_.toStdString();
    p.pitch        = currentPitchSemitones_;
    p.releaseDate  = currentSong.releaseDate;
    p.tempo        = currentSong.tempo;
    p.type         = "karaoke"; // No singalong mode in this app (matches AuditService).

    p.singerName   = hasLocalNowPlaying_ ? localNowPlaying_.name : std::string();
    p.avatar       = hasLocalNowPlaying_ ? localNowPlaying_.avatar : std::string();
    p.profileId    = hasLocalNowPlaying_ ? localNowPlaying_.id : std::string();
    p.deviceId     = hasLocalNowPlaying_ ? localNowPlaying_.deviceId : std::string();

    if (HostService::getInstance().hasCurrent())
        p.kdId = HostService::getInstance().getCurrent().userId;

    return p;
}

void MainComponent::writePlayingDocIfNeeded()
{
    if (playingDocWritten_ || activeVenueId_.isEmpty())
        return;

    playingDocWritten_ = true;
    const auto venueId = activeVenueId_;
    auto playing = buildPlayingFromCurrentState();
    juce::Component::SafePointer<MainComponent> safe (this);

    // Enforce "at most one playing doc, and only while someone is actually
    // singing": always delete-then-create, sequenced (not fired in
    // parallel), so a KJ jumping straight from one singer to another
    // (without pressing Skip/Stop first) can never leave two playing docs
    // stacked, and the delete can't race ahead of or behind the create.
    // Skip/Stop/natural-finish's own clearPlayingDoc() calls become
    // redundant-but-harmless backstops rather than the only cleanup.
    VenueService::getInstance().removeAllCurrentSongPlaying(
        [safe, venueId, playing](bool ok, juce::String err)
        {
            if (! ok) DBG ("[Playing] removeAllCurrentSongPlaying (pre-write) failed: " << err);
            if (safe == nullptr || safe->activeVenueId_ != venueId)
                return;

            VenueService::getInstance().addCurrentSongPlaying (playing,
                [](bool ok2, juce::String err2)
                {
                    if (! ok2) DBG ("[Playing] addCurrentSongPlaying failed: " << err2);
                });
        });
}

void MainComponent::clearPlayingDoc()
{
    playingDocWritten_ = false;
    if (activeVenueId_.isEmpty())
        return;

    VenueService::getInstance().removeAllCurrentSongPlaying(
        [](bool ok, juce::String err)
        {
            if (! ok) DBG ("[Playing] removeAllCurrentSongPlaying failed: " << err);
        });
}

//==============================================================================
// Shared "a song has ended" handler — see the header comment on
// handleSongFinished() for why this is a named method rather than staying
// inline inside AudioEngine::onSongFinished.
void MainComponent::handleSongFinished()
{
    logPlayHistoryIfNeeded(true);

    // Karaoke song ended — fade background music back in to fill the silence.
    if (bgPlayer_ != nullptr)
        bgPlayer_->fadeIn (2.0f);

    if (lyricWindow_ != nullptr)
        lyricWindow_->setForceIdleScreen (true);

    // Soft-clear emoji reactions: the idle screen (just forced above)
    // never paints them and LyricDisplayComponent stops accepting new
    // spawns while idle, so nothing new appears; whatever was already
    // mid-animation keeps quietly finishing in the background and
    // deletes its own doc. This bulk clear is just a backstop so
    // nothing carries over to the next singer.
    if (activeVenueId_.isNotEmpty())
        VenueService::getInstance().clearEmojis (activeVenueId_);

    // Song has genuinely ended — remove the now-playing doc (unlike pause,
    // which deliberately leaves it in place).
    clearPlayingDoc();

    if (queueBar == nullptr) return;

    // The round robin already cycled the moment this singer was promoted
    // into Now Singing (see onPlaySinger -- it advances the anchor and
    // removes the song from their queue right then, so the queue's top
    // slot never shows the person currently performing). All that's left
    // to do here is bump their completed-songs count; the RR/anchor and
    // their songs[] list are untouched.
    if (hasLocalNowPlaying_)
    {
        auto singers = queueBar->getSingers();
        for (auto& s : singers)
        {
            if (juce::String(s.id).trim() == juce::String(localNowPlaying_.id).trim()
                || juce::String(s.name).trim().equalsIgnoreCase(juce::String(localNowPlaying_.name).trim()))
            {
                s.songsPerformed += 1;
                break;
            }
        }
        queueBar->setSingers(singers);
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
}
