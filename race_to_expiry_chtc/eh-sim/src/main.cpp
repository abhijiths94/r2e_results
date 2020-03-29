#include <argagg/argagg.hpp>

#include <fstream>
#include <iomanip>
#include <iostream>
#include <memory>

#include "scheme/backup_every_cycle.hpp"
#include "scheme/clank.hpp"
#include "scheme/parametric.hpp"
#include "scheme/mem_rename.hpp"

#include "simulate.hpp"
#include "voltage_trace.hpp"
#include "liveness_trace.hpp"

void print_usage(std::ostream &stream, argagg::parser const &arguments)
{
  argagg::fmt_ostream help(stream);

  help << "Simulate an energy harvesting environment.\n\n";
  help << "simulate [options] ARG [ARG...]\n\n";
  help << arguments;
}

void ensure_file_exists(std::string const &path_to_file)
{
  std::ifstream binary_file(path_to_file);
  if(!binary_file.good()) {
    throw std::runtime_error("File does not exist: " + path_to_file);
  }
}

void validate(argagg::parser_results const &options)
{
  if(options["binary"].count() == 0) {
    throw std::runtime_error("Missing path to application binary.");
  }

  auto const path_to_binary = options["binary"].as<std::string>();
  ensure_file_exists(path_to_binary);

  if(options["voltages"].count() == 0) {
    throw std::runtime_error("Missing path to voltage trace.");
  }

  auto const path_to_voltage_trace = options["voltages"].as<std::string>();
  ensure_file_exists(path_to_voltage_trace);

  std::string path_to_reg_liveness_trace = "";
  if(options["use_reg_lva"].as<int>(1) == 1) { 
    path_to_reg_liveness_trace = options["reg_liveness"].as<std::string>();
    ensure_file_exists(path_to_reg_liveness_trace);
  }

  std::string path_to_mem_liveness_trace = "";
  if(options["use_mem_lva"].as<int>(1) == 1) {
    path_to_mem_liveness_trace = options["mem_liveness"].as<std::string>();
    ensure_file_exists(path_to_mem_liveness_trace);
  }

  if(options["rate"].count() == 0) {
    throw std::runtime_error("No sampling rate provided for the voltage trace.");
  }
}

int main(int argc, char *argv[])
{
  argagg::parser arguments{{{"help", {"-h", "--help"}, "display help information", 0},
      {"voltages", {"--voltage-trace"}, "path to voltage trace", 1},
      {"use_reg_lva", {"--reg-lva"}, "use register liveness analysis", 1},
      {"reg_liveness", {"--reg-liveness-trace"}, "path to register liveness trace", 1},
      {"use_mem_lva", {"--mem-lva"}, "use memory liveness analysis", 1},
      {"mem_liveness", {"--mem-liveness-trace"}, "path to memory liveness trace", 1},
      {"rate", {"--voltage-rate"}, "sampling rate of voltage trace (microseconds)", 1},
      {"harvest", {"--always-harvest"}, "harvest during active periods", 1},
      {"scheme", {"--scheme"}, "the checkpointing scheme to use", 1},
      {"tau_B", {"--tau-b"}, "the backup period for the parametric scheme", 1},
      {"binary", {"-b", "--binary"}, "path to application binary", 1},
      {"rf_entries", {"--rf-entries"}, "size of read first buffer", 1},
      {"wf_entries", {"--wf-entries"}, "size of write first buffer", 1},
      {"wb_entries", {"--wb-entries"}, "size of write back buffer", 1},
      {"lbf_size", {"--lbf-size"}, "size of local bloom filter", 1},
      {"watchdog_period", {"--watchdog-period"}, "watchdog timer period", 1},
      {"icache_assoc", {"--icache-assoc"}, " instruction cache associativity", 1},
      {"icache_block_size", {"--icache-block-size"}, " instruction cache block size", 1},
      {"icache_size", {"--icache-size"}, " instruction cache size", 1},
      {"dcache_assoc", {"--dcache-assoc"}, " data cache associativity", 1},
      {"dcache_block_size", {"--dcache-block-size"}, " data cache block size", 1},
      {"dcache_size", {"--dcache-size"}, " data cache size", 1},
      {"use_optimal_backup_scheme", {"--use-optimal-backup-scheme"}, "use optimal backup scheme for mem_rename", 1},
      {"add_renamer", {"--add-renamer"}, "add renamer to scheme", 1},
      {"reclaim_addr", {"--reclaim-addr"}, "reclaim original program address if available in free list", 1},
      {"map_table_entries", {"--map-table-entries"}, "size of map table", 1},
      {"num_avail_rename_addrs", {"--num-avail-rename-addrs"}, "number of available rename addresses", 1},
      {"icache_read_energy", {"--icache-read-energy"}, "energy consumed to read a icache block", 1},
      {"icache_write_energy", {"--icache-write-energy"}, "energy consumed to write a icache block", 1},
      {"icache_leakage_power", {"--icache-leakage-power"}, "leakage power of a icache bank", 1},
      {"dcache_read_energy", {"--dcache-read-energy"}, "energy consumed to read a dcache block", 1},
      {"dcache_write_energy", {"--dcache-write-energy"}, "energy consumed to write a dcache block", 1},
      {"dcache_leakage_power", {"--dcache-leakage-power"}, "leakage power of a dcache bank", 1},
      {"rf_access_energy", {"--rf-access-energy"}, "access energy of global read first buffer", 1},
      {"rf_leakage_power", {"--rf-leakage-power"}, "leakage power of global read first buffer", 1},
      {"lbf_access_energy", {"--lbf-access-energy"}, "access energy of local read first buffer", 1},
      {"lbf_leakage_power", {"--lbf-leakage-power"}, "leakage power of local read first buffer", 1},
      {"map_table_access_energy", {"--map-table-access-energy"}, "map table access energy", 1},
      {"map_table_read_energy", {"--map-table-read-energy"}, "map table read energy", 1},
      {"map_table_write_energy", {"--map-table-write-energy"}, "map table write energy", 1},
      {"map_table_leakage_power", {"--map-table-leakage-power"}, "map table leakage power", 1},
      {"free_list_read_energy", {"--free-list-read-energy"}, "free list read energy", 1},
      {"free_list_leakage_power", {"--free-list-leakage-power"}, "free list leakage power", 1},
      {"output", {"-o", "--output"}, "output file", 1}}};

  try {
    auto const options = arguments.parse(argc, argv);
    if(options["help"]) {
      print_usage(std::cout, arguments);
      return EXIT_SUCCESS;
    }

    validate(options);

    auto const path_to_binary = options["binary"];
    bool always_harvest = options["harvest"].as<int>(1) == 1;

    auto const path_to_voltage_trace = options["voltages"];

    bool  use_reg_lva = options["use_reg_lva"].as<int>(1) == 1;

    std::string path_to_reg_liveness_trace = "";
    if(use_reg_lva)
      path_to_reg_liveness_trace = options["reg_liveness"].as<std::string>();

    bool  use_mem_lva = options["use_mem_lva"].as<int>(1) == 1;

    std::string path_to_mem_liveness_trace = "";
    if(use_mem_lva)
      path_to_mem_liveness_trace = options["mem_liveness"].as<std::string>();

    std::chrono::milliseconds sampling_period(options["rate"]);

    std::unique_ptr<ehsim::eh_scheme> scheme = nullptr;
    auto const scheme_select = options["scheme"].as<std::string>("bec");
    if(scheme_select == "bec") {
      scheme = std::unique_ptr<ehsim::backup_every_cycle>(new ehsim::backup_every_cycle());
    } else if(scheme_select == "odab") {
      throw std::runtime_error("ODAB is no longer supported.");
    } else if(scheme_select == "magic") {
      throw std::runtime_error("Magic is no longer supported.");
    } else if(scheme_select == "clank") {
      auto rf_entries = options["rf_entries"].as<size_t>(8);
      auto wf_entries = options["wf_entries"].as<size_t>(8);
      auto wb_entries = options["wb_entries"].as<size_t>(8);
      auto watchdog_period = options["watchdog_period"]. as<int>(8000);
      scheme = std::unique_ptr<ehsim::clank>(new ehsim::clank(rf_entries,
		                                              wf_entries,
					                      wb_entries,
					                      watchdog_period));
    } else if(scheme_select == "mem_rename") {
      auto rf_entries = options["rf_entries"].as<size_t>(8);
      auto lbf_size = options["lbf_size"].as<size_t>(16);
      auto icache_assoc = options["icache_assoc"].as<size_t>(1);
      auto icache_block_size = options["icache_block_size"].as<uint32_t>(0);
      auto icache_size = options["icache_size"].as<uint32_t>(0);
      auto dcache_assoc = options["dcache_assoc"].as<size_t>(1);
      auto dcache_block_size = options["dcache_block_size"].as<uint32_t>(0);
      auto dcache_size = options["dcache_size"].as<uint32_t>(0);
      auto use_optimal_backup_scheme = options["use_optimal_backup_scheme"].as<int>(0) == 1;
      auto add_renamer = options["add_renamer"].as<int>(0) == 1;
      auto reclaim_addr = options["reclaim_addr"].as<int>(0) == 1;
      auto map_table_entries = options["map_table_entries"].as<size_t>(4);
      auto num_avail_rename_addrs = options["num_avail_rename_addrs"].as<uint32_t>(8);
      auto watchdog_period = options["watchdog_period"]. as<int>(8000);
      auto icache_read_energy = options["icache_read_energy"].as<double>(4.87e-13);
      auto icache_write_energy = options["icache_write_energy"].as<double>(5.11e-13);
      auto icache_leakage_power = options["icache_leakage_power"].as<double>(1.21e-3);
      auto dcache_read_energy = options["dcache_read_energy"].as<double>(4.87e-13);
      auto dcache_write_energy = options["dcache_write_energy"].as<double>(5.11e-13);
      auto dcache_leakage_power = options["dcache_leakage_power"].as<double>(1.21e-3);
      auto rf_access_energy = options["rf_access_energy"].as<double>(0.19e-13);
      auto rf_leakage_power = options["rf_leakage_power"].as<double>(0.047e-3);
      auto lbf_access_energy = options["lbf_access_energy"].as<double>(0);
      auto lbf_leakage_power = options["lbf_leakage_power"].as<double>(0);
      auto map_table_access_energy = options["map_table_access_energy"].as<double>(0);
      auto map_table_read_energy = options["map_table_read_energy"].as<double>(0);
      auto map_table_write_energy = options["map_table_write_energy"].as<double>(0);
      auto map_table_leakage_power = options["map_table_leakage_power"].as<double>(0);
      auto free_list_read_energy = options["free_list_read_energy"].as<double>(0);
      auto free_list_leakage_power = options["free_list_leakage_power"].as<double>(0);

      scheme = std::unique_ptr<ehsim::mem_rename>(new ehsim::mem_rename(rf_entries,
                                                                        lbf_size,
                                                                        watchdog_period,
                                                                        icache_assoc,
                                                                        icache_block_size,
                                                                        icache_size,
                                                                        dcache_assoc,
                                                                        dcache_block_size,
                                                                        dcache_size,
						                        use_optimal_backup_scheme,
                                                                        add_renamer,
						                        reclaim_addr,
                                                                        map_table_entries,
                                                                        num_avail_rename_addrs,
                                                                        icache_read_energy,
                                                                        icache_write_energy,
                                                                        icache_leakage_power,
                                                                        dcache_read_energy,
                                                                        dcache_write_energy,
                                                                        dcache_leakage_power,
                                                                        rf_access_energy,
                                                                        rf_leakage_power,
                                                                        lbf_access_energy,
                                                                        lbf_leakage_power,
						                        map_table_access_energy,
						                        map_table_read_energy,
						                        map_table_write_energy,
						                        map_table_leakage_power,
						                        free_list_read_energy,
						                        free_list_leakage_power));
    } else if(scheme_select == "parametric") {
      auto const tau_b = options["tau_B"].as<int>(1000);
      scheme = std::unique_ptr<ehsim::parametric>(new ehsim::parametric(tau_b));
    } else {
      throw std::runtime_error("Unknown scheme selected.");
    }

    ehsim::voltage_trace power(path_to_voltage_trace, sampling_period);

    ehsim::liveness_trace reg_liveness(use_reg_lva, path_to_reg_liveness_trace);

    ehsim::liveness_trace mem_liveness(use_mem_lva, path_to_mem_liveness_trace);

    auto const stats = ehsim::simulate(path_to_binary, power, use_reg_lva, reg_liveness, use_mem_lva, mem_liveness, scheme.get(), always_harvest);

    std::cout << "CPU instructions executed: " << std::dec << stats.cpu.instruction_count << "\n";
    std::cout << "CPU instructions executed towards forward progress: " << std::dec << stats.cpu.instruction_count_forward_progress << "\n";
    if(scheme_select == "mem_rename") {
      std::cout << "Number of true positives: " << std::dec << stats.system.true_positives << "\n";
      std::cout << "Number of false positives: " << std::dec << stats.system.false_positives << "\n";
      std::cout << "Number of times renamed: " << std::dec << stats.system.num_renamed_mappings << "\n";
      std::cout << "Number of times reclaimed: " << std::dec << stats.system.num_reclaimed_mappings << "\n";
    }
    std::cout << "CPU time (cycles): " << std::dec << stats.cpu.cycle_count << "\n";
    std::cout << "Total time (ns): " << std::dec << stats.system.time.count() << "\n";
    std::cout << "Energy harvested (J): " << std::dec << stats.system.energy_harvested * 1e-9 << "\n";
    std::cout << "Energy remaining (J): " << std::dec << stats.system.energy_remaining * 1e-9 << "\n";

    std::string output_file_name(scheme_select + ".csv");
    if(options["output"].count() > 0) {
      output_file_name = options["output"].as<std::string>();
    }

    std::ofstream out(output_file_name);
    out.setf(std::ios::fixed);
    out << "id, E, epsilon, epsilon_C, tau_B, alpha_B, energy_consumed, n_B, tau_P, tau_D, e_P, e_B, "
           "e_R, sim_p, eh_p, n_iB\n";

    int id = 0;
    for(auto const &model : stats.models) {
      out << id++ << ", ";

      auto const eh_parameters = ehsim::eh_model_parameters(model);
      out << std::setprecision(3) << eh_parameters.E << ", ";
      out << std::setprecision(3) << eh_parameters.epsilon << ", ";
      out << std::setprecision(3) << eh_parameters.epsilon_C << ", ";
      out << std::setprecision(2) << eh_parameters.tau_B << ", ";
      out << std::setprecision(4) << eh_parameters.alpha_B << ", ";

      auto const tau_D = model.time_for_instructions - model.time_forward_progress;
      out << std::setprecision(3) << model.energy_consumed << ", ";
      out << std::setprecision(0) << model.num_backups << ", ";
      out << std::setprecision(0) << model.time_forward_progress << ", ";
      out << std::setprecision(0) << tau_D << ", ";
      out << std::setprecision(3) << model.energy_forward_progress << ", ";
      out << std::setprecision(3) << model.energy_for_backups << ", ";
      out << std::setprecision(3) << model.energy_for_restore << ", ";
      out << std::setprecision(3) << model.progress << ", ";
      out << std::setprecision(3) << model.eh_progress << ", ";
      out << std::setprecision(0) << model.num_id_backups << "\n";
    }
  } catch(std::exception const &e) {
    std::cerr << "Error: " << e.what() << "\n";
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
