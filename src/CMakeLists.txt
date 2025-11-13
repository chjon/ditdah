cmake_minimum_required(VERSION 3.28)

project(ditdah VERSION 0.0.0)

set (CMAKE_CXX_STANDARD 11)

add_library(portaudio SHARED IMPORTED)
set_target_properties(portaudio PROPERTIES
  IMPORTED_LOCATION "/usr/local/lib/libportaudio.so"
  INTERFACE_INCLUDE_DIRECTORIES "/usr/local/include/portaudio.h"
)

add_executable(ditdah main.cpp)
target_link_libraries(ditdah portaudio)