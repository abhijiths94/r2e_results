
'''
first traverse traceA, find lastReads as normal for the given addr:
zero out bit for trace_liveness arr after lastreads
can now plot liveness afterwards 
for every write to a reg, traverse back upwards until last read if any
mark it as dead between last read and new write
'''
REGS = 16
CHECK_MEM = 1
PREDICT_LIVENESS = 1
SAVEPERCENTLIVEREGS = 0
SAVEPERCENTLIVEMEMADDRS = 0
SAVETABLEUSAGE = 1
SAVESTATSBEST = 0
SAVETRACEARR = 0
RETRIEVE_TRACEARR = 0
TESTPRED_TYPES_PARAMS = 1

# bimodal: BimodalTableSizeBits
# gshare: gs_PHT_BITS, GHR_gs_len
# perc: GHR_perc_len, NumW, BudgetBits
# choice: choice_table_BITS, gs_PHT_BITS, GHR_len, NumW, PercBudgetBits



# REGSTOTRACKLIVENESS = list(range(REGS - 3))
REGSTOTRACKLIVENESS = [13,]
# pc,lr,sp,r12,fp,sl,r9,r8,r7,r6,r5,r4,r3,r2,r1,r0
# sl:	stack limit
toPredictMASK_reg = 0b0001111111111111
toPredictMASK_mem = set()

RAM_SIZE = (1 << 23) >> 2 # (8MB >> 2) addresses

def visualizeMem(traceArr):
	res = []
	for t in traceArr:
		cycle, PC,	memAddrsRead, memAddrsWritten, liveMemAddrs, toPredict = t

		res.append((cycle 
			, '{}'.format(hex(int(PC)))
			, ','.join(list(map(str, memAddrsRead)))
			, ','.join(list(map(str, memAddrsWritten)))
			, ','.join(list(map(str, liveMemAddrs)))
			))
	
	return np.array(res)

def visualizeRegs_and_Mem(traceArr):
	res = []
	for t in traceArr:
		cycle, PC, toPredict, insn, regsRead, regsWritten, liveRegs, \
		memAddrsRead, memAddrsWritten, liveMemAddrs = t

		res.append((cycle 
			, '{}'.format(hex(int(PC)))
			, ''.join(list(reversed([('1' if toPredict & (1 << reg) else '0') for reg in range(REGS)] )))
			, '{}'.format(hex(int(insn)))
			, ''.join(list(reversed([('1' if regsRead & (1 << reg) else '0') for reg in range(REGS)] )))
			, ''.join(list(reversed([('1' if regsWritten & (1 << reg) else '0') for reg in range(REGS)])))
			, ''.join(list(reversed([('1' if liveRegs & (1 << reg) else '0') for reg in range(REGS)])))
			, ','.join(list(map(str, memAddrsRead)))
			, ','.join(list(map(str, memAddrsWritten)))
			, ','.join(list(map(str, liveMemAddrs)))
			))
	
	return np.array(res)


# https://matplotlib.org/examples/api/barchart_demo.html
def autolabel(rects, ax):
    """
    Attach a text label above each bar displaying its height
    """
    for rect in rects:
        height = rect.get_height()
        ax.text(rect.get_x(), 1.02*height,
                '%d' % int(height),
                ha='center', va='bottom')

lastReads = {}
lastReads_mem = {}
interesting_stats = defaultdict(list)
interesting_stats['regsNeverRead'] = []
interesting_stats['memAddrsNeverRead'] = []

traces = [
			("bitcount_traceA.thumb", "bitcount_trace_allMemAddr.thumb"),
			# ("rsa_traceA.thumb", "rsa_trace_allMemAddr.thumb"),
			# ("susan_traceA.thumb", "susan_trace_allMemAddr.thumb"),
			# ("basicmath_traceA.thumb", "basicmath_trace_allMemAddr.thumb"),
			# ("crc_traceA.thumb", "crc_trace_allMemAddr.thumb"),
			# ("fft_traceA.thumb", "fft_trace_allMemAddr.thumb"),
			# ("limits_traceA.thumb", "limits_trace_allMemAddr.thumb"),
			# ("overflow_traceA.thumb", "overflow_trace_allMemAddr.thumb"),
			# ("sha_traceA.thumb", "sha_trace_allMemAddr.thumb"),
			# ("randmath_traceA.thumb", "randmath_trace_allMemAddr.thumb"),
			# ("vcflags_traceA.thumb", "vcflags_trace_allMemAddr.thumb"),
			# ("rc4_traceA.thumb", "rc4_trace_allMemAddr.thumb"),
		 ]

traceArr_sizes = {}

# Number of times read from and written to in same cycle before death ; 
# read from and written to in next cycle ; 
# read from again less than ten cycles after going dead; 
# closeness of deaths / clusters in terms of cycles: bursts of reads (so read every cycle for a bit) before death. 

# allLiveRegs_traces = OrderedDict()
stats_readsBeforeEachWrite = {trace:[[0, []]for reg in range(REGS)]  for (trace, t2) in traces}
stats_writesBeforeEachRead = {trace:[[0, []]for reg in range(REGS)]  for (trace, t2) in traces}
stats_readsBeforeEachDeath = {trace:[[0, []]for reg in range(REGS)]  for (trace, t2) in traces}
stats_writesBeforeEachDeath = {trace:[[0, []]for reg in range(REGS)]  for (trace, t2) in traces}
stats_cyclesBetweenEachDeath = {trace:[[0, []]for reg in range(REGS)]  for (trace, t2) in traces}

stats_readsWritesSameCycleBeforeEachDeath = {trace:[[0, []]for reg in range(REGS)]  for (trace, t2) in traces}

########### Format 2
window_size = 400
stats_cyclesDeadInWindow = {trace:[[]for reg in range(REGS)]  for (trace, t2) in traces}

temp_dt = np.dtype([('cycle_reg', np.uint, 1)] + [('PC_reg', int, 1)]  + [('toPredict', np.uint16, 1)] 
		+ [('insn_reg', np.uint16, 1)] 
		+ [('regsRead', np.uint16, 1)] + [('regsWritten', np.uint16, 1)] + [('liveRegs', np.uint16, 1)] 
		+ [('cycle_mem', np.uint64),('PC_mem', np.uint64), ('insn_mem', np.uint16), ('memAddrsRead', object), \
		('memAddrsWritten', object), ('liveMemAddrs', object)] )

dt = np.dtype([('cycle', np.uint, 1)] + [('PC', int, 1)]  + [('toPredict', np.uint16, 1)] 
		+ [('insn', np.uint16, 1)] 
		+ [('regsRead', np.uint16, 1)] + [('regsWritten', np.uint16, 1)] + [('liveRegs', np.uint16, 1)] 
		+ [('memAddrsRead', object), ('memAddrsWritten', object), ('liveMemAddrs', object)] )

max_PC = -sys.maxsize - 1


# TODO: get prettyprint for printing updateStatsBest and prettyprint after all reg/mem is done

def updateStatsBest(memNotReg, type, traceA, *args):
	key = 'mem' if memNotReg else 'reg'

	if not len(stats_best[type][key][traceA[:len(traceA)-13]]):
		stats_best[type][key][traceA[:len(traceA)-13]] = [args]
	else:
		if (len(stats_best[type][key][traceA[:len(traceA)-13]]) < 500):
			heapq.heappush(stats_best[type][key][traceA[:len(traceA)-13]], args)
		else:
			heapq.heappushpop(stats_best[type][key][traceA[:len(traceA)-13]], args)

# https://web.eecs.umich.edu/~prabal/teaching/eecs373-f10/readings/ARMv7-M_ARM.pdf
def isBranch(insn):
	return (((insn & 0xF000) == 0xD000) or
		(insn & 0x4780) == 0x4700 or		# 0100 0111 0x Branch and Exchange (Bx)
		(insn & 0x4780) == 0x4780 or		# 0100 0111 1x Branch with Link and Exchange (Blx)
		(insn & 0xF800) == 0xF000 or   		# Branch with Link (Bl)
		(insn & 0xF000) == 0xE000 or   		# 11100x Unconditional Branch, (B)
		(insn & 0xFF00) == 0xBB00 or			# 1011 1011xxx Compare and Branch on Nonzero - A6-52 / pg. 116
		(insn & 0xFF00) == 0xB900 or			# 1011 1001xxx Compare and Branch on Nonzero - A6-52
		(insn & 0xFF00) == 0xB300 or			# 1011 0011xxx Compare and Branch on Zero    - A6-52
		(insn & 0xFF00) == 0xB100 			# 1011 0001xxx Compare and Branch on Zero    - A6-52
		)

pos_neg_pred = {trace[:len(trace)-13]:[]  for (trace, t2) in traces}
mispredict = {trace[:len(trace)-13]:[]  for (trace, t2) in traces}

def plot_prediction_results(traceA, wrapper, memNotReg=0):

	key = 'mem' if memNotReg else 'reg'


	if TESTPRED_TYPES_PARAMS:
		types=['bimodal', 'perc', 'gshare', 'choice_gs_perc']
		pred_types_params = {'bimodal': {'BimodalTableSizeBits': 0}, 
			'gshare': {'gs_PHT_BITS': 12, 'GHR_gs_len': 12}, 
			'perc':{'GHR_perc_len': 12, 'NumW': 12, 'BudgetBits': 12},
			'choice_gs_perc': {'choice_table_BITS':12, 'gs_PHT_BITS':12, 'GHR_len':12, 'NumW':12, 'PercBudgetBits':12}
			}

		iter_BTSB = pred_types_params['bimodal']['BimodalTableSizeBits']

		iter_gs_PHT_BITS = pred_types_params['gshare']['gs_PHT_BITS']
		iter_GHR_gs_len = pred_types_params['gshare']['GHR_gs_len']

		iter_GHR_choice_gs_perc_len = pred_types_params['choice_gs_perc']['GHR_len']
		iter_choice_gs_PHT_BITS = pred_types_params['choice_gs_perc']['gs_PHT_BITS']
		iter_choice_table_BITS = pred_types_params['choice_gs_perc']['choice_table_BITS']
		iter_choice_gs_perc_N = pred_types_params['choice_gs_perc']['NumW']
		iter_c_PBB = pred_types_params['choice_gs_perc']['PercBudgetBits']
		
		iter_perc_N = pred_types_params['perc']['GHR_perc_len']
		iter_PBB = pred_types_params['perc']['GHR_perc_len']
		iter_GHR_perc_len = pred_types_params['perc']['GHR_perc_len']
		
		Predictor = predictor(
						types=types,
						max_PC = max_PC,
						gs_PHT_SIZE=2**(iter_gs_PHT_BITS), GHR_gs_len=(iter_GHR_gs_len),
						bimodal_TABLE_SIZE=2**iter_BTSB,
						N=iter_perc_N, Budget=iter_PBB, GHR_perc_len=iter_GHR_perc_len
						)
		
		for type_ in types:
			wrapper(Predictor)[type_]

		# pos_neg_pred[traceA[:len(traceA)-13]] = Predictor.pos_neg_pred
		toSave = np.array(list(Predictor.pos_neg_pred.values())).T
		print(traceA)
		print(np.array(types).T)
		print(np.array(list(Predictor.pos_neg_pred.values())).T)
		toSave = np.vstack((np.array(types).T, toSave))
		# toSave = np.hstack((np.array(['.', 'corr', 'incorr']).T, toSave))
		np.save('{}_Pos_neg_pred{}.npy'.format(traceA[:len(traceA)-13], ('_MEM' if memNotReg else '')), toSave)
		
		if SAVETABLEUSAGE:
			with open("tables_usage.txt", "a+") as table_f:
				table_f.write("{}".format(datetime.now().time()))
				# fig_, ax = plt.subplots()
				# plt.ioff()
				# new_dir_ = os.getcwd() + "/AllTraceArrs"
				# access_rights = 0o755
				# try:
				#     os.mkdir(new_dir_, access_rights)
				# except OSError:
				# 	pass
				for type_ in types:
					if type_ == 'perc':
						usage = len(Predictor.perceptron.keys())
						budget = iter_PBB
					elif type_ == 'bimodal':
						usage = len(Predictor.bimodal_live_pred_table.keys())
						budget = bimodal_TABLE_SIZE
					elif type_ == 'gshare':
						usage = len(Predictor.gs_PHT.keys())
						budget = gs_PHT_SIZE
					elif type_ == 'choice_gs_perc':
						usage = len(Predictor.choice_gs_perc_table.keys())
						budget = choice_gs_perc_TABLE_SIZE
					table_f.write("{}, {}: usg({}) bgt({}) {}%".format(traceA[:len(traceA)-13], type_, usage, budget, (100*usage/budget)))

				# 	# plot usage
				# 	marker = '{}'.format(type_)
				# 	p=ax.bar(range(budget), usage, label=marker)
				# 	plt.tight_layout()
				# 	ax.set_title("{} - Tables Usage{}".format(traceA[:len(traceA)-13], (' MEM' if memNotReg else '')))
				# 	ax.set_xlabel('cycle')
				# 	ax.set_ylabel('Prediction accuracy')
				# 	plt.tight_layout()		
				# full_path=os.path.join(new_dir,"{}_GSHARE_pred_over_time{}.png".format(traceA[:len(traceA)-13], ('_MEM' if memNotReg else '')))
				# fig.savefig(full_path)
				# plt.close(fig_)


		# mispredict[traceA[:len(traceA)-13]] = Predictor.mispredictDict
		# np.save('{}_mispredict.npy'.format(traceA[:len(traceA)-13]), mispredict)

		# Predictor = predictor(
		# 				types=['choice_gs_perc'],
		# 				max_PC = max_PC,
		# 				N=iter_choice_gs_perc_N, Budget=iter_c_PBB, GHR_perc_len=iter_GHR_choice_gs_perc_len,
		# 				gs_PHT_SIZE=2**iter_choice_gs_PHT_BITS, GHR_gs_len=iter_GHR_choice_gs_perc_len,
		# 				choice_gs_perc_TABLE_SIZE=2**iter_choice_table_BITS)

		
	else:
		# max_gs_PHT_BITS, max_GHR_gs_len = 3, 3
		# min_GHR_perc_len, max_GHR_perc_len, step_GHR_perc_len = 16, 33, 16

		# # max_gs_PHT_BITS, max_GHR_gs_len = 32, 32
		# # min_GHR_perc_len, max_GHR_perc_len, step_GHR_perc_len = 16, 65, 8
		# # min_PN, max_perc_N, step_PN = 4, 36, 2 
		# min_PN, max_perc_N, step_PN = 4, 36, 20 
		# # min_PBB, max_budget_BITS, step_PBB = 3*10, 9*10, 10  # MAX_Budget_bits=log2(256*1024)
		# min_PBB, max_budget_BITS, step_PBB = 3*10, 9*10, 80  # MAX_Budget_bits=log2(256*1024)
		# # min_BTSB, max_bimodal_TABLE_SIZE_BITS, step_BTSB = 4, 36, 4
		# min_BTSB, max_bimodal_TABLE_SIZE_BITS, step_BTSB = 4, 36, 40

		# min_GHR_gs_perc_len, max_GHR_gs_perc_len, step_GHR_gs_perc_len = 16, 65, 400
		# # min_GHR_gs_perc_len, max_GHR_gs_perc_len, step_GHR_gs_perc_len = 16, 65, 4
		# min_c_PN, max_choice_gs_perc_N, step_c_PN = 4, 36, 20
		# # min_c_PN, max_choice_gs_perc_N, step_c_PN = 4, 36, 2
		# min_c_PBB, max_choice_budget_BITS, step_c_PBB = 3*10, 9*10, 80  # MAX_Budget_bits=log2(256*1024)
		# # min_c_PBB, max_choice_budget_BITS, step_c_PBB = 3*10, 9*10, 10  # MAX_Budget_bits=log2(256*1024)
		# min_choice_gs_PHT_BITS, max_choice_gs_PHT_BITS,step_choice_gs_PHT_BITS = 1, 32, 4

		max_gs_PHT_BITS, min_GHR_gs_len, max_GHR_gs_len = 48, 4, 62
		min_GHR_perc_len, max_GHR_perc_len, step_GHR_perc_len = 16, 62, 4

		min_PN, max_perc_N, step_PN = 4, 48, 2
		min_PBB, max_budget_BITS, step_PBB = 2*10, 9*10, 1  # MAX_Budget_bits=log2(256*1024)
		min_BTSB, max_bimodal_TABLE_SIZE_BITS, step_BTSB = 4, 48, 4
		min_GHR_gs_perc_len, max_GHR_gs_perc_len, step_GHR_gs_perc_len = 16, 62, 4
		min_c_PN, max_choice_gs_perc_N, step_c_PN = 4, 48, 2
		min_c_PBB, max_choice_budget_BITS, step_c_PBB = 2*10, 9*10, 10  # MAX_Budget_bits=log2(256*1024)
		min_choice_gs_PHT_BITS, max_choice_gs_PHT_BITS,step_choice_gs_PHT_BITS = 1, 48, 4
			
		# NUM_COLORS = (max_GHR_gs_len-min_GHR_gs_len+1)*(max_gs_PHT_BITS)
		# sns.reset_orig()  # get default matplotlib styles back
		# clrs = sns.color_palette('husl', n_colors=NUM_COLORS)  # a list of RGB tuples
		# fig, ax = plt.subplots(1)
		# fig.set_size_inches(60, 40)


		# for choice, we're going to do one GHR_len for both gs, perc

		
		gs_prediction_results = np.array([[0 for ghr in range(1, max_GHR_gs_len-min_GHR_gs_len+1)] \
			for pht in range(1, max_gs_PHT_BITS)])

		for iter_gs_PHT_BITS in range(1, max_gs_PHT_BITS):
			for iter_GHR_gs_len in range(min_GHR_gs_len, max_GHR_gs_len):
				Predictor = predictor(
								types=['gshare'],
								max_PC = max_PC,
								gs_PHT_SIZE=2**iter_gs_PHT_BITS, GHR_gs_len=iter_GHR_gs_len)

				wrapper(Predictor)['gshare']
				# over_time = wrapper(Predictor)['gshare']
				curr_acc = (100*Predictor.pos_neg_pred['gshare'][0]/max(1, np.sum(Predictor.pos_neg_pred['gshare'])))
				updateStatsBest(memNotReg, 'gshare', traceA, round(curr_acc, 2), iter_gs_PHT_BITS, iter_GHR_gs_len)


				# marker = '({}, {})'.format(iter_gs_PHT_BITS, iter_GHR_gs_len)
				# if (iter_gs_PHT_BITS == 1 and iter_GHR_gs_len-min_GHR_gs_len+1 == 1):
				# 	p=ax.plot(range(traceArr.size), over_time, label=marker)
				# 	p[0].set_color(clrs[(iter_gs_PHT_BITS-1)*(max_GHR_gs_len-min_GHR_gs_len+1) + (iter_GHR_gs_len-min_GHR_gs_len)])
				# else:
				# 	p=ax.scatter(range(traceArr.size), over_time, label=marker)
				# 	p.set_color(clrs[(iter_gs_PHT_BITS-1)*(max_GHR_gs_len-min_GHR_gs_len+1) + (iter_GHR_gs_len-min_GHR_gs_len)])
				
				# ax.tick_params(labelleft=True, labelright=True)
				# ax.legend(title='log2(PHT), GHR_len', ncol=32)
				
				gs_prediction_results[iter_gs_PHT_BITS-1][iter_GHR_gs_len-min_GHR_gs_len] = \
					100*Predictor.pos_neg_pred['gshare'][0]/max(1, np.sum(Predictor.pos_neg_pred['gshare']))

		# 	ax.set_title("{} - GSHARE Prediction Accuracy Patterns{}".format(traceA[:len(traceA)-13], (' MEM' if memNotReg else '')))
		# 	ax.set_xlabel('cycle')
		# 	ax.set_ylabel('Prediction accuracy')
		# 	plt.tight_layout()		
		# 	full_path=os.path.join(new_dir,"{}_GSHARE_pred_over_time{}.png".format(traceA[:len(traceA)-13], ('_MEM' if memNotReg else '')))
		# 	fig.savefig(full_path)
		# plt.close(fig)

		# del over_time
		
		fig, ax = plt.subplots()
		plt.ioff()
		ax = sns.heatmap( gs_prediction_results.T, 
			xticklabels=[x for x in range(1, max_gs_PHT_BITS)], 
			yticklabels=list(range(min_GHR_gs_len, max_GHR_gs_len)), 
			cmap="YlOrBr", linewidth=0.05)
		ax.invert_yaxis()
		
		full_path=os.path.join(new_dir,"{}_GSHARE_pred{}.png".format(traceA[:len(traceA)-13], ('_MEM' if memNotReg else '')))
		ax.set_title("{} - GSHARE Prediction Accuracy{}".format(traceA[:len(traceA)-13], (' MEM' if memNotReg else '')))
		ax.set_xlabel('log2 (PHT Entries)')
		ax.set_ylabel('GHR Length')
		plt.tight_layout()
		fig.savefig(full_path)
		fig.set_size_inches(60, 40)
		# plt.show()
		plt.close(fig)

		del gs_prediction_results
		# ########
		# NUM_COLORS = (len(range(min_PBB, max_budget_BITS, step_PBB))+1) *(len(range(min_PN, max_perc_N, step_PN))+1) 
		# sns.reset_orig()  # get default matplotlib styles back
		# clrs = sns.color_palette('husl', n_colors=NUM_COLORS)  # a list of RGB tuples 
		# fig, ax = plt.subplots()
		# plt.ioff()


		perc_prediction_results = np.array([[0 for ghr in range(min_PBB, max_budget_BITS, step_PBB)] \
			for pht in range(min_PN, max_perc_N, step_PN)])
		for iter_GHR_perc_len in range(min_GHR_perc_len, max_GHR_perc_len, step_GHR_perc_len):

			for iter_perc_N in range(min_PN, max_perc_N, step_PN):
				for iter_PBB in range(min_PBB, max_budget_BITS, step_PBB):
					Predictor = predictor(
									types=['perc'],
									max_PC = max_PC,
									N=iter_perc_N, Budget=iter_PBB, GHR_perc_len=iter_GHR_perc_len)

					wrapper(Predictor)['perc']
					# over_time = wrapper(Predictor)['perc']
					curr_acc = (100*Predictor.pos_neg_pred['perc'][0]/max(1, np.sum(Predictor.pos_neg_pred['perc'])))
					updateStatsBest(memNotReg, 'perc', traceA, round(curr_acc, 2), iter_GHR_perc_len, iter_perc_N, iter_PBB)

					# marker = '({}, {})'.format(iter_perc_N, iter_PBB)
					# if (iter_perc_N == 1 and iter_PBB == 1):
					# 	p=ax.plot(range(traceArr.size), over_time, label=marker)
					# 	p[0].set_color(clrs[0])
					# else:
					# 	p=ax.scatter(range(traceArr.size), over_time, label=marker)
					# 	p.set_color(clrs[(((iter_perc_N-min_PN)//step_PN)*((max_budget_BITS-min_PBB)//step_PBB)) + ((iter_PBB-min_PBB)//step_PBB)-1])

					# ax.tick_params(labelleft=True, labelright=True)
					# ax.legend(title='NumW, log2(budget)', ncol=32)
					
					perc_prediction_results[(iter_perc_N-min_PN)//step_PN][(iter_PBB-min_PBB)//step_PBB] = \
						100*Predictor.pos_neg_pred['perc'][0]/max(1, np.sum(Predictor.pos_neg_pred['perc']))

			# 	ax.set_title("{} - Perceptron Prediction Accuracy Patterns ({} GHR Len){}".format\
			# 		(traceA[:len(traceA)-13], iter_GHR_perc_len, (' MEM' if memNotReg else '')))
			# 	ax.set_xlabel('cycle')
			# 	ax.set_ylabel('Prediction accuracy')
			# 	full_path=os.path.join(new_dir,"{}_Perceptron_pred_over_time_{}_GHR_Len{}.png".format\
			# 		(traceA[:len(traceA)-13], iter_GHR_perc_len, ('_MEM' if memNotReg else '')))

			# 	plt.tight_layout()
			# 	fig.savefig(full_path)
			# 	fig.set_size_inches(60, 40)
			# plt.close(fig)
			# if with

			fig_, ax_ = plt.subplots()
			plt.ioff()
			ax_ = sns.heatmap( perc_prediction_results.T, 
				xticklabels=list(range(min_PN, max_perc_N, step_PN)), 
				yticklabels=(np.array([x for x in range(min_PBB, max_budget_BITS, step_PBB)])),
				cmap="YlOrBr", linewidth=0.05)
			ax_.invert_yaxis()

			full_path=os.path.join(new_dir,"{}_Perceptron_pred_GHRLen_{}{}.png".format(traceA[:len(traceA)-13], iter_GHR_perc_len, ('_MEM' if memNotReg else '')))
			ax_.set_title("{} - Perceptron Prediction Accuracy_GHRLen_{}{}".format(traceA[:len(traceA)-13], iter_GHR_perc_len, (' MEM' if memNotReg else '')))
			ax_.set_xlabel('Number of Weights per Perceptron')
			ax_.set_ylabel('log2 (budget))')
			plt.tight_layout()
			fig_.savefig(full_path)
			fig_.set_size_inches(60, 40)
			plt.close(fig_)

		del perc_prediction_results

		########
		# NUM_COLORS = len(range(min_BTSB, max_bimodal_TABLE_SIZE_BITS, step_BTSB))
		# sns.reset_orig()  # get default matplotlib styles back
		# clrs = sns.color_palette('husl', n_colors=NUM_COLORS)  # a list of RGB tuples
		# fig, ax = plt.subplots(1)

		bimodal_prediction_results = np.array([0 for trad in range(min_BTSB, max_bimodal_TABLE_SIZE_BITS, step_BTSB)])

		for iter_BTSB in range(min_BTSB, max_bimodal_TABLE_SIZE_BITS, step_BTSB):
			Predictor = predictor(
							types=['bimodal'],
							max_PC = max_PC,
							bimodal_TABLE_SIZE=2**iter_BTSB)

			wrapper(Predictor)['bimodal']
			curr_acc = (100*Predictor.pos_neg_pred['bimodal'][0]/max(1, np.sum(Predictor.pos_neg_pred['bimodal'])))
			updateStatsBest(memNotReg, 'bimodal', traceA, round(curr_acc, 2), iter_BTSB)

			# marker = '({})'.format(iter_BTSB)
			# if (iter_BTSB == 1):
			# 	p=ax.plot(range(traceArr.size), over_time, label=marker)
			# 	p.set_color(clrs[(iter_BTSB-min_BTSB)//step_BTSB])
			# else:
			# 	ax.scatter(range(traceArr.size), over_time, label=marker)
			# 	p.set_color(clrs[(iter_BTSB-min_BTSB)//step_BTSB])
			# ax.legend(title='log2(Budget)', numpoints=1)
			
			bimodal_prediction_results[(iter_BTSB-min_BTSB)//step_BTSB] = \
				100*Predictor.pos_neg_pred['bimodal'][0]/max(1, np.sum(Predictor.pos_neg_pred['bimodal']))

		# ax.set_title("{} - Bimodal Prediction Accuracy Patterns{}".format(traceA[:len(traceA)-13], (' MEM' if memNotReg else '')))
		# ax.set_xlabel('cycle')
		# ax.set_ylabel('Prediction accuracy')
		# plt.tight_layout()		
		# full_path=os.path.join(new_dir,"{}_Bimodal_pred_over_time{}.png".format(traceA[:len(traceA)-13], ('_MEM' if memNotReg else '')))
		# fig.savefig(full_path)
		# fig.set_size_inches(60, 40)
		# plt.close(fig)

		fig, ax = plt.subplots()
		plt.ioff()
		full_path=os.path.join(new_dir, "{}_Bimodal_pred{}.png".format(traceA[:len(traceA)-13], ('_MEM' if memNotReg else '')))

		ax.set_title('{} - Bimodal Prediction Accuracy'.format(traceA[:len(traceA)-13]))
		ax.set_ylabel('Percent Accuracy')
		ax.set_xlabel('log2 (Number of Table Entries)')
		
		x_coord = np.array([x for x in range(min_BTSB, max_bimodal_TABLE_SIZE_BITS, step_BTSB)])
		ax.bar(x_coord, bimodal_prediction_results, tick_label = x_coord)
		fig.savefig(full_path)
		plt.close(fig)

		del bimodal_prediction_results

		# NUM_COLORS = (len(range(min_c_PBB, max_choice_budget_BITS, step_c_PBB))+1) *(len(range(min_c_PN, max_choice_gs_perc_N, step_c_PN))+1)
		# sns.reset_orig()  # get default matplotlib styles back
		# clrs = sns.color_palette('husl', n_colors=NUM_COLORS)  # a list of RGB tuples
		# fig, ax = plt.subplots(1)

		choice_gs_perc_prediction_results = np.array([[0 \
			for ghr in range(min_c_PBB, max_choice_budget_BITS, step_c_PBB)] \
			for pht in range(min_c_PN, max_choice_gs_perc_N, step_c_PN)])


		for iter_choice_table_BITS in range(11, 13):
			for iter_choice_gs_PHT_BITS in range(min_choice_gs_PHT_BITS, max_choice_gs_PHT_BITS, step_choice_gs_PHT_BITS):
				for iter_GHR_choice_gs_perc_len in range(min_GHR_gs_perc_len, \
					max_GHR_gs_perc_len, step_GHR_gs_perc_len):
					for iter_choice_gs_perc_N in range(min_c_PN, max_choice_gs_perc_N, step_c_PN):
						for iter_c_PBB in range(min_c_PBB, max_choice_budget_BITS, step_c_PBB):
							Predictor = predictor(
											types=['choice_gs_perc'],
											max_PC = max_PC,
											N=iter_choice_gs_perc_N, Budget=iter_c_PBB, GHR_perc_len=iter_GHR_choice_gs_perc_len,
											gs_PHT_SIZE=2**iter_choice_gs_PHT_BITS, GHR_gs_len=iter_GHR_choice_gs_perc_len,
											choice_gs_perc_TABLE_SIZE=2**iter_choice_table_BITS)

							wrapper(Predictor)['choice_gs_perc']
							curr_acc = (100*Predictor.pos_neg_pred['choice_gs_perc'][0]/max(1, \
								np.sum(Predictor.pos_neg_pred['choice_gs_perc'])))
							updateStatsBest(memNotReg, 'choice_gs_perc', traceA, round(curr_acc, 2), \
								iter_choice_table_BITS, iter_choice_gs_PHT_BITS, iter_GHR_choice_gs_perc_len, iter_choice_gs_perc_N, iter_c_PBB)

							# marker = '({}, {})'.format(iter_choice_gs_perc_N, iter_c_PBB)
							# if (iter_choice_gs_perc_N == 1 and iter_c_PBB == 1):
							# 	p=ax.plot(range(traceArr.size), over_time, label=marker)
							# 	p[0].set_color(clrs[0])
							# else:
							# 	p=ax.scatter(range(traceArr.size), over_time, label=marker)
							# 	p.set_color(clrs[(((iter_choice_gs_perc_N-min_c_PN)//step_c_PN)*\
							# 		((max_choice_budget_BITS-min_c_PBB)//step_c_PBB)) + ((iter_c_PBB-min_c_PBB)//step_c_PBB)-1])

							# ax.tick_params(labelleft=True, labelright=True)
							# ax.legend(title='NumW, log2(Budget)', ncol=32)
							
							choice_gs_perc_prediction_results[(iter_choice_gs_perc_N-min_c_PN)//step_c_PN][(iter_c_PBB-min_c_PBB)//step_c_PBB] = \
								100*Predictor.pos_neg_pred['choice_gs_perc'][0]/max(1, np.sum(Predictor.pos_neg_pred['choice_gs_perc']))

					# 	ax.set_title("{} - Choice_gs_perc Prediction Accuracy Patterns ({} GHR Len){}".format\
					# 		(traceA[:len(traceA)-13], iter_GHR_choice_gs_perc_len, (' MEMORY' if memNotReg else '')))
					# 	ax.set_xlabel('cycle')
					# 	ax.set_ylabel('Prediction accuracy')
					# 	full_path=os.path.join(new_dir,"{}_Choice_gs_perc_pred_over_time_{}_GHR_Len{}.png".format\
					# 		(traceA[:len(traceA)-13], iter_GHR_choice_gs_perc_len, ('_MEM' if memNotReg else '')))

					# 	plt.tight_layout()
					# 	fig.savefig(full_path)
					# 	fig.set_size_inches(60, 40)
					# plt.close(fig)

					fig_, ax_ = plt.subplots()
					plt.ioff()
					ax_ = sns.heatmap( choice_gs_perc_prediction_results.T, 
						xticklabels=list(range(min_c_PN, max_choice_gs_perc_N, step_c_PN)), 
						yticklabels=(np.array([x for x in range(min_c_PBB, max_choice_budget_BITS, step_c_PBB)])),
						cmap="YlOrBr", linewidth=0.05)
					ax_.invert_yaxis()

					full_path=os.path.join(new_dir,"{}_Choice_gs_perc_pred_TableBits_{}_GHRLen_{}_GS_PHTBits_{}{}.png".format(traceA[:len(traceA)-13],\
						iter_choice_table_BITS, iter_GHR_choice_gs_perc_len, iter_choice_gs_PHT_BITS, ('_MEM' if memNotReg else '')))
					ax_.set_title("{} - Choice_gs_perc Prediction Accuracy_TableBits_{}_GHRLen_{}_GS_PHTBits_{}{}".format(traceA[:len(traceA)-13],\
						iter_choice_table_BITS, iter_GHR_choice_gs_perc_len, iter_choice_gs_PHT_BITS, (' MEM' if memNotReg else '')))
					ax_.set_xlabel('Number of Weights per Perceptron')
					ax_.set_ylabel('log2 (budget))')
					plt.tight_layout()
					fig_.savefig(full_path)
					fig_.set_size_inches(60, 40)
					plt.close(fig_)

		del choice_gs_perc_prediction_results

	

###################################################################################################################

stats_best = {
	'bimodal':{'reg':{trace[:len(trace)-13]:[]  for (trace, t2) in traces},
		'mem':{trace[:len(trace)-13]:[]  for (trace, t2) in traces}} ,	# PRED_ACC, PHT_BITS
	'gshare': {'reg':{trace[:len(trace)-13]:[]  for (trace, t2) in traces}, 
		'mem':{trace[:len(trace)-13]:[]  for (trace, t2) in traces}},  # PRED_ACC, PHT_BITS, GHR_len
	'perc': {'reg':{trace[:len(trace)-13]:[]  for (trace, t2) in traces}, 
		'mem':{trace[:len(trace)-13]:[]  for (trace, t2) in traces}},  # PRED_ACC, GHR_len, NumW, PBB
	'choice_gs_perc': {'reg':{trace[:len(trace)-13]:[]  for (trace, t2) in traces}, 
		'mem':{trace[:len(trace)-13]:[]  for (trace, t2) in traces}},  # PRED_ACC, GHR_len, NumW, PBB
	}
  

for traceIdx, (traceA, traceAllMem) in enumerate(traces):
	if RETRIEVE_TRACEARR:
		new_dir_ = os.getcwd() + "/AllTraceArrs"
		traceArr = np.load(os.path.join(new_dir_, '{}TraceArr.npy'.format(traceA[:len(traceA)-13])))
		# np.
	else:

		traceArr = OrderedDict()
		# extract cycle, insn, regsRead, regsWritten in each trace Packet
		with open(traceA, 'r') as traceA_fp:
			foundFirstRegInsn = 0
			for line in traceA_fp:
				m = re.match(r'(?=LD|ST)(LD|ST):(\d+)\t(\d+)\t(.{4})\t(\d+)|REG:(\d+)\t(\d+)\t([^\t]{4})\t([^\t]{8})\t([^\t]{8})', line, re.I)
				if (m):
					accessType, cycle_mem, PC_mem, insn_mem, addr_mem, \
					cycle_reg, PC_reg, insn_reg, regsRead, regsWritten = m.groups()

					if accessType:				# mem_access
						if foundFirstRegInsn:
							cycle_mem, PC_mem, insn_mem, addr_mem = int(cycle_mem), int(PC_mem), int(insn_mem, 16), int(addr_mem)
							if cycle_mem in traceArr:
								if accessType == "LD":
									traceArr[cycle_mem][10].add(addr_mem) 	# mem_reads
								if accessType == "ST":
									traceArr[cycle_mem][11].add(addr_mem)	# mem_writes
							else:
								# tracePkt : (cycle_mem, PC, insn, [<list of memAddrs Read>], [<list of memAddrs Written>], [<list of memAddrs Live>], [<list of memAddrs toPredict>])
								if accessType == "LD":
									traceArr[cycle_mem] = (0, 0, 0, 0, 0, 0, 0, \
										cycle_mem, PC_mem, insn_mem, set([addr_mem]), set(), set())
								if accessType == "ST":
									traceArr[cycle_mem] = (0, 0, 0, 0, 0, 0, 0, \
										cycle_mem, PC_mem, insn_mem, set(), set([addr_mem]), set())
					else:
						foundFirstRegInsn = 1
						cycle_reg, PC_reg, toPredict = int(cycle_reg), int(PC_reg), 0
						insn_reg, regsRead, regsWritten = int(insn_reg, 16), int(regsRead, 16), int(regsWritten, 16)
						liveRegs = 0

						if cycle_reg in traceArr:
				 			traceArr[cycle_reg] = (cycle_reg, PC_reg, toPredict, insn_reg, regsRead, regsWritten, liveRegs) + traceArr[cycle_reg][7:]
						else:
							traceArr[cycle_reg] = (cycle_reg, PC_reg, toPredict, insn_reg, regsRead, regsWritten, liveRegs, \
								0, 0, 0, set(), set(), set())
			
			if not len(traceArr): 
				print("{}: Trace empty?", traceA)
				continue
			traceArr = np.array(list(traceArr.values()), dtype=temp_dt)

			# sanitize trace output
			to_del = []
			for t in traceArr:
				if (t['cycle_reg'] and t['cycle_mem'] and (t['cycle_reg'] != t['cycle_mem'])):
					print("{}: mem/reg cycle mismatch\tREG: {}, MEM: {}", traceA, t['cycle_reg'], t['cycle_mem'])
					to_del.append(t['cycle_reg'])
					# TODO: do sth with to_del. maybe add a flag to each element in traceArr to skip
				elif (t['cycle_mem'] and not t['cycle_reg']):
					isZero = 1
					for x in t[['cycle_reg', 'PC_reg', 'toPredict', 'insn_reg', 'regsRead', 'regsWritten', 'liveRegs']]:
						isZero &= (x==0)
					print("{}: mem/reg cycle mismatch or mem access w/o reg access\tREG: {}, MEM: {}", traceA, t['cycle_reg'], t['cycle_mem'])

			if not len(traceArr): 
				print("{}: Trace emptied due to mem/reg cycle mismatch?", traceA)
				continue

			traceArr = np.array(traceArr[['cycle_reg', 'PC_reg', 'toPredict', 'insn_reg', \
				'regsRead', 'regsWritten', 'liveRegs', 'memAddrsRead', 'memAddrsWritten', 'liveMemAddrs']], dtype=dt)

		traceArr['insn'] = np.roll(traceArr['insn'], -1)
		traceArr = traceArr[:-1]
	if traceArr.size == 0: continue
	# num_windows = int(traceArr[-1]['cycle']//window_size + 1)												
	stats_cyclesDeadInWindow[traceA] = [[0 for w in range(2**16)] for reg in range(REGS)]

	# traverse upwards for each register to find lastRead
	for reg in range(REGS):
		for i, tracePkt in enumerate(traceArr[::-1]):
			# before lastReads predict liveness for all regs

			if (tracePkt['regsRead'] & (1 << reg)):
				lastReads[reg] = {'cycle': tracePkt['cycle'], 'idx': traceArr.size - i-1}
				break
			else:
				tracePkt['toPredict'] |= ((1 << reg) & toPredictMASK_reg)
		else:
			if not(traceArr[0]['regsRead'] & (1 << reg)):
				# if a register is never read, no need to predict its liveness
				toPredictMASK_reg &= ~(1 << reg)
				traceArr['toPredict'] = traceArr['toPredict'] & np.array([~(1 << reg) for hello in range(traceArr.size)])
				interesting_stats['regsNeverRead'].append((traceA, reg))

	# reg is live between each [write or first read] to [next read]
	totalLiveRegs = 0
	for reg in range(REGS):
		if (reg in lastReads):
			currIdx = lastReads[reg]['idx']
			
			currPredWdw = []
			branchFound, readStart , writeStart = False, False, False
			toPredict = False
			
			while (currIdx >= 0):

				traversed = None
				
				idx = currIdx
				# find lastWrite before this read
				if (currIdx >= 0):
					for tracePkt in traceArr[currIdx::-1]:
						idx -= 1
						traversed = True

						#####################################
						# determime which to predict liveness for
						if isBranch(tracePkt['insn']) and (readStart or writeStart):
							currPredWdw.append((tracePkt['cycle'], reg))
							branchFound = True
						elif (tracePkt['regsRead'] & (1 << reg)):
							# this read: start of new window
							if not readStart:
								if not writeStart:
									readStart = True
									currPredWdw = []
									currPredWdw.append((tracePkt['cycle'], reg))
									branchFound = False
								else:  # rd --> (branch?) --> wr[start of window]
									if branchFound:
										currPredWdw.append((tracePkt['cycle'], reg))
										for cycle, reg in currPredWdw:
											predIdx = np.where(traceArr['cycle'] == cycle)[0][0]
											# print('b4', traceArr[predIdx]['toPredict'])
											traceArr[predIdx]['toPredict'] |= ((1 << reg) & toPredictMASK_reg) # toPredictMASK_reg|= ((1 << reg) & toPredictMASK_reg)
											# print('aft:', traceArr[predIdx]['toPredict'])
											
										currPredWdw = []
										currPredWdw.append((tracePkt['cycle'], reg))
										branchFound = False
									else:
										currPredWdw = []
										currPredWdw.append((tracePkt['cycle'], reg))
										# this read starts new wdw
										readStart, writeStart = True, False
							else:
								if (isBranch(tracePkt['insn'])): 
									# this branch reads curr reg but DON'T handle as normal opening/closing read
									currPredWdw.append((tracePkt['cycle'], reg))
									branchFound = True
								else:
									if branchFound:
										# handle as normal opening/closing read
										currPredWdw.append((tracePkt['cycle'], reg))
										for cycle, reg in currPredWdw:
											predIdx = np.where(traceArr['cycle'] == cycle)[0][0]
											traceArr[predIdx]['toPredict'] |= ((1 << reg) & toPredictMASK_reg) # toPredictMASK_reg|= ((1 << reg) & toPredictMASK_reg)

									else:
										currPredWdw.append((tracePkt['cycle'], reg))
									currPredWdw = []
									currPredWdw.append((tracePkt['cycle'], reg))
									branchFound = False
									readStart = True
						elif not(tracePkt['regsWritten'] & (1 << reg) and not tracePkt['regsRead'] & (1 << reg)):
							currPredWdw.append((tracePkt['cycle'], reg))
						
						#####################################

						if (tracePkt['regsWritten'] & (1 << reg) and not tracePkt['regsRead'] & (1 << reg)):
							
							# found a write: handle whether to predict
							if (readStart or writeStart) and branchFound:
								currPredWdw.append((tracePkt['cycle'], reg))
								for cycle, reg in currPredWdw:
									predIdx = np.where(traceArr['cycle'] == cycle)[0][0]
									traceArr[predIdx]['toPredict'] |= ((1 << reg) & toPredictMASK_reg) # toPredictMASK_reg|= ((1 << reg) & toPredictMASK_reg)
								currPredWdw = []
								currPredWdw.append((tracePkt['cycle'], reg))
								branchFound = False
							else:
								currPredWdw.append((tracePkt['cycle'], reg))
							readStart, writeStart = False, True
							currPredWdw = []
							currPredWdw.append((tracePkt['cycle'], reg))

							break

						if not (tracePkt['regsRead'] & (1 << reg)):
							# not a read/write of this reg so add to pred wdw
							currPredWdw.append((tracePkt['cycle'], reg))

						tracePkt['liveRegs'] |= (1 << reg) 
					currIdx = idx
				
				# now found prevWrite before/at this Read
				# find prevRead before this write
				if (currIdx >= 0):
					for tracePkt in traceArr[currIdx::-1]:
						idx -= 1
						traversed = True
	
						#####################################
						# HAD TO COPY OVER FROM TOP
						# determime which to predict liveness for
						if isBranch(tracePkt['insn']) and (readStart or writeStart):
							currPredWdw.append((tracePkt['cycle'], reg))
							branchFound = True
						elif (tracePkt['regsRead'] & (1 << reg)):
							# this read: start of new window
							if not readStart:
								if not writeStart:
									readStart = True
									currPredWdw = []
									currPredWdw.append((tracePkt['cycle'], reg))
									branchFound = False
								else:  # rd --> (branch?) --> wr[start of window]
									if branchFound:
										currPredWdw.append((tracePkt['cycle'], reg))
										for cycle, reg in currPredWdw:
											predIdx = np.where(traceArr['cycle'] == cycle)[0][0]
											traceArr[predIdx]['toPredict'] |= ((1 << reg) & toPredictMASK_reg) # toPredictMASK_reg|= ((1 << reg) & toPredictMASK_reg)
										currPredWdw = []
										currPredWdw.append((tracePkt['cycle'], reg))
										branchFound = False
									else:
										currPredWdw = []
										currPredWdw.append((tracePkt['cycle'], reg))
										# this read starts new wdw
										readStart, writeStart = True, False
							else:
								if (isBranch(tracePkt['insn'])): 
									# this branch reads curr reg but DON'T handle as normal opening/closing read
									currPredWdw.append((tracePkt['cycle'], reg))
									branchFound = True
								else:
									# handle as normal opening/closing read
									if branchFound:
										currPredWdw.append((tracePkt['cycle'], reg))
										for cycle, reg in currPredWdw:
											predIdx = np.where(traceArr['cycle'] == cycle)[0][0]
											traceArr[predIdx]['toPredict'] |= ((1 << reg) & toPredictMASK_reg) # toPredictMASK_reg|= ((1 << reg) & toPredictMASK_reg)
									currPredWdw = []
									currPredWdw.append((tracePkt['cycle'], reg))
									branchFound = False
									readStart = True

						elif (tracePkt['regsWritten'] & (1 << reg)):
							# found a write: handle whether to predict
							if (readStart or writeStart) and branchFound:
								for cycle, reg in currPredWdw:
									predIdx = np.where(traceArr['cycle'] == cycle)[0][0]
									traceArr[predIdx]['toPredict'] |= ((1 << reg) & toPredictMASK_reg) # toPredictMASK_reg|= ((1 << reg) & toPredictMASK_reg)
								currPredWdw = []
								currPredWdw.append((tracePkt['cycle'], reg))
								branchFound = False
							else:
								# START
								currPredWdw = [(tracePkt['cycle'], reg)]
							readStart, writeStart = False, True
						else:
							currPredWdw.append((tracePkt['cycle'], reg))
						#####################################

						if (tracePkt['regsRead'] & (1 << reg)):
							tracePkt['liveRegs'] |= (1 << reg)
							break
					currIdx = idx
				
				if (not traversed):
					currIdx -= 1
				# we found cycle of lastRead before the next write.
				# now find last write before that Read.
				
		# unset liveness for each reg until first read/write
		for tracePkt in traceArr:
			if (not (tracePkt['regsRead'] & (1 << reg)) and not (tracePkt['regsWritten'] & (1 << reg))):
				tracePkt['liveRegs'] &= ~(1 << reg)
			else:
				break
		
		countWritesBeforeRead = 0
		for tracePkt in traceArr:
			# find read
			if (tracePkt['regsRead'] & (1 << reg)):
				if (countWritesBeforeRead > 0):
					stats_writesBeforeEachRead[traceA][reg][0] += countWritesBeforeRead
					stats_writesBeforeEachRead[traceA][reg][1].append(countWritesBeforeRead)
					if (tracePkt['regsWritten'] & (1 << reg)):
						# read/write in same cycle so for subsequent reads, init count = 1
						countWritesBeforeRead = 1
					else:
						countWritesBeforeRead = 0
			elif (tracePkt['regsWritten'] & (1 << reg)):
				countWritesBeforeRead += 1
		
		countReadsBeforeWrite = 0
		for tracePkt in traceArr:
			# find write
			if (tracePkt['regsWritten'] & (1 << reg)):
				if (countReadsBeforeWrite > 0):
					stats_readsBeforeEachWrite[traceA][reg][0] += countReadsBeforeWrite
					stats_readsBeforeEachWrite[traceA][reg][1].append(countReadsBeforeWrite)
					if (tracePkt['regsRead'] & (1 << reg)):
						# read/write in same cycle so for subsequent writes, init count = 1
						countReadsBeforeWrite = 1
					else:
						countReadsBeforeWrite = 0
			elif (tracePkt['regsRead'] & (1 << reg)):
				countReadsBeforeWrite += 1				

	# FOR EACH REG: ASSIGN LIVENESS bit to whether reg is live after that insn, 
	# so roll this column of the array upwards
	traceArr['liveRegs'] = np.roll(traceArr['liveRegs'], -1)
	if traceArr.size: traceArr[-1]['liveRegs'] = 0
	if SAVETRACEARR:
		new_dir_ = os.getcwd() + "/AllTraceArrs"
		access_rights = 0o755
		try:
		    os.mkdir(new_dir_, access_rights)
		except OSError:
			pass
		np.save(os.path.join(new_dir_, '{}TraceArr.npy'.format(traceA[:len(traceA)-13])), visualizeRegs_and_Mem(traceArr))

	#################################
	# PREDICTION 					#
	#################################

	if PREDICT_LIVENESS:	
		new_dir = os.getcwd() + "/Prediction"
		access_rights = 0o755
		try:
		    os.mkdir(new_dir, access_rights)
		except OSError:
			pass

		def prediction_wrapper_reg(Predictor):
			global totalLiveRegs
			if traceArr.size:
				predictions_over_time_REG15 = {pred_t:(np.array([np.uint8(0) for t in range(traceArr.size)])) \
					for pred_t in Predictor.types}
				prev_acc = {pred_t:np.uint8(0) for pred_t in Predictor.types}
				for i, tracePkt in enumerate(traceArr):
					for reg in range(REGS):
						# only predict if reg is an operand
						if (tracePkt['regsRead'] >> reg) & 1:
							Predictor.make_prediction(pc=np.int64(tracePkt['PC']),
								toPredictFlag=(tracePkt['toPredict'] >> reg & 1), 
								reg=reg)

							outcome = (tracePkt['liveRegs'] >> reg) & 1
							Predictor.train_predictor(np.int64(tracePkt['PC']), outcome, 
								toPredictFlag=(tracePkt['toPredict'] >> reg & 1), 
								reg=reg)

							for t in Predictor.types:
								if (reg == 15):
									predictions_over_time_REG15[t][i] = \
										np.uint8(100*Predictor.pos_neg_pred[t][0]/max(1, np.sum(Predictor.pos_neg_pred[t])))
								# printToFile (predictions_over_time_REG15[i])
								prev_acc[t] = predictions_over_time_REG15[t][i]
						else:
							for t in Predictor.types:
								predictions_over_time_REG15[t][i] = prev_acc[t]

				return predictions_over_time_REG15

		plot_prediction_results(traceA, memNotReg=0, wrapper=prediction_wrapper_reg)

	if SAVEPERCENTLIVEREGS:
		with open("pLiveRegs.txt", "a+") as pLiveFile:
			if traceArr.size:
				percentLiveRegs = defaultdict(int)
				for reg in range(REGS):
					if reg not in REGSTOTRACKLIVENESS:
						continue
					prev = None
					for tracePkt in traceArr:
						currCycle = tracePkt['cycle']
						if prev:
							# fill in for cycles between two instructions
							for c in range(prev['cycle']+np.uint64(1), currCycle):
								percentLiveRegs[c] += (100/len(REGSTOTRACKLIVENESS))* ((prev['liveRegs'] >> reg) & 1)
							totalLiveRegs +=  (currCycle - (prev['cycle']+np.uint64(1))) if (prev['liveRegs'] & (1 << reg)) else 0

						prev = tracePkt
						percentLiveRegs[currCycle] += (100/len(REGSTOTRACKLIVENESS))* ((tracePkt['liveRegs'] >> reg) & 1)
						x = str("RR " + traceA[:len(traceA)-13] + " %d, %d\t" % (currCycle, percentLiveRegs[currCycle]))
						# x = str("RR" + traceA[:len(traceA)-13] + " cycle: %d, pLiveReg: %d\t" % (currCycle, percentLiveRegs[currCycle]))
						pLiveFile.write(str(x))
						totalLiveRegs +=  1 if (tracePkt['liveRegs'] & (1 << reg)) else 0
				# allLiveRegs_traces[traceA] = [percentLiveRegs, totalLiveRegs,  (100/len(REGSTOTRACKLIVENESS))*totalLiveRegs/int(traceArr[-1]['cycle'])]

				x = str("AR " + traceA[:len(traceA)-13] + " %d, %f" % (totalLiveRegs, (100/len(REGSTOTRACKLIVENESS))*totalLiveRegs/traceArr[-1]['cycle']))
				#ALLREGS
				# x = str(traceA[:len(traceA)-13] + " totalLiveRegs: %d, avgPercentlLiveRegs: %f" % (totalLiveRegs, (100/len(REGSTOTRACKLIVENESS))*totalLiveRegs/traceArr[-1]['cycle']))
				pLiveFile.write(str(x))
				print(traceA, " totalLiveRegs: %d, avgPercentlLiveRegs: %f" % (totalLiveRegs, (100/len(REGSTOTRACKLIVENESS))*totalLiveRegs/traceArr[-1]['cycle']))

			countReadsBeforeDead, countWritesBeforeDead = 0, 0
			countCyclesBetweenDead, countReadsWritesSameCycleBeforeDead = 0, 0

	# curr_window = {'st': 0, 'end': window_size}
	# for reg in range(REGS):
	# 	foundLive, foundFirstLive = False, False
	# 	for tracePkt in traceArr:
	# 		# find first live
	# 		if (not foundFirstLive):
	# 			foundFirstLive = (tracePkt['liveRegs'] & (1 << reg))
	# 			if (foundFirstLive) : countCyclesBetweenDead = 1
	# 			continue

	# 		if (foundFirstLive):
	# 			# find reg death
	# 			if not (tracePkt['liveRegs'] & (1 << reg)):
	# 				if (True or countReadsBeforeDead > 0):
	# 					stats_readsBeforeEachDeath[traceA][reg][0] += countReadsBeforeDead
	# 					stats_readsBeforeEachDeath[traceA][reg][1].append(countReadsBeforeDead)
	# 					countReadsBeforeDead = 0
	# 				if (True or countWritesBeforeDead > 0):
	# 					stats_writesBeforeEachDeath[traceA][reg][0] += countWritesBeforeDead
	# 					stats_writesBeforeEachDeath[traceA][reg][1].append(countWritesBeforeDead)					
	# 					countWritesBeforeDead = 0
	# 				if (True or countCyclesBetweenDead > 0):
	# 					stats_cyclesBetweenEachDeath[traceA][reg][0] += countCyclesBetweenDead
	# 					stats_cyclesBetweenEachDeath[traceA][reg][1].append(countCyclesBetweenDead)					
	# 					countCyclesBetweenDead = 0
	# 				if (True or countReadsWritesSameCycleBeforeDead > 0):
	# 					stats_readsWritesSameCycleBeforeEachDeath[traceA][reg][0] += countReadsWritesSameCycleBeforeDead
	# 					stats_readsWritesSameCycleBeforeEachDeath[traceA][reg][1].append(countReadsWritesSameCycleBeforeDead)					
	# 					countReadsWritesSameCycleBeforeDead = 0

	# 				stats_cyclesDeadInWindow[traceA][reg][ int(tracePkt['cycle']//window_size) ] += 1

	# 			else:
	# 				countReadsBeforeDead += (tracePkt['regsRead'] & (1 << reg) != 0)
	# 				countWritesBeforeDead += (tracePkt['regsWritten'] & (1 << reg) != 0)
	# 				countCyclesBetweenDead += 1
	# 				countReadsWritesSameCycleBeforeDead += ((tracePkt['regsRead'] & (1 << reg) != 0) \
	# 					and (tracePkt['regsWritten'] & (1 << reg) != 0))


	######################
	# MEMORY 			 #
	######################

	if CHECK_MEM:
		allMemAddr = set()
		memAddrSoFar = set()
		branch_cycles = set()

		with open(traceAllMem, 'r') as traceAllMem_fp:
			for line in traceAllMem_fp:
				lst = list(map(int, re.findall(r'(?:(\d+)\t)', line, re.I|re.M)))
				allMemAddr.update(lst if lst else [])
		
		# mem_stats_readsBeforeEachWrite = {t2:{}  for (t1, t2) in traces}
		# mem_stats_writesBeforeEachRead = {t2:{}  for (t1, t2) in traces}
		mem_stats_readsBeforeEachDeath = {t2:{}  for (t1, t2) in traces}
		mem_stats_writesBeforeEachDeath = {t2:{}  for (t1, t2) in traces}
		mem_stats_cyclesBetweenEachDeath = {t2:{}  for (t1, t2) in traces}
		mem_stats_readsWritesSameCycleBeforeEachDeath = {t2:{}  for (t1, t2) in traces}
		mem_stats_cyclesDeadInWindow = {t2:{}  for (t1, t2) in traces}

		# mem_stats_readsBeforeEachWrite[traceA] = {addr: [0, []] for addr in allMemAddr}
		# mem_stats_writesBeforeEachRead[traceA] = {addr: [0, []] for addr in allMemAddr}
		mem_stats_readsBeforeEachDeath[traceA] = {addr: [0, []] for addr in allMemAddr}
		mem_stats_writesBeforeEachDeath[traceA] = {addr: [0, []] for addr in allMemAddr}
		mem_stats_cyclesBetweenEachDeath[traceA] = {addr: [0, []] for addr in allMemAddr}
		mem_stats_readsWritesSameCycleBeforeEachDeath[traceA] = {addr: [0, []] for addr in allMemAddr}
		
		num_windows = int(traceArr[-1]['cycle']//window_size + 1)
		mem_stats_cyclesDeadInWindow[traceA] = {addr: [0 for w in range(num_windows)] for addr in allMemAddr}
		# traverse upwards for each memAddr to find lastRead
		for memAddr in allMemAddr:
			for i, tracePkt in enumerate(traceArr[::-1]):
					if (memAddr in tracePkt['memAddrsRead']):	# READ
						lastReads_mem[memAddr] = {'cycle': tracePkt['cycle'], 'idx': traceArr.size - i-1}
						break

		# memAddr is live between each [write or first read] to [next read]
		totalLiveMemAddrs = 0
		for memAddr in allMemAddr:
			if (memAddr not in lastReads_mem):
				continue
			currIdx = lastReads_mem[memAddr]['idx']

			while (currIdx >= 0):
				traversed = None
				
				idx = currIdx
				# find lastWrite before this read
				if (currIdx >= 0):
					for tracePkt in traceArr[currIdx::-1]:
						idx -= 1
						traversed = True
						if memAddr in tracePkt['memAddrsWritten'] and not (memAddr in tracePkt['memAddrsRead']):
							# totalLiveMemAddrs += 1 if (memAddr not in tracePkt['liveMemAddrs']) else 0
							break
						# totalLiveMemAddrs += 1 if (memAddr not in tracePkt['liveMemAddrs']) else 0
						tracePkt['liveMemAddrs'].add(memAddr)
					currIdx = idx
				
				# now found prevWrite before/at this Read
				# find prevRead before this write
				if (currIdx >= 0):
					for tracePkt in traceArr[currIdx::-1]:
						idx -= 1
						traversed = True
						if (memAddr in tracePkt['memAddrsRead']):
							# totalLiveMemAddrs += 1 if (memAddr not in tracePkt['liveMemAddrs']) else 0
							tracePkt['liveMemAddrs'].add(memAddr)
							break
					currIdx = idx
				
				if (not traversed):
					currIdx -= 1
				# we found cycle of lastRead before the next write.
				# now find last write before that Read.
				
				
			# unset liveness for each memAddr until first read/write
			for tracePkt in traceArr:
				if (not (memAddr in tracePkt['memAddrsRead']) and not (memAddr in tracePkt['memAddrsWritten'])):
					if (memAddr in tracePkt['liveMemAddrs']):
						tracePkt['liveMemAddrs'].remove(memAddr)
				else:
					break
			
			countWritesBeforeRead = 0
			for tracePkt in traceArr:
				# find read
				if (memAddr in tracePkt['memAddrsRead']):
					if (countWritesBeforeRead > 0):
						# mem_stats_writesBeforeEachRead[traceA][memAddr][0] += countWritesBeforeRead
						# mem_stats_writesBeforeEachRead[traceA][memAddr][1].append(countWritesBeforeRead)
						if (memAddr in tracePkt['memAddrsWritten']):
							 # read/write in same cycle so for subsequent reads, init count = 1
							countWritesBeforeRead = 1
						else:
							countWritesBeforeRead = 0
				elif (memAddr in tracePkt['memAddrsWritten']):
					countWritesBeforeRead += 1
			
			countReadsBeforeWrite = 0
			for tracePkt in traceArr:
				# find write
				if (memAddr in tracePkt['memAddrsWritten']):
					if (countReadsBeforeWrite > 0):
						# mem_stats_readsBeforeEachWrite[traceA][memAddr][0] += countWritesBeforeRead
						# mem_stats_readsBeforeEachWrite[traceA][memAddr][1].append(countWritesBeforeRead)
						if (memAddr in tracePkt['memAddrsRead']):
							# read/write in same cycle so for subsequent writes, init count = 1
							countWritesBeforeRead = 1
						else:
							countWritesBeforeRead = 0
				elif (memAddr in tracePkt['memAddrsRead']):
						countWritesBeforeRead += 1		


		# FOR EACH REG: ASSIGN LIVENESS bit to whether reg is live after that insn, 
		# so roll this column of the array upwards
		traceArr['liveMemAddrs'] = np.roll(traceArr['liveMemAddrs'], -1)
		if traceArr.size: traceArr[-1]['liveMemAddrs'] = set()

		if PREDICT_LIVENESS:
			# max_gs_PHT_BITS, max_GHR_gs_len = 6, 6
			# min_PN, max_perc_N, step_PN = 2, 9, 1
			# min_PBB, max_budget_BITS, step_PBB = 1*10, 4*10, 10  # MAX_Budget_bits=log2(256*1024)
			# min_BTSB, max_bimodal_TABLE_SIZE_BITS, step_BTSB = 1, 4, 2
			# min_PN, max_perc_N, step_PN = 4, 36, 2 
			# min_PBB, max_budget_BITS, step_PBB = 3*10, 9*10, 10  # MAX_Budget_bits=log2(256*1024)
			# min_BTSB, max_bimodal_TABLE_SIZE_BITS, step_BTSB = 4, 36, 4
			
			new_dir = os.getcwd() + "/Prediction_MEM"
			access_rights = 0o755
			try:
			    os.mkdir(new_dir, access_rights)
			except OSError:
				pass

			def prediction_wrapper_mem(Predictor):
				"pred_over_time accuracy starts at 0 until first read"
				if traceArr.size:
					predictions_over_time_memAddr_last = {pred_t:(np.array([np.uint8(0) for t in range(traceArr.size)])) \
						for pred_t in Predictor.types}
					prev_acc = {pred_t:np.uint8(0) for pred_t in Predictor.types}

					for i, tracePkt in enumerate(traceArr):
						for m_idx, memAddr in enumerate(allMemAddr):
							# only predict if memAddr is an operand
							if memAddr in tracePkt['memAddrsRead']:
								Predictor.make_prediction(pc=np.int64(tracePkt['PC']),
									regNotMem=0,
									memAddr=memAddr)

								outcome = memAddr in tracePkt['liveMemAddrs']
								Predictor.train_predictor(np.int64(tracePkt['PC']), outcome, 
									regNotMem=0,
									memAddr=memAddr)

								for t in Predictor.types:
									predictions_over_time_memAddr_last[t][i] = \
										np.uint8(100*Predictor.pos_neg_pred[t][0]/max(1, np.sum(Predictor.pos_neg_pred[t])))
									# print(predictions_over_time_memAddr_last[t][i])
									# printToFile (predictions_over_time_memAddr_last[i])
									prev_acc[t] = predictions_over_time_memAddr_last[t][i]
							else:
								for t in Predictor.types:
									predictions_over_time_memAddr_last[t][i] = prev_acc[t]

					# print(predictions_over_time_memAddr_last)
					return predictions_over_time_memAddr_last

			plot_prediction_results(traceA, memNotReg=1, wrapper=prediction_wrapper_mem)

		if SAVEPERCENTLIVEMEMADDRS:
			with open("pLiveMemAddr.txt", "a+") as pLiveFile:
				if traceArr.size:
					percentLiveMemAddrs = defaultdict(int)
					for memAddr in allMemAddr:
						prev = None
						for tracePkt in traceArr:
							currCycle = tracePkt['cycle']
							if prev:
								# fill in for cycles between two instructions
								for c in range(prev['cycle']+np.uint64(1), currCycle):
									percentLiveMemAddrs[c] += int(memAddr in prev['liveMemAddrs'])
								totalLiveMemAddrs +=  (currCycle - (prev['cycle']+np.uint64(1))) if (memAddr in prev['liveMemAddrs']) else 0

							prev = tracePkt
							percentLiveMemAddrs[currCycle] += int(memAddr in tracePkt['liveMemAddrs'])

							x = str("MM " + traceA[:len(traceA)-13] + " %d, %d\t" % (currCycle, percentLiveMemAddrs[currCycle]))
							# x = str(traceA[:len(traceA)-13] + " cycle: %d, pLiveMemAddr: %d\t" % (currCycle, percentLiveMemAddrs[currCycle]))
							pLiveFile.write(str(x))

							totalLiveMemAddrs +=  1 if (memAddr in tracePkt['liveMemAddrs']) else 0
					# allLiveMemAddrs_traces[traceA] = [percentLiveMemAddrs, totalLiveMemAddrs,  (100/RAM_SIZE)*totalLiveMemAddrs/int(traceArr[-1]['cycle'])]

					# AM %d {number of avg mem addrs live per cycle}
					x = str("AM " + traceA[:len(traceA)-13] + " %d, %f" % (totalLiveMemAddrs, totalLiveMemAddrs/traceArr[-1]['cycle']))
					# x = str("AM " + traceA[:len(traceA)-13] + " %d, %f" % (totalLiveMemAddrs, (100/RAM_SIZE)*totalLiveMemAddrs/traceArr[-1]['cycle']))
					# x = str(traceA[:len(traceA)-13] + " totalLiveMemAddrs: %d, avgPercentlLiveMemAddrs: %f" % (totalLiveMemAddrs, (100/RAM_SIZE)*totalLiveMemAddrs/traceArr[-1]['cycle']))
					pLiveFile.write(str(x))

	if SAVESTATSBEST:

		first_trace = sorted([trace[:len(trace)-13] for (trace, t2) in traces])[0]

		# Add headings
		for t in ['reg', 'mem']:
			for s_idx, (trace, List) in enumerate(stats_best['bimodal'][t].items()):
				x = [', '.join(map(str, list(tup))) for tup in List]
				List.insert(0, x)
				if (trace == first_trace):
					List.insert(0, ('ACC, BimodalTableSizeBits'))
					stats_best['bimodal'][t][trace] = List[:2]
				else:
					stats_best['bimodal'][t][trace] = List[:1]
			for s_idx, (trace, List) in enumerate(stats_best['gshare'][t].items()):
				x = [', '.join(map(str, list(tup))) for tup in List]
				List.insert(0, x)
				if (trace == first_trace):
					List.insert(0, ('ACC, gs_PHT_BITS, GHR_gs_len'))
					stats_best['gshare'][t][trace] = List[:2]
				else:
					stats_best['gshare'][t][trace] = List[:1]
			for s_idx, (trace, List) in enumerate(stats_best['perc'][t].items()):
				x = [', '.join(map(str, list(tup))) for tup in List]
				List.insert(0, x)
				if (trace == first_trace):
					List.insert(0, ('ACC, GHR_perc_len, NumW, BudgetBits'))
					stats_best['perc'][t][trace] = List[:2]
				else:
					stats_best['perc'][t][trace] = List[:1]
			for s_idx, (trace, List) in enumerate(stats_best['choice_gs_perc'][t].items()):
				x = [', '.join(map(str, list(tup))) for tup in List]
				List.insert(0, x)
				if (trace == first_trace):
					List.insert(0, ('ACC, choice_table_BITS, gs_PHT_BITS, GHR_len, NumW, PercBudgetBits'))
					stats_best['choice_gs_perc'][t][trace] = List[:2]
				else:
					stats_best['choice_gs_perc'][t][trace] = List[:1]


		txt = 'stats_best.txt'.format(datetime.now().time())
		yml = 'stats_best_{}.yml'.format(datetime.now().time())
		with open(txt, 'w') as outfile:  
		    json.dump(stats_best, outfile)

		with open(txt) as f, open(yml, 'w') as outfile:
		   yaml.safe_dump(json.load(f), outfile, default_flow_style=False)






# fig, ax = plt.subplots()
# plt.ioff()
# # full_path=os.path.join(new_dir, "{}_AvgLiveRegs{}.png".format(traceA[:len(traceA)-13], ('_MEM' if memNotReg else '')))

# ax.set_title('{} - Bimodal Prediction Accuracy'.format(traceA[:len(traceA)-13]))
# ax.set_ylabel('Percent Accuracy')
# ax.set_xlabel('log2 (Number of Table Entries)')

# x_coord = np.array([x for x in range(min_BTSB, max_bimodal_TABLE_SIZE_BITS, step_BTSB)])
# ax.bar(x_coord, bimodal_prediction_results, tick_label = x_coord)
# fig.savefig(full_path)
# plt.close(fig)