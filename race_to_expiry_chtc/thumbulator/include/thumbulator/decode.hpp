#ifndef THUMBULATOR_DECODE_H
#define THUMBULATOR_DECODE_H

#include <cstdint>

namespace thumbulator {

/**
 * Registers that hold the results of instruction decoding.
 *
 * Passed to exectue, memory access, and write-back stage in decoded variable.
 */
struct decode_result {
  /**
   * Destination register-index.
   */
  uint8_t Rd;

  /**
   * Operand register-index.
   */
  uint8_t Rm;

  /**
   * Operand register-index.
   */
  uint8_t Rn;

  /**
   * Immediate value.
   */
  uint32_t imm;

  /**
   * Condition.
   */
  uint32_t cond;

  /**
   * Register list for push/pop instructions.
   */
  uint32_t register_list;
};

/**
 * Interface to the decode stage.
 *
 * @param instruction The instruction to decode.
 * @return The decode stage registers based upon the passed instruction.
 */
decode_result decode(uint16_t instruction);
}

#endif
