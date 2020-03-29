import argparse
import sys
import subprocess
import os


def run(eh_sim, app, trace, rate, tau_b, harvest, out_dir):
    base_name = "parametric" + "-" + str(tau_b) + "-" + str(harvest)
    path_to_output = out_dir + "/" + base_name + ".csv"

    to_run = "{} -b{} --voltage-trace={} --voltage-rate={} --scheme=parametric --tau-b={} -o{}".format(eh_sim, app,
                                                                                                       trace, rate,
                                                                                                       tau_b,
                                                                                                       path_to_output)
    to_run = to_run.split()

    if harvest is True:
        to_run.append('--always-harvest=1')
    else:
        to_run.append('--always-harvest=0')

    stdout_file = open(out_dir + "/" + base_name + ".stdout", "w")
    stderr_file = open(out_dir + "/" + base_name + ".stderr", "w")
    subprocess.run(to_run, stdout=stdout_file, stderr=stderr_file)


if __name__ == "__main__":
    p = argparse.ArgumentParser(description='Run eh-sim.')
    p.add_argument('-x', '--exe', dest='eh_sim', default=None)
    p.add_argument('--benchmark-dir', dest='benchmark_dir', default=None)
    p.add_argument('--voltage-trace-dir', dest="vtrace_dir", default=None)
    p.add_argument('-d', '--destination', dest='output_dir', default=None)

    (args) = p.parse_args()

    if args.eh_sim is None:
        sys.exit("Error: need path to eh-sim executable.")
    if args.benchmark_dir is None:
        sys.exit("Error: no path to benchmarks.")
    if args.vtrace_dir is None:
        sys.exit("Error: no path to voltage traces.")
    if args.output_dir is None:
        sys.exit("Error: no path given to output destination.")

    # the benchmarks to run, from Matthew Hicks' version of MiBench
    benchmark_whitelist = ['adpcm_decode', 'adpcm_encode', 'aes', 'crc', 'limits', 'lzfx', 'overflow',
                           'randmath', 'rc4', 'regress', 'susan', 'vcflags']

    # the voltage traces to use, from BatterylessSim
    vtrace_whitelist = ['6']
    # different backup periods (in cycles) to try
    backup_periods = list(range(250, 3000, 250))

    for vtrace in vtrace_whitelist:
        for benchmark in benchmark_whitelist:
            for bperiod in backup_periods:
                print("Running {}.bin with {}.txt voltage trace with tau_b={}".format(benchmark, vtrace, bperiod))

                path_to_benchmark = args.benchmark_dir + "/" + benchmark + ".bin"
                path_to_vtrace = args.vtrace_dir + "/" + vtrace + ".txt"

                path_to_destination = args.output_dir + "/" + benchmark + "/" + vtrace
                os.makedirs(path_to_destination, exist_ok=True)

                run(args.eh_sim, path_to_benchmark, path_to_vtrace, 1, bperiod, True, path_to_destination)
