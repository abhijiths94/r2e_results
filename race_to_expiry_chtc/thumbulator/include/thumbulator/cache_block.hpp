#ifndef THUMBULATOR_CACHE_BLOCK_HPP
#define THUMBULATOR_CACHE_BLOCK_HPP

namespace thumbulator {

// class object for cache block
class cache_block{
  public:
    cache_block() : valid(0), dirty(0), wf(0), tag(0), address(0), cnt(0)
    {
    }

    cache_block(bool valid, bool dirty, bool wf, uint32_t tag, uint32_t address, uint32_t cnt)
    {
      this->valid = valid;
      this->dirty = dirty;
      this->wf = wf;
      this->tag = tag;
      this->address = address;
      this->cnt = cnt;
    }

    void set_valid(bool valid)         { this->valid = valid; }
    void set_dirty(bool dirty)         { this->dirty = dirty; }
    void set_wf(bool wf)               { this->wf = wf; }
    void set_tag(uint32_t tag)         { this->tag = tag; }
    void set_address(uint32_t address) { this->address = address; }
    void set_cnt(uint32_t cnt)         { this->cnt = cnt; }

    bool get_valid() const             { return valid; }
    bool get_dirty() const             { return dirty; }
    bool get_wf() const                { return wf; }
    uint32_t get_tag() const           { return tag; }
    uint32_t get_address() const       { return address; }
    uint32_t get_cnt() const           { return cnt; }

    cache_block& operator = (const cache_block& blk)
    {
      valid = blk.get_valid();
      dirty = blk.get_dirty();
      wf = blk.get_wf();
      tag = blk.get_tag();
      address = blk.get_address();
      cnt = blk.get_cnt();

      return *this;
    }

  private:
    bool     valid;
    bool     dirty;
    bool     wf;
    uint32_t tag;
    uint32_t address;
    uint32_t cnt; // counter for LRU replacement policy
};
}

#endif //THUMBULATOR_CACHE_BLOCK_HPP