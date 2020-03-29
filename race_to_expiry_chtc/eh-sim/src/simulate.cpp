#include "simulate.hpp"

#include <thumbulator/cpu.hpp>
#include <thumbulator/memory.hpp>
#include <thumbulator/cache.hpp>

#include "scheme/eh_scheme.hpp"
#include "capacitor.hpp"
#include "stats.hpp"
#include "voltage_trace.hpp"
#include "liveness_trace.hpp"

#include <torch/script.h>
#include <cstring>
#include <iostream>


#define MEAN_0 2.1731910037760214
#define MEAN_1 76325.01716411403
#define SD_0   1.1933448413265264
#define SD_1   91963.38072192093


//#define MEAN_0 2.617074395841535
//#define MEAN_1 67213.58046138467
//#define SD_0   1.3330418578555099
//#define SD_1   89587.96387189817

//#define MEAN_0 1.2287316313547834 //2.617074395841535
//#define MEAN_1 76271.89987944828 //67213.58046138467
//#define SD_0   1.3531843045335201 //1.3330418578555099
//#define SD_1   91905.93661451967 //89587.96387189817

namespace ehsim {


// stats tracking
stats_bundle stats{};
double gl_env_volt = 0;
double gl_batt_energy = 0;
torch::jit::script::Module module;

int spendthrift_backup(int print)
{
    float array[2]; //1.5050, 277.0000};
    int b_nb = 0;

    array[0] = (float)((gl_env_volt - MEAN_0)/SD_0);
    array[1] = (float) ((gl_batt_energy - MEAN_1)/SD_1);


    torch::Tensor tensor_in = torch::from_blob(array, {1, 2});
    std::vector<torch::jit::IValue> inputs;
    inputs.push_back(tensor_in);


    // Execute the model and turn its output into a tensor.
    at::Tensor output = module.forward(inputs).toTensor();
    //std::cout << output.item<float>() << std::endl;
    if(output.item<float>() >= 0.5 )
    {
        b_nb = 1;
    }

     if(print && (gl_batt_energy < 1300))
    {
        std::cout << gl_env_volt <<"   " << gl_batt_energy<<"   " << b_nb << "spendthrift : " << output.item<float>()<< std::endl;
    }

    return b_nb;
}

double get_env_voltage(void)
{
    return gl_env_volt;
}

void load_program(char const *file_name)
{
  std::FILE *fd = std::fopen(file_name, "r");
  if(fd == nullptr) {
    throw std::runtime_error("Could not open binary file.\n");
  }

  std::fread(&thumbulator::FLASH_MEMORY, sizeof(uint32_t),
      sizeof(thumbulator::FLASH_MEMORY) / sizeof(uint32_t), fd);
  std::fclose(fd);
}

void initialize_system(eh_scheme* scheme, char const *binary_file)
{
  // Reset memory, then load program to memory
  std::memset(thumbulator::RAM, 0, sizeof(thumbulator::RAM));
  std::memset(thumbulator::FLASH_MEMORY, 0, sizeof(thumbulator::FLASH_MEMORY));
  load_program(binary_file);

  // Initialize CPU state
  thumbulator::cpu_reset();

  // PC seen is PC + 4
  thumbulator::cpu_set_pc(thumbulator::cpu_get_pc() + 0x4);
}

/**
 * Execute one instruction.
 *
 * @return Number of cycles to execute that instruction.
 */
uint32_t step_cpu(stats_bundle *stats, eh_scheme* scheme, uint64_t active_start, uint64_t& elapsed_cycles, bool& was_backup)
{
  thumbulator::BRANCH_WAS_TAKEN = false;

  if((thumbulator::cpu_get_pc() & 0x1) == 0) {
    printf("Oh no! Current PC: 0x%08X\n", thumbulator::cpu.gpr[15][0]);
    throw std::runtime_error("PC moved out of thumb mode.");
  }

  // fetch
  uint16_t instruction;
  thumbulator::fetch_instruction(thumbulator::cpu_get_pc() - 0x4, &instruction);
  // decode
  // std::cout << "Cycle " << stats->cpu.cycle_count << std::endl;
  auto const decoded = thumbulator::decode(instruction);

  if(thumbulator::dcache && thumbulator::OPTIMAL_BACKUP_POLICY) {
    // scheme = memory renaming and optimal backup policy = ON --> mock execute, memory and write-back
    bool is_memwr = false;
    bool is_memop = false;
    bool is_branch = false;
    bool is_branch_link = false;
    thumbulator::mock_exmemwb = true;
    uint32_t num_mem_access = 0; // only for multiple load and store instructions like ldm/stm
    uint32_t address = thumbulator::exmemwb_mock(instruction, &decoded, is_memwr, is_memop, is_branch, is_branch_link, num_mem_access);

    thumbulator::cache_attributes attr;
    thumbulator::dcache_hit = thumbulator::dcache->is_hit(address, attr);

//    if(scheme->optimal_backup_scheme((stats->cpu.cycle_count - active_start), address, attr.set, attr.way, is_memwr, is_memop, is_branch, is_branch_link, num_mem_access)) 
    
    if(spendthrift_backup(0))
    {

      //std::cout << "Cycle " << std::dec << stats->cpu.cycle_count << ": backup (optimal backup scheme)" << std::endl;
      std::cout << "Cycle " << std::dec << stats->cpu.cycle_count << ": backup (Spendthrift backup scheme): " <<gl_env_volt<< "   "<< gl_batt_energy<<std::endl;
      auto const backup_time = scheme->backup(stats);


      elapsed_cycles += backup_time;

      auto &active_stats = stats->models.back();
      thumbulator::dcache_hit = false;
      active_stats.time_for_backups += backup_time;
      active_stats.energy_forward_progress = active_stats.energy_for_instructions;
      active_stats.time_forward_progress = stats->cpu.cycle_count - active_start;
      was_backup = true;
    }

    thumbulator::mock_exmemwb = false;
  }

  // execute, memory, and write-back
  uint32_t instruction_ticks = thumbulator::exmemwb(instruction, &decoded);
  if(!thumbulator::icache_hit) {
    instruction_ticks += ((thumbulator::dcache->get_block_size() >> 2) + 1);
  }
  else {
    instruction_ticks++;
  }

  // advance to next PC
  if(!thumbulator::BRANCH_WAS_TAKEN) {
    thumbulator::cpu_set_pc(thumbulator::cpu_get_pc() + 0x2);
  } else {
    thumbulator::cpu_set_pc(thumbulator::cpu_get_pc() + 0x4);
  }

  return instruction_ticks;
}

std::chrono::nanoseconds get_time(uint64_t const cycle_count, uint32_t const frequency)
{
  double const CPU_PERIOD = 1.0 / frequency;
  auto const time = static_cast<uint64_t>(CPU_PERIOD * cycle_count * 1e9);

  return std::chrono::nanoseconds(time);
}

std::chrono::milliseconds to_milliseconds(std::chrono::nanoseconds const &time)
{
  return std::chrono::duration_cast<std::chrono::milliseconds>(time);
}

uint64_t time_to_cycles(std::chrono::nanoseconds elapsed_time, uint32_t clock_frequency)
{
  return static_cast<uint64_t>(
      ceil(clock_frequency * std::chrono::duration<double>(elapsed_time).count()));
}

// returns amount of energy per cycles
double calculate_charging_rate(double env_voltage, capacitor &battery, double cpu_freq)
{
#ifdef MODEL_VOLTAGE_SOURCE
  // only charge if source voltage higher than current voltage across capacitor
  if(env_voltage > battery.voltage()) {
    // assume always max charging rate
    auto dV_dt = battery.max_current() / battery.capacitance();
    double energy_per_cycle = dV_dt / cpu_freq;
  }
#else
  // voltage in the trace is measured across 30kohm resistor
  double current_source = env_voltage / 30000;
  auto dV_dt = current_source / battery.capacitance();
  double energy_per_cycle = dV_dt / cpu_freq;
#endif

  return energy_per_cycle;
}

double update_energy_harvested(uint64_t elapsed_cycles,
    std::chrono::nanoseconds exec_end_time,
    double &charging_rate,
    double &env_voltage,
    std::chrono::nanoseconds &next_charge_time,
    uint32_t clock_freq,
    ehsim::voltage_trace const &power,
    capacitor &battery)
{
  // execution can span over more than 1 voltage trace sample period
  // accumulate charge of each periods separately
  auto potential_harvested_energy = 0.0;
  auto cycles_accounted = 0;

  while(exec_end_time >= next_charge_time) {
    uint64_t cycles_in_cur_charge_rate =
        elapsed_cycles - time_to_cycles(exec_end_time - next_charge_time, clock_freq);
    potential_harvested_energy += cycles_in_cur_charge_rate * charging_rate;
    cycles_accounted += cycles_in_cur_charge_rate;

    // move to next voltage sample
    next_charge_time += power.sample_period();
    env_voltage = power.get_voltage(to_milliseconds(next_charge_time));
    charging_rate = calculate_charging_rate(env_voltage, battery, clock_freq);
  }

  potential_harvested_energy += (elapsed_cycles - cycles_accounted) * charging_rate;

  // update battery -- battery may be full so ahe<=phe
  auto actual_harvested_energy = battery.harvest_energy(potential_harvested_energy);

  return actual_harvested_energy;
}

stats_bundle simulate(char const *binary_file,
    ehsim::voltage_trace const &power,
    bool const use_reg_lva,
    ehsim::liveness_trace const &reg_liveness,
    bool const use_mem_lva,
    ehsim::liveness_trace const &mem_liveness,
    eh_scheme *scheme,
    bool always_harvest)
{
  // using namespace std::chrono_literals;

  stats.system.time = std::chrono::nanoseconds(0);

  initialize_system(scheme, binary_file);

  // energy harvesting
  auto &battery = scheme->get_battery();
  // start in power-off mode
  auto was_active = false;

  // frequency in Hz, sample period in ms
  auto cycles_per_sample = static_cast<uint64_t>(
      scheme->clock_frequency() * std::chrono::duration<double>(power.sample_period()).count());

  std::cout.setf(std::ios::unitbuf);
  //std::cout << "cycles per sample: " << cycles_per_sample << "\n";

  // get voltage based current time (includes active+sleep) -- this should be @ time 0
  auto env_voltage = power.get_voltage(to_milliseconds(stats.system.time));

  auto charging_rate = calculate_charging_rate(env_voltage, battery, scheme->clock_frequency());
  auto next_charge_time = std::chrono::nanoseconds(power.sample_period());
  //std::cout << "next_charge_time: " << next_charge_time.count() << "ns\n";

  /* ABSO edit */
  gl_env_volt = env_voltage;
  gl_batt_energy = battery.energy_stored();
  uint64_t active_start = 0u;
  int no_progress_counter = 0;

  // was there a backup previous iteration
  auto was_backup = false;

  uint64_t start_backup_insn = 0u;
  // uint64_t end_backup_insn = 0u;

   

  // Execute the program
  // Simulation will terminate when it executes insn == 0xBFAA
  /* Open spendthrift */
    char buff[120];

    getcwd(buff, 120);
    std::cout<<"Working dir : " << buff << std::endl;

    module = torch::jit::load("traced_spendthrift_model_updated.pt");

  while(!thumbulator::EXIT_INSTRUCTION_ENCOUNTERED && stats.cpu.instruction_count_forward_progress < 10000000) {
    uint64_t elapsed_cycles = 0;
    std::set<uint64_t> dead_regs{};

    if(use_mem_lva) {
      auto const dead_mem_addrs = mem_liveness.get_liveness(stats.cpu.cycle_count);
      if(!dead_mem_addrs.empty())
        scheme->set_dead_addresses(dead_mem_addrs);
    }

    if(use_reg_lva)
      dead_regs = reg_liveness.get_liveness(stats.cpu.cycle_count);
    // std::cout << "cycle: " << std::dec << stats.cpu.cycle_count << " number of dead regs: " << std::dec << dead_regs.size() << std::endl;
    // for(auto it=dead_regs.begin(); it !=dead_regs.end(); it++) {
    //   std::cout << *it << " ";
    // }
    // if(!dead_regs.empty()) {
    //   std::cout << std::endl;
    // }

    scheme->calculate_backup_locs(use_reg_lva, dead_regs);

    if(scheme->is_active(&stats)) {
      if(!was_active) {
        //std::cout << "["
        //          << std::chrono::duration_cast<std::chrono::nanoseconds>(stats.system.time).count()
        //          << "ns - ";
        // allocate space for a new active period model
        stats.models.emplace_back();
        // track the time this active mode started
        active_start = stats.cpu.cycle_count;
        stats.cpu.instruction_count_forward_progress = stats.cpu.end_backup_insn;
        stats.models.back().energy_start = battery.energy_stored();

        if(stats.cpu.instruction_count != 0) {
          // restore state
          auto const restore_time = scheme->restore(&stats);
          elapsed_cycles += restore_time;

          stats.models.back().time_for_restores += restore_time;
        }
      }

      if(was_backup || stats.cpu.was_mr_backup)
        start_backup_insn = stats.cpu.instruction_count;

      was_active = true;
      was_backup = false;
      stats.cpu.was_mr_backup = false;
      stats.cpu.mr_backup_time = 0;

      scheme->reset_stats();

      thumbulator::icache_hit = false;

      if((stats.cpu.instruction_count_forward_progress % 100000) == 0) {
      	std::cout << "Cycle " << stats.cpu.cycle_count << ": instructions towards forward progress=" << std::dec << stats.cpu.instruction_count_forward_progress << std::endl;
      }

      gl_env_volt = power.get_voltage(to_milliseconds(stats.system.time));
      gl_batt_energy = battery.energy_stored();

      auto const instruction_ticks = step_cpu(&stats, scheme, active_start, elapsed_cycles, was_backup);

      if(stats.cpu.was_mr_backup) {
        elapsed_cycles += stats.cpu.mr_backup_time;

        auto &active_stats = stats.models.back();
        active_stats.time_for_backups += stats.cpu.mr_backup_time;
        active_stats.energy_forward_progress = active_stats.energy_for_instructions;
        active_stats.time_forward_progress = stats.cpu.cycle_count - active_start;
      }

      stats.cpu.instruction_count++;
      stats.cpu.instruction_count_forward_progress++;
      stats.cpu.cycle_count += instruction_ticks;
      stats.models.back().time_for_instructions += instruction_ticks;
      elapsed_cycles += instruction_ticks;

      // consume energy for execution
      scheme->execute_instruction(&stats);

      //uint64_t num_dead_addrs = 0;

      if(use_mem_lva) {
        auto const dead_mem_addrs = mem_liveness.get_liveness(stats.cpu.cycle_count);
        // num_dead_addrs = dead_mem_addrs.size();
        if(!dead_mem_addrs.empty())
          scheme->set_dead_addresses(dead_mem_addrs);
      }

      uint32_t num_dead_dirty_regs = 0;
      if(use_reg_lva) {
        dead_regs = reg_liveness.get_liveness(stats.cpu.cycle_count);
        // std::cout << "Cycle " << stats.cpu.cycle_count << ": (dead_dirty)";
        for(auto it=dead_regs.begin(); it!=dead_regs.end(); it++) {
          if(thumbulator::cpu_get_gpr_dbit(*it)) {
            num_dead_dirty_regs++;
            // std::cout << " " << *it;
          }
        }
        //std::cout << std::endl;
      }

      scheme->calculate_backup_locs(use_reg_lva, dead_regs);

      // auto num_dirty_mem_addrs = scheme->get_wb_buffer_size();
      uint32_t num_dirty_regs = 0;
      // std::cout << "Cycle " << stats.cpu.cycle_count << ": (dirty)";
      for(uint32_t i=0; i<13; i++) {
        if(thumbulator::cpu_get_gpr_dbit(i)) {
          // std::cout << " " << i;
          num_dirty_regs++;
        }
      }
      // std::cout << std::endl;

      auto num_dirty_bytes = 4 * (num_dirty_regs/* + num_dirty_mem_addrs*/);
      auto num_dirty_live_bytes = num_dirty_bytes - 4 * (num_dead_dirty_regs/* + num_dead_addrs*/);

      // std::cout << "Cycle " << stats.cpu.cycle_count << ": num_dirty_bytes=" << num_dirty_bytes 
      //           << " num_dirty_live_bytes=" << num_dirty_live_bytes
      //           << " num_dirty_regs=" << num_dirty_regs
      //           << " num_dead_dirty_regs=" << num_dead_dirty_regs << std::endl;

      assert(num_dirty_bytes >= 0);
      assert(num_dirty_live_bytes <= num_dirty_bytes);

      int clank_b = scheme->will_backup(&stats);

      int spendthrift_b = 0;
      if(!(thumbulator::OPTIMAL_BACKUP_POLICY))
           spendthrift_b = spendthrift_backup(0);

      if(clank_b || spendthrift_b) 
      {
        if(clank_b)
            std::cout << "Cycle " << stats.cpu.cycle_count << ": clank backup(Will Backup)" << std::endl;
        else if(spendthrift_b)
            std::cout << "Cycle " << stats.cpu.cycle_count << ": spendthrift backup(Will Backup)" << std::endl;

        auto num_backup_insn = stats.cpu.end_backup_insn - start_backup_insn;
        // std::cout << "backup: num_backup_insn=" << std::dec << num_backup_insn << std::endl;
        auto const backup_time = scheme->backup(&stats);
        elapsed_cycles += backup_time;

        auto &active_stats = stats.models.back();
        active_stats.time_for_backups += backup_time;
        active_stats.energy_forward_progress = active_stats.energy_for_instructions;
        active_stats.time_forward_progress = stats.cpu.cycle_count - active_start;
        was_backup = true;
      }

      stats.system.time += get_time(elapsed_cycles, scheme->clock_frequency());

      if(always_harvest) {
        // update energy harvested & voltage sample corresponding to current time
        auto harvested_energy = update_energy_harvested(elapsed_cycles, stats.system.time, charging_rate, env_voltage,
                next_charge_time, scheme->clock_frequency(), power, battery);
        stats.system.energy_harvested += harvested_energy;
        stats.models.back().energy_charged += harvested_energy;
      } else {
        // just update voltage sample value
        if(stats.system.time >= next_charge_time) {
          while(stats.system.time >= next_charge_time) {
            next_charge_time += power.sample_period();
          }

          env_voltage = power.get_voltage(to_milliseconds(stats.system.time));
          /* ABSO edit */
          gl_env_volt = env_voltage;
          charging_rate = calculate_charging_rate(env_voltage, battery, scheme->clock_frequency());
        }
      }
    } else { // powered off
      if(was_active) {
        //std::cout << std::chrono::duration_cast<std::chrono::nanoseconds>(stats.system.time).count()
        //          << "ns]\n";
        // we just powered off
        auto &active_period = stats.models.back();

        // ensure forward progress is being made, otherwise throw
        //ensure_forward_progress(&no_progress_counter, active_period.num_backups, 5);

        active_period.time_total = active_period.time_for_instructions +
                                   active_period.time_for_backups + active_period.time_for_restores;

        active_period.energy_consumed = active_period.energy_for_instructions +
                                        active_period.energy_for_backups +
                                        active_period.energy_for_restore;

        active_period.progress =
            active_period.energy_forward_progress / active_period.energy_consumed;
        active_period.eh_progress = scheme->estimate_progress(eh_model_parameters(active_period));
      }

      was_active = false;

      // figure out how long to be off for
      // move in steps of voltage sample (1ms)
      double const min_energy = scheme->min_energy_to_power_on(&stats);
      double const min_voltage = sqrt(2 * min_energy / battery.capacitance());

      // assume linear max dV/dt for now
      double const max_dV_dt = battery.max_current() / battery.capacitance();
      double const dV_dt_per_cycle = max_dV_dt / scheme->clock_frequency();
      auto const min_cycles =
      static_cast<uint64_t>(ceil((min_voltage - battery.voltage()) / dV_dt_per_cycle));

      auto time_until_next_charge = next_charge_time - stats.system.time;
      uint64_t cycles_until_next_charge =
          time_to_cycles(time_until_next_charge, scheme->clock_frequency());

      if(min_cycles > cycles_until_next_charge) {
        stats.system.time = next_charge_time;
        elapsed_cycles = cycles_until_next_charge;
      } else {
        elapsed_cycles = min_cycles;
        auto elapsed_time = std::chrono::nanoseconds(
            static_cast<uint64_t>(elapsed_cycles * scheme->clock_frequency() * 1e9));
        stats.system.time += elapsed_time;
      }

      // update energy harvested & voltage sample corresponding to current time
      auto harvested_energy = update_energy_harvested(elapsed_cycles, stats.system.time,
          charging_rate, env_voltage, next_charge_time, scheme->clock_frequency(), power, battery);
      stats.system.energy_harvested += harvested_energy;
    }
  }
  std::cout << "done\n";

  // scheme->print_map_table();

  auto &active_period = stats.models.back();
  active_period.time_total = active_period.time_for_instructions + active_period.time_for_backups +
                             active_period.time_for_restores;

  active_period.energy_consumed = active_period.energy_for_instructions +
                                  active_period.energy_for_backups +
                                  active_period.energy_for_restore;

  if(active_period.num_backups == 0) {
    active_period.energy_forward_progress = active_period.energy_for_instructions;
  }

  active_period.progress = active_period.energy_forward_progress / active_period.energy_consumed;
  active_period.eh_progress = scheme->estimate_progress(eh_model_parameters(active_period));

  stats.system.true_positives  = scheme->get_true_positives();
  stats.system.false_positives = scheme->get_false_positives();
  stats.system.num_renamed_mappings = scheme->get_renamed_mappings();
  stats.system.num_reclaimed_mappings = scheme->get_reclaimed_mappings();

  stats.system.energy_remaining = battery.energy_stored();

  return stats;
}
}
