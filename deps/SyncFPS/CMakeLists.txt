cmake_minimum_required(VERSION 3.15)

project(SyncFPS LANGUAGES C)

FILE(GLOB src "src/*.c")
FILE(GLOB include "include/*.h")
FILE(GLOB share "share/SyncFPS/*.h")
add_library(SyncFPS STATIC ${src} ${include} ${share})
target_include_directories(
  SyncFPS
  PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include
  PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/share
)

target_compile_options(
  SyncFPS
  PRIVATE "-Wall"
  PRIVATE "-Wextra"
  PRIVATE "-Werror"
)

target_link_libraries(
  SyncFPS
  INTERFACE winmm.lib
)
