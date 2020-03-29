#ifndef THUMBULATOR_CPU_FLAGS_H
#define THUMBULATOR_CPU_FLAGS_H

#include "thumbulator/cpu.hpp"

namespace thumbulator {

// Define bit fields of APSR
#define FLAG_N_INDEX 31
#define FLAG_Z_INDEX 30
#define FLAG_C_INDEX 29
#define FLAG_V_INDEX 28
#define FLAG_N_MASK (1 << FLAG_N_INDEX)
#define FLAG_Z_MASK (1 << FLAG_Z_INDEX)
#define FLAG_C_MASK (1 << FLAG_C_INDEX)
#define FLAG_V_MASK (1 << FLAG_V_INDEX)

// GPRs with special functions
#define GPR_SP 13
#define GPR_LR 14
#define cpu_get_sp() cpu_get_gpr(GPR_SP)
#define cpu_set_sp(x) (cpu_set_gpr(GPR_SP, (x)))
#define cpu_get_lr() cpu_get_gpr(GPR_LR)
#define cpu_set_lr(x) cpu_set_gpr(GPR_LR, (x))

// Get, set, and compute the CPU flags
#define cpu_get_flag_z() ((cpu.apsr & FLAG_Z_MASK) >> FLAG_Z_INDEX)
#define cpu_get_flag_n() ((cpu.apsr & FLAG_N_MASK) >> FLAG_N_INDEX)
#define cpu_get_flag_c() ((cpu.apsr & FLAG_C_MASK) >> FLAG_C_INDEX)
#define cpu_get_flag_v() ((cpu.apsr & FLAG_V_MASK) >> FLAG_V_INDEX)
#define cpu_set_flag_z(x) cpu.apsr = ((((x)&0x1) << FLAG_Z_INDEX) | (cpu.apsr & ~FLAG_Z_MASK))
#define cpu_set_flag_n(x) cpu.apsr = ((((x)&0x1) << FLAG_N_INDEX) | (cpu.apsr & ~FLAG_N_MASK))
#define cpu_set_flag_c(x) cpu.apsr = ((((x)&0x1) << FLAG_C_INDEX) | (cpu.apsr & ~FLAG_C_MASK))
#define cpu_set_flag_v(x) cpu.apsr = ((((x)&0x1) << FLAG_V_INDEX) | (cpu.apsr & ~FLAG_V_MASK))

#define do_zflag(x) cpu_set_flag_z(((x) == 0) ? 1 : 0)
#define do_nflag(x) cpu_set_flag_n((x) >> 31)
#define do_vflag(a, b, r) \
  cpu_set_flag_v(         \
      (((a) >> 31) & ((b) >> 31) & ~((r) >> 31)) | (~((a) >> 31) & ~((b) >> 31) & ((r) >> 31)))

static void do_cflag(uint32_t a, uint32_t b, uint32_t carry)
{
  uint32_t result;

  result = (a & 0x7FFFFFFF) + (b & 0x7FFFFFFF) + carry; //carry in
  result = (result >> 31) + (a >> 31) + (b >> 31);      //carry out
  cpu_set_flag_c(result >> 1);
}

#define cpu_get_apsr() (cpu.apsr)
#define cpu_set_apsr(x) cpu.apsr = (x)

// Other SPR
#define CPU_MODE_HANDLER 0
#define CPU_MODE_THREAD 1
#define cpu_mode_is_handler() (cpu.mode == 0x0)
#define cpu_mode_is_thread() (cpu.mode == 0x1)
#define cpu_mode_handler() cpu.mode = (0x0)
#define cpu_mode_thread() cpu.mode = (0x1)
#define cpu_get_ipsr() (cpu.ipsr)
#define cpu_set_ipsr(x) cpu.ipsr = (x & 0x1F)
#define CPU_STACK_MAIN 0
#define CPU_STACK_PROCESS 1
#define cpu_stack_is_main() ((cpu.control & 0x2) == 0x0)
#define cpu_stack_is_process() (~cpu_stack_is_main())
#define cpu_stack_use_main() cpu.control = (cpu.control & ~0x2)
#define cpu_stack_use_process() cpu.control = (cpu.control | 0x2)

// Sign extension
#define zeroExtend32(x) (x)
#define signExtend32(x, n) \
  (((((x) >> ((n)-1)) & 0x1) != 0) ? (~((unsigned int)0) << (n)) | (x) : (x))

// Special write to PC
#define alu_write_pc(x)    \
  do {                     \
    BRANCH_WAS_TAKEN = 1;  \
    cpu_set_pc((x) | 0x1); \
  } while(0)
}
#endif //THUMBULATOR_CPU_FLAGS_H
