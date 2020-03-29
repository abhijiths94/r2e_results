#ifndef THUMBULATOR_RENAME_HPP
#define THUMBULATOR_RENAME_HPP

#include <deque>

namespace thumbulator {

#define RENAME_MEM_START 0x40800000
#define RENAME_MEM_SIZE_BYTES (1 << 22) // 2MB

class rename {	
	public:
	rename():rename(0, 0, false)
	{}
	rename(size_t map_table_entries, uint32_t num_avail_rename_addrs, bool reclaim_addr)
	: MAP_TABLE_ENTRIES(map_table_entries)
	, NUM_AVAIL_RENAME_ADDRS(num_avail_rename_addrs)
	, RECLAIM_ADDR(reclaim_addr)
	{
		assert((NUM_AVAIL_RENAME_ADDRS * 64) <= RENAME_MEM_SIZE_BYTES);

		map_table = new map_table_entry[MAP_TABLE_ENTRIES];
		shadow_map_table = new map_table_entry[MAP_TABLE_ENTRIES];

		fl_start_addr = RENAME_MEM_START;
		fl_end_addr   = RENAME_MEM_START + NUM_AVAIL_RENAME_ADDRS * 64;
		for(auto addr=fl_start_addr; addr<fl_end_addr; addr+=64) {
			freelist.push_back(addr);
		}
	}

	~rename()
	{
		delete [] map_table;
		delete [] shadow_map_table;
		map_table = nullptr; 
		shadow_map_table = nullptr; 
	}

	bool is_map_table_full() 
	{
		return (map_table_curr_entries == MAP_TABLE_ENTRIES);
	}

	bool lookup_map_table(const uint32_t& tag, uint32_t& index)
	{
		for(auto i=0; i<MAP_TABLE_ENTRIES; i++) {
			if(map_table[i].valid && map_table[i].tag == tag) {
				index = i;
				return true; // hit
			}
		}

		return false; // miss
	}

	uint32_t read_map_table(const uint32_t& index) 
	{
		return map_table[index].curr_name;
	}

	void write_map_table(bool hit, const uint32_t& tag, const uint32_t& addr, uint32_t& index)
	{	
		if(hit) {
			// fprintf(stdout, "(HIT) write_map_table: write entry=%d\n", index);
			if(map_table[index].old_name == 0xFFFFFFFF) {
				map_table[index].old_name = map_table[index].curr_name;
            }
			else {
				add_to_freelist(map_table[index].curr_name);
			}
			map_table[index].curr_name = remove_from_freelist(addr);
			if(addr == map_table[index].curr_name) {
				map_table[index].valid = false;
				map_table_curr_entries--;
				reclaimed_mappings++;
			}
			renamed_mappings++;
		}
		else {
			// fprintf(stdout, "(MISS) write_map_table: add entry=%d\n", map_table_curr_entries);
			for(auto i=0; i<MAP_TABLE_ENTRIES; i++) {
				if(!map_table[i].valid) {
					map_table[i].valid = true;
					map_table[i].tag = tag;
					map_table[i].old_name = addr;
					map_table[i].curr_name = remove_from_freelist(addr);
					
					map_table_curr_entries++;
					renamed_mappings++;
					index = i;
					return;
				}
			}
		}
	}

	void backup_map_table()
	{
		for(auto i=0; i<MAP_TABLE_ENTRIES; i++) {
			if(map_table[i].valid && map_table[i].old_name != 0xFFFFFFFF) {
				add_to_freelist(map_table[i].old_name);
				map_table[i].old_name = 0xFFFFFFFF;
			}

			shadow_map_table[i].valid     = map_table[i].valid;
			shadow_map_table[i].tag       = map_table[i].tag;
			shadow_map_table[i].old_name  = map_table[i].old_name;
			shadow_map_table[i].curr_name = map_table[i].curr_name;

		}

	    if(shadow_freelist.size() > 0) {
	     	shadow_freelist.clear();
		}

		auto fl_size = freelist.size();

		for(auto i=0; i<fl_size; i++) {
			auto address = freelist.front();
			shadow_freelist.push_back(address);
			freelist.pop_front();
			freelist.push_back(address);
		}
	}

	uint32_t restore_map_table()
	{
		uint32_t num_map_table_restores = 0;

		auto sfl_size = shadow_freelist.size();

		if(sfl_size > 0) {
			if(freelist.size() > 0) { 
				freelist.clear();
			}

			for(auto i=0; i<sfl_size; i++) {
				auto address = shadow_freelist.front();
				freelist.push_back(address);
				shadow_freelist.pop_front();
				shadow_freelist.push_back(address);
			}
		}

		for(auto i=0; i<MAP_TABLE_ENTRIES; i++) {
			if(map_table[i].valid && map_table[i].old_name != 0xFFFFFFFF) {
				// add_to_freelist(map_table[i].curr_name);
				num_map_table_restores++;
			}

			map_table[i].valid     = shadow_map_table[i].valid;
			map_table[i].tag       = shadow_map_table[i].tag;
			map_table[i].old_name  = shadow_map_table[i].old_name;
			map_table[i].curr_name = shadow_map_table[i].curr_name;
		}

		return num_map_table_restores;
	}

	bool is_name_avail()
	{
		return !freelist.empty();
	}

	uint32_t get_num_valid_entries()
	{
		return map_table_curr_entries;
	}

	uint32_t get_num_backup_entries()
	{
		uint32_t count = 0;

		for(auto i=0; i<MAP_TABLE_ENTRIES; i++) {
			if(map_table[i].valid && map_table[i].dirty) {
				count++;
			}
		}

		return count;
	}

	size_t get_map_table_size()
	{
		return MAP_TABLE_ENTRIES;
	}

	const uint64_t& num_reclaimed_mappings()
	{
		return reclaimed_mappings;
	}

	const uint64_t& num_renamed_mappings()
	{
		return renamed_mappings;
	} 

	void print()
	{
		for(auto i=0; i<MAP_TABLE_ENTRIES; i++) {
			if(map_table[i].valid) {
				fprintf(stdout, "%d tag=0x%8.8x old=0x%8.8x new=0x%8.8x\n", i, map_table[i].tag, map_table[i].old_name, map_table[i].curr_name);
			}
		}
	}

	private:
	struct map_table_entry {
		map_table_entry(): valid(false), dirty(false), tag(0), old_name(0xFFFFFFFF), curr_name(0xFFFFFFFF) {}
		bool     valid;	
		bool     dirty;
		uint32_t tag;
		uint32_t old_name;
		uint32_t curr_name;
	};

	size_t   const MAP_TABLE_ENTRIES;
  	uint32_t const NUM_AVAIL_RENAME_ADDRS;
	bool     const RECLAIM_ADDR;

  	uint32_t fl_start_addr          = 0;
  	uint32_t fl_end_addr            = 0;
  	uint32_t map_table_curr_entries = 0;
  	uint64_t renamed_mappings       = 0;
  	uint64_t reclaimed_mappings     = 0;

	map_table_entry*     map_table = nullptr;
	map_table_entry*     shadow_map_table = nullptr;
	std::deque<uint32_t> freelist;
	std::deque<uint32_t> shadow_freelist;

	void add_to_freelist(const uint32_t& addr) 
	{
		// fprintf(stdout, "add_to_freelist: addr=0x%8.8x\n", addr);
		freelist.push_back(addr);
	}

	bool is_orig_mapping_avail(const uint32_t& addr)
	{
		for(auto itr=freelist.begin(); itr!=freelist.end(); itr++) {
			if(addr == *itr) {
				freelist.erase(itr);
				return true;
			}
		}

		return false;
	}

	uint32_t remove_from_freelist(const uint32_t& addr) 
	{
		auto rc = freelist.front();
		if(!RECLAIM_ADDR) {
                        freelist.pop_front();    
	        }
		else {
		        if(is_orig_mapping_avail(addr)) {
			        rc = addr;
		        }
		        else {
			        freelist.pop_front();
		        }
		}
		// fprintf(stdout, "remove_from_freelist: addr=0x%8.8x\n", rc);
		return rc;
	}
};
}

#endif // THUMBULATOR_RENAME_HPP
