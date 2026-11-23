cmake_minimum_required(VERSION 3.21)
project(DisneySkateNative LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# The launcher is the only component that must build from a clean clone without
# the game data, PS2Recomp or generated AOT sources. Use this switch for the
# reproducible launcher/CI build: cmake -S . -B build/launcher -DDISNEY_BUILD_LAUNCHER_ONLY=ON
option(DISNEY_BUILD_LAUNCHER_ONLY "Build only the native Windows launcher and its tests" OFF)

# The previous research build contains PS2 hardware interpreters. It is opt-in.
option(DISNEY_BUILD_LEGACY_PROTOTYPE "Build the legacy PS2Recomp research prototype" OFF)
option(DISNEY_BUILD_ORIGINAL_AOT_CANDIDATE "Build original-code integration candidate without instruction interpreters" OFF)

if(DISNEY_BUILD_LAUNCHER_ONLY)
    if(NOT WIN32)
        message(FATAL_ERROR "The launcher target is Windows-only.")
    endif()
    enable_testing()
    add_subdirectory(src/launcher)
else()
    if(DISNEY_BUILD_ORIGINAL_AOT_CANDIDATE)
        include("${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.original-aot.txt")
    elseif(DISNEY_BUILD_LEGACY_PROTOTYPE)
        include("${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.legacy.txt")
    else()
        add_subdirectory(native)
    endif()

    # One visible Windows entry point; no PowerShell or command prompt required.
    if(WIN32)
        enable_testing()
        add_subdirectory(src/launcher)
    endif()
endif()
