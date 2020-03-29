cmake_minimum_required(VERSION 3.1 FATAL_ERROR)

project(
  thumbulator
  VERSION 0.0.1
  LANGUAGES CXX
)

add_library(
  ${PROJECT_NAME}
  include/thumbulator/cpu.hpp
  include/thumbulator/decode.hpp
  include/thumbulator/cache_block.hpp
  include/thumbulator/cache.hpp
  include/thumbulator/rename.hpp
  include/thumbulator/memory.hpp
  src/cpu_flags.hpp
  src/decode.cpp
  src/exit.hpp
  src/cpu.cpp
  src/exmemwb_arith.cpp
  src/exmemwb_branch.cpp
  src/exmemwb_logic.cpp
  src/exmemwb_mem.cpp
  src/exmemwb_misc.cpp
  src/memory.cpp
  src/trace.hpp
)

target_include_directories(
  ${PROJECT_NAME}
  PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(
  ${PROJECT_NAME}
  PRIVATE hashmap
)

set_target_properties(
  ${PROJECT_NAME} PROPERTIES
  CXX_STANDARD 14
  CXX_STANDARD_REQUIRED ON
)