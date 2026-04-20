# Encore Karaoke CMakeLists.txt

cmake_minimum_required(VERSION 3.22)

project(EncoreJUCE VERSION 1.0.0)

# Add JUCE subdirectory (using complete JUCE framework)
# add_subdirectory(C:/JUCE-Full JUCE)
add_subdirectory(/Users/raymac/JUCE JUCE)

# Create the GUI application target
juce_add_gui_app(EncoreJUCE
    PRODUCT_NAME "Encore Karaoke"
    BUNDLE_ID "com.viracicom.encore")

# Generate JuceHeader.h
juce_generate_juce_header(EncoreJUCE)

# Add all source files
target_sources(EncoreJUCE
    PRIVATE
        Source/Main.cpp
        Source/Audio/AudioEngine.cpp
        Source/UI/MainComponent.cpp
        Source/UI/ResponsiveLayout.cpp
        Source/UI/TopBar.cpp
        Source/UI/BottomBar.cpp
        Source/UI/NavBar.cpp
        Source/UI/MainArea.cpp
        Source/UI/QueueBar.cpp
        Source/UI/HomePage.cpp
        Source/UI/SearchPage.cpp
        Source/Localization/LocalizationManager.cpp
        Source/Models/VenueItem.cpp
        Source/Models/QueueItem.cpp
        Source/Models/Singers.cpp
        Source/Models/Track.cpp
        Source/Models/CdgSong.cpp
        Source/Models/CdgGroup.cpp
        Source/Models/Host.cpp
        Source/Models/User.cpp
        Source/Models/Playing.cpp
        Source/Models/Lyric.cpp
        Source/Models/License.cpp
        Source/Models/Playlist.cpp
        Source/Models/Member.cpp
        Source/Models/Audit.cpp
        Source/Models/Ad.cpp
        Source/Models/Preset.cpp
        Source/Models/VenueUser.cpp
        Source/Models/Location.cpp)

# Set compile definitions
target_compile_definitions(EncoreJUCE
    PRIVATE
        JUCE_STRICT_REFCOUNTEDPOINTER=1
        JUCE_USE_CURL=1
        JUCE_WEB_BROWSER=0
        JUCE_USE_CAMERA=0
        JUCE_DISPLAY_SPLASH_SCREEN=1)

# Link JUCE modules
target_link_libraries(EncoreJUCE
    PRIVATE
        juce::juce_audio_basics
        juce::juce_audio_devices
        juce::juce_audio_formats
        juce::juce_audio_processors
        juce::juce_audio_utils
        juce::juce_core
        juce::juce_data_structures
        juce::juce_events
        juce::juce_graphics
        juce::juce_gui_basics
        juce::juce_gui_extra
        juce::juce_opengl
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags)

# Copy assets to output directory
add_custom_command(TARGET EncoreJUCE POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/assets
        $<TARGET_FILE_DIR:EncoreJUCE>/assets
    COMMENT "Copying assets to output directory")

# Set assets path for the application to find resources
target_compile_definitions(EncoreJUCE
    PRIVATE
        ASSETS_PATH="./assets/")