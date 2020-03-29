cmake_minimum_required(VERSION 3.1 FATAL_ERROR)

project(
  energy-harvesting
  VERSION 0.0.1
)

# include bundled dependencies
add_subdirectory(external)

add_subdirectory(thumbulator)
add_subdirectory(eh-sim)
