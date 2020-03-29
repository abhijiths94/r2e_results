import argparse
import sys
import os
import csv


def calculate_energy(capacitance, maximum_voltage):
    return 0.5 * capacitance * maximum_voltage * maximum_voltage * 1e9


class Scheme:
    name = ''
    energy_supply = 0.0
    backup_time = 0
    restore_time = 0
    sigma_backup = 0
    omega_backup = 0.0
    arch_backup = 0
    alpha_backup = 0
    sigma_restore = 0
    omega_restore = 0.0
    arch_restore = 0
    alpha_restore = 0


BEC = Scheme()
BEC.name = 'bec'
BEC.energy_supply = 0.25 + 0.125 + 0.03125
BEC.backup_time = 2
BEC.restore_time = 1
BEC.sigma_backup = 6
BEC.omega_backup = 0.125 / (4 * 4)
BEC.arch_backup = 16
BEC.alpha_backup = 0
BEC.sigma_restore = 4
BEC.omega_restore = 0.25 / 4
BEC.arch_restore = 4
BEC.alpha_restore = 0

CLANK = Scheme()
CLANK.name = 'clank'
CLANK.energy_supply = calculate_energy(470e-9, 4.5)
CLANK.backup_time = 40
CLANK.restore_time = 40
CLANK.sigma_backup = 0.5
CLANK.omega_backup = 1e9 * 0.7e-6 * 3.6 * 3.94e-3
CLANK.arch_backup = 80
CLANK.alpha_backup = 0
CLANK.sigma_restore = 0.5
CLANK.omega_restore = CLANK.omega_backup
CLANK.arch_restore = 80
CLANK.alpha_restore = 0

PARAMETRIC = Scheme()
PARAMETRIC.name = 'parametric'
PARAMETRIC.energy_supply = calculate_energy(10e-6, 4.5)
PARAMETRIC.sigma_backup = 0.5
PARAMETRIC.omega_backup = 1e9 * 0.7e-6 * 3.6 * 3.94e-3
PARAMETRIC.arch_backup = 80
PARAMETRIC.alpha_backup = 0
PARAMETRIC.sigma_restore = 0.5
PARAMETRIC.omega_restore = PARAMETRIC.omega_backup
PARAMETRIC.arch_restore = 80
PARAMETRIC.alpha_restore = 0


def eh_model_estimate(active_id, energy_supply, epsilon, epsilon_charge, tau_backup, alpha_backup):
    tau_dead = tau_backup / 2

    # calculate dead energy - Equation 5
    energy_dead = (epsilon - epsilon_charge) * tau_dead

    energy_restore = 0
    if active_id > 0:
        # calculate restore energy - Equation 8
        energy_restore = (SCHEME.omega_restore - epsilon_charge / SCHEME.sigma_restore) * (SCHEME.arch_restore +
                                                                                           SCHEME.alpha_restore *
                                                                                           tau_dead)
    # numerator of Equation 9
    numerator = 1 - (energy_dead / energy_supply) - (energy_restore / energy_supply)

    # calculate backup energy - Equation 4
    energy_backup = (SCHEME.omega_backup - epsilon_charge / SCHEME.sigma_backup) * (SCHEME.arch_backup + alpha_backup
                                                                                    * tau_backup)
    # denominator of Equation 9 (assume e_Theta is 0)
    denominator = (1 + energy_backup / ((epsilon - epsilon_charge) * tau_backup)) * (1 - epsilon_charge / epsilon)

    return numerator / denominator


def read_data(data_file):
    with open(data_file) as csvin:
        reader = csv.DictReader(csvin, skipinitialspace=True)

        for row in reader:
            forward_cycles = int(row['tau_P'])
            if forward_cycles == 0:
                continue

            backup_count = int(row['n_B'])
            if backup_count == 0:
                continue

            tau_b = float(row['tau_B'])
            alpha_backup = float(row['alpha_B'])

            if scheme == 'parametric':
                backup_time = CLANK.backup_time + (SCHEME.omega_backup * alpha_backup * tau_b)
            else:
                backup_time = SCHEME.backup_time

            backup_time = backup_time * backup_count

            dead_cycles = int(row['tau_D'])
            # calculate total cycles in active period
            active_time = backup_time + SCHEME.restore_time + forward_cycles + dead_cycles

            # calculate charging energy per cycle (epc)
            active_energy = float(row['E'])
            energy_charged = 0
            if active_energy >= SCHEME.energy_supply:
                energy_charged = active_energy - SCHEME.energy_supply

                if scheme != "clank":
                    active_energy = SCHEME.energy_supply
                else:
                    active_energy = SCHEME.energy_supply - 794.304

            epc = energy_charged / active_time

            # calculate execution energy per cycle (epsilon)
            forward_energy = float(row['e_P'])
            epsilon = forward_energy / forward_cycles

            if epc > epsilon:
                continue

            if scheme == 'parametric':
                tau_b = parametric_tau
            elif tau_b <= 0:
                continue

            if scheme == "clank" and (benchmark == "overflow" or benchmark == "vcflags"):
                # these benchmarks finish in one active period and typically have significant energy remaining
                active_energy = SCHEME.energy_supply - 1233.858

            active_id = int(row['id'])

            if scheme == 'parametric':
                model_progress = eh_model_estimate(active_id, active_energy, epsilon, epc, tau_b, alpha_backup)
            else:
                model_progress = eh_model_estimate(active_id, active_energy, epsilon, epc, tau_b, SCHEME.alpha_backup)

            simulator_progress = float(row['p'])

            tau_d = float(row['tau_D'])
            to_write = [benchmark, trace, scheme, active_id, active_energy, epsilon, epc, tau_b, tau_d, model_progress,
                        simulator_progress]
            writer.writerow(to_write)


if __name__ == "__main__":
    p = argparse.ArgumentParser(description='Run eh-sim.')
    p.add_argument('-r', '--root-dir', dest='root_dir', default=None)
    p.add_argument('-n', '--num-samples', dest='num_samples', default=1000)
    p.add_argument('-o', '--output-file', dest='output_file', default='model-vs-sim.csv')

    (args) = p.parse_args()

    if args.root_dir is None:
        sys.exit("Error: no path given to data's root directory.")

    benchmark_whitelist = ['adpcm_decode', 'adpcm_encode', 'aes', 'crc', 'limits', 'lzfx', 'overflow', 'picojpeg',
                           'randmath', 'rc4', 'regress', 'rsa', 'susan', 'vcflags']

    header = ['benchmark', 'vtrace', 'scheme', 'id', 'E', 'epsilon', 'epsilon_C', 'tau_B', 'tau_D', 'model_p',
              'simulator_p']

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
                        scheme = file.replace('.csv', '')

                        scheme_array = scheme.split('-')

                        scheme = scheme_array[0]
                        if scheme == 'odab':
                            continue

                        parametric_tau = 0
                        if scheme == 'parametric':
                            parametric_tau = int(scheme_array[1])
                    else:
                        continue

                    print("Collecting data for (benchmark, voltage trace, scheme): {}, {}, {}".format(benchmark, trace,
                                                                                                      scheme))

                    if scheme == 'clank':
                        SCHEME = CLANK
                    elif scheme == 'bec':
                        SCHEME = BEC
                    elif scheme == 'parametric':
                        SCHEME = PARAMETRIC

                    file = os.path.join(trace_dir, file)
                    read_data(file)
