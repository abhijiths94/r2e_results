#ifndef THUMBULATOR_CPU_H
#define THUMBULATOR_CPU_H

#include <cstdint>

#include "thumbulator/decode.hpp"

namespace thumbulator {

constexpr auto CPU_FREQ = 24000000;

/**
 * The state of an armv6m CPU.
 */
struct cpu_state {
  /**
   * General-purpose register including FP, SP, LR, and PC with dirty bit.
   */
  uint32_t gpr[16][2];

  /**
   * Application program status register.
   */
  uint32_t apsr;

  /**
   * Interrupt program status register.
   *
   * Exception number.
   */
  uint32_t ipsr;

  /**
   * Exception program status register.
   *
   * Not software readable.
   */
  uint32_t espr;

  uint32_t primask;
  uint32_t control;
  uint32_t sp_main;
  uint32_t sp_process;
  uint32_t mode;

  /**
   * Bit mask of pending exceptions.
   */
  uint32_t exceptmask;
};

/**
 * Informs fetch that previous instruction caused a control flow change
 */
extern bool BRANCH_WAS_TAKEN;

/**
 * Whether or not the exit instruction has been executed.
 */
extern bool EXIT_INSTRUCTION_ENCOUNTERED;

/**
 * If oracle backup policy is set for memory renaming
 */
extern bool OPTIMAL_BACKUP_POLICY;

/**
 * Enable mock execute, memory and write-back for optimal backup policy
 */
extern bool mock_exmemwb; 

/**
 * Resets the CPU according to the specification.
 */
void cpu_reset();

extern cpu_state cpu;

/**
 * Get a general-purpose register.
 */
uint32_t cpu_get_gpr(uint8_t x);

bool cpu_get_gpr_dbit(uint8_t x);

/**
 * Set a general-purpose register.
 */
void cpu_set_gpr(uint8_t x, uint32_t y);

void cpu_clear_gpr_dbit();

/**
 * The register-index of the program counter.
 */
#define GPR_PC 15

/**
 * Get the value currently stored in the program counter.
 */
#define cpu_get_pc() cpu_get_gpr(GPR_PC)

/**
 * Change the value stored in the program counter.
 */
#define cpu_set_pc(x) cpu_set_gpr(GPR_PC, (x))

struct system_tick {
  uint32_t control;
  uint32_t reload;
  uint32_t value;
  uint32_t calib;
};

extern system_tick SYSTICK;

/**
 * Cycles taken for branch instructions.
 */
#define TIMING_BRANCH 2

/**
 * Cycles taken for the branch with link instruction.
 */
#define TIMING_BRANCH_LINK 3

/**
 * Cycles taken for instructions that update the program counter.
 */
#define TIMING_PC_UPDATE 2

/**
 * Cycles taken for memory instructions.
 */
#define TIMING_MEM 2

/**
 * Perform the execute, mem, and write-back stages.
 *
 * @param instruction The instruction to execute.
 * @param decoded The result from the decode stage.
 *
 * @return The number of cycles taken.
 */
uint32_t exmemwb(uint16_t instruction, decode_result const *decoded);

/**
 * Perform a mock execute, mem and write-back (no change to cpu state and memory)
 *
 * @param instruction The instruction to execute.
 * @param decoded The result from the decode stage.
 * 
 * @return the address if it is a memory instruction
 * @return the number of times memory is accesses by a load/store (e.g. ldm/stm)
 */

uint32_t exmemwb_mock(uint16_t instruction, decode_result const *decoded, bool& mem_write, bool& mem_op, bool& branch, bool& branch_link, uint32_t& numMemAccess);
}

#endif //THUMBULATOR_CPU_H
