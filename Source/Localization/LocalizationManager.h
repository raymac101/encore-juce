/*
  ==============================================================================

    LocalizationManager.h
    Created: 15 Apr 2026 7:04:12pm
    Author:  GitHub Copilot

    Internationalization and localization system for global karaoke markets

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include <map>
#include <string>

//==============================================================================
/**
    Centralized localization manager providing text translation, date/time formatting,
    number formatting, and cultural adaptations for international markets.
*/
class LocalizationManager
{
public:
    //==============================================================================
    LocalizationManager();
    ~LocalizationManager();
    
    //==============================================================================
    // Singleton access (thread-safe implementation)
    static LocalizationManager& getInstance()
    {
        static LocalizationManager* instance = nullptr;
        static juce::SpinLock lock;
        
        if (instance == nullptr)
        {
            juce::SpinLock::ScopedLockType scopedLock(lock);
            if (instance == nullptr)
            {
                instance = new LocalizationManager();
            }
        }
        return *instance;
    }
    
    //==============================================================================
    // Language Management
    
    /** Set the current language using locale code (e.g., "en_US", "es_ES") */
    void setLanguage(const juce::String& languageCode);
    
    /** Get the current language code */
    juce::String getCurrentLanguage() const { return currentLanguage; }
    
    /** Get the current language display name */
    juce::String getCurrentLanguageDisplayName() const;
    
    /** Get display name for any language code */
    juce::String getLanguageDisplayName(const juce::String& languageCode) const;
    
    /** Get list of all supported languages */
    juce::StringArray getAvailableLanguages() const;
    
    /** Auto-detect system language and set it if supported */
    juce::String detectSystemLanguage();
    
    //==============================================================================
    // Text Translation
    
    /** Get translated text for a key */
    juce::String getText(const juce::String& key) const;
    
    /** Get translated text with pluralization support */
    juce::String getText(const juce::String& key, int count) const;
    
    /** Get translated text with parameter substitution {0}, {1}, etc. */
    juce::String getTextWithParams(const juce::String& key, 
                                   const juce::StringArray& params) const;
    
    //==============================================================================
    // Cultural Formatting
    
    /** Format date according to current locale */
    juce::String formatDate(const juce::Time& time, bool includeYear = true) const;
    
    /** Format time according to current locale (12/24 hour) */
    juce::String formatTime(const juce::Time& time, bool includeSeconds = false) const;
    
    /** Format duration in localized format */
    juce::String formatDuration(int seconds) const;
    
    /** Format numbers with proper decimal/thousands separators */
    juce::String formatNumber(double number, int decimals = 2) const;
    
    //==============================================================================
    // Text Direction & Layout
    
    /** Check if current language uses right-to-left text direction */
    bool isRightToLeft() const;
    
    /** Get text justification for current language */
    juce::Justification getDefaultTextJustification() const;
    
    //==============================================================================
    // Development & Testing
    
    /** Reload language files for development/testing */
    void reloadLanguageFiles();
    
    /** Check if a translation key exists */
    bool hasTranslation(const juce::String& key) const;
    
    /** Get debug info about current locale settings */
    juce::String getDebugInfo() const;
    
private:
    //==============================================================================
    juce::String currentLanguage = "en_US";
    std::map<juce::String, juce::String> translations;
    std::map<juce::String, juce::String> supportedLanguages;
    
    // Cultural formatting settings
    juce::String decimalSeparator = ".";
    juce::String thousandsSeparator = ",";
    bool use24HourTime = false;
    bool isRTL = false;
    
    //==============================================================================
    /** Load translation file for specified language */
    void loadLanguageFile(const juce::String& languageCode);
    
    /** Parse translation file content */
    void parseTranslationFile(const juce::File& file);
    
    /** Initialize supported languages list */
    void initializeSupportedLanguages();
    
    /** Update cultural formatting settings for current language */
    void updateCulturalSettings();
    
    /** Substitute parameters in translated text */
    juce::String substituteParameters(const juce::String& text, 
                                    const juce::StringArray& params) const;
    
    /** Load fallback translations for essential functionality */
    void loadFallbackTranslations();
    
    /** Find language file in various possible locations */
    juce::File findLanguageFile(const juce::String& languageCode);
                                    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LocalizationManager)
};

//==============================================================================
// Convenient macros for UI components (renamed to avoid JUCE conflicts)
#define ENCORE_TRANS(key) LocalizationManager::getInstance().getText(key)
#define ENCORE_TRANS_PLURAL(key, count) LocalizationManager::getInstance().getText(key, count)
#define ENCORE_TRANS_PARAMS(key, ...) LocalizationManager::getInstance().getTextWithParams(key, juce::StringArray(__VA_ARGS__))