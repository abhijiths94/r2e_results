#include "thumbulator/memory.hpp"

#include <cstdio>

#include "cpu_flags.hpp"
#include "exit.hpp"

namespace thumbulator {

std::shared_ptr<cache> dcache = nullptr;
std::shared_ptr<cache> icache = nullptr;
std::shared_ptr<rename> renamer = nullptr;

uint32_t RAM[RAM_SIZE_BYTES >> 2];

std::function<uint32_t(uint32_t, uint32_t)> ram_load_hook;
std::function<uint32_t(uint32_t, uint32_t, uint32_t, bool)> ram_store_hook;

std::function<bool(cache_block&, uint32_t, bool, size_t, size_t)> cache_load_hook;
std::function<bool(cache_block&, uint32_t, bool, size_t, size_t, bool&)> cache_store_hook;

uint32_t FLASH_MEMORY[FLASH_SIZE_BYTES >> 2];

bool icache_hit = false;
bool dcache_hit = false;

uint32_t ram_load(uint32_t address, bool false_read)
{
  auto data = RAM[(address & RAM_ADDRESS_MASK) >> 2];

  if(!false_read && ram_load_hook != nullptr) {
    data = ram_load_hook(address, data);
  }

  // fprintf(stdout, "In ram_load: address=0x%8.8x data=0x%x\n", address, data);
  return data;
}

void ram_store(uint32_t address, uint32_t value, bool backup)
{
  if(ram_store_hook != nullptr) {
    auto const old_value = ram_load(address, true);

    value = ram_store_hook(address, old_value, value, backup);
  }

  // fprintf(stdout, "In ram_store: value=0x%x\n", value);

  RAM[(address & RAM_ADDRESS_MASK) >> 2] = value;
}

uint32_t load_from_memory(uint32_t address, uint32_t false_read)
{
  if((address & (~0x3f)) == 0xE0000000) {
    return 0;
  }
  // fprintf(stdout, "In load_from_memory: addr=0x%8.8x\n", address);
  if(address >= RAM_START) {
    if(address >= (RAM_START + RAM_SIZE_BYTES)) {
      // Check for UART
      if(address == 0xE0000000) {
        return 0;
      }

      // Check for SYSTICK
      if((address >> 4) == 0xE000E01) {
        auto value = ((uint32_t *)&SYSTICK)[(address >> 2) & 0x3];
        if(address == 0xE000E010)
          SYSTICK.control &= 0x00010000;

        return value;
      }

      fprintf(
          stderr, "Error: DLR Memory access out of range: 0x%8.8X, pc=%x\n", address, cpu_get_pc());
      terminate_simulation(1);
    }

    // fprintf(stdout, "RAM load\n");
    return ram_load(address, false_read == 1);
  } else {
    if(address >= (FLASH_START + FLASH_SIZE_BYTES)) {
      fprintf(
          stderr, "Error: DLF Memory access out of range: 0x%8.8X, pc=%x\n", address, cpu_get_pc());
      terminate_simulation(1);
    }

    // fprintf(stdout, "FLASH load\n");
    return FLASH_MEMORY[(address & FLASH_ADDRESS_MASK) >> 2];
  }
}

void store_in_memory(uint32_t address, uint32_t value, bool backup)
{
  if((address & (~0x3f)) == 0xE0000000) {
    return;
  }

  // fprintf(stdout, "In store_in_memory: addr=0x%8.8x value=0x%x\n", address, value);
  if(address >= RAM_START) {
    if(address >= (RAM_START + RAM_SIZE_BYTES)) {
      // Check for UART
      if(address == 0xE0000000) {
        return;
      }

      // Check for SYSTICK
      if((address >> 4) == 0xE000E01 && address != 0xE000E01C) {
        if(address == 0xE000E010) {
          SYSTICK.control = (value & 0x1FFFD) | 0x4; // No external tick source, no interrupt

          if(value & 0x2) {
            fprintf(stderr, "Warning: SYSTICK interrupts not implemented, ignoring\n");
          }
        } else if(address == 0xE000E014) {
          SYSTICK.reload = value & 0xFFFFFF;
        } else if(address == 0xE000E018) {
          // Reads clears current value
          SYSTICK.value = 0;
        }

        return;
      }

      fprintf(
          stderr, "Error: DSR Memory access out of range: 0x%8.8X, pc=%x\n", address, cpu_get_pc());
      terminate_simulation(1);
    }

    // fprintf(stdout, "RAM store\n");
    ram_store(address, value, backup);
  } else {
    if(address >= (FLASH_START + FLASH_SIZE_BYTES)) {
      fprintf(
          stderr, "Error: DSF Memory access out of range: 0x%8.8X, pc=%x\n", address, cpu_get_pc());
      terminate_simulation(1);
    }

    // fprintf(stdout, "FLASH store\n");
    FLASH_MEMORY[(address & FLASH_ADDRESS_MASK) >> 2] = value;
  }
}

uint32_t cache_load(uint32_t address, bool false_read)
{
  // fprintf(stdout, "In cache_load: addr=0x%8.8x\n", address);
  auto word_offset  = (address & dcache->get_block_mask()) >> 2; 
  auto load_addr    = address & (~dcache->get_block_mask());
  auto mt_tag       = address >> (dcache->get_block_offset()); // map table tag

  cache_attr attr;
  dcache_hit = dcache->is_hit(load_addr, attr);

  if(dcache_hit) {
    auto blk = dcache->cache_read(attr, false_read);
    if(cache_load_hook != nullptr) {
      cache_load_hook(blk, load_addr, true, attr.set, attr.way);
    }
    if(dcache->get_state(attr.set, attr.way, word_offset) == hmap::cUnknown) {
      dcache->set_state(attr.set, attr.way, word_offset, hmap::cReadFirst);
    }

    return dcache->get_data(attr.set, attr.way, word_offset);
  }
  else {
    auto victim = dcache->get_victim(attr);
    auto lbf = false;
    if(victim.get_valid()) { 
      lbf = dcache->get_block_state(attr.set, attr.way);
    }

    if(cache_load_hook != nullptr) {
      auto actual_address = victim.get_address();
      cache_load_hook(victim, load_addr, lbf, attr.set, attr.way);

      // fprintf(stdout, "cache_load: [victim] v=%d, d=%d, wf=%d, set=%zu, way=%zu, tag=0x%x, actual_address=0x%8.8x, renamed_address=0x%8.8x\n",
        // victim.get_valid(), victim.get_dirty(), victim.get_wf(), attr.set, attr.way, victim.get_tag(), actual_address, victim.get_address());

      if(dcache->get_block(attr.set, attr.way).get_valid() && dcache->get_block(attr.set, attr.way).get_dirty()) {
        for(uint32_t beat=0; beat<(dcache->get_block_size() >> 2); beat++) {
          // fprintf(stdout, "actual_address=0x%8.8x renamed_address=0x%8.8x data=0x%x\n", 
          //   (actual_address + (beat << 2)), (victim.get_address() + (beat << 2)), dcache->get_data(attr.set, attr.way, beat));
          store_in_memory(victim.get_address() + (beat << 2), dcache->get_data(attr.set, attr.way, beat), false);
        }
      }
    }

    cache_block blk;
    blk.set_valid(1);
    blk.set_tag(load_addr >> (dcache->get_set_offset() + dcache->get_block_offset()));
    blk.set_address(load_addr);

    auto actual_address = load_addr;

    if(renamer) {
      uint32_t index = 0;
      if(renamer->lookup_map_table(mt_tag, index)) {
        // fprintf(stdout, "cache_load: tag=0x%8.8x index=%d\n", mt_tag, index);
        load_addr = renamer->read_map_table(index);
      }
    }

    dcache->clear_state(attr.set, attr.way);

    for(uint32_t beat=0; beat<(dcache->get_block_size() >> 2); beat++) {
      auto data  = load_from_memory(load_addr + (beat << 2), false_read);
      // fprintf(stdout, "cache_load(load): actual_address=0x%8.8x renamed_address=0x%8.8x data=0x%x\n", actual_address + (beat << 2), load_addr + (beat << 2), data);
      dcache->set_data(attr.set, attr.way, beat, data);
      dcache->set_state(attr.set, attr.way, beat, hmap::cReadFirst);
    }
    dcache->cache_insert(attr, blk, false_read);
    return dcache->get_data(attr.set, attr.way, word_offset);
  }
}

void cache_store(uint32_t address, uint32_t value)
{
  // fprintf(stdout, "In cache_store: addr=0x%8.8x value=0x%x\n", address, value);
  auto word_offset  = (address & dcache->get_block_mask()) >> 2; 
  auto store_addr   = address & (~dcache->get_block_mask());
  auto mt_tag       = address >> (dcache->get_block_offset()); // map table tag
  bool start_backup = false;
  auto gbf_hit      = false;

  cache_attr attr;
  dcache_hit = dcache->is_hit(store_addr, attr);

  if(dcache_hit) { 
    auto blk = dcache->cache_read(attr, false);
    if(cache_store_hook != nullptr) {
      cache_store_hook(blk, store_addr, true, attr.set, attr.way , gbf_hit);
    }
    dcache->cache_write(false, attr);
    dcache->set_data(attr.set, attr.way, word_offset, value);
    if(dcache->get_state(attr.set, attr.way, word_offset) == hmap::cUnknown) {
      dcache->set_state(attr.set, attr.way, word_offset, hmap::cWriteFirst);
    }
  }
  else {
    auto victim = dcache->get_victim(attr);
    auto lbf = false;
    if(victim.get_valid()) { 
      lbf = dcache->get_block_state(attr.set, attr.way);
    }
    
    if(cache_store_hook != nullptr) {
      auto actual_address = victim.get_address();
      cache_store_hook(victim, store_addr, lbf, attr.set, attr.way , gbf_hit);

      // fprintf(stdout, "cache_store: [victim] v=%d, d=%d, wf=%d, set=%zu, way=%zu, tag=0x%x, actual_address=0x%8.8x, renamed_address=0x%8.8x\n", 
        // victim.get_valid(), victim.get_dirty(), victim.get_wf(), attr.set, attr.way, victim.get_tag(), actual_address, victim.get_address());

      if(dcache->get_block(attr.set, attr.way).get_valid() && dcache->get_block(attr.set, attr.way).get_dirty()) {
        for(uint32_t beat=0; beat<(dcache->get_block_size() >> 2); beat++) {
          // fprintf(stdout, "actual_address=0x%8.8x renamed_address=0x%8.8x data=0x%x\n",
          //   (actual_address + (beat << 2)), (victim.get_address() + (beat << 2)), dcache->get_data(attr.set, attr.way, beat));
          store_in_memory(victim.get_address() + (beat << 2), dcache->get_data(attr.set, attr.way, beat), false);
        }
      }
    }

    cache_block blk;
    blk.set_valid(1);
    blk.set_dirty(1);
    blk.set_wf(false);
    blk.set_tag(store_addr >> (dcache->get_set_offset() + dcache->get_block_offset()));
    blk.set_address(store_addr);

    auto actual_address = store_addr;

    if(renamer) {
      uint32_t index = 0;
      if(renamer->lookup_map_table(mt_tag, index)) {
        // fprintf(stdout, "cache_store: tag=0x%8.8x index=%d\n", mt_tag, index);
        store_addr = renamer->read_map_table(index);
      }
    }

    dcache->clear_state(attr.set, attr.way);

    for(uint32_t beat=0; beat<(dcache->get_block_size() >> 2); beat++) {
      auto data = load_from_memory(store_addr + (beat << 2), false);
      // fprintf(stdout, "cache_store(load): actual_address=0x%8.8x renamed_address=0x%8.8x data=0x%x\n", actual_address + (beat << 2), store_addr + (beat << 2), data);
      dcache->set_data(attr.set, attr.way, beat, data);
      if(gbf_hit) {
        dcache->set_state(attr.set, attr.way, beat, hmap::cReadFirst);
      }
    }
    dcache->set_data(attr.set, attr.way, word_offset, value);
    if(gbf_hit)
      dcache->set_state(attr.set, attr.way, word_offset, hmap::cReadFirst);
    else
      dcache->set_state(attr.set, attr.way, word_offset, hmap::cWriteFirst);
    dcache->cache_insert(attr, blk, false);
  }
}

// Memory access functions assume that RAM has a higher address than Flash
void fetch_instruction(uint32_t address, uint16_t *value)
{
  // fprintf(stdout, "In fetch_instruction: address=0x%8.8x\n", address);
  uint32_t fromMem;
  auto icache_hit = false;

  if(icache) {
    auto word_offset  = (address & icache->get_block_mask()) >> 2; 
    auto load_addr    = address & (~icache->get_block_mask());

    cache_attr attr;
    icache_hit = icache->is_hit(load_addr, attr);

    if(icache_hit) {
      auto blk = icache->cache_read(attr, false);
      fromMem = icache->get_data(attr.set, attr.way, word_offset);
    }
    else {
      auto victim = icache->get_victim(attr);

      cache_block blk;
      blk.set_valid(1);
      blk.set_tag(load_addr >> (icache->get_set_offset() + icache->get_block_offset()));

      for(uint32_t beat=0; beat<(icache->get_block_size() >> 2); beat++) {
	auto fetch_addr = load_addr + (beat << 2);
        if(fetch_addr >= RAM_START) {
          if(fetch_addr >= (RAM_START + RAM_SIZE_BYTES)) {
            fprintf(
                stderr, "Error: ILR Memory access out of range: 0x%8.8X, pc=%x\n", fetch_addr, cpu_get_pc());
            terminate_simulation(1);
          }

          // fprintf(stdout, "RAM load\n");
          fromMem = ram_load(fetch_addr, false);
        } else {
          if(fetch_addr >= (FLASH_START + FLASH_SIZE_BYTES)) {
            fprintf(
                stderr, "Error: ILF Memory access out of range: 0x%8.8X, pc=%x\n", fetch_addr, cpu_get_pc());
            terminate_simulation(1);
          }

          // fprintf(stdout, "FLASH load\n");
          fromMem = FLASH_MEMORY[(fetch_addr & FLASH_ADDRESS_MASK) >> 2];
        }
        icache->set_data(attr.set, attr.way, beat, fromMem);
      }
      icache->cache_insert(attr, blk, false);
      fromMem = icache->get_data(attr.set, attr.way, word_offset);
    }
  }
  else {
    if(address >= RAM_START) {
      if(address >= (RAM_START + RAM_SIZE_BYTES)) {
        fprintf(
            stderr, "Error: ILR Memory access out of range: 0x%8.8X, pc=%x\n", address, cpu_get_pc());
        terminate_simulation(1);
      }

      // fprintf(stdout, "RAM load\n");
      fromMem = ram_load(address, false);
    } else {
      if(address >= (FLASH_START + FLASH_SIZE_BYTES)) {
        fprintf(
            stderr, "Error: ILF Memory access out of range: 0x%8.8X, pc=%x\n", address, cpu_get_pc());
        terminate_simulation(1);
      }

      // fprintf(stdout, "FLASH load\n");
      fromMem = FLASH_MEMORY[(address & FLASH_ADDRESS_MASK) >> 2];
    }
  }

  // Data 32-bits, but instruction 16-bits
  *value = ((address & 0x2) != 0) ? (uint16_t)(fromMem >> 16) : (uint16_t)fromMem;
}

bool load(uint32_t address, uint32_t *value, uint32_t false_read)
{
  dcache_hit = false;

  if(dcache) {
    if(address >= RAM_START && address < (RAM_START + RAM_SIZE_BYTES)) {
      *value = cache_load(address, false_read);
    }
    else {
      *value = load_from_memory(address, false_read);
    }
  }
  else {
    *value = load_from_memory(address, false_read);
  }

  return dcache_hit;
}

bool store(uint32_t address, uint32_t value, bool backup)
{
  dcache_hit = false;

  if(dcache && !backup) {
    if(address >= RAM_START && address < (RAM_START + RAM_SIZE_BYTES)) {
      cache_store(address, value);
    }
    else {
      store_in_memory(address, value, backup);
    }
  }
  else {
    store_in_memory(address, value, backup);
  }

  return dcache_hit;
}
}
