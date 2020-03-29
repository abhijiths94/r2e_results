#include "liveness_trace.hpp"

#include <fstream>
#include <iostream>
#include <sstream>

namespace ehsim {
liveness_trace::liveness_trace(bool use_liveness, std::string const &path_to_trace)
{
  if(use_liveness) {
    uint64_t cycle;
    uint64_t dead_item;
    std::string line;

    std::ifstream trace(path_to_trace);

    while(getline(trace, line)) {
      std::istringstream iss(line);
      if(iss >> cycle) {
        std::set<uint64_t> dead_item_set;
        dead_item_set.clear();
        while(iss >> std::hex >> dead_item) {
          dead_item_set.emplace(dead_item);
        }
        // std::cout << "cycle=" << cycle << std:: endl;
        // for(auto it=dead_item_set.begin(); it!=dead_item_set.end(); it++)
        //   std::cout << "r" << *it << " ";
        // std::cout << std::endl;
        liveness.emplace(cycle, dead_item_set);
      } 
    }
  }
}

const std::set<uint64_t> liveness_trace::get_liveness(uint64_t const cycle) const
{
  // find the liveness count from the cycle provided or immediate lower cycle
  if(liveness.find(cycle) != liveness.end()) {
    return liveness.at(cycle);
  }

  for(uint64_t i=cycle-1; i>0; i--) {
    if(liveness.find(i) != liveness.end()) {
      return liveness.at(i);
    }
  }

  return std::set<uint64_t>();
}
}
