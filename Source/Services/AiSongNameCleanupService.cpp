/*
  ==============================================================================

    AiSongNameCleanupService.cpp

  ==============================================================================
*/

#include "AiSongNameCleanupService.h"
#include "UserPreferences.h"

namespace
{
    constexpr const char* kAnthropicUrl = "https://api.anthropic.com/v1/messages";
    constexpr const char* kModel = "claude-haiku-4-5-20251001";
    constexpr int kConnectionTimeoutMs = 15000;

    const char* const kSystemPrompt =
        "You clean up karaoke song/artist text before it's used as a Spotify search "
        "query. You will be given an ARTIST and a SONG TITLE extracted from a local "
        "karaoke filename; they may contain misspellings, swapped fields, stray "
        "symbols left over from filename parsing, or informal spelling. Return ONLY "
        "a single JSON object, no other text, no markdown fences, in exactly this "
        "shape: {\"artistName\":\"...\",\"songName\":\"...\",\"changed\":true|false}. "
        "Rules: (1) Fix obvious misspellings of well-known artist/song names. "
        "(2) If the two fields look swapped -- the \"artist\" is actually a song "
        "title, or vice versa -- swap them. (3) Remove stray symbols/punctuation left "
        "over from filename parsing (extra #, unmatched parentheses, doubled spaces) "
        "without altering the real words. (4) Normalise informal contractions to the "
        "spelling Spotify most likely uses for that SPECIFIC song (e.g. a title using "
        "\"Believing\" that you recognise as actually being titled \"Believin'\") -- "
        "only when confident about that exact song, never guess for an unfamiliar one. "
        "(5) Never invent a different song or artist -- only clean up the one given. "
        "(6) If nothing needs changing, return the original values unchanged with "
        "changed=false.";

    juce::String extractJsonText (const juce::var& parsed)
    {
        auto content = parsed.getProperty ("content", juce::var());
        if (auto* arr = content.getArray())
        {
            for (auto& block : *arr)
            {
                if (block.getProperty ("type", juce::String()).toString() == "text")
                    return block.getProperty ("text", juce::String()).toString();
            }
        }
        return {};
    }
}

//==============================================================================
AiSongNameCleanupService& AiSongNameCleanupService::getInstance()
{
    static AiSongNameCleanupService instance;
    return instance;
}

void AiSongNameCleanupService::cleanup (const juce::String& artistName,
                                        const juce::String& songName,
                                        Callback onDone)
{
    const auto apiKey = UserPreferences::getInstance().getAnthropicApiKey().trim();
    if (apiKey.isEmpty())
    {
        if (onDone)
        {
            Result r;
            r.errorMessage = "No Anthropic API key configured.";
            juce::MessageManager::callAsync ([onDone, r] { onDone (r); });
        }
        return;
    }

    juce::Thread::launch ([apiKey, artistName, songName, onDone]()
    {
        Result r;
        r.artistName = artistName;
        r.songName = songName;

        juce::DynamicObject::Ptr userMsg = new juce::DynamicObject();
        userMsg->setProperty ("role", "user");
        userMsg->setProperty ("content", "ARTIST: " + artistName + "\nSONG: " + songName);

        juce::Array<juce::var> messages { juce::var (userMsg.get()) };

        juce::DynamicObject::Ptr body = new juce::DynamicObject();
        body->setProperty ("model", kModel);
        body->setProperty ("max_tokens", 300);
        body->setProperty ("system", juce::String (kSystemPrompt));
        body->setProperty ("messages", juce::var (messages));

        const auto bodyText = juce::JSON::toString (juce::var (body.get()), false);

        juce::URL url (kAnthropicUrl);
        url = url.withPOSTData (bodyText);

        int statusCode = 0;
        const auto headers = juce::String ("x-api-key: ") + apiKey + "\r\n"
                            + "anthropic-version: 2023-06-01\r\n"
                            + "Content-Type: application/json";

        auto opts = juce::URL::InputStreamOptions (juce::URL::ParameterHandling::inPostData)
            .withConnectionTimeoutMs (kConnectionTimeoutMs)
            .withExtraHeaders (headers)
            .withHttpRequestCmd ("POST")
            .withStatusCode (&statusCode);

        auto stream = std::unique_ptr<juce::InputStream> (url.createInputStream (opts));
        if (stream == nullptr)
        {
            r.errorMessage = "Could not connect to Anthropic API.";
            if (onDone) juce::MessageManager::callAsync ([onDone, r] { onDone (r); });
            return;
        }

        const auto responseText = stream->readEntireStreamAsString();

        if (statusCode < 200 || statusCode >= 300)
        {
            r.errorMessage = "Anthropic API HTTP " + juce::String (statusCode)
                            + (responseText.isNotEmpty() ? (" -- " + responseText.substring (0, 200)) : juce::String());
            if (onDone) juce::MessageManager::callAsync ([onDone, r] { onDone (r); });
            return;
        }

        auto parsed = juce::JSON::parse (responseText);
        if (! parsed.isObject())
        {
            r.errorMessage = "Malformed Anthropic API response.";
            if (onDone) juce::MessageManager::callAsync ([onDone, r] { onDone (r); });
            return;
        }

        const auto jsonText = extractJsonText (parsed).trim();
        auto cleaned = juce::JSON::parse (jsonText);
        if (! cleaned.isObject())
        {
            r.errorMessage = "Could not parse cleanup result.";
            if (onDone) juce::MessageManager::callAsync ([onDone, r] { onDone (r); });
            return;
        }

        const auto newArtist = cleaned.getProperty ("artistName", juce::String()).toString().trim();
        const auto newSong   = cleaned.getProperty ("songName", juce::String()).toString().trim();

        if (newArtist.isEmpty() || newSong.isEmpty())
        {
            r.errorMessage = "Cleanup result missing artistName/songName.";
            if (onDone) juce::MessageManager::callAsync ([onDone, r] { onDone (r); });
            return;
        }

        r.ok = true;
        r.artistName = newArtist;
        r.songName = newSong;
        r.changed = (newArtist != artistName) || (newSong != songName);

        if (onDone) juce::MessageManager::callAsync ([onDone, r] { onDone (r); });
    });
}
