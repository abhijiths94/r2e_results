#include "thumbulator/memory.hpp"

#include "cpu_flags.hpp"
#include "exit.hpp"
#include "trace.hpp"

namespace thumbulator {

///--- Add operations --------------------------------------------///

// ADCS - add with carry and update flags
uint32_t adcs(decode_result const *decoded)
{
  TRACE_INSTRUCTION("adcs r%u, r%u\n", decoded->Rd, decoded->Rm);

  uint32_t opA = cpu_get_gpr(decoded->Rd);
  uint32_t opB = cpu_get_gpr(decoded->Rm);
  uint32_t result = opA + opB + cpu_get_flag_c();

  cpu_set_gpr(decoded->Rd, result);

  do_nflag(result);
  do_zflag(result);
  do_cflag(opA, opB, cpu_get_flag_c());
  do_vflag(opA, opB, result);

  return 1;
}

// ADD - add small immediate to a register and update flags
uint32_t adds_i3(decode_result const *decoded)
{
  TRACE_INSTRUCTION("adds r%u, r%u, #0x%X\n", decoded->Rd, decoded->Rn, decoded->imm);

  uint32_t opA = cpu_get_gpr(decoded->Rn);
  uint32_t opB = zeroExtend32(decoded->imm);
  uint32_t result = opA + opB;

  cpu_set_gpr(decoded->Rd, result);

  do_nflag(result);
  do_zflag(result);
  do_cflag(opA, opB, 0);
  do_vflag(opA, opB, result);

  return 1;
}

// ADD - add large immediate to a register and update flags
uint32_t adds_i8(decode_result const *decoded)
{
  TRACE_INSTRUCTION("adds r%u, #0x%X\n", decoded->Rd, decoded->imm);

  uint32_t opA = cpu_get_gpr(decoded->Rd);
  uint32_t opB = zeroExtend32(decoded->imm);
  uint32_t result = opA + opB;

  cpu_set_gpr(decoded->Rd, result);

  do_nflag(result);
  do_zflag(result);
  do_cflag(opA, opB, 0);
  do_vflag(opA, opB, result);

  return 1;
}

// ADD - add two registers and update flags
uint32_t adds_r(decode_result const *decoded)
{
  TRACE_INSTRUCTION("adds r%u, r%u, r%u\n", decoded->Rd, decoded->Rn, decoded->Rm);

  uint32_t opA = cpu_get_gpr(decoded->Rn);
  uint32_t opB = cpu_get_gpr(decoded->Rm);
  uint32_t result = opA + opB;

  // fprintf(stdout, "result=0x%8.8X opA=0x%8.8X opB=0x%8.8X\n", result, opA, opB);

  cpu_set_gpr(decoded->Rd, result);

  do_nflag(result);
  do_zflag(result);
  do_cflag(opA, opB, 0);
  do_vflag(opA, opB, result);

  return 1;
}

// ADD - add two registers, one or both high no flags
uint32_t add_r(decode_result const *decoded)
{
  TRACE_INSTRUCTION("add r%u, r%u\n", decoded->Rd, decoded->Rm);

  // Check for malformed instruction
  if(decoded->Rd == 15 && decoded->Rm == 15) {
    //UNPREDICTABLE
    fprintf(stderr, "Error: Instruction format error.\n");
    terminate_simulation(1);
  }

  uint32_t opA = cpu_get_gpr(decoded->Rd);
  uint32_t opB = cpu_get_gpr(decoded->Rm);
  uint32_t result = opA + opB;

  // fprintf(stdout, "r%d=0x%x r%d=0x%x r%d=0x%x\n", decoded->Rd, opA, decoded->Rm, opB, decoded->Rd, result);

  // If changing the PC, check that thumb mode maintained
  if(decoded->Rd == GPR_PC)
    alu_write_pc(result);
  else
    cpu_set_gpr(decoded->Rd, result);

  // Instruction takes two cycles when PC is the destination
  return (decoded->Rd == GPR_PC) ? 2 : 1;
}

// ADD - add an immpediate to SP
uint32_t add_sp(decode_result const *decoded)
{
  TRACE_INSTRUCTION("add r%u, SP, #0x%02X\n", decoded->Rd, decoded->imm);

  uint32_t opA = cpu_get_sp();
  uint32_t opB = zeroExtend32(decoded->imm << 2);
  uint32_t result = opA + opB;

  // fprintf(stdout, "r%d=0x%8.8X + 0x%8.8X\n", decoded->Rd, opA, opB);

  cpu_set_gpr(decoded->Rd, result);

  return 1;
}

// ADR - add an immpediate to PC
uint32_t adr(decode_result const *decoded)
{
  TRACE_INSTRUCTION("adr r%u, PC, #0x%02X\n", decoded->Rd, decoded->imm);

  uint32_t opA = cpu_get_pc();
  // Align PC to 4 bytes
  opA = opA & 0xFFFFFFFC;
  uint32_t opB = zeroExtend32(decoded->imm << 2);
  uint32_t result = opA + opB;

  cpu_set_gpr(decoded->Rd, result);

  return 1;
}

///--- Subtract operations --------------------------------------------///

uint32_t subs_i3(decode_result const *decoded)
{
  TRACE_INSTRUCTION("subs r%u, r%u, #0x%X\n", decoded->Rd, decoded->Rn, decoded->imm);

  uint32_t opA = cpu_get_gpr(decoded->Rn);
  uint32_t opB = ~zeroExtend32(decoded->imm);
  uint32_t result = opA + opB + 1;

  cpu_set_gpr(decoded->Rd, result);

  do_nflag(result);
  do_zflag(result);
  do_cflag(opA, opB, 1);
  do_vflag(opA, opB, result);

  return 1;
}

uint32_t subs_i8(decode_result const *decoded)
{
  TRACE_INSTRUCTION("subs r%u, #0x%02X\n", decoded->Rd, decoded->imm);

  uint32_t opA = cpu_get_gpr(decoded->Rd);
  uint32_t opB = ~zeroExtend32(decoded->imm);
  uint32_t result = opA + opB + 1;

  cpu_set_gpr(decoded->Rd, result);

  do_nflag(result);
  do_zflag(result);
  do_cflag(opA, opB, 1);
  do_vflag(opA, opB, result);

  return 1;
}

uint32_t subs(decode_result const *decoded)
{
  TRACE_INSTRUCTION("subs r%u, r%u, r%u\n", decoded->Rd, decoded->Rn, decoded->Rm);

  uint32_t opA = cpu_get_gpr(decoded->Rn);
  uint32_t opB = ~cpu_get_gpr(decoded->Rm);
  uint32_t result = opA + opB + 1;

  cpu_set_gpr(decoded->Rd, result);

  do_nflag(result);
  do_zflag(result);
  do_cflag(opA, opB, 1);
  do_vflag(opA, opB, result);

  return 1;
}

uint32_t sub_sp(decode_result const *decoded)
{
  TRACE_INSTRUCTION("sub SP, #0x%02X\n", decoded->imm);

  uint32_t opA = cpu_get_sp();
  uint32_t opB = ~zeroExtend32(decoded->imm << 2);
  uint32_t result = opA + opB + 1;

  cpu_set_sp(result);

  return 1;
}

uint32_t sbcs(decode_result const *decoded)
{
  TRACE_INSTRUCTION("sbcs r%u, r%u\n", decoded->Rd, decoded->Rm);

  uint32_t opA = cpu_get_gpr(decoded->Rd);
  uint32_t opB = ~cpu_get_gpr(decoded->Rm);
  uint32_t result = opA + opB + cpu_get_flag_c();

  cpu_set_gpr(decoded->Rd, result);

  do_nflag(result);
  do_zflag(result);
  do_cflag(opA, opB, cpu_get_flag_c());
  do_vflag(opA, opB, result);

  return 1;
}

uint32_t rsbs(decode_result const *decoded)
{
  TRACE_INSTRUCTION("rsbs r%u, r%u, #0\n", decoded->Rd, decoded->Rn);

  uint32_t opA = 0;
  uint32_t opB = ~(cpu_get_gpr(decoded->Rn));
  uint32_t result = opA + opB + 1;

  cpu_set_gpr(decoded->Rd, result);

  do_nflag(result);
  do_zflag(result);
  do_cflag(opA, opB, 1);
  do_vflag(opA, opB, result);

  return 1;
}

///--- Multiply operations --------------------------------------------///

// MULS - multiply the source and destination and store 32-bits in dest
// Does not update carry or overflow: simple mult
uint32_t muls(decode_result const *decoded)
{
  TRACE_INSTRUCTION("muls r%u, r%u\n", decoded->Rd, decoded->Rm);

  uint32_t opA = cpu_get_gpr(decoded->Rd);
  uint32_t opB = cpu_get_gpr(decoded->Rm);
  uint32_t result = opA * opB;

  cpu_set_gpr(decoded->Rd, result);

  do_nflag(result);
  do_zflag(result);

  return 32;
}
}
