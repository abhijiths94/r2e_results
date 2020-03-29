#include "thumbulator/memory.hpp"

#include "cpu_flags.hpp"
#include "trace.hpp"

namespace thumbulator {

uint32_t breakpoint(decode_result const *decoded)
{
  return 0;
}

///--- Move operations -------------------------------------------///

// MOVS - write an immediate to the destination register
uint32_t movs_i(decode_result const *decoded)
{
  TRACE_INSTRUCTION("movs r%u, #0x%02X\n", decoded->Rd, decoded->imm);

  uint32_t opA = zeroExtend32(decoded->imm);
  cpu_set_gpr(decoded->Rd, opA);

  do_nflag(opA);
  do_zflag(opA);

  return 1;
}

// MOV - copy the source register value to the destination register
uint32_t mov_r(decode_result const *decoded)
{
  TRACE_INSTRUCTION("mov r%u, r%u\n", decoded->Rd, decoded->Rm);

  uint32_t opA = cpu_get_gpr(decoded->Rm);

  if(decoded->Rd == GPR_PC)
    alu_write_pc(opA);
  else
    cpu_set_gpr(decoded->Rd, opA);
  
  // fprintf(stdout, "r%d=0x%8.8X\n", decoded->Rd, opA);

  return 1;
}

// MOVS - copy the low source register value to the destination low register
uint32_t movs_r(decode_result const *decoded)
{
  TRACE_INSTRUCTION("movs r%u, r%u\n", decoded->Rd, decoded->Rm);

  uint32_t opA = cpu_get_gpr(decoded->Rm);
  cpu_set_gpr(decoded->Rd, opA);

  do_nflag(opA);
  do_zflag(opA);

  return 1;
}

///--- Bit twiddling operations -------------------------------------------///

// SXTB - Sign extend a byte to a word
uint32_t sxtb(decode_result const *decoded)
{
  TRACE_INSTRUCTION("sxtb r%u, r%u\n", decoded->Rd, decoded->Rm);

  uint32_t result = 0xFF & cpu_get_gpr(decoded->Rm);
  result = (result & 0x80) != 0 ? (result | 0xFFFFFF00) : result;

  cpu_set_gpr(decoded->Rd, result);

  return 1;
}

// SXTH - Sign extend a halfword to a word
uint32_t sxth(decode_result const *decoded)
{
  TRACE_INSTRUCTION("sxth r%u, r%u\n", decoded->Rd, decoded->Rm);

  uint32_t result = 0xFFFF & cpu_get_gpr(decoded->Rm);
  result = (result & 0x8000) != 0 ? (result | 0xFFFF0000) : result;

  cpu_set_gpr(decoded->Rd, result);

  return 1;
}

// UXTB - Extend a byte to a word
uint32_t uxtb(decode_result const *decoded)
{
  TRACE_INSTRUCTION("uxtb r%u, r%u\n", decoded->Rd, decoded->Rm);

  uint32_t result = 0xFF & cpu_get_gpr(decoded->Rm);
  cpu_set_gpr(decoded->Rd, result);

  return 1;
}

// UXTH - Extend a halfword to a word
uint32_t uxth(decode_result const *decoded)
{
  TRACE_INSTRUCTION("uxth r%u, r%u\n", decoded->Rd, decoded->Rm);

  uint32_t result = 0xFFFF & cpu_get_gpr(decoded->Rm);
  cpu_set_gpr(decoded->Rd, result);

  return 1;
}

// REV - Reverse ordering of bytes in a word
uint32_t rev(decode_result const *decoded)
{
  TRACE_INSTRUCTION("rev r%u, r%u\n", decoded->Rd, decoded->Rm);

  uint32_t opA = cpu_get_gpr(decoded->Rm);
  uint32_t result = opA << 24;
  result |= (opA << 8) & 0xFF0000;
  result |= (opA >> 8) & 0xFF00;
  result |= (opA >> 24);

  cpu_set_gpr(decoded->Rd, result);

  return 1;
}

// REV16 - Reverse ordering of bytes in a packed halfword
uint32_t rev16(decode_result const *decoded)
{
  TRACE_INSTRUCTION("rev16 r%u, r%u\n", decoded->Rd, decoded->Rm);

  uint32_t opA = cpu_get_gpr(decoded->Rm);
  uint32_t result = (opA << 8) & 0xFF000000;
  result |= (opA >> 8) & 0xFF0000;
  result |= (opA << 8) & 0xFF00;
  result |= (opA >> 8) & 0xFF;

  cpu_set_gpr(decoded->Rd, result);

  return 1;
}

// REVSH - Reverse ordering of bytes in a signed halfword
uint32_t revsh(decode_result const *decoded)
{
  TRACE_INSTRUCTION("revsh r%u, r%u\n", decoded->Rd, decoded->Rm);

  uint32_t opA = cpu_get_gpr(decoded->Rm);
  uint32_t result = (opA & 0x8) != 0 ? (0xFFFFFF00 | opA) : (0xFF & opA);
  result <<= 8;
  result |= (opA >> 8) & 0xFF;

  cpu_set_gpr(decoded->Rd, result);

  return 1;
}
}
