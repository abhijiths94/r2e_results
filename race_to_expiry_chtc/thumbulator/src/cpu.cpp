#include "thumbulator/cpu.hpp"

#include "thumbulator/memory.hpp"
#include "cpu_flags.hpp"
#include "exit.hpp"

#include <cstring>

namespace thumbulator {

uint16_t insn;

bool BRANCH_WAS_TAKEN = false;
bool EXIT_INSTRUCTION_ENCOUNTERED = false;
bool OPTIMAL_BACKUP_POLICY = false;
bool mock_exmemwb = false;

// Reset CPU state in accordance with B1.5.5 and B3.2.2
void cpu_reset()
{
  constexpr auto ESPR_T = (1 << 24);

  // Initialize the special-purpose registers
  cpu.apsr = 0;       // No flags set
  cpu.ipsr = 0;       // No exception number
  cpu.espr = ESPR_T;  // Thumb mode
  cpu.primask = 0;    // No except priority boosting
  cpu.control = 0;    // Priv mode and main stack
  cpu.sp_main = 0;    // Stack pointer for exception handling
  cpu.sp_process = 0; // Stack pointer for process

  // Clear the general purpose registers
  memset(cpu.gpr, 0, sizeof(cpu.gpr));

  // Set the reserved GPRs
  cpu.gpr[GPR_LR][0] = 0;

  // May need to add logic to send writes and reads to the
  // correct stack pointer
  // Set the stack pointers
  load(0, &cpu.sp_main, 0);
  cpu.sp_main &= 0xFFFFFFFC;
  cpu.sp_process = 0;
  cpu_set_sp(cpu.sp_main);

  // Set the program counter to the address of the reset exception vector
  uint32_t startAddr;
  load(0x4, &startAddr, 0);
  cpu_set_pc(startAddr);

  // No pending exceptions
  cpu.exceptmask = 0;

  // Check for attempts to go to ARM mode
  if((cpu_get_pc() & 0x1) == 0) {
    printf("Error: Reset PC to an ARM address 0x%08X\n", cpu_get_pc());
    terminate_simulation(1);
  }

  // Reset the SYSTICK unit
  SYSTICK.control = 0x4;
  SYSTICK.reload = 0x0;
  SYSTICK.value = 0x0;
  SYSTICK.calib = CPU_FREQ / 100 | 0x80000000;
}

uint32_t cpu_get_gpr(uint8_t x)
{
  return cpu.gpr[x][0];
}

bool cpu_get_gpr_dbit(uint8_t x)
{
  return cpu.gpr[x][1];
}

void cpu_set_gpr(uint8_t x, uint32_t y)
{
  cpu.gpr[x][0] = y;
  cpu.gpr[x][1] = 1;
}

void cpu_clear_gpr_dbit()
{
  for (uint8_t x=0; x<16; x++)
    cpu.gpr[x][1] = 0;
}

cpu_state cpu;
system_tick SYSTICK;

uint32_t adcs(decode_result const *);
uint32_t adds_i3(decode_result const *);
uint32_t adds_i8(decode_result const *);
uint32_t adds_r(decode_result const *);
uint32_t add_r(decode_result const *);
uint32_t add_sp(decode_result const *);
uint32_t adr(decode_result const *);
uint32_t subs_i3(decode_result const *);
uint32_t subs_i8(decode_result const *);
uint32_t subs(decode_result const *);
uint32_t sub_sp(decode_result const *);
uint32_t sbcs(decode_result const *);
uint32_t rsbs(decode_result const *);
uint32_t muls(decode_result const *);
uint32_t cmn(decode_result const *);
uint32_t cmp_i(decode_result const *);
uint32_t cmp_r(decode_result const *);
uint32_t tst(decode_result const *);
uint32_t b(decode_result const *);
uint32_t b_c(decode_result const *);
uint32_t blx(decode_result const *);
uint32_t bx(decode_result const *);
uint32_t bl(decode_result const *);
uint32_t ands(decode_result const *);
uint32_t bics(decode_result const *);
uint32_t eors(decode_result const *);
uint32_t orrs(decode_result const *);
uint32_t mvns(decode_result const *);
uint32_t asrs_i(decode_result const *);
uint32_t asrs_r(decode_result const *);
uint32_t lsls_i(decode_result const *);
uint32_t lsrs_i(decode_result const *);
uint32_t lsls_r(decode_result const *);
uint32_t lsrs_r(decode_result const *);
uint32_t rors(decode_result const *);
uint32_t ldm(decode_result const *);
uint32_t stm(decode_result const *);
uint32_t pop(decode_result const *);
uint32_t push(decode_result const *);
uint32_t ldr_i(decode_result const *);
uint32_t ldr_sp(decode_result const *);
uint32_t ldr_lit(decode_result const *);
uint32_t ldr_r(decode_result const *);
uint32_t ldrb_i(decode_result const *);
uint32_t ldrb_r(decode_result const *);
uint32_t ldrh_i(decode_result const *);
uint32_t ldrh_r(decode_result const *);
uint32_t ldrsb_r(decode_result const *);
uint32_t ldrsh_r(decode_result const *);
uint32_t str_i(decode_result const *);
uint32_t str_sp(decode_result const *);
uint32_t str_r(decode_result const *);
uint32_t strb_i(decode_result const *);
uint32_t strb_r(decode_result const *);
uint32_t strh_i(decode_result const *);
uint32_t strh_r(decode_result const *);
uint32_t movs_i(decode_result const *);
uint32_t mov_r(decode_result const *);
uint32_t movs_r(decode_result const *);
uint32_t sxtb(decode_result const *);
uint32_t sxth(decode_result const *);
uint32_t uxtb(decode_result const *);
uint32_t uxth(decode_result const *);
uint32_t rev(decode_result const *);
uint32_t rev16(decode_result const *);
uint32_t revsh(decode_result const *);
uint32_t breakpoint(decode_result const *);

uint32_t exmemwb_error(decode_result const *decoded)
{
  fprintf(stderr, "Error: Unsupported instruction: Unable to execute\n");
  terminate_simulation(1);
  return 0;
}

uint32_t exmemwb_exit_simulation(decode_result const *decoded)
{
  EXIT_INSTRUCTION_ENCOUNTERED = true;

  return 0;
}

// Execute functions that require more opcode bits than the first 6
uint32_t (*executeJumpTable6[2])(decode_result const *) = {
    adds_r, /* 060 - 067 */
    subs    /* 068 - 06F */
};

uint32_t entry6(decode_result const *decoded)
{
  return executeJumpTable6[(insn >> 9) & 0x1](decoded);
}

uint32_t (*executeJumpTable7[2])(decode_result const *) = {
    adds_i3, /* (070 - 077) */
    subs_i3  /* (078 - 07F) */
};

uint32_t entry7(decode_result const *decoded)
{
  return executeJumpTable7[(insn >> 9) & 0x1](decoded);
}

uint32_t (*executeJumpTable16[16])(decode_result const *) = {ands, eors, lsls_r, lsrs_r, asrs_r,
    adcs, sbcs, rors, tst, rsbs, cmp_r, exmemwb_error, orrs, muls, bics, mvns};

uint32_t entry16(decode_result const *decoded)
{
  return executeJumpTable16[(insn >> 6) & 0xF](decoded);
}

uint32_t (*executeJumpTable17[8])(decode_result const *) = {
    add_r,        /* (110 - 113) */
    add_r, cmp_r, /* (114 - 117) */
    cmp_r, mov_r, /* (118 - 11B) */
    mov_r, bx,    /* (11C - 11D) */
    blx           /* (11E - 11F) */
};

uint32_t entry17(decode_result const *decoded)
{
  return executeJumpTable17[(insn >> 7) & 0x7](decoded);
}

uint32_t (*executeJumpTable20[2])(decode_result const *) = {
    str_r, /* (140 - 147) */
    strh_r /* (148 - 14F) */
};

uint32_t entry20(decode_result const *decoded)
{
  return executeJumpTable20[(insn >> 9) & 0x1](decoded);
}

uint32_t (*executeJumpTable21[2])(decode_result const *) = {
    strb_r, /* (150 - 157) */
    ldrsb_r /* (158 - 15F) */
};

uint32_t entry21(decode_result const *decoded)
{
  return executeJumpTable21[(insn >> 9) & 0x1](decoded);
}

uint32_t (*executeJumpTable22[2])(decode_result const *) = {
    ldr_r, /* (160 - 167) */
    ldrh_r /* (168 - 16F) */
};

uint32_t entry22(decode_result const *decoded)
{
  return executeJumpTable22[(insn >> 9) & 0x1](decoded);
}

uint32_t (*executeJumpTable23[2])(decode_result const *) = {
    ldrb_r, /* (170 - 177) */
    ldrsh_r /* (178 - 17F) */
};

uint32_t entry23(decode_result const *decoded)
{
  return executeJumpTable23[(insn >> 9) & 0x1](decoded);
}

uint32_t (*executeJumpTable44[16])(decode_result const *) = {add_sp, /* (2C0 - 2C1) */
    add_sp, sub_sp,                                                  /* (2C2 - 2C3) */
    sub_sp, exmemwb_error, exmemwb_error, exmemwb_error, exmemwb_error, sxth, sxtb, uxth, uxtb,
    exmemwb_error, exmemwb_error, exmemwb_error, exmemwb_error};

uint32_t entry44(decode_result const *decoded)
{
  return executeJumpTable44[(insn >> 6) & 0xF](decoded);
}

uint32_t (*executeJumpTable46[16])(decode_result const *) = {exmemwb_error, exmemwb_error,
    exmemwb_error, exmemwb_error, exmemwb_error, exmemwb_error, exmemwb_error, exmemwb_error, rev,
    rev16, exmemwb_error, exmemwb_error, exmemwb_error, exmemwb_error, exmemwb_error,
    exmemwb_error};

uint32_t entry46(decode_result const *decoded)
{
  return executeJumpTable46[(insn >> 6) & 0xF](decoded);
}

uint32_t (*executeJumpTable47[2])(decode_result const *) = {
    pop,       /* (2F0 - 2F7) */
    breakpoint /* (2F8 - 2FB) */
};

uint32_t entry47(decode_result const *decoded)
{
  return executeJumpTable47[(insn >> 9) & 0x1](decoded);
}

uint32_t entry55(decode_result const *decoded)
{
  if((insn & 0x0300) != 0x0300) {
    return b_c(decoded);
  }

  if(insn == 0xDF01) {
    return exmemwb_exit_simulation(decoded);
  }

  return exmemwb_error(decoded);
}

uint32_t (*executeJumpTable[64])(decode_result const *) = {lsls_i, lsls_i, lsrs_i, lsrs_i, asrs_i,
    asrs_i, entry6,                                                            /* 6 */
    entry7,                                                                    /* 7 */
    movs_i, movs_i, cmp_i, cmp_i, adds_i8, adds_i8, subs_i8, subs_i8, entry16, /* 16 */
    entry17,                                                                   /* 17 */
    ldr_lit, ldr_lit, entry20,                                                 /* 20 */
    entry21,                                                                   /* 21 */
    entry22,                                                                   /* 22 */
    entry23,                                                                   /* 23 */
    str_i, str_i, ldr_i, ldr_i, strb_i, strb_i, ldrb_i, ldrb_i, strh_i, strh_i, ldrh_i, ldrh_i,
    str_sp, str_sp, ldr_sp, ldr_sp, adr, adr, add_sp, add_sp, entry44, /* 44 */
    push, entry46,                                                     /* 46 */
    entry47,                                                           /* 47 */
    stm, stm, ldm, ldm, b_c, b_c, b_c, entry55,                        /* 55 */
    b, b, exmemwb_error, exmemwb_error, bl,                            /* 60 ignore mrs */
    bl,                                                                /* 61 ignore udef */
    exmemwb_error, exmemwb_error};

uint32_t exmemwb(uint16_t instruction, decode_result const *decoded)
{
  insn = instruction;
  // fprintf(stdout, "%x\n", insn);

  uint32_t insnTicks = executeJumpTable[instruction >> 10](decoded);

  // Update the SYSTICK unit and look for resets
  if(SYSTICK.control & 0x1) {
    if(insnTicks >= SYSTICK.value) {
      // Ignore resets due to reads
      if(SYSTICK.value > 0)
        SYSTICK.control |= 0x00010000;

      SYSTICK.value = SYSTICK.reload - insnTicks + SYSTICK.value;
    } else
      SYSTICK.value -= insnTicks;
  }

  return insnTicks;
}

uint32_t exmemwb_mock(uint16_t instruction, decode_result const *decoded, bool& mem_write, bool& mem_op, bool& branch, bool& branch_link, uint32_t& numMemAccess)
{
  insn = instruction >> 10;
  //fprintf(stdout, "%x\n", insn);

  uint32_t address = 0xFFFFFFFF; 

  if(insn == 18 || insn == 19 || (insn > 23 && insn < 40) || (insn > 47 && insn < 52)) {
    address = executeJumpTable[instruction >> 10](decoded);
    mem_op = true;
    numMemAccess = 1;

    auto count = 0; 
    if(insn > 47 && insn < 52) {
      for(int i = 0; i < 8; ++i) {
        int mask = 1 << i;
        if(decoded->register_list & mask) {
          if(count == 0 || count == 1 || count == 5) {            
            numMemAccess++;
          }
          count++;
        }
      }
    }
  }

  switch(insn) {
    case 24:
    case 25:
    case 28:
    case 29:
    case 32:
    case 33:
    case 36:
    case 37:
    case 48:
    case 49: mem_write = true; break;
    case 52:
    case 53:
    case 54:
    case 56:
    case 57: branch = true; break;
    case 60:
    case 61: branch_link = true; break;
    default: break;
  }

  return address;
}

}
