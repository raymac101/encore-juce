/*
  ==============================================================================

    GifDecoder.cpp

  ==============================================================================
*/

#include "GifDecoder.h"
#include <cstring>

namespace
{
    // Reads GIF LZW codes LSB-first from a flat (sub-block markers already
    // stripped) compressed-data buffer.
    class BitReader
    {
    public:
        BitReader (const juce::uint8* data, size_t size) : data_ (data), size_ (size) {}

        // Returns -1 once the buffer is exhausted.
        int readCode (int numBits)
        {
            int result = 0;
            for (int i = 0; i < numBits; ++i)
            {
                if (bytePos_ >= size_)
                    return -1;

                const int bit = (data_[bytePos_] >> bitPos_) & 1;
                result |= (bit << i);

                if (++bitPos_ == 8)
                {
                    bitPos_ = 0;
                    ++bytePos_;
                }
            }
            return result;
        }

    private:
        const juce::uint8* data_;
        size_t size_;
        size_t bytePos_ = 0;
        int bitPos_ = 0;
    };

    // Standard GIF-variant LZW decompression. Palette-index output is left
    // flat (row-major in on-disk/interlace-pass order) -- the caller handles
    // mapping that back into (x, y) with any interlace reordering.
    bool lzwDecode (const std::vector<juce::uint8>& packed, int minCodeSize,
                    std::vector<juce::uint8>& out, size_t expectedPixels)
    {
        const int clearCode   = 1 << minCodeSize;
        const int endCode     = clearCode + 1;
        const int maxDictSize = 4096;

        std::vector<std::vector<juce::uint8>> dict;
        auto resetDict = [&]
        {
            dict.clear();
            dict.resize ((size_t) endCode + 1);
            for (int i = 0; i < clearCode; ++i)
                dict[(size_t) i] = { (juce::uint8) i };
        };
        resetDict();

        int codeSize  = minCodeSize + 1;
        int nextCode  = endCode + 1;
        int prevCode  = -1;

        BitReader reader (packed.data(), packed.size());
        out.clear();
        out.reserve (expectedPixels);

        while (out.size() < expectedPixels)
        {
            const int code = reader.readCode (codeSize);
            if (code < 0)
                break; // ran out of data -- caller pads/truncates gracefully

            if (code == clearCode)
            {
                resetDict();
                codeSize = minCodeSize + 1;
                nextCode = endCode + 1;
                prevCode = -1;
                continue;
            }
            if (code == endCode)
                break;

            std::vector<juce::uint8> entry;
            if (code < (int) dict.size() && ! dict[(size_t) code].empty())
            {
                entry = dict[(size_t) code];
            }
            else if (code == nextCode && prevCode >= 0 && prevCode < (int) dict.size())
            {
                // The classic LZW-GIF special case: a code equal to the
                // next-not-yet-defined entry means "previous entry plus its
                // own first byte".
                entry = dict[(size_t) prevCode];
                entry.push_back (dict[(size_t) prevCode][0]);
            }
            else
            {
                break; // corrupt/truncated stream
            }

            out.insert (out.end(), entry.begin(), entry.end());

            if (prevCode >= 0 && nextCode < maxDictSize)
            {
                std::vector<juce::uint8> newEntry = dict[(size_t) prevCode];
                newEntry.push_back (entry[0]);
                if (nextCode >= (int) dict.size())
                    dict.resize ((size_t) nextCode + 1);
                dict[(size_t) nextCode] = std::move (newEntry);
                ++nextCode;
                if (nextCode == (1 << codeSize) && codeSize < 12)
                    ++codeSize;
            }

            prevCode = code;
        }

        return ! out.empty();
    }
}

//==============================================================================
bool GifDecoder::decode (const juce::File& file, std::vector<Frame>& outFrames)
{
    outFrames.clear();

    juce::MemoryBlock raw;
    if (! file.loadFileAsData (raw) || raw.getSize() < 13)
        return false;

    const auto* data = static_cast<const juce::uint8*> (raw.getData());
    const size_t size = raw.getSize();
    size_t pos = 0;

    auto readU8 = [&]() -> int { return pos < size ? data[pos++] : -1; };
    auto readU16 = [&]() -> int
    {
        if (pos + 2 > size) { pos = size; return -1; }
        const int v = data[pos] | (data[pos + 1] << 8);
        pos += 2;
        return v;
    };
    auto skip = [&] (size_t n) { pos = juce::jmin (size, pos + n); };

    if (std::memcmp (data, "GIF87a", 6) != 0 && std::memcmp (data, "GIF89a", 6) != 0)
        return false;
    pos = 6;

    const int screenW = readU16();
    const int screenH = readU16();
    if (screenW <= 0 || screenH <= 0 || screenW > 8192 || screenH > 8192)
        return false;

    const int packedScreen = readU8();
    readU8(); // background colour index -- unused (see disposal-method-2 comment below)
    readU8(); // pixel aspect ratio -- unused

    const bool globalCtFlag = (packedScreen & 0x80) != 0;
    const int  globalCtSize = 2 << (packedScreen & 0x07);

    // Palettes stored as packed 0xAARRGGBB, matching CDGDecoder::rebuildImage's
    // established convention for writing raw ARGB pixels directly.
    std::vector<juce::uint32> globalPalette;
    if (globalCtFlag)
    {
        globalPalette.resize ((size_t) globalCtSize);
        for (int i = 0; i < globalCtSize; ++i)
        {
            const int r = readU8(), g = readU8(), b = readU8();
            if (r < 0 || g < 0 || b < 0) return false;
            globalPalette[(size_t) i] = 0xff000000u | ((juce::uint32) r << 16)
                                       | ((juce::uint32) g << 8) | (juce::uint32) b;
        }
    }

    juce::Image canvas (juce::Image::ARGB, screenW, screenH, true); // starts fully transparent

    // Pending Graphic Control Extension state, consumed by the NEXT image block.
    int pendingDelayMs           = 100;
    int pendingTransparentIndex  = -1;
    int pendingDisposal          = 0;

    // Needed to apply disposal methods 2 ("restore to background" -- treated
    // as "clear to transparent", correct for a transparent sprite GIF with
    // no meaningful background colour) and 3 ("restore to previous") before
    // drawing the next frame.
    juce::Rectangle<int> lastFrameRect;
    int lastDisposal = 0;
    juce::Image preFrameSnapshot;
    bool firstFrame = true;

    for (;;)
    {
        const int marker = readU8();
        if (marker < 0 || marker == 0x3B) // EOF or trailer
            break;

        if (marker == 0x21) // extension introducer
        {
            const int label = readU8();
            if (label < 0) break;

            if (label == 0xF9) // Graphic Control Extension
            {
                readU8(); // block size, always 4
                const int packed          = readU8();
                const int delayCs         = readU16();
                const int transparentIdx  = readU8();
                readU8(); // block terminator
                if (packed < 0 || delayCs < 0 || transparentIdx < 0)
                    break;

                pendingDisposal = (packed >> 2) & 0x07;
                pendingTransparentIndex = (packed & 0x01) != 0 ? transparentIdx : -1;
                // Delay is in 1/100ths of a second; 0 (some encoders emit
                // this) doesn't mean "as fast as possible" in practice --
                // clamp to a sane minimum so this never flickers unreadably.
                pendingDelayMs = juce::jmax (20, delayCs > 0 ? delayCs * 10 : 100);
            }
            else
            {
                // Comment / Application / Plain Text extensions all share
                // the same generic [size][data...] ... [0] sub-block layout
                // after their label -- skip every sub-block uniformly.
                for (;;)
                {
                    const int blockSize = readU8();
                    if (blockSize <= 0) break;
                    skip ((size_t) blockSize);
                }
            }
            continue;
        }

        if (marker != 0x2C) // not an image descriptor -- unknown/corrupt, stop gracefully
            break;

        const int left   = readU16();
        const int top     = readU16();
        const int width   = readU16();
        const int height  = readU16();
        const int packedImg = readU8();
        if (left < 0 || top < 0 || width <= 0 || height <= 0 || packedImg < 0)
            break;

        const bool localCtFlag = (packedImg & 0x80) != 0;
        const bool interlaced  = (packedImg & 0x40) != 0;
        const int  localCtSize = 2 << (packedImg & 0x07);

        std::vector<juce::uint32> localPalette;
        if (localCtFlag)
        {
            localPalette.resize ((size_t) localCtSize);
            for (int i = 0; i < localCtSize; ++i)
            {
                const int r = readU8(), g = readU8(), b = readU8();
                if (r < 0 || g < 0 || b < 0) { localPalette.clear(); break; }
                localPalette[(size_t) i] = 0xff000000u | ((juce::uint32) r << 16)
                                          | ((juce::uint32) g << 8) | (juce::uint32) b;
            }
        }
        const auto& palette = (localCtFlag && ! localPalette.empty()) ? localPalette : globalPalette;
        if (palette.empty())
            break; // no usable colour table -- corrupt

        const int minCodeSize = readU8();
        if (minCodeSize < 2 || minCodeSize > 11)
            break;

        std::vector<juce::uint8> packedData;
        for (;;)
        {
            const int blockSize = readU8();
            if (blockSize <= 0) break;
            if (pos + (size_t) blockSize > size) { pos = size; break; }
            packedData.insert (packedData.end(), data + pos, data + pos + (size_t) blockSize);
            pos += (size_t) blockSize;
        }

        std::vector<juce::uint8> indices;
        const size_t expectedPixels = (size_t) width * (size_t) height;
        lzwDecode (packedData, minCodeSize, indices, expectedPixels);
        if (indices.empty())
            break;
        if (indices.size() < expectedPixels)
            indices.resize (expectedPixels, 0); // truncated stream -- pad, don't crash

        // Apply the PREVIOUS frame's disposal before drawing this one.
        if (! firstFrame)
        {
            if (lastDisposal == 2)
                canvas.clear (lastFrameRect);
            else if (lastDisposal == 3 && preFrameSnapshot.isValid())
            {
                juce::Graphics cg (canvas);
                cg.drawImageAt (preFrameSnapshot, 0, 0);
            }
            // 0 and 1 ("unspecified" / "do not dispose"): leave the canvas as-is.
        }

        // Snapshot the canvas now, in case THIS frame's own disposal is 3
        // and a later frame needs to restore back to this point.
        preFrameSnapshot = pendingDisposal == 3 ? canvas.createCopy() : juce::Image();

        // Build this frame's sub-image with transparency applied, honouring
        // interlacing -- raw ARGB writes via BitmapData, same convention as
        // CDGDecoder::rebuildImage().
        juce::Image sub (juce::Image::ARGB, width, height, true);
        {
            juce::Image::BitmapData bmp (sub, juce::Image::BitmapData::writeOnly);

            std::vector<int> rowOrder;
            rowOrder.reserve ((size_t) height);
            if (interlaced)
            {
                for (int y = 0; y < height; y += 8) rowOrder.push_back (y);
                for (int y = 4; y < height; y += 8) rowOrder.push_back (y);
                for (int y = 2; y < height; y += 4) rowOrder.push_back (y);
                for (int y = 1; y < height; y += 2) rowOrder.push_back (y);
            }
            else
            {
                for (int y = 0; y < height; ++y) rowOrder.push_back (y);
            }

            for (size_t r = 0; r < rowOrder.size(); ++r)
            {
                const int destY = rowOrder[r];
                auto* dst = (juce::uint32*) bmp.getLinePointer (destY);

                for (int x = 0; x < width; ++x)
                {
                    const size_t srcIdx = r * (size_t) width + (size_t) x;
                    const int paletteIndex = srcIdx < indices.size() ? indices[srcIdx] : 0;

                    juce::uint32 argb = (paletteIndex == pendingTransparentIndex)
                                       ? 0x00000000u  // fully-premultiplied transparent: alpha AND rgb both zero.
                                                      // This image gets composited onto `canvas` below via
                                                      // juce::Graphics (an Image-to-Image draw, unlike
                                                      // CDGDecoder's single direct-to-screen draw), which blends
                                                      // assuming premultiplied source -- leaving full-brightness
                                                      // RGB at alpha=0 shows up as a solid matte of that colour
                                                      // instead of true transparency.
                                       : (paletteIndex >= 0 && paletteIndex < (int) palette.size())
                                           ? palette[(size_t) paletteIndex]
                                           : 0xff000000u;

                    dst[x] = argb;
                }
            }
        }

        {
            juce::Graphics cg (canvas);
            cg.drawImageAt (sub, left, top);
        }

        Frame frame;
        frame.image   = canvas.createCopy();
        frame.delayMs = pendingDelayMs;
        outFrames.push_back (std::move (frame));

        lastFrameRect = { left, top, width, height };
        lastDisposal  = pendingDisposal;
        firstFrame    = false;

        // A frame with no GCE before it (rare, but valid) should get
        // ordinary defaults rather than silently inheriting the previous
        // frame's values.
        pendingDelayMs          = 100;
        pendingTransparentIndex = -1;
        pendingDisposal         = 0;
    }

    return ! outFrames.empty();
}
