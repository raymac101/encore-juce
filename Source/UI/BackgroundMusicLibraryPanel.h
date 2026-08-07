/*
  ==============================================================================

    BackgroundMusicLibraryPanel.h

    Folder picker + track checklist shown below the transport controls in
    the Ribbon's Background Music full-screen view (Source/UI/RibbonMenu.cpp)
    when that panel is expanded. Lets a host point background music at
    their own folder and choose which tracks in it actually play.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include <vector>

class BackgroundMusicLibraryPanel : public juce::Component
{
public:
    BackgroundMusicLibraryPanel();
    ~BackgroundMusicLibraryPanel() override;

    void resized() override;

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

private:
    class TrackRow;

    void browseForFolder();
    void handleSelectionChanged();

    juce::Label folderPathLabel_;
    juce::TextButton browseButton_ { "bgBrowse" };
    juce::TextButton useDefaultButton_ { "bgUseDefault" };

    juce::Viewport viewport_;
    std::unique_ptr<juce::Component> listContent_;
    std::vector<std::unique_ptr<TrackRow>> rows_;

    juce::File currentFolder_;
    std::unique_ptr<juce::FileChooser> fileChooser_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BackgroundMusicLibraryPanel)
};
