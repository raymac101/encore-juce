/*
  ==============================================================================

    IntroVoiceService.cpp

  ==============================================================================
*/

#include "IntroVoiceService.h"
#include <algorithm>

namespace
{
    // ElevenLabs' core synthesize/voice-list endpoints -- stable for a long
    // time, but worth a quick check against their current docs if either
    // ever starts returning unexpected errors.
    constexpr const char* kVoicesUrl = "https://api.elevenlabs.io/v1/voices";
    constexpr const char* kTtsUrlPrefix = "https://api.elevenlabs.io/v1/text-to-speech/";
    constexpr const char* kModelId = "eleven_multilingual_v2";
    constexpr int kConnectionTimeoutMs = 20000;

    // How the voice sits over the music bed: a fixed ducking envelope, not
    // true dynamic sidechain compression -- simple and predictable.
    constexpr float kMusicFullGain = 1.0f;
    constexpr float kMusicDuckedGain = 0.32f;
    constexpr float kVoiceGain = 1.0f;
    constexpr double kVoicePickupSeconds = 1.5; // music plays alone before the voice starts
    constexpr double kOutroTailSeconds = 6.0;   // music plays alone this long after the voice ends, then gets cut
    constexpr double kOutroFadeSeconds = 1.5;   // fade-out length when the track is actually cut short

    // Decodes `file` in full, resampled (simple linear interpolation -- this
    // is a one-off offline render, not real-time playback, so a heavier
    // resampler isn't worth the complexity) to `targetSampleRate` and
    // exactly `targetChannels` channels. Returns an empty buffer if the
    // file can't be read.
    juce::AudioBuffer<float> readWholeFileResampled (juce::AudioFormatManager& formatManager,
                                                      const juce::File& file,
                                                      double targetSampleRate,
                                                      int targetChannels)
    {
        std::unique_ptr<juce::AudioFormatReader> reader (formatManager.createReaderFor (file));
        if (reader == nullptr || reader->lengthInSamples <= 0)
            return {};

        const int sourceChannels = juce::jlimit (1, 2, (int) reader->numChannels);
        juce::AudioBuffer<float> source ((int) sourceChannels, (int) reader->lengthInSamples);
        if (! reader->read (&source, 0, (int) reader->lengthInSamples, 0, true, sourceChannels > 1))
            return {};

        const double ratio = reader->sampleRate / targetSampleRate;
        const int outLength = (int) std::ceil (source.getNumSamples() / ratio);

        juce::AudioBuffer<float> out (targetChannels, juce::jmax (1, outLength));
        out.clear();

        for (int ch = 0; ch < targetChannels; ++ch)
        {
            const int srcCh = juce::jmin (ch, sourceChannels - 1);
            juce::LagrangeInterpolator interpolator;
            interpolator.reset();
            interpolator.process (ratio, source.getReadPointer (srcCh), out.getWritePointer (ch), out.getNumSamples());
        }

        return out;
    }

    bool writeBufferToWav (const juce::AudioBuffer<float>& buffer, double sampleRate, const juce::File& destFile)
    {
        destFile.deleteFile();
        std::unique_ptr<juce::FileOutputStream> stream (destFile.createOutputStream());
        if (stream == nullptr)
            return false;

        juce::WavAudioFormat wavFormat;
        std::unique_ptr<juce::AudioFormatWriter> writer (
            wavFormat.createWriterFor (stream.get(), sampleRate, (unsigned int) buffer.getNumChannels(), 16, {}, 0));
        if (writer == nullptr)
            return false;

        stream.release(); // writer now owns it
        return writer->writeFromAudioSampleBuffer (buffer, 0, buffer.getNumSamples());
    }
}

//==============================================================================
IntroVoiceService& IntroVoiceService::getInstance()
{
    static IntroVoiceService instance;
    return instance;
}

juce::File IntroVoiceService::getGeneratedDirectory()
{
    auto dir = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                   .getChildFile ("EncoreKaraoke")
                   .getChildFile ("generated-intro");
    if (! dir.exists())
        dir.createDirectory();
    return dir;
}

juce::File IntroVoiceService::getCachedIntroFile()
{
    auto file = getGeneratedDirectory().getChildFile ("intro-mixed.wav");
    return file.existsAsFile() ? file : juce::File();
}

void IntroVoiceService::fetchAvailableVoices (const juce::String& apiKey,
                                              std::function<void (bool, std::vector<VoiceInfo>, juce::String)> onDone)
{
    juce::Thread::launch ([apiKey, onDone]
    {
        const juce::URL url (kVoicesUrl);
        int status = 0;
        const auto headers = juce::String ("xi-api-key: ") + apiKey;

        auto opts = juce::URL::InputStreamOptions (juce::URL::ParameterHandling::inAddress)
                        .withConnectionTimeoutMs (kConnectionTimeoutMs)
                        .withExtraHeaders (headers)
                        .withHttpRequestCmd ("GET")
                        .withStatusCode (&status);

        auto stream = std::unique_ptr<juce::InputStream> (url.createInputStream (opts));
        if (stream == nullptr || status != 200)
        {
            if (onDone)
                juce::MessageManager::callAsync ([onDone, status]
                {
                    onDone (false, {}, "Could not reach ElevenLabs (HTTP " + juce::String (status) + ")");
                });
            return;
        }

        const auto parsed = juce::JSON::parse (stream->readEntireStreamAsString());
        const auto voicesVar = parsed.getProperty ("voices", {});
        if (! voicesVar.isArray())
        {
            if (onDone)
                juce::MessageManager::callAsync ([onDone]
                {
                    onDone (false, {}, "Unexpected response from ElevenLabs");
                });
            return;
        }

        std::vector<VoiceInfo> voices;
        for (int i = 0; i < voicesVar.size(); ++i)
        {
            const auto v = voicesVar[i];
            VoiceInfo info;
            info.id = v.getProperty ("voice_id", "").toString();
            info.name = v.getProperty ("name", "").toString();
            if (info.id.isNotEmpty())
                voices.push_back (info);
        }

        if (onDone)
            juce::MessageManager::callAsync ([onDone, voices]
            {
                onDone (true, voices, {});
            });
    });
}

void IntroVoiceService::generateAndCache (const juce::String& apiKey,
                                          const juce::String& scriptText,
                                          const juce::String& voiceId,
                                          const juce::File& introMusicFile,
                                          std::function<void (bool, juce::String)> onDone)
{
    juce::Thread::launch ([apiKey, scriptText, voiceId, introMusicFile, onDone]
    {
        auto fail = [&onDone] (const juce::String& error)
        {
            if (onDone)
                juce::MessageManager::callAsync ([onDone, error] { onDone (false, error); });
        };

        if (apiKey.isEmpty() || scriptText.isEmpty() || voiceId.isEmpty())
        {
            fail ("Missing API key, script, or voice.");
            return;
        }
        if (! introMusicFile.existsAsFile())
        {
            fail ("Intro music track not found.");
            return;
        }

        // 1) Synthesize the voice-over.
        juce::DynamicObject::Ptr bodyObj = new juce::DynamicObject();
        bodyObj->setProperty ("text", scriptText);
        bodyObj->setProperty ("model_id", kModelId);
        const auto body = juce::JSON::toString (juce::var (bodyObj.get()));

        const juce::URL url (juce::String (kTtsUrlPrefix) + voiceId);
        int status = 0;
        const auto headers = juce::String ("xi-api-key: ") + apiKey + "\r\nContent-Type: application/json";

        auto opts = juce::URL::InputStreamOptions (juce::URL::ParameterHandling::inAddress)
                        .withConnectionTimeoutMs (kConnectionTimeoutMs)
                        .withExtraHeaders (headers)
                        .withHttpRequestCmd ("POST")
                        .withStatusCode (&status);
        const auto postUrl = url.withPOSTData (body);

        auto stream = std::unique_ptr<juce::InputStream> (postUrl.createInputStream (opts));
        if (stream == nullptr || status != 200)
        {
            fail ("ElevenLabs request failed (HTTP " + juce::String (status) + ")");
            return;
        }

        const auto voiceTempFile = getGeneratedDirectory().getChildFile ("voice-temp.mp3");
        voiceTempFile.deleteFile();
        {
            juce::FileOutputStream out (voiceTempFile);
            if (! out.openedOk())
            {
                fail ("Could not write temporary voice file.");
                return;
            }

            juce::HeapBlock<char> buffer (1 << 16);
            for (;;)
            {
                const auto bytesRead = stream->read (buffer.getData(), 1 << 16);
                if (bytesRead <= 0)
                    break;
                out.write (buffer.getData(), (size_t) bytesRead);
            }
            out.flush();
        }

        if (! voiceTempFile.existsAsFile())
        {
            fail ("Voice synthesis produced no file.");
            return;
        }

        // 2) Decode both files and mix them offline.
        juce::AudioFormatManager formatManager;
        formatManager.registerBasicFormats();

        std::unique_ptr<juce::AudioFormatReader> musicReader (formatManager.createReaderFor (introMusicFile));
        if (musicReader == nullptr)
        {
            fail ("Could not read the chosen intro music file.");
            return;
        }
        const double sampleRate = musicReader->sampleRate;
        constexpr int channels = 2;

        auto musicBuffer = readWholeFileResampled (formatManager, introMusicFile, sampleRate, channels);
        auto voiceBuffer  = readWholeFileResampled (formatManager, voiceTempFile, sampleRate, channels);
        voiceTempFile.deleteFile();

        if (musicBuffer.getNumSamples() == 0 || voiceBuffer.getNumSamples() == 0)
        {
            fail ("Could not decode the voice-over or music track.");
            return;
        }

        const int voiceStartSample = (int) (kVoicePickupSeconds * sampleRate);
        const int voiceEndSample = voiceStartSample + voiceBuffer.getNumSamples();

        // The chosen track is a full song, not a purpose-made sting -- cut
        // it down to end shortly after the voice-over finishes rather than
        // playing the whole thing. If the track happens to be shorter than
        // that, just use what's there (no looping/padding).
        const int desiredLength = voiceEndSample + (int) (kOutroTailSeconds * sampleRate);
        const int totalLength = juce::jmin (desiredLength,
                                            juce::jmax (musicBuffer.getNumSamples(), voiceEndSample));
        const bool musicWasTruncated = totalLength < musicBuffer.getNumSamples();
        const int musicSamplesToCopy = juce::jmin (musicBuffer.getNumSamples(), totalLength);
        const int fadeSamples = musicWasTruncated
            ? juce::jmin (musicSamplesToCopy, (int) (kOutroFadeSeconds * sampleRate))
            : 0;
        const int fadeStartSample = musicSamplesToCopy - fadeSamples;

        juce::AudioBuffer<float> combined (channels, totalLength);
        combined.clear();

        // Music bed: full gain, ducked under the voice's window, faded out
        // over the last kOutroFadeSeconds if it was cut short.
        for (int ch = 0; ch < channels; ++ch)
        {
            auto* dst = combined.getWritePointer (ch);
            const auto* src = musicBuffer.getReadPointer (ch);
            for (int i = 0; i < musicSamplesToCopy; ++i)
            {
                float gain = (i >= voiceStartSample && i < voiceEndSample) ? kMusicDuckedGain : kMusicFullGain;
                if (fadeSamples > 0 && i >= fadeStartSample)
                    gain *= 1.0f - (float) (i - fadeStartSample) / (float) fadeSamples;
                dst[i] = src[i] * gain;
            }
        }

        // Voice-over, additively, starting after the pickup.
        for (int ch = 0; ch < channels; ++ch)
        {
            auto* dst = combined.getWritePointer (ch) + voiceStartSample;
            const auto* src = voiceBuffer.getReadPointer (ch);
            for (int i = 0; i < voiceBuffer.getNumSamples(); ++i)
                dst[i] += src[i] * kVoiceGain;
        }

        // Simple safety clamp -- additive mixing can exceed +/-1.0.
        for (int ch = 0; ch < channels; ++ch)
        {
            auto* d = combined.getWritePointer (ch);
            for (int i = 0; i < totalLength; ++i)
                d[i] = juce::jlimit (-1.0f, 1.0f, d[i]);
        }

        // 3) Write the combined file, overwriting any previous cache.
        const auto destFile = getGeneratedDirectory().getChildFile ("intro-mixed.wav");
        if (! writeBufferToWav (combined, sampleRate, destFile))
        {
            fail ("Could not write the mixed intro file.");
            return;
        }

        if (onDone)
            juce::MessageManager::callAsync ([onDone] { onDone (true, {}); });
    });
}
