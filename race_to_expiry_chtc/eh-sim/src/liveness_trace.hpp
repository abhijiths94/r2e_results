#ifndef EH_SIM_LIVENESS_TRACE_HPP
#define EH_SIM_LIVENESS_TRACE_HPP

#include <chrono>
#include <string>
#include <map>
#include <set>

namespace ehsim {
class liveness_trace {
public:
  /**
   * Constructor.
   *
   * @param path_to_trace Path to an existing and valid trace file.
   */
  liveness_trace(bool use_liveness, std::string const &path_to_trace);

  /**
   * Get the liveness count at the specified cycle.
   *
   * @param cycle in number of cycles.
   *
   * @return The liveness count.
   */
  const std::set<uint64_t> get_liveness(uint64_t const cycle) const;

private:

  std::map<uint64_t, std::set<uint64_t>> liveness;
};
}

#endif //EH_SIM_LIVENESS_TRACE_HPP
