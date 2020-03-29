#include "voltage_trace.hpp"

#include <fstream>
#include <iostream>

namespace ehsim {
voltage_trace::voltage_trace(std::string const &path_to_trace, std::chrono::milliseconds const &sample_period)
  : maximum_time(0), period(sample_period)
{
  std::ifstream trace(path_to_trace);

  uint64_t raw_time;
  double voltage;
  if(trace >> raw_time >> voltage) {
    voltages.emplace_back(voltage);

    while(trace >> raw_time >> voltage) {
      voltages.emplace_back(voltage);
    }

    maximum_time = std::chrono::milliseconds(voltages.size());
    // std::cout << "maximum_time: " << maximum_time.count() << "\n";
  }
}

double voltage_trace::get_voltage(std::chrono::milliseconds const &time) const
{
  // this wraps around the voltage trace
  auto const index = (time.count() / period.count()) % maximum_time.count();
  return voltages[index];
}
}
