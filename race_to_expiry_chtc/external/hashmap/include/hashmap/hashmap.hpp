#ifndef HASHMAP_HASHMAP_HPP
#define HASHMAP_HASHMAP_HPP

namespace hmap {

enum filter_states {
	cWriteFirst,
	cReadFirst,
	cUnknown
};

typedef enum filter_states lbf_states;

class hashmap {
	public:
	hashmap():hashmap(16)
	{}

	hashmap(size_t tablesize):
	TABLESIZE(tablesize) 
	{
		// fprintf(stdout, "hashtable size=%zu\n", tablesize);
		hashtable = new lbf_states[tablesize];
	}

	~hashmap()
	{
		delete[] hashtable;
	}

	const lbf_states& get(const uint32_t& key)
	{
		return hashtable[hashfunc(key)];
	}

	void put(const uint32_t& key, const lbf_states& value)
	{
		hashtable[hashfunc(key)] = value;
	}

	const bool get_all()
	{
		for(size_t i=0; i<TABLESIZE; i++) {
			if(hashtable[i] == cReadFirst) {
				return true;
			}
		}

		return false;
	}

	void clear()
	{
		for(size_t i=0; i<TABLESIZE; i++) {
			hashtable[i] = cUnknown;
		}
	}

	private:
	lbf_states* hashtable = nullptr;

	const size_t TABLESIZE;

	uint32_t hashfunc(const uint32_t& key) const
    {
        return key % TABLESIZE;
    }
};
}

#endif // HASHMAP_HASHMAP_HPP
