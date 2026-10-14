# RMT Application Sources

# Platform and dependency detection
message(STATUS "Configuring RMT dependencies...")

# Bitmaps of res/ compiled into the program (CompatTypes.h CBitmap)
include(${PROJECT_SOURCE_DIR}/cmake/EmbedResources.cmake)
set(RMT_EMBEDDED_RESOURCES ${CMAKE_CURRENT_BINARY_DIR}/RmtEmbeddedResources.cpp)
rmt_embed_resources(${RMT_EMBEDDED_RESOURCES}
    IDB_GFX             ${CMAKE_CURRENT_SOURCE_DIR}/res/asciagr.bmp
    IDR_MAINFRAME       ${CMAKE_CURRENT_SOURCE_DIR}/res/Toolbar.bmp
    IDR_TOOLBARPLAY     ${CMAKE_CURRENT_SOURCE_DIR}/res/toolbarp.bmp
    IDR_TOOLBARBLOCK    ${CMAKE_CURRENT_SOURCE_DIR}/res/toolbar1.bmp
    IDR_TOOLBARCHANNELS ${CMAKE_CURRENT_SOURCE_DIR}/res/bmp00001.bmp
)

# The prompts of Rmt.rc's string tables, for the script command "dump actions"
include(${PROJECT_SOURCE_DIR}/cmake/GenerateRcTables.cmake)
set(RMT_PROMPTS ${CMAKE_CURRENT_BINARY_DIR}/RmtPrompts.cpp ${CMAKE_CURRENT_BINARY_DIR}/RmtMenuTables.cpp)
rmt_generate_prompts(${CMAKE_CURRENT_BINARY_DIR}/RmtPrompts.cpp ${CMAKE_CURRENT_SOURCE_DIR}/Rmt.rc ${CMAKE_CURRENT_SOURCE_DIR}/resource.h)
rmt_generate_menus(${CMAKE_CURRENT_BINARY_DIR}/RmtMenuTables.cpp ${CMAKE_CURRENT_SOURCE_DIR}/Rmt.rc)

# Check for Windows audio libraries
if(WIN32)
    find_library(DSOUND_LIB NAMES dsound QUIET)
    find_library(WINMM_LIB NAMES winmm QUIET)
    
    if(DSOUND_LIB)
        message(STATUS "Found DirectSound: ${DSOUND_LIB}")
    else()
        message(WARNING "DirectSound not found - may affect audio")
    endif()
    
    if(WINMM_LIB)
        message(STATUS "Found WinMM: ${WINMM_LIB}")
    else()
        message(WARNING "WinMM not found - may affect MIDI")
    endif()
endif()

# Option to build test/core only (for Phase 1 cross-platform testing)
option(RMT_BUILD_CORE_ONLY "Build only core components without UI (for testing)" OFF)
if(RMT_BUILD_CORE_ONLY)
    message(STATUS "Building core components only (no UI)")
    
    set(RMT_SOURCES
        # Audio Backend (Phase 2) - cross-platform
        AudioBackend.cpp
        DirectSoundAudio.cpp
        PortAudioBackend.cpp
        
        # MIDI Backend (Phase 2) - cross-platform
        MidiBackend.cpp
        WindowsMidiBackend.cpp
        RtMidiBackend.cpp
        
        # Test main for audio/MIDI backends
        main-portaudio.cpp
    )
    
    set(RMT_HEADERS
        # Audio Backend (Phase 2)
        AudioBackend.h
        DirectSoundAudio.h
        PortAudioBackend.h
        
        # MIDI Backend (Phase 2)
        MidiBackend.h
        WindowsMidiBackend.h
        RtMidiBackend.h
    )
else()
    # Full build with UI.
    #
    # Sources are split by role:
    #   - RMT_CORE_SOURCES:       the engine, no GUI dependency at all.
    #   - RMT_GUI_SHARED_SOURCES: the rendering code that draws on
    #                             g_mem_dc/g_gfx_dc (CompatDC*, see CompatTypes.h).
    #   - RMT_GUI_QT_SOURCES:     the Qt frontend and the tracker view/document.
    set(RMT_CORE_SOURCES
        # Audio Backend (Phase 2) - cross-platform
        AudioBackend.cpp
        DirectSoundAudio.cpp
        PortAudioBackend.cpp

        # MIDI Backend (Phase 2) - cross-platform
        MidiBackend.cpp
        WindowsMidiBackend.cpp
        RtMidiBackend.cpp

        # Engine
        emu/Cpu6502.cpp
        emu/PokeySound.cpp
        ASMFileBuilder.cpp
        ASMFile.cpp
        Accelerators.cpp
        ActionTable.cpp
        ASMFileExporter.cpp
        AssemblerTypes.cpp
        AtariBinaries.cpp
        Atari.cpp
        AtariIO.cpp
        AtariTrackerDriver.cpp
        C6502.cpp
        Clipboard.cpp
        Fraction.cpp
        Instruments.cpp
        IOHelpers.cpp
        IO_Importer.cpp
        IO_Instruments.cpp
        IO_Song.cpp
        IO_Song_ExportAsm.cpp
        IO_Tracks.cpp
        Keyboard2NoteMapping.cpp
        LZSSFile.cpp
        lzss_sap.cpp
        Midi_Song.cpp
        Notes.cpp
        Pokey.cpp
        PokeyController.cpp
        PokeyRenderer.cpp
        PokeyStream.cpp
        RuntimeException.cpp
        Script.cpp
        ScriptMessages.cpp
        ScriptRunner.cpp
        SAPFile.cpp
        SAPFileExporter.cpp
        Shell.cpp
        SongContainer.cpp
        Song.cpp
        Song_DumpSong.cpp
        SongExport.cpp
        SongExporter.cpp
        SongExporterTest.cpp
        SongTimer.cpp
        Tracks.cpp
        Tuning.cpp
        TuningTypes.cpp
        Undo.cpp
        VUPlayer.cpp
        WaveFile.cpp
        WaveFileExporter.cpp
    )

    set(RMT_GUI_SHARED_SOURCES
        ChannelControl.cpp
        GuiHelpers.cpp
        GUI_Instruments.cpp
        GUI_Song.cpp
        TracksControl.cpp
        Global.cpp
    )

    # The Qt frontend: the tracker view/document (RmtView.cpp, RmtDoc.cpp)
    # compiled against CompatTypes.h, hosted by a Qt main window (qt/).
    set(RMT_GUI_QT_SOURCES
        qt/main-qt.cpp
        qt/RmtQtFrontend.cpp
        qt/RmtQtFrontend.h
        qt/RmtQtKeys.cpp
        qt/RmtQtDialogs.cpp
        qt/RmtQtSettings.cpp
        qt/ritmo.qrc
        qt/RmtQtScreen.h
        qt/RmtQtSettings.h
        qt/RmtQtDialogs.h
        qt/QtMainFrame.cpp
        RmtView.cpp
        RmtDoc.cpp
    )

    set(RMT_SOURCES ${RMT_CORE_SOURCES} ${RMT_GUI_SHARED_SOURCES} ${RMT_GUI_QT_SOURCES} ${RMT_PROMPTS}
        CompatTypes.cpp CompatAudio.cpp CompatDialogs.cpp RmtMidiRt.cpp ${RMT_EMBEDDED_RESOURCES})

set(RMT_HEADERS
    # Audio Backend (Phase 2)
    AudioBackend.h
    DirectSoundAudio.h
    PortAudioBackend.h
    
    # MIDI Backend (Phase 2)
    MidiBackend.h
    WindowsMidiBackend.h
    RtMidiBackend.h
    
    ASMFileBuilder.h
    ASMFile.h
    ASMFileExporter.h
    AssemblerTypes.h
    AtariBinaries.h
    Atari.h
    AtariIO.h
    AtariTrackerDriver.h
    C6502.h
    ChannelControl.h
    Clipboard.h
    OptionsDialog.h
    EffectsDlg.h
    ExportDlgs.h
    FileNewDlg.h
    Fraction.h
    General.h
    Global.h
    GuiHelpers.h
    InstrumentTypes.h
    Instruments.h
    IOHelpers.h
    IO_Importer.h
    IO_Instruments.h
    IO_Song.h
    IO_Song_ExportAsm.h
    IO_Tracks.h
    Keyboard2NoteMapping.h
    LZSSFile.h
    lzssp.h
    lzss_sap.h
    MainFrm.h
    Memory.h
    Notes.h
    Pokey.h
    PokeyController.h
    PokeyRenderer.h
    PokeyStream.h
    Resource.h
    RmtDoc.h
    RmtFile.h
    RmtMidi.h
    RmtView.h
    rmt_xex.h
    RuntimeException.h
    Script.h
    ScriptMessages.h
    ScriptRunner.h
    SAPFile.h
    SAPFileExportDialog.h
    SAPFileExporter.h
    Shell.h
    SongContainer.h
    Song.h
    Song_DumpSong.h
    SongExport.h
    SongExporter.h
    SongExporterTest.h
    SongTimer.h
    SongTypes.h
    StdAfx.h
    TrackerDriverVersion.h
    tracker_obx.h
    TracksControl.h
    Tracks.h
    TracksTypes.h
    TrackTypes.h
    Tuning.h
    TuningDialog.h
    TuningTypes.h
    Undo.h
    VUPlayer.h
    WaveFile.h
    WaveFileExporter.h
)
endif()

# Platform-specific preprocessor definitions
set(RMT_DEFINES
    NO_CONSOL_SOUND
    NO_VOL_ONLY
    STEREO
)

if(WIN32)
    list(APPEND RMT_DEFINES
        _WINDOWS
        WIN32
        _AFXDLL
    )
endif()

# Search for optional libraries BEFORE creating executable
# PortAudio cross-platform audio library
find_package(PortAudio QUIET)
# A CMake package of PortAudio (MSYS2, vcpkg) gives an imported target
# and no PortAudio_LIBRARIES: link what there is
set(RMT_PORTAUDIO_LINK ${PortAudio_LIBRARIES})
foreach(_pa PortAudio::portaudio PortAudio::PortAudio portaudio)
    if(TARGET ${_pa})
        set(RMT_PORTAUDIO_LINK ${_pa})
        break()
    endif()
endforeach()
if(NOT PortAudio_FOUND)
    find_library(PORTAUDIO_LIB portaudio QUIET)
    # its header may not be in a default include folder (Homebrew, MSYS2)
    find_path(PORTAUDIO_INCLUDE portaudio.h QUIET)
endif()

# RtMidi cross-platform MIDI library: MIDI IN
# (ALSA on Linux, WinMM on Windows, CoreMIDI on macOS; RmtMidiRt.cpp)
# Try pkg-config first; not when cross-compiling, it would find the
# library of the host
if(NOT CMAKE_CROSSCOMPILING)
    find_package(PkgConfig QUIET)
    if(PkgConfig_FOUND)
        pkg_check_modules(RTMIDI QUIET rtmidi)
    endif()
endif()

# If pkg-config didn't find it, try manual search
if(NOT RTMIDI_FOUND)
    find_library(RTMIDI_LIB rtmidi QUIET)
    find_path(RTMIDI_INCLUDE RtMidi.h PATH_SUFFIXES rtmidi QUIET)
endif()

# Windows without PortAudio headers (e.g. MinGW cross-compile): drop the
# PortAudio backend, AudioBackendFactory falls back to DirectSound
set(RMT_NO_PORTAUDIO FALSE)
if(WIN32 AND NOT PortAudio_FOUND AND NOT PORTAUDIO_LIB)
    find_path(PORTAUDIO_INCLUDE portaudio.h)
    if(NOT PORTAUDIO_INCLUDE)
        set(RMT_NO_PORTAUDIO TRUE)
        list(REMOVE_ITEM RMT_SOURCES PortAudioBackend.cpp)
        list(APPEND RMT_DEFINES RMT_NO_PORTAUDIO)
        message(STATUS "PortAudio not found: Windows build uses DirectSound only")
    endif()
endif()

# Qt native GUI frontend: Qt6
if(NOT RMT_BUILD_CORE_ONLY)
    find_package(Qt6 REQUIRED COMPONENTS Core Widgets PrintSupport)
    message(STATUS "Qt frontend: Qt ${Qt6_VERSION}")
endif()

# std::thread (song timer of CompatAudio.cpp, backends)
find_package(Threads REQUIRED)

# The icon of Ritmo.exe: a resource file, compiled by windres with MinGW
if(WIN32 AND NOT RMT_BUILD_CORE_ONLY)
    enable_language(RC)
    list(APPEND RMT_SOURCES ritmo-icon.rc)
endif()

# Create executable
add_executable(Rmt WIN32 ${RMT_SOURCES})
target_link_libraries(Rmt PRIVATE Threads::Threads)
# dlopen (libasound, to see where the default device of ALSA goes: CompatAudio.cpp)
target_link_libraries(Rmt PRIVATE ${CMAKE_DL_LIBS})

# Set target properties
# VERSION/SOVERSION on an executable only make a versioned file plus a
# symlink (Rmt.exe-1.35.0 -> Rmt.exe): not wanted on Windows
if(NOT WIN32)
    set_target_properties(Rmt PROPERTIES
        VERSION ${PROJECT_VERSION}
        SOVERSION 1
    )
endif()

# Preprocessor definitions
target_compile_definitions(Rmt PRIVATE ${RMT_DEFINES})

# Include directories
target_include_directories(Rmt PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_BINARY_DIR}
)

# Qt native GUI frontend
if(NOT RMT_BUILD_CORE_ONLY)
    set_target_properties(Rmt PROPERTIES AUTOMOC ON AUTORCC ON AUTOUIC OFF)
    target_link_libraries(Rmt PRIVATE Qt6::Core Qt6::Widgets Qt6::PrintSupport)
    target_compile_definitions(Rmt PRIVATE RMT_QT_GUI=1)
    target_include_directories(Rmt PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/qt)
endif()

# Windows-specific libraries
if(WIN32)
    # Link against Windows audio libraries if found
    if(DSOUND_LIB)
        target_link_libraries(Rmt PRIVATE dsound)
    endif()
    if(WINMM_LIB)
        target_link_libraries(Rmt PRIVATE winmm)
    endif()
    
    # If libraries not found but we're cross-compiling, still link by name
    # (MinGW cross-compiler should handle these)
    if(CMAKE_CROSSCOMPILING)
        if(NOT DSOUND_LIB)
            target_link_libraries(Rmt PRIVATE dsound)
        endif()
        if(NOT WINMM_LIB)
            target_link_libraries(Rmt PRIVATE winmm)
        endif()
    endif()
endif()

# Link PortAudio library (search was done earlier). It plays the DirectSound
# buffer of the sound code (CompatAudio.cpp)
if(PortAudio_FOUND)
    target_link_libraries(Rmt PRIVATE ${RMT_PORTAUDIO_LINK})
    target_include_directories(Rmt PRIVATE ${PortAudio_INCLUDE_DIRS})
elseif(PORTAUDIO_LIB)
    target_link_libraries(Rmt PRIVATE ${PORTAUDIO_LIB})
    if(PORTAUDIO_INCLUDE)
        target_include_directories(Rmt PRIVATE ${PORTAUDIO_INCLUDE})
    endif()
else()
    message(WARNING "PortAudio not found - audio functionality may be limited")
endif()
if((PortAudio_FOUND OR PORTAUDIO_LIB) AND NOT RMT_BUILD_CORE_ONLY)
    target_compile_definitions(Rmt PRIVATE RMT_HAVE_PORTAUDIO)
endif()

# Link RtMidi library (search was done earlier)
if(RTMIDI_FOUND)
    target_link_libraries(Rmt PRIVATE ${RTMIDI_LINK_LIBRARIES})   # full paths (not only in the default folders)
    target_include_directories(Rmt PRIVATE ${RTMIDI_INCLUDE_DIRS})
    target_compile_definitions(Rmt PRIVATE RTMIDI_AVAILABLE=1)
    target_compile_options(Rmt PRIVATE ${RTMIDI_CFLAGS})
    message(STATUS "Found RtMidi via pkg-config: ${RTMIDI_LINK_LIBRARIES}")
elseif(RTMIDI_LIB AND RTMIDI_INCLUDE)
    target_link_libraries(Rmt PRIVATE ${RTMIDI_LIB})
    target_include_directories(Rmt PRIVATE ${RTMIDI_INCLUDE})
    target_compile_definitions(Rmt PRIVATE RTMIDI_AVAILABLE=1)
    message(STATUS "Found RtMidi: ${RTMIDI_LIB}")
else()
    message(WARNING "RtMidi not found - MIDI functionality will be disabled")
    message(STATUS "To install: sudo apt install librtmidi-dev")
endif()

# Platform-specific compiler flags
if(MSVC)
    # Use multi-threaded runtime
    set_property(TARGET Rmt PROPERTY MSVC_RUNTIME_LIBRARY MultiThreadedDLL)
    
    # Disable specific warnings if needed
    target_compile_options(Rmt PRIVATE /permissive-)
elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    # GCC/Clang specific options
    target_compile_options(Rmt PRIVATE
        -Wall
        -Wextra
        -Wno-unknown-pragmas
        -Wno-unused-parameter
    )
endif()

# Output executable name
set_target_properties(Rmt PROPERTIES
    OUTPUT_NAME "Ritmo"
)

if(NOT WIN32)
    if(NOT RMT_BUILD_CORE_ONLY)
        set_target_properties(Rmt PROPERTIES OUTPUT_NAME "ritmo")    # the Qt tracker
    else()
        set_target_properties(Rmt PROPERTIES OUTPUT_NAME "Ritmo.exe")
    endif()
endif()

message(STATUS "Rmt executable target configured")
message(STATUS "  Sources: ${CMAKE_CURRENT_SOURCE_DIR}")
message(STATUS "  Defines: ${RMT_DEFINES}")

# Create separate MIDI test executable (core-only builds)
if(RMT_BUILD_CORE_ONLY)
    set(RMT_MIDITEST_SOURCES
        main-midi.cpp
        # Audio Backend
        AudioBackend.cpp
        DirectSoundAudio.cpp
        PortAudioBackend.cpp
        # MIDI Backend
        MidiBackend.cpp
        WindowsMidiBackend.cpp
        RtMidiBackend.cpp
    )
    if(RMT_NO_PORTAUDIO)
        list(REMOVE_ITEM RMT_MIDITEST_SOURCES PortAudioBackend.cpp)
    endif()
    add_executable(RmtMidiTest WIN32 ${RMT_MIDITEST_SOURCES})
    target_link_libraries(RmtMidiTest PRIVATE Threads::Threads)

    # Apply same configuration as main Rmt executable
    if(WIN32)
        set_target_properties(RmtMidiTest PROPERTIES OUTPUT_NAME "RmtMidiTest")
    else()
        set_target_properties(RmtMidiTest PROPERTIES
            VERSION ${PROJECT_VERSION}
            OUTPUT_NAME "RmtMidiTest.exe"
        )
    endif()
    
    target_compile_definitions(RmtMidiTest PRIVATE ${RMT_DEFINES})
    if(WIN32)
        target_link_libraries(RmtMidiTest PRIVATE winmm dsound)
    endif()
    target_include_directories(RmtMidiTest PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_BINARY_DIR})
    
    # Link audio libraries
    if(PortAudio_FOUND)
        target_link_libraries(RmtMidiTest PRIVATE ${RMT_PORTAUDIO_LINK})
        target_include_directories(RmtMidiTest PRIVATE ${PortAudio_INCLUDE_DIRS})
    elseif(PORTAUDIO_LIB)
        target_link_libraries(RmtMidiTest PRIVATE ${PORTAUDIO_LIB})
        if(PORTAUDIO_INCLUDE)
            target_include_directories(RmtMidiTest PRIVATE ${PORTAUDIO_INCLUDE})
        endif()
    endif()
    
    # Link MIDI libraries
    if(RTMIDI_FOUND)
        target_link_libraries(RmtMidiTest PRIVATE ${RTMIDI_LINK_LIBRARIES})   # full paths (not only in the default folders)
        target_include_directories(RmtMidiTest PRIVATE ${RTMIDI_INCLUDE_DIRS})
        target_compile_definitions(RmtMidiTest PRIVATE RTMIDI_AVAILABLE=1)
        target_compile_options(RmtMidiTest PRIVATE ${RTMIDI_CFLAGS})
    elseif(RTMIDI_LIB AND RTMIDI_INCLUDE)
        target_link_libraries(RmtMidiTest PRIVATE ${RTMIDI_LIB})
        target_include_directories(RmtMidiTest PRIVATE ${RTMIDI_INCLUDE})
        target_compile_definitions(RmtMidiTest PRIVATE RTMIDI_AVAILABLE=1)
    endif()
    
    # Compiler flags
    if(MSVC)
        set_property(TARGET RmtMidiTest PROPERTY MSVC_RUNTIME_LIBRARY MultiThreadedDLL)
        target_compile_options(RmtMidiTest PRIVATE /permissive-)
    elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(RmtMidiTest PRIVATE
            -Wall -Wextra -Wno-unknown-pragmas -Wno-unused-parameter
        )
    endif()
    
    message(STATUS "RmtMidiTest executable configured (MIDI backend test)")
endif()

# ---------------------------------------------------------------------------
# RmtCoreTest
#
# Compiles the *real* RMT engine (Song/Tracks/Instruments/C6502/Pokey/IO_*/
# ASM*/SAPFile*/...) plus the "GUI-shared" rendering files (GUI_Song.cpp,
# GUI_Instruments.cpp, GuiHelpers.cpp, TracksControl.cpp, ChannelControl.cpp,
# Global.cpp) without the Qt frontend, on plain GCC/Linux: the smoke test of the
# engine. It leaves out the Qt frontend (qt/), the view and the document
# (RmtView.cpp, RmtDoc.cpp); the dialogs of the engine are the data-only
# classes of CompatDialogs.cpp.
#
# Usage:
#   cmake -B build-core-linux -DRMT_BUILD_CORE_ONLY=ON -DRMT_CORE_TEST=ON
#   cmake --build build-core-linux
option(RMT_CORE_TEST "Build RmtCoreTest: compile the RMT engine + GUI-shared sources without Qt (smoke test)" OFF)
if(RMT_CORE_TEST)
    message(STATUS "Building RmtCoreTest (engine + GUI-shared sources, no Qt)")

    set(RMT_ENGINE_SOURCES
        # ---- engine (no GUI) ----
        emu/Cpu6502.cpp
        emu/PokeySound.cpp
        AssemblerTypes.cpp
        AtariBinaries.cpp
        Atari.cpp
        AtariIO.cpp
        AtariTrackerDriver.cpp
        C6502.cpp
        Clipboard.cpp
        Fraction.cpp
        Instruments.cpp
        IOHelpers.cpp
        IO_Importer.cpp
        IO_Instruments.cpp
        IO_Song.cpp
        IO_Song_ExportAsm.cpp
        IO_Tracks.cpp
        Keyboard2NoteMapping.cpp
        LZSSFile.cpp
        lzss_sap.cpp
        Midi_Song.cpp
        Notes.cpp
        Pokey.cpp
        PokeyController.cpp
        PokeyRenderer.cpp
        PokeyStream.cpp
        RuntimeException.cpp
        Script.cpp
        ScriptMessages.cpp
        SAPFile.cpp
        SAPFileExporter.cpp
        Shell.cpp
        SongContainer.cpp
        Song.cpp
        Song_DumpSong.cpp
        SongExport.cpp
        SongExporter.cpp
        SongExporterTest.cpp
        SongTimer.cpp
        Tracks.cpp
        Tuning.cpp
        TuningTypes.cpp
        Undo.cpp
        VUPlayer.cpp
        WaveFile.cpp
        WaveFileExporter.cpp
        ASMFileBuilder.cpp
        ASMFile.cpp
        ASMFileExporter.cpp

        # ---- "GUI-shared" rendering files (draw on g_mem_dc/g_gfx_dc) ----
        ChannelControl.cpp
        GuiHelpers.cpp
        GUI_Instruments.cpp
        GUI_Song.cpp
        TracksControl.cpp
        Global.cpp
    )

    list(APPEND RMT_ENGINE_SOURCES
        ${RMT_EMBEDDED_RESOURCES}
        CompatTypes.cpp
        CompatAudio.cpp
        CompatDialogs.cpp
        RmtMidiRt.cpp
    )

    add_executable(RmtCoreTest ${RMT_ENGINE_SOURCES} main-coretest.cpp)
    target_link_libraries(RmtCoreTest PRIVATE Threads::Threads)

    target_compile_definitions(RmtCoreTest PRIVATE ${RMT_DEFINES})
    target_include_directories(RmtCoreTest PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_BINARY_DIR})

    if(PortAudio_FOUND)
        target_link_libraries(RmtCoreTest PRIVATE ${RMT_PORTAUDIO_LINK})
        target_include_directories(RmtCoreTest PRIVATE ${PortAudio_INCLUDE_DIRS})
    elseif(PORTAUDIO_LIB)
        target_link_libraries(RmtCoreTest PRIVATE ${PORTAUDIO_LIB})
        if(PORTAUDIO_INCLUDE)
            target_include_directories(RmtCoreTest PRIVATE ${PORTAUDIO_INCLUDE})
        endif()
    endif()

    if(MSVC)
        set_property(TARGET RmtCoreTest PROPERTY MSVC_RUNTIME_LIBRARY MultiThreadedDLL)
        target_compile_options(RmtCoreTest PRIVATE /permissive-)
    elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(RmtCoreTest PRIVATE
            -Wall -Wextra -Wno-unknown-pragmas -Wno-unused-parameter
        )
    endif()

    message(STATUS "RmtCoreTest executable configured (engine + GUI-shared, smoke test)")
endif()
