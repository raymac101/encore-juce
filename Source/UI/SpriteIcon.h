/*
  ==============================================================================

    SpriteIcon.h

    Shared helper for loading a colour-tintable icon from a <symbol> in
    assets/images/sprite.svg. Extracted out of RibbonMenu.cpp (which had its
    own file-local copy) so other components -- e.g. EditSingerModal's trash
    button -- can use crisp vector icons instead of relying on emoji/font
    glyph rendering, which is inconsistent across platforms and fonts.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>

namespace SpriteIcon
{
    /** Resolves a path relative to the app by trying the current working
        directory, the executable's directory, and a few parent directories --
        covers both an installed build and running straight from the build
        tree. Returns an invalid (non-existent) juce::File if not found. */
    juce::File resolveAssetFile (const juce::String& relativePath);

    /** Same root-search strategy as resolveAssetFile(), but for a directory
        (e.g. "assets/sounds"). Returns an invalid (non-existent) juce::File
        if no root has a directory at that relative path. */
    juce::File resolveAssetDirectory (const juce::String& relativePath);

    /** Loads the <symbol id="symbolId"> from assets/images/sprite.svg,
        tinted to `colour`. Falls back to a small set of hand-coded inline
        SVG paths for a few common transport symbols if the sprite file (or
        that particular symbol) can't be found. Returns nullptr if neither
        source has the symbol. */
    std::unique_ptr<juce::Drawable> create (const juce::String& symbolId, const juce::Colour& colour);

    /** Loads an arbitrary standalone SVG file (as opposed to a <symbol> in
        the shared sprite sheet -- e.g. assets/sound-icons/*.svg) and tints
        it to `colour`, both by walking the SVG tree's fill/stroke
        attributes and by replacing a set of common dark literal colours
        (covers icons that set colour via a class/style block instead of
        plain attributes). Returns nullptr if the file doesn't exist or
        can't be parsed as an SVG. */
    std::unique_ptr<juce::Drawable> createFromSvgFile (const juce::File& svgFile, const juce::Colour& colour);
}
