/*
  ==============================================================================

    SongEditDialog.h

    Modal "Edit Song" dialog opened from the SearchPage Edit column.
    Mirrors the Angular edit-song-modal-component:

      • Album-art preview (top)
      • Artist / Song fields with a swap button between them
      • Song ID (read-only) + Release Date
      • Version + Tempo (BPM)
      • Key + Duration (mm:ss)
      • Genres (comma-separated)
      • Image URL (album art)
      • Playlist checkboxes: Add to New / Popular / Recommended
      • Get Metadata button — populates fields from the metadata API
      • Delete Song button — removes from songbook + Firestore
      • Status message line for inline feedback
      • Cancel / Save action buttons

    The dialog itself only mutates an in-memory copy.  Persisting the result
    (songbook/Firestore writes, playlist patches, song deletion) is the
    caller's responsibility, signalled via the SongEditResult.action field.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "../Models/CdgSong.h"

//==============================================================================
struct SongEditResult
{
    enum class Action { Save, Cancel, Delete };

    Action  action = Action::Cancel;
    CdgSong song;                       // Updated record (Save) or original (Delete)

    // Playlist membership toggles — caller patches the venue's playlists
    // collection accordingly.  Always populated; only meaningful for Save.
    bool    addToNew         = false;
    bool    addToPopular     = false;
    bool    addToRecommended = false;

    // Helpers — true when the corresponding toggle changed from the
    // membership state the dialog was opened with.
    bool    newChanged         = false;
    bool    popularChanged     = false;
    bool    recommendedChanged = false;

    // Convenience for the "accepted any save action" check.
    bool isSave()   const { return action == Action::Save;   }
    bool isDelete() const { return action == Action::Delete; }
    bool isCancel() const { return action == Action::Cancel; }
};

//==============================================================================
class SongEditDialog : public juce::Component
{
public:
    /** Initial playlist membership flags — caller queries Firestore before
        opening the dialog so the boxes start in the correct state. */
    struct InitialPlaylists
    {
        bool inNew         = false;
        bool inPopular     = false;
        bool inRecommended = false;
    };

    SongEditDialog(const CdgSong& song, InitialPlaylists pls);
    ~SongEditDialog() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

    std::function<void(const SongEditResult&)> onResult;

    /** Optional: caller provides a function that performs an asynchronous
        metadata lookup (Spotify-style) for {artist, song} and calls back with
        a partially-populated CdgSong.  When unset, the Get Metadata button
        shows a "not configured" status. */
    using MetadataFetcher =
        std::function<void(juce::String artist,
                           juce::String song,
                           std::function<void(bool ok, CdgSong updated, juce::String message)>)>;
    MetadataFetcher onFetchMetadata;

    static void launch(juce::Component* parent,
                       const CdgSong& song,
                       InitialPlaylists pls,
                       MetadataFetcher fetcher,
                       std::function<void(const SongEditResult&)> onResult);

private:
    void closeWith(SongEditResult::Action action);
    void doSwap();
    void doGetMetadata();
    void doDelete();
    void setStatus(const juce::String& msg, juce::Colour colour);
    void clearStatus();

    /** (Re)load the album artwork from the supplied URL into artworkImage_,
        falling back to the "No artwork" placeholder when the URL is empty
        or fetch fails.  Async loads (via ArtworkCache) repaint when the
        download completes. */
    void refreshArtwork(const juce::String& url);

    static juce::String formatDuration(int durationMS);
    static int          parseDuration(const juce::String& s);

    CdgSong song_;
    InitialPlaylists initial_;

    juce::Label      titleLabel_;
    juce::TextButton closeButton_   { "X" };

    juce::ImageComponent artworkImage_;
    juce::Label          artworkPlaceholder_;

    // Row 1: Artist  [⇄]  Song
    juce::Label      artistFieldLabel_;
    juce::TextEditor artistEditor_;
    juce::TextButton swapButton_   { juce::String::charToString(0x21c4) }; // ⇄
    juce::Label      songFieldLabel_;
    juce::TextEditor songEditor_;

    // Row 2: Song ID (read-only)  Release Date
    juce::Label      songIdFieldLabel_;
    juce::TextEditor songIdEditor_;
    juce::Label      releaseFieldLabel_;
    juce::TextEditor releaseEditor_;

    // Row 3: Version  Tempo
    juce::Label      versionFieldLabel_;
    juce::TextEditor versionEditor_;
    juce::Label      tempoFieldLabel_;
    juce::TextEditor tempoEditor_;

    // Row 4: Key  Duration
    juce::Label      keyFieldLabel_;
    juce::TextEditor keyEditor_;
    juce::Label      durationFieldLabel_;
    juce::TextEditor durationEditor_;

    // Genres
    juce::Label      genresFieldLabel_;
    juce::TextEditor genresEditor_;

    // Image URL
    juce::Label      imageFieldLabel_;
    juce::TextEditor imageEditor_;

    // Playlists
    juce::Label         playlistsLabel_;
    juce::ToggleButton  addToNewToggle_         { "New" };
    juce::ToggleButton  addToPopularToggle_     { "Popular" };
    juce::ToggleButton  addToRecommendedToggle_ { "Recommended" };

    // Status + actions
    juce::Label      statusLabel_;
    juce::TextButton getMetadataButton_;
    juce::TextButton deleteButton_;
    juce::TextButton cancelButton_;
    juce::TextButton saveButton_;

    bool busy_ = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SongEditDialog)
};
