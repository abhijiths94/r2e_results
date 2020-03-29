import argparse
import sys
import os
import csv
import random


def read_data_sampled(data_file, num_samples):
    with open(data_file) as csvin:
        reader = csv.reader(csvin, skipinitialspace=True)

        row_count = len(list(reader)) - 1
        if row_count < num_samples:
            num_samples = row_count

        probability = float(num_samples) / row_count

        sample_count = 0
        samples = []

        # back to the beginning of the file
        csvin.seek(0)
        next(reader)
        for row in reader:
            if random.random() < probability:
                row.append(benchmark)
                row.append(scheme)
                row.append(trace)

                samples.append(row)
                sample_count = sample_count + 1

                if sample_count >= num_samples:
                    break
        writer.writerows(samples)


def read_data(data_file):
    with open(data_file) as csvin:
        reader = csv.reader(csvin, skipinitialspace=True)

        # skip header
        next(reader)

        samples = []

        for row in reader:
            row.append(benchmark)
            row.append(scheme)
            row.append(trace)

            samples.append(row)

        writer.writerows(samples)


if __name__ == "__main__":
    p = argparse.ArgumentParser(description='Run eh-sim.')
    p.add_argument('-r', '--root-dir', dest='root_dir', default=None)
    p.add_argument('-n', '--num-samples', dest='num_samples', default=None)
    p.add_argument('-s', '--scheme', dest='scheme', default=None)
    p.add_argument('-o', '--output-file', dest='output_file', default='data.csv')

    (args) = p.parse_args()

    if args.root_dir is None:
        sys.exit("Error: no path given to data's root directory.")

    benchmark_whitelist = ['adpcm_decode', 'adpcm_encode', 'aes', 'crc', 'limits', 'lzfx', 'overflow', 'picojpeg',
                           'randmath', 'rc4', 'regress', 'rsa', 'susan', 'vcflags']

    header = "id, E, epsilon, epsilon_C, tau_B, alpha_B, energy_consumed, n_B, tau_P, tau_D, e_P, e_B, e_R, sim_p, eh_p"
    header = header.split(", ")
    header.append('benchmark')
    header.append('scheme')
    header.append('vtrace')

    with open(args.output_file, 'w', newline='') as csvout:
        writer = csv.writer(csvout, delimiter=',')
        writer.writerow(header)

        for benchmark in os.listdir(args.root_dir):
            benchmark_dir = os.path.join(args.root_dir, benchmark)

            for trace in os.listdir(benchmark_dir):
                trace_dir = os.path.join(benchmark_dir, trace)

                for file in os.listdir(trace_dir):
                    scheme = ""
                    if file.endswith('.csv'):
                        scheme = file.replace('-True.csv', '')
                    else:
                        continue

                    if args.scheme is not None and scheme != args.scheme:
                        continue

                    print("Collecting data for (benchmark, voltage trace, scheme): {}, {}, {}".format(benchmark, trace,
                                                                                                      scheme))

                    file = os.path.join(trace_dir, file)

                    if args.num_samples is None:
                        read_data(file)
                    else:
                        read_data_sampled(file, int(args.num_samples))
