cmake_minimum_required(VERSION 3.15)

project(Allegro LANGUAGES C)
add_library(Allegro INTERFACE)

target_include_directories(
  Allegro
  INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(
  Allegro
  INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/lib/ALLEG40.lib
)
