cmake_minimum_required(VERSION 3.1 FATAL_ERROR)

project(
  libbf
  VERSION 0.0.1
  LANGUAGES CXX
)

add_library(
  ${PROJECT_NAME}
  include/bf/bloom_filter/a2.hpp
  include/bf/bloom_filter/basic.hpp
  include/bf/bloom_filter/bitwise.hpp
  include/bf/bloom_filter/counting.hpp
  include/bf/bloom_filter/stable.hpp
  include/bf/all.hpp
  include/bf/bitvector.hpp
  include/bf/bloom_filter.hpp
  include/bf/counter_vector.hpp
  include/bf/h3.hpp
  include/bf/hash.hpp
  include/bf/object.hpp
  include/bf/wrap.hpp
  src/bloom_filter/a2.cpp
  src/bloom_filter/basic.cpp
  src/bloom_filter/bitwise.cpp
  src/bloom_filter/counting.cpp
  src/bloom_filter/stable.cpp
  src/bitvector.cpp
  src/counter_vector.cpp
  src/hash.cpp
)

target_include_directories(
  ${PROJECT_NAME}
  PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
)

set_target_properties(
  ${PROJECT_NAME} PROPERTIES
  CXX_STANDARD 14
  CXX_STANDARD_REQUIRED ON
)