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

target_compile_features(
  SyncFPS
  PRIVATE c_std_17
)

if(MSVC)
  target_compile_options(
    SyncFPS
    PRIVATE "/W4"
    PRIVATE "/wd5105"
    PRIVATE "/WX"
  )
elseif(CMAKE_C_COMPILER_ID MATCHES GNU OR CMAKE_C_COMPILER_ID MATCHES Clang)
  target_compile_options(
    SyncFPS
    PRIVATE "-Wall"
    PRIVATE "-Wextra"
    PRIVATE "-Werror"
  )
else()
  message(FATAL_ERROR "Using unsupported toolset")
endif()

target_link_libraries(
  SyncFPS
  INTERFACE winmm.lib
)
