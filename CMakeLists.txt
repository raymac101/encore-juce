# Encore Karaoke CMakeLists.txt

cmake_minimum_required(VERSION 3.22)

project(EncoreJUCE VERSION 1.0.0)

set(ENCORE_BUILD_NUMBER_FILE "${CMAKE_SOURCE_DIR}/build_number.txt")
set(ENCORE_BUILD_INFO_HEADER "${CMAKE_BINARY_DIR}/generated/BuildInfo.h")

# Add JUCE subdirectory (using complete JUCE framework)
if(WIN32)
    set(JUCE_SOURCE_DIR "C:/JUCE-Full")
else()
    set(JUCE_SOURCE_DIR "/Users/raymac/JUCE")
endif()

if(NOT EXISTS "${JUCE_SOURCE_DIR}")
    message(FATAL_ERROR "JUCE source directory not found at ${JUCE_SOURCE_DIR}. Set JUCE_SOURCE_DIR in CMakeLists.txt to your local JUCE path.")
endif()

add_subdirectory(${JUCE_SOURCE_DIR} JUCE)

# Create the GUI application target
juce_add_gui_app(EncoreJUCE
    PRODUCT_NAME "Encore Karaoke"
    BUNDLE_ID "com.viracicom.encore"
    ICON_BIG   "${CMAKE_CURRENT_SOURCE_DIR}/assets/images/Encore-Icon.png"
    ICON_SMALL "${CMAKE_CURRENT_SOURCE_DIR}/assets/images/Encore-Icon.png")

add_custom_target(EncoreBuildNumber ALL
    COMMAND ${CMAKE_COMMAND}
        -DBUILD_NUMBER_FILE=${ENCORE_BUILD_NUMBER_FILE}
        -DOUTPUT_HEADER=${ENCORE_BUILD_INFO_HEADER}
        -DPROJECT_VERSION=${PROJECT_VERSION}
        -P ${CMAKE_SOURCE_DIR}/cmake/IncrementBuildNumber.cmake
    COMMENT "Incrementing Encore build number")

add_dependencies(EncoreJUCE EncoreBuildNumber)

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
        Source/UI/RibbonMenu.cpp
        Source/UI/MixerPage.cpp
        Source/UI/TestingPage.cpp
        Source/UI/QueueBar.cpp
        Source/UI/EditSingerModal.cpp
        Source/UI/HomePage.cpp
        Source/UI/SearchPage.cpp
        Source/UI/LibraryPage.cpp
        Source/UI/SettingsPage.cpp
        Source/UI/CompanyAdminPage.cpp
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
        Source/Services/BackgroundMusicPlayer.cpp
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
    # Allow Windows builds to discover vcpkg-installed libraries even when
    # VCPKG_ROOT is not defined in the environment.
    set(VCPKG_ROOT_HINTS "$ENV{VCPKG_ROOT}" "D:/vcpkg" "C:/vcpkg")
    set(VCPKG_INCLUDE_HINTS)
    set(VCPKG_LIBRARY_HINTS)
    foreach(_vcpkg_root IN LISTS VCPKG_ROOT_HINTS)
        if(_vcpkg_root)
            list(APPEND VCPKG_INCLUDE_HINTS
                "${_vcpkg_root}/installed/x64-windows/include"
                "${_vcpkg_root}/installed/x64-windows-static/include")
            list(APPEND VCPKG_LIBRARY_HINTS
                "${_vcpkg_root}/installed/x64-windows/lib"
                "${_vcpkg_root}/installed/x64-windows/debug/lib"
                "${_vcpkg_root}/installed/x64-windows-static/lib")
        endif()
    endforeach()

    if(APPLE)
        execute_process(COMMAND brew --prefix rubberband
                        OUTPUT_VARIABLE RUBBERBAND_BREW_PREFIX
                        OUTPUT_STRIP_TRAILING_WHITESPACE
                        ERROR_QUIET)
    endif()
    find_path(RUBBERBAND_INCLUDE_DIR rubberband/RubberBandStretcher.h
              HINTS ${RUBBERBAND_BREW_PREFIX}/include
                    ${VCPKG_INCLUDE_HINTS})
    find_library(RUBBERBAND_LIBRARY NAMES rubberband
                 HINTS ${RUBBERBAND_BREW_PREFIX}/lib
                       ${VCPKG_LIBRARY_HINTS})
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

target_include_directories(EncoreJUCE PRIVATE ${CMAKE_BINARY_DIR}/generated)

# SQLite3 — used by SongDatabase. macOS ships with system sqlite3.
find_package(SQLite3 QUIET)
if(NOT SQLite3_FOUND)
    set(SQLITE_VCPKG_ROOT_HINTS "$ENV{VCPKG_ROOT}" "D:/vcpkg" "C:/vcpkg")
    set(SQLITE_VCPKG_INCLUDE_HINTS)
    set(SQLITE_VCPKG_LIBRARY_HINTS)
    foreach(_sqlite_vcpkg_root IN LISTS SQLITE_VCPKG_ROOT_HINTS)
        if(_sqlite_vcpkg_root)
            list(APPEND SQLITE_VCPKG_INCLUDE_HINTS
                "${_sqlite_vcpkg_root}/installed/x64-windows/include"
                "${_sqlite_vcpkg_root}/installed/x64-windows-static/include")
            list(APPEND SQLITE_VCPKG_LIBRARY_HINTS
                "${_sqlite_vcpkg_root}/installed/x64-windows/lib"
                "${_sqlite_vcpkg_root}/installed/x64-windows/debug/lib"
                "${_sqlite_vcpkg_root}/installed/x64-windows-static/lib")
        endif()
    endforeach()

    find_path(SQLite3_INCLUDE_DIR sqlite3.h HINTS ${SQLITE_VCPKG_INCLUDE_HINTS})
    find_library(SQLite3_LIBRARY NAMES sqlite3 HINTS ${SQLITE_VCPKG_LIBRARY_HINTS})

    if(SQLite3_INCLUDE_DIR AND SQLite3_LIBRARY)
        add_library(SQLite3::SQLite3 UNKNOWN IMPORTED)
        set_target_properties(SQLite3::SQLite3 PROPERTIES
            IMPORTED_LOCATION "${SQLite3_LIBRARY}"
            INTERFACE_INCLUDE_DIRECTORIES "${SQLite3_INCLUDE_DIR}")
    else()
        message(FATAL_ERROR "SQLite3 not found. Install with your package manager or `vcpkg install sqlite3` (Windows).")
    endif()
endif()
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