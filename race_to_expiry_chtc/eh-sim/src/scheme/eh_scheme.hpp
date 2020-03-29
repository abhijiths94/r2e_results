
#ifndef EH_SIM_SCHEME_HPP
#define EH_SIM_SCHEME_HPP

#include <set>
#include <unordered_map>

namespace ehsim {

class capacitor;
struct stats_bundle;
struct eh_model_parameters;

/**
 * An abstract checkpointing scheme.
 */
class eh_scheme {
public:
  virtual capacitor &get_battery() = 0;

  virtual uint32_t clock_frequency() const = 0;

  virtual double min_energy_to_power_on(stats_bundle *stats) = 0;

  virtual void execute_instruction(stats_bundle *stats) = 0;

  virtual void calculate_backup_locs(bool use_reg_lva, const std::set<uint64_t> dead_regs) = 0;

  virtual bool is_active(stats_bundle *stats) = 0;

  virtual bool will_backup(stats_bundle *stats) = 0;

  virtual uint64_t backup(stats_bundle *stats) = 0;

  virtual uint64_t restore(stats_bundle *stats) = 0;

  virtual double estimate_progress(eh_model_parameters const &) const = 0;

  virtual void set_dead_addresses(const std::set<uint64_t>&) = 0;

  virtual const uint32_t get_wb_buffer_size() = 0;

  virtual const uint64_t& get_true_positives() = 0;

  virtual const uint64_t& get_false_positives() = 0;

  virtual const uint64_t get_renamed_mappings() = 0;

  virtual const uint64_t get_reclaimed_mappings() = 0;

  virtual bool optimal_backup_scheme(uint64_t curr_insn_cycle, uint32_t address, size_t set, size_t way, bool memwr, bool memop, bool branch, bool branch_link, uint32_t num_mem_access) = 0;

  virtual void reset_stats() = 0;

  virtual void print_map_table() = 0;
};
}

#endif //EH_SIM_SCHEME_HPP
