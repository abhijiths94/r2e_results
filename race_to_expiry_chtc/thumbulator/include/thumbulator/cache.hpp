#ifndef THUMBULATOR_CACHE_HPP
#define THUMBULATOR_CACHE_HPP

#include <cstdint>
#include <cmath>
#include <cassert>

#include <thumbulator/cache_block.hpp>
#include "../../../external/hashmap/include/hashmap/hashmap.hpp"

namespace thumbulator {

struct cache_attributes {
	cache_attributes(): set(0), way(0) {}
	size_t set;
	size_t way;
};

typedef struct cache_attributes cache_attr;

class cache {

	public:
	cache(): cache(1, 64, 2048, 16)
	{}
	cache(size_t assoc, uint32_t block_size, uint32_t cache_size, size_t lbf_size):
	ASSOC(assoc),
	BLOCK_SIZE(block_size),
	CACHE_SIZE(cache_size),
	LBF_SIZE(lbf_size),
	hits(0),
	misses(0) 
	{
		assert(BLOCK_SIZE >= 4);

		block_offset = std::ceil(std::log2(BLOCK_SIZE));

		for(uint32_t i=0; i<block_offset; i++) {
			block_mask = (block_mask << 1) | 1;
		}

		if(ASSOC > 0) { // direct-mapped or set-associative
			SET = CACHE_SIZE/(ASSOC * BLOCK_SIZE);
			set_offset = std::ceil(std::log2(SET));
		}
		else { // fully-associative
			SET = 1;
			ASSOC = CACHE_SIZE/BLOCK_SIZE;
		}

		for(uint32_t i=0; i<set_offset; i++) {
			set_mask = (set_mask << 1) | 1;
		}

		tag_array = new cache_block*[SET];
		data_array = new uint32_t*[SET];
		if(LBF_SIZE > 0)
			lbf_array = new std::unique_ptr<hmap::hashmap>*[SET];

		for(size_t set=0; set<SET; set++) {
			tag_array[set] = new cache_block[ASSOC];
			data_array[set] = new uint32_t[BLOCK_SIZE/sizeof(uint32_t) * ASSOC];
			if(LBF_SIZE > 0) {
				lbf_array[set] = new std::unique_ptr<hmap::hashmap>[ASSOC];

				for(size_t way=0; way<ASSOC; way++) {
					lbf_array[set][way] = std::unique_ptr<hmap::hashmap>(new hmap::hashmap(lbf_size));
				}
			}
		}

		for(size_t set=0; set<SET; set++) {
			for(size_t idx=0; idx<BLOCK_SIZE/sizeof(uint32_t) * ASSOC; idx++) {
				data_array[set][idx] = 0;
			}
		}

	}

	~cache()
	{
	    for(size_t set=0; set<SET; set++) {
	      delete[] tag_array[set];
	      delete[] data_array[set];
	      tag_array[set]  = nullptr;
	      data_array[set] = nullptr;
	      if(lbf_array[set]) {
	      	delete[] lbf_array[set];
	      	lbf_array[set]  = nullptr;
	      }	
	    }

	    delete[] tag_array;
	    delete[] data_array;
	    tag_array  = nullptr;
	    data_array = nullptr;
	    if(lbf_array) {
	      delete[] lbf_array;
	      lbf_array  = nullptr;
	    }
	}

	/*
	* Access methods for tag array
	*/

	bool is_hit(uint32_t addr, cache_attr& attr)
	{
		auto address = addr;
		addr >>= block_offset;
		attr.set = addr & set_mask;
		auto tag = addr >> set_offset;

		for(size_t i=0; i<ASSOC; i++) {
			if(tag_array[attr.set][i].get_valid() && tag_array[attr.set][i].get_tag() == tag) {
				attr.way = i;
				// fprintf(stdout, "CACHE HIT: address=0x%8.8x set=%zu way=%zu\n", address, attr.set, attr.way);
				hits++;
				return true;
			}
		}

		// fprintf(stdout, "CACHE MISS: address=0x%8.8x set=%zu\n", address, attr.set);
		misses++;
		return false;
	}

	cache_block get_victim(cache_attr& attr)
	{
		cache_block victim = tag_array[attr.set][0];
		attr.way = 0;
		for(size_t i=0; i<ASSOC; i++) {
			if(!tag_array[attr.set][i].get_valid()) {
				attr.way = i;
				return tag_array[attr.set][i];
			}
			if(tag_array[attr.set][i].get_cnt() > victim.get_cnt()) {
				victim = tag_array[attr.set][i];
				attr.way = i;
			}
		}

		return victim;
	}

	const cache_block& cache_read(const cache_attr& attr, const bool false_read)
	{
		if(!false_read) {
		  update_cnt(attr);
		}
		return tag_array[attr.set][attr.way];
	}

	void cache_write(bool wf, const cache_attr& attr)
	{
		tag_array[attr.set][attr.way].set_dirty(1);
		if(wf) {
			tag_array[attr.set][attr.way].set_wf(1);
		}

		update_cnt(attr);
	}

	void cache_insert(const cache_attr& attr, const cache_block& blk, const bool false_read)
	{
		tag_array[attr.set][attr.way] = blk;
		// fprintf(stdout, "cache_insert: v=%d, d=%d, set=%zu, way=%zu, tag=%X, address=0x%8.8X\n", 
		//     tag_array[attr.set][attr.way].get_valid(), tag_array[attr.set][attr.way].get_dirty(), attr.set, attr.way, tag_array[attr.set][attr.way].get_tag(), tag_array[attr.set][attr.way].get_address());
		if(!false_read) {
		  update_cnt(attr);
		}
	}

	void flush()
	{
		for(size_t i=0; i<SET; i++) {
			for(size_t j=0; j<ASSOC; j++) {
				tag_array[i][j].set_valid(0);
				tag_array[i][j].set_dirty(0);
				if(LBF_SIZE > 0)
					lbf_array[i][j]->clear();
			}
		}
	}

	void mark_clean(size_t set, size_t way)
	{
		tag_array[set][way].set_dirty(0);
		tag_array[set][way].set_wf(0);

		if(LBF_SIZE > 0)
			lbf_array[set][way]->clear();
	}

	const cache_block& get_block(size_t set, size_t way)
	{
		return tag_array[set][way];
	}

	/*
	* Access methods for data array
	*/

	uint32_t get_data(size_t set, size_t way, uint32_t beat)
	{
		cache_attr attr;
		attr.set = set;
		attr.way = way;

		// fprintf(stdout, "In get_data: beat=%d data=%d\n", beat, data_array[set][get_index(beat, attr)]);
		return data_array[set][get_index(beat, attr)];
	}

	void set_data(size_t set, size_t way, uint32_t beat, uint32_t data)
	{
		// fprintf(stdout, "In set_data: beat=%d data=%X\n", beat, data);
		cache_attr attr;
		attr.set = set;
		attr.way = way;

		data_array[set][get_index(beat, attr)] = data;
	}

	/*
	* Access methods for local bloom filter array
	*/

	const hmap::lbf_states get_state(size_t set, size_t way, uint32_t key)
	{
		// fprintf(stdout, "get_state: set=%zu way=%zu state=%d\n", set, way, word_state);
		if(LBF_SIZE > 0)
			return lbf_array[set][way]->get(key);
		return hmap::cReadFirst;
	} 

	void set_state(size_t set, size_t way, uint32_t key, const hmap::lbf_states& state)
	{
		// fprintf(stdout, "set_state: set=%zu way=%zu state=%d\n", set, way, value);
		if(LBF_SIZE > 0)
			lbf_array[set][way]->put(key, state);
	}

	const bool get_block_state(size_t set, size_t way)
	{
		bool blk_state = true;
		if(LBF_SIZE > 0)
			blk_state = lbf_array[set][way]->get_all();
		// fprintf(stdout, "get_block_state: set=%zu way=%zu state=%d\n", set, way, blk_state);
		return blk_state;
	}

	void clear_state(size_t set, size_t way)
	{
		// fprintf(stdout, "clear_state: set=%zu way=%zu\n", set, way);
		if(LBF_SIZE > 0)
			lbf_array[set][way]->clear();
	}

	/*
	* Helper methods for querying various parameters of the cache
	*/

	const size_t get_numset()
	{
		return SET;
	}

	const size_t get_numway()
	{
		return ASSOC;
	}

	const uint32_t get_set_offset()
	{
		return set_offset;
	}

	const uint32_t get_block_offset()
	{
		return block_offset;
	}

	const uint32_t get_block_size()
	{
		return BLOCK_SIZE;
	}

	const uint32_t get_block_mask()
	{
		return block_mask;
	}

	const uint64_t get_num_hits()
	{
		return hits;
	}

	const uint64_t get_num_misses()
	{
		return misses;
	}

	private:
	size_t ASSOC = 0;
	size_t SET = 0;
	const uint32_t BLOCK_SIZE;
	const uint32_t CACHE_SIZE;
	const size_t   LBF_SIZE;

	uint32_t block_offset = 0;
	uint32_t set_offset = 0;
	uint32_t block_mask = 0;
	uint32_t set_mask = 0;
	uint64_t hits = 0u;
	uint64_t misses = 0u;

	cache_block** tag_array = nullptr;
	uint32_t** data_array = nullptr;
	std::unique_ptr<hmap::hashmap>** lbf_array = nullptr; // lbf = local bloom filter

	uint32_t get_index(uint32_t beat, const cache_attr& attr)
	{
		return (BLOCK_SIZE >> 2) * attr.way + beat;
	}

	void update_cnt(const cache_attr& attr)
	{
		for(uint32_t i=0; i<ASSOC; i++) {
			if(i != attr.way && tag_array[attr.set][i].get_valid() && (tag_array[attr.set][i].get_cnt() < ASSOC)) {
				tag_array[attr.set][i].set_cnt(tag_array[attr.set][i].get_cnt() + 1);
			}
		}
		tag_array[attr.set][attr.way].set_cnt(0);
	}
};
}

#endif // THUMBULATOR_CACHE_HPP
