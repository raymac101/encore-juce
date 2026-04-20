/*
  ==============================================================================

    Lyric.h
    Created: 19 Apr 2026
    Author:  GitHub Copilot

    Lyric data models - migrated from TypeScript
    Contains: Syllable, LyricWord, LyricWordId, LyricLine, Lyric

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include <string>
#include <vector>

//==============================================================================
/** Syllable scoring data */
struct Syllable
{
    int numSyllables = 0;
    double score = 0.0;
    std::string word;
};

//==============================================================================
/** A single word with timing data within a lyric line */
struct LyricWord
{
    double ws = 0.0;        // Word start time
    double we = 0.0;        // Word end time
    std::string w;          // Word text
    int syl = 0;            // Syllable count
};

//==============================================================================
/** Extended word ID for editing / lookup purposes */
struct LyricWordId
{
    int wordId = 0;
    int wordFBId = 0;       // Firebase word ID
    double ws = 0.0;        // Word start time
    double we = 0.0;        // Word end time
    std::string w;          // Word text
    int lineId = 0;
    double ls = 0.0;        // Line start time
    double le = 0.0;        // Line end time
    std::string l;          // Line text
    int lastFBId = 0;
};

//==============================================================================
/** A line of lyrics containing timed words */
struct LyricLine
{
    double ls = 0.0;        // Line start time
    double le = 0.0;        // Line end time
    std::string l;          // Full line text
    std::vector<LyricWord> words;
};

//==============================================================================
/**
    Complete lyric data for a track, with synchronized timing.
    Maps to the Firebase Realtime Database lyrics structure.
*/
struct Lyric
{
    std::string id;                     // Track ID
    std::string artist;
    int year = 0;
    std::string key;
    double tempo = 0.0;
    int restricted = 0;
    std::string lyricsCopyright;
    int length = 0;                     // Duration in seconds
    std::string language;
    std::string languageDescription;
    std::vector<LyricLine> lyrics;

    //==============================================================================
    juce::String toJson() const;
    static Lyric fromJson(const juce::String& json);
    static Lyric fromJsonObject(juce::DynamicObject* obj);
    bool isValid() const;

    /** Find the lyric line active at a given time (seconds) */
    const LyricLine* getLineAtTime(double timeSeconds) const;

    /** Find the word active at a given time (seconds) */
    const LyricWord* getWordAtTime(double timeSeconds) const;
};
