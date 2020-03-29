#include "thumbulator/memory.hpp"

#include "cpu_flags.hpp"
#include "exit.hpp"
#include "trace.hpp"

namespace thumbulator {

///--- Load/store multiple operations --------------------------------------------///

// LDM - Load multiple registers from the stack
uint32_t ldm(decode_result const *decoded)
{
  if(!thumbulator::mock_exmemwb)
    TRACE_INSTRUCTION("ldm r%u!, {0x%X}\n", decoded->Rn, decoded->register_list);

  uint32_t numLoaded = 0;
  uint32_t rNWritten = (1 << decoded->Rn) & decoded->register_list;
  uint32_t address = cpu_get_gpr(decoded->Rn);

  if(thumbulator::mock_exmemwb)
    return address;

  for(int i = 0; i < 8; ++i) {
    int mask = 1 << i;
    if(decoded->register_list & mask) {
      uint32_t data = 0;
      auto hit = load(address, &data, 0);
      cpu_set_gpr(i, data);
      address += 4;
      if(dcache) {
	if(hit) {
	  numLoaded = 1;
	}
	else {	
          numLoaded += ((dcache->get_block_size() >> 2) + 1);
	}
      }
      else {
        ++numLoaded;
      }
    }
  }

  if(rNWritten == 0)
    cpu_set_gpr(decoded->Rn, address);

  if(dcache) {
    return numLoaded;
  }

  return 1 + numLoaded;
}

// STM - Store multiple registers to the stack
uint32_t stm(decode_result const *decoded)
{
  if(!thumbulator::mock_exmemwb)
    TRACE_INSTRUCTION("stm r%u!, {0x%X}\n", decoded->Rn, decoded->register_list);

  uint32_t numStored = 0;
  uint32_t address = cpu_get_gpr(decoded->Rn);

  if(thumbulator::mock_exmemwb)
    return address;

  for(int i = 0; i < 8; ++i) {
    int mask = 1 << i;
    if(decoded->register_list & mask) {
      if(i == decoded->Rn && numStored == 0) {
        fprintf(stderr, "Error: Malformed instruction!\n");
        terminate_simulation(1);
      }

      uint32_t data = cpu_get_gpr(i);
      auto hit = store(address, data);
      address += 4;
      if(dcache) {
	if(hit) {
	  numStored = 1;
	}
	else {	
          numStored += ((dcache->get_block_size() >> 2) + 1);
	}
      }
      else {
        ++numStored;
      }
    }
  }

  cpu_set_gpr(decoded->Rn, address);

  if(dcache) {
    return numStored;
  }

  return 1 + numStored;
}

///--- Stack operations --------------------------------------------///

// Pop multiple reg values from the stack and update SP
uint32_t pop(decode_result const *decoded)
{
  if(!thumbulator::mock_exmemwb)
    TRACE_INSTRUCTION("pop {0x%X}\n", decoded->register_list);

  uint32_t numLoaded = 0;
  uint32_t address = cpu_get_sp();

  if(thumbulator::mock_exmemwb)
    return address;

  for(int i = 0; i < 16; ++i) {
    int mask = 1 << i;
    if(decoded->register_list & mask) {
      uint32_t data = 0;
      auto hit = load(address, &data, 0);
      cpu_set_gpr(i, data);
      // fprintf(stdout, "r%d<-0x%x\n", i, data);

      if(dcache) {
	if(hit) {
	  numLoaded = 1;
	}
	else {	
          numLoaded += ((dcache->get_block_size() >> 2) + 1);
	}
      }
      else {
        ++numLoaded;
      }
      if(i == 15)
        BRANCH_WAS_TAKEN = 1;
      address += 4;
    }

    // Skip constant 0s
    if(i == 7)
      i = 14;
  }

  cpu_set_sp(address);

  if(dcache) {
    return numLoaded + BRANCH_WAS_TAKEN ? TIMING_PC_UPDATE : 0;
  }

  return 1 + numLoaded + BRANCH_WAS_TAKEN ? TIMING_PC_UPDATE : 0;
}

// Push multiple reg values to the stack and update SP
uint32_t push(decode_result const *decoded)
{
  if(!thumbulator::mock_exmemwb)
    TRACE_INSTRUCTION("push {0x%4.4X}\n", decoded->register_list);

  uint32_t numStored = 0;
  uint32_t address = cpu_get_sp();

  if(thumbulator::mock_exmemwb)
    return address;

  for(int i = 14; i >= 0; --i) {
    int mask = 1 << i;
    if(decoded->register_list & mask) {
      address -= 4;
      uint32_t data = cpu_get_gpr(i);
      // fprintf(stdout, "r%d->0x%x\n", i, data);
      auto hit = store(address, data);
      if(dcache) {
	if(hit) {
	  numStored = 1;
	}
	else {	
          numStored += ((dcache->get_block_size() >> 2) + 1);
	}
      }
      else {
        ++numStored;
      }
    }

    // Skip constant 0s
    if(i == 14)
      i = 8;
  }

  cpu_set_sp(address);

  if(dcache) {
    return numStored;
  }

  return 1 + numStored;
}

///--- Single load operations --------------------------------------------///

// LDR - Load from offset from register
uint32_t ldr_i(decode_result const *decoded)
{
  if(!thumbulator::mock_exmemwb)
    TRACE_INSTRUCTION("ldr r%u, [r%u, #0x%X]\n", decoded->Rd, decoded->Rn, decoded->imm << 2);

  uint32_t numLoaded = 0;
  uint32_t base = cpu_get_gpr(decoded->Rn);
  uint32_t offset = zeroExtend32(decoded->imm << 2);
  uint32_t effectiveAddress = base + offset;

  if(thumbulator::mock_exmemwb)
    return effectiveAddress;

  uint32_t result = 0;
  auto hit = load(effectiveAddress, &result, 0);
  if(dcache) {
    if(hit) {
      numLoaded = 1;
    }
    else {	
      numLoaded += ((dcache->get_block_size() >> 2) + 1);
    }
  }
  else {
    ++numLoaded;
  }

  cpu_set_gpr(decoded->Rd, result);

  if(dcache) {
    return numLoaded;
  }

  return 1 + numLoaded;
}

// LDR - Load from offset from SP
uint32_t ldr_sp(decode_result const *decoded)
{
  if(!thumbulator::mock_exmemwb)
    TRACE_INSTRUCTION("ldr r%u, [SP, #0x%X]\n", decoded->Rd, decoded->imm << 2);

  uint32_t numLoaded = 0;
  uint32_t base = cpu_get_sp();
  uint32_t offset = zeroExtend32(decoded->imm << 2);
  uint32_t effectiveAddress = base + offset;

  if(thumbulator::mock_exmemwb)
    return effectiveAddress;

  uint32_t result = 0;
  auto hit = load(effectiveAddress, &result, 0);

  if(dcache) {
    if(hit) {
      numLoaded = 1;
    }
    else {	
      numLoaded += ((dcache->get_block_size() >> 2) + 1);
    }
  }
  else {
    ++numLoaded;
  }

  cpu_set_gpr(decoded->Rd, result);

  // fprintf(stdout, "result=0x%8.8X base=0x%8.8X offset=0x%8.8X\n", result, base, offset);

  if(dcache) {
    return numLoaded;
  }

  return 1 + numLoaded;
}

// LDR - Load from offset from PC
uint32_t ldr_lit(decode_result const *decoded)
{
  if(!thumbulator::mock_exmemwb)
    TRACE_INSTRUCTION("ldr r%u, [PC, #%d]\n", decoded->Rd, decoded->imm << 2);

  uint32_t numLoaded = 0;
  uint32_t base = cpu_get_pc() & 0xFFFFFFFC;
  uint32_t offset = zeroExtend32(decoded->imm << 2);
  uint32_t effectiveAddress = base + offset;

  if(thumbulator::mock_exmemwb)
    return effectiveAddress;

  uint32_t result = 0;
  auto hit = load(effectiveAddress, &result, 0);

  if(dcache) {
    if(hit) {
      numLoaded = 1;
    }
    else {	
      numLoaded += ((dcache->get_block_size() >> 2) + 1);
    }
  }
  else {
    ++numLoaded;
  }

  cpu_set_gpr(decoded->Rd, result);

  // fprintf(stdout, "r%d=0x%x\n", decoded->Rd, result);

  if(dcache) {
    return numLoaded;
  }

  return 1 + numLoaded;
}

// LDR - Load from an offset from a reg based on another reg value
uint32_t ldr_r(decode_result const *decoded)
{
  if(!thumbulator::mock_exmemwb)
    TRACE_INSTRUCTION("ldr r%u, [r%u, r%u]\n", decoded->Rd, decoded->Rn, decoded->Rm);

  uint32_t numLoaded = 0;
  uint32_t base = cpu_get_gpr(decoded->Rn);
  uint32_t offset = cpu_get_gpr(decoded->Rm);
  uint32_t effectiveAddress = base + offset;

  if(thumbulator::mock_exmemwb)
    return effectiveAddress;

  uint32_t result = 0;
  auto hit = load(effectiveAddress, &result, 0);

  if(dcache) {
    if(hit) {
      numLoaded = 1;
    }
    else {	
      numLoaded += ((dcache->get_block_size() >> 2) + 1);
    }
  }
  else {
    ++numLoaded;
  }

  cpu_set_gpr(decoded->Rd, result);

  if(dcache) {
    return numLoaded;
  }

  return 1 + numLoaded;
}

// LDRB - Load byte from offset from register
uint32_t ldrb_i(decode_result const *decoded)
{
  if(!thumbulator::mock_exmemwb)
    TRACE_INSTRUCTION("ldrb r%u, [r%u, #0x%X]\n", decoded->Rd, decoded->Rn, decoded->imm);

  uint32_t numLoaded = 0;
  uint32_t base = cpu_get_gpr(decoded->Rn);
  uint32_t offset = zeroExtend32(decoded->imm);
  uint32_t effectiveAddress = base + offset;
  uint32_t effectiveAddressWordAligned = effectiveAddress & ~0x3;

  if(thumbulator::mock_exmemwb)
    return effectiveAddressWordAligned;

  uint32_t result = 0;
  auto hit = load(effectiveAddressWordAligned, &result, 0);

  if(dcache) {
    if(hit) {
      numLoaded = 1;
    }
    else {	
      numLoaded += ((dcache->get_block_size() >> 2) + 1);
    }
  }
  else {
    ++numLoaded;
  }

  // Select the correct byte
  switch(effectiveAddress & 0x3) {
  case 0:
    break;
  case 1:
    result >>= 8;
    break;
  case 2:
    result >>= 16;
    break;
  case 3:
    result >>= 24;
  }

  result = zeroExtend32(result & 0xFF);

  cpu_set_gpr(decoded->Rd, result);

  if(dcache) {
    return numLoaded;
  }

  return 1 + numLoaded;
}

// LDRB - Load byte from an offset from a reg based on another reg value
uint32_t ldrb_r(decode_result const *decoded)
{
  if(!thumbulator::mock_exmemwb)
    TRACE_INSTRUCTION("ldrb r%u, [r%u, r%u]\n", decoded->Rd, decoded->Rn, decoded->Rm);

  uint32_t numLoaded = 0;
  uint32_t base = cpu_get_gpr(decoded->Rn);
  uint32_t offset = cpu_get_gpr(decoded->Rm);
  uint32_t effectiveAddress = base + offset;
  uint32_t effectiveAddressWordAligned = effectiveAddress & ~0x3;

  if(thumbulator::mock_exmemwb)
    return effectiveAddressWordAligned;

  uint32_t result = 0;
  auto hit = load(effectiveAddressWordAligned, &result, 0);

  if(dcache) {
    if(hit) {
      numLoaded = 1;
    }
    else {	
      numLoaded += ((dcache->get_block_size() >> 2) + 1);
    }
  }
  else {
    ++numLoaded;
  }

  // Select the correct byte
  switch(effectiveAddress & 0x3) {
  case 0:
    break;
  case 1:
    result >>= 8;
    break;
  case 2:
    result >>= 16;
    break;
  case 3:
    result >>= 24;
  }

  result = zeroExtend32(result & 0xFF);

  cpu_set_gpr(decoded->Rd, result);

  if(dcache) {
    return numLoaded;
  }

  return 1 + numLoaded;
}

// LDRH - Load halfword from offset from register
uint32_t ldrh_i(decode_result const *decoded)
{
  if(!thumbulator::mock_exmemwb)
    TRACE_INSTRUCTION("ldrh r%u, [r%u, #0x%X]\n", decoded->Rd, decoded->Rn, decoded->imm);

  uint32_t numLoaded = 0;
  uint32_t base = cpu_get_gpr(decoded->Rn);
  uint32_t offset = zeroExtend32(decoded->imm << 1);
  // fprintf(stdout, "base=0x%8.8X offset=0x%8.8X\n", base, offset);
  uint32_t effectiveAddress = base + offset;
  uint32_t effectiveAddressWordAligned = effectiveAddress & ~0x3;

  if(thumbulator::mock_exmemwb)
    return effectiveAddressWordAligned;

  uint32_t result = 0;
  auto hit = load(effectiveAddressWordAligned, &result, 0);

  if(dcache) {
    if(hit) {
      numLoaded = 1;
    }
    else {	
      numLoaded += ((dcache->get_block_size() >> 2) + 1);
    }
  }
  else {
    ++numLoaded;
  }

  // Select the correct halfword
  switch(effectiveAddress & 0x2) {
  case 0:
    break;
  default:
    result >>= 16;
    break;
  }

  result = zeroExtend32(result & 0xFFFF);

  cpu_set_gpr(decoded->Rd, result);

  if(dcache) {
    return numLoaded;
  }

  return 1 + numLoaded;
}

// LDRH - Load halfword from an offset from a reg based on another reg value
uint32_t ldrh_r(decode_result const *decoded)
{
  if(!thumbulator::mock_exmemwb)
    TRACE_INSTRUCTION("ldrh r%u, [r%u, r%u]\n", decoded->Rd, decoded->Rn, decoded->Rm);

  uint32_t numLoaded = 0;
  uint32_t base = cpu_get_gpr(decoded->Rn);
  uint32_t offset = cpu_get_gpr(decoded->Rm);
  uint32_t effectiveAddress = base + offset;
  uint32_t effectiveAddressWordAligned = effectiveAddress & ~0x3;

  if(thumbulator::mock_exmemwb)
    return effectiveAddressWordAligned;

  uint32_t result = 0;
  auto hit = load(effectiveAddressWordAligned, &result, 0);

  if(dcache) {
    if(hit) {
      numLoaded = 1;
    }
    else {	
      numLoaded += ((dcache->get_block_size() >> 2) + 1);
    }
  }
  else {
    ++numLoaded;
  }

  // Select the correct halfword
  switch(effectiveAddress & 0x2) {
  case 0:
    break;
  default:
    result >>= 16;
    break;
  }

  result = zeroExtend32(result & 0xFFFF);

  cpu_set_gpr(decoded->Rd, result);

  if(dcache) {
    return numLoaded;
  }

  return 1 + numLoaded;
}

// LDRSB - Load signed byte from an offset from a reg based on another reg value
uint32_t ldrsb_r(decode_result const *decoded)
{
  if(!thumbulator::mock_exmemwb)
    TRACE_INSTRUCTION("ldrsb r%u, [r%u, r%u]\n", decoded->Rd, decoded->Rn, decoded->Rm);

  uint32_t numLoaded = 0;
  uint32_t base = cpu_get_gpr(decoded->Rn);
  uint32_t offset = cpu_get_gpr(decoded->Rm);
  uint32_t effectiveAddress = base + offset;
  uint32_t effectiveAddressWordAligned = effectiveAddress & ~0x3;

  if(thumbulator::mock_exmemwb)
    return effectiveAddressWordAligned;

  uint32_t result = 0;
  auto hit = load(effectiveAddressWordAligned, &result, 0);

  if(dcache) {
    if(hit) {
      numLoaded = 1;
    }
    else {	
      numLoaded += ((dcache->get_block_size() >> 2) + 1);
    }
  }
  else {
    ++numLoaded;
  }

  // Select the correct byte
  switch(effectiveAddress & 0x3) {
  case 0:
    break;
  case 1:
    result >>= 8;
    break;
  case 2:
    result >>= 16;
    break;
  case 3:
    result >>= 24;
  }

  result = signExtend32(result & 0xFF, 8);

  cpu_set_gpr(decoded->Rd, result);

  if(dcache) {
    return numLoaded;
  }

  return 1 + numLoaded;
}

// LDRSH - Load signed halfword from an offset from a reg based on another reg value
uint32_t ldrsh_r(decode_result const *decoded)
{
  if(!thumbulator::mock_exmemwb)
    TRACE_INSTRUCTION("ldrsh r%u, [r%u, r%u]\n", decoded->Rd, decoded->Rn, decoded->Rm);

  uint32_t numLoaded = 0;
  uint32_t base = cpu_get_gpr(decoded->Rn);
  uint32_t offset = cpu_get_gpr(decoded->Rm);
  uint32_t effectiveAddress = base + offset;
  uint32_t effectiveAddressWordAligned = effectiveAddress & ~0x3;

  if(thumbulator::mock_exmemwb)
    return effectiveAddressWordAligned;

  uint32_t result = 0;
  auto hit = load(effectiveAddressWordAligned, &result, 0);

  if(dcache) {
    if(hit) {
      numLoaded = 1;
    }
    else {	
      numLoaded += ((dcache->get_block_size() >> 2) + 1);
    }
  }
  else {
    ++numLoaded;
  }

  // Select the correct halfword
  switch(effectiveAddress & 0x2) {
  case 0:
    break;
  default:
    result >>= 16;
  }

  result = signExtend32(result & 0xFFFF, 16);

  cpu_set_gpr(decoded->Rd, result);

  if(dcache) {
    return numLoaded;
  }

  return 1 + numLoaded;
}

///--- Single store operations --------------------------------------------///

// STR - Store to offset from register
uint32_t str_i(decode_result const *decoded)
{
  if(!thumbulator::mock_exmemwb)
    TRACE_INSTRUCTION("str r%u, [r%u, #%d]\n", decoded->Rd, decoded->Rn, decoded->imm << 2);

  uint32_t numStored = 0;
  uint32_t base = cpu_get_gpr(decoded->Rn);
  uint32_t offset = zeroExtend32(decoded->imm << 2);
  uint32_t effectiveAddress = base + offset;

  if(thumbulator::mock_exmemwb)
    return effectiveAddress;

  auto hit = store(effectiveAddress, cpu_get_gpr(decoded->Rd));

  if(dcache) {
    if(hit) {
      numStored = 1;
    }
    else {	
      numStored += ((dcache->get_block_size() >> 2) + 1);
    }
  }
  else {
    ++numStored;
  }

  return numStored;
}

// STR - Store to offset from SP
uint32_t str_sp(decode_result const *decoded)
{
  if(!thumbulator::mock_exmemwb)
    TRACE_INSTRUCTION("str r%u, [SP, #%d]\n", decoded->Rd, decoded->imm << 2);

  uint32_t numStored = 0;
  uint32_t base = cpu_get_sp();
  uint32_t offset = zeroExtend32(decoded->imm << 2);
  uint32_t effectiveAddress = base + offset;

  if(thumbulator::mock_exmemwb)
    return effectiveAddress;

  auto hit = store(effectiveAddress, cpu_get_gpr(decoded->Rd));

  if(dcache) {
    if(hit) {
      numStored = 1;
    }
    else {	
      numStored += ((dcache->get_block_size() >> 2) + 1);
    }
  }
  else {
    ++numStored;
  }

  if(dcache) {
    return numStored;
  }

  return numStored;
}

// STR - Store to an offset from a reg based on another reg value
uint32_t str_r(decode_result const *decoded)
{
  if(!thumbulator::mock_exmemwb)
    TRACE_INSTRUCTION("str r%u, [r%u, r%u]\n", decoded->Rd, decoded->Rn, decoded->Rm);

  uint32_t numStored = 0;
  uint32_t base = cpu_get_gpr(decoded->Rn);
  uint32_t offset = cpu_get_gpr(decoded->Rm);
  uint32_t effectiveAddress = base + offset;

  if(thumbulator::mock_exmemwb)
    return effectiveAddress;

  auto hit = store(effectiveAddress, cpu_get_gpr(decoded->Rd));

  if(dcache) {
    if(hit) {
      numStored = 1;
    }
    else {	
      numStored += ((dcache->get_block_size() >> 2) + 1);
    }
  }
  else {
    ++numStored;
  }

  if(dcache) {
    return numStored;
  }

  return numStored;
}

// STRB - Store byte to offset from register
uint32_t strb_i(decode_result const *decoded)
{
  if(!thumbulator::mock_exmemwb)
    TRACE_INSTRUCTION("strb r%u, [r%u, #0x%X]\n", decoded->Rd, decoded->Rn, decoded->imm);

  uint32_t numStored = 0;
  uint32_t base = cpu_get_gpr(decoded->Rn);
  uint32_t offset = zeroExtend32(decoded->imm);
  uint32_t effectiveAddress = base + offset;
  uint32_t effectiveAddressWordAligned = effectiveAddress & ~0x3;
  uint32_t data = cpu_get_gpr(decoded->Rd) & 0xFF;

  if(thumbulator::mock_exmemwb)
    return effectiveAddressWordAligned;

  uint32_t orig;
  load(effectiveAddressWordAligned, &orig, 1);

  // Select the correct byte
  switch(effectiveAddress & 0x3) {
  case 0:
    orig = (orig & 0xFFFFFF00) | (data << 0);
    break;
  case 1:
    orig = (orig & 0xFFFF00FF) | (data << 8);
    break;
  case 2:
    orig = (orig & 0xFF00FFFF) | (data << 16);
    break;
  case 3:
    orig = (orig & 0x00FFFFFF) | (data << 24);
  }

  auto hit = store(effectiveAddressWordAligned, orig);

  if(dcache) {
    if(hit) {
      numStored = 1;
    }
    else {	
      numStored += ((dcache->get_block_size() >> 2) + 1);
    }
  }
  else {
    ++numStored;
  }

  if(dcache) {
    return numStored;
  }

  return 1 + numStored;
}

// STRB - Store byte to an offset from a reg based on another reg value
uint32_t strb_r(decode_result const *decoded)
{
  if(!thumbulator::mock_exmemwb)
    TRACE_INSTRUCTION("strb r%u, [r%u, r%u]\n", decoded->Rd, decoded->Rn, decoded->Rm);

  uint32_t numStored = 0;
  uint32_t base = cpu_get_gpr(decoded->Rn);
  uint32_t offset = cpu_get_gpr(decoded->Rm);
  uint32_t effectiveAddress = base + offset;
  uint32_t effectiveAddressWordAligned = effectiveAddress & ~0x3;
  uint32_t data = cpu_get_gpr(decoded->Rd) & 0xFF;

  if(thumbulator::mock_exmemwb)
    return effectiveAddressWordAligned;

  uint32_t orig;
  load(effectiveAddressWordAligned, &orig, 1);

  // Select the correct byte
  switch(effectiveAddress & 0x3) {
  case 0:
    orig = (orig & 0xFFFFFF00) | (data << 0);
    break;
  case 1:
    orig = (orig & 0xFFFF00FF) | (data << 8);
    break;
  case 2:
    orig = (orig & 0xFF00FFFF) | (data << 16);
    break;
  case 3:
    orig = (orig & 0x00FFFFFF) | (data << 24);
  }

  auto hit = store(effectiveAddressWordAligned, orig);

  if(dcache) {
    if(hit) {
      numStored = 1;
    }
    else {	
      numStored += ((dcache->get_block_size() >> 2) + 1);
    }
  }
  else {
    ++numStored;
  }

  if(dcache) {
    return numStored;
  }

  return 1 + numStored;
}

// STRH - Store halfword to offset from register
uint32_t strh_i(decode_result const *decoded)
{
  if(!thumbulator::mock_exmemwb)
    TRACE_INSTRUCTION("strh r%u, [r%u, #0x%X]\n", decoded->Rd, decoded->Rn, decoded->imm);

  uint32_t numStored = 0;
  uint32_t base = cpu_get_gpr(decoded->Rn);
  uint32_t offset = zeroExtend32(decoded->imm << 1);
  uint32_t effectiveAddress = base + offset;
  uint32_t effectiveAddressWordAligned = effectiveAddress & ~0x3;
  uint32_t data = cpu_get_gpr(decoded->Rd) & 0xFFFF;

  if(thumbulator::mock_exmemwb)
    return effectiveAddressWordAligned;

  uint32_t orig;
  load(effectiveAddressWordAligned, &orig, 1);

  // Select the correct byte
  switch(effectiveAddress & 0x2) {
  case 0:
    orig = (orig & 0xFFFF0000) | (data << 0);
    break;
  default:
    orig = (orig & 0x0000FFFF) | (data << 16);
  }

  auto hit = store(effectiveAddressWordAligned, orig);

  if(dcache) {
    if(hit) {
      numStored = 1;
    }
    else {	
      numStored += ((dcache->get_block_size() >> 2) + 1);
    }
  }
  else {
    ++numStored;
  }

  if(dcache) {
    return numStored;
  }

  return 1 + numStored;
}

// STRH - Store halfword to an offset from a reg based on another reg value
uint32_t strh_r(decode_result const *decoded)
{
  if(!thumbulator::mock_exmemwb)
    TRACE_INSTRUCTION("strh r%u, [r%u, r%u]\n", decoded->Rd, decoded->Rn, decoded->Rm);

  uint32_t numStored = 0;
  uint32_t base = cpu_get_gpr(decoded->Rn);
  uint32_t offset = cpu_get_gpr(decoded->Rm);
  uint32_t effectiveAddress = base + offset;
  uint32_t effectiveAddressWordAligned = effectiveAddress & ~0x3;
  uint32_t data = cpu_get_gpr(decoded->Rd) & 0xFFFF;

  if(thumbulator::mock_exmemwb)
    return effectiveAddressWordAligned;

  uint32_t orig;
  load(effectiveAddressWordAligned, &orig, 1);

  // Select the correct byte
  switch(effectiveAddress & 0x2) {
  case 0:
    orig = (orig & 0xFFFF0000) | (data << 0);
    break;
  default:
    orig = (orig & 0x0000FFFF) | (data << 16);
  }

  auto hit = store(effectiveAddressWordAligned, orig);

  if(dcache) {
    if(hit) {
      numStored = 1;
    }
    else {	
      numStored += ((dcache->get_block_size() >> 2) + 1);
    }
  }
  else {
    ++numStored;
  }

  if(dcache) {
    return numStored;
  }

  return 1 + numStored;
}
}
