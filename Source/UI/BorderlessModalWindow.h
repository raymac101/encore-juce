/*
  ==============================================================================

    BorderlessModalWindow.h

    Shared modal host for every custom-painted dialog in this app
    (SongEditDialog, SongSelectionDialog, AdEditDialog, AdPreviewDialog,
    EditSingerModal's popup, BulkMetadataTool, ...). None of these use
    juce::DialogWindow's own title bar -- native or JUCE-painted -- because
    each one already paints its own rounded-rect background, title label,
    and close button; wrapping that in a second title bar just doubles the
    chrome (a real bug reported against BulkMetadataTool: a native-looking
    outer bar stacked above the dialog's own header).

    Originally a private copy inside SongEditDialog.cpp; extracted here so
    every dialog shares the exact same borderless-host behaviour instead of
    each hand-rolling (or skipping) it slightly differently.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>

class BorderlessModalWindow : public juce::DocumentWindow
{
public:
    BorderlessModalWindow (juce::Component* contentToOwn, juce::Colour bg)
        : juce::DocumentWindow ({}, bg, /*requiredButtons*/ 0, /*addToDesktop*/ true)
    {
        setUsingNativeTitleBar (false);
        setTitleBarHeight (0); // hides the chrome strip entirely
        setDropShadowEnabled (true);
        setResizable (false, false);
        setContentOwned (contentToOwn, true);
        centreAroundComponent (nullptr, contentToOwn->getWidth(), contentToOwn->getHeight());
        setVisible (true);
        enterModalState (true,
                         juce::ModalCallbackFunction::create ([this] (int) { delete this; }),
                         /*deleteWhenDismissed*/ false);
    }

    void closeButtonPressed() override
    {
        exitModalState (0);
    }

    /** Convenience: build the window, own contentToOwn, and (if parent is
        non-null) centre around it -- the pattern every launch() below uses. */
    static void launch (juce::Component* parent, juce::Component* contentToOwn, juce::Colour bg)
    {
        auto* w = new BorderlessModalWindow (contentToOwn, bg);
        if (parent != nullptr)
            w->centreAroundComponent (parent, contentToOwn->getWidth(), contentToOwn->getHeight());
    }
};
