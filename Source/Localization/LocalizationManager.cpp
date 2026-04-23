/*
  ==============================================================================

    LocalizationManager.cpp
    Created: 15 Apr 2026 7:04:12pm
    Author:  GitHub Copilot

    Internationalization and localization system implementation

  ==============================================================================
*/

#include "LocalizationManager.h"

//==============================================================================
LocalizationManager::LocalizationManager()
{
    try
    {
        initializeSupportedLanguages();
        
        // Try to detect system language, fallback to English
        juce::String systemLang = detectSystemLanguage();
        if (supportedLanguages.find(systemLang) != supportedLanguages.end())
            setLanguage(systemLang);
        else
            setLanguage("en_US");
            
        DBG("LocalizationManager initialized successfully");
    }
    catch (const std::exception& e)
    {
        DBG("LocalizationManager initialization error: " + juce::String(e.what()));
        // Set safe fallback state
        currentLanguage = "en_US";
        initializeSupportedLanguages();
        loadFallbackTranslations();
    }
    catch (...)
    {
        DBG("LocalizationManager initialization error: unknown exception");
        // Set safe fallback state
        currentLanguage = "en_US";
        initializeSupportedLanguages();
        loadFallbackTranslations();
    }
}

LocalizationManager::~LocalizationManager()
{
    // Simple destructor - no singleton cleanup needed with static instance approach
}

//==============================================================================
void LocalizationManager::initializeSupportedLanguages()
{
    supportedLanguages["en_US"] = "English (United States)";
    supportedLanguages["en_GB"] = "English (United Kingdom)";
    supportedLanguages["es_ES"] = "Español (España)";
    supportedLanguages["es_MX"] = "Español (México)";
    supportedLanguages["fr_FR"] = "Français (France)";
    supportedLanguages["de_DE"] = "Deutsch (Deutschland)";
    supportedLanguages["pt_BR"] = "Português (Brasil)";
    supportedLanguages["ja_JP"] = "日本語 (日本)";
    supportedLanguages["ko_KR"] = "한국어 (대한민국)";
    supportedLanguages["zh_CN"] = "中文 (简体)";
}

//==============================================================================
void LocalizationManager::setLanguage(const juce::String& languageCode)
{
    try
    {
        if (supportedLanguages.find(languageCode) != supportedLanguages.end())
        {
            currentLanguage = languageCode;
            loadLanguageFile(languageCode);
            updateCulturalSettings();
            
            DBG("LocalizationManager: Language set to " + languageCode);
        }
        else
        {
            DBG("LocalizationManager: Unsupported language " + languageCode + ", keeping " + currentLanguage);
        }
    }
    catch (const std::exception& e)
    {
        DBG("LocalizationManager: Error setting language: " + juce::String(e.what()));
        loadFallbackTranslations();
    }
    catch (...)
    {
        DBG("LocalizationManager: Unknown error setting language");
        loadFallbackTranslations();
    }
}

//==============================================================================
juce::String LocalizationManager::getCurrentLanguageDisplayName() const
{
    auto it = supportedLanguages.find(currentLanguage);
    if (it != supportedLanguages.end())
        return it->second;
    else
        return currentLanguage;  // Fallback to language code if not found
}

//==============================================================================
juce::String LocalizationManager::getLanguageDisplayName(const juce::String& languageCode) const
{
    auto it = supportedLanguages.find(languageCode);
    if (it != supportedLanguages.end())
        return it->second;
    else
        return languageCode;  // Fallback to language code if not found
}

//==============================================================================
juce::StringArray LocalizationManager::getAvailableLanguages() const
{
    juce::StringArray languages;
    
    for (const auto& pair : supportedLanguages)
    {
        languages.add(pair.first + " - " + pair.second);
    }
    
    return languages;
}

//==============================================================================
juce::String LocalizationManager::detectSystemLanguage()
{
    juce::String systemLocale = juce::SystemStats::getDisplayLanguage();
    
    // Map common system locales to our supported languages
    if (systemLocale.startsWith("en"))
    {
        if (systemLocale.contains("GB") || systemLocale.contains("UK"))
            return "en_GB";
        else
            return "en_US";  // Default English
    }
    else if (systemLocale.startsWith("es"))
    {
        if (systemLocale.contains("MX") || systemLocale.contains("Mexico"))
            return "es_MX";
        else
            return "es_ES";
    }
    else if (systemLocale.startsWith("fr"))
        return "fr_FR";
    else if (systemLocale.startsWith("de"))
        return "de_DE";
    else if (systemLocale.startsWith("pt"))
        return "pt_BR";
    else if (systemLocale.startsWith("ja"))
        return "ja_JP";
    else if (systemLocale.startsWith("ko"))
        return "ko_KR";
    else if (systemLocale.startsWith("zh"))
        return "zh_CN";
    
    // Default fallback
    return "en_US";
}

//==============================================================================
void LocalizationManager::loadLanguageFile(const juce::String& languageCode)
{
    try
    {
        // Clear existing translations
        translations.clear();
        
        // Always load fallback translations first
        loadFallbackTranslations();
        
        // Try to load language file
        juce::File languageFile = findLanguageFile(languageCode);
        
        if (languageFile.existsAsFile())
        {
            parseTranslationFile(languageFile);
            DBG("LocalizationManager: Loaded " + juce::String(translations.size()) + " translations from " + languageFile.getFullPathName());
        }
        else
        {
            DBG("LocalizationManager: Could not find language file for " + languageCode + ", using fallback translations");
        }
    }
    catch (const std::exception& e)
    {
        DBG("LocalizationManager: Error loading language file: " + juce::String(e.what()));
        translations.clear();
        loadFallbackTranslations();
    }
    catch (...)
    {
        DBG("LocalizationManager: Unknown error loading language file");
        translations.clear();
        loadFallbackTranslations();
    }
}

//==============================================================================
void LocalizationManager::parseTranslationFile(const juce::File& file)
{
    try
    {
        if (!file.existsAsFile())
        {
            DBG("LocalizationManager: Translation file does not exist: " + file.getFullPathName());
            return;
        }
        
        juce::String fileContent = file.loadFileAsString();
        if (fileContent.isEmpty())
        {
            DBG("LocalizationManager: Translation file is empty: " + file.getFullPathName());
            return;
        }
        
        juce::StringArray lines = juce::StringArray::fromLines(fileContent);
        
        for (const juce::String& line : lines)
        {
            // Skip comments and empty lines
            juce::String trimmedLine = line.trim();
            if (trimmedLine.startsWith("#") || trimmedLine.isEmpty())
                continue;
            
            // Parse key=value format
            int equalsPos = trimmedLine.indexOf("=");
            if (equalsPos > 0 && equalsPos < trimmedLine.length() - 1)
            {
                juce::String key = trimmedLine.substring(0, equalsPos).trim();
                juce::String value = trimmedLine.substring(equalsPos + 1).trim();
                
                // Remove quotes if present
                if (value.length() >= 2 && value.startsWith("\"") && value.endsWith("\""))
                    value = value.substring(1, value.length() - 1);
                
                if (!key.isEmpty())
                {
                    translations[key] = value;
                }
            }
        }
    }
    catch (const std::exception& e)
    {
        DBG("LocalizationManager: Error parsing translation file: " + juce::String(e.what()));
    }
    catch (...)
    {
        DBG("LocalizationManager: Unknown error parsing translation file");
    }
}

//==============================================================================
void LocalizationManager::updateCulturalSettings()
{
    // Set formatting based on language/culture
    if (currentLanguage == "en_US")
    {
        decimalSeparator = ".";
        thousandsSeparator = ",";
        use24HourTime = false;
        isRTL = false;
    }
    else if (currentLanguage == "en_GB")
    {
        decimalSeparator = ".";
        thousandsSeparator = ",";
        use24HourTime = true;  // UK typically uses 24-hour time
        isRTL = false;
    }
    else if (currentLanguage.startsWith("es"))  // Spanish
    {
        decimalSeparator = ",";
        thousandsSeparator = ".";
        use24HourTime = true;
        isRTL = false;
    }
    else if (currentLanguage == "fr_FR")  // French
    {
        decimalSeparator = ",";
        thousandsSeparator = " ";  // French uses space as thousands separator
        use24HourTime = true;
        isRTL = false;
    }
    else if (currentLanguage == "de_DE")  // German
    {
        decimalSeparator = ",";
        thousandsSeparator = ".";
        use24HourTime = true;
        isRTL = false;
    }
    // Add more cultures as needed...
}

//==============================================================================
juce::String LocalizationManager::getText(const juce::String& key) const
{
    auto it = translations.find(key);
    if (it != translations.end())
        return it->second;
    
    // Return the key itself as fallback (helpful for development)
    return "[" + key + "]";
}

//==============================================================================
juce::String LocalizationManager::getText(const juce::String& key, int count) const
{
    // Try plural form first (key + ".plural" or key + ".zero")
    juce::String pluralKey;
    
    if (count == 0)
        pluralKey = key + ".zero";
    else if (count == 1)
        pluralKey = key + ".one";  // Singular
    else
        pluralKey = key + ".other";  // Plural
    
    juce::String pluralText = getText(pluralKey);
    if (pluralText != "[" + pluralKey + "]")
    {
        // Replace {0} with count
        return pluralText.replace("{0}", juce::String(count));
    }
    
    // Fallback to regular key
    return getText(key);
}

//==============================================================================
juce::String LocalizationManager::getTextWithParams(const juce::String& key, 
                                                   const juce::StringArray& params) const
{
    juce::String text = getText(key);
    return substituteParameters(text, params);
}

//==============================================================================
juce::String LocalizationManager::substituteParameters(const juce::String& text, 
                                                       const juce::StringArray& params) const
{
    juce::String result = text;
    
    for (int i = 0; i < params.size(); ++i)
    {
        juce::String placeholder = "{" + juce::String(i) + "}";
        result = result.replace(placeholder, params[i]);
    }
    
    return result;
}

//==============================================================================
juce::String LocalizationManager::formatDate(const juce::Time& time, bool includeYear) const
{
    if (currentLanguage == "en_US")
    {
        if (includeYear)
            return time.toString(false, true, false).substring(0, 10);  // MM/DD/YYYY
        else
            return time.toString(false, true, false).substring(0, 5);   // MM/DD
    }
    else  // Most other countries use DD/MM format
    {
        juce::String dateStr = time.toString(false, true, false);
        // Convert MM/DD/YYYY to DD/MM/YYYY format
        // This is a simplified conversion - full implementation would use proper locale formatting
        return dateStr;  // TODO: Implement proper date formatting per locale
    }
}

//==============================================================================
juce::String LocalizationManager::formatTime(const juce::Time& time, bool includeSeconds) const
{
    if (use24HourTime)
    {
        if (includeSeconds)
            return time.formatted("%H:%M:%S");
        else
            return time.formatted("%H:%M");
    }
    else  // 12-hour format
    {
        if (includeSeconds)
            return time.formatted("%I:%M:%S %p");
        else
            return time.formatted("%I:%M %p");
    }
}

//==============================================================================
juce::String LocalizationManager::formatDuration(int seconds) const
{
    int mins = seconds / 60;
    int secs = seconds % 60;
    
    return juce::String::formatted("%d:%02d", mins, secs);
}

//==============================================================================
juce::String LocalizationManager::formatNumber(double number, int decimals) const
{
    juce::String numStr = juce::String(number, decimals);
    
    // Replace decimal point with localized separator
    if (decimalSeparator != ".")
        numStr = numStr.replace(".", decimalSeparator);
    
    // TODO: Add thousands separator formatting for large numbers
    
    return numStr;
}

//==============================================================================
bool LocalizationManager::isRightToLeft() const
{
    return isRTL;
}

//==============================================================================
juce::Justification LocalizationManager::getDefaultTextJustification() const
{
    return isRTL ? juce::Justification::centredRight : juce::Justification::centredLeft;
}

//==============================================================================
void LocalizationManager::reloadLanguageFiles()
{
    loadLanguageFile(currentLanguage);
}

//==============================================================================
bool LocalizationManager::hasTranslation(const juce::String& key) const
{
    return translations.find(key) != translations.end();
}

//==============================================================================
juce::String LocalizationManager::getDebugInfo() const
{
    juce::String info;
    info << "Current Language: " << currentLanguage << "\n";
    info << "Translations loaded: " << translations.size() << "\n";
    info << "Decimal separator: " << decimalSeparator << "\n";
    info << "Thousands separator: " << thousandsSeparator << "\n";
    info << "24-hour time: " << (use24HourTime ? "Yes" : "No") << "\n";
    info << "Right-to-left: " << (isRTL ? "Yes" : "No") << "\n";
    
    return info;
}

//==============================================================================
// Private helper methods

void LocalizationManager::loadFallbackTranslations()
{
    // Load essential translations to ensure app functionality
    translations["app.name"] = "Encore Karaoke";

    // ── NavBar menu items ──
    translations["nav.home"] = "Home";
    translations["nav.search"] = "Search";
    translations["nav.library"] = "Library";
    translations["nav.charts"] = "Charts";
    translations["nav.mixer"] = "Mixer";
    translations["nav.settings"] = "Settings";
    translations["nav.testing"] = "Testing";
    translations["nav.ads"] = "Ads";
    translations["nav.playlist"] = "Playlist";
    translations["nav.venue_management"] = "Venue Mgmt";
    translations["nav.queue"] = "Queue";
    translations["nav.songs"] = "Songs";
    translations["nav.genres"] = "GENRES:";

    // ── TopBar ──
    translations["topbar.key"] = "KEY";
    translations["topbar.bpm"] = "BPM";
    translations["topbar.offline_warning"] = "YOUR COMPUTER IS OFFLINE - PLEASE CONNECT TO THE INTERNET";
    translations["topbar.no_cover"] = "No Cover";
    translations["topbar.anonymous"] = "Anonymous";
    translations["topbar.vu"] = "VU";

    // ── BottomBar ──
    translations["bottombar.pitch"] = "PITCH";
    translations["bottombar.volume"] = "VOL";
    translations["bottombar.return_to_zero"] = "Return to 0";
    translations["bottombar.stop"] = "Stop and return to zero";
    translations["bottombar.play_pause"] = "Play/Pause";
    translations["bottombar.jump_to_end"] = "Jump to end";

    // ── MainArea placeholder pages ──
    translations["page.home"] = "Home";
    translations["page.search"] = "Search";
    translations["page.library"] = "Library";
    translations["page.charts"] = "Charts";
    translations["page.mixer"] = "Mixer";
    translations["page.settings"] = "Settings";
    translations["page.testing"] = "Testing";
    translations["page.ads"] = "Ads";
    translations["page.playlist"] = "Playlist";
    translations["page.venue_management"] = "Venue Management";

    // ── General audio / transport ──
    translations["audio.play"] = "Play";
    translations["audio.pause"] = "Pause";
    translations["audio.stop"] = "Stop";

    // ── Status ──
    translations["status.ready"] = "Ready";
    translations["status.loading"] = "Loading...";
    translations["status.connected"] = "Connected";
    translations["status.disconnected"] = "Disconnected";

    // ── Buttons ──
    translations["button.ok"] = "OK";
    translations["button.cancel"] = "Cancel";
    translations["button.close"] = "Close";

    // ── Languages ──
    translations["language.english"] = "English";
    translations["language.spanish"] = "Español";
    translations["language.french"] = "Français";
    translations["language.german"] = "Deutsch";

    // ── QueueBar ──
    translations["queue.now_singing"] = "Now Singing:";
    translations["queue.clear_queue"] = "Clear Queue";
    translations["queue.queue_label"] = "Queue";
    translations["queue.auto_play"] = "Auto Play";
    translations["queue.delay_label"] = "Delay (sec):";

    // ── HomePage ──
    translations["home.recently_played"] = "Recently Played";
    translations["home.new_songs"] = "New Songs";
    translations["home.popular_songs"] = "Popular Songs";
    translations["home.recommended_songs"] = "Recommended Songs";

    // Search page
    translations["search.placeholder"]  = "Search songs...";
    translations["search.clear"]        = "Clear";
    translations["search.filter_all"]   = "All";
    translations["search.filter_song"]  = "Song";
    translations["search.filter_artist"]= "Artist";
    translations["search.filter_year"]  = "Year";
    translations["search.filter_genre"] = "Genre";
    translations["search.count_suffix"] = "songs in this list";
    translations["search.col_song"]     = "SONG";
    translations["search.col_artist"]   = "ARTIST";
    translations["search.col_version"]  = "VERSION";
    translations["search.col_year"]     = "YEAR";
    translations["search.col_genre"]    = "GENRE";
    translations["search.scroll_top"]   = "Top";
    translations["search.row_edit"]     = "Edit";

    translations["library.title"]             = "LIBRARY";
    translations["library.path_label"]        = "Path to Karaoke Song Files:";
    translations["library.scanning"]          = "Scanning:";
    translations["library.applying_metadata"] = "Applying metadata:";
    translations["library.btn_initial_load"]  = "Initial Song Load";
    translations["library.btn_add_songs"]     = "Add Songs";
    translations["library.btn_get_metadata"]  = "Get Meta Data";
    translations["library.btn_edit_genres"]   = "Edit Genres";
    translations["library.chooser_root"]      = "Select Karaoke Collection Root Directory";
    translations["library.chooser_append"]    = "Select Directory to Add Songs From";
    translations["library.no_songs"]          = "No songs loaded. Run an Initial Song Load first.";
    translations["library.songs_loaded"]      = "{n} songs loaded.";
    translations["library.stats_total"]       = "Total Songs:        ";
    translations["library.stats_meta"]        = "Metadata Available: ";
    translations["library.stats_cdg"]         = "CDG Files:          ";
    translations["library.stats_zip"]         = "ZIP Files:          ";
    translations["library.stats_mp4"]         = "MP4 Files:          ";
    translations["library.stats_m4a"]         = "M4A Files:          ";
    translations["library.stats_xml"]         = "XML Files:          ";
    translations["library.stats_unknown"]     = "Unknown Files:      ";
    translations["library.stats_groups"]      = "Folders/Groups:     ";
    translations["library.edit_genres_title"] = "Edit Genres";
    translations["library.edit_genres_body"]  = "Genre editor coming soon.\n\nGenres are automatically extracted from your CDG collection folders and metadata.";
}

juce::File LocalizationManager::findLanguageFile(const juce::String& languageCode)
{
    // Try multiple locations for language files
    juce::StringArray searchPaths = 
    {
        // Application directory
        juce::File::getSpecialLocation(juce::File::currentApplicationFile).getParentDirectory().getChildFile("Resources/Languages").getFullPathName(),
        // Working directory (development)
        juce::File::getCurrentWorkingDirectory().getChildFile("Resources/Languages").getFullPathName(),
        // User documents (fallback)
        juce::File::getSpecialLocation(juce::File::userDocumentsDirectory).getChildFile("EncoreKaraoke/Languages").getFullPathName()
    };
    
    for (const juce::String& path : searchPaths)
    {
        juce::File languageFile = juce::File(path).getChildFile(languageCode + ".txt");
        if (languageFile.existsAsFile())
        {
            return languageFile;
        }
    }
    
    // Return invalid file if not found
    return juce::File();
}