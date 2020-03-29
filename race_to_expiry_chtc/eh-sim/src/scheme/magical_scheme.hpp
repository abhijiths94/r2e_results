#ifndef EH_SIM_MAGIC_HPP
#define EH_SIM_MAGIC_HPP

#include <thumbulator/cpu.hpp>

#include "scheme/eh_scheme.hpp"
#include "scheme/data_sheet.hpp"
#include "capacitor.hpp"
#include "stats.hpp"

namespace ehsim {

/**
 * A magical checkpointing scheme.
 *
 * Does not waste any energy, because it is magic.
 */
class magical_scheme : public eh_scheme {
public:
  magical_scheme() : battery(MEMENTOS_CAPACITANCE, MEMENTOS_MAX_CAPACITOR_VOLTAGE, MEMENTOS_MAX_CURRENT)
  {
  }

  capacitor &get_battery() override
  {
    return battery;
  }

  uint32_t clock_frequency() const override
  {
    return MEMENTOS_CPU_FREQUENCY;
  }

  void execute_instruction(stats_bundle *stats) override
  {
  }

  void calculate_backup_locs(bool use_reg_lva, const std::set<uint64_t> dead_regs) override
  {
  }

  bool is_active(stats_bundle *stats) override
  {
    return true;
  }

  bool will_backup(stats_bundle *stats) override
  {
    return false;
  }

  uint64_t backup(stats_bundle *stats) override
  {
    stats->cpu.end_backup_insn = stats->cpu.instruction_count;
    // do not touch arch/app state
    return 0;
  }

  uint64_t restore(stats_bundle *stats) override
  {
    // do not touch arch/app state
    return 0;
  }

  void set_dead_addresses(const std::set<uint64_t>& dead_mem_addrs) override
  {}

  const uint32_t get_wb_buffer_size() override
  {}

  const uint64_t& get_true_positives() override
  {}

  const uint64_t& get_false_positives() override
  {}

  const uint64_t get_renamed_mappings() override
  {}

  const uint64_t get_reclaimed_mappings() override
  {}

  void reset_stats() override
  {}

  void print_map_table() override
  {}

  bool optimal_backup_scheme(uint64_t curr_insn_cycle, uint32_t address, size_t set, size_t way, bool memwr, bool memop, bool branch, bool branch_link, uint32_t num_mem_access) override
  { return false; }
  
private:
  capacitor battery;
};
}

#endif //EH_SIM_MAGIC_HPP
