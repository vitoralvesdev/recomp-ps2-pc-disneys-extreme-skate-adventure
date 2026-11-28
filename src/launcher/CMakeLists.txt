if(NOT WIN32)
    return()
endif()

if(POLICY CMP0091)
    cmake_policy(SET CMP0091 NEW)
endif()
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded")

enable_language(RC)

set(DISNEY_LAUNCHER_CORE_SOURCES
    common.cpp
    config.cpp
    install.cpp
    iso.cpp
    json.cpp
    layout.cpp
    log.cpp
    package.cpp
    process.cpp
    update.cpp
    validate.cpp)

add_library(DisneyLauncherCore STATIC ${DISNEY_LAUNCHER_CORE_SOURCES})
target_include_directories(DisneyLauncherCore PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_compile_features(DisneyLauncherCore PUBLIC cxx_std_20)
target_compile_definitions(DisneyLauncherCore PUBLIC
    UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX _CRT_SECURE_NO_WARNINGS)
if(MSVC)
    # The sources contain UTF-8 Portuguese text; without this MSVC decodes them
    # using the system code page and the UI would display mojibake. PUBLIC
    # propagates the flag to the launcher and test executables.
    target_compile_options(DisneyLauncherCore PUBLIC /utf-8)
endif()
target_link_libraries(DisneyLauncherCore PUBLIC
    bcrypt winhttp shlwapi ole32 shell32 user32 advapi32)

add_executable(DisneySkateLauncher WIN32
    main.cpp
    app.cpp
    ui.cpp
    launcher.rc)
target_compile_features(DisneySkateLauncher PRIVATE cxx_std_20)
target_compile_definitions(DisneySkateLauncher PRIVATE
    UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX _CRT_SECURE_NO_WARNINGS)
target_link_libraries(DisneySkateLauncher PRIVATE
    DisneyLauncherCore comctl32 comdlg32 gdiplus ole32 shell32 user32)
if(MSVC)
    # The .rc already embeds launcher.manifest with ID 1; stop the linker from
    # generating a second manifest resource for the same executable.
    target_link_options(DisneySkateLauncher PRIVATE /MANIFEST:NO)
endif()
set_target_properties(DisneySkateLauncher PROPERTIES
    OUTPUT_NAME "Disney's Extreme Skate Adventure"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}")
foreach(cfg DEBUG RELEASE RELWITHDEBINFO MINSIZEREL)
    set_target_properties(DisneySkateLauncher PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY_${cfg} "${CMAKE_SOURCE_DIR}")
endforeach()

add_executable(DisneyLauncherTests tests/launcher_tests.cpp)
target_compile_features(DisneyLauncherTests PRIVATE cxx_std_20)
target_link_libraries(DisneyLauncherTests PRIVATE DisneyLauncherCore)
set_target_properties(DisneyLauncherTests PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/launcher-tests")
add_test(NAME DisneyLauncherTests COMMAND DisneyLauncherTests)

# The old probe driver is kept for the packaged-runtime regression test.
add_executable(DisneyLauncherProbe EXCLUDE_FROM_ALL "${CMAKE_SOURCE_DIR}/tests/launcher_probe.cpp")
set_target_properties(DisneyLauncherProbe PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/launcher-test")
