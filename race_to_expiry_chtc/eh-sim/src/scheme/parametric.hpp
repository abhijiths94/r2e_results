#ifndef EH_SIM_PARAMETRIC_HPP
#define EH_SIM_PARAMETRIC_HPP

#include "scheme/eh_scheme.hpp"
#include "scheme/data_sheet.hpp"
#include "capacitor.hpp"
#include "stats.hpp"

#include <thumbulator/memory.hpp>

namespace ehsim {

class parametric : public eh_scheme {
public:
  explicit parametric(int backup_period)
      : battery(MEMENTOS_CAPACITANCE, MEMENTOS_MAX_CAPACITOR_VOLTAGE, MEMENTOS_MAX_CURRENT)
      , BACKUP_PERIOD(backup_period)
      , countdown_to_backup(BACKUP_PERIOD)
  {
    thumbulator::ram_load_hook = [this](
        uint32_t address, uint32_t data) -> uint32_t { return this->process_read(address, data); };

    thumbulator::ram_store_hook = [this](uint32_t address, uint32_t last_value,
        uint32_t value, bool wb) -> uint32_t { return this->process_store(address, last_value, value); };
  }

  capacitor &get_battery() override
  {
    return battery;
  }

  uint32_t clock_frequency() const override
  {
    return MEMENTOS_CPU_FREQUENCY;
  }

  double min_energy_to_power_on(stats_bundle *stats) override
  {
    return battery.maximum_energy_stored();
  }

  void execute_instruction(stats_bundle *stats) override
  {
    battery.consume_energy(CLANK_INSTRUCTION_ENERGY);
    stats->models.back().energy_for_instructions += CLANK_INSTRUCTION_ENERGY;

    countdown_to_backup -= stats->cpu.cycle_count - last_tick;
    last_tick = stats->cpu.cycle_count;
  }

  void calculate_backup_locs(bool use_reg_lva, const std::set<uint64_t> dead_regs) override
  {
  }

  bool is_active(stats_bundle *stats) override
  {
    if(battery.energy_stored() == battery.maximum_energy_stored()) {
      power_on();
    } else if(battery.energy_stored() < calculate_backup_energy()) {
      power_off();
    } else if(countdown_to_backup < 0) {
      // the countdown continues to decrease even if there is not enough energy
      // to backup. Once the countdown is negative, we should power off and wait
      // for more energy.
      power_off();
    }

    return active;
  }

  bool will_backup(stats_bundle *stats) override
  {
    if(battery.energy_stored() < calculate_backup_energy()) {
      return false;
    }

    return countdown_to_backup <= 0;
  }

  uint64_t backup(stats_bundle *stats) override
  {
    stats->cpu.end_backup_insn = stats->cpu.instruction_count;	  
    auto &active_stats = stats->models.back();
    active_stats.num_backups++;

    auto const tau_B = stats->cpu.cycle_count - last_backup_cycle;
    active_stats.time_between_backups += tau_B;
    last_backup_cycle = stats->cpu.cycle_count;

    auto const backup_energy = calculate_backup_energy();
    active_stats.energy_for_backups += backup_energy;
    battery.consume_energy(backup_energy);

    // reset countdown
    countdown_to_backup = BACKUP_PERIOD;
    // save architectural state
    architectural_state = thumbulator::cpu;
    // save application state
    auto const num_stores = write_back();

    auto const backup_time = CLANK_BACKUP_ARCH_TIME + (num_stores * CLANK_MEMORY_TIME);
    active_stats.bytes_application += static_cast<double>(num_stores * 4) / tau_B;
    return backup_time;
  }

  uint64_t restore(stats_bundle *stats) override
  {
    // reset countdown
    countdown_to_backup = BACKUP_PERIOD;
    last_backup_cycle = stats->cpu.cycle_count;

    // restore saved architectural state
    thumbulator::cpu_reset();
    thumbulator::cpu = architectural_state;

    stats->models.back().energy_for_restore = CLANK_RESTORE_ENERGY;
    battery.consume_energy(CLANK_RESTORE_ENERGY);

    return CLANK_BACKUP_ARCH_TIME;
  }

  double estimate_progress(eh_model_parameters const &eh) const override
  {
    return estimate_eh_progress(eh, dead_cycles::average_case, PARAMETRIC_OMEGA_R,
        PARAMETRIC_SIGMA_R, PARAMETRIC_A_R, PARAMETRIC_OMEGA_B, PARAMETRIC_SIGMA_B, PARAMETRIC_A_B);
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
  bool active = false;

  uint64_t last_backup_cycle = 0u;
  uint64_t last_tick = 0u;

  int const BACKUP_PERIOD;
  int countdown_to_backup;

  thumbulator::cpu_state architectural_state{};
  std::unordered_map<uint32_t, uint32_t> stores;

  void power_on()
  {
    active = true;
  }

  void power_off()
  {
    active = false;
    stores.clear();
  }

  double calculate_backup_energy() const
  {
    return CLANK_BACKUP_ARCH_ENERGY + (stores.size() * 4 * CORTEX_M0PLUS_ENERGY_FLASH);
  }

  size_t write_back()
  {
    auto const count = stores.size();

    for(auto const &store : stores) {
      thumbulator::RAM[(store.first & RAM_ADDRESS_MASK) >> 2] = store.second;
    }
    stores.clear();

    return count;
  }

  uint32_t process_read(uint32_t address, uint32_t value)
  {
    auto it = stores.find(address);
    if(it != stores.end()) {
      return it->second;
    }

    return value;
  }

  uint32_t process_store(uint32_t address, uint32_t old_value, uint32_t value)
  {
    auto it = stores.find(address);
    if(it != stores.end()) {
      it->second = value;
    } else {
      stores.emplace(address, value);
    }

    return old_value;
  }
};
}

#endif //EH_SIM_PARAMETRIC_HPP
