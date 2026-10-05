cmake_minimum_required(VERSION 3.25)

# Project definition
project(
    Rmt
    VERSION 2.6.0
    DESCRIPTION "RITMO - Atari XL/XE music tracker, based on RASTER Music Tracker"
    LANGUAGES CXX C
)

# ---------------------------------------------------------------------------
# Version string: the git tag when the commit is tagged (v2.3, v2.4-rc1, …), else the
# RMT_BASE_VERSION, the numeric major.minor of the version in development (also used by
# scripts/push.sh). RMT_VERSION_FULL is the human-readable string baked into the binary.
# ---------------------------------------------------------------------------
set(RMT_BASE_VERSION "2.6")

find_package(Git QUIET)
if(Git_FOUND)
    # Exact tag on current commit?
    execute_process(
        COMMAND "${GIT_EXECUTABLE}" describe --tags --match "v[0-9]*" --exact-match
        OUTPUT_VARIABLE _RMT_EXACT_TAG
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    )
    if(_RMT_EXACT_TAG)
        string(REGEX REPLACE "^v" "" RMT_VERSION_FULL "${_RMT_EXACT_TAG}")
    else()
        set(RMT_VERSION_FULL "${RMT_BASE_VERSION}")
    endif()
else()
    set(RMT_VERSION_FULL "${RMT_BASE_VERSION}")
endif()

message(STATUS "RMT version: ${RMT_VERSION_FULL}")

# Platform detection EARLY
if(WIN32)
    set(WINDOWS_PLATFORM TRUE)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Windows" OR CMAKE_CROSSCOMPILING)
    # Also True if cross-compiling for Windows
    set(WINDOWS_PLATFORM TRUE)
else()
    set(WINDOWS_PLATFORM FALSE)
endif()

# Core-only build option (for Phase 2 development on non-Windows)
option(RMT_BUILD_CORE_ONLY "Build only core components without UI (for testing audio/MIDI backends)" OFF)
if(RMT_BUILD_CORE_ONLY)
    message(STATUS "Building core components only (no UI) - for Phase 2 development")
endif()


# Set C++ standard globally
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Set C standard globally
set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

# Build type configuration
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "Generator: ${CMAKE_GENERATOR}")
message(STATUS "Compiler: ${CMAKE_CXX_COMPILER_ID}")
message(STATUS "Platform: Windows=${WINDOWS_PLATFORM}, CrossCompiling=${CMAKE_CROSSCOMPILING}")

# Compiler-specific settings
if(MSVC)
    # MSVC specific settings
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /permissive- /MP")
    set(CMAKE_CXX_FLAGS_DEBUG "/MDd /Zi /Ob0 /RTC1")
    set(CMAKE_CXX_FLAGS_RELEASE "/MD /O2 /Ob2 /DNDEBUG /GL")
    
    # Warning level
    if(CMAKE_CXX_FLAGS MATCHES "/W[0-4]")
        string(REGEX REPLACE "/W[0-4]" "/W3" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
    else()
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /W3")
    endif()
    
    # Link time code generation for Release
    set(CMAKE_EXE_LINKER_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS_RELEASE} /LTCG")
    set(CMAKE_SHARED_LINKER_FLAGS_RELEASE "${CMAKE_SHARED_LINKER_FLAGS_RELEASE} /LTCG")
    
elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    # GCC/Clang specific settings
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wno-unknown-pragmas")
    set(CMAKE_CXX_FLAGS_DEBUG "-g -O0 -fno-omit-frame-pointer")
    set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG")
    
    # Link time optimization for Release
    if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang" OR CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL 11)
        set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -flto")
        set(CMAKE_EXE_LINKER_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS_RELEASE} -flto")
    endif()
endif()

# Output directories
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/out)

# Runtime files next to the programs: the tracker drivers (rmt_driver_v*.obx)
# and the other files RMT loads from <program folder>/resources
file(COPY ${PROJECT_SOURCE_DIR}/rmt/resources DESTINATION ${CMAKE_RUNTIME_OUTPUT_DIRECTORY})

# Generate version header from git tag (used by CompatTypes.cpp and the Qt frontend)
configure_file(
    "${PROJECT_SOURCE_DIR}/src/RmtVersion.h.in"
    "${CMAKE_BINARY_DIR}/RmtVersion.h"
    @ONLY
)

# Main source subdirectory
add_subdirectory(src)

# Print configuration summary
message(STATUS "")
message(STATUS "=== RMT CMake Configuration ===")
message(STATUS "Version: ${RMT_VERSION_FULL}")
message(STATUS "Platform: ${CMAKE_SYSTEM_NAME} ${CMAKE_SYSTEM_PROCESSOR}")
message(STATUS "Compiler: ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "Build Type: ${CMAKE_BUILD_TYPE}")
message(STATUS "C++ Standard: C++${CMAKE_CXX_STANDARD}")
message(STATUS "C Standard: C${CMAKE_C_STANDARD}")
if(CMAKE_CROSSCOMPILING)
    message(STATUS "Target: ${CMAKE_SYSTEM_NAME}")
endif()
message(STATUS "===============================")
