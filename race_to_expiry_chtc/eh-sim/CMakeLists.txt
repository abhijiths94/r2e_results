cmake_minimum_required(VERSION 3.1 FATAL_ERROR)

project(
  eh-sim
  VERSION 0.0.1
  LANGUAGES CXX
)

find_package(Torch REQUIRED)

add_executable(
  ${PROJECT_NAME}
  src/scheme/backup_every_cycle.hpp
  src/scheme/clank.hpp
  src/scheme/data_sheet.hpp
  src/scheme/eh_model.hpp
  src/scheme/eh_scheme.hpp
  src/scheme/magical_scheme.hpp
  src/scheme/on_demand_all_backup.hpp
  src/scheme/parametric.hpp
  src/scheme/mem_rename.hpp
  src/capacitor.hpp
  src/main.cpp
  src/simulate.cpp
  src/simulate.hpp
  src/stats.hpp
  src/voltage_trace.cpp
  src/voltage_trace.hpp
  src/liveness_trace.cpp
  src/liveness_trace.hpp
)

target_include_directories(
  ${PROJECT_NAME}
  PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(
  ${PROJECT_NAME}
  PRIVATE argagg
  PRIVATE libbf
  PRIVATE thumbulator
  "${TORCH_LIBRARIES}"
)

set_target_properties(
  ${PROJECT_NAME} PROPERTIES
  CXX_STANDARD 14
  CXX_STANDARD_REQUIRED ON
)




