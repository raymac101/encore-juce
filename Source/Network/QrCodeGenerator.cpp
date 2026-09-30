/*
  ==============================================================================

    QrCodeGenerator.cpp

    See QrCodeGenerator.h. Implements just enough of ISO/IEC 18004 to encode a
    short ASCII string (Byte mode) at versions 1-4 / ECC level L, where each
    version still fits in a single Reed-Solomon block (no interleaving needed)
    and has at most one alignment pattern (no per-version alignment-coordinate
    table needed beyond a single extra coordinate). The Reed-Solomon generator
    polynomial is computed at runtime in GF(256) rather than looked up from a
    table, and mask pattern 0 is always used -- mask choice only affects scan
    robustness, never decodability (the format bits tell the reader which mask
    was applied), so skipping the 8-way penalty-scoring search trades a
    theoretical best-mask optimisation for a much smaller, lower-risk
    implementation.

  ==============================================================================
*/

#include "QrCodeGenerator.h"
#include <cstdlib>
#include <vector>

namespace QrCodeGenerator
{

namespace
{
    //==========================================================================
    // GF(256) arithmetic, primitive polynomial x^8+x^4+x^3+x^2+1 (0x11D) -- the
    // one mandated by the QR spec.
    uint8_t gfMultiply (uint8_t a, uint8_t b)
    {
        int result = 0;
        int aa = a;
        for (int i = 0; i < 8; ++i)
        {
            if ((b & 1) != 0)
                result ^= aa;
            b >>= 1;
            const bool carry = (aa & 0x80) != 0;
            aa = (aa << 1) & 0xFF;
            if (carry)
                aa ^= 0x1D; // reduce mod 0x11D (top bit already dropped by & 0xFF)
        }
        return (uint8_t) result;
    }

    // Monic generator polynomial of the given degree, big-endian coefficients
    // (index 0 = leading term, always 1; index [degree] = constant term).
    std::vector<uint8_t> computeGeneratorPolynomial (int degree)
    {
        std::vector<uint8_t> g = { 1 };
        uint8_t root = 1;
        for (int i = 0; i < degree; ++i)
        {
            std::vector<uint8_t> next (g.size() + 1, (uint8_t) 0);
            for (size_t j = 0; j < next.size(); ++j)
            {
                uint8_t term = 0;
                if (j < g.size())
                    term = (uint8_t) (term ^ g[j]);
                if (j >= 1)
                    term = (uint8_t) (term ^ gfMultiply (root, g[j - 1]));
                next[j] = term;
            }
            g = next;
            root = gfMultiply (root, 2);
        }
        return g;
    }

    // Reed-Solomon remainder of (data * x^degree) mod generator -- schoolbook
    // polynomial long division in GF(256). `generator` includes the implicit
    // leading 1, so it has (numEccBytes + 1) entries.
    std::vector<uint8_t> computeRsRemainder (const std::vector<uint8_t>& data,
                                             const std::vector<uint8_t>& generator)
    {
        const auto r = (int) generator.size() - 1;
        std::vector<uint8_t> work (data.size() + (size_t) r, (uint8_t) 0);
        std::copy (data.begin(), data.end(), work.begin());

        for (size_t i = 0; i < data.size(); ++i)
        {
            const uint8_t coef = work[i];
            if (coef != 0)
                for (int j = 0; j <= r; ++j)
                    work[i + (size_t) j] = (uint8_t) (work[i + (size_t) j] ^ gfMultiply (generator[(size_t) j], coef));
        }
        return std::vector<uint8_t> (work.end() - r, work.end());
    }

    //==========================================================================
    struct BitBuffer
    {
        std::vector<uint8_t> bytes;
        int bitCount = 0;

        void appendBit (bool bit)
        {
            const auto byteIndex = (size_t) (bitCount / 8);
            if (byteIndex >= bytes.size())
                bytes.push_back (0);
            if (bit)
                bytes[byteIndex] = (uint8_t) (bytes[byteIndex] | (0x80 >> (bitCount % 8)));
            ++bitCount;
        }

        void writeBits (uint32_t value, int numBits)
        {
            for (int i = numBits - 1; i >= 0; --i)
                appendBit (((value >> i) & 1u) != 0);
        }

        bool getBit (int index) const
        {
            if (index < 0 || index >= bitCount)
                return false;
            const auto byteIndex = (size_t) (index / 8);
            return (bytes[byteIndex] & (0x80 >> (index % 8))) != 0;
        }
    };

    //==========================================================================
    struct VersionSpec
    {
        int size;           // modules per side
        int totalCodewords; // data + ecc
        int eccCodewords;
        int alignmentCoord; // 0 == no alignment pattern (version 1 only)
    };

    constexpr int kMaxVersion = 4;
    const VersionSpec kVersions[kMaxVersion] =
    {
        { 21,  26,  7, 0 },  // Version 1
        { 25,  44, 10, 18 }, // Version 2
        { 29,  70, 15, 22 }, // Version 3
        { 33, 100, 20, 26 }, // Version 4
    };

    int dataCodewordsFor (const VersionSpec& v) { return v.totalCodewords - v.eccCodewords; }

    //==========================================================================
    // Builds the data codeword sequence (already ECC-appended) for one QR
    // symbol encoding `data` in Byte mode at the given version.
    std::vector<uint8_t> buildCodewords (const std::vector<uint8_t>& data, const VersionSpec& version)
    {
        const int dataCodewords = dataCodewordsFor (version);

        BitBuffer bits;
        bits.writeBits (0b0100, 4);                    // Byte mode indicator
        bits.writeBits ((uint32_t) data.size(), 8);     // character count (8 bits for versions 1-9)
        for (auto b : data)
            bits.writeBits (b, 8);

        const int capacityBits = dataCodewords * 8;
        const int terminatorLen = juce::jlimit (0, 4, capacityBits - bits.bitCount);
        bits.writeBits (0, terminatorLen);

        while (bits.bitCount % 8 != 0)
            bits.appendBit (false);

        bool useEcPad = true;
        while ((int) bits.bytes.size() < dataCodewords)
        {
            bits.bytes.push_back (useEcPad ? (uint8_t) 0xEC : (uint8_t) 0x11);
            useEcPad = ! useEcPad;
        }

        auto generator = computeGeneratorPolynomial (version.eccCodewords);
        auto ecc = computeRsRemainder (bits.bytes, generator);

        std::vector<uint8_t> codewords = bits.bytes;
        codewords.insert (codewords.end(), ecc.begin(), ecc.end());
        return codewords;
    }

    //==========================================================================
    // Format info (ECC level + mask) BCH(15,5) encoding -- the two magic
    // constants (generator 0x537, mask 0x5412) are the ones fixed by the QR
    // spec for this exact purpose and are reproduced in every conformant
    // encoder/decoder.
    uint32_t computeFormatBits (int eccLevelBits /* L=0b01 */, int maskBits /* always 0 here */)
    {
        const uint32_t data5 = (uint32_t) ((eccLevelBits << 3) | maskBits);
        uint32_t rem = data5;
        for (int i = 0; i < 10; ++i)
            rem = (rem << 1) ^ ((rem >> 9) * 0x537u);
        return ((data5 << 10) | (rem & 0x3FFu)) ^ 0x5412u;
    }

    //==========================================================================
    struct Matrix
    {
        int size;
        std::vector<uint8_t> dark;       // 1 = dark module
        std::vector<uint8_t> isFunction; // 1 = reserved (finder/timing/alignment/format/dark-module)

        explicit Matrix (int s) : size (s), dark ((size_t) (s * s), 0), isFunction ((size_t) (s * s), 0) {}

        bool get (int r, int c) const { return dark[(size_t) (r * size + c)] != 0; }
        void set (int r, int c, bool value, bool markFunction)
        {
            dark[(size_t) (r * size + c)] = value ? 1 : 0;
            if (markFunction)
                isFunction[(size_t) (r * size + c)] = 1;
        }
        bool functionAt (int r, int c) const { return isFunction[(size_t) (r * size + c)] != 0; }
    };

    void drawFinderPattern (Matrix& m, int topLeftRow, int topLeftCol)
    {
        for (int dr = -1; dr <= 7; ++dr)
        {
            for (int dc = -1; dc <= 7; ++dc)
            {
                const int r = topLeftRow + dr;
                const int c = topLeftCol + dc;
                if (r < 0 || c < 0 || r >= m.size || c >= m.size)
                    continue;

                bool dark = false;
                if (dr >= 0 && dr <= 6 && dc >= 0 && dc <= 6)
                {
                    const bool onOuterRing = (dr == 0 || dr == 6 || dc == 0 || dc == 6);
                    const bool onInnerCore = (dr >= 2 && dr <= 4 && dc >= 2 && dc <= 4);
                    dark = onOuterRing || onInnerCore;
                }
                m.set (r, c, dark, true);
            }
        }
    }

    void drawAlignmentPattern (Matrix& m, int centerRow, int centerCol)
    {
        for (int dr = -2; dr <= 2; ++dr)
        {
            for (int dc = -2; dc <= 2; ++dc)
            {
                const int r = centerRow + dr;
                const int c = centerCol + dc;
                const bool onOuterRing = (std::abs (dr) == 2 || std::abs (dc) == 2);
                const bool isCenter = (dr == 0 && dc == 0);
                m.set (r, c, onOuterRing || isCenter, true);
            }
        }
    }

    void drawTimingPatterns (Matrix& m)
    {
        for (int i = 8; i < m.size - 8; ++i)
        {
            const bool dark = (i % 2) == 0;
            m.set (6, i, dark, true);
            m.set (i, 6, dark, true);
        }
    }

    void reserveFormatInfoAreas (Matrix& m)
    {
        // Marks the format-info cells as function modules with a placeholder
        // value; drawFormatBits() overwrites the actual bits afterwards.
        for (int i = 0; i <= 5; ++i) m.set (i, 8, false, true);
        m.set (7, 8, false, true);
        m.set (8, 8, false, true);
        m.set (8, 7, false, true);
        for (int i = 9; i <= 14; ++i) m.set (8, 14 - i, false, true);

        for (int i = 0; i < 8; ++i) m.set (8, m.size - 1 - i, false, true);
        for (int i = 8; i < 15; ++i) m.set (m.size - 15 + i, 8, false, true);
    }

    void drawFormatBits (Matrix& m, uint32_t formatBits)
    {
        auto bitAt = [formatBits] (int i) { return ((formatBits >> i) & 1u) != 0; };

        for (int i = 0; i <= 5; ++i) m.set (i, 8, bitAt (i), true);
        m.set (7, 8, bitAt (6), true);
        m.set (8, 8, bitAt (7), true);
        m.set (8, 7, bitAt (8), true);
        for (int i = 9; i <= 14; ++i) m.set (8, 14 - i, bitAt (i), true);

        for (int i = 0; i < 8; ++i) m.set (8, m.size - 1 - i, bitAt (i), true);
        for (int i = 8; i < 15; ++i) m.set (m.size - 15 + i, 8, bitAt (i), true);
    }

    void placeDataBits (Matrix& m, const BitBuffer& moduleBits)
    {
        int bitIndex = 0;
        bool upward = true;
        int col = m.size - 1;

        while (col > 0)
        {
            if (col == 6)
                --col;

            for (int count = 0; count < m.size; ++count)
            {
                const int row = upward ? (m.size - 1 - count) : count;
                for (int dc = 0; dc < 2; ++dc)
                {
                    const int actualCol = col - dc;
                    if (! m.functionAt (row, actualCol))
                    {
                        const bool bit = moduleBits.getBit (bitIndex++);
                        // Mask pattern 0: invert when (row+actualCol) % 2 == 0.
                        const bool invert = ((row + actualCol) % 2) == 0;
                        m.set (row, actualCol, bit != invert, false);
                    }
                }
            }
            upward = ! upward;
            col -= 2;
        }
    }

    juce::Image renderMatrixToImage (const Matrix& m, int pixelsPerModule, int quietZoneModules)
    {
        const int quietPx = quietZoneModules * pixelsPerModule;
        const int imgSize = m.size * pixelsPerModule + quietPx * 2;

        juce::Image image (juce::Image::RGB, imgSize, imgSize, true);
        juce::Graphics g (image);
        g.fillAll (juce::Colours::white);
        g.setColour (juce::Colours::black);

        for (int r = 0; r < m.size; ++r)
            for (int c = 0; c < m.size; ++c)
                if (m.get (r, c))
                    g.fillRect (quietPx + c * pixelsPerModule, quietPx + r * pixelsPerModule,
                                pixelsPerModule, pixelsPerModule);

        return image;
    }
}

juce::Image generateQrCodeImage (const juce::String& text, int pixelsPerModule, int quietZoneModules)
{
    const auto textData = text.toRawUTF8();
    std::vector<uint8_t> bytes (textData, textData + text.getNumBytesAsUTF8());

    const VersionSpec* chosen = nullptr;
    for (const auto& v : kVersions)
    {
        const int usableBytes = (dataCodewordsFor (v) * 8 - 12) / 8;
        if ((int) bytes.size() <= usableBytes)
        {
            chosen = &v;
            break;
        }
    }

    if (chosen == nullptr)
        return {};

    const auto codewords = buildCodewords (bytes, *chosen);

    BitBuffer moduleBits;
    for (auto b : codewords)
        moduleBits.writeBits (b, 8);

    Matrix matrix (chosen->size);
    drawFinderPattern (matrix, 0, 0);
    drawFinderPattern (matrix, 0, chosen->size - 7);
    drawFinderPattern (matrix, chosen->size - 7, 0);
    drawTimingPatterns (matrix);

    if (chosen->alignmentCoord != 0)
        drawAlignmentPattern (matrix, chosen->alignmentCoord, chosen->alignmentCoord);

    matrix.set (chosen->size - 8, 8, true, true); // fixed dark module
    reserveFormatInfoAreas (matrix);

    placeDataBits (matrix, moduleBits);

    const uint32_t formatBits = computeFormatBits (0b01 /* ECC level L */, 0 /* mask 0 */);
    drawFormatBits (matrix, formatBits);

    return renderMatrixToImage (matrix, pixelsPerModule, quietZoneModules);
}

}
