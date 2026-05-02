# Encore Karaoke CMakeLists.txt

cmake_minimum_required(VERSION 3.22)

project(EncoreJUCE VERSION 1.0.0)

# Add JUCE subdirectory (using complete JUCE framework)
# add_subdirectory(C:/JUCE-Full JUCE)
add_subdirectory(/Users/raymac/JUCE JUCE)

# Create the GUI application target
juce_add_gui_app(EncoreJUCE
    PRODUCT_NAME "Encore Karaoke"
    BUNDLE_ID "com.viracicom.encore"
    ICON_BIG   "${CMAKE_CURRENT_SOURCE_DIR}/assets/images/Encore-Icon.png"
    ICON_SMALL "${CMAKE_CURRENT_SOURCE_DIR}/assets/images/Encore-Icon.png")

# Generate JuceHeader.h
juce_generate_juce_header(EncoreJUCE)

# Add all source files
target_sources(EncoreJUCE
    PRIVATE
        Source/Main.cpp
        Source/Audio/AudioEngine.cpp
        Source/Audio/PitchShifter.cpp
        Source/UI/MainComponent.cpp
        Source/UI/ResponsiveLayout.cpp
        Source/UI/TopBar.cpp
        Source/UI/BottomBar.cpp
        Source/UI/NavBar.cpp
        Source/UI/MainArea.cpp
        Source/UI/QueueBar.cpp
        Source/UI/EditSingerModal.cpp
        Source/UI/HomePage.cpp
        Source/UI/SearchPage.cpp
        Source/UI/LibraryPage.cpp
        Source/UI/SettingsPage.cpp
        Source/UI/SongSelectionDialog.cpp
        Source/UI/SongEditDialog.cpp
        Source/UI/AddSongsDialog.cpp
        Source/UI/LyricDisplayComponent.cpp
        Source/UI/LyricDisplayWindow.cpp
        Source/UI/LoginWindow.cpp
        Source/CDG/CDGDecoder.cpp
        Source/Services/LibraryScanner.cpp
        Source/Services/SongDatabase.cpp
        Source/Services/ImageCache.cpp
        Source/Services/UserPreferences.cpp
        Source/Services/WaveformGenerator.cpp
        Source/Services/FirestoreClient.cpp
        Source/Services/VenueService.cpp
        Source/Services/QueueService.cpp
        Source/Services/RequestService.cpp
        Source/Services/ArchiveService.cpp
        Source/Services/ApiService.cpp
        Source/Auth/LoginFlowController.cpp
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
        JUCE_DISPLAY_SPLASH_SCREEN=1
        # Dev-only: shows a "Skip Login (Dev)" button on the login page that
        # auto-signs in as raymac@shaw.ca and selects the "Karaoke Palace"
        # venue. Set to 0 (or delete this line) for production builds.
        ENCORE_DEV_SKIP_LOGIN=1)

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
        juce::juce_video
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags)

# Rubber Band Library — required by Source/Audio/PitchShifter for real-time
# pitch shifting. On macOS users install via `brew install rubberband`; on
# Windows use `vcpkg install rubberband`. We prefer pkg-config when
# available, otherwise fall back to find_path / find_library so the build
# still works on machines without pkg-config installed.
find_package(PkgConfig QUIET)
if(PkgConfig_FOUND)
    pkg_check_modules(RUBBERBAND rubberband)
endif()
if(NOT RUBBERBAND_FOUND)
    if(APPLE)
        execute_process(COMMAND brew --prefix rubberband
                        OUTPUT_VARIABLE RUBBERBAND_BREW_PREFIX
                        OUTPUT_STRIP_TRAILING_WHITESPACE
                        ERROR_QUIET)
    endif()
    find_path(RUBBERBAND_INCLUDE_DIR rubberband/RubberBandStretcher.h
              HINTS ${RUBBERBAND_BREW_PREFIX}/include)
    find_library(RUBBERBAND_LIBRARY NAMES rubberband
                 HINTS ${RUBBERBAND_BREW_PREFIX}/lib)
    if(NOT RUBBERBAND_INCLUDE_DIR OR NOT RUBBERBAND_LIBRARY)
        message(FATAL_ERROR "Rubber Band Library not found. Install with `brew install rubberband` (macOS) or `vcpkg install rubberband` (Windows).")
    endif()
    set(RUBBERBAND_INCLUDE_DIRS ${RUBBERBAND_INCLUDE_DIR})
    set(RUBBERBAND_LIBRARIES    ${RUBBERBAND_LIBRARY})
endif()
target_include_directories(EncoreJUCE PRIVATE ${RUBBERBAND_INCLUDE_DIRS})
if(RUBBERBAND_LIBRARY_DIRS)
    target_link_directories(EncoreJUCE PRIVATE ${RUBBERBAND_LIBRARY_DIRS})
endif()
target_link_libraries(EncoreJUCE      PRIVATE ${RUBBERBAND_LIBRARIES})

# SQLite3 — used by SongDatabase. macOS ships with system sqlite3.
find_package(SQLite3 REQUIRED)
target_link_libraries(EncoreJUCE PRIVATE SQLite3::SQLite3)

# Copy assets to output directory
add_custom_command(TARGET EncoreJUCE POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/assets
        $<TARGET_FILE_DIR:EncoreJUCE>/assets
    COMMENT "Copying assets to output directory")

# Copy localisation files. On macOS they go inside the .app bundle's
# Contents/Resources/Languages/ (standard bundle layout). On other platforms
# they sit alongside the executable in Resources/Languages/.
if(APPLE)
    add_custom_command(TARGET EncoreJUCE POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_SOURCE_DIR}/Resources/Languages
            $<TARGET_BUNDLE_CONTENT_DIR:EncoreJUCE>/Resources/Languages
        COMMENT "Copying language files into app bundle Resources")
else()
    add_custom_command(TARGET EncoreJUCE POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_SOURCE_DIR}/Resources/Languages
            $<TARGET_FILE_DIR:EncoreJUCE>/Resources/Languages
        COMMENT "Copying language files next to executable")
endif()

# Set assets path for the application to find resources
target_compile_definitions(EncoreJUCE
    PRIVATE
        ASSETS_PATH="./assets/")