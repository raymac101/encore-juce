# Encore Karaoke CMakeLists.txt

cmake_minimum_required(VERSION 3.22)

# --- Version: 1.1.<build number> --------------------------------------------
# The build number is the PATCH component of the app's real version, not a
# 4th field bolted on afterward -- so ProjectInfo::versionString (which
# UpdateService.cpp compares against the update manifest), the macOS bundle
# version, and the Windows exe version resource all agree on one number.
# This must happen BEFORE project(), since CMake bakes VERSION in at
# configure time. Read the CURRENT value (already incremented by the
# previous configure) and use it as-is for THIS build; write the next value
# back immediately so the next configure gets a fresh one. This is
# deliberately configure-time-only, not build-time -- a compile-time
# constant like ProjectInfo::versionString can't change between two
# `cmake --build` runs of the same configure anyway, and a release's
# reported version shouldn't drift just from recompiling unchanged code.
set(ENCORE_VERSION_MAJOR_MINOR "1.1")
set(ENCORE_BUILD_NUMBER_FILE "${CMAKE_SOURCE_DIR}/build_number.txt")

if(EXISTS "${ENCORE_BUILD_NUMBER_FILE}")
    file(READ "${ENCORE_BUILD_NUMBER_FILE}" _encore_build_number_raw)
    string(STRIP "${_encore_build_number_raw}" _encore_build_number_raw)
else()
    set(_encore_build_number_raw "1")
endif()
if(NOT _encore_build_number_raw MATCHES "^[0-9]+$")
    set(_encore_build_number_raw "1")
endif()
set(ENCORE_BUILD_NUMBER "${_encore_build_number_raw}")

math(EXPR _encore_next_build_number "${ENCORE_BUILD_NUMBER} + 1")
file(WRITE "${ENCORE_BUILD_NUMBER_FILE}" "${_encore_next_build_number}\n")

project(EncoreJUCE VERSION ${ENCORE_VERSION_MAJOR_MINOR}.${ENCORE_BUILD_NUMBER})

set(ENCORE_BUILD_INFO_HEADER "${CMAKE_BINARY_DIR}/generated/BuildInfo.h")
get_filename_component(_encore_build_info_dir "${ENCORE_BUILD_INFO_HEADER}" DIRECTORY)
file(MAKE_DIRECTORY "${_encore_build_info_dir}")
file(WRITE "${ENCORE_BUILD_INFO_HEADER}"
    "#pragma once\n\n#define ENCORE_BUILD_NUMBER ${ENCORE_BUILD_NUMBER}\n#define ENCORE_VERSION_WITH_BUILD \"${PROJECT_VERSION}\"\n")
message(STATUS "Encore version: ${PROJECT_VERSION}")

# Add JUCE subdirectory (using complete JUCE framework)
# The default paths match the standard install locations per platform.
# Override on the command line:  cmake -B build -DJUCE_SOURCE_DIR=/your/path
# or via CMakePresets.json cacheVariables (the platform presets do this).
if(WIN32)
    set(_juce_default "C:/JUCE-Full")
else()
    set(_juce_default "/Users/raymac/JUCE")
endif()
set(JUCE_SOURCE_DIR "${_juce_default}" CACHE PATH "Path to the JUCE source directory")

if(NOT EXISTS "${JUCE_SOURCE_DIR}")
    message(FATAL_ERROR "JUCE source directory not found at ${JUCE_SOURCE_DIR}.\n"
        "Override with: cmake -B build -DJUCE_SOURCE_DIR=/path/to/JUCE\n"
        "or set it in CMakePresets.json cacheVariables.")
endif()

add_subdirectory(${JUCE_SOURCE_DIR} JUCE)

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
        Source/Audio/ChannelPluginChain.cpp
        Source/UI/MainComponent.cpp
        Source/UI/ResponsiveLayout.cpp
        Source/UI/TopBar.cpp
        Source/UI/BottomBar.cpp
        Source/UI/NavBar.cpp
        Source/UI/MainArea.cpp
        Source/UI/MenuTheme.cpp
        Source/UI/RibbonMenu.cpp
        Source/UI/SpriteIcon.cpp
        Source/UI/MixerPage.cpp
        Source/UI/TestingPage.cpp
        Source/UI/QueueBar.cpp
        Source/UI/EditSingerModal.cpp
        Source/UI/HomePage.cpp
        Source/UI/SearchPage.cpp
        Source/UI/LibraryPage.cpp
        Source/UI/ChartsPage.cpp
        Source/UI/SettingsPage.cpp
        Source/UI/CompanyAdminPage.cpp
        Source/UI/CustomerAdminPage.cpp
        Source/UI/ProfilePage.cpp
        Source/UI/SongSelectionDialog.cpp
        Source/UI/SongEditDialog.cpp
        Source/UI/AddSongsDialog.cpp
        Source/UI/LyricDisplayComponent.cpp
        Source/UI/LyricDisplayWindow.cpp
        Source/UI/LoginWindow.cpp
        Source/UI/Onboarding/OnboardingWizard.cpp
        Source/CDG/CDGDecoder.cpp
        Source/Services/LibraryScanner.cpp
        Source/Services/SongDatabase.cpp
        Source/Services/ImageCache.cpp
        Source/Services/UserPreferences.cpp
        Source/Services/WaveformGenerator.cpp
        Source/Services/FirestoreClient.cpp
        Source/Services/SongbookStorageService.cpp
        Source/Services/VenueSessionService.cpp
        Source/Services/VenueService.cpp
        Source/Services/QueueService.cpp
        Source/Services/QueueRotation.cpp
        Source/Services/RequestService.cpp
        Source/Services/EmojiService.cpp
        Source/Services/ArchiveService.cpp
        Source/Services/AnalyticsService.cpp
        Source/Services/AuditService.cpp
        Source/Services/BackgroundMusicPlayer.cpp
        Source/Services/PluginHostService.cpp
        Source/Services/UpdateService.cpp
        Source/Services/CustomerAdminService.cpp
        Source/Services/ApiService.cpp
        Source/Services/GlobalProgressService.cpp
        Source/Services/CompanyService.cpp
        Source/Services/LicenseService.cpp
        Source/Services/InvitationService.cpp
        Source/Auth/LoginFlowController.cpp
        Source/Localization/LocalizationManager.cpp
        Source/Models/VenueItem.cpp
        Source/Models/QueueItem.cpp
        Source/Models/Singers.cpp
        Source/Models/Track.cpp
        Source/Models/CdgSong.cpp
        Source/Models/CdgGroup.cpp
        Source/Models/Host.cpp
        Source/Models/Company.cpp
        Source/Models/User.cpp
        Source/Models/Playing.cpp
        Source/Models/Lyric.cpp
        Source/Models/License.cpp
        Source/Models/Playlist.cpp
        Source/Models/Member.cpp
        Source/Models/Audit.cpp
        Source/Models/Ad.cpp
        Source/Models/Preset.cpp
        Source/Models/Location.cpp)

# Set compile definitions
target_compile_definitions(EncoreJUCE
    PRIVATE
        JUCE_STRICT_REFCOUNTEDPOINTER=1
        JUCE_USE_CURL=1
        JUCE_WEB_BROWSER=0
        JUCE_USE_CAMERA=0
        JUCE_DISPLAY_SPLASH_SCREEN=1
        # VST3 plugin hosting (Source/Audio/ChannelPluginChain, PluginHostService).
        JUCE_PLUGINHOST_VST3=1)
        # The "Skip Login (Dev)" button (Source/UI/LoginWindow.cpp) used to be
        # gated by an ENCORE_DEV_SKIP_LOGIN=1 compile define here -- meaning it
        # shipped in every build, including the production installer, with a
        # real hardcoded password. It's now a runtime opt-in instead: launch
        # with --dev-skip-login to reveal it (see isDevSkipLoginEnabled() in
        # that file). Nothing to set here anymore.

# Link JUCE modules
target_link_libraries(EncoreJUCE
    PRIVATE
        juce::juce_audio_basics
        juce::juce_audio_devices
        juce::juce_audio_formats
        juce::juce_audio_processors
        juce::juce_audio_utils
        juce::juce_core
        juce::juce_cryptography
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

# Embed librubberband into the app bundle and repoint the executable at it
# via @rpath, instead of leaving it linked against the absolute Homebrew
# path found above. A hardened-runtime-signed build (required for
# notarization, see packaging/macos/build_dmg.sh) enforces library
# validation and refuses to load a dylib not signed with this project's
# own Team ID -- Homebrew's rubberband build isn't, so without this the
# signed/notarized app aborts at launch on every machine, including the
# one it was built on. See packaging/macos/embed_rubberband.sh.
if(APPLE)
    add_custom_command(TARGET EncoreJUCE POST_BUILD
        COMMAND ${CMAKE_SOURCE_DIR}/packaging/macos/embed_rubberband.sh
            $<TARGET_FILE:EncoreJUCE>
            $<TARGET_BUNDLE_CONTENT_DIR:EncoreJUCE>/Frameworks
        COMMENT "Embedding librubberband into the app bundle for distribution")
endif()

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

# Copy assets to output directory. On macOS the real files must live under
# Contents/Resources/ (standard bundle layout) rather than next to the
# executable in Contents/MacOS/ -- newer codesign deep-verifies loose,
# non-executable files sitting alongside the binary as if they were
# subcomponents needing their own signature, which a plain resource file
# can never satisfy, and the whole bundle fails to sign. A compatibility
# symlink at the old Contents/MacOS/assets location keeps every existing
# `currentExecutableFile`-relative asset lookup in Source/ working unchanged.
if(APPLE)
    add_custom_command(TARGET EncoreJUCE POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_SOURCE_DIR}/assets
            $<TARGET_BUNDLE_CONTENT_DIR:EncoreJUCE>/Resources/assets
        COMMAND ${CMAKE_COMMAND} -E rm -rf $<TARGET_FILE_DIR:EncoreJUCE>/assets
        COMMAND ${CMAKE_COMMAND} -E create_symlink
            ../Resources/assets
            $<TARGET_FILE_DIR:EncoreJUCE>/assets
        COMMENT "Copying assets into app bundle Resources (with MacOS/ compatibility symlink)")
else()
    add_custom_command(TARGET EncoreJUCE POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_SOURCE_DIR}/assets
            $<TARGET_FILE_DIR:EncoreJUCE>/assets
        COMMENT "Copying assets to output directory")
endif()

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