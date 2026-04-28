/*
  ==============================================================================

    MainComponent.cpp
    Created: 15 Apr 2026 7:04:12pm
    Author:  GitHub Copilot

    Main application component implementation

  ==============================================================================
*/

#include "MainComponent.h"
#include "../Services/WaveformGenerator.h"
#include "../Services/VenueService.h"
#include "../Services/QueueService.h"
#include "../Services/ImageCache.h"
#include <cmath>

//==============================================================================
// Simple semi-transparent overlay shown while a song is loading. It paints a
// dim backdrop, a rounded card and a label, plus an indeterminate activity
// indicator. It intercepts all mouse input so the user can't fire another
// Play Now while the first one is still resolving.
//==============================================================================
class MainComponent::LoadingOverlay : public juce::Component,
                                      private juce::Timer
{
public:
    LoadingOverlay()
    {
        setInterceptsMouseClicks(true, false);
        setAlwaysOnTop(true);
        startTimerHz(30);
    }

    void setMessage(const juce::String& msg) { message_ = msg; repaint(); }

    void paint(juce::Graphics& g) override
    {
        // Dimmed backdrop
        g.fillAll(juce::Colour(0xcc000000));

        // Centred card
        const int cardW = 320;
        const int cardH = 120;
        auto card = juce::Rectangle<int>(cardW, cardH)
                        .withCentre(getLocalBounds().getCentre());

        g.setColour(juce::Colour(0xff1a2030));
        g.fillRoundedRectangle(card.toFloat(), 10.0f);
        g.setColour(juce::Colour(0xff30daff));
        g.drawRoundedRectangle(card.toFloat().reduced(0.5f), 10.0f, 1.5f);

        // Spinner
        auto spinnerBox = card.removeFromLeft(56).reduced(12).toFloat();
        drawSpinner(g, spinnerBox);

        // Message
        g.setColour(juce::Colours::white);
        g.setFont(juce::Font(juce::FontOptions().withHeight(18.0f)).boldened());
        g.drawFittedText(message_, card.reduced(12, 8), juce::Justification::centredLeft, 2);
    }

private:
    void timerCallback() override
    {
        phase_ += 0.12f;
        if (phase_ > juce::MathConstants<float>::twoPi)
            phase_ -= juce::MathConstants<float>::twoPi;
        repaint();
    }

    void drawSpinner(juce::Graphics& g, juce::Rectangle<float> r) const
    {
        const auto centre = r.getCentre();
        const float radius = juce::jmin(r.getWidth(), r.getHeight()) * 0.5f - 2.0f;
        const int segments = 12;
        for (int i = 0; i < segments; ++i)
        {
            float angle = phase_ + (juce::MathConstants<float>::twoPi * i) / (float) segments;
            float alpha = 0.15f + 0.85f * ((float) i / (float) segments);
            g.setColour(juce::Colour(0xff30daff).withAlpha(alpha));
            juce::Point<float> p1(centre.x + std::cos(angle) * radius * 0.55f,
                                  centre.y + std::sin(angle) * radius * 0.55f);
            juce::Point<float> p2(centre.x + std::cos(angle) * radius,
                                  centre.y + std::sin(angle) * radius);
            g.drawLine({ p1, p2 }, 2.6f);
        }
    }

    juce::String message_ { "Loading song..." };
    float phase_ = 0.0f;
};

//==============================================================================
MainComponent::MainComponent()
{
    // Set initial size (will be adjusted by responsive system)
    setSize(1200, 800);

    // Create audio engine up front so it's ready before UI hooks it.
    audioEngine = std::make_unique<AudioEngine>();
    audioEngine->initialize();

    // Open the singer-facing lyric display window on the secondary monitor
    // (falls back to a windowed display if there's only one screen). It
    // shows an idle screen until a song is loaded.
    lyricWindow_ = std::make_unique<LyricDisplayWindow> (audioEngine.get());

    try
    {
        // Setup UI components carefully
        setupUI();
        
        // Force initial layout after components are created
        resized();
        
        // Ensure components are visible and painted
        repaint();
        
        // Start timer for periodic updates - disabled until safer implementation
        // startTimer(2000);
        
        // Initial screen configuration - simplified
        // detectAndConfigureScreens(); // Commenting out temporarily
        
        DBG("MainComponent initialized successfully");
    }
    catch (const std::exception& e)
    {
        DBG("Error in MainComponent constructor: " + juce::String(e.what()));
        // Fallback: create a simple label
        titleLabel = std::make_unique<juce::Label>("title", "Encore Karaoke"); 
        titleLabel->setFont(juce::Font(juce::FontOptions().withHeight(28.0f)).boldened());
        titleLabel->setJustificationType(juce::Justification::centred);
        addAndMakeVisible(titleLabel.get());
    }
}

MainComponent::~MainComponent()
{
    stopTimer();
    // Close the secondary display before the audio engine goes away — its
    // timer may be trying to poll the engine's position.
    lyricWindow_.reset();
    if (audioEngine) audioEngine->shutdown();
}

void MainComponent::setupUI()
{
    // Setup UI with LocalizationManager integration
    DBG("Starting setupUI with LocalizationManager");
    
    // Create TopBar first
    topBar = std::make_unique<TopBar>();
    addAndMakeVisible(topBar.get());

    // Create BottomBar (music transport and waveform controls)
    bottomBar = std::make_unique<BottomBar>();
    addAndMakeVisible(bottomBar.get());
    
    // Set up TopBar callbacks
    topBar->onLogoClicked = [this]() {
        DBG("Encore logo clicked");
        // TODO: Implement logo click action (about dialog, etc.)
    };
    
    topBar->onUserButtonClicked = [this]() {
        DBG("User button clicked");
        // TODO: Implement user menu/account access
    };
    
    topBar->onHeightChanged = [this](int newHeight) {
        DBG("TopBar height changed to: " + juce::String(newHeight));
        resized(); // Re-layout when TopBar height changes
    };

    bottomBar->onHeightChanged = [this](int newHeight) {
        DBG("BottomBar height changed to: " + juce::String(newHeight));
        resized(); // Re-layout when BottomBar height changes
    };
    
    // Set sample data for TopBar
    topBar->setOnlineStatus(true); // Start with online status
    topBar->setUserInfo("Demo User", juce::Image());

    // BottomBar callbacks — drive the AudioEngine
    bottomBar->onReturnToZero = [this]() {
        if (audioEngine) audioEngine->seekToPosition(0.0);
        bottomBar->setProgress(0.0f);
    };

    bottomBar->onStopAndReturnToZero = [this]() {
        if (audioEngine) audioEngine->stop();
        bottomBar->setProgress(0.0f);
        bottomBar->setPlaying(false);
    };

    bottomBar->onPlayPause = [this](bool isNowPlaying) {
        if (! audioEngine) return;
        if (isNowPlaying) audioEngine->play();
        else              audioEngine->pause();
    };

    bottomBar->onJumpToEnd = [this]() {
        if (audioEngine && audioEngine->getTotalLength() > 0.25)
            audioEngine->seekToPosition(audioEngine->getTotalLength() - 0.25);
    };

    bottomBar->onSeek = [this](float newProgress) {
        if (! audioEngine) return;
        double total = audioEngine->getTotalLength();
        if (total > 0.0)
            audioEngine->seekToPosition(total * (double) juce::jlimit(0.0f, 1.0f, newProgress));
    };

    bottomBar->onPitchChanged = [this](int semitones) {
        if (audioEngine) audioEngine->setPitchShift((float) semitones);
    };

    bottomBar->onVolumeChanged = [this](int volumeStep) {
        if (! audioEngine) return;
        // volumeStep is 0..10 in the slider -> map to 0..1.
        float v = juce::jlimit(0.0f, 1.0f, (float) volumeStep / 10.0f);
        audioEngine->setMasterVolume(v);
        audioEngine->setMusicVolume(v);
    };

    // The BottomBar's own 30Hz timer will no longer auto-advance progress —
    // our 100ms timer below polls the real AudioEngine position instead.
    bottomBar->setExternalProgressControl(true);

    // Start timer to poll audio engine + update VU / progress UI
    startTimer(50);
    
    DBG("TopBar created and configured");
    
    // Create NavBar (left navigation)
    navBar = std::make_unique<NavBar>();
    addAndMakeVisible(navBar.get());

    // Create MainArea (central content area)
    mainArea = std::make_unique<MainArea>();
    addAndMakeVisible(mainArea.get());

    // Wire NavBar page selection to MainArea
    navBar->onPageSelected = [this](NavPage page) {
        DBG("NavBar: page selected -> " + juce::String(static_cast<int>(page)));
        mainArea->setCurrentPage(page);
    };

    // Handle Song Selection dialog result from Home / Search
    mainArea->onSongSelectionResult = [this](const SongSelectionResult& r)
    {
        if (r.action == SongSelectionResult::Action::PlayNow)
            loadAndPlaySong(r.song, r.versionIndex, r.pitchSemitones);
        // Play Next / Add to Queue will be wired to the queue service later.
    };

    // Push settings-page edits back to Firestore. On success the queue bar
    // and lyric display also pick up any name / code changes.
    mainArea->onVenueSettingsChanged = [this](const VenueItem& updated)
    {
        if (activeVenueId_.isEmpty())
            return;

        juce::Component::SafePointer<MainComponent> safe (this);
        const auto venueId = activeVenueId_;
        VenueService::getInstance().saveVenue (venueId, updated,
            [safe, updated](bool ok, juce::String error)
            {
                if (safe == nullptr) return;
                if (! ok)
                {
                    DBG ("[Venue] save failed: " << error);
                    return;
                }

                if (safe->queueBar != nullptr)
                    safe->queueBar->setVenueInfo (juce::String(updated.name),
                                                  juce::String(updated.code));
                if (safe->lyricWindow_ != nullptr)
                    if (auto* d = safe->lyricWindow_->getDisplay())
                        d->setVenueCode (juce::String(updated.code));
            });
    };

    // When NavBar width changes via drag, re-layout
    navBar->onWidthChanged = [this](int /*newWidth*/) {
        resized();
    };

    // Sample genre list for the bottom half of NavBar
    navBar->setGenreList({ "Pop", "Rock", "Country", "R&B", "Hip Hop",
                           "Dance", "Latin", "Jazz", "Classical", "Oldies" });

    DBG("NavBar and MainArea created and configured");

    // Create QueueBar (right-side singer queue)
    queueBar = std::make_unique<QueueBar>();
    addAndMakeVisible(queueBar.get());

    queueBar->onWidthChanged = [this](int /*newWidth*/) {
        resized();
    };

    queueBar->onPlaySinger = [this](int singerIndex) {
        DBG("QueueBar: Play singer at index " + juce::String(singerIndex));
    };

    queueBar->onPlayCurrent = [this]() {
        DBG("QueueBar: Play current singer");
    };

    queueBar->onPauseCurrent = [this]() {
        DBG("QueueBar: Pause current singer");
    };

    queueBar->onClearQueue = [this]() {
        DBG("QueueBar: Clear queue requested");
    };

    queueBar->onReorder = [this](int from, int to) {
        DBG("QueueBar: Reorder singer from " + juce::String(from) + " to " + juce::String(to));
    };

    // Populate with sample data so the queue is visible on launch
    {
        // Queue starts empty until a venue is loaded; setVenueId() fetches
        // the live queue from Firestore at venues/<venueId>/queue and
        // populates the bar via QueueService.
        queueBar->clearNowPlaying();
        queueBar->setSingers({});
    }

    DBG("QueueBar created (waiting on venue queue load)");

    // Title label - using LocalizationManager
    titleLabel = std::make_unique<juce::Label>("title", LocalizationManager::getInstance().getText("app.name"));
    titleLabel->setFont(juce::Font(juce::FontOptions().withHeight(28.0f)).boldened());
    titleLabel->setJustificationType(juce::Justification::centred);
    titleLabel->setColour(juce::Label::textColourId, juce::Colours::darkblue);
    addAndMakeVisible(titleLabel.get());
    DBG("Title label created with localized text");

    // Language selection button - using LocalizationManager
    languageButton = std::make_unique<juce::TextButton>(LocalizationManager::getInstance().getText("language.english"));
    languageButton->onClick = [this]() { 
        DBG("Language button clicked");
        showLanguageSelector(); 
    };
    addAndMakeVisible(languageButton.get());
    DBG("Language button created with localized text");
    
    // Status display - using LocalizationManager
    statusLabel = std::make_unique<juce::Label>("status", LocalizationManager::getInstance().getText("status.ready"));
    statusLabel->setFont(juce::Font(juce::FontOptions().withHeight(14.0f)));
    statusLabel->setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(statusLabel.get());
    DBG("Status label created with localized text");
    
    // Debug information (always visible for now)
    debugLabel = std::make_unique<juce::Label>("debug", "Debug: Application Running");
    debugLabel->setFont(juce::Font(juce::FontOptions().withHeight(12.0f)));
    debugLabel->setJustificationType(juce::Justification::bottomLeft);
    debugLabel->setColour(juce::Label::textColourId, juce::Colours::grey);
    addAndMakeVisible(debugLabel.get());
    DBG("Debug label created");
    
    // Force initial bounds for all components to ensure visibility
    auto tempBounds = juce::Rectangle<int>(20, 70, 1160, 710);  // Reserve top 50px for TopBar
    if (titleLabel) titleLabel->setBounds(tempBounds.removeFromTop(60));
    if (languageButton) languageButton->setBounds(tempBounds.removeFromTop(40).withWidth(200));
    if (statusLabel) statusLabel->setBounds(tempBounds.removeFromTop(30));
    if (debugLabel) debugLabel->setBounds(tempBounds.removeFromTop(30));
    
    DBG("setupUI completed successfully with initial bounds set");
    
    // Load default background tile
    loadBackgroundTile();
}

//==============================================================================
void MainComponent::loadBackgroundTile(const juce::String& path)
{
    auto appDir = juce::File::getSpecialLocation(juce::File::currentExecutableFile).getParentDirectory();
    juce::String tilePath = path.isEmpty() ? "assets/images/backgrounds/background1.png" : path;
    juce::File tileFile = appDir.getChildFile(tilePath);
    if (tileFile.existsAsFile())
    {
        backgroundTileImage_ = juce::ImageFileFormat::loadFrom(tileFile);
        if (backgroundTileImage_.isValid())
            repaint();
    }
}

//==============================================================================
void MainComponent::paint(juce::Graphics& g)
{
    if (backgroundTileImage_.isValid())
    {
        // Scale tile to backgroundTileSize_ width, maintain aspect ratio (like CSS background-size: Npx auto)
        float aspectRatio = (float)backgroundTileImage_.getHeight() / (float)backgroundTileImage_.getWidth();
        int tileW = backgroundTileSize_;
        int tileH = juce::roundToInt(tileW * aspectRatio);
        if (tileH < 1) tileH = tileW;

        for (int y = 0; y < getHeight(); y += tileH)
            for (int x = 0; x < getWidth(); x += tileW)
                g.drawImage(backgroundTileImage_, x, y, tileW, tileH,
                            0, 0, backgroundTileImage_.getWidth(), backgroundTileImage_.getHeight());
    }
    else
    {
        // Fallback solid fill when no tile image is loaded
        g.fillAll(juce::Colour(0xff16213e));
    }
    
    // Draw responsive layout debug info in debug builds
    #if JUCE_DEBUG
    g.setColour(juce::Colours::yellow.withAlpha(0.3f));
    g.drawRect(getLocalBounds(), 2);
    
    // Simplified debug info to avoid getCurrentScreenSize() issues
    juce::String debugInfo = "Debug Mode - " + juce::String(getWidth()) + "x" + juce::String(getHeight());
    g.setColour(juce::Colours::yellow);
    g.setFont(juce::Font(juce::FontOptions().withHeight(10.0f)));
    g.drawText(debugInfo, getLocalBounds().reduced(5), 
               juce::Justification::topRight, true);
    #endif
}

//==============================================================================
void MainComponent::resized()
{
    // Basic responsive resized method
    auto bounds = getLocalBounds();

    // Non-mac: embedded menu bar at the very top.
   #if ! JUCE_MAC
    if (menuBar_ != nullptr && menuBar_->isVisible())
    {
        auto menuBounds = bounds.removeFromTop (24);
        menuBar_->setBounds (menuBounds);
    }
   #endif

    // Reserve bottom area for BottomBar
    if (bottomBar)
    {
        auto bottomBarBounds = bounds.removeFromBottom(bottomBar->getBarHeight());
        bottomBar->setBounds(bottomBarBounds);
    }
    
    // Reserve top area for TopBar
    if (topBar)
    {
        auto topBarBounds = bounds.removeFromTop(topBar->getBarHeight());
        topBar->setBounds(topBarBounds);
    }
    
    // NavBar on the left (resizable width)
    if (navBar)
    {
        auto navBounds = bounds.removeFromLeft(navBar->getBarWidth());
        navBar->setBounds(navBounds);
    }

    // QueueBar on the right (resizable width)
    if (queueBar)
    {
        auto queueBounds = bounds.removeFromRight(queueBar->getBarWidth());
        queueBar->setBounds(queueBounds);
    }

    // MainArea fills the remaining centre space
    if (mainArea)
    {
        mainArea->setBounds(bounds);
    }

    // Loading overlay always covers the full window when visible.
    if (loadingOverlay_)
        loadingOverlay_->setBounds(getLocalBounds());

    // The old placeholder labels are no longer laid out in the centre;
    // they can be hidden or removed entirely later.
    if (titleLabel)     titleLabel->setVisible(false);
    if (languageButton) languageButton->setVisible(false);
    if (statusLabel)    statusLabel->setVisible(false);
    if (debugLabel)     debugLabel->setVisible(false);
    
    // Basic responsive features (commented out until safer)
    // updateUIForScreenSize();
}

// Simple implementations to satisfy linker requirements
void MainComponent::showLanguageSelector()
{
    // Enhanced language selector using LocalizationManager
    DBG("Language selector clicked - using LocalizationManager");
    
    auto& lm = LocalizationManager::getInstance();
    
    // Create popup menu with available languages
    juce::PopupMenu languageMenu;
    juce::StringArray languages = lm.getAvailableLanguages();
    
    // Add fallback languages if none loaded from files
    if (languages.isEmpty())
    {
        languageMenu.addItem(1, "English (English)", true, lm.getCurrentLanguage() == "en_US");
        languageMenu.addItem(2, "Español (Spanish)", true, lm.getCurrentLanguage() == "es_ES");  
        languageMenu.addItem(3, "Français (French)", true, lm.getCurrentLanguage() == "fr_FR");
        languageMenu.addItem(4, "Deutsch (German)", true, lm.getCurrentLanguage() == "de_DE");
    }
    else
    {
        // Use languages from LocalizationManager
        for (int i = 0; i < languages.size(); ++i)
        {
            juce::String langInfo = languages[i];
            languageMenu.addItem(i + 1, langInfo, true, false);
        }
    }
    
    languageMenu.showMenuAsync(
        juce::PopupMenu::Options()
            .withTargetComponent(languageButton.get())
            .withStandardItemHeight(30),
        [this, &lm](int result)
        {
            if (result > 0)
            {
                juce::String selectedLang;
                
                // Map menu result to language codes
                switch (result)
                {
                    case 1: selectedLang = "en_US"; break;
                    case 2: selectedLang = "es_ES"; break; 
                    case 3: selectedLang = "fr_FR"; break;
                    case 4: selectedLang = "de_DE"; break;
                    default: selectedLang = "en_US"; break;
                }
                
                // Change language using LocalizationManager
                lm.setLanguage(selectedLang);
                
                // Update UI text
                updateAllText();
                
                DBG("Language changed to: " + selectedLang);
            }
        }
    );
}

void MainComponent::updateUIForScreenSize()
{
    // Basic UI updates for different screen sizes
    DBG("updateUIForScreenSize called");
    
    // Simple font scaling based on window size  
    float baseScale = juce::jmin(getWidth() / 1200.0f, getHeight() / 800.0f);
    baseScale = juce::jmax(0.7f, juce::jmin(baseScale, 2.0f)); // Clamp between 0.7 and 2.0
    
    if (titleLabel != nullptr)
    {
        auto scaledFont = juce::Font(juce::FontOptions().withHeight(28.0f * baseScale)).boldened();
        titleLabel->setFont(scaledFont);
    }
    
    if (statusLabel != nullptr)
    {
        auto scaledFont = juce::Font(juce::FontOptions().withHeight(14.0f * baseScale));
        statusLabel->setFont(scaledFont);
    }
    
    #if JUCE_DEBUG
    if (debugLabel != nullptr)
    {
        auto scaledFont = juce::Font(juce::FontOptions().withHeight(12.0f * baseScale));
        debugLabel->setFont(scaledFont);
    }
    #endif
    
    resized(); // Update layout
}

void MainComponent::timerCallback()
{
    updateConnectionStatus();
    updateDebugInfo();

    if (audioEngine == nullptr)
        return;

    // Feed real audio level into the VU meter.
    if (topBar != nullptr)
        topBar->setAudioLevel(juce::jlimit(0.0f, 1.0f, audioEngine->getCurrentLevel() * 4.0f));

    // Feed real playback position into the BottomBar progress/time labels.
    if (bottomBar != nullptr)
    {
        // Prefer the lyric-window video's position when an MP4 is playing.
        const bool videoActive = (lyricWindow_ != nullptr && lyricWindow_->isVideoActive());

        double total = videoActive ? lyricWindow_->getVideoDuration()
                                   : audioEngine->getTotalLength();
        if (total > 0.0)
        {
            double pos = videoActive ? lyricWindow_->getVideoPosition()
                                     : audioEngine->getCurrentPosition();
            pos = juce::jlimit (0.0, total, pos);
            bottomBar->setProgress((float) (pos / total));
        }
        bottomBar->setPlaying (videoActive ? true : audioEngine->isPlaying());
    }
}

void MainComponent::updateConnectionStatus()
{
    // Simple status updates without LocalizationManager
    static int statusCounter = 0;
    statusCounter++;
    
    juce::String statusText = "Status: Active (Update #" + juce::String(statusCounter) + ")";
    
    if (statusLabel != nullptr)
        statusLabel->setText(statusText, juce::dontSendNotification);
}

void MainComponent::updateDebugInfo()
{
    // Simple debug info updates
    juce::String debugText = "Size: " + juce::String(getWidth()) + "x" + juce::String(getHeight());
    debugText += " | Timer Active | Memory: " + juce::String(juce::SystemStats::getMemorySizeInMegabytes()) + "MB";
    
    if (debugLabel != nullptr)
        debugLabel->setText(debugText, juce::dontSendNotification);
}

// Configuration methods temporarily disabled for debugging
/*
void MainComponent::configureForMobile()
{
    auto bounds = getLocalBounds();
    auto margin = getScaledMargin(12); // Smaller margin for mobile
    bounds.reduce(margin, margin);
    
    // Vertical stack layout for mobile
    auto titleHeight = 60;
    auto buttonHeight = 44;
    auto spacing = 16;
    
    titleLabel->setBounds(bounds.removeFromTop(titleHeight));
    bounds.removeFromTop(spacing);
    
    languageButton->setBounds(bounds.removeFromTop(buttonHeight));
    bounds.removeFromTop(spacing);
    
    statusLabel->setBounds(bounds.removeFromTop(30));
    
    #if JUCE_DEBUG
    debugLabel->setBounds(bounds.removeFromBottom(40));
    #endif
}

void MainComponent::configureForStandard()
{
    auto bounds = getLocalBounds();
    auto margin = getScaledMargin(16); // Standard margin
    bounds.reduce(margin, margin);
    
    // Standard laptop/desktop layout (HD, WXGA, etc.)
    auto titleArea = bounds.removeFromTop(80);
    titleLabel->setBounds(titleArea);
    
    auto topControls = bounds.removeFromTop(50);
    languageButton->setBounds(topControls.removeFromRight(150));
    
    bounds.removeFromTop(20);
    statusLabel->setBounds(bounds.removeFromTop(30));
    
    #if JUCE_DEBUG
    debugLabel->setBounds(bounds.removeFromBottom(40));
    #endif
}
// Configuration methods temporarily disabled for debugging
/*void MainComponent::configureForWide()
{
    auto bounds = getLocalBounds();
    auto margin = getScaledMargin(20); // Larger margin for wide screens
    bounds.reduce(margin, margin);
    
    // Wide screen layout (Full HD, QHD, etc.)
    auto header = bounds.removeFromTop(100);
    titleLabel->setBounds(header.removeFromTop(60));
    
    auto controls = header.removeFromTop(40);
    languageButton->setBounds(controls.removeFromRight(150));
    
    // More space available, adjust accordingly
    bounds.removeFromTop(20);
    statusLabel->setBounds(bounds.removeFromTop(25));
    
    #if JUCE_DEBUG
    debugLabel->setBounds(bounds.removeFromBottom(30));
    #endif
}

void MainComponent::configureForUltraWide()
{
    auto bounds = getLocalBounds();
    auto margin = getScaledMargin(24); // Even larger margin for ultra-wide
    bounds.reduce(margin, margin);
    
    // Ultra-wide layout (21:9, 32:9 aspect ratios)
    // Use horizontal space more efficiently
    auto header = bounds.removeFromTop(80);
    
    // Split header horizontally for ultra-wide
    auto titleArea = header.removeFromLeft(header.getWidth() * 0.6f);
    titleLabel->setBounds(titleArea);
    
    auto controlArea = header.reduced(20, 0);
    languageButton->setBounds(controlArea.removeFromRight(150));
    
    bounds.removeFromTop(20);
    
    // Status area - use left portion of ultra-wide screen
    auto statusArea = bounds.removeFromTop(30);
    statusLabel->setBounds(statusArea.removeFromLeft(statusArea.getWidth() * 0.7f));
    
    #if JUCE_DEBUG
    auto debugArea = bounds.removeFromBottom(30);
    debugLabel->setBounds(debugArea.removeFromLeft(debugArea.getWidth() * 0.8f));
    #endif
}

/*
void MainComponent::configureForHighResolution()
{
    // 4K/5K/8K displays - use desktop layout with automatic font scaling applied
    configureForWide();
    
    // Additional adjustments for very high resolution displays
    if (getCurrentScreenSize() == ScreenSize::UHD_8K)
    {
        // For 8K displays, we might want even more spacing
        auto bounds = getLocalBounds();
        auto extraMargin = bounds.getWidth() * 0.05f; // 5% extra margin on 8K
        
        // Apply extra margin to all components (this is illustrative)
        if (titleLabel)
        {
            auto titleBounds = titleLabel->getBounds();
            titleBounds.reduce(static_cast<int>(extraMargin * 0.5f), 0);
            titleLabel->setBounds(titleBounds);
        }
    }
}

//==============================================================================
void MainComponent::updateUIForScreenSize()
{
    ResponsiveLayout::updateUIForScreenSize();
    
    // Apply font scaling based on screen size
    if (titleLabel != nullptr)
    {
        auto baseFont = 28.0f * getScaleFactor();
        titleLabel->setFont(juce::Font(juce::FontOptions().withHeight(baseFont)).boldened());
    }
    
    if (languageButton != nullptr)
    {
        // TextButton font is handled by LookAndFeel, not setFont
    }
    
    if (statusLabel != nullptr)
    {
        auto baseFont = 14.0f * getScaleFactor();
        statusLabel->setFont(juce::Font(juce::FontOptions().withHeight(baseFont)));
    }
    
    #if JUCE_DEBUG
    if (debugLabel != nullptr)
    {
        auto baseFont = 12.0f * getScaleFactor();
        debugLabel->setFont(juce::Font(juce::FontOptions().withHeight(baseFont)));
    }
    #endif
}
*/

//==============================================================================
void MainComponent::updateAllText()
{
    // Update all UI text using LocalizationManager 
    DBG("updateAllText called - refreshing localized text");
    
    auto& lm = LocalizationManager::getInstance();
    
    try
    {
        // Update title
        if (titleLabel != nullptr)
        {
            titleLabel->setText(lm.getText("app.name"), juce::dontSendNotification);
        }
        
        // Update language button with current language display name  
        if (languageButton != nullptr)
        {
            languageButton->setButtonText(lm.getCurrentLanguageDisplayName());
        }
        
        // Update status
        if (statusLabel != nullptr)
        {
            statusLabel->setText(lm.getText("status.ready"), juce::dontSendNotification);
        }
        
        // Update debug info
        if (debugLabel != nullptr)
        {
            juce::String debugText = "Debug: " + lm.getCurrentLanguage() + " Active";
            debugLabel->setText(debugText, juce::dontSendNotification);
        }
        
        // Propagate to child bar components
        if (topBar)    topBar->updateAllText();
        if (bottomBar) bottomBar->updateAllText();
        if (navBar)    navBar->updateAllText();
        if (mainArea)  mainArea->updateAllText();
        if (queueBar)  queueBar->updateAllText();
        
        // Trigger repaint to update any drawn text
        repaint();
        
        DBG("All UI text updated successfully");
    }
    catch (const std::exception& e)
    {
        DBG("Error updating UI text: " + juce::String(e.what()));
    }
    catch (...)
    {
        DBG("Unknown error updating UI text");
    }
}

// Methods temporarily disabled for debugging
/*
//==============================================================================
void MainComponent::changeLanguage(const juce::String& languageCode)
{
    auto& lm = LocalizationManager::getInstance();
    
    lm.setLanguage(languageCode);
    updateAllText();
    repaint();
    
    DBG("Language changed to: " + languageCode);
        
    // Save preference (in a real app, this would go to settings)
    // For now, just log it
    auto message = lm.getText("status.language_changed");
    message = message.replace("{language}", lm.getCurrentLanguageDisplayName());
    statusLabel->setText(message, juce::dontSendNotification);
}

void MainComponent::showLanguageSelector()
{
    // Simplified language selector - no LocalizationManager for now
    DBG("Language selector clicked - functionality temporarily disabled");
    
    // Show a simple popup menu with basic options
    juce::PopupMenu languageMenu;
    languageMenu.addItem(1, "English", true, true);
    languageMenu.addItem(2, "Español", true, false);
    languageMenu.addItem(3, "Français", true, false);
    
    languageMenu.showMenuAsync(
        juce::PopupMenu::Options()
            .withTargetComponent(languageButton.get())
            .withStandardItemHeight(30),
        [this](int result)
        {
            DBG("Selected language option: " + juce::String(result));
            // Language change functionality disabled for now
        }
    );
}

//==============================================================================
void MainComponent::detectAndConfigureScreens()
{
    auto& desktop = juce::Desktop::getInstance();
    auto primaryDisplay = desktop.getDisplays().getPrimaryDisplay();
    
    // Simple implementation - just detect if there might be multiple displays
    // by checking if the primary display is not the main screen boundary
    auto mainScreenBounds = desktop.getDisplays().getTotalBounds(true);
    bool hasMultipleDisplays = mainScreenBounds.getWidth() > primaryDisplay->totalArea.getWidth() || 
                               mainScreenBounds.getHeight() > primaryDisplay->totalArea.getHeight();
    
    DBG("Primary display: " + 
        juce::String(primaryDisplay->totalArea.getWidth()) + "x" + 
        juce::String(primaryDisplay->totalArea.getHeight()));
        
    if (hasMultipleDisplays)
    {
        setupDualScreenLayout();
        DBG("Multiple displays detected");
    }
    else
    {
        DBG("Single display detected");
    }
}

void MainComponent::setupDualScreenLayout()
{
    // In a full karaoke system, this would setup:
    // - Primary display: DJ/operator interface
    // - Secondary display: Public song display with lyrics
    
    DBG("Setting up dual-screen karaoke layout");
    
    // For now, just log that we detected multiple screens
    auto& lm = LocalizationManager::getInstance();
    statusLabel->setText(lm.getText("status.dual_screen_detected"), juce::dontSendNotification);
}

//==============================================================================
void MainComponent::timerCallback()
{
    updateConnectionStatus();
    updateDebugInfo();
}

void MainComponent::updateConnectionStatus()
{
    // Simplified - no LocalizationManager for now
    static int statusCounter = 0;
    statusCounter++;
    
    juce::String statusText = "Status Update #" + juce::String(statusCounter);
    
    if (statusLabel != nullptr)
        statusLabel->setText(statusText, juce::dontSendNotification);
}

void MainComponent::updateDebugInfo()
{
    // Simplified debug info - no LocalizationManager calls
    juce::String debugText = "Size: " + juce::String(getWidth()) + "x" + juce::String(getHeight());
    debugText += " | Debug Mode Active";
    
    if (debugLabel != nullptr)
        debugLabel->setText(debugText, juce::dontSendNotification);
}

//==============================================================================
void MainComponent::setHighContrastMode(bool enabled)
{
    highContrastMode = enabled;
    
    // Apply high contrast colors
    if (enabled)
    {
        titleLabel->setColour(juce::Label::textColourId, juce::Colours::white);
        statusLabel->setColour(juce::Label::textColourId, juce::Colours::lightgrey);
    }
    else
    {
        titleLabel->setColour(juce::Label::textColourId, juce::Colours::darkblue);
        statusLabel->setColour(juce::Label::textColourId, juce::Colours::black);
    }
    
    repaint();
}

void MainComponent::setLargeTextMode(bool enabled)
{
    largeTextMode = enabled;
    
    // Increase font sizes for accessibility
    float textScale = enabled ? 1.25f : 1.0f;
    
    titleLabel->setFont(juce::Font(juce::FontOptions().withHeight(28.0f * textScale * getScaleFactor())).boldened());
    // TextButton font is handled by LookAndFeel, not setFont
    statusLabel->setFont(juce::Font(juce::FontOptions().withHeight(14.0f * textScale * getScaleFactor())));
    
    #if JUCE_DEBUG
    debugLabel->setFont(juce::Font(juce::FontOptions().withHeight(12.0f * textScale * getScaleFactor())));
    #endif
    
    resized(); // Relayout with new font sizes
}
*/
//==============================================================================
// Song playback
//==============================================================================
void MainComponent::loadAndPlaySong(const CdgSong& song, int versionIndex, int pitchSemitones)
{
    if (! audioEngine)
        return;

    // Show the loading overlay immediately so the user gets instant feedback
    // instead of the OS beach-ball that the synchronous load would otherwise
    // produce.
    showLoadingOverlay("Loading \"" + juce::String(song.songName) + "\"...");

    // Defer the actual load work to the next message-loop tick so the overlay
    // has a chance to paint before we block the UI thread on file I/O.
    juce::Component::SafePointer<MainComponent> self(this);
    juce::MessageManager::callAsync([self, song, versionIndex, pitchSemitones]()
    {
        if (! self || ! self->audioEngine)
            return;

    // Pick the file path for the chosen version (fall back to the first path).
    juce::String path;
    if (versionIndex >= 0 && versionIndex < (int) song.fullPath.size())
        path = juce::String(song.fullPath[(size_t) versionIndex]);
    else if (! song.fullPath.empty())
        path = juce::String(song.fullPath.front());

    if (path.isEmpty())
    {
        DBG("loadAndPlaySong: no file path on song \"" + juce::String(song.songName) + "\"");
        self->hideLoadingOverlay();
        return;
    }

    juce::File audioFile(path);

    // Some karaoke libraries store a ZIP containing CDG + MP3, or a .cdg file
    // beside a .mp3 — try a sibling MP3 if the direct path isn't playable.
    auto ext = audioFile.getFileExtension().toLowerCase();
    if (ext == ".cdg" || ext == ".zip")
    {
        auto sibling = audioFile.withFileExtension("mp3");
        if (sibling.existsAsFile())
            audioFile = sibling;
        ext = audioFile.getFileExtension().toLowerCase();
    }

    if (! audioFile.existsAsFile())
    {
        DBG("loadAndPlaySong: file not found: " + audioFile.getFullPathName());
        self->hideLoadingOverlay();
        return;
    }

    // ─── MP4 / video branch ────────────────────────────────────────────────
    // The lyric display owns its own video player (juce::VideoComponent) which
    // handles audio + video together. We bypass AudioEngine entirely for
    // these formats so the two pipelines don't double-up the audio.
    const bool isVideo = (ext == ".mp4" || ext == ".m4v" || ext == ".mov");

    if (isVideo)
    {
        // Stop any currently-playing audio song so we don't overlap.
        self->audioEngine->stop();

        if (self->lyricWindow_ == nullptr
            || ! self->lyricWindow_->loadVideo (audioFile))
        {
            DBG("loadAndPlaySong: video load failed: " + audioFile.getFullPathName());
            self->hideLoadingOverlay();
            return;
        }

        self->currentSong         = song;
        self->currentSongImageUrl = juce::String (song.imageUrl);
        self->currentSongDuration = self->lyricWindow_->getVideoDuration();

        if (self->topBar)
        {
            juce::String version;
            if (versionIndex >= 0 && versionIndex < (int) song.version.size())
                version = juce::String (song.version[(size_t) versionIndex]);

            self->topBar->setTrackInfo (juce::String (song.songName),
                                        juce::String (song.artistName),
                                        version);
            self->topBar->setMusicInfo (juce::String (song.keySignature),
                                        (int) std::round (song.tempo));

            if (self->currentSongImageUrl.isNotEmpty())
            {
                juce::Component::SafePointer<TopBar> safeTop (self->topBar.get());
                juce::String url = self->currentSongImageUrl;
                juce::Image img = ArtworkCache::getInstance().getOrFetch (url, [safeTop, url]() {
                    if (! safeTop) return;
                    auto cached = ArtworkCache::getInstance().getOrFetch (url, nullptr);
                    if (cached.isValid()) safeTop->setCoverArt (cached);
                });
                if (img.isValid())
                    self->topBar->setCoverArt (img);
            }
            else
            {
                self->topBar->setCoverArt ({});
            }
        }

        if (self->bottomBar)
        {
            self->bottomBar->setDurationSeconds (self->currentSongDuration);
            self->bottomBar->setProgress (0.0f);
            self->bottomBar->setPlaying (true);
            self->bottomBar->setPitch (pitchSemitones);
            self->bottomBar->setWaveformSamples ({}); // No waveform for video.
        }

        self->hideLoadingOverlay();
        return;
    }

    if (! self->audioEngine->loadSong(audioFile))
    {
        DBG("loadAndPlaySong: AudioEngine failed to load " + audioFile.getFullPathName());
        self->hideLoadingOverlay();
        return;
    }

    // Push the matching CDG file to the secondary lyric display. If no CDG
    // companion exists the window will simply fall back to its idle screen.
    if (self->lyricWindow_)
    {
        const auto cdgFile = self->resolveCdgFileFor (audioFile);
        self->lyricWindow_->loadCDG (cdgFile);
    }

    self->currentSong          = song;
    self->currentSongImageUrl  = juce::String(song.imageUrl);
    self->currentSongDuration  = self->audioEngine->getTotalLength();

    // Apply pitch (the dialog's semitone adjustment)
    self->audioEngine->setPitchShift((float) pitchSemitones);

    // TopBar — song info
    if (self->topBar)
    {
        juce::String version;
        if (versionIndex >= 0 && versionIndex < (int) song.version.size())
            version = juce::String(song.version[(size_t) versionIndex]);

        self->topBar->setTrackInfo(juce::String(song.songName),
                             juce::String(song.artistName),
                             version);

        int bpm = (int) std::round(song.tempo);
        self->topBar->setMusicInfo(juce::String(song.keySignature), bpm);

        // Artwork (async via ArtworkCache)
        if (self->currentSongImageUrl.isNotEmpty())
        {
            juce::Component::SafePointer<TopBar> safeTop(self->topBar.get());
            juce::String url = self->currentSongImageUrl;
            juce::Image img = ArtworkCache::getInstance().getOrFetch(url, [safeTop, url]() {
                if (! safeTop) return;
                auto cached = ArtworkCache::getInstance().getOrFetch(url, nullptr);
                if (cached.isValid()) safeTop->setCoverArt(cached);
            });
            if (img.isValid())
                self->topBar->setCoverArt(img);
        }
        else
        {
            self->topBar->setCoverArt({}); // reset
        }
    }

    // BottomBar — duration + waveform
    if (self->bottomBar)
    {
        self->bottomBar->setDurationSeconds(self->currentSongDuration);
        self->bottomBar->setProgress(0.0f);
        self->bottomBar->setPlaying(true);
        self->bottomBar->setPitch(pitchSemitones);

        // Build a real waveform asynchronously.
        juce::Component::SafePointer<BottomBar> safeBottom(self->bottomBar.get());
        WaveformGenerator::generateAsync(audioFile, 240, [safeBottom](std::vector<float> peaks) {
            if (safeBottom) safeBottom->setWaveformSamples(peaks);
        });
    }

    self->audioEngine->play();
    self->hideLoadingOverlay();
    });
}

//==============================================================================
void MainComponent::showLoadingOverlay(const juce::String& message)
{
    if (! loadingOverlay_)
    {
        loadingOverlay_ = std::make_unique<LoadingOverlay>();
        addAndMakeVisible(loadingOverlay_.get());
    }
    loadingOverlay_->setMessage(message);
    loadingOverlay_->setBounds(getLocalBounds());
    loadingOverlay_->toFront(false);
    loadingOverlay_->setVisible(true);
    loadingOverlay_->repaint();
}

void MainComponent::hideLoadingOverlay()
{
    if (loadingOverlay_)
        loadingOverlay_->setVisible(false);
}

//==============================================================================
juce::File MainComponent::resolveCdgFileFor (const juce::File& audioFile) const
{
    if (! audioFile.existsAsFile())
        return {};

    // If the audio file is itself a .cdg, that's our target.
    if (audioFile.getFileExtension().equalsIgnoreCase (".cdg"))
        return audioFile;

    // Common case: the audio file is an .mp3/.m4a and the CDG graphics sit
    // in a sibling file with the same stem. Probe a couple of common casings.
    for (const juce::String& ext : { ".cdg", ".CDG", ".Cdg" })
    {
        auto sibling = audioFile.withFileExtension (ext);
        if (sibling.existsAsFile())
            return sibling;
    }

    return {};
}

//==============================================================================
void MainComponent::installMenuBarModel (juce::MenuBarModel* model)
{
   #if JUCE_MAC
    juce::ignoreUnused (model);
    // macOS uses the system menu bar, managed directly by EncoreApplication
    // via juce::MenuBarModel::setMacMainMenu. Nothing to do here.
   #else
    if (model == nullptr)
    {
        menuBar_.reset();
    }
    else
    {
        if (menuBar_ == nullptr)
        {
            menuBar_ = std::make_unique<juce::MenuBarComponent> (model);
            addAndMakeVisible (menuBar_.get());
        }
        else
        {
            menuBar_->setModel (model);
        }
    }
    resized();
   #endif
}

//==============================================================================
void MainComponent::setVenueId (const juce::String& venueId, bool requestInitialScan)
{
    activeVenueId_ = venueId;

    if (venueId.isEmpty())
    {
        if (queueBar != nullptr)
            queueBar->setVenueInfo ("No Venue", "");
        if (lyricWindow_ != nullptr)
            if (auto* d = lyricWindow_->getDisplay())
                d->setVenueCode ({});
        return;
    }

    // Provisional state until the doc loads.
    if (queueBar != nullptr)
        queueBar->setVenueInfo ("Loading...", "");

    juce::Component::SafePointer<MainComponent> safe (this);

    VenueService::getInstance().loadVenue (venueId,
        [safe, requestInitialScan] (bool ok, VenueItem v, juce::String error)
        {
            if (safe == nullptr)
                return;

            if (! ok)
            {
                DBG ("[Venue] load failed: " << error);
                if (safe->queueBar != nullptr)
                    safe->queueBar->setVenueInfo ("Venue unavailable", "");
                return;
            }

            const juce::String name (v.name);
            const juce::String code (v.code);
            const juce::String logoUrl (v.logoUrl);

            if (safe->queueBar != nullptr)
                safe->queueBar->setVenueInfo (name, code);

            if (safe->lyricWindow_ != nullptr)
                if (auto* d = safe->lyricWindow_->getDisplay())
                    d->setVenueCode (code);

            // Push the full venue snapshot into the Settings page so the admin
            // can edit any field. Saves are routed through MainArea's
            // onVenueSettingsChanged callback wired in the constructor.
            if (safe->mainArea != nullptr)
                safe->mainArea->setVenueData (v);

            // Fetch the venue logo asynchronously and push it into the lyric
            // display once it arrives. ArtworkCache invokes the callback on
            // the message thread.
            if (logoUrl.isNotEmpty())
            {
                auto img = ArtworkCache::getInstance().getOrFetch (logoUrl,
                    [safe, logoUrl]
                    {
                        if (safe == nullptr) return;
                        auto loaded = ArtworkCache::getInstance().getOrFetch (logoUrl);
                        if (loaded.isValid() && safe->lyricWindow_ != nullptr)
                            if (auto* d = safe->lyricWindow_->getDisplay())
                                d->setVenueLogo (loaded);
                    });

                if (img.isValid() && safe->lyricWindow_ != nullptr)
                    if (auto* d = safe->lyricWindow_->getDisplay())
                        d->setVenueLogo (img);
            }

            // Venue switch path: if this load was triggered by the user
            // picking a different venue than the one configured on this PC,
            // navigate to the Library page and kick off the full song scan.
            if (requestInitialScan && safe->mainArea != nullptr)
                safe->mainArea->triggerInitialSongLoad();

            // Load the live queue for this venue from Firestore and push it
            // into the QueueBar (replaces the placeholder/empty state).
            const juce::String vid (v.id);
            QueueService::getInstance().loadQueue (vid,
                [safe] (bool qok, QueueService::Snapshot snap, juce::String qerr)
                {
                    if (safe == nullptr || safe->queueBar == nullptr)
                        return;

                    if (! qok)
                    {
                        DBG ("[Queue] load failed: " << qerr);
                        safe->queueBar->clearNowPlaying();
                        safe->queueBar->setSingers ({});
                        return;
                    }

                    if (snap.hasNowPlaying)
                        safe->queueBar->setNowPlaying (snap.nowPlaying);
                    else
                        safe->queueBar->clearNowPlaying();

                    safe->queueBar->setSingers (snap.singers);
                });
        });
}
