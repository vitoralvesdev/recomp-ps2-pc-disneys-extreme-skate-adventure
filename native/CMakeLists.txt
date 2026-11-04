# Asset conversion runs on the PC during development. No guest CPU, BIOS,
# IRX module, PS2 runtime or game executable is linked or executed here.
add_executable(DisneyAssetConverter asset_converter.cpp)
target_compile_features(DisneyAssetConverter PRIVATE cxx_std_20)
if(MSVC)
    target_compile_options(DisneyAssetConverter PRIVATE /W4 /permissive-)
endif()
set_target_properties(DisneyAssetConverter PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/dist/native-tools")

option(DISNEY_BUILD_RESOURCE_LAB "Build the asset laboratory with approximate gameplay (not the original game)" OFF)
if(DISNEY_BUILD_RESOURCE_LAB)
    include(native-target.cmake)
endif()

# Recovered original-game code. Not a replacement gameplay simulation.
add_library(DisneyOriginalCore STATIC original/occlusion.cpp)
target_compile_features(DisneyOriginalCore PUBLIC cxx_std_20)
target_include_directories(DisneyOriginalCore PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/original")
add_executable(DisneyOriginalCoreTest original/occlusion_test.cpp)
target_link_libraries(DisneyOriginalCoreTest PRIVATE DisneyOriginalCore)
if(MSVC)
 target_compile_options(DisneyOriginalCore PRIVATE /W4 /permissive- /fp:strict)
 target_compile_options(DisneyOriginalCoreTest PRIVATE /W4 /permissive- /fp:strict)
endif()
set_target_properties(DisneyOriginalCoreTest PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/dist/native-tools")
# Compiled original module logic, with no instruction-interpreter fallback.
add_subdirectory(original/iop)
