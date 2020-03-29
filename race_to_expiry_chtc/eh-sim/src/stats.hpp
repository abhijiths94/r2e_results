#ifndef EH_SIM_STATS_HPP
#define EH_SIM_STATS_HPP

#include <chrono>
#include <deque>

namespace ehsim {
struct cpu_stats {
  /**
   * Number of executed instructions.
   */
  uint64_t instruction_count = 0u;

  /**
   * Number of executed instructions towards forward progress (excluding re-execution after power failure)
   */
  uint64_t instruction_count_forward_progress = 0u;

  /**
   * Number of cycles ticked.
   */
  uint64_t cycle_count = 0u;

  /** 
   * Backup due to map table being full/free list being empty and backup time
   */
  bool was_mr_backup = false;
  uint32_t mr_backup_time = 0;

  /**
   * Number of instructions till last backup
   */
  uint64_t end_backup_insn = 0u;
};

struct system_stats {
  /**
   * Total execution time, including active and charging periods.
   */
  std::chrono::nanoseconds time{0};

  /**
   * Amount of energy harvested (nJ).
   */
  double energy_harvested = 0.0;

  /**
   * Remaining energy in battery.
   */
  double energy_remaining = 0.0;

  /**
  * Number of backups due to idempotent violations (mem_rename; same as total num_backups for any other scheme)
  */
  uint64_t true_positives = 0u;

  /**
  * Number of backups due to false positives from rf bloom filter (mem_rename)
  */
  uint64_t false_positives = 0u;

  /**
  * Number of times program address is renamed
  */
  uint64_t num_renamed_mappings = 0u;

  /**
  * Number of times default mappings are reclaimed for a program address
  */
  uint64_t num_reclaimed_mappings = 0u;
};

struct active_stats {
  /**
   * The accumulated cycle count of the time between all backups.
   */
  uint64_t time_between_backups = 0;

  /**
   * The accumulated cycle count spent doing backups.
   */
  uint64_t time_for_backups = 0;

  /**
   * The accumulated cycle count spent doing restores.
   */
  uint64_t time_for_restores = 0;

  /**
   * The total CPU cycles spent on forward progress.
   */
  uint64_t time_forward_progress = 0u;

  /**
   * The total CPU cycles of this active period.
   */
  uint64_t time_for_instructions = 0u;

  /**
   * The total cycles of this active period.
   */
  uint64_t time_total = 0u;

  /**
   * The number of backups performed.
   */
  int num_backups = 0;

  /**
   * The number of backups due to idempotent violations
   */
  int num_id_backups = 0;

  /**
   * The energy in the capacitor at the start of the active period.
   */
  double energy_start = 0.0;

  /**
   * The total energy used in an active period, including energy charged.
   */
  double energy_consumed = 0.0;

  /**
   * The accumulated energy (nJ) spent doing backups.
   */
  double energy_for_backups = 0.0;

  /**
   * The total energy charged during the active period.
   */
  double energy_charged = 0.0;

  /**
   * The energy (nJ) spent doing the restore.
   */
  double energy_for_restore = 0.0;

  /**
   * The total amount of energy spent on executing instructions.
   */
  double energy_for_instructions = 0.0;

  /**
   * Energy spent on executing instructions that were backed up.
   */
  double energy_forward_progress = 0.0;

  /**
   * The accumulated bytes/cycle backed up by the application.
   */
  double bytes_application = 0u;

  /**
   * Forward progress made as a percentage of total energy used.
   */
  double progress = 0.0;

  /**
   * Estimated forward progress made as a percentage of total energy used.
   */
  double eh_progress = 0.0;
};

struct eh_model_parameters {
  explicit eh_model_parameters(active_stats const &active_period)
      : E(0.0)
      , epsilon(active_period.energy_for_instructions / active_period.time_for_instructions)
      , epsilon_C(0.0)
      , tau_B(static_cast<double>(active_period.time_between_backups) / active_period.num_backups)
      , alpha_B(active_period.bytes_application / active_period.num_backups)
      , alpha_R(0.0)
      , do_restore(active_period.energy_for_restore > 0)
  {
    if(active_period.energy_consumed <= active_period.energy_start) {
      E = active_period.energy_consumed;
    } else {
      E = active_period.energy_start;
    }

    if(active_period.energy_consumed > active_period.energy_start) {
      auto const energy_charged = active_period.energy_consumed - active_period.energy_start;
      epsilon_C = energy_charged / active_period.time_total;
    }

    if(epsilon_C > epsilon) {
      // special case
      epsilon_C = 0.0;
      E = active_period.energy_consumed;
    }
  }

  double E;

  double const epsilon;

  double epsilon_C;

  double const tau_B;

  double const alpha_B;

  double const alpha_R;

  bool const do_restore;
};

struct stats_bundle {
  system_stats system;
  cpu_stats cpu;

  /**
   * Model of multiple active periods.
   */
  std::deque<active_stats> models;
};

extern stats_bundle stats;

}

#endif //EH_SIM_STATS_HPP
