cmake_minimum_required(VERSION 3.1 FATAL_ERROR)

project(
  argagg
  VERSION 0.4.6
)

add_library(${PROJECT_NAME} INTERFACE)

target_sources(
  ${PROJECT_NAME}
  INTERFACE "$<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/include/argagg/argagg.hpp>"
)

target_include_directories(
  ${PROJECT_NAME}
  INTERFACE "$<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/include>"
)
