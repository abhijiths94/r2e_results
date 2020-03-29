#ifndef EH_SIM_BACKUP_EVERY_CYCLE_HPP
#define EH_SIM_BACKUP_EVERY_CYCLE_HPP

#include "scheme/eh_scheme.hpp"
#include "scheme/data_sheet.hpp"
#include "scheme/eh_model.hpp"
#include "capacitor.hpp"

namespace ehsim {

/**
 * Based on Architecture Exploration for Ambient Energy Harvesting Nonvolatile Processors.
 *
 * See the data relating to the BEC scheme.
 */
class backup_every_cycle : public eh_scheme {
public:
  backup_every_cycle() : battery(NVP_CAPACITANCE, MEMENTOS_MAX_CAPACITOR_VOLTAGE, MEMENTOS_MAX_CURRENT)
  {}

  capacitor &get_battery() override
  {
    return battery;
  }

  uint32_t clock_frequency() const override
  {
    return NVP_CPU_FREQUENCY;
  }

  double min_energy_to_power_on(stats_bundle *stats) override
  {
    auto required_energy = NVP_INSTRUCTION_ENERGY + NVP_BEC_BACKUP_ENERGY;

    if(stats->cpu.instruction_count != 0) {
      // we only need to restore if an instruction has been executed
      required_energy += NVP_BEC_RESTORE_ENERGY;
    }
    return required_energy;
  }

  void execute_instruction(stats_bundle *stats) override
  {
    battery.consume_energy(NVP_INSTRUCTION_ENERGY);

    stats->models.back().energy_for_instructions += NVP_INSTRUCTION_ENERGY;
  }

  void calculate_backup_locs(bool use_reg_lva, const std::set<uint64_t> dead_regs) override
  {
  }

  bool is_active(stats_bundle *stats) override
  {
    auto required_energy = NVP_INSTRUCTION_ENERGY + NVP_BEC_BACKUP_ENERGY;

    if(stats->cpu.instruction_count != 0) {
      // we only need to restore if an instruction has been executed
      required_energy += NVP_BEC_RESTORE_ENERGY;
    }

    return battery.energy_stored() > required_energy;
  }

  bool will_backup(stats_bundle *stats) override
  {
    return true;
  }

  uint64_t backup(stats_bundle *stats) override
  {
    stats->cpu.end_backup_insn = stats->cpu.instruction_count;
    // do not touch arch/app state, assume it is all non-volatile
    auto &active_stats = stats->models.back();
    active_stats.num_backups++;

    active_stats.time_between_backups += stats->cpu.cycle_count - last_backup_cycle;
    last_backup_cycle = stats->cpu.cycle_count;

    active_stats.energy_for_backups += NVP_BEC_BACKUP_ENERGY;
    battery.consume_energy(NVP_BEC_BACKUP_ENERGY);

    return NVP_BEC_BACKUP_TIME;
  }

  uint64_t restore(stats_bundle *stats) override
  {
    last_backup_cycle = stats->cpu.cycle_count;

    // do not touch arch/app state, assume it is all non-volatile

    stats->models.back().energy_for_restore = NVP_BEC_RESTORE_ENERGY;
    battery.consume_energy(NVP_BEC_RESTORE_ENERGY);

    return NVP_BEC_RESTORE_TIME;
  }

  double estimate_progress(eh_model_parameters const &eh) const override
  {
    return estimate_eh_progress(eh, dead_cycles::best_case, NVP_BEC_OMEGA_R, NVP_BEC_SIGMA_R, NVP_BEC_A_R,
        NVP_BEC_OMEGA_B, NVP_BEC_SIGMA_B, NVP_BEC_A_B);
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

  uint64_t last_backup_cycle = 0u;
};
}

#endif //EH_SIM_BACKUP_EVERY_CYCLE_HPP
