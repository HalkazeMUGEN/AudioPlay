cmake_minimum_required(VERSION 3.15)

project(MCIManager LANGUAGES C)

add_library(uthash INTERFACE)
target_include_directories(
  uthash
  INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/deps/uthash/src
)

if(MSVC)
  target_compile_options(
    uthash
    INTERFACE "/wd4127"
    INTERFACE "/wd4702"
  )
endif()

FILE(GLOB src "src/*.c")
FILE(GLOB include "include/*.h")
FILE(GLOB share "share/MCIManager/*.h")
add_library(MCIManager STATIC ${src} ${include} ${share})
target_include_directories(
  MCIManager
  PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include
  PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/share
)

target_link_libraries(MCIManager PRIVATE uthash)

target_compile_features(
  MCIManager
  PRIVATE c_std_17
)

if (MSVC)
  target_compile_options(
    MCIManager
    PRIVATE "/W4"
    PRIVATE "/wd5105"
    PRIVATE "/WX"
  )  
  if(CMAKE_C_COMPILER_ID MATCHES MSVC)
    target_compile_options(
      MCIManager
      PRIVATE "/experimental:c11atomics"
    )
  endif()
elseif(CMAKE_C_COMPILER_ID MATCHES GNU OR CMAKE_C_COMPILER_ID MATCHES Clang)
  target_compile_options(
    MCIManager
    PRIVATE "-Wall"
    PRIVATE "-Wextra"
    PRIVATE "-Werror"
  )
else()
  message(FATAL_ERROR "Using unsupported toolset")
endif()
