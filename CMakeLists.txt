cmake_minimum_required(VERSION 3.20)
project(Journey LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release)
endif()

# ---- raylib -----------------------------------------------------------------
# Uses an installed raylib if there is one; otherwise downloads 5.5 (needs internet once).
# Offline? Point at a local checkout:  -DFETCHCONTENT_SOURCE_DIR_RAYLIB=/path/to/raylib
# A copy of raylib 5.5 ships in third_party/ so the Android and Windows builds need no git or internet.
if(EXISTS ${CMAKE_SOURCE_DIR}/third_party/raylib/CMakeLists.txt)
  set(FETCHCONTENT_SOURCE_DIR_RAYLIB ${CMAKE_SOURCE_DIR}/third_party/raylib CACHE PATH "" FORCE)
endif()
if(ANDROID)
  set(PLATFORM Android CACHE STRING "" FORCE)    # raylib builds its Android backend (OpenGL ES 2, native_app_glue)
else()
  find_package(raylib 5.5 QUIET)
endif()
if(NOT raylib_FOUND)
  include(FetchContent)
  set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
  set(BUILD_GAMES OFF CACHE BOOL "" FORCE)
  FetchContent_Declare(raylib GIT_REPOSITORY https://github.com/raysan5/raylib.git GIT_TAG 5.5 GIT_SHALLOW ON)
  FetchContent_MakeAvailable(raylib)
endif()

# ---- game core: pure C++, no raylib, unit tested ---------------------------------
add_library(journey_core STATIC src/world/TileMap.cpp src/world/Pathfinder.cpp)
target_include_directories(journey_core PUBLIC src)
if(MSVC)
  target_compile_options(journey_core PRIVATE /W4)
else()
  target_compile_options(journey_core PRIVATE -Wall -Wextra)
endif()

# ---- the game -----------------------------------------------------------------------
set(JOURNEY_SOURCES
  src/main.cpp
  src/game/Player.cpp
  src/platform/PointerInput.cpp
  src/render/CameraRig.cpp
  src/render/CharacterRenderer.cpp
  src/game/Appearance.cpp
  src/render/LitShader.cpp
  src/render/TextureCache.cpp
  src/render/TownRenderer.cpp
  src/ui/CharacterCreator.cpp
  src/ui/Hud.cpp)
if(ANDROID)
  # Phones load the game as libjourney.so inside a NativeActivity (see android/).
  add_library(journey SHARED ${JOURNEY_SOURCES})
  target_link_options(journey PRIVATE -u ANativeActivity_onCreate)
  target_link_libraries(journey PRIVATE journey_core raylib log android EGL GLESv2 OpenSLES m dl)
else()
  add_executable(journey ${JOURNEY_SOURCES})
  target_link_libraries(journey PRIVATE journey_core raylib)
endif()
target_include_directories(journey PRIVATE src)
if(MSVC)
  target_compile_options(journey PRIVATE /W4)
else()
  target_compile_options(journey PRIVATE -Wall -Wextra -Wno-missing-field-initializers)
endif()
if(WIN32 AND NOT MSVC)
  target_link_options(journey PRIVATE -static)   # single .exe, no MinGW DLLs to ship
endif()

# Copy assets next to the executable (the game looks for ./assets at runtime).
# (On Android the assets are packed into the APK by Gradle instead.)
if(NOT ANDROID)
  add_custom_command(TARGET journey POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/assets $<TARGET_FILE_DIR:journey>/assets)
endif()

# ---- tests ---------------------------------------------------------------------------
option(JOURNEY_BUILD_TESTS "Build unit tests" ON)
if(JOURNEY_BUILD_TESTS AND NOT CMAKE_CROSSCOMPILING AND NOT ANDROID)
  enable_testing()
  add_executable(test_core tests/test_core.cpp)
  target_link_libraries(test_core PRIVATE journey_core)
  add_test(NAME core COMMAND test_core ${CMAKE_SOURCE_DIR}/assets/maps/town.map)
endif()
