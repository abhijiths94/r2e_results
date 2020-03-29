#ifndef EH_SIM_SIMULATE_HPP
#define EH_SIM_SIMULATE_HPP

#include <chrono>
#include <cstdint>

namespace ehsim {

class eh_scheme;
struct stats_bundle;
class voltage_trace;
class liveness_trace;

/**
 * Simulate an energy harvesting device.
 *
 * @param binary_file The path to the application binary file.
 * @param power The power supply over time.
 * @param scheme The energy harvesting scheme to use.
 * @param always_harvest true to harvest always, false to harvest during off periods only.
 *
 * @return The statistics tracked during the simulation.
 */
stats_bundle simulate(char const *binary_file,
    ehsim::voltage_trace const &power,
    bool const use_reg_lva,
    ehsim::liveness_trace const &reg_liveness,
    bool const use_mem_lva,
    ehsim::liveness_trace const &mem_liveness,
    eh_scheme *scheme,
    bool always_harvest);
}

#endif //EH_SIM_SIMULATE_HPP
