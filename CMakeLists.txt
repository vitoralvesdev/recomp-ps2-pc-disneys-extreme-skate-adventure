cmake_minimum_required(VERSION 3.21)
project(DisneySkateNative LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# The previous research build contains PS2 hardware interpreters. It is opt-in.
option(DISNEY_BUILD_LEGACY_PROTOTYPE "Build the legacy PS2Recomp research prototype" OFF)
option(DISNEY_BUILD_ORIGINAL_AOT_CANDIDATE "Build original-code integration candidate without instruction interpreters" OFF)
if(DISNEY_BUILD_ORIGINAL_AOT_CANDIDATE)
    include("${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.original-aot.txt")
elseif(DISNEY_BUILD_LEGACY_PROTOTYPE)
    include("${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.legacy.txt")
else()
    add_subdirectory(native)
endif()

# One visible Windows entry point; no PowerShell or command prompt required.
if(WIN32)
 enable_language(RC)
 add_executable(DisneySkateLauncher WIN32 src/launcher/main.cpp src/launcher/launcher.rc)
 target_compile_features(DisneySkateLauncher PRIVATE cxx_std_20)
 target_compile_definitions(DisneySkateLauncher PRIVATE UNICODE _UNICODE)
 target_link_libraries(DisneySkateLauncher PRIVATE user32 shell32)
 set_target_properties(DisneySkateLauncher PROPERTIES
  OUTPUT_NAME "Disney's Extreme Skate Adventure"
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}")
 foreach(cfg DEBUG RELEASE RELWITHDEBINFO MINSIZEREL)
  set_target_properties(DisneySkateLauncher PROPERTIES RUNTIME_OUTPUT_DIRECTORY_${cfg} "${CMAKE_SOURCE_DIR}")
 endforeach()
 add_executable(DisneyLauncherProbe EXCLUDE_FROM_ALL tests/launcher_probe.cpp)
 set_target_properties(DisneyLauncherProbe PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/launcher-test")
endif()
