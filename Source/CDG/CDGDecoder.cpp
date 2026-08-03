/*
  ==============================================================================

    CDGDecoder.cpp

  ==============================================================================
*/

#include "CDGDecoder.h"

namespace
{
    // CD+G subcode command / instruction constants
    constexpr uint8_t kSubcodeCommand           = 0x09;

    constexpr uint8_t kInstMemoryPreset         = 1;
    constexpr uint8_t kInstBorderPreset         = 2;
    constexpr uint8_t kInstTileBlock            = 6;
    constexpr uint8_t kInstScrollPreset         = 20;
    constexpr uint8_t kInstScrollCopy           = 24;
    constexpr uint8_t kInstDefineTransparent    = 28;
    constexpr uint8_t kInstLoadColorTableLow    = 30;
    constexpr uint8_t kInstLoadColorTableHigh   = 31;
    constexpr uint8_t kInstTileBlockXor         = 38;

    /** Expand a 4-bit CD+G channel value to an 8-bit value (0..255). */
    inline uint8_t expand4Bit (int v) noexcept
    {
        v &= 0x0F;
        return (uint8_t) ((v << 4) | v);
    }

    /** Convert a 2-byte CD+G colour entry to an ARGB pixel.

        The two bytes each carry only 6 meaningful bits:
           byte0 = 00 r3 r2 r1 r0 g3 g2
           byte1 = 00 g1 g0 b3 b2 b1 b0
        which combine into 4-bit R, 4-bit G, 4-bit B. */
    inline juce::uint32 cdgColorToARGB (uint8_t b0, uint8_t b1) noexcept
    {
        const int r = (b0 >> 2) & 0x0F;
        const int g = ((b0 & 0x03) << 2) | ((b1 >> 4) & 0x03);
        const int b = b1 & 0x0F;

        return juce::Colour (expand4Bit (r), expand4Bit (g), expand4Bit (b))
                   .getARGB();
    }
}

//==============================================================================
CDGDecoder::CDGDecoder()
{
    clear();
}

bool CDGDecoder::loadFile (const juce::File& cdgFile)
{
    clear();

    if (! cdgFile.existsAsFile())
        return false;

    juce::MemoryBlock mb;
    if (! cdgFile.loadFileAsData (mb))
        return false;

    const size_t size = mb.getSize();
    if (size < (size_t) kPacketSize)
        return false;

    fileBytes_.resize (size);
    std::memcpy (fileBytes_.data(), mb.getData(), size);
    loaded_ = true;
    return true;
}

void CDGDecoder::clear()
{
    fileBytes_.clear();
    nextPacketIndex_ = 0;
    loaded_          = false;

    // Default palette: index 0 = transparent black, others = white.
    // The CDG stream usually uploads a real palette within its first packets.
    palette_.fill (juce::Colour (0, 0, 0).getARGB());

    pixels_.fill (0);
    transparentIndex_ = -1;
    borderColor_      = 0;
    hOffset_          = 0;
    vOffset_          = 0;
    imageDirty_       = true;

    // Reset the image to fully transparent black so the idle display shows
    // through cleanly until a song begins rendering.
    image_.clear (image_.getBounds(), juce::Colours::transparentBlack);
}

//==============================================================================
void CDGDecoder::renderAt (double positionSeconds)
{
    if (! loaded_)
        return;

    if (positionSeconds < 0.0)
        positionSeconds = 0.0;

    const int totalPackets = (int) (fileBytes_.size() / (size_t) kPacketSize);
    int targetIndex = (int) (positionSeconds * (double) kPacketsPerSecond);
    if (targetIndex > totalPackets) targetIndex = totalPackets;

    if (targetIndex < nextPacketIndex_)
    {
        // Seek backwards — replay the stream from the beginning. CD+G has no
        // random-access keyframes, so this is the only safe option.
        nextPacketIndex_ = 0;
        resetPixelBuffer (0);
        palette_.fill (juce::Colour (0, 0, 0).getARGB());
        transparentIndex_ = -1;
        borderColor_      = 0;
        hOffset_ = vOffset_ = 0;
    }

    processPacketsUpTo (targetIndex);

    if (imageDirty_)
    {
        rebuildImage();
        imageDirty_ = false;
    }
}

void CDGDecoder::processPacketsUpTo (int targetIndex)
{
    const size_t bytesAvailable = fileBytes_.size();
    const uint8_t* base = fileBytes_.data();

    for (int i = nextPacketIndex_; i < targetIndex; ++i)
    {
        const size_t offset = (size_t) i * (size_t) kPacketSize;
        if (offset + (size_t) kPacketSize > bytesAvailable)
            break;

        processPacket (base + offset);
    }

    nextPacketIndex_ = targetIndex;
}

void CDGDecoder::processPacket (const uint8_t* packet)
{
    const uint8_t command = packet[0] & 0x3F;
    if (command != kSubcodeCommand)
        return;   // Non-CDG packet — skip.

    const uint8_t instruction = packet[1] & 0x3F;
    const uint8_t* data       = packet + 4;   // 16-byte payload

    switch (instruction)
    {
        case kInstMemoryPreset:        memoryPreset    (data);              break;
        case kInstBorderPreset:        borderPreset    (data);              break;
        case kInstTileBlock:           tileBlock       (data, false);       break;
        case kInstTileBlockXor:        tileBlock       (data, true);        break;
        case kInstScrollPreset:        scroll          (data, false);       break;
        case kInstScrollCopy:          scroll          (data, true);        break;
        case kInstDefineTransparent:   defineTransparent (data);            break;
        case kInstLoadColorTableLow:   loadColorTable  (data, 0);           break;
        case kInstLoadColorTableHigh:  loadColorTable  (data, 8);           break;
        default: break;
    }
}

//==============================================================================
void CDGDecoder::memoryPreset (const uint8_t* data)
{
    const uint8_t colour = data[0] & 0x0F;
    const uint8_t repeat = data[1] & 0x0F;

    // Repeat > 0 means this is a duplicate of an earlier preset — ignore to
    // avoid wiping tiles that have already been rendered on top.
    if (repeat != 0)
        return;

    resetPixelBuffer (colour);
    borderColor_ = colour;
    imageDirty_  = true;
}

void CDGDecoder::borderPreset (const uint8_t* data)
{
    borderColor_ = data[0] & 0x0F;

    // Border = the outer tile ring (row 0/17, column 0/49).
    auto setPixel = [this] (int x, int y, uint8_t c)
    {
        if ((unsigned) x < (unsigned) kScreenWidth
         && (unsigned) y < (unsigned) kScreenHeight)
            pixels_[(size_t) (y * kScreenWidth + x)] = c;
    };

    // Top & bottom border rows (12 pixels tall each)
    for (int y = 0; y < kTileHeight; ++y)
        for (int x = 0; x < kScreenWidth; ++x)
            setPixel (x, y, borderColor_);

    for (int y = kScreenHeight - kTileHeight; y < kScreenHeight; ++y)
        for (int x = 0; x < kScreenWidth; ++x)
            setPixel (x, y, borderColor_);

    // Left & right border columns (6 pixels wide each)
    for (int y = 0; y < kScreenHeight; ++y)
    {
        for (int x = 0; x < kTileWidth; ++x)
            setPixel (x, y, borderColor_);
        for (int x = kScreenWidth - kTileWidth; x < kScreenWidth; ++x)
            setPixel (x, y, borderColor_);
    }

    imageDirty_ = true;
}

void CDGDecoder::tileBlock (const uint8_t* data, bool xorMode)
{
    const uint8_t colour0 = data[0] & 0x0F;
    const uint8_t colour1 = data[1] & 0x0F;
    const int     row     = data[2] & 0x1F;   // 0..17
    const int     column  = data[3] & 0x3F;   // 0..49

    if (row >= kTileRows || column >= kTileCols)
        return;

    const int px = column * kTileWidth;
    const int py = row    * kTileHeight;

    for (int yy = 0; yy < kTileHeight; ++yy)
    {
        const uint8_t rowBits = data[4 + yy] & 0x3F;
        for (int xx = 0; xx < kTileWidth; ++xx)
        {
            // Bit 5 = leftmost pixel.
            const bool bit = (rowBits & (1 << (5 - xx))) != 0;
            const uint8_t newColour = bit ? colour1 : colour0;

            const size_t idx = (size_t) ((py + yy) * kScreenWidth + (px + xx));
            if (xorMode)
                pixels_[idx] ^= newColour;
            else
                pixels_[idx]  = newColour;
        }
    }

    imageDirty_ = true;
}

void CDGDecoder::scroll (const uint8_t* data, bool copy)
{
    const uint8_t fillColour = data[0] & 0x0F;

    const uint8_t hScroll = data[1] & 0x3F;
    const uint8_t vScroll = data[2] & 0x3F;

    const int hCmd    = (hScroll >> 4) & 0x03; // 0 none, 1 +6, 2 -6
    const int hOffset = hScroll & 0x07;
    const int vCmd    = (vScroll >> 4) & 0x03; // 0 none, 1 +12, 2 -12
    const int vOffset = vScroll & 0x0F;

    hOffset_ = juce::jlimit (0, kTileWidth  - 1, hOffset);
    vOffset_ = juce::jlimit (0, kTileHeight - 1, vOffset);

    int dx = 0, dy = 0;
    if (hCmd == 1) dx =  kTileWidth;
    if (hCmd == 2) dx = -kTileWidth;
    if (vCmd == 1) dy =  kTileHeight;
    if (vCmd == 2) dy = -kTileHeight;

    if (dx == 0 && dy == 0)
        return;   // Offset-only update — nothing to shift.

    std::array<uint8_t, (size_t) kScreenWidth * (size_t) kScreenHeight> src = pixels_;

    for (int y = 0; y < kScreenHeight; ++y)
    {
        for (int x = 0; x < kScreenWidth; ++x)
        {
            int sx = x - dx;
            int sy = y - dy;

            bool wrapped = false;
            if (copy)
            {
                if (sx < 0) { sx += kScreenWidth;  wrapped = true; }
                if (sx >= kScreenWidth) { sx -= kScreenWidth; wrapped = true; }
                if (sy < 0) { sy += kScreenHeight; wrapped = true; }
                if (sy >= kScreenHeight) { sy -= kScreenHeight; wrapped = true; }
            }

            if (sx < 0 || sx >= kScreenWidth || sy < 0 || sy >= kScreenHeight)
            {
                pixels_[(size_t) (y * kScreenWidth + x)] = fillColour;
            }
            else
            {
                juce::ignoreUnused (wrapped);
                pixels_[(size_t) (y * kScreenWidth + x)]
                    = src[(size_t) (sy * kScreenWidth + sx)];
            }
        }
    }

    imageDirty_ = true;
}

void CDGDecoder::loadColorTable (const uint8_t* data, int tableOffset)
{
    for (int i = 0; i < 8; ++i)
    {
        const uint8_t b0 = data[i * 2];
        const uint8_t b1 = data[i * 2 + 1];
        palette_[(size_t) (tableOffset + i)] = cdgColorToARGB (b0, b1);
    }

    imageDirty_ = true;
}

void CDGDecoder::defineTransparent (const uint8_t* data)
{
    transparentIndex_ = data[0] & 0x0F;
}

//==============================================================================
void CDGDecoder::resetPixelBuffer (uint8_t colorIndex)
{
    pixels_.fill (colorIndex & 0x0F);
}

void CDGDecoder::rebuildImage()
{
    juce::Image::BitmapData bmp (image_, juce::Image::BitmapData::writeOnly);

    for (int y = 0; y < kScreenHeight; ++y)
    {
        auto* dst = (juce::uint32*) bmp.getLinePointer (y);
        const uint8_t* src = &pixels_[(size_t) y * (size_t) kScreenWidth];

        for (int x = 0; x < kScreenWidth; ++x)
        {
            const uint8_t idx = src[x] & 0x0F;
            juce::uint32 argb = palette_[idx];

            // Treat the stream's background / transparent indices as actually
            // transparent, so the host window's theme colour shows through
            // instead of a solid black rectangle.
            const bool isTransparent = (transparentIndex_ >= 0 && idx == (uint8_t) transparentIndex_)
                                    || idx == borderColor_;
            if (isTransparent)
                argb &= 0x00FFFFFFu;

            dst[x] = argb;
        }
    }
}
