/*
  ==============================================================================

    CDGDecoder.h

    Parses CD+G (.cdg) karaoke subcode-graphics files and renders a 300x216
    paletted framebuffer that can be sampled at any playback position in
    seconds. A standard CD+G stream delivers 300 24-byte packets per second
    (4 subcode packets per sector at 75 sectors/sec).

    References:
      - Jim Bumgardner, "CD+G Revealed" (jbum.com/cdg_revealed.html)

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include <array>
#include <vector>
#include <cstdint>

class CDGDecoder
{
public:
    static constexpr int kScreenWidth      = 300;
    static constexpr int kScreenHeight     = 216;
    static constexpr int kTileWidth        = 6;
    static constexpr int kTileHeight       = 12;
    static constexpr int kTileCols         = 50;   // 300 / 6
    static constexpr int kTileRows         = 18;   // 216 / 12
    static constexpr int kPacketsPerSecond = 300;
    static constexpr int kPacketSize       = 24;

    CDGDecoder();

    /** Load a .cdg file into memory. Resets the framebuffer. */
    bool loadFile (const juce::File& cdgFile);

    /** Forget the currently loaded file and return to a blank state. */
    void clear();

    bool isLoaded() const noexcept        { return loaded_; }
    int  getTransparentIndex() const noexcept { return transparentIndex_; }
    int  getHorizontalOffset() const noexcept { return hOffset_; }
    int  getVerticalOffset()   const noexcept { return vOffset_; }

    /** Advance (or rewind+replay) the decoder so the framebuffer reflects
        the CDG state at the given playback position in seconds. Safe to
        call at any rate — the decoder tracks its own progress. */
    void renderAt (double positionSeconds);

    /** The decoded framebuffer. Image is kARGB, 300 x 216. Ownership stays
        with the decoder; copy the Image (cheap — it is reference counted)
        if you need to hold on to it. */
    const juce::Image& getImage() const noexcept { return image_; }

private:
    //==============================================================================
    // Subcode packet processing
    void processPacketsUpTo (int targetIndex);
    void processPacket (const uint8_t* packet);

    // Instruction handlers (operate on the 16-byte data payload)
    void memoryPreset     (const uint8_t* data);
    void borderPreset     (const uint8_t* data);
    void tileBlock        (const uint8_t* data, bool xorMode);
    void scroll           (const uint8_t* data, bool copy);
    void loadColorTable   (const uint8_t* data, int tableOffset);
    void defineTransparent(const uint8_t* data);

    // Image helpers
    void resetPixelBuffer (uint8_t colorIndex);
    void rebuildImage();

    //==============================================================================
    std::vector<uint8_t> fileBytes_;
    int  nextPacketIndex_ = 0;
    bool loaded_          = false;

    // 16-colour palette, 4-bit per channel CDG values expanded to ARGB.
    std::array<juce::uint32, 16> palette_ {};

    // Paletted framebuffer (indices 0–15) of size 300 x 216.
    std::array<uint8_t, (size_t) kScreenWidth * (size_t) kScreenHeight> pixels_ {};

    juce::Image image_ { juce::Image::ARGB, kScreenWidth, kScreenHeight, true };

    int     transparentIndex_ = -1;
    uint8_t borderColor_      = 0;
    int     hOffset_          = 0;   // smooth-scroll horizontal offset (0..5)
    int     vOffset_          = 0;   // smooth-scroll vertical   offset (0..11)

    // True once we've written to the image from the current framebuffer at
    // least once. Used to avoid an expensive rebuild on initial blank state.
    bool imageDirty_ = true;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CDGDecoder)
};
