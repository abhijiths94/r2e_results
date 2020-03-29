#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Created on Wed Jul 10 23:21:35 2019

@author: obi
"""

import argparse, sys, os, re, csv, math
import matplotlib.pyplot as plt
import numpy as np
import pandas as pd
from collections import OrderedDict
from statistics import mean 

if __name__ == "__main__":
    p = argparse.ArgumentParser(description='Thumbulator trace parser.')
    p.add_argument('--trace-dirlist', dest='trace_dirlist', nargs='+', help='<Required> Set flag', required=True)
    p.add_argument('--plot-dirty_dirtylive', dest='plot_ddl', action='store_true', default=False)
    p.add_argument('--plot-num_insns_between_backups', dest='plot_nibb', action='store_true', default=False)
    p.add_argument('--plot-forward_progress', dest='plot_fp', action='store_true', default=False)
    p.add_argument('--plot-num_backups', dest='plot_nb', action='store_true', default=False)
    p.add_argument('--plot-num_addr_renamed', dest='plot_nar', action='store_true', default=False)
    p.add_argument('--plot-num_renamed_reclaimed_addrs', dest='plot_nrra', action='store_true', default=False)
    p.add_argument('--plot-results', dest='plot_results', action='store_true', default=False)
    p.add_argument('--plot-mr-results', dest='plot_mr_results', action='store_true', default=False)
    p.add_argument('--destination', dest='output_dir', default=None)

    (args) = p.parse_args()
    
    if args.trace_dirlist is None:
        sys.exit("Error: no path given to trace directory.")
    if args.output_dir is None:
        sys.exit("Error: no path given to output destination.")
    
    benchmark_whitelist = ['adpcm_decode', 'adpcm_encode', 'aes', 'bitcount', 'blowfish',
                           'crc', 'dijkstra', 'limits', 'overflow', 'patricia',
                           'randmath', 'rc4', 'regress', 'rsa', 'susan', 'vcflags']
    
    benchmark_whitelist = ['adpcm_decode', 'adpcm_encode', 'bitcount',
                           'dijkstra', 'qsort', 'regress', 'stringsearch', 'susan']
        
    if not os.path.exists(args.output_dir):
        os.makedirs(args.output_dir)
    
    if args.plot_ddl:        
        for trace_dir in args.trace_dirlist:
            cycle = []
            backup_cycle = []
            num_dirty_bytes = []
            num_dirty_live_bytes = []
            
            for benchmark in benchmark_whitelist:
                cycle.clear()
                backup_cycle.clear()
                num_dirty_bytes.clear()
                num_dirty_live_bytes.clear()
                path_to_trace = trace_dir + '/' + benchmark + '/test/clank-True.stdout'
                print("Plotting for {}".format(path_to_trace))
                path_to_output = args.output_dir + '/' + benchmark + '.csv'
                print("Writing to {}".format(path_to_output))
                with open(path_to_trace, "r") as fptr:
                    with open(path_to_output, "w") as csvfile:
                        fieldnames = ['cycle', 'dirty bytes', 'live (dirty) bytes']
                        writer = csv.DictWriter(csvfile, fieldnames=fieldnames)
                        writer.writeheader()
                        item_per_cycle = OrderedDict()
                        for line in fptr.readlines():
                            if line.startswith('Cycle'):
                                number_list = re.findall(r'[0-9]+', line)
                                if len(number_list) > 1: 
                                    item_per_cycle[int(number_list[0])] = (int(number_list[1]), int(number_list[2]))
                                else:
                                    if 'backup' in line:
                                        item_per_cycle[int(number_list[0])] = (0, 0)
                                        backup_cycle.append(int(number_list[0]))
                                        for key, value in item_per_cycle.items():
                                            cycle.append(key)
                                            num_dirty_bytes.append(value[0])
                                            num_dirty_live_bytes.append(value[1])
                                            writer.writerow({'cycle': key, 'dirty bytes': value[0], 'live (dirty) bytes': value[1]})
                                        item_per_cycle.clear()
                    csvfile.close()
    
                fptr.close()
                
                plt.title('Benchmark:' + benchmark)
                plt.plot(cycle, num_dirty_bytes, marker='o', linestyle='-', color='r', label='dirty')
                plt.plot(cycle, num_dirty_live_bytes, marker='x', linestyle='-', color='g', label='live (dirty)')
                count = 0
                for cyc in backup_cycle:
                    count += 1
                    if count == len(backup_cycle):
                        plt.axvline(x=cyc, linestyle='--', label='backup')
                    else:
                        plt.axvline(x=cyc, linestyle='--')
                plt.legend(loc = 'upper right')
                plt.ylim(bottom=0)
                plt.xticks(np.arange(0, max(cycle)+1, 20))
                plt.ylabel('Bytes')
                plt.xlabel('Cycles')
                plt.show()
    
    if args.plot_nibb:
        for trace_dir in args.trace_dirlist:
            num_insns = []
            net_insns = []
            net_backups = []
            mins = []
            means = []
            for benchmark in benchmark_whitelist:
                num_insns.clear()
                path_to_trace = trace_dir + '/' + benchmark + '/test/clank-True.stdout'
                print("Plotting for {}".format(path_to_trace))
                with open(path_to_trace, "r") as fptr:
                    item_per_cycle = OrderedDict()
                    count = 0
                    for line in fptr.readlines():
                        if line.startswith('backup'):
                            if count > 0:
                                num_insns.append(int(re.findall(r'[0-9]+', line)[0]))
                            count += 1
                    
                    net_insns.append(np.sum(num_insns))
                    net_backups.append(len(num_insns))
                    means.append(np.mean(num_insns))
                    mins.append(np.min(num_insns))
                fptr.close()
        print(net_insns)
        print(net_backups)
        
        print("net_min:", np.min(mins))
        print("net_avg:", np.sum(net_insns)/np.sum(net_backups))
        
        index = np.arange(len(benchmark_whitelist))
        bar_width = 0.5
        fig, ax = plt.subplots()
        
        ax.bar(index, means, bar_width)
        ax.set_ylabel('avg. number of instructions between backups')
        ax.set_xticks(index)
        ax.set_yticks(np.arange(0, max(means)+2000, 2000))
        ax.set_xticklabels(benchmark_whitelist, rotation=30)
        plt.show()
        
    if args.plot_fp:
        cache_size = np.array(['64', '128', '256', '512', '1024', '2048'])
        rf_size = np.array(['0', '1', '2', '4', '8', '32'])
        for trace_dir in args.trace_dirlist:
            for benchmark in benchmark_whitelist:
                print("Plotting for {}".format(benchmark))
                row = 0
                z1 = np.zeros(shape=(rf_size.size, cache_size.size))
                z2 = np.zeros(shape=(rf_size.size, cache_size.size))
                z3 = np.zeros(shape=(rf_size.size, cache_size.size))
                z4 = np.zeros(shape=(rf_size.size, cache_size.size))
                for i in rf_size:
                    col = 0
                    for j in cache_size:
                        path_to_trace = trace_dir + '/rf_' + i + '_wb_' + j + '/mem_rename/' + benchmark + '/test/mem_rename-True.csv'
                        
                        if not os.path.exists(path_to_trace):
                            sys.exit('Error: path %s does not exist', path_to_trace)
                        # forward progress
                        z1[row][col] = np.genfromtxt(path_to_trace, delimiter=',', skip_header=1, usecols=[13])
                        # number of backups
                        z2[row][col] = np.genfromtxt(path_to_trace, delimiter=',', skip_header=1, usecols=[7])
                        # application state per backup (bytes/cycle)
                        z3[row][col] = np.genfromtxt(path_to_trace, delimiter=',', skip_header=1, usecols=[5])
                        # cycles between backups
                        z4[row][col] = np.genfromtxt(path_to_trace, delimiter=',', skip_header=1, usecols=[4])
                        z5 = np.multiply(z3, z4)
                        
                        col +=1
                    row += 1
                
                fig, axes = plt.subplots(ncols=3, figsize=(15, 4))
                
                ax1, ax2, ax3 = axes
                ax1.set_xticks(np.arange(len(cache_size)))
                ax1.set_yticks(np.arange(len(rf_size)))
                ax1.set_xticklabels(cache_size)
                ax1.set_yticklabels(rf_size)
                ax1.set_xlabel('cache size (bytes)')
                ax1.set_ylabel('rf size (bits)')
                im1 = ax1.imshow(z1, cmap='jet', origin='lower')
                cbar1 = fig.colorbar(im1, ax=ax1)
                cbar1.set_label('forward progress', rotation=90)
                
                ax2.set_xticks(np.arange(len(cache_size)))
                ax2.set_yticks(np.arange(len(rf_size)))
                ax2.set_xticklabels(cache_size)
                ax2.set_yticklabels(rf_size)
                ax2.set_xlabel('cache size (bytes)')
                ax2.set_ylabel('rf size (bits)')
                im2 = ax2.imshow(z2, cmap='jet', origin='lower')
                cbar2 = fig.colorbar(im2, ax=ax2)
                cbar2.set_label('number of backups', rotation=90)
                
                ax3.set_xticks(np.arange(len(cache_size)))
                ax3.set_yticks(np.arange(len(rf_size)))
                ax3.set_xticklabels(cache_size)
                ax3.set_yticklabels(rf_size)
                ax3.set_xlabel('cache size (bytes)')
                ax3.set_ylabel('rf size (bits)')
                im3 = ax3.imshow(z5, cmap='jet', origin='lower')
                cbar3 = fig.colorbar(im3, ax=ax3)
                cbar3.set_label('app state per backup (bytes/cycle)', rotation=90)
                
                fig.tight_layout()
                fig.suptitle(benchmark, y=1)
                path_to_output = args.output_dir + '/' + benchmark + '_fp.png'
                plt.savefig(path_to_output)
                plt.close(fig)
    
    if args.plot_nb:
        cache_size = np.array(['64', '128', '256', '512', '1024', '2048'])
        rf_size = np.array(['0', '1', '2', '4', '8', '32'])
        for trace_dir in args.trace_dirlist:    
            z6 = np.zeros(shape=(rf_size.size, cache_size.size, len(benchmark_whitelist)))
            z7 = np.zeros(shape=(rf_size.size, cache_size.size, len(benchmark_whitelist)))
            
            sl = 0 # slice of 3D numpy array
            for i in rf_size:
                row = 0
                for j in cache_size:
                    col = 0
                    for benchmark in benchmark_whitelist:
                        print("Plotting for {}".format(benchmark))                        
                        path_to_outfile = trace_dir + '/rf_' + i + '_wb_' + j + '/mem_rename/' + benchmark + '/test/mem_rename-True.stdout'
                        if not os.path.exists(path_to_outfile):
                            sys.exit('Error: path %s does not exist', path_to_outfile)
                        print("Parsing {}".format(path_to_outfile))
                        
                        with open(path_to_outfile, "r") as fptr:
                            for line in fptr.readlines():
                                if 'true positives' in line:
                                    # number of true positives due to rf bloom filter (mem_rename scheme)
                                    z6[sl][row][col] = line.split(':')[-1]          
                                if 'false positives' in line:
                                    # number of false positives due to rf bloom filter (mem_rename scheme)
                                    z7[sl][row][col] = line.split(':')[-1]
                        col +=1
                    row +=1
                
                index = np.arange(len(benchmark_whitelist))
                width = 0.15
                opacity = 1
                
                fig, ax = plt.subplots(figsize=(15, 4))
                p1 = ax.bar(index - 2 * width, z6[sl][0], width, label='64 (TP)', edgecolor='black', align='center')
                p2 = ax.bar(index - width, z6[sl][1], width, label='128 (TP)', edgecolor='black', align='center')
                p3 = ax.bar(index, z6[sl][2], width, label='256 (TP)', edgecolor='black', align='center')
                p4 = ax.bar(index + width, z6[sl][3], width, label='512 (TP)', edgecolor='black', align='center')
                p5 = ax.bar(index + 2 * width, z6[sl][4], width, label='1024 (TP)', edgecolor='black', align='center')
                p6 = ax.bar(index + 3 * width, z6[sl][5], width, label='2048 (TP)', edgecolor='black', align='center')
                
                q1 = ax.bar(index - 2 * width, z7[sl][0], width, bottom=z6[sl][0], label='64 (FP)', hatch='//', alpha=opacity, edgecolor='black', align='center')
                q2 = ax.bar(index - width, z7[sl][1], width, bottom=z6[sl][1], label='128 (FP)', hatch='//', alpha=opacity, edgecolor='black', align='center')
                q3 = ax.bar(index, z7[sl][2], width, bottom=z6[sl][2], label='256 (FP)', hatch='//', alpha=opacity, edgecolor='black', align='center')
                q4 = ax.bar(index + width, z7[sl][3], width, bottom=z6[sl][3], label='512 (FP)', hatch='//', alpha=opacity, edgecolor='black', align='center')
                q5 = ax.bar(index + 2 * width, z7[sl][4], width, bottom=z6[sl][4], label='1024 (FP)', hatch='//', alpha=opacity, edgecolor='black', align='center')
                q6 = ax.bar(index + 3 * width, z7[sl][5], width, bottom=z6[sl][5], label='2048 (FP)', hatch='//', alpha=opacity, edgecolor='black', align='center')
            
                plt.rcParams.update({'font.size': 8})
                # Add some text for labels, title and custom x-axis tick labels, etc.
                ax.set_ylabel('number of backups')
                ax.set_title('rf filter size = '+ i + ' bits')
                ax.set_xticks(index)
                ax.set_xticklabels(benchmark_whitelist, rotation=35)
                ax.legend(title='cache size (bytes)', fancybox=True)
                fig.tight_layout()
                
                path_to_output = args.output_dir + '/' + 'num_backups_rf' + i + '.png'
                plt.savefig(path_to_output)
                plt.close(fig)
            
                sl +=1
                
            z6 = np.zeros(shape=(cache_size.size, rf_size.size, len(benchmark_whitelist)))
            z7 = np.zeros(shape=(cache_size.size, rf_size.size, len(benchmark_whitelist)))
            
            sl = 0 # slice of 3D numpy array
            for i in cache_size:
                row = 0
                for j in rf_size:
                    col = 0
                    for benchmark in benchmark_whitelist:
                        print("Plotting for {}".format(benchmark))
                        path_to_outfile = trace_dir + '/rf_' + j + '_wb_' + i + '/mem_rename/' + benchmark + '/test/mem_rename-True.stdout'
                        if not os.path.exists(path_to_outfile):
                            sys.exit('Error: path %s does not exist', path_to_outfile)    
                        print("Parsing {}".format(path_to_outfile))
                        
                        with open(path_to_outfile, "r") as fptr:
                            for line in fptr.readlines():
                                if 'true positives' in line:
                                    # number of true positives due to rf bloom filter (mem_rename scheme)
                                    z6[sl][row][col] = line.split(':')[-1]          
                                if 'false positives' in line:
                                    # number of false positives due to rf bloom filter (mem_rename scheme)
                                    z7[sl][row][col] = line.split(':')[-1]
                        col +=1
                    row +=1
                
                index = np.arange(len(benchmark_whitelist))
                width = 0.15
                opacity = 1
                
                fig, ax = plt.subplots(figsize=(15, 4))
                p1 = ax.bar(index - 2 * width, z6[sl][0], width, label='0 (TP)', edgecolor='black', align='center')
                p2 = ax.bar(index - width, z6[sl][1], width, label='1 (TP)', edgecolor='black', align='center')
                p3 = ax.bar(index, z6[sl][2], width, label='2 (TP)', edgecolor='black', align='center')
                p4 = ax.bar(index + width, z6[sl][3], width, label='4 (TP)', edgecolor='black', align='center')
                p5 = ax.bar(index + 2 * width, z6[sl][4], width, label='8 (TP)', edgecolor='black', align='center')
                p6 = ax.bar(index + 3 * width, z6[sl][5], width, label='32 (TP)', edgecolor='black', align='center')
                
                q1 = ax.bar(index - 2 * width, z7[sl][0], width, bottom=z6[sl][0], label='0 (FP)', hatch='//', alpha=opacity, edgecolor='black', align='center')
                q2 = ax.bar(index - width, z7[sl][1], width, bottom=z6[sl][1], label='1 (FP)', hatch='//', alpha=opacity, edgecolor='black', align='center')
                q3 = ax.bar(index, z7[sl][2], width, bottom=z6[sl][2], label='2 (FP)', hatch='//', alpha=opacity, edgecolor='black', align='center')
                q4 = ax.bar(index + width, z7[sl][3], width, bottom=z6[sl][3], label='4 (FP)', hatch='//', alpha=opacity, edgecolor='black', align='center')
                q5 = ax.bar(index + 2 * width, z7[sl][4], width, bottom=z6[sl][4], label='8 (FP)', hatch='//', alpha=opacity, edgecolor='black', align='center')
                q6 = ax.bar(index + 3 * width, z7[sl][5], width, bottom=z6[sl][5], label='32 (FP)', hatch='//', alpha=opacity, edgecolor='black', align='center')
            
                # Add some text for labels, title and custom x-axis tick labels, etc.
                ax.set_ylabel('number of backups')
                ax.set_title('cache size = '+ i + ' bytes')
                ax.set_xticks(index)
                ax.set_xticklabels(benchmark_whitelist, rotation=45)
                ax.legend(title='rf filter size (bits)', fancybox=True)
                fig.tight_layout()
                
                path_to_output = args.output_dir + '/' + 'num_backups_wb' + i + '.png'
                plt.savefig(path_to_output)
                plt.close(fig)
            
                sl +=1
    
    if args.plot_nar:
        #cache_size = np.array(['2048'])
        watchdog_timer = np.array(['4096', '8192'])
        for trace_dir in args.trace_dirlist:
            z2 = {}
            z1_max = []
            z1_mean = []
            for timer in watchdog_timer:
                z1 = OrderedDict()
                z1_max.clear()
                z1_mean.clear()
                z2.clear()
                
#                plt.title('watchdog timer:' + timer)
                for benchmark in benchmark_whitelist:
                
                    z1.clear()
                    path_to_outfile = trace_dir + '/wb_2048' + '/mem_rename/' + benchmark + '/test/' + timer + '/mem_rename-True.stdout'
                    if not os.path.exists(path_to_outfile):
                        sys.exit('Error: path %s does not exist', path_to_outfile)    
                    print("Parsing {}".format(path_to_outfile))
                    
                    with open(path_to_outfile, "r") as fptr:
                        for line in fptr.readlines():
                            if line.startswith('Cycle'):
                                number_list = re.findall(r'[0-9]+', line)
                                z1[int(number_list[0])] = int(number_list[1])
                    
                    z1_max.append(list(z1.values())[-1])
                    z1_mean.append(math.ceil(mean(list(z1.values()))))
                    
#                    plt.plot(list(z1.keys()), list(z1.values()), label='watchdog timer:' + timer)
                
                z2 = {'benchmark':benchmark_whitelist, 'mean':z1_mean, 'max':z1_max}
                df = pd.DataFrame(z2)
                df.to_csv(args.output_dir + '/' + timer + '_map_table_entries_used.csv')
                print("Writing csv file for {} done".format(benchmark))                
            
#                plt.ylim(bottom=0)
#                plt.ylabel('Number of map table entries')
#                plt.xlabel('Cycles')
#                plt.locator_params(tight=True, nbins='auto')
#                plt.legend(loc = 'upper left')
#                path_to_output = args.output_dir + '/' + benchmark + '.png'
#                plt.savefig(path_to_output)
#                plt.close()
#                print("Plotting for {} done".format(benchmark))
                
    if args.plot_nrra:
        for trace_dir in args.trace_dirlist:
            watchdog_timer = np.array(['4096', '8192'])
            for time in watchdog_timer:
                z2 = {}
                num_renamed_list = []
                num_reclaimed_list = []
                for benchmark in benchmark_whitelist:
                    path_to_outfile = trace_dir + '/wb_2048/mem_rename/' + benchmark + '/test/' + time + '/mem_rename-True.stdout'
                    if not os.path.exists(path_to_outfile):
                        sys.exit('Error: path %s does not exist', path_to_outfile)
                    print("Parsing {}".format(path_to_outfile))
                    
                    with open(path_to_outfile, "r") as fptr:
                        for line in fptr.readlines():
                            if 'renamed' in line:
                                num_renamed = re.findall(r'[0-9]+', line)
                                num_renamed_list.append(int(num_renamed[0]))
                            if 'reclaimed' in line:
                                num_reclaimed = re.findall(r'[0-9]+', line)
                                num_reclaimed_list.append(int(num_reclaimed[0]))
                            
                z2 = {'benchmark':benchmark_whitelist, 'renamed':num_renamed_list, 'reclaimed':num_reclaimed_list}
                df = pd.DataFrame(z2)
                df.to_csv(args.output_dir + '/' + 'data_' + time + '.csv')
                print("Writing csv file for {} cycle timer done".format(time))

    if args.plot_results:
        gbf_size = np.array(['0', '8', '32'])
        lbf_size = np.array(['0', '4', '8', '16'])

        z6 = np.zeros(shape=(gbf_size.size, lbf_size.size, len(benchmark_whitelist)))
        z7 = np.zeros(shape=(gbf_size.size, lbf_size.size, len(benchmark_whitelist)))

        sl = 0 # slice of 3D numpy array
        for i in gbf_size:
            row = 0
            for j in lbf_size:
                col = 0
                for benchmark in benchmark_whitelist:
                    print("Plotting for {}".format(benchmark))                        
                    path_to_trace = args.trace_dirlist[0] + '/gbf' + i + '_lbf' + j + '/mem_rename/' + benchmark + '/6/8000/mem_rename-True.csv'
                    if not os.path.exists(path_to_trace):
                        sys.exit('Error: path %s does not exist', path_to_trace)
                    print("Parsing {}".format(path_to_trace))

                    z6[sl][row][col] = np.sum(np.genfromtxt(path_to_trace, delimiter=',', skip_header=1, usecols=[6]))

                    path_to_trace = args.trace_dirlist[1] + '/gbf' + i + '_lbf' + j + '/mem_rename/' + benchmark + '/6/8000/mem_rename-True.csv'
                    if not os.path.exists(path_to_trace):
                        sys.exit('Error: path %s does not exist', path_to_trace)
                    print("Parsing {}".format(path_to_trace))

                    z7[sl][row][col] = np.sum(np.genfromtxt(path_to_trace, delimiter=',', skip_header=1, usecols=[6]))

                    col += 1
                row += 1

            index = np.arange(len(benchmark_whitelist))
            width = 0.1
            opacity = 1
            
            fig, ax = plt.subplots(figsize=(25, 4))

            plt.rcParams.update({'font.size': 8})
            p1 = ax.bar(index - 3*width, z6[sl][0], width, label='0(MR)', edgecolor='black', align='center')
            p2 = ax.bar(index - 2*width, z7[sl][0], width, label='0(BL)', edgecolor='black', align='center')
            p3 = ax.bar(index - width, z6[sl][1], width, label='4(MR)', edgecolor='black', align='center')
            p4 = ax.bar(index, z7[sl][1], width, label='4(BL)', edgecolor='black', align='center')
            p5 = ax.bar(index + width, z6[sl][2], width, label='8(MR)', edgecolor='black', align='center')
            p6 = ax.bar(index + 2*width, z7[sl][2], width, label='8(BL)', edgecolor='black', align='center')
            p7 = ax.bar(index + 3*width, z6[sl][3], width, label='16(MR)', edgecolor='black', align='center')
            p8 = ax.bar(index + 4*width, z7[sl][3], width, label='16(BL)', edgecolor='black', align='center')

            # Add some text for labels, title and custom x-axis tick labels, etc.
            ax.set_ylabel('total energy consumed (nJ)')
            ax.set_title('gbf entries = '+ i)
            ax.set_xticks(index)
            ax.set_xticklabels(benchmark_whitelist, rotation=45)
            ax.legend(title='lbf entries', fancybox=True)
            fig.tight_layout()
            
            path_to_output = args.output_dir + '/' + 'MRvsBL_TotEnergy' + i + '.png'
            plt.savefig(path_to_output)
            plt.close(fig)

            sl += 1


    if args.plot_mr_results:
        gbf_size = np.array(['0', '8', '32'])
        lbf_size = np.array(['0', '4', '8', '16'])
        mt_size = np.array(['32', '64', '128', '256', '512'])

        z6 = np.zeros(shape=(gbf_size.size, lbf_size.size, len(benchmark_whitelist))) # baseline ave. forward progress
        z7 = np.zeros(shape=(gbf_size.size, lbf_size.size, mt_size.size, len(benchmark_whitelist))) # memory rename ave. forward progress

        dim0 = 0
        for i in gbf_size:
            dim1 = 0
            for j in lbf_size:
                dim2 = 0
                for benchmark in benchmark_whitelist:
                    path_to_trace = args.trace_dirlist[0] + benchmark + '/clank_cache/gbf' + i + '_lbf' + j + '/6/8000/mem_rename-True.csv'
                    if not os.path.exists(path_to_trace):
                        sys.exit('Error: path %s does not exist', path_to_trace)
                    print("Parsing {}".format(path_to_trace))

                    z6[dim0][dim1][dim2] = np.average(np.genfromtxt(path_to_trace, delimiter=',', skip_header=1, usecols=[13]))

                    dim2 += 1
                dim1 += 1
            dim0 += 1


        dim0 = 0
        for i in gbf_size:
            dim1 = 0
            for j in lbf_size:
                dim2 = 0
                for k in mt_size:
                    dim3 = 0
                    for benchmark in benchmark_whitelist:
                        path_to_trace = args.trace_dirlist[0] + benchmark + '/mem_rename/mt' + k + '/gbf' + i + '_lbf' + j + '/6/8000/mem_rename-True.csv'
                        if not os.path.exists(path_to_trace):
                            sys.exit('Error: path %s does not exist', path_to_trace)
                        print("Parsing {}".format(path_to_trace))

                        z7[dim0][dim1][dim2][dim3] = np.average(np.genfromtxt(path_to_trace, delimiter=',', skip_header=1, usecols=[13]))

                        dim3 += 1
                    dim2 += 1

                index = np.arange(len(benchmark_whitelist))
                width = 0.1
                opacity = 1
                
                fig, ax = plt.subplots(figsize=(25, 4))

                plt.rcParams.update({'font.size': 8})
                p1 = ax.bar(index - 3*width, z6[dim0][dim1], width, label='BL', edgecolor='black', align='center')
                p2 = ax.bar(index - 2*width, z7[dim0][dim1][0], width, label='32(MR)', edgecolor='black', align='center')
                p3 = ax.bar(index - width, z7[dim0][dim1][1], width, label='64(MR)', edgecolor='black', align='center')
                p4 = ax.bar(index, z7[dim0][dim1][2], width, label='128(MR)', edgecolor='black', align='center')
                p5 = ax.bar(index + width, z7[dim0][dim1][3], width, label='256(MR)', edgecolor='black', align='center')
                p6 = ax.bar(index + 2*width, z7[dim0][dim1][4], width, label='512(MR)', edgecolor='black', align='center')

                # Add some text for labels, title and custom x-axis tick labels, etc.
                ax.set_ylabel('forward progress')
                ax.set_title('gbf entries = ' + i + ', lbf entries = ' + j)
                ax.set_xticks(index)
                ax.set_xticklabels(benchmark_whitelist, rotation=45)
                ax.legend(title='mt entries', fancybox=True)
                fig.tight_layout()
                
                path_to_output = args.output_dir + '/' + 'fp_gbf' + i + '_lbf' + j + '.png'
                plt.savefig(path_to_output)
                plt.close(fig)

                dim1 += 1
            dim0 += 1
                
    print("Plotting complete..exiting")
