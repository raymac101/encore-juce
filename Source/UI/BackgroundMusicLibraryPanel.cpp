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
}

BackgroundMusicLibraryPanel::~BackgroundMusicLibraryPanel() = default;

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
