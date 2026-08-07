/*
  ==============================================================================

    SfxLibraryListPanel.h

    Scrollable, filterable library of every sound in SfxLibraryService,
    shown below the Ribbon's 8 fixed Sound Effects slots when that panel is
    expanded to full screen (Source/UI/RibbonMenu.cpp). Each sound renders
    as a small tile: click previews it, drag starts a
    juce::DragAndDropContainer drag (description = the sound's plain name)
    that RibbonMenu accepts as a DragAndDropTarget to assign a slot.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "../Services/SfxLibraryService.h"

class SfxLibraryListPanel : public juce::Component
{
public:
    SfxLibraryListPanel();
    ~SfxLibraryListPanel() override;

    void resized() override;

    /** Fired when a tile is clicked (a plain click, not a drag) -- wire
        this to the same one-shot player used for the 8 slots themselves,
        so browsing the library can be done by ear. */
    std::function<void (juce::String soundName)> onPreviewRequested;

private:
    class SoundTile;
    class ListContent;

    void applyFilter();

    juce::TextEditor filterEditor_;
    juce::Viewport viewport_;
    std::unique_ptr<ListContent> content_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SfxLibraryListPanel)
};
