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

target_compile_features(
  AudioPlay
  PRIVATE c_std_17
)

target_compile_definitions(
  AudioPlay
  PRIVATE
  WIN32
  _WINDOWS
  UNICODE
  _UNICODE
	$<$<CONFIG:Debug>:
    DEBUG
    _DEBUG
	>
)

if(MSVC)
  message("tools = cl / clang-cl (MSVC CLI)")
  target_compile_options(
    AudioPlay
    PRIVATE
    "/W4"
    "/wd5105"
    "/WX"
    "/arch:AVX"
    "/EHa-"
    "/EHs-"
    "/GA"
    "/GF"
    "/nologo"
    "/TC"
    "/utf-8"
		$<$<CONFIG:Debug>:
			"/GR"
			"/JMC"
			"/RTC1"
		>
		$<$<CONFIG:Release>:
			"/GL"
			"/GR-"
			"/Gw"
			"/Gy"
			"/Oi"
		>
  )
  target_link_options(
    AudioPlay
    PRIVATE
    "LINKER:/NODEFAULTLIB:libucrt.lib"
  )

  target_link_libraries(
    AudioPlay
    PRIVATE
    ucrt.lib
  )
elseif(CMAKE_C_COMPILER_ID MATCHES GNU)
  message("tools = gcc")
  target_compile_options(
    AudioPlay
    PRIVATE
    "-Wall"
    "-Wextra"
    "-Werror"
    "-mavx"
		"-mfpmath=sse"
		$<$<CONFIG:Debug>:
			"-Og"
		>
		$<$<CONFIG:Release>:
			"-fomit-frame-pointer"
			"-ftree-vectorize"
		>
  )

  target_link_libraries(
    AudioPlay
    PRIVATE
    ucrt.lib
  )
elseif(CMAKE_C_COMPILER_ID MATCHES Clang)
  message("tools = clang / clang-cl (GNU CLI)")
  target_compile_options(
    AudioPlay
    PRIVATE
    "-Wall"
    "-Wextra"
    "-Werror"
    "-mavx"
		"-mfpmath=sse"
		$<$<CONFIG:Debug>:
			"-Og"
		>
		$<$<CONFIG:Release>:
			"-fomit-frame-pointer"
			"-ftree-vectorize"
		>
  )
else()
  message(FATAL_ERROR "Using unsupported toolset")
endif()
