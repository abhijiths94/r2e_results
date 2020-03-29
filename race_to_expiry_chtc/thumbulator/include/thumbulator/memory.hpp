#ifndef THUMBULATOR_MEMORY_H
#define THUMBULATOR_MEMORY_H

#include <cstdint>
#include <functional>
#include <memory>

#include <thumbulator/cache_block.hpp>
#include <thumbulator/cache.hpp>
#include <thumbulator/rename.hpp>

namespace thumbulator {

#define RAM_START 0x40000000
#define RAM_SIZE_BYTES (1 << 26) // 64 MB
#define RAM_SIZE_ELEMENTS (RAM_SIZE_BYTES >> 2)
#define RAM_ADDRESS_MASK (((~0) << 26) ^ (~0))

extern std::function<bool(cache_block&, uint32_t, bool, size_t, size_t)> cache_load_hook;

extern std::function<bool(cache_block&, uint32_t, bool, size_t, size_t, bool&)> cache_store_hook;

/**
 * Random-Access Memory, like SRAM.
 */
extern uint32_t RAM[RAM_SIZE_ELEMENTS];

/**
 * Hook into loads to RAM.
 *
 * The first parameter is the address.
 * The second parameter is the data that would be loaded.
 *
 * The function returns the data that will be loaded, potentially different than the second parameter.
 */
extern std::function<uint32_t(uint32_t, uint32_t)> ram_load_hook;

/**
 * Hook into stores to RAM.
 *
 * The first parameter is the address.
 * The second parameter is the value at the address before the store.
 * The third parameter is the desired value to store at the address.
 * The fourth parameter distinguishes between a normal store and a backup store
 *
 * The function returns the data that will be stored, potentially different from the third parameter.
 */
extern std::function<uint32_t(uint32_t, uint32_t, uint32_t, bool)> ram_store_hook;

#define FLASH_START 0x0
#define FLASH_SIZE_BYTES (1 << 23) // 8 MB
#define FLASH_SIZE_ELEMENTS (FLASH_SIZE_BYTES >> 2)
#define FLASH_ADDRESS_MASK (((~0) << 23) ^ (~0))

/**
 * Read-Only Memory.
 *
 * Typically used to store the application code.
 */
extern uint32_t FLASH_MEMORY[FLASH_SIZE_ELEMENTS];

/**
 * Fetch an instruction from memory.
 *
 * @param address The address to fetch.
 * @param value The data in memory at that address.
 *
 */

void fetch_instruction(uint32_t address, uint16_t *value);

/**
 * Load data from memory.
 *
 * @param address The address to load data from.
 * @param value The data in memory at that address.
 * @param false_read true if this is a read due to anything other than the program.
 *
 * @return dcache hit/miss
 */
bool load(uint32_t address, uint32_t *value, uint32_t false_read);

/**
 * Store data into memory.
 *
 * @param address The address to store the data to.
 * @param value The data to store at that address.
 * @param normal or backup store. 
 *
 * @return dcache hit/miss
 */
bool store(uint32_t address, uint32_t value, bool backup=false);

/**
* Instruction Cache (SRAM)
*/  
extern std::shared_ptr<cache> icache;

/**
* Data Cache (SRAM)
*/
extern std::shared_ptr<cache> dcache;

/**
* Renamer (Controller for map table and free list)
*/
extern std::shared_ptr<rename> renamer;

extern bool icache_hit;
extern bool dcache_hit;

}

#endif
