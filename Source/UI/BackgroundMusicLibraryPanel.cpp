/*
  ==============================================================================

    BackgroundMusicLibraryPanel.cpp

  ==============================================================================
*/

#include "BackgroundMusicLibraryPanel.h"
#include "../Localization/LocalizationManager.h"

namespace
{
    constexpr int kRowHeight = 28;
    const auto kText = juce::Colours::white;
    const auto kAccent = juce::Colour (0xff5a8fd8);
    const auto kInactiveBg = juce::Colour (0xff2d2d3a);
    const auto kDanger = juce::Colour (0xffd9534f);

    void initLabelWhite (juce::Label& l, const juce::String& text)
    {
        l.setText (text, juce::dontSendNotification);
        l.setColour (juce::Label::textColourId, kText);
        l.setFont (juce::Font (juce::FontOptions().withHeight (12.0f)));
    }
}

//==============================================================================
class BackgroundMusicLibraryPanel::TrackRow : public juce::Component
{
public:
    TrackRow (const juce::File& file, BackgroundMusicLibraryPanel& owner)
        : file_ (file), owner_ (owner)
    {
        addAndMakeVisible (checkbox_);
        checkbox_.onClick = [this] { owner_.handleSelectionChanged(); };

        nameLabel_.setText (file_.getFileNameWithoutExtension(), juce::dontSendNotification);
        nameLabel_.setColour (juce::Label::textColourId, kText);
        nameLabel_.setInterceptsMouseClicks (false, false);
        addAndMakeVisible (nameLabel_);
    }

    void resized() override
    {
        auto b = getLocalBounds();
        checkbox_.setBounds (b.removeFromLeft (30));
        nameLabel_.setBounds (b.reduced (4, 0));
    }

    void mouseUp (const juce::MouseEvent& e) override
    {
        if (! e.mouseWasDraggedSinceMouseDown() && owner_.onPreviewRequested)
            owner_.onPreviewRequested (file_);
    }

    const juce::File& getFile() const noexcept { return file_; }
    bool isChecked() const noexcept { return checkbox_.getToggleState(); }
    void setChecked (bool checked) { checkbox_.setToggleState (checked, juce::dontSendNotification); }

private:
    juce::File file_;
    BackgroundMusicLibraryPanel& owner_;
    juce::ToggleButton checkbox_;
    juce::Label nameLabel_;
};

//==============================================================================
BackgroundMusicLibraryPanel::BackgroundMusicLibraryPanel()
{
    auto& lm = LocalizationManager::getInstance();

    addAndMakeVisible (sourceLocalButton_);
    sourceLocalButton_.setButtonText (lm.getText ("ribbon.bg.source_local"));
    sourceLocalButton_.setClickingTogglesState (false);
    sourceLocalButton_.setColour (juce::TextButton::buttonColourId, kInactiveBg);
    sourceLocalButton_.setColour (juce::TextButton::buttonOnColourId, kAccent);
    sourceLocalButton_.onClick = [this] { if (onSourceChanged) onSourceChanged ("local"); };

    addAndMakeVisible (sourceSpotifyButton_);
    sourceSpotifyButton_.setButtonText (lm.getText ("ribbon.bg.source_spotify"));
    sourceSpotifyButton_.setClickingTogglesState (false);
    sourceSpotifyButton_.setColour (juce::TextButton::buttonColourId, kInactiveBg);
    sourceSpotifyButton_.setColour (juce::TextButton::buttonOnColourId, kAccent);
    sourceSpotifyButton_.onClick = [this] { if (onSourceChanged) onSourceChanged ("spotify"); };

    addAndMakeVisible (spotifyClientIdLabel_);
    initLabelWhite (spotifyClientIdLabel_, lm.getText ("ribbon.bg.spotify_client_id_label"));
    addAndMakeVisible (spotifyClientIdEditor_);
    spotifyClientIdEditor_.setTextToShowWhenEmpty (lm.getText ("ribbon.bg.spotify_client_id_placeholder"), kText.withAlpha (0.5f));
    spotifyClientIdEditor_.onFocusLost = [this]
    {
        if (onSpotifyClientIdChanged)
            onSpotifyClientIdChanged (spotifyClientIdEditor_.getText().trim());
    };

    addAndMakeVisible (spotifyConnectButton_);
    spotifyConnectButton_.setButtonText (lm.getText ("ribbon.bg.spotify_connect"));
    spotifyConnectButton_.onClick = [this] { if (onSpotifyConnectRequested) onSpotifyConnectRequested(); };

    addAndMakeVisible (spotifyDisconnectButton_);
    spotifyDisconnectButton_.setButtonText (lm.getText ("ribbon.bg.spotify_disconnect"));
    spotifyDisconnectButton_.onClick = [this] { if (onSpotifyDisconnectRequested) onSpotifyDisconnectRequested(); };

    addAndMakeVisible (spotifyStatusLabel_);
    spotifyStatusLabel_.setColour (juce::Label::textColourId, kText.withAlpha (0.85f));
    spotifyStatusLabel_.setFont (juce::Font (juce::FontOptions().withHeight (12.0f)));

    addAndMakeVisible (spotifyPlaylistLabel_);
    initLabelWhite (spotifyPlaylistLabel_, lm.getText ("ribbon.bg.spotify_playlist_label"));
    addAndMakeVisible (spotifyPlaylistCombo_);
    spotifyPlaylistCombo_.onChange = [this]
    {
        const int id = spotifyPlaylistCombo_.getSelectedId();
        if (id > 0 && id <= (int) spotifyPlaylistUris_.size() && onSpotifyPlaylistSelected)
            onSpotifyPlaylistSelected (spotifyPlaylistUris_[(size_t) (id - 1)], spotifyPlaylistCombo_.getText());
    };

    addAndMakeVisible (spotifyRefreshPlaylistsButton_);
    spotifyRefreshPlaylistsButton_.setButtonText (lm.getText ("ribbon.bg.spotify_refresh_playlists"));
    spotifyRefreshPlaylistsButton_.onClick = [this] { if (onSpotifyPlaylistsRefreshRequested) onSpotifyPlaylistsRefreshRequested(); };

    addAndMakeVisible (folderPathLabel_);
    folderPathLabel_.setColour (juce::Label::textColourId, kText.withAlpha (0.85f));
    folderPathLabel_.setFont (juce::Font (juce::FontOptions().withHeight (12.0f)));

    addAndMakeVisible (browseButton_);
    browseButton_.setButtonText (lm.getText ("ribbon.bg.browse"));
    browseButton_.onClick = [this] { browseForFolder(); };

    addAndMakeVisible (useDefaultButton_);
    useDefaultButton_.setButtonText (lm.getText ("ribbon.bg.use_default"));
    useDefaultButton_.onClick = [this] { if (onUseDefaultRequested) onUseDefaultRequested(); };

    listContent_ = std::make_unique<juce::Component>();
    addAndMakeVisible (viewport_);
    viewport_.setViewedComponent (listContent_.get(), false);
    viewport_.setScrollBarsShown (true, false);

    sourceLocalButton_.setToggleState (true, juce::dontSendNotification);
    updateSourceVisibility();
}

BackgroundMusicLibraryPanel::~BackgroundMusicLibraryPanel() = default;

void BackgroundMusicLibraryPanel::setSource (const juce::String& source)
{
    currentSource_ = source == "spotify" ? "spotify" : "local";
    sourceLocalButton_.setToggleState (currentSource_ == "local", juce::dontSendNotification);
    sourceSpotifyButton_.setToggleState (currentSource_ == "spotify", juce::dontSendNotification);
    updateSourceVisibility();
    resized();
}

void BackgroundMusicLibraryPanel::updateSourceVisibility()
{
    const bool local = currentSource_ == "local";

    folderPathLabel_.setVisible (local);
    browseButton_.setVisible (local);
    useDefaultButton_.setVisible (local);
    viewport_.setVisible (local);

    const bool spotify = ! local;
    spotifyClientIdLabel_.setVisible (spotify);
    spotifyClientIdEditor_.setVisible (spotify);
    spotifyConnectButton_.setVisible (spotify && ! spotifyConnected_);
    spotifyDisconnectButton_.setVisible (spotify && spotifyConnected_);
    spotifyStatusLabel_.setVisible (spotify);
    spotifyPlaylistLabel_.setVisible (spotify && spotifyConnected_);
    spotifyPlaylistCombo_.setVisible (spotify && spotifyConnected_);
    spotifyRefreshPlaylistsButton_.setVisible (spotify && spotifyConnected_);
}

void BackgroundMusicLibraryPanel::setSpotifyClientId (const juce::String& clientId)
{
    spotifyClientIdEditor_.setText (clientId, juce::dontSendNotification);
}

void BackgroundMusicLibraryPanel::reportSpotifyPlaybackResult (bool ok, const juce::String& error)
{
    if (ok)
    {
        spotifyStatusLabel_.setColour (juce::Label::textColourId, kText.withAlpha (0.85f));
        spotifyStatusLabel_.setText (LocalizationManager::getInstance().getText ("ribbon.bg.spotify_playing"),
                                    juce::dontSendNotification);
    }
    else
    {
        spotifyStatusLabel_.setColour (juce::Label::textColourId, kDanger);
        spotifyStatusLabel_.setText (error, juce::dontSendNotification);
    }
}

void BackgroundMusicLibraryPanel::setSpotifyState (bool connected, const juce::String& accountName,
                                                   const std::vector<SpotifyService::PlaylistInfo>& playlists,
                                                   const juce::String& selectedUri)
{
    spotifyConnected_ = connected;
    auto& lm = LocalizationManager::getInstance();

    spotifyStatusLabel_.setColour (juce::Label::textColourId, kText.withAlpha (0.85f));
    spotifyStatusLabel_.setText (connected
        ? (lm.getText ("ribbon.bg.spotify_connected_as") + " " + accountName)
        : lm.getText ("ribbon.bg.spotify_not_connected"),
        juce::dontSendNotification);

    spotifyPlaylistUris_.clear();
    spotifyPlaylistCombo_.clear (juce::dontSendNotification);
    if (playlists.empty())
    {
        spotifyPlaylistCombo_.addItem (lm.getText ("ribbon.bg.spotify_no_playlists"), 1);
        spotifyPlaylistCombo_.setItemEnabled (1, false);
        spotifyPlaylistCombo_.setSelectedId (1, juce::dontSendNotification);
    }
    else
    {
        int selectedId = 0;
        for (int i = 0; i < (int) playlists.size(); ++i)
        {
            spotifyPlaylistUris_.push_back (playlists[(size_t) i].uri);
            spotifyPlaylistCombo_.addItem (playlists[(size_t) i].name, i + 1);
            if (playlists[(size_t) i].uri == selectedUri)
                selectedId = i + 1;
        }
        spotifyPlaylistCombo_.setSelectedId (selectedId, juce::dontSendNotification);
    }

    updateSourceVisibility();
    resized();
}

void BackgroundMusicLibraryPanel::setFolderPath (const juce::String& path)
{
    currentFolder_ = juce::File (path);

    folderPathLabel_.setText (
        path.isNotEmpty() ? path : LocalizationManager::getInstance().getText ("ribbon.bg.using_default"),
        juce::dontSendNotification);
}

void BackgroundMusicLibraryPanel::setTracks (const std::vector<juce::File>& tracks, const juce::StringArray& selectedFilenames)
{
    rows_.clear();
    listContent_->removeAllChildren();

    for (auto& file : tracks)
    {
        auto row = std::make_unique<TrackRow> (file, *this);
        row->setChecked (selectedFilenames.isEmpty() || selectedFilenames.contains (file.getFileName()));
        listContent_->addAndMakeVisible (*row);
        rows_.push_back (std::move (row));
    }

    resized();
}

void BackgroundMusicLibraryPanel::handleSelectionChanged()
{
    juce::StringArray selected;
    for (auto& row : rows_)
        if (row->isChecked())
            selected.add (row->getFile().getFileName());

    if (onSelectionChanged)
        onSelectionChanged (selected);
}

void BackgroundMusicLibraryPanel::browseForFolder()
{
    juce::File startDir = currentFolder_.isDirectory()
        ? currentFolder_
        : juce::File::getSpecialLocation (juce::File::userMusicDirectory);

    fileChooser_ = std::make_unique<juce::FileChooser> (
        LocalizationManager::getInstance().getText ("ribbon.bg.choose_folder_title"),
        startDir);

    fileChooser_->launchAsync (
        juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories,
        [this] (const juce::FileChooser& fc)
        {
            auto result = fc.getResult();
            if (! result.isDirectory())
                return;

            if (onFolderChanged)
                onFolderChanged (result);
        });
}

void BackgroundMusicLibraryPanel::resized()
{
    auto area = getLocalBounds();

    auto sourceRow = area.removeFromTop (26);
    sourceLocalButton_.setBounds (sourceRow.removeFromLeft (140));
    sourceRow.removeFromLeft (8);
    sourceSpotifyButton_.setBounds (sourceRow.removeFromLeft (140));
    area.removeFromTop (8);

    if (currentSource_ == "local")
    {
        auto topRow = area.removeFromTop (26);
        useDefaultButton_.setBounds (topRow.removeFromRight (110));
        topRow.removeFromRight (8);
        browseButton_.setBounds (topRow.removeFromRight (100));
        topRow.removeFromRight (8);
        folderPathLabel_.setBounds (topRow);

        area.removeFromTop (8);
        viewport_.setBounds (area);

        const int width = viewport_.getMaximumVisibleWidth();
        for (int i = 0; i < (int) rows_.size(); ++i)
            rows_[(size_t) i]->setBounds (0, i * kRowHeight, width, kRowHeight);

        listContent_->setSize (width, juce::jmax (1, (int) rows_.size()) * kRowHeight);
    }
    else
    {
        auto clientIdRow = area.removeFromTop (26);
        spotifyClientIdLabel_.setBounds (clientIdRow.removeFromLeft (90));
        (spotifyConnected_ ? spotifyDisconnectButton_ : spotifyConnectButton_)
            .setBounds (clientIdRow.removeFromRight (100));
        clientIdRow.removeFromRight (6);
        spotifyClientIdEditor_.setBounds (clientIdRow);
        area.removeFromTop (6);

        spotifyStatusLabel_.setBounds (area.removeFromTop (20));
        area.removeFromTop (6);

        if (spotifyConnected_)
        {
            auto playlistRow = area.removeFromTop (26);
            spotifyPlaylistLabel_.setBounds (playlistRow.removeFromLeft (90));
            spotifyRefreshPlaylistsButton_.setBounds (playlistRow.removeFromRight (120));
            playlistRow.removeFromRight (6);
            spotifyPlaylistCombo_.setBounds (playlistRow);
        }
    }
}
