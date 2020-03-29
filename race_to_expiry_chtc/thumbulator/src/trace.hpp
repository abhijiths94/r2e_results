#ifndef THUMBULATOR_TRACE_HPP
#define THUMBULATOR_TRACE_HPP

#include <cstdio>

namespace thumbulator {

#define ENABLE_INSTRUCTION_TRACE 0
#define TRACE_INSTRUCTION(format, ...)                \
  do {                                                \
    if(ENABLE_INSTRUCTION_TRACE) {                    \
      fprintf(stderr, "%08X:\t", cpu_get_pc() - 0x5); \
      fprintf(stderr, format, __VA_ARGS__);           \
    }                                                 \
  } while(0)
}
#endif //THUMBULATOR_TRACE_HPP
