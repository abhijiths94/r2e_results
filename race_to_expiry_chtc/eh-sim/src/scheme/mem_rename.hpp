#ifndef EH_SIM_MEM_RENAME_HPP
#define EH_SIM_MEM_RENAME_HPP

#include "scheme/eh_scheme.hpp"
#include "scheme/data_sheet.hpp"
#include "capacitor.hpp"
#include "stats.hpp"

#include <unordered_set>
#include <thumbulator/memory.hpp>
#include <thumbulator/cpu.hpp>
#include <bf/bloom_filter/basic.hpp>


namespace ehsim {

/**
 * Based on Clank: Architectural Support for Intermittent Computation.
 *
 * Only implements the read- and write-first buffers.
 */
class mem_rename : public eh_scheme {
public:
  /**
   * Construct a default mem_rename configuration.
   */
  mem_rename() : mem_rename(8,
                            16,
                            8000,
                            1,
                            64,
                            64,
                            1,
                            64,
                            512,
			                      false,
                            false,
			                      false,
                            4,
                            8,
                            0,
                            0,
                            0,
                            4.7e-13,
                            5.11e-13,
                            1.21e-3,
                            0,
                            0,
                            0,
                            0,
                            0,
                            0,
                      	    0,
                      	    0,
                      	    0,
                      	    0)
  {
  }

  mem_rename(size_t   rf_entries,
             size_t   lbf_size,
             int      watchdog_period,
             size_t   icache_assoc,
             uint32_t icache_block_size,
             uint32_t icache_size,
             size_t   dcache_assoc,
             uint32_t dcache_block_size,
             uint32_t dcache_size,
	           bool     use_optimal_backup_scheme,
             bool     add_renamer,
	           bool     reclaim_addr,
             size_t   map_table_entries,
             uint32_t num_avail_rename_addrs, 
             double   icache_read_energy,
             double   icache_write_energy,
             double   icache_leakage_power,
             double   dcache_read_energy,
             double   dcache_write_energy,
             double   dcache_leakage_power,
             double   rf_access_energy,
             double   rf_leakage_power,
             double   lbf_access_energy,
             double   lbf_leakage_power,
      	     double   map_table_access_energy,
      	     double   map_table_read_energy,
      	     double   map_table_write_energy,
      	     double   map_table_leakage_power,
      	     double   free_list_read_energy,
      	     double   free_list_leakage_power)
             : battery(BATTERYLESS_CAPACITANCE, BATTERYLESS_MAX_CAPACITOR_VOLTAGE, MEMENTOS_MAX_CURRENT)
             , WATCHDOG_PERIOD(watchdog_period)
             , READFIRST_ENTRIES(rf_entries)
             , IASSOC(icache_assoc)
             , IBLOCK_SIZE(icache_block_size)
             , ICACHE_SIZE(icache_size)
             , DASSOC(dcache_assoc)
             , DBLOCK_SIZE(dcache_block_size)
             , DCACHE_SIZE(dcache_size)
             , progress_watchdog(WATCHDOG_PERIOD)
             , MEM_RENAME_ICACHE_READ_ENERGY(icache_read_energy)
             , MEM_RENAME_ICACHE_WRITE_ENERGY(icache_write_energy)
             , MEM_RENAME_ICACHE_LEAKAGE_POWER(icache_leakage_power)
             , MEM_RENAME_DCACHE_READ_ENERGY(dcache_read_energy)
             , MEM_RENAME_DCACHE_WRITE_ENERGY(dcache_write_energy)
             , MEM_RENAME_DCACHE_LEAKAGE_POWER(dcache_leakage_power)
             , MEM_RENAME_RF_ACCESS_ENERGY(rf_access_energy)
             , MEM_RENAME_RF_LEAKAGE_POWER(rf_leakage_power)
             , LOCAL_BLOOMFILTER_ACCESS_ENERGY(lbf_access_energy)
             , LOCAL_BLOOMFILTER_LEAKAGE_POWER(lbf_leakage_power)
             , MAP_TABLE_ACCESS_ENERGY(map_table_access_energy)
             , MAP_TABLE_READ_ENERGY(map_table_read_energy)
	           , MAP_TABLE_WRITE_ENERGY(map_table_write_energy)
             , MAP_TABLE_LEAKAGE_POWER(map_table_leakage_power)
             , FREE_LIST_READ_ENERGY(free_list_read_energy)
             , FREE_LIST_LEAKAGE_POWER(free_list_leakage_power)
  {
    if(READFIRST_ENTRIES > 0) {
      readfirst_filter = std::unique_ptr<bf::basic_bloom_filter>(new bf::basic_bloom_filter(0.25, READFIRST_ENTRIES, 1, false, false));
    }

    insn_cache = std::make_shared<thumbulator::cache>(icache_assoc, icache_block_size, icache_size, 0);
    thumbulator::icache = insn_cache;

    data_cache = std::make_shared<thumbulator::cache>(dcache_assoc, dcache_block_size, dcache_size, lbf_size);
    thumbulator::dcache = data_cache;

    // victim_data = new uint32_t[dcache_block_size];

    thumbulator::OPTIMAL_BACKUP_POLICY = use_optimal_backup_scheme;

    if(add_renamer) {
      mem_renamer = std::make_shared<thumbulator::rename>(map_table_entries, num_avail_rename_addrs, reclaim_addr);
      thumbulator::renamer = mem_renamer;
    }

    thumbulator::cache_load_hook = [this](
        thumbulator::cache_block& blk, uint32_t address, bool lbf, size_t set, size_t way) -> bool { return this->process_cache_read(blk, address, lbf, set, way); };

    thumbulator::cache_store_hook = [this](
        thumbulator::cache_block& blk, uint32_t address, bool lbf, size_t set, size_t way, bool& gbf_hit) -> bool { return this->process_cache_write(blk, address, lbf, set, way, gbf_hit); };

    thumbulator::ram_load_hook = [this](
        uint32_t address, uint32_t data) -> uint32_t { return this->process_ram_load(address, data); };

    thumbulator::ram_store_hook = [this](uint32_t address, uint32_t last_value,
        uint32_t value, bool backup) -> uint32_t { return this->process_ram_store(address, last_value, value, backup); };          
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
    // ----------- This section of code is only for collecting stats related to the map table -----------	  
    if(mem_renamer) {
      if(last_tick == 0)
        // std::cout << "Cycle " << std::dec << last_tick << ": " << curr_num_entries << std::endl;
      curr_num_entries = mem_renamer->get_num_valid_entries();
      if(curr_num_entries > last_num_entries) {
        // std::cout << "Cycle " << std::dec << last_tick << ": " << curr_num_entries << std::endl;
        last_num_entries = curr_num_entries;
      }
    }
    // --------------------------------------------------------------------------------------------------


    auto const insn_cycles = stats->cpu.cycle_count - last_tick;
    // mem_rename's instruction/cache leakage energy is in Energy-per-Cycle
    auto instruction_fetch_energy = 0;
    if(thumbulator::icache_hit) {
    	instruction_fetch_energy =  MEM_RENAME_ICACHE_READ_ENERGY;
    }
    else {
    	instruction_fetch_energy = (IBLOCK_SIZE >> 2) * CORTEX_M0PLUS_ENERGY_FLASH + MEM_RENAME_ICACHE_WRITE_ENERGY;
        stats->cpu.cycle_count += ((IBLOCK_SIZE >> 2) * 2);
    }

    auto const elapsed_cycles = stats->cpu.cycle_count - last_tick;
    last_tick = stats->cpu.cycle_count;

    auto const instruction_energy       = CLANK_INSTRUCTION_ENERGY * insn_cycles;
    auto const cache_leakage_energy     = (MEM_RENAME_ICACHE_LEAKAGE_POWER + MEM_RENAME_DCACHE_LEAKAGE_POWER) / clock_frequency() * elapsed_cycles;
    auto const rf_buffer_leakage_energy = MEM_RENAME_RF_LEAKAGE_POWER / clock_frequency() * elapsed_cycles;
    auto const lbf_leakage_energy       = LOCAL_BLOOMFILTER_LEAKAGE_POWER / clock_frequency() * elapsed_cycles;
    auto const map_table_leakage_energy = 0;
    auto const free_list_leakage_energy = 0;

    if(mem_renamer) {
      MAP_TABLE_LEAKAGE_POWER / clock_frequency() * elapsed_cycles;
      FREE_LIST_LEAKAGE_POWER / clock_frequency() * elapsed_cycles;
    }

    auto const op_energy = instruction_energy + cache_leakage_energy 
                           + cache_energy_per_insn + mem_access_energy 
                           + rf_buffer_access_energy + rf_buffer_leakage_energy
                           + lbf_leakage_energy // lbf access energy is included in cache_energy_per_insn (see process_cache_read/write functions)
			                     + rename_overhead_energy + map_table_leakage_energy 
			                     + free_list_leakage_energy;

    if(continuous_power_supply) {
      stats->models.back().energy_for_instructions += op_energy;
    }

    if(battery.energy_stored() <= op_energy) {
      battery.consume_energy(battery.energy_stored());
      stats->models.back().energy_for_instructions += battery.energy_stored();
    }
    else {
      battery.consume_energy(op_energy);
      stats->models.back().energy_for_instructions += op_energy;
    }  

    progress_watchdog -= elapsed_cycles;
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
      // std::cout << "cycle " << std::dec << stats->cpu.cycle_count << " POWER ON" << std::endl;
    } else if(battery.energy_stored() < calculate_backup_energy()) {
      if(active)
      {
        std::cout << "cycle " << std::dec << stats->cpu.cycle_count << ": POWER OFF - is_active()" << std::endl;
      }
      power_off();
    }

    return active;
  }

  bool will_backup(stats_bundle *stats) override
  {
    if(progress_watchdog <= 0 && !thumbulator::OPTIMAL_BACKUP_POLICY) {
        
      std::cout << "cycle " << std::dec << stats->cpu.cycle_count << ": progress watchdog timed off" << std::endl;
	  return false;
    }

    if(mem_renamer && thumbulator::OPTIMAL_BACKUP_POLICY) {
      return false;
    }

    if(continuous_power_supply) {
      return idempotent_violation;
    }

    if((battery.energy_stored() < calculate_backup_energy()) && idempotent_violation) {
      std::cout << "cycle " << std::dec << stats->cpu.cycle_count << ": battery energy not enough for backup" << std::endl;
      power_off();
      return false;
    }

    if(battery.energy_stored() == 0) {
      std::cout << "cycle " << std::dec << stats->cpu.cycle_count << ": battery energy drained" << std::endl;
      power_off();
      return false;
    }

    if(idempotent_violation) {
      std::cout << "cycle " << std::dec << stats->cpu.cycle_count << ": backup (idempotent_violation)" << std::endl;
    }
    return idempotent_violation;
  }

  uint64_t backup(stats_bundle *stats) override
  {
    stats->cpu.end_backup_insn = stats->cpu.instruction_count_forward_progress;
    // std::cout << "Cycle " << stats->cpu.cycle_count << ": end_backup_insn = " << std::dec << stats->cpu.end_backup_insn << std::endl;
    auto &active_stats = stats->models.back();
    active_stats.num_backups++;
    if(!mem_renamer && thumbulator::OPTIMAL_BACKUP_POLICY && idempotent_violation) {
      active_stats.num_id_backups++;
    }

    auto const tau_B = stats->cpu.cycle_count - last_backup_cycle;
    active_stats.time_between_backups += tau_B;
    last_backup_cycle = stats->cpu.cycle_count;

    // reset the watchdog
    progress_watchdog = WATCHDOG_PERIOD;

    // clear idempotency-tracking buffers
    clear_buffers();

    // save application state
    write_back(true);

    if(!active) {
      return 0;
    }

    if(!continuous_power_supply) {
      battery.consume_energy(calculate_backup_energy());
    }

    active_stats.energy_for_backups += calculate_backup_energy();

    // the backup has resolved the idempotancy violation and/or exception
    idempotent_violation = false;

    uint32_t map_table_backup_words = 0;
    // backup map table
    if(mem_renamer) {
      map_table_backup_words = mem_renamer->get_num_backup_entries() * 11/4; 
      mem_renamer->backup_map_table();
    }

    auto const backup_time = (CLANK_BACKUP_ARCH_TIME * num_backup_regs/20) + (num_stores * DBLOCK_SIZE / 4 + map_table_backup_words) * CLANK_MEMORY_TIME;
    active_stats.bytes_application += static_cast<double>((num_stores * DBLOCK_SIZE / 4) * 4) / tau_B;

    num_backup_regs = 0;
    num_stores = 0;
    num_map_table_backup_hits = 0;

    thumbulator::cpu_clear_gpr_dbit();

    // save architectural state
    architectural_state = thumbulator::cpu;

    return backup_time;
  }

  uint64_t restore(stats_bundle *stats) override
  {
    // std::cout << "Cycle " << stats->cpu.cycle_count << ": restore" << std::endl;
    progress_watchdog = WATCHDOG_PERIOD;
    idempotent_violation = false;

    insn_cache->flush();
    data_cache->flush();

    // restore saved architectural state
    thumbulator::cpu_reset();

    if(last_backup_cycle > 0) {
      thumbulator::cpu = architectural_state;
    }
    else {
      thumbulator::cpu_set_pc(thumbulator::cpu_get_pc() + 0x4);
    }

    // restore map table
    uint32_t num_map_table_restores = 0;
    if(mem_renamer) {
      num_map_table_restores = mem_renamer->restore_map_table();
    }

    stats->models.back().energy_for_restore = CLANK_RESTORE_ENERGY + num_map_table_restores * 3 * CORTEX_M0PLUS_ENERGY_FLASH; // reading (tag + old_name + new_name) will require reading three 4 byte data
    if(!continuous_power_supply) {
      battery.consume_energy(CLANK_RESTORE_ENERGY + num_map_table_restores * 3 * CORTEX_M0PLUS_ENERGY_FLASH);
    }

    // assume memory access latency for reads and writes is the same
    return CLANK_BACKUP_ARCH_TIME + 2 * num_map_table_restores * 3;
  }

  double estimate_progress(eh_model_parameters const &eh) const override
  {
    return estimate_eh_progress(eh, dead_cycles::average_case, CLANK_OMEGA_R, CLANK_SIGMA_R, CLANK_A_R,
        CLANK_OMEGA_B, CLANK_SIGMA_B, CLANK_A_B);
  }

  const uint32_t get_wb_buffer_size() override
  {}

  void set_dead_addresses(const std::set<uint64_t>& dead_mem_addrs) override
  {
    dead_mem_locs = dead_mem_addrs;
  }

  void reset_stats() override
  {
    cache_energy_per_insn = 0;
    mem_access_energy = 0;
    rf_buffer_access_energy = 0;
    rename_overhead_energy = 0; 
  }

  const uint64_t& get_true_positives() override
  {
    return true_positives;
  }

  const uint64_t& get_false_positives() override
  {
    return false_positives;
  }

  const uint64_t get_reclaimed_mappings() override
  {
    if(mem_renamer)
    	return mem_renamer->num_reclaimed_mappings();
    return 0;
  }

  const uint64_t get_renamed_mappings() override
  {
    if(mem_renamer)
    	return mem_renamer->num_renamed_mappings();
    return 0;
  }

  void print_map_table() override
  {
    mem_renamer->print();     
  }

  bool optimal_backup_scheme(uint64_t curr_insn_cycle, uint32_t address, size_t set, size_t way, bool memwr, bool memop, bool branch, bool branch_link, uint32_t num_mem_access) override
  {
    // auto insn_cycles = 0;
    // auto instruction_fetch_energy = 0;
    // auto need_renaming = false;

    // if(thumbulator::icache_hit) {
    //   curr_insn_cycle += 1;
    //   instruction_fetch_energy = MEM_RENAME_ICACHE_READ_ENERGY;
    // }
    // else {
    //   curr_insn_cycle = (IBLOCK_SIZE >> 2) + 1;
    //   instruction_fetch_energy = (IBLOCK_SIZE >> 2) * (CORTEX_M0PLUS_ENERGY_FLASH + MEM_RENAME_ICACHE_WRITE_ENERGY);
    // }


    // insn_cycles = 1;

    // if(memop) { // store/load instruction
    //   if(thumbulator::dcache_hit) {
    //     insn_cycles = num_mem_access * 1;
    //     if(memwr) 
    //       cache_energy_per_insn = num_mem_access * (MEM_RENAME_DCACHE_WRITE_ENERGY + 2 * LOCAL_BLOOMFILTER_ACCESS_ENERGY);
    //     else
    //       cache_energy_per_insn = num_mem_access * (MEM_RENAME_DCACHE_READ_ENERGY + 2 * LOCAL_BLOOMFILTER_ACCESS_ENERGY);
    //   }
    //   else {
    //     insn_cycles = num_mem_access * ((DBLOCK_SIZE >> 2) + 1);
    //     cache_energy_per_insn = num_mem_access * (MEM_RENAME_DCACHE_WRITE_ENERGY + (DBLOCK_SIZE >> 2) * LOCAL_BLOOMFILTER_ACCESS_ENERGY);
    //     mem_access_energy = num_mem_access * (DBLOCK_SIZE >> 2) * CORTEX_M0PLUS_ENERGY_FLASH;

	   //    thumbulator::cache_attributes attr;
    //   	attr.set = set;
	   //    attr.way = way;
    //     auto load_addr = address & (~data_cache->get_block_mask());
	   //    auto victim    = data_cache->get_victim(attr);
	   //    auto lbf       = false;

    //     if(victim.get_valid()) { 
    //       lbf = data_cache->get_block_state(attr.set, attr.way);
    //     }

    //     operation op = operation::read;
    //     if(memwr) {
    //       op = operation::write;
    //       num_stores += num_mem_access;
    //     }

    //     need_renaming = detect_violation(victim, load_addr, lbf, attr.set, attr.way, op, true);
    //   	if(mem_renamer) {          
    //   	  if(need_renaming) {
    //   	    rename_overhead_energy = num_mem_access * (MAP_TABLE_WRITE_ENERGY + FREE_LIST_READ_ENERGY);
    //         if(memwr) {
    //           num_map_table_backup_hits += num_mem_access;
    //         }
    //   	  }
    //   	}
    //   }
    // }

    // if(branch)
    //   insn_cycles = 2;

    // if(branch_link)
    //   insn_cycles = 3;

    // curr_insn_cycle += insn_cycles;

    // auto const instruction_energy       = CLANK_INSTRUCTION_ENERGY * insn_cycles;
    // auto const cache_leakage_energy     = (MEM_RENAME_ICACHE_LEAKAGE_POWER + MEM_RENAME_DCACHE_LEAKAGE_POWER) / clock_frequency() * curr_insn_cycle;
    // auto const rf_buffer_leakage_energy = MEM_RENAME_RF_LEAKAGE_POWER / clock_frequency() * curr_insn_cycle;
    // auto const lbf_leakage_energy       = LOCAL_BLOOMFILTER_LEAKAGE_POWER / clock_frequency() * curr_insn_cycle;
    // auto map_table_leakage_energy       = 0;
    // auto free_list_leakage_energy       = 0;

    // if(mem_renamer) {
    //   map_table_leakage_energy = MAP_TABLE_LEAKAGE_POWER / clock_frequency() * curr_insn_cycle;
    //   free_list_leakage_energy = FREE_LIST_LEAKAGE_POWER / clock_frequency() * curr_insn_cycle;
    // }

    // auto const op_energy = instruction_energy + cache_leakage_energy 
    //                        + cache_energy_per_insn + mem_access_energy 
    //                        + rf_buffer_access_energy + rf_buffer_leakage_energy
    //                        + lbf_leakage_energy // lbf access energy is included in cache_energy_per_insn (see process_cache_read/write functions)
    // 			                 + rename_overhead_energy + map_table_leakage_energy 
    // 			                 + free_list_leakage_energy;	

    // reset_stats();

    auto worst_case_insn_energy = calculate_worst_case_instruction_energy(curr_insn_cycle);
    auto backup_energy = calculate_backup_energy();
    // std::cout << "battery energy=" << std:: dec << battery.energy_stored() << " energy needed for next instruction=" << (/*op_energy + */backup_energy + worst_case_insn_energy) << std::endl;

    if(mem_renamer) {
      num_map_table_backup_hits -= 3;
    }
    num_stores -= 3;


    if(battery.energy_stored() <= (/*op_energy + */backup_energy + worst_case_insn_energy)) {
      // if(mem_renamer) {
      //   if(memwr && need_renaming)
      //     num_map_table_backup_hits -= num_mem_access;
      // }
      // if(!thumbulator::dcache_hit && memwr)
      //   num_stores -= num_mem_access;

      return true;
    }

    return false;
  }

private:
  capacitor battery;

  uint64_t last_backup_cycle = 0u;
  uint64_t last_tick = 0u;

  thumbulator::cpu_state architectural_state{};
  bool active = false;

  int      const WATCHDOG_PERIOD;
  size_t   const READFIRST_ENTRIES;
  size_t   const IASSOC;
  uint32_t const IBLOCK_SIZE;
  uint32_t const ICACHE_SIZE;
  size_t   const DASSOC;
  uint32_t const DBLOCK_SIZE;
  uint32_t const DCACHE_SIZE;
  double   const MEM_RENAME_ICACHE_READ_ENERGY;
  double   const MEM_RENAME_ICACHE_WRITE_ENERGY;
  double   const MEM_RENAME_ICACHE_LEAKAGE_POWER;
  double   const MEM_RENAME_DCACHE_READ_ENERGY;
  double   const MEM_RENAME_DCACHE_WRITE_ENERGY;
  double   const MEM_RENAME_DCACHE_LEAKAGE_POWER;
  double   const MEM_RENAME_RF_ACCESS_ENERGY;
  double   const MEM_RENAME_RF_LEAKAGE_POWER;
  double   const LOCAL_BLOOMFILTER_ACCESS_ENERGY;
  double   const LOCAL_BLOOMFILTER_LEAKAGE_POWER;
  double   const MAP_TABLE_ACCESS_ENERGY;
  double   const MAP_TABLE_READ_ENERGY; 
  double   const MAP_TABLE_WRITE_ENERGY;
  double   const MAP_TABLE_LEAKAGE_POWER;
  double   const FREE_LIST_READ_ENERGY;
  double   const FREE_LIST_LEAKAGE_POWER;

  double   cache_energy_per_insn      = 0;
  double   mem_access_energy          = 0;
  double   rf_buffer_access_energy    = 0;
  double   rename_overhead_energy     = 0;
  uint8_t  num_backup_regs            = 0;
  uint32_t num_stores                 = 0;
  // uint32_t num_victim_blocks          = 0;
  uint32_t num_map_table_backup_hits  = 0;
  int      progress_watchdog          = 0;
  bool     idempotent_violation       = false;
  bool     continuous_power_supply    = false;
  uint64_t true_positives             = 0;
  uint64_t false_positives            = 0;
  uint32_t curr_num_entries           = 0;
  uint32_t last_num_entries           = 0;
  // uint32_t victim_block_addr          = 0;
  // uint32_t* victim_data               = nullptr;

  std::unique_ptr<bf::bloom_filter>    readfirst_filter; // global bloom filter
  std::unordered_set<uint32_t>         rf_stats_buffer;
  std::shared_ptr<thumbulator::cache>  insn_cache = nullptr;
  std::shared_ptr<thumbulator::cache>  data_cache = nullptr;
  std::shared_ptr<thumbulator::rename> mem_renamer = nullptr;
  std::set<uint64_t>                   dead_mem_locs;

  enum class operation { read, write };

  void clear_buffers()
  {
    if(READFIRST_ENTRIES > 0) {
      readfirst_filter->clear();
      rf_stats_buffer.clear();
    }
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
  }

  bool try_insert(std::set<uint32_t> *buffer, uint32_t const address, size_t const max_buffer_size)
  {
    if(buffer->size() < max_buffer_size) {
      buffer->insert(address);

      return true;
    }

    return false;
  }

  /**
   * Detection logic for idempotency violations.
   */
  bool detect_violation(thumbulator::cache_block& blk, uint32_t address, bool lbf, size_t set, size_t way, operation op, bool optimal_backup = false)
  {
    bool readfirst_hit       = true;
    bool rf_stats_buffer_hit = false;

    rf_stats_buffer_hit = (rf_stats_buffer.find(blk.get_address()) != rf_stats_buffer.end());
    if(lbf && !rf_stats_buffer_hit) {
      rf_stats_buffer.emplace(blk.get_address());
    }

    if(READFIRST_ENTRIES > 0) {
      readfirst_hit = readfirst_filter->lookup(blk.get_address());
      if(lbf && !readfirst_hit) {
        readfirst_filter->add(blk.get_address());
        rf_buffer_access_energy = MEM_RENAME_RF_ACCESS_ENERGY;
      }

      // this lookup does not incur any energy loss during actual program execution
      // it is done merely to calculate true vs false positives stats
      readfirst_hit = readfirst_filter->lookup(address);
    }
    
    rf_stats_buffer_hit = (rf_stats_buffer.find(address) != rf_stats_buffer.end());

    if(rf_stats_buffer_hit && readfirst_hit && !optimal_backup) {
      true_positives++;
    }
    if(!rf_stats_buffer_hit && readfirst_hit && !optimal_backup) {
      false_positives++;
    }

    if(blk.get_valid() && blk.get_dirty()) {
      if(optimal_backup) {
        return true;
      }
      if(mem_renamer) {
        rename_addr(blk, lbf);
      }
      else {
        idempotent_violation = true;
      }
    }

    if(optimal_backup) {
     return false;
    }

    return readfirst_hit;
  }

  bool rename_addr(thumbulator::cache_block& blk, bool lbf)
  {
    uint32_t index = 0;
    auto tag = blk.get_address() >> data_cache->get_block_offset();
    auto map_table_hit = mem_renamer->lookup_map_table(tag, index);

    if(!lbf) {
      if(map_table_hit) {
        blk.set_address(mem_renamer->read_map_table(index));
        rename_overhead_energy += (MAP_TABLE_READ_ENERGY + MAP_TABLE_ACCESS_ENERGY);
      }
    }
    else {
      if(!map_table_hit && mem_renamer->is_map_table_full()) {
        rename_overhead_energy += MAP_TABLE_ACCESS_ENERGY;
        std::cout << "rename_addr: (backup) map table full" << std::endl;
        idempotent_violation = true;
        if(will_backup(&stats)) {
        	stats.cpu.mr_backup_time = backup(&stats);
          stats.cpu.was_mr_backup =  true;
        }
      }
      else {
        if(mem_renamer->is_name_avail()) {
          auto addr = blk.get_address();
          mem_renamer->write_map_table(map_table_hit, tag, addr, index);
          blk.set_address(mem_renamer->read_map_table(index));
          // std::cout << "rename_addr: renamed address from 0x" << std::hex << addr << " to 0x" << blk.get_address() << std::endl;
          rename_overhead_energy += (MAP_TABLE_WRITE_ENERGY + FREE_LIST_READ_ENERGY);
        }
        else {
          rename_overhead_energy += MAP_TABLE_ACCESS_ENERGY;
          std::cout << "rename_addr: (backup) no available rename addresses" << std::endl;
          idempotent_violation = true;
          if(will_backup(&stats)) {
            stats.cpu.mr_backup_time = backup(&stats);
            stats.cpu.was_mr_backup = true;
          }
        }
      }
    }  

    return idempotent_violation;
  }

  bool process_cache_read(thumbulator::cache_block& blk, uint32_t address, bool lbf, size_t set, size_t way)
  {
    if(thumbulator::dcache_hit) {
      cache_energy_per_insn += MEM_RENAME_DCACHE_READ_ENERGY + 2 * LOCAL_BLOOMFILTER_ACCESS_ENERGY;
    }
    else {
      detect_violation(blk, address, lbf, set, way, operation::read);
      cache_energy_per_insn += MEM_RENAME_DCACHE_WRITE_ENERGY + (DBLOCK_SIZE >> 2) * LOCAL_BLOOMFILTER_ACCESS_ENERGY;
    }

    return idempotent_violation;
  }

  bool process_cache_write(thumbulator::cache_block& blk, uint32_t address, bool lbf, size_t set, size_t way, bool& gbf_hit)
  {
    if(thumbulator::dcache_hit) {
      cache_energy_per_insn += MEM_RENAME_DCACHE_WRITE_ENERGY + 2 * LOCAL_BLOOMFILTER_ACCESS_ENERGY;
    }
    else {
      gbf_hit = detect_violation(blk, address, lbf, set, way, operation::write);
      cache_energy_per_insn += MEM_RENAME_DCACHE_WRITE_ENERGY + (DBLOCK_SIZE >> 2) * LOCAL_BLOOMFILTER_ACCESS_ENERGY;
    }

    return idempotent_violation;
  }

  uint32_t process_ram_load(uint32_t address, uint32_t value)
  {
    if(continuous_power_supply) {
      mem_access_energy += CORTEX_M0PLUS_ENERGY_FLASH;
      return value;
    }

    if(active && battery.energy_stored() < calculate_backup_energy() && !thumbulator::OPTIMAL_BACKUP_POLICY) {
      std::cout << " POWER OFF: Not enough energy to load data from RAM: address=0x" << std::hex << address << std::endl;
      power_off();
    }
    else {
      mem_access_energy += CORTEX_M0PLUS_ENERGY_FLASH;
    }

    return value;
  }

  uint32_t process_ram_store(uint32_t address, uint32_t old_value, uint32_t value, bool backup)
  {
    if(continuous_power_supply) {
      mem_access_energy += CORTEX_M0PLUS_ENERGY_FLASH;
      return value;
    }

    if(battery.energy_stored() < calculate_backup_energy() && !thumbulator::OPTIMAL_BACKUP_POLICY) {
      std::cout << " POWER OFF: Not enough energy to store data into RAM: address=0x" << std::hex << address << std::endl;
      power_off();
      return old_value;
    }
    else {
      if(!backup) {
        mem_access_energy += CORTEX_M0PLUS_ENERGY_FLASH;
      }
    }
    
    return value;
  }
  
  double calculate_backup_energy()
  {
    uint32_t map_table_backup_words = 0;

    if(mem_renamer) {
      auto num_valid_map_table_entries = mem_renamer->get_num_backup_entries();
      map_table_backup_words = num_valid_map_table_entries * 11/4; // 23 bits(tag) + 32 bits(old name) + 32 bits(new name) = 86 bits
    }

    return (CLANK_BACKUP_ARCH_ENERGY * num_backup_regs/20) + (num_stores * DBLOCK_SIZE / 4 + map_table_backup_words) * CORTEX_M0PLUS_ENERGY_FLASH + num_map_table_backup_hits * MAP_TABLE_READ_ENERGY;
  }

  size_t write_back(bool backup)
  {
    auto count = 0;
    num_map_table_backup_hits = 0;

    for(size_t i=0; i<data_cache->get_numset(); i++) {
      for(size_t j=0; j<data_cache->get_numway(); j++) {

        thumbulator::cache_block blk = data_cache->get_block(i, j);

        if(blk.get_valid() && blk.get_dirty()) {
          auto write_addr = blk.get_address();
		
      	  if(mem_renamer) {
      	    uint32_t index = 0;
          	auto tag = blk.get_address() >> data_cache->get_block_offset();
      	    if(mem_renamer->lookup_map_table(tag, index)) {
              write_addr = mem_renamer->read_map_table(index);
      	      num_map_table_backup_hits++;
            }
      	  }

          for(uint32_t beat=0; beat<(DBLOCK_SIZE >> 2); beat++) {
            if(backup && active) {
              // std::cout << "write_back: addr=0x" << std::hex << (write_addr + (beat << 2)) 
              //  	        << " data=0x" << data_cache->get_data(i, j, beat) << std::endl; 
              thumbulator::store(write_addr + (beat << 2), data_cache->get_data(i, j, beat), true);
            }
          }

          if(backup && active) {
            data_cache->mark_clean(i, j);
          }
          count++;
        }
      }
    }

    return count;
  }


  const uint32_t calculate_worst_case_instruction_energy(uint64_t elapsed_cycles)
  {
    auto insn_cycles = 0;
    auto instruction_fetch_energy = 0;
    auto need_renaming = true;
    auto num_mem_access = 3;

    // if(thumbulator::icache_hit) {
    //   elapsed_cycles = 1;
    //   instruction_fetch_energy = MEM_RENAME_ICACHE_READ_ENERGY;
    // }
    // else {
      elapsed_cycles += (IBLOCK_SIZE >> 2) + 1;
      instruction_fetch_energy = (IBLOCK_SIZE >> 2) * (CORTEX_M0PLUS_ENERGY_FLASH + MEM_RENAME_ICACHE_WRITE_ENERGY);
    // }


    // insn_cycles = 1;

    // if(memop) { // store/load instruction
      // if(thumbulator::dcache_hit) {
      //   insn_cycles = num_mem_access * 1;
      //   if(memwr) 
      //     cache_energy_per_insn = num_mem_access * (MEM_RENAME_DCACHE_WRITE_ENERGY + 2 * LOCAL_BLOOMFILTER_ACCESS_ENERGY);
      //   else
      //     cache_energy_per_insn = num_mem_access * (MEM_RENAME_DCACHE_READ_ENERGY + 2 * LOCAL_BLOOMFILTER_ACCESS_ENERGY);
      // }
      // else {
        insn_cycles = num_mem_access * ((DBLOCK_SIZE >> 2) + 1);
        cache_energy_per_insn = num_mem_access * (MEM_RENAME_DCACHE_WRITE_ENERGY + (DBLOCK_SIZE >> 2) * LOCAL_BLOOMFILTER_ACCESS_ENERGY);
        mem_access_energy = num_mem_access * (DBLOCK_SIZE >> 2) * CORTEX_M0PLUS_ENERGY_FLASH;

        // thumbulator::cache_attributes attr;
        // attr.set = set;
        // attr.way = way;
        // auto load_addr = address & (~data_cache->get_block_mask());
        // auto victim    = data_cache->get_victim(attr);
        // auto lbf       = false;

        // if(victim.get_valid()) { 
        //   lbf = data_cache->get_block_state(attr.set, attr.way);
        // }

        // operation op = operation::read;
        // if(memwr) {
        //   op = operation::write;
        num_stores += num_mem_access;
        // }

        // need_renaming = detect_violation(victim, load_addr, lbf, attr.set, attr.way, op, true);
        if(mem_renamer) {          
          if(need_renaming) {
            rename_overhead_energy = num_mem_access * (MAP_TABLE_WRITE_ENERGY + FREE_LIST_READ_ENERGY);
            // if(memwr) {
               num_map_table_backup_hits += num_mem_access;
            // }
          }
        }
      // }
    // }

    // if(branch)
    //   insn_cycles = 2;

    // if(branch_link)
    //   insn_cycles = 3;

    elapsed_cycles += insn_cycles;

    auto const instruction_energy       = CLANK_INSTRUCTION_ENERGY * insn_cycles;
    auto const cache_leakage_energy     = (MEM_RENAME_ICACHE_LEAKAGE_POWER + MEM_RENAME_DCACHE_LEAKAGE_POWER) / clock_frequency() * elapsed_cycles;
    auto const rf_buffer_leakage_energy = MEM_RENAME_RF_LEAKAGE_POWER / clock_frequency() * elapsed_cycles;
    auto const lbf_leakage_energy       = LOCAL_BLOOMFILTER_LEAKAGE_POWER / clock_frequency() * elapsed_cycles;
    auto map_table_leakage_energy       = 0;
    auto free_list_leakage_energy       = 0;

    if(mem_renamer) {
      map_table_leakage_energy = MAP_TABLE_LEAKAGE_POWER / clock_frequency() * elapsed_cycles;
      free_list_leakage_energy = FREE_LIST_LEAKAGE_POWER / clock_frequency() * elapsed_cycles;
    }

    auto const op_energy = instruction_energy + cache_leakage_energy 
                           + cache_energy_per_insn + mem_access_energy 
                           + rf_buffer_access_energy + rf_buffer_leakage_energy
                           + lbf_leakage_energy // lbf access energy is included in cache_energy_per_insn (see process_cache_read/write functions)
                           + rename_overhead_energy + map_table_leakage_energy 
                           + free_list_leakage_energy;  

    reset_stats();

    return op_energy;
  }
};
}

#endif //EH_SIM_MEM_RENAME_HPP

