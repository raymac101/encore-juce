/*
  ==============================================================================

    Lyric.cpp
    Created: 19 Apr 2026
    Author:  GitHub Copilot

    Lyric data model implementation

  ==============================================================================
*/

#include "Lyric.h"

//==============================================================================
juce::String Lyric::toJson() const
{
    juce::DynamicObject::Ptr obj = new juce::DynamicObject();

    obj->setProperty("id", juce::String(id));
    obj->setProperty("artist", juce::String(artist));
    obj->setProperty("year", year);
    obj->setProperty("key", juce::String(key));
    obj->setProperty("tempo", tempo);
    obj->setProperty("restricted", restricted);
    obj->setProperty("lyricsCopyright", juce::String(lyricsCopyright));
    obj->setProperty("length", length);
    obj->setProperty("language", juce::String(language));
    obj->setProperty("languageDescription", juce::String(languageDescription));

    // Serialize lyrics array
    juce::Array<juce::var> lyricsArr;
    for (auto& line : lyrics)
    {
        juce::DynamicObject::Ptr lineObj = new juce::DynamicObject();
        lineObj->setProperty("ls", line.ls);
        lineObj->setProperty("le", line.le);
        lineObj->setProperty("l", juce::String(line.l));

        juce::Array<juce::var> wordsArr;
        for (auto& word : line.words)
        {
            juce::DynamicObject::Ptr wordObj = new juce::DynamicObject();
            wordObj->setProperty("ws", word.ws);
            wordObj->setProperty("we", word.we);
            wordObj->setProperty("w", juce::String(word.w));
            wordObj->setProperty("syl", word.syl);
            wordsArr.add(juce::var(wordObj.get()));
        }
        lineObj->setProperty("words", juce::var(wordsArr));
        lyricsArr.add(juce::var(lineObj.get()));
    }
    obj->setProperty("lyrics", juce::var(lyricsArr));

    return juce::JSON::toString(juce::var(obj.get()));
}

//==============================================================================
Lyric Lyric::fromJson(const juce::String& json)
{
    juce::var parsed = juce::JSON::parse(json);
    if (parsed.isObject())
        return fromJsonObject(parsed.getDynamicObject());
    return {};
}

Lyric Lyric::fromJsonObject(juce::DynamicObject* obj)
{
    Lyric ly;
    if (obj == nullptr) return ly;

    ly.id                  = obj->getProperty("id").toString().toStdString();
    ly.artist              = obj->getProperty("artist").toString().toStdString();
    ly.year                = (int)obj->getProperty("year");
    ly.key                 = obj->getProperty("key").toString().toStdString();
    ly.tempo               = (double)obj->getProperty("tempo");
    ly.restricted          = (int)obj->getProperty("restricted");
    ly.lyricsCopyright     = obj->getProperty("lyricsCopyright").toString().toStdString();
    ly.length              = (int)obj->getProperty("length");
    ly.language            = obj->getProperty("language").toString().toStdString();
    ly.languageDescription = obj->getProperty("languageDescription").toString().toStdString();

    auto lyricsVar = obj->getProperty("lyrics");
    if (lyricsVar.isArray())
    {
        for (int i = 0; i < lyricsVar.size(); ++i)
        {
            LyricLine line;
            if (auto* lineObj = lyricsVar[i].getDynamicObject())
            {
                line.ls = (double)lineObj->getProperty("ls");
                line.le = (double)lineObj->getProperty("le");
                line.l  = lineObj->getProperty("l").toString().toStdString();

                auto wordsVar = lineObj->getProperty("words");
                if (wordsVar.isArray())
                {
                    for (int j = 0; j < wordsVar.size(); ++j)
                    {
                        LyricWord word;
                        if (auto* wordObj = wordsVar[j].getDynamicObject())
                        {
                            word.ws  = (double)wordObj->getProperty("ws");
                            word.we  = (double)wordObj->getProperty("we");
                            word.w   = wordObj->getProperty("w").toString().toStdString();
                            word.syl = (int)wordObj->getProperty("syl");
                        }
                        line.words.push_back(word);
                    }
                }
            }
            ly.lyrics.push_back(line);
        }
    }

    return ly;
}

//==============================================================================
bool Lyric::isValid() const
{
    return !id.empty() && !lyrics.empty();
}

//==============================================================================
const LyricLine* Lyric::getLineAtTime(double timeSeconds) const
{
    for (auto& line : lyrics)
        if (timeSeconds >= line.ls && timeSeconds <= line.le)
            return &line;
    return nullptr;
}

const LyricWord* Lyric::getWordAtTime(double timeSeconds) const
{
    if (auto* line = getLineAtTime(timeSeconds))
        for (auto& word : line->words)
            if (timeSeconds >= word.ws && timeSeconds <= word.we)
                return &word;
    return nullptr;
}
