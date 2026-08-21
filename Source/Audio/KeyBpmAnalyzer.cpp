/*
  ==============================================================================

    KeyBpmAnalyzer.cpp

  ==============================================================================
*/

#include "KeyBpmAnalyzer.h"
#include <cmath>
#include <array>

namespace
{
    constexpr double kAnalysisSampleRate = 22050.0;
    constexpr double kLeadInSeconds      = 5.0;   // skip likely silence/fade-in
    constexpr double kAnalysisSeconds    = 90.0;  // enough for a stable estimate
    constexpr int    kFftOrder           = 11;    // 2048-point FFT
    constexpr int    kFftSize            = 1 << kFftOrder;
    constexpr int    kHopSize            = kFftSize / 4; // 75% overlap

    constexpr double kMinBpm = 50.0;
    constexpr double kMaxBpm = 200.0;

    // Same letter spellings ApiService::getKeySignature uses, so a
    // locally-detected key reads identically to a Spotify-sourced one.
    const char* const kPitchClassNames[12] =
    {
        "C", "C#", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B"
    };

    // Krumhansl-Kessler key profiles (tonic-relative weights, index 0 = tonic).
    const std::array<double, 12> kMajorProfile = {
        6.35, 2.23, 3.48, 2.33, 4.38, 4.09, 2.52, 5.19, 2.39, 3.66, 2.29, 2.88
    };
    const std::array<double, 12> kMinorProfile = {
        6.33, 2.68, 3.52, 5.38, 2.60, 3.53, 2.54, 4.75, 3.98, 2.69, 3.34, 3.17
    };

    // Decodes up to kAnalysisSeconds of audioFile (after skipping kLeadInSeconds),
    // downmixed to mono and resampled to kAnalysisSampleRate. Returns an empty
    // vector if the file can't be read.
    std::vector<float> decodeMonoAnalysisWindow (const juce::File& audioFile)
    {
        juce::AudioFormatManager formatManager;
        formatManager.registerBasicFormats();

        std::unique_ptr<juce::AudioFormatReader> reader (formatManager.createReaderFor (audioFile));
        if (reader == nullptr || reader->sampleRate <= 0.0 || reader->lengthInSamples <= 0)
            return {};

        const auto totalSeconds = (double) reader->lengthInSamples / reader->sampleRate;
        const double startSeconds = juce::jmin (kLeadInSeconds, juce::jmax (0.0, totalSeconds - 1.0));
        const double wantSeconds = juce::jmin (kAnalysisSeconds, totalSeconds - startSeconds);
        if (wantSeconds <= 1.0)
            return {};

        const juce::int64 startSample = (juce::int64) (startSeconds * reader->sampleRate);
        const int numSamples = (int) (wantSeconds * reader->sampleRate);

        const int numChannels = juce::jlimit (1, 2, (int) reader->numChannels);
        juce::AudioBuffer<float> source (numChannels, numSamples);
        if (! reader->read (&source, 0, numSamples, startSample, true, numChannels > 1))
            return {};

        // Downmix to mono.
        std::vector<float> mono ((size_t) numSamples);
        if (numChannels == 1)
        {
            auto* src = source.getReadPointer (0);
            for (int i = 0; i < numSamples; ++i)
                mono[(size_t) i] = src[i];
        }
        else
        {
            auto* l = source.getReadPointer (0);
            auto* r = source.getReadPointer (1);
            for (int i = 0; i < numSamples; ++i)
                mono[(size_t) i] = 0.5f * (l[i] + r[i]);
        }

        // Resample to the fixed analysis rate.
        if (std::abs (reader->sampleRate - kAnalysisSampleRate) < 1.0)
            return mono;

        const double ratio = reader->sampleRate / kAnalysisSampleRate;
        const int outLength = juce::jmax (1, (int) std::ceil (numSamples / ratio));
        std::vector<float> resampled ((size_t) outLength, 0.0f);

        juce::LagrangeInterpolator interpolator;
        interpolator.reset();
        interpolator.process (ratio, mono.data(), resampled.data(), outLength);

        return resampled;
    }

    // Maps an FFT bin's centre frequency to a 0-11 pitch class (0 = C),
    // using standard equal-temperament tuning referenced to A440.
    int frequencyToPitchClass (double frequencyHz)
    {
        const double midiNote = 69.0 + 12.0 * std::log2 (frequencyHz / 440.0);
        int pc = ((int) std::llround (midiNote)) % 12;
        if (pc < 0) pc += 12;
        return pc;
    }

    double correlate (const std::array<double, 12>& a, const std::array<double, 12>& b)
    {
        double meanA = 0.0, meanB = 0.0;
        for (int i = 0; i < 12; ++i) { meanA += a[(size_t) i]; meanB += b[(size_t) i]; }
        meanA /= 12.0; meanB /= 12.0;

        double num = 0.0, denA = 0.0, denB = 0.0;
        for (int i = 0; i < 12; ++i)
        {
            const double da = a[(size_t) i] - meanA;
            const double db = b[(size_t) i] - meanB;
            num += da * db;
            denA += da * da;
            denB += db * db;
        }

        const double den = std::sqrt (denA * denB);
        return den > 1.0e-9 ? (num / den) : 0.0;
    }
}

//==============================================================================
juce::File KeyBpmAnalyzer::resolvePlayableAudioFile (const CdgSong& song, int versionIndex,
                                                     juce::File& outTempFileToDelete)
{
    outTempFileToDelete = juce::File();

    auto buildVersionPath = [&song] (int index) -> juce::String
    {
        if (index >= 0 && index < (int) song.fullPath.size())
        {
            const auto path = juce::String (song.fullPath[(size_t) index]).trim();
            if (path.isNotEmpty())
                return path;
        }
        if (index >= 0 && index < (int) song.filePath.size() && index < (int) song.fileName.size())
        {
            const auto dir = juce::String (song.filePath[(size_t) index]).trim();
            const auto name = juce::String (song.fileName[(size_t) index]).trim();
            if (dir.isNotEmpty() && name.isNotEmpty())
                return juce::File (dir).getChildFile (name).getFullPathName();
        }
        return {};
    };

    juce::String path = buildVersionPath (versionIndex);
    if (path.isEmpty())
        path = buildVersionPath (0);
    if (path.isEmpty())
        return {};

    const juce::File sourceFile (path);
    const auto ext = sourceFile.getFileExtension().toLowerCase();

    if (ext == ".cdg" || ext == ".xml")
    {
        static const juce::StringArray sidecarExts { ".mp3", ".wav", ".ogg", ".flac", ".aac", ".m4a" };
        for (const auto& sidecarExt : sidecarExts)
        {
            auto sibling = sourceFile.withFileExtension (sidecarExt);
            if (sibling.existsAsFile())
                return sibling;
        }
        return {};
    }

    if (ext == ".zip")
    {
        if (! sourceFile.existsAsFile())
            return {};

        juce::ZipFile zip (sourceFile);
        static const juce::StringArray audioExts { ".mp3", ".wav", ".ogg", ".flac", ".aac", ".m4a" };

        int bestIndex = -1, bestRank = std::numeric_limits<int>::max();
        for (int i = 0; i < zip.getNumEntries(); ++i)
        {
            auto* entry = zip.getEntry (i);
            if (entry == nullptr) continue;
            const auto entryExt = juce::File (entry->filename).getFileExtension().toLowerCase();
            const int rank = audioExts.indexOf (entryExt);
            if (rank >= 0 && rank < bestRank) { bestIndex = i; bestRank = rank; }
        }

        if (bestIndex < 0)
            return {};

        auto tempDir = juce::File::getSpecialLocation (juce::File::tempDirectory)
                           .getChildFile ("EncoreKeyBpmAnalysis");
        tempDir.createDirectory();
        auto tempFile = tempDir.getChildFile ("analysis_" + juce::String (juce::Time::currentTimeMillis())
                                              + juce::File (zip.getEntry (bestIndex)->filename).getFileExtension());

        std::unique_ptr<juce::InputStream> entryStream (zip.createStreamForEntry (bestIndex));
        if (entryStream == nullptr)
            return {};

        std::unique_ptr<juce::FileOutputStream> outStream (tempFile.createOutputStream());
        if (outStream == nullptr)
            return {};

        outStream->writeFromInputStream (*entryStream, -1);
        outStream.reset();

        outTempFileToDelete = tempFile;
        return tempFile;
    }

    // Already directly playable (.mp4, .m4a, .mp3, etc).
    return sourceFile;
}

//==============================================================================
KeyBpmAnalyzer::Result KeyBpmAnalyzer::analyze (const juce::File& audioFile)
{
    Result result;

    if (! audioFile.existsAsFile())
    {
        result.errorMessage = "Audio file not found.";
        return result;
    }

    auto mono = decodeMonoAnalysisWindow (audioFile);
    if (mono.size() < (size_t) kFftSize * 2)
    {
        result.errorMessage = "Could not decode enough audio to analyse.";
        return result;
    }

    juce::dsp::FFT fft (kFftOrder);
    juce::HeapBlock<float> fftBuffer (2 * (size_t) kFftSize);

    const int numFrames = ((int) mono.size() - kFftSize) / kHopSize;
    if (numFrames < 4)
    {
        result.errorMessage = "Audio window too short to analyse.";
        return result;
    }

    std::array<double, 12> chroma {};
    chroma.fill (0.0);

    std::vector<float> previousMagnitude ((size_t) kFftSize / 2 + 1, 0.0f);
    std::vector<double> onsetStrength ((size_t) numFrames, 0.0);

    juce::dsp::WindowingFunction<float> window ((size_t) kFftSize, juce::dsp::WindowingFunction<float>::hann);

    for (int frame = 0; frame < numFrames; ++frame)
    {
        const size_t start = (size_t) frame * (size_t) kHopSize;

        std::memset (fftBuffer.getData(), 0, 2 * (size_t) kFftSize * sizeof (float));
        std::memcpy (fftBuffer.getData(), mono.data() + start, (size_t) kFftSize * sizeof (float));
        window.multiplyWithWindowingTable (fftBuffer.getData(), (size_t) kFftSize);

        fft.performFrequencyOnlyForwardTransform (fftBuffer.getData());

        const int numBins = kFftSize / 2 + 1;
        double flux = 0.0;

        for (int bin = 1; bin < numBins; ++bin)
        {
            const float mag = fftBuffer[bin];
            const double freq = (double) bin * kAnalysisSampleRate / (double) kFftSize;

            // Onset strength: half-wave rectified spectral flux.
            const float diff = mag - previousMagnitude[(size_t) bin];
            if (diff > 0.0f)
                flux += diff;

            // Chroma: fold musically-relevant bins into their pitch class,
            // weighted by magnitude.
            if (freq >= 80.0 && freq <= 5000.0)
                chroma[(size_t) frequencyToPitchClass (freq)] += mag;

            previousMagnitude[(size_t) bin] = mag;
        }

        onsetStrength[(size_t) frame] = flux;
    }

    //--- Tempo: autocorrelate the onset-strength envelope ---------------------
    const double frameRate = kAnalysisSampleRate / (double) kHopSize; // frames/sec
    const int minLag = juce::jmax (1, (int) std::round (60.0 * frameRate / kMaxBpm));
    const int maxLag = juce::jmin (numFrames - 1, (int) std::round (60.0 * frameRate / kMinBpm));

    int bestLag = -1;
    double bestScore = -1.0;
    for (int lag = minLag; lag <= maxLag; ++lag)
    {
        double sum = 0.0;
        const int count = numFrames - lag;
        for (int i = 0; i < count; ++i)
            sum += onsetStrength[(size_t) i] * onsetStrength[(size_t) (i + lag)];
        const double score = sum / juce::jmax (1, count);

        if (score > bestScore)
        {
            bestScore = score;
            bestLag = lag;
        }
    }

    if (bestLag > 0)
        result.bpm = (int) std::round (60.0 * frameRate / (double) bestLag);

    //--- Key: correlate the averaged chroma vector against KK profiles --------
    double chromaSum = 0.0;
    for (int i = 0; i < 12; ++i) chromaSum += chroma[(size_t) i];
    if (chromaSum > 1.0e-9)
        for (int i = 0; i < 12; ++i) chroma[(size_t) i] /= chromaSum;

    int bestPitchClass = 0;
    bool bestIsMinor = false;
    double bestKeyScore = -2.0;

    for (int tonic = 0; tonic < 12; ++tonic)
    {
        std::array<double, 12> rotatedMajor {}, rotatedMinor {};
        for (int i = 0; i < 12; ++i)
        {
            rotatedMajor[(size_t) ((i + tonic) % 12)] = kMajorProfile[(size_t) i];
            rotatedMinor[(size_t) ((i + tonic) % 12)] = kMinorProfile[(size_t) i];
        }

        const double majorScore = correlate (chroma, rotatedMajor);
        const double minorScore = correlate (chroma, rotatedMinor);

        if (majorScore > bestKeyScore) { bestKeyScore = majorScore; bestPitchClass = tonic; bestIsMinor = false; }
        if (minorScore > bestKeyScore) { bestKeyScore = minorScore; bestPitchClass = tonic; bestIsMinor = true; }
    }

    result.keySignature = juce::String (kPitchClassNames[bestPitchClass]) + (bestIsMinor ? " minor" : "");

    result.ok = result.bpm > 0 && result.keySignature.isNotEmpty();
    if (! result.ok && result.errorMessage.isEmpty())
        result.errorMessage = "Analysis did not converge on a confident result.";

    return result;
}
