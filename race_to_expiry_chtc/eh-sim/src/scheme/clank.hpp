#ifndef EH_SIM_CLANK_HPP
#define EH_SIM_CLANK_HPP

#include "scheme/eh_scheme.hpp"
#include "scheme/data_sheet.hpp"
#include "capacitor.hpp"
#include "stats.hpp"

#include <thumbulator/memory.hpp>
#include <thumbulator/cpu.hpp>
#include <unordered_map>

namespace ehsim {

/**
 * Based on Clank: Architectural Support for Intermittent Computation.
 */
class clank : public eh_scheme {
public:
  /**
   * Construct a default clank configuration.
   */
  clank() : clank(8, 8, 8, 8000)
  {
  }

  clank(size_t rf_entries, size_t wf_entries, size_t wb_entries, int watchdog_period)
      : battery(BATTERYLESS_CAPACITANCE, BATTERYLESS_MAX_CAPACITOR_VOLTAGE, MEMENTOS_MAX_CURRENT)
      , WATCHDOG_PERIOD(watchdog_period)
      , READFIRST_ENTRIES(rf_entries)
      , WRITEFIRST_ENTRIES(wf_entries)
      , WRITEBACK_ENTRIES(wb_entries)
      , progress_watchdog(WATCHDOG_PERIOD)
  {
    assert(READFIRST_ENTRIES >= 1);
    assert(WRITEFIRST_ENTRIES >= 0);

    thumbulator::ram_load_hook = [this](
        uint32_t address, uint32_t data) -> uint32_t { return this->process_read(address, data); };

    thumbulator::ram_store_hook = [this](uint32_t address, uint32_t last_value,
        uint32_t value, bool wb) -> uint32_t { return this->process_store(address, last_value, value, wb); };
  }

  capacitor &get_battery() override
  {
    return battery;
  }

  uint32_t clock_frequency() const override
  {
    return CORTEX_M0PLUS_FREQUENCY;
  }

  double min_energy_to_power_on(stats_bundle *stats) override
  {
    return battery.maximum_energy_stored();
  }

  void execute_instruction(stats_bundle *stats) override
  {
    auto const elapsed_cycles = stats->cpu.cycle_count - last_tick;
    last_tick = stats->cpu.cycle_count;

    progress_watchdog -= elapsed_cycles;

    //std::cout << "Cycle" << stats->cpu.cycle_count << ": progress_watchdog=" << progress_watchdog << std::endl;

    // clank's instruction energy is in Energy-per-Cycle
    auto const instruction_energy = CLANK_INSTRUCTION_ENERGY * elapsed_cycles + CORTEX_M0PLUS_ENERGY_FLASH;
    battery.consume_energy(instruction_energy);
    stats->models.back().energy_for_instructions += instruction_energy;
  }

  void calculate_backup_locs(bool use_reg_lva, const std::set<uint64_t> dead_regs) override
  {
    num_backup_regs = 20;

    if(use_reg_lva) {
      num_backup_regs = 7;

      for (uint64_t reg = 0; reg < 13; reg++) {
        if (dead_regs.find(reg) == dead_regs.end()) {
          if (thumbulator::cpu_get_gpr_dbit(reg)) {
            //std::cout << "r" << reg << "(L)" << std::endl;
            num_backup_regs++;
          }
        }
        //else {
        //  std::cout << "r" << reg << "(D)" << std::endl;
        //}
      }
    }

    //std::cout << "cycle " << std::dec << stats->cpu.cycle_count << ": backup -> num_backup_regs = " << num_backup_regs << std::endl;

    num_stores = write_back(false);
  }

  bool is_active(stats_bundle *stats) override
  {
    if(battery.energy_stored() >= battery.maximum_energy_stored()) {
      power_on();
    } else if(battery.energy_stored() < calculate_backup_energy()) {
      power_off();
    }

    return active;
  }

  bool will_backup(stats_bundle *stats) override
  {
    if(battery.energy_stored() < calculate_backup_energy()) {
      return false;
    }

    if(progress_watchdog <= 0) {
      //std::cout << "Cycle " << stats->cpu.cycle_count << ": progress watchdog timed off" << std::endl;
      return true;
    }

    return idempotent_violation;
  }

  uint64_t backup(stats_bundle *stats) override
  {
    stats->cpu.end_backup_insn = stats->cpu.instruction_count;
    auto &active_stats = stats->models.back();
    active_stats.num_backups++;

    auto const tau_B = stats->cpu.cycle_count - last_backup_cycle;
    active_stats.time_between_backups += tau_B;
    last_backup_cycle = stats->cpu.cycle_count;

    // reset the watchdog
    progress_watchdog = WATCHDOG_PERIOD;
    // clear idempotency-tracking buffers
    clear_buffers();
    // the backup has resolved the idempotancy violation and/or exception
    idempotent_violation = false;

    // save application state
    write_back(true);

    active_stats.energy_for_backups += calculate_backup_energy();
    battery.consume_energy(calculate_backup_energy());

    auto const backup_time = (CLANK_BACKUP_ARCH_TIME * num_backup_regs/20) + (num_stores * CLANK_MEMORY_TIME);
    active_stats.bytes_application += static_cast<double>(num_stores * 4) / tau_B;

    thumbulator::cpu_clear_gpr_dbit();

    // save architectural state
    architectural_state = thumbulator::cpu;

    return backup_time;
  }

  uint64_t restore(stats_bundle *stats) override
  {
    progress_watchdog = WATCHDOG_PERIOD;
    last_backup_cycle = stats->cpu.cycle_count;

    // restore saved architectural state
    thumbulator::cpu_reset();
    thumbulator::cpu = architectural_state;

    stats->models.back().energy_for_restore = CLANK_RESTORE_ENERGY;
    battery.consume_energy(CLANK_RESTORE_ENERGY);

    // assume memory access latency for reads and writes is the same
    return CLANK_BACKUP_ARCH_TIME;
  }

  double estimate_progress(eh_model_parameters const &eh) const override
  {
    return estimate_eh_progress(eh, dead_cycles::average_case, CLANK_OMEGA_R, CLANK_SIGMA_R, CLANK_A_R,
        CLANK_OMEGA_B, CLANK_SIGMA_B, CLANK_A_B);
  }

  const uint32_t get_wb_buffer_size() override
  {
    return writeback_buffer.size();
  }

  const uint64_t& get_true_positives() override
  {}

  const uint64_t& get_false_positives() override
  {}

  const uint64_t get_renamed_mappings() override
  {}

  const uint64_t get_reclaimed_mappings() override
  {}

  void set_dead_addresses(const std::set<uint64_t>& dead_mem_addrs) override
  {
    dead_mem_locs = dead_mem_addrs;
  }

  void reset_stats() override
  {}

  void print_map_table() override
  {}

  bool optimal_backup_scheme(uint64_t curr_insn_cycle, uint32_t address, size_t set, size_t way, bool memwr, bool memop, bool branch, bool branch_link, uint32_t num_mem_access) override
  { return false; }

private:
  capacitor battery;

  uint64_t last_backup_cycle = 0u;
  uint64_t last_tick = 0u;

  thumbulator::cpu_state architectural_state{};
  bool active = false;

  int const WATCHDOG_PERIOD;
  size_t const READFIRST_ENTRIES;
  size_t const WRITEFIRST_ENTRIES;
  size_t const WRITEBACK_ENTRIES;

  uint8_t  num_backup_regs = 0;
  uint32_t num_stores = 0;
  int progress_watchdog = 0;
  bool idempotent_violation = false;

  // class object for writeback buffer entry
  class wb_entry {
    public:
      wb_entry() : data(0), liveness(0)
      {
      }

      wb_entry(uint32_t data, bool liveness)
      {
        data = data;
        liveness = liveness;
      }

      void set_data(const uint32_t& data)     { this->data = data;}
      void set_liveness(const bool& liveness) { this->liveness = liveness; }
      uint32_t get_data() const        { return data; }
      bool get_liveness() const        { return liveness; }

    private:
      uint32_t data;
      bool     liveness;
  };

  std::set<uint32_t> readfirst_buffer;
  std::set<uint32_t> writefirst_buffer;
  std::unordered_map<uint32_t, wb_entry> writeback_buffer;
  std::set<uint64_t> dead_mem_locs;

  enum class operation { read, write };

  void clear_buffers()
  {
    readfirst_buffer.clear();
    writefirst_buffer.clear();
  }

  void power_on()
  {
    active = true;
  }

  void power_off()
  {
    // std::cout << "POWER OFF" << std::endl;
    active = false;
    clear_buffers();
    if(!writeback_buffer.empty()) {  
      writeback_buffer.clear();
    }
  }

  bool try_insert(std::set<uint32_t> *buffer, uint32_t const address, size_t const max_buffer_size)
  {
    if(buffer->size() < max_buffer_size) {
      buffer->insert(address);

      return true;
    }

    return false;
  }

  bool try_insert(std::unordered_map<uint32_t, wb_entry> *buffer, uint32_t const address, bool const liveness, uint32_t const data, size_t const max_buffer_size)
  {
    wb_entry entry;
    if(buffer->size() < max_buffer_size) {
      entry.set_data(data);
      entry.set_liveness(liveness);
      buffer->emplace(address, entry);

      return true;
    }

    return false;
  }

  /**
   * Detection logic for idempotency violations.
   */
  void detect_violation(uint32_t address, operation op, uint32_t old_value, uint32_t &value)
  {
    auto const readfirst_it = readfirst_buffer.find(address);
    auto const readfirst_hit = readfirst_it != readfirst_buffer.end();

    auto const writefirst_it = writefirst_buffer.find(address);
    auto const writefirst_hit = writefirst_it != writefirst_buffer.end();

    auto const writeback_it = writeback_buffer.find(address);
    auto const writeback_hit = writeback_it != writeback_buffer.end(); 

    auto const dead_loc_it = dead_mem_locs.find(address);
    auto const dead_loc_hit = dead_loc_it != dead_mem_locs.end();

    //std::cout << "address=0x" << std::hex << address 
    //          << " readfirst_hit=" << readfirst_hit
    //          << " writefirst_hit=" << writefirst_hit
    //          << " writeback_hit=" << writeback_hit 
    //          << " op=" << (op == operation::read) << std::endl;

    // if(!dead_mem_locs.empty()) {
    //   std::cout << "*****************DEAD MEM LOCATIONS******************" << std:: endl; 
    //   for (auto it=dead_mem_locs.begin(); it!=dead_mem_locs.end(); it++)
    //     std::cout << std::hex << *it << std::endl;
    // }

    // read
    if(op == operation::read) {
      // check if address is in writefirst buffer
      // if hit -> do nothing
      if(writefirst_hit) {
        return;
      }
      // check if address is in writeback buffer
      // if hit -> forward data from writeback buffer
      if(writeback_hit) {
        value = writeback_it->second.get_data();
	// std::cout << "writeback: value=" << std::hex << value << std::endl;
        return;
      }

      // Otherwise check if address is in readfirst buffer
      // if hit -> do nothing
      // else -> add address to buffer
      // if buffer is full -> flag idempotent violation
      if(!readfirst_hit) {
        bool was_added = false;
        was_added = try_insert(&readfirst_buffer, address, READFIRST_ENTRIES);

        if(!was_added) {
          // std::cout << "pc=0x" << std::hex << thumbulator::cpu_get_pc() << " (idempotent violation) RF BUF full: addr=0x" << std::hex << address << std::endl;
          idempotent_violation = true;
        }
        else {
          // std::cout << "pc=0x" << std::hex << thumbulator::cpu_get_pc() << " RF BUF: addr=0x" << std::hex << address << std::endl;
        }
      }
    }
    // write
    else if(op == operation::write) {
      // check if address is in writeback buffer
      // if hit -> change data value in buffer
      if(writeback_hit) {
        writeback_it->second.set_data(value);
        return;
      }

      // Otherwise check if address is in readfirst buffer
      // if hit -> idempotent violation -> check for free entries in writeback buffer
      // if free entries available add address to writeback buffer -> remove address from readfirst buffer
      // else writeback buffer is full -> flag idempotent violation
      if(readfirst_hit) {
        if(WRITEBACK_ENTRIES > 0) {
          bool was_added = false;
          was_added = try_insert(&writeback_buffer, address, !dead_loc_hit, value, WRITEBACK_ENTRIES);

          if(!was_added) {
            // std::cout << "pc=0x" << std::hex << thumbulator::cpu_get_pc() << " (idempotent violation) WB BUF full: addr=0x" << std::hex << address << std::endl;
            idempotent_violation = true;
          }
          else {
            // std::cout << "pc=0x" << std::hex << thumbulator::cpu_get_pc() << " WB BUF: addr=0x" << std::hex << address << " data=0x" << value << std::endl;
            readfirst_buffer.erase(readfirst_it);
          }
        }
        else {
          // std::cout << "pc=0x" << std::hex << thumbulator::cpu_get_pc() << " (idempotent violation) No WB BUF: addr=0x" << std::hex << address << std::endl;
          idempotent_violation = true;  
        }
        return;
      }

      // Otherwise check if address is in writefirst buffer
      // if hit -> do nothing
      // else -> add address to buffer
      // if buffer is full -> flag idempotent violation
      if(!writefirst_hit) {
        bool was_added = false;
        was_added = try_insert(&writefirst_buffer, address, WRITEFIRST_ENTRIES);

        if(!was_added) {
          // std::cout << "pc=0x" << std::hex << thumbulator::cpu_get_pc() << " (idempotent violation) WF BUF full: addr=0x" << std::hex << address << std::endl;
          idempotent_violation = true;
        }
        else {
          // std::cout << "pc=0x" << std::hex << thumbulator::cpu_get_pc() << " WF BUF: addr=0x" << std::hex << address << std::endl;
        }
      } 
    }
  }

  uint32_t process_read(uint32_t address, uint32_t value)
  {
    detect_violation(address, operation::read, value, value);

    if(idempotent_violation && battery.energy_stored() < calculate_backup_energy()) {
      power_off();
    }

    return value;
  }

  uint32_t process_store(uint32_t address, uint32_t old_value, uint32_t value, bool wb)
  {
    if(!wb) {
      detect_violation(address, operation::write, old_value, value);
    }

    if(idempotent_violation && battery.energy_stored() < calculate_backup_energy()) {
      power_off();

      return old_value;
    }

    return value;
  }

  double calculate_backup_energy() const
  {
    return (CLANK_BACKUP_ARCH_ENERGY * num_backup_regs/20) + (num_stores * 4 * CORTEX_M0PLUS_ENERGY_FLASH);
  }

  // if wb=1 write back to memory, else return count of stores
  const size_t write_back(bool write_back)
  {
    auto count = 0;

    for(auto const &wb : writeback_buffer) {
      if(!wb.second.get_liveness())
        continue;
      count++;
      if(write_back) {
        thumbulator::store(wb.first, wb.second.get_data(), true);
      }
    }

    if(write_back) {
      writeback_buffer.clear();
    }

    return count;
  }
};
}

#endif //EH_SIM_CLANK_HPP
