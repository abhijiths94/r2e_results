import argparse
import sys
import subprocess
import os


def run(eh_sim, app, trace, use_reg_lva, rlvtrace, use_mem_lva, mlvtrace, rate, s, harvest, rf_entries, wf_entries, wb_entries, lbf_size,
        timer, icache_assoc, icache_block_size, icache_size, dcache_assoc, dcache_block_size, dcache_size, optimal_backup, rename, reclaim, mte, avail_addrs, icre, icwe, iclp, dcre, dcwe, dclp, rfae, rflp, lbfae, lbflp, mtae, mtre, mtwe, mtlp, flre, fllp, out_dir):
    base_name = s + "-" + str(harvest)
    path_to_output = out_dir + "/" + base_name + ".csv"

    to_run = "{} -b{} --voltage-trace={} --reg-liveness-trace={} --mem-liveness-trace={} --voltage-rate={} --scheme={} --rf-entries={} --wf-entries={} --wb-entries={} --lbf-size={} --watchdog-period={} --icache-assoc={} --icache-block-size={} --icache-size={} --dcache-assoc={} --dcache-block-size={} --dcache-size={} --map-table-entries={} --num-avail-rename-addrs={} --icache-read-energy={} --icache-write-energy={} --icache-leakage-power={} --dcache-read-energy={} --dcache-write-energy={} --dcache-leakage-power={} --rf-access-energy={} --rf-leakage-power={} --lbf-access-energy={} --lbf-leakage-power={} --map-table-access-energy={} --map-table-read-energy={} --map-table-write-energy={} --map-table-leakage-power={} --free-list-read-energy={} --free-list-leakage-power={} -o{}".format(eh_sim, app, trace, rlvtrace, mlvtrace, rate, s, rf_entries, wf_entries, wb_entries, lbf_size, timer, icache_assoc, icache_block_size, icache_size, dcache_assoc, dcache_block_size, dcache_size, mte, avail_addrs, icre, icwe, iclp, dcre, dcwe, dclp, rfae, rflp, lbfae, lbflp, mtae, mtre, mtwe, mtlp, flre, fllp, path_to_output)
    to_run = to_run.split()

    if harvest is True:
        to_run.append('--always-harvest=1')
    else:
        to_run.append('--always-harvest=0')
        
    if use_reg_lva is True:
        to_run.append('--reg-lva=1')
    else:
        to_run.append('--reg-lva=0')
        
    if use_mem_lva is True:
        to_run.append('--mem-lva=1')
    else:
        to_run.append('--mem-lva=0')
        
    if rename is True:
        to_run.append('--add-renamer=1')
    else:
        to_run.append('--add-renamer=0')

    if reclaim is True:
        to_run.append('--reclaim-addr=1')
    else:
        to_run.append('--reclaim-addr=0')

    if optimal_backup is True:
        to_run.append('--use-optimal-backup-scheme=1')
    else:
        to_run.append('--use-optimal-backup-scheme=0')

    stdout_file = open(out_dir + "/" + base_name + ".stdout", "w")
    stderr_file = open(out_dir + "/" + base_name + ".stderr", "w")
    subprocess.run(to_run, stdout=stdout_file, stderr=stderr_file)


if __name__ == "__main__":
    p = argparse.ArgumentParser(description='Run eh-sim.')
    p.add_argument('--exe', dest='eh_sim', default=None)
    p.add_argument('--scheme', dest='scheme', default=None)
    p.add_argument('--benchmark-dir', dest='benchmark_dir', default=None)
    p.add_argument('--voltage-trace-dir', dest="vtrace_dir", default=None)
    p.add_argument('--use-reg-lva', dest="use_reg_lva", action='store_true', default=False)
    p.add_argument('--reg-liveness-trace-dir', dest="reg_liveness_dir", default=None)
    p.add_argument('--use-mem-lva', dest="use_mem_lva", action='store_true', default=False)
    p.add_argument('--mem-liveness-trace-dir', dest="mem_liveness_dir", default=None)
    p.add_argument('--rf-entries', dest="rf_entries", default=8)
    p.add_argument('--wf-entries', dest="wf_entries", default=8)
    p.add_argument('--wb-entries', dest="wb_entries", default=8)
    p.add_argument('--lbf-size', dest="lbf_size", default=16)
    p.add_argument('--watchdog-period', dest="watchdog_period", default=8000)
    p.add_argument('--icache-assoc', dest="icache_assoc", default=1)
    p.add_argument('--icache-block-size', dest="icache_block_size", default=64)
    p.add_argument('--icache-size', dest="icache_size", default=2048)
    p.add_argument('--dcache-assoc', dest="dcache_assoc", default=1)
    p.add_argument('--dcache-block-size', dest="dcache_block_size", default=64)
    p.add_argument('--dcache-size', dest="dcache_size", default=2048)
    p.add_argument('--use-optimal-backup-scheme', dest="use_optimal_backup_scheme", action='store_true', default=False)
    p.add_argument('--add-renamer', dest="add_renamer", action='store_true', default=False)
    p.add_argument('--reclaim-addr', dest="reclaim_addr", action='store_true', default=False)
    p.add_argument('--map-table-entries', dest='map_table_entries', default=4)
    p.add_argument('--num-avail-rename-addrs', dest='num_avail_rename_addrs', default=8)
    p.add_argument('--icache-read-energy', dest="icache_read_energy", default=0)
    p.add_argument('--icache-write-energy', dest="icache_write_energy", default=0)
    p.add_argument('--icache-leakage-power', dest="icache_leakage_power", default=0)
    p.add_argument('--dcache-read-energy', dest="dcache_read_energy", default=0)
    p.add_argument('--dcache-write-energy', dest="dcache_write_energy", default=0)
    p.add_argument('--dcache-leakage-power', dest="dcache_leakage_power", default=0)
    p.add_argument('--rf-access-energy', dest="rf_access_energy", default=0)
    p.add_argument('--rf-leakage-power', dest="rf_leakage_power", default=0)
    p.add_argument('--lbf-access-energy', dest='lbf_access_energy', default=0)
    p.add_argument('--lbf-leakage-power', dest='lbf_leakage_power', default=0)
    p.add_argument('--map-table-access-energy', dest='map_table_access_energy', default=0)
    p.add_argument('--map-table-read-energy', dest='map_table_read_energy', default=0)
    p.add_argument('--map-table-write-energy', dest='map_table_write_energy', default=0)
    p.add_argument('--map-table-leakage-power', dest='map_table_leakage_power', default=0)
    p.add_argument('--free-list-read-energy', dest='free_list_read_energy', default=0)
    p.add_argument('--free-list-leakage-power', dest='free_list_leakage_power', default=0)
    p.add_argument('--destination', dest='output_dir', default=None)

    (args) = p.parse_args()

    if args.eh_sim is None:
        sys.exit("Error: need path to eh-sim executable.")
    if args.benchmark_dir is None:
        sys.exit("Error: no path to benchmarks.")
    if args.vtrace_dir is None:
        sys.exit("Error: no path to voltage traces.")
    if args.use_reg_lva == 1 and args.reg_liveness_dir is None:
        sys.exit("Error: no path to register liveness traces.")
    if args.use_mem_lva == 1 and args.mem_liveness_dir is None:
        sys.exit("Error: no path to memory liveness traces.")
    if args.output_dir is None:
        sys.exit("Error: no path given to output destination.")

    # the schemes to run
    #schemes = ['clank']

    # the benchmarks to run, from Matthew Hicks' version of MiBench
    #benchmark_whitelist = ['adpcm_decode', 'adpcm_encode', 'aes', 'basicmath', 'bitcount', 'blowfish',
    #                       'crc', 'dijkstra', 'fft', 'limits', 'lzfx', 'overflow', 'patricia', 'picojpeg',
    #                       'qsort', 'randmath', 'rc4', 'regress', 'rsa', 'sha', 'stringsearch', 'susan', 'vcflags']
    #
    benchmark_whitelist = ['regress']


    #benchmark_whitelist = ['adpcm_encode']

    # the voltage traces to use, from BatterylessSim
    vtrace_whitelist = ['6']
    
    # the time between samples of the voltage trace, in microseconds. The longer the time, the smaller the rate
    # of charge per cycle
    vtrace_rates = {'bec': 1, 'odab': 1, 'clank': 1, 'mem_rename': 1}

    for vtrace in vtrace_whitelist:
        for benchmark in benchmark_whitelist:
            if args.scheme == 'mem_rename':
                print("Running {}/main.bin with {}.txt voltage trace in {} scheme - gbf size={}, lbf size={}, icache size={}, dcache size={}".format(benchmark, vtrace, args.scheme, args.rf_entries, args.lbf_size, args.icache_size, args.dcache_size))
            else:
                print("Running {}/main.bin with {}.txt voltage trace in {} scheme".format(benchmark, vtrace, args.scheme))

            path_to_benchmark = args.benchmark_dir + "/" + benchmark + "/" + "main.bin"
            if not os.path.exists(path_to_benchmark):
                print("path_to_benchmark : ", path_to_benchmark)
                sys.exit('Error: path %s does not exist', path_to_benchmark)
                
            path_to_vtrace = args.vtrace_dir + "/" + vtrace + ".txt"
            if not os.path.exists(path_to_vtrace):
                sys.exit('Error: path %s does not exist', path_to_vtrace)
            
            path_to_rlvtrace = None
            path_to_mlvtrace = None
            
            if args.use_reg_lva:
                path_to_rlvtrace = args.reg_liveness_dir + "/" + benchmark + "_reg_liveness"
            if args.use_mem_lva:
                path_to_mlvtrace = args.mem_liveness_dir + "/" + benchmark + "_mem_liveness"

            if int(args.watchdog_period) < 2000000000:
                path_to_destination = args.output_dir + "/" + args.scheme + "/" + benchmark + "/" + vtrace + "/" + args.watchdog_period
            else:
                path_to_destination = args.output_dir + "/" + args.scheme + "/" + benchmark + "/" + vtrace
            os.makedirs(path_to_destination, exist_ok=True)

            run(args.eh_sim, path_to_benchmark, path_to_vtrace, args.use_reg_lva, path_to_rlvtrace, args.use_mem_lva, path_to_mlvtrace, vtrace_rates[args.scheme], args.scheme, True,
                args.rf_entries, args.wf_entries, args.wb_entries, args.lbf_size, args.watchdog_period, args.icache_assoc, args.icache_block_size, args.icache_size, args.dcache_assoc, args.dcache_block_size, args.dcache_size, args.use_optimal_backup_scheme, args.add_renamer, args.reclaim_addr, args.map_table_entries, args.num_avail_rename_addrs,
                args.icache_read_energy, args.icache_write_energy, args.icache_leakage_power, args.dcache_read_energy, args.dcache_write_energy, args.dcache_leakage_power, args.rf_access_energy, args.rf_leakage_power, args.lbf_access_energy, args.lbf_leakage_power, args.map_table_access_energy, args.map_table_read_energy, args.map_table_write_energy, args.map_table_leakage_power, args.free_list_read_energy, args.free_list_leakage_power, path_to_destination)
