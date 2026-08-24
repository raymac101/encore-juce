/*
  ==============================================================================

    MainComponent.h
    Created: 15 Apr 2026 7:04:12pm
    Author:  GitHub Copilot

    Main application component with responsive design and localization

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include <unordered_set>
#include "ResponsiveLayout.h"
#include "../Localization/LocalizationManager.h"
#include "../Audio/AudioEngine.h"
#include "../Services/ImageCache.h"
#include "../Models/CdgSong.h"
#include "../Models/QueueItem.h"
#include "../Models/Singers.h"
#include "../Models/Playing.h"
#include "TopBar.h"
#include "NavBar.h"
#include "MainArea.h"
#include "QueueBar.h"
#include "RibbonMenu.h"
#include "SongSelectionDialog.h"
#include "LyricDisplayWindow.h"
#include "../Services/BackgroundMusicPlayer.h"

class BottomBar;

//==============================================================================
/**
    Main application component that serves as the root UI container.
    Provides responsive layout and coordinates between all major subsystems.
*/
class MainComponent : public ResponsiveLayout,
                      public juce::DragAndDropContainer,
                      public juce::Timer
{
public:
    //==============================================================================
    MainComponent();
    ~MainComponent() override;

    //==============================================================================
    // Component Overrides
    void paint(juce::Graphics&) override;
    void resized() override;
    
    //==============================================================================
    // Multi-language Support
    void changeLanguage(const juce::String& languageCode);
    void showLanguageSelector();
    void updateAllText();
    
    //==============================================================================
    // Screen Management
    void detectAndConfigureScreens();
    void setupDualScreenLayout();  // DJ screen + public display

    /** Accessor for the secondary (singer-facing) lyric window. May be null
        if the window couldn't be constructed. */
    LyricDisplayWindow* getLyricWindow() noexcept { return lyricWindow_.get(); }

    /** Install (or remove) the application's MenuBarModel. On Windows/Linux
        this embeds a MenuBarComponent at the top of the window. On macOS the
        system menu bar is used instead so this is a no-op. */
    void installMenuBarModel (juce::MenuBarModel* model);

    /** Show/hide the embedded menu bar row (Windows/Linux only) without
        destroying it -- used while the main window is in true fullscreen
        (see EncoreApplication::toggleMainFullscreen() in Main.cpp). No-op
        on macOS, where the system menu bar is unaffected by this window's
        fullscreen state. */
    void setMenuBarVisible (bool visible);

    /** Update the BottomBar's main-screen expand/collapse icon to match the
        main window's actual fullscreen state. Called from Main.cpp after
        toggling, whether triggered by the BottomBar button or the Window
        menu's Fullscreen item, so the icon never goes stale. */
    void setMainScreenFullscreenIcon (bool expanded);

    /** Fired when the user clicks the BottomBar's "expand main screen"
        button. MainComponent has no visibility into EncoreApplication's
        concrete type (defined in Main.cpp), so -- same hand-off pattern as
        onSignOutRequested below -- this is the request; Main.cpp wires it
        to actually toggle the main window's fullscreen state. */
    std::function<void()> onToggleMainFullscreenRequested;

    /** Load the active venue from Firestore and propagate its name/code to
        the queue bar and its logo + code to the lyric display window. Safe
        to call from the message thread; network work happens in background.
        If `requestInitialScan` is true, the user is switching to a venue
        that wasn't configured on this PC — once the venue loads we switch
        to the Library page and start the initial song-load flow. */
    void setVenueId (const juce::String& venueId, bool requestInitialScan = false);

    /** Switches straight to the Company Admin page and pushes company
        context into NavBar / MainArea's CompanyAdminPage. Called once from
        Main.cpp right after setVenueId() when the user picked "Manage
        Company" on the login window's venue-select screen rather than a
        specific venue (a venue is still loaded underneath -- MainComponent
        has no venue-less mode -- but the host lands on the company
        dashboard instead of Home). */
    void openCompanyDashboard (const juce::String& companyId, const juce::String& companyRole);

    /** Shows a small, persistent "Update" pill in TopBar, just left of the
        avatar (VS Code-style) -- stays until the host clicks it or the app
        restarts. Called either directly from UpdateService's
        checkForUpdates callback (if the download finishes while this
        MainComponent already exists), or from the constructor below (if it
        finished earlier, during the login/venue-selection flow). Never a
        modal dialog — a live show must never be blocked by this. */
    void showUpdateAvailableBanner (const juce::String& version);

    /** Fired when the user picks "Sign Out" from the TopBar user-menu
        dropdown, after this component has already stopped its own watchers
        and cleared session-scoped service state (see the dropdown handler
        in setupUI()). Main.cpp wires this to tear down the MainWindow and
        show the LoginWindow again -- MainComponent itself has no visibility
        into EncoreApplication's concrete type (defined in Main.cpp), so this
        callback is the hand-off point, the same pattern already used for
        menu-bar reattachment via installMenuBarModel(). */
    std::function<void()> onSignOutRequested;

    //==============================================================================
    // Accessibility
    void setHighContrastMode(bool enabled);
    void setLargeTextMode(bool enabled);

protected:
    //==============================================================================
    // ResponsiveLayout Overrides
    void updateUIForScreenSize() override;
    
    // Timer callback for periodic updates
    void timerCallback() override;

private:
    //==============================================================================
    // Audio playback -- declared before the UI components below so it is
    // constructed first and destroyed last. SettingsContentPanel (owned,
    // deeply nested, by mainArea) holds a raw AudioEngine* and unregisters
    // itself from its device manager in its own destructor; if audioEngine
    // were destroyed first, that would be a use-after-free during shutdown.
    std::unique_ptr<AudioEngine> audioEngine;
    std::unique_ptr<BackgroundMusicPlayer> bgPlayer_;

    // Throttles SpotifyService::getPlaybackState polling (driven from the
    // existing 20Hz timerCallback()) to roughly every 3s, matching
    // RequestService's polling cadence -- a real network call, not
    // something to run at timer frequency. Guarded against overlap since a
    // slow response could otherwise still be in flight when the next poll
    // would fire.
    juce::int64 lastSpotifyPollMs_ = 0;
    bool spotifyPollInFlight_ = false;

    // UI Components
    std::unique_ptr<TopBar> topBar;
    std::unique_ptr<BottomBar> bottomBar;
    std::unique_ptr<NavBar> navBar;
    std::unique_ptr<MainArea> mainArea;
    std::unique_ptr<QueueBar> queueBar;
    std::unique_ptr<RibbonMenu> ribbonMenu;
    std::unique_ptr<juce::Label> titleLabel;
    std::unique_ptr<juce::TextButton> languageButton;
    std::unique_ptr<juce::Label> statusLabel;
    std::unique_ptr<juce::Label> debugLabel;

    // Language selector popup
    std::unique_ptr<juce::PopupMenu> languageMenu;

    //==============================================================================
    CdgSong      currentSong;
    juce::String currentSongImageUrl;
    juce::String currentSongVersion_;
    float        currentPitchSemitones_ = 0.0f;
    double       currentSongDuration = 0.0;
    juce::File   currentRibbonCdgFile_;

    //==============================================================================
    // Secondary-monitor lyric / CDG display
    std::unique_ptr<LyricDisplayWindow> lyricWindow_;
    juce::String pendingVenueCode_;
    juce::Image pendingVenueLogo_;

    //==============================================================================
    // Embedded menu bar (Windows/Linux only — macOS uses the system bar).
    std::unique_ptr<juce::MenuBarComponent> menuBar_;
    std::unique_ptr<juce::ImageComponent> menuBarIcon_;

    /** Resolve the .cdg file that pairs with a given audio file (typically
        a sibling with the same base name). Returns juce::File{} if none is
        found. */
    juce::File resolveCdgFileFor (const juce::File& audioFile) const;

    /** Load (and optionally start playing) the chosen song. When `autoStart`
        is false the song is loaded into the audio engine and the top/bottom
        bars are updated, but playback is left paused — used by the queue
        flow where the host presses play on the now-singing avatar (or the
        bottom-bar transport) to actually start the track. */
    void loadAndPlaySong(const CdgSong& song,
                         int versionIndex,
                         int pitchSemitones,
                         bool autoStart = true,
                         std::function<void(bool)> onDone = nullptr);

    /** Show / hide a full-window loading overlay. */
    void showLoadingOverlay(const juce::String& message, double progress = -1.0);
    void updateLoadingOverlay(const juce::String& message, double progress = -1.0);
    void hideLoadingOverlay();
    void startDeferredAudioServices(const juce::String& venueId, int startupToken);

    //==============================================================================
    // /requested pipeline — start RequestService for the active venue and
    // run incoming requests through the auto-approve checks (queue closed,
    // max songs per singer, repeats). Mirrors Angular's processNewRequest /
    // processApprovedRequest / processRejectedRequest / processDeleteRequest
    // in queue-bar.component.ts.
    void startRequestPipelineFor (const juce::String& venueId);

    /** Manual "Reconnect Now" action -- forces RequestService and
        QueueService to drop any stuck in-flight/failure state and restart
        polling immediately for the active venue, without waiting out
        FirestoreClient's request watchdog. Wired to the reconnect prompt
        shown from onNetworkHealthChanged(). */
    void reconnectNetworkServices();

    /** Called for every incoming TAGG request (regardless of auto-approve
        outcome) so a song missing metadata gets enriched the moment someone
        actually requests it, not only during a library scan. Looks up the
        request's songId (falling back to artist/song name) in the local
        library; on a match with no metadata, kicks off a silent background
        ApiService::searchArtistAndSong() and -- if it finds something --
        upserts the result into the songbook (local + Firebase Storage,
        via LibraryPage::upsertSong(), which also refreshes Search/Home). */
    void enrichSongMetadataIfMissing (const juce::String& songId,
                                      const juce::String& artistHint,
                                      const juce::String& songHint);

    void onIncomingNewRequest      (const ::QueueItem& item);
    void onIncomingApprovedRequest (const ::QueueItem& item);
    void onIncomingRejectedRequest (const ::QueueItem& item);
    void onIncomingDeleteRequest   (const ::QueueItem& item);
    void reloadQueueFromFirestore  (const juce::String& venueId);

    class LoadingOverlay;
    std::unique_ptr<LoadingOverlay> loadingOverlay_;

    //==============================================================================
    // Background Tile
    juce::Image backgroundTileImage_;
    int backgroundTileSize_ = 340;
    void loadBackgroundTile(const juce::String& path = "");

    //==============================================================================
    // Application State
    bool highContrastMode = false;
    bool largeTextMode = false;
    bool isConnectedToFirebase = true;
    int startupLoadToken_ = 0;

    // Per-service health, merged into isConnectedToFirebase and TopBar's
    // status indicator by onNetworkHealthChanged(). Both start healthy;
    // only a real outage (several consecutive failed polls, see
    // RequestService/QueueService::kUnhealthyFailureThreshold) flips them.
    bool requestServiceHealthy_ = true;
    bool queueServiceHealthy_   = true;
    // Guards against re-showing the reconnect prompt every poll tick while
    // still unhealthy -- reset the next time we go fully healthy again.
    bool reconnectPromptShown_  = false;
    /** Re-derives isConnectedToFirebase from requestServiceHealthy_ &&
        queueServiceHealthy_, updates TopBar's indicator, and shows/resets
        the manual reconnect prompt on a transition. Call after updating
        either health flag. */
    void updateNetworkHealthUI();
    bool audioStartupInProgress_ = false;
    bool audioStartupComplete_ = false;

    // Non-empty while a Library scan/metadata/upload phase is in progress
    // ("Scanning Folders...", "Uploading Songbook...", etc.) -- takes
    // priority over the normal audio.feedback.* text in
    // updateAudioStatusIndicator() until LibraryPage reports it's done.
    juce::String librarySyncStatusMessage_;

    juce::String activeVenueId_;
    juce::String activeVenueName_;
    bool queueExpanded_ = false;
    // Whether "Start the Night" has been used (or bypassed) yet this
    // session -- reset to false on every fresh venue load in setVenueId().
    bool nightStarted_ = false;
    bool companyContextEnabled_ = false;
    juce::String companyId_;
    juce::String companyRole_;

    // Cached venue config — populated from VenueService::loadVenue and used
    // by the rotation/strikes logic when a singer is moved to now-singing.
    int activeVenueNumStrikes_ = 0;

    // Venue playlist membership — refreshed from Firestore on venue change
    // and after every Song-Edit save.  Powers both the home-page Popular /
    // Recommended rows and the SongEditDialog's initial checkbox state.
    std::unordered_set<std::string> popularSongIds_;
    std::unordered_set<std::string> recommendedSongIds_;
    std::unordered_set<std::string> newSongIds_;

    void loadVenuePlaylists();
    void applyCurrentIdentityToUi();
    void applyCompanyContextToUi();
    void applyStartupPageForCurrentIdentity();
    void refreshCompanyDashboard();
    void wireTestingPageCallbacks();
    void seedTestingQueue(const TestingPage::SeedOptions& options,
                          std::function<void(float)> onProgress,
                          std::function<void(bool, juce::String)> onDone);
    void applyNavRoleForActiveVenue();
    void refreshSettingsUsers();
    void refreshSettingsInvitations();
    void refreshSettingsSessionStats();
    void showMaintenanceToast(const juce::String& message);

    /** Compares this PC's local songbook.json against the copy already in
        Firebase Storage for `venueId` (see SongbookStorageService::
        checkSongbookInSync()) and, if they differ, asks the host whether to
        overwrite Storage with the local copy. No-op (silently) if they
        match or the check itself couldn't complete. */
    void checkSongbookSyncAndPromptIfNeeded(const juce::String& venueId);

    /** The actual out-of-sync check + dialog, run only after
        checkSongbookSyncAndPromptIfNeeded() has confirmed no other device
        currently looks live on this venue. */
    void runSongbookSyncCheck(const juce::String& venueId);

    /** Write a play-history entry if the song played long enough (>30 s).
        Pass `naturalEnd=true` when the audio finished on its own (always
        qualifies); `false` when the KJ skipped — checked against the 30 s
        threshold. Resets `playStartTimeMs_` to prevent duplicate writes. */
    void logPlayHistoryIfNeeded(bool naturalEnd);

    // Play-history tracking — set when a song starts, read by logPlayHistoryIfNeeded.
    juce::int64  playStartTimeMs_ = 0;

    //==============================================================================
    // venues/<id>/playing — "now playing" doc for the mobile app / other
    // clients. Written once per song start (including the first Play press
    // after a preload), left untouched across pause, and removed on manual
    // Stop, natural finish, or the KJ skipping/returning the singer to the
    // queue. Resuming from pause does NOT rewrite the doc.
    Playing buildPlayingFromCurrentState() const;
    void    writePlayingDocIfNeeded();
    void    clearPlayingDoc();
    bool    playingDocWritten_ = false;

    /** Shared "a song has ended" handler — the natural conclusion of both
        AudioEngine::onSongFinished (audio/CDG) and the video natural-end
        check driven from timerCallback() (MP4/M4V/MOV, which bypasses
        AudioEngine entirely). */
    void handleSongFinished();
    bool videoFinishedFired_ = false;
    double lastVideoPositionSec_ = -1.0;

    // Local "now singing" override. We mirror the Angular behaviour where
    // the now-playing card is purely UI state on the host machine — there
    // is no Firestore field for it. The QueueService watcher polls
    // /queue and, finding no singer with `currentlyUp`, would otherwise
    // clear the card on every tick. We therefore retain the card we set
    // when the host pressed Play and re-apply it inside the watcher /
    // loadQueue callbacks until the host explicitly clears or replaces it.
    Singers localNowPlaying_;
    bool    hasLocalNowPlaying_ = false;
    bool    lyricLowerThirdHoldNowSinging_ = false;
    
    //==============================================================================
    // UI Setup
    void setupUI();
    void setupLanguageButton();
    
    // Responsive configurations for different screen categories
    void configureForMobile();
    void configureForStandard();        // Standard laptop/desktop (HD, WXGA, etc.)
    void configureForWide();            // Wide screens (Full HD, QHD, etc.)
    void configureForUltraWide();       // Ultra-wide monitors (21:9, 32:9)
    void configureForHighResolution();  // 4K/5K/8K displays
    
    // Status updates
    void updateConnectionStatus();
    void updateDebugInfo();
    void updateAudioStatusIndicator();
    void setLibrarySyncStatusMessage(const juce::String& message);
    void runSongbookHealthCheckIfReady();
    void showSongUnavailableMessage(const QueueItem& item);
    void showSongLoadFailedMessage(const juce::String& songName,
                                   const juce::String& reason,
                                   const juce::String& path = {});
    bool queueAndLoadNextSingerSong(bool autoStartAfterLoad = false,
                                    bool showNoSongsMessage = false);
    std::vector<Singers> composeQueueWithHost(const std::vector<Singers>& queueSingers) const;
    void syncLyricIdlePreview(const std::vector<Singers>& singers);
    void refreshRibbonState();

    /** KJ manually moves the rotation anchor ("who's up next") one step
        forward or backward, without anyone actually performing -- the RR
        itself (`order`) is untouched, only `rotationOrder` (and thus the
        displayed Queue order) changes. Wraps around at either end. No-op
        if the queue has fewer than 2 singers. */
    void rotateQueueManually(bool forward);

    /** "Start the Night" -- plays the cached AI-voice intro (if one has
        been generated) then starts the first queued singer's song exactly
        as onPlayNextSinger already does. Marks nightStarted_ true so the
        Ribbon button reverts to its normal behaviour immediately, even if
        the intro itself fails to play. */
    void startTheNight();

    /** Scans assets/music/ (the same default background-music folder) for
        the intro config panel's music picker -- reuses whatever tracks are
        already bundled/added there rather than a separate sting library. */
    std::vector<juce::File> getAvailableIntroMusicTracks() const;
    void syncLyricNowSingingSummary();
    juce::String buildLyricLowerThirdNextUpSinger(const std::vector<Singers>& singers) const;
    void syncLyricLowerThirdNextUp(const std::vector<Singers>& singers);
    std::vector<LyricDisplayComponent::QueuePreviewEntry>
    buildLyricQueuePreview(const std::vector<Singers>& singers) const;

    bool pendingSongbookHealthCheck_ = true;
    bool songbookHealthPromptShown_ = false;
    std::unique_ptr<juce::Label> maintenanceToastLabel_;
    int maintenanceToastToken_ = 0;
    bool queueAutoStartRequested_ = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainComponent)
};