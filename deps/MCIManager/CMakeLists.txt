cmake_minimum_required(VERSION 3.15)

project(MCIManager LANGUAGES C)

FILE(GLOB src "src/*.c")
FILE(GLOB include "include/*.h")
FILE(GLOB share "share/MCIManager/*.h")
add_library(MCIManager STATIC ${src} ${include} ${share})
target_include_directories(
  MCIManager
  PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include
  PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/deps/uthash/src
  PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/share
)

target_compile_options(
  MCIManager
  PRIVATE "-Wall"
  PRIVATE "-Wextra"
  PRIVATE "-Werror"
)
