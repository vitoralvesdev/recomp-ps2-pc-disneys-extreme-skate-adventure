
# Converted-data native integration. This target has no legacy source, guest
# CPU, console-memory context, original-code loader or PS2 runtime dependency.
set(DISNEY_RAYLIB_ROOT "" CACHE PATH "Root containing raylib-src and raylib-build")
if(NOT DISNEY_RAYLIB_ROOT OR
   NOT EXISTS "${DISNEY_RAYLIB_ROOT}/raylib-build/raylib/Release/raylib.lib" OR
   NOT EXISTS "${DISNEY_RAYLIB_ROOT}/raylib-src/src/raylib.h")
    message(FATAL_ERROR "Set DISNEY_RAYLIB_ROOT to an existing native raylib 5.5 source/build directory")
endif()
add_library(disney_native_raylib STATIC IMPORTED)
set_target_properties(disney_native_raylib PROPERTIES
    IMPORTED_LOCATION "${DISNEY_RAYLIB_ROOT}/raylib-build/raylib/Release/raylib.lib"
    INTERFACE_INCLUDE_DIRECTORIES "${DISNEY_RAYLIB_ROOT}/raylib-src/src")
add_executable(DisneyNativeTest native_skate.cpp)
target_compile_features(DisneyNativeTest PRIVATE cxx_std_20)
target_compile_options(DisneyNativeTest PRIVATE /W4 /permissive- /utf-8)
target_link_libraries(DisneyNativeTest PRIVATE disney_native_raylib opengl32 winmm gdi32 user32 shell32)
set_target_properties(DisneyNativeTest PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/dist/native-test")
add_executable(DisneyNativeCollisionTest EXCLUDE_FROM_ALL tests/native_collision_test.cpp)
target_compile_features(DisneyNativeCollisionTest PRIVATE cxx_std_20)
target_include_directories(DisneyNativeCollisionTest PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}" "${DISNEY_RAYLIB_ROOT}/raylib-src/src")
target_compile_options(DisneyNativeCollisionTest PRIVATE /W4 /permissive- /utf-8)
set_target_properties(DisneyNativeCollisionTest PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/dist/native-tools")
add_custom_command(TARGET DisneyNativeTest POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:DisneyNativeTest>/data"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_SOURCE_DIR}/assets/native/levels/hub/HUB.glb"
        "${CMAKE_SOURCE_DIR}/assets/native/collision/hub/HUB.dscollision"
        "${CMAKE_SOURCE_DIR}/assets/native/spawn.txt"
        "$<TARGET_FILE_DIR:DisneyNativeTest>/data"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_SOURCE_DIR}/assets/native/characters/timon/timon.glb"
        "$<TARGET_FILE_DIR:DisneyNativeTest>/data/timon.glb")
