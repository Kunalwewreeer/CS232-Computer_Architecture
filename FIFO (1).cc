#include <algorithm>
#include <cassert>
#include <map>
#include <vector>

#include "cache.h"

namespace
{
    // Instead of tracking last used cycles, we will track the insertion cycle.
    std::map<CACHE*, std::vector<uint64_t>> insertion_cycles;
}

void CACHE::initialize_replacement()
{
    ::insertion_cycles[this] = std::vector<uint64_t>(NUM_SET * NUM_WAY, 0);
}

uint32_t CACHE::find_victim(uint32_t triggering_cpu, uint64_t instr_id, uint32_t set, const BLOCK* current_set, uint64_t ip, uint64_t full_addr, uint32_t type)
{
    auto begin = std::next(std::begin(::insertion_cycles[this]), set * NUM_WAY);
    auto end = std::next(begin, NUM_WAY);

    // Find the block that was inserted earliest (FIFO)
    auto victim = std::min_element(begin, end);
    assert(begin <= victim);
    assert(victim < end);
    return static_cast<uint32_t>(std::distance(begin, victim)); // cast protected by prior asserts
}

void CACHE::update_replacement_state(uint32_t triggering_cpu, uint32_t set, uint32_t way, uint64_t full_addr, uint64_t ip, uint64_t victim_addr, uint32_t type, uint8_t hit)
{
    // With FIFO, we only set the insertion cycle when a block is first inserted, not on every hit
    // Therefore, we check if the block is being inserted (i.e., if it's a miss and we're replacing a block)
    if (!hit)
        ::insertion_cycles[this].at(set * NUM_WAY + way) = current_cycle;
}

void CACHE::replacement_final_stats() {}
