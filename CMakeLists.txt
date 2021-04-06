cmake_minimum_required(VERSION 3.15)

project(Memory-Pool LANGUAGES CXX)

add_library(Memory-Pool INTERFACE)
target_include_directories(Memory-Pool INTERFACE
  ${CMAKE_SOURCE_DIR}/include)
add_library(lida::Memory-Pool ALIAS Memory-Pool)

add_executable(test
  "main.cpp")
target_link_libraries(test PRIVATE Memory-Pool)
set_target_properties(test PROPERTIES
  CXX_STANDARD 17
  CXX_STANDARD_REQUIRED ON)

