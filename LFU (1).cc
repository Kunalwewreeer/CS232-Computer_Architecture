#include <algorithm>
#include <cassert>
#include <map>
#include <vector>
#include <limits>

#include "cache.h"

namespace {
    // This structure maps each CACHE to a vector of access counts for each cache line.
    std::map<CACHE*, std::vector<uint64_t>> access_counts;
}

void CACHE::initialize_replacement() {
    ::access_counts[this] = std::vector<uint64_t>(NUM_SET * NUM_WAY, 0);
}

uint32_t CACHE::find_victim(uint32_t triggering_cpu, uint64_t instr_id, uint32_t set, const BLOCK* current_set, uint64_t ip, uint64_t full_addr, uint32_t type) {
    auto begin = std::next(std::begin(::access_counts[this]), set * NUM_WAY);
    auto end = std::next(begin, NUM_WAY);

    // Find the way with the minimum access count.
    auto victim = std::min_element(begin, end);
    assert(begin <= victim);
    assert(victim < end);

    // Return the index of the way within the set as the victim for replacement.
    return static_cast<uint32_t>(std::distance(begin, victim));
}

void CACHE::update_replacement_state(uint32_t triggering_cpu, uint32_t set, uint32_t way, uint64_t full_addr, uint64_t ip, uint64_t victim_addr, uint32_t type, uint8_t hit) {
    // Increment the access count for the cache line that was accessed.
    // We increment on both hit and miss to keep track of frequency.
    ::access_counts[this].at(set * NUM_WAY + way) += 1;

    // If a block is evicted (i.e., hit is 0), then we reset its access count.
    if (!hit) {
        ::access_counts[this].at(set * NUM_WAY + way) = 0;
    }
}

void CACHE::replacement_final_stats() {}

