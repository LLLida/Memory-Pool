cmake_minimum_required(VERSION 3.15)

project(Memory-Pool LANGUAGES CXX)

add_library(Memory-Pool INTERFACE)
target_include_directories(Memory-Pool INTERFACE
  ${CMAKE_SOURCE_DIR}/include)
add_library(lida::Memory-Pool ALIAS Memory-Pool)

