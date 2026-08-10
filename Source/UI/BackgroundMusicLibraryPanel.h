/*
  ==============================================================================

    BackgroundMusicLibraryPanel.h

    Source picker shown below the transport controls in the Ribbon's
    Background Music full-screen view (Source/UI/RibbonMenu.cpp) when that
    panel is expanded. Two sources:
      - Local Folder: a folder picker + track checklist (the original,
        default behaviour -- lets a host point background music at their
        own folder and choose which tracks in it actually play).
      - Spotify: remote-controls whatever Spotify app/device the host
        already has running via Source/Services/SpotifyService.h -- audio
        plays through Spotify's own output, never through this app's
        engine (Spotify is DRM-locked, like every other streaming service).

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include <vector>
#include "../Services/SpotifyService.h"

class BackgroundMusicLibraryPanel : public juce::Component
{
public:
    BackgroundMusicLibraryPanel();
    ~BackgroundMusicLibraryPanel() override;

    void resized() override;

    /** "local" or "spotify" -- shows/hides the two sections and updates the
        toggle buttons' highlighted state. */
    void setSource (const juce::String& source);

    //--- Local Folder section ---------------------------------------------------
    /** Reflects the currently configured folder in the path label. Empty
        means "using the bundled default." */
    void setFolderPath (const juce::String& path);

    /** Rebuilds the checklist from `tracks` (typically
        BackgroundMusicPlayer::getAvailableTracks()). `selectedFilenames`
        empty means every track starts checked. Call whenever the folder or
        the persisted selection changes underneath this panel. */
    void setTracks (const std::vector<juce::File>& tracks, const juce::StringArray& selectedFilenames);

    std::function<void (juce::File folder)> onFolderChanged;
    std::function<void (juce::StringArray selectedFilenames)> onSelectionChanged;
    std::function<void (juce::File file)> onPreviewRequested;
    std::function<void()> onUseDefaultRequested;

    //--- Spotify section ---------------------------------------------------------
    /** Reflects connection + playlist state. `accountName` is only shown
        when connected; `playlists` populates the picker (empty is fine --
        shows a placeholder); `selectedUri` pre-selects one if it's still
        in the list. */
    void setSpotifyState (bool connected, const juce::String& accountName,
                         const std::vector<SpotifyService::PlaylistInfo>& playlists,
                         const juce::String& selectedUri);

    std::function<void (juce::String source)> onSourceChanged;
    std::function<void (juce::String clientId)> onSpotifyClientIdChanged;
    std::function<void()> onSpotifyConnectRequested;
    std::function<void()> onSpotifyDisconnectRequested;
    std::function<void()> onSpotifyPlaylistsRefreshRequested;
    std::function<void (juce::String uri, juce::String name)> onSpotifyPlaylistSelected;

    /** Seeds the Client ID field once, e.g. right after construction. */
    void setSpotifyClientId (const juce::String& clientId);

    /** MainComponent calls this once SpotifyService::playPlaylist's
        callback fires, so a failure (most commonly "no active device" --
        Spotify Connect needs the host to already have Spotify open
        somewhere) is actually visible instead of silently doing nothing. */
    void reportSpotifyPlaybackResult (bool ok, const juce::String& error);

private:
    class TrackRow;

    void browseForFolder();
    void handleSelectionChanged();
    void updateSourceVisibility();

    juce::String currentSource_ { "local" };
    juce::TextButton sourceLocalButton_   { "sourceLocal" };
    juce::TextButton sourceSpotifyButton_ { "sourceSpotify" };

    // Local Folder controls
    juce::Label folderPathLabel_;
    juce::TextButton browseButton_ { "bgBrowse" };
    juce::TextButton useDefaultButton_ { "bgUseDefault" };

    juce::Viewport viewport_;
    std::unique_ptr<juce::Component> listContent_;
    std::vector<std::unique_ptr<TrackRow>> rows_;

    juce::File currentFolder_;
    std::unique_ptr<juce::FileChooser> fileChooser_;

    // Spotify controls
    juce::Label spotifyClientIdLabel_;
    juce::TextEditor spotifyClientIdEditor_;
    juce::TextButton spotifyConnectButton_ { "spotifyConnect" };
    juce::TextButton spotifyDisconnectButton_ { "spotifyDisconnect" };
    juce::Label spotifyStatusLabel_;
    juce::Label spotifyPlaylistLabel_;
    juce::ComboBox spotifyPlaylistCombo_;
    juce::TextButton spotifyRefreshPlaylistsButton_ { "spotifyRefreshPlaylists" };
    bool spotifyConnected_ = false;
    std::vector<juce::String> spotifyPlaylistUris_; // parallel to spotifyPlaylistCombo_'s items

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BackgroundMusicLibraryPanel)
};
