#include "thumbulator/memory.hpp"

#include "cpu_flags.hpp"
#include "exit.hpp"
#include "trace.hpp"

namespace thumbulator {

///--- Compare operations --------------------------------------------///

uint32_t cmn(decode_result const *decoded)
{
  TRACE_INSTRUCTION("cmns r%u, r%u\n", decoded->Rm, decoded->Rn);

  uint32_t opA = cpu_get_gpr(decoded->Rm);
  uint32_t opB = cpu_get_gpr(decoded->Rn);
  uint32_t result = opA + opB;

  do_nflag(result);
  do_zflag(result);
  do_cflag(opA, opB, 0);
  do_vflag(opA, opB, result);

  return 1;
}

uint32_t cmp_i(decode_result const *decoded)
{
  TRACE_INSTRUCTION("cmp r%u, #0x%02X\n", decoded->Rd, decoded->imm);

  uint32_t opA = cpu_get_gpr(decoded->Rd);
  uint32_t opB = ~zeroExtend32(decoded->imm);
  uint32_t result = opA + opB + 1;

  do_nflag(result);
  do_zflag(result);
  do_cflag(opA, opB, 1);
  do_vflag(opA, opB, result);

  return 1;
}

uint32_t cmp_r(decode_result const *decoded)
{
  TRACE_INSTRUCTION("cmp r%u, r%u\n", decoded->Rd, decoded->Rm); // Rn to Rd due to decoding

  uint32_t opA = cpu_get_gpr(decoded->Rd);
  uint32_t opB = ~zeroExtend32(cpu_get_gpr(decoded->Rm));
  uint32_t result = opA + opB + 1;

  do_nflag(result);
  do_zflag(result);
  do_cflag(opA, opB, 1);
  do_vflag(opA, opB, result);

  return 1;
}

// TST - Test for matches
uint32_t tst(decode_result const *decoded)
{
  TRACE_INSTRUCTION("tst r%u, r%u\n", decoded->Rn, decoded->Rd); // Switch operands to ease decoding

  uint32_t opA = cpu_get_gpr(decoded->Rd);
  uint32_t opB = cpu_get_gpr(decoded->Rm);
  uint32_t result = opA & opB;

  do_nflag(result);
  do_zflag(result);

  return 1;
}

///--- Branch operations --------------------------------------------///

// B - Unconditional branch
uint32_t b(decode_result const *decoded)
{
  uint32_t offset = signExtend32(decoded->imm << 1, 12);

  TRACE_INSTRUCTION("B 0x%08X\n", offset);

  uint32_t result = offset + cpu_get_pc();
  cpu_set_pc(result);
  BRANCH_WAS_TAKEN = 1;

  return TIMING_BRANCH;
}

// B - Conditional branch
uint32_t b_c(decode_result const *decoded)
{
  TRACE_INSTRUCTION("Bcc 0x%08X\n", decoded->imm);
  uint32_t taken = 0;

  switch(decoded->cond) {
  case 0x0: // b eq, z set
    TRACE_INSTRUCTION("beq 0x%08X\n", decoded->imm);
    if(cpu_get_flag_z())
      taken = 1;
    break;
  case 0x1: // b ne, z clear
    TRACE_INSTRUCTION("bne 0x%08X\n", decoded->imm);
    if(!cpu_get_flag_z())
      taken = 1;
    break;
  case 0x2: // b cs, c set
    TRACE_INSTRUCTION("bcs 0x%08X\n", decoded->imm);
    if(cpu_get_flag_c())
      taken = 1;
    break;
  case 0x3: // b cc, c clear
    TRACE_INSTRUCTION("bcc 0x%08X\n", decoded->imm);
    if(!cpu_get_flag_c())
      taken = 1;
    break;
  case 0x4: // b mi, n set
    TRACE_INSTRUCTION("bmi 0x%08X\n", decoded->imm);
    if(cpu_get_flag_n())
      taken = 1;
    break;
  case 0x5: // b pl, n clear
    TRACE_INSTRUCTION("bpl 0x%08X\n", decoded->imm);
    if(!cpu_get_flag_n())
      taken = 1;
    break;
  case 0x6: // b vs, v set
    TRACE_INSTRUCTION("bvs 0x%08X\n", decoded->imm);
    if(cpu_get_flag_v())
      taken = 1;
    break;
  case 0x7: // b vc, v clear
    TRACE_INSTRUCTION("bvc 0x%08X\n", decoded->imm);
    if(!cpu_get_flag_v())
      taken = 1;
    break;
  case 0x8: // b hi, c set z clear
    TRACE_INSTRUCTION("bhi 0x%08X\n", decoded->imm);
    if(cpu_get_flag_c() && !cpu_get_flag_z())
      taken = 1;
    break;
  case 0x9: // b ls, c clear or z set
    TRACE_INSTRUCTION("bls 0x%08X\n", decoded->imm);
    if(cpu_get_flag_z() || !cpu_get_flag_c())
      taken = 1;
    break;
  case 0xA: // b ge, N  ==  V
    TRACE_INSTRUCTION("bge 0x%08X\n", decoded->imm);
    if(cpu_get_flag_n() == cpu_get_flag_v())
      taken = 1;
    break;
  case 0xB: // b lt, N ! =  V
    TRACE_INSTRUCTION("blt 0x%08X\n", decoded->imm);
    if(cpu_get_flag_n() != cpu_get_flag_v())
      taken = 1;
    break;
  case 0xC: // b gt, Z == 0 and N  ==  V
    TRACE_INSTRUCTION("bgt 0x%08X\n", decoded->imm);
    if(!cpu_get_flag_z() && (cpu_get_flag_n() == cpu_get_flag_v()))
      taken = 1;
    break;
  case 0xD: // b le, Z == 1 or N ! =  V
    TRACE_INSTRUCTION("ble 0x%08X\n", decoded->imm);
    if(cpu_get_flag_z() || (cpu_get_flag_n() != cpu_get_flag_v()))
      taken = 1;
    break;
  default:
    fprintf(stderr, "Error: Malformed instruction!");
    terminate_simulation(1);
  }

  if(taken == 0) {
    return 1;
  }

  uint32_t offset = signExtend32(decoded->imm << 1, 9);
  uint32_t pc = cpu_get_pc();
  uint32_t result = offset + pc;
  cpu_set_pc(result);
  BRANCH_WAS_TAKEN = 1;

  return TIMING_BRANCH;
}

// BLX - Unconditional branch and link with switch to ARM mode
uint32_t blx(decode_result const *decoded)
{
  TRACE_INSTRUCTION("blx r%u\n", decoded->Rm);

  uint32_t address = cpu_get_gpr(decoded->Rm);

  if((address & 0x1) == 0) {
    fprintf(stderr, "Error: Interworking not supported: 0x%8.8X\n", address);
    terminate_simulation(1);
  }

  cpu_set_lr(cpu_get_pc() - 0x2);
  cpu_set_pc(address);
  BRANCH_WAS_TAKEN = 1;

  return TIMING_BRANCH;
}

// BX - Unconditional branch with switch to ARM mode
// Also may be used as exception return
uint32_t bx(decode_result const *decoded)
{
  TRACE_INSTRUCTION("bx r%u\n", decoded->Rm);

  uint32_t address = cpu_get_gpr(decoded->Rm);

  if((address & 0x1) == 0) {
    fprintf(stderr, "Error: Interworking not supported: 0x%8.8X\n", address);
    terminate_simulation(1);
  }

  if((address >> 28) == 0xF) {
    fprintf(stderr, "Error: CPU exceptions not supported: 0x%8.8X\n", address);
  } else {
    cpu_set_pc(address);
  }

  BRANCH_WAS_TAKEN = 1;

  return TIMING_BRANCH;
}

// BL - Unconditional branch and link
// 32 bit instruction
uint32_t bl(decode_result const *decoded)
{
  uint32_t result = signExtend32(decoded->imm << 1, 25);

  TRACE_INSTRUCTION("bl 0x%08X\n", result);

  result += cpu_get_pc();

  cpu_set_lr(cpu_get_pc());
  cpu_set_pc(result);
  BRANCH_WAS_TAKEN = 1;

  return TIMING_BRANCH_LINK;
}
}
