cmake_minimum_required(VERSION 3.15)

set(VERSION_MAJOR 1)
set(VERSION_MINOR 0)
set(VERSION_PATCH 2)

project(AudioPlay VERSION ${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_PATCH} LANGUAGES C)

FILE(GLOB src "src/*.c")
FILE(GLOB include "include/*.h")
add_library(AudioPlay SHARED ${src} ${include})
target_include_directories(AudioPlay PRIVATE include)

add_subdirectory("deps/SyncFPS" EXCLUDE_FROM_ALL)
target_link_libraries(AudioPlay PRIVATE SyncFPS)

add_subdirectory("deps/MCIManager" EXCLUDE_FROM_ALL)
target_link_libraries(AudioPlay PRIVATE MCIManager)

add_subdirectory("deps/allegro" EXCLUDE_FROM_ALL)
target_link_libraries(MCIManager PUBLIC Allegro)

set_property(TARGET AudioPlay PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded")
set_property(TARGET SyncFPS PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded")
set_property(TARGET MCIManager PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded")

target_compile_options(
  AudioPlay
  PRIVATE "-Wall"
  PRIVATE "-Wextra"
  PRIVATE "-Werror"
)

target_link_options(
  AudioPlay
  PRIVATE "LINKER:/SUBSYSTEM:CONSOLE"
  PRIVATE "LINKER:/NODEFAULTLIB:libucrt.lib"
)

target_link_libraries(
  AudioPlay
  PRIVATE ucrt.lib
)
