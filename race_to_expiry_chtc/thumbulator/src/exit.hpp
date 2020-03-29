#ifndef THUMBULATOR_EXIT_HPP
#define THUMBULATOR_EXIT_HPP

#include <cstdio>
#include <cstdlib>

namespace thumbulator {

/**
 * Terminate the simulation prematurely.
 *
 * Use this on a fatal error.
 */
inline void terminate_simulation(int exit_code)
{
  printf("Simulation was terminated prematurely.\n");
  exit(exit_code);
}
}
#endif //THUMBULATOR_EXIT_HPP
