#include <algorithm>
#include <cassert>
#include <map>
#include <random>
#include <vector>

#include "cache.h"

namespace
{
std::map<CACHE*, std::vector<uint64_t>> last_used_cycles;
std::map<CACHE*, std::mt19937> rng_engines;             // Random number generators for each cache instance
std::map<CACHE*, std::vector<bool>> is_in_lru_position; // Tracks whether a line is considered LRU
} // namespace

void CACHE::initialize_replacement()
{
  ::last_used_cycles[this] = std::vector<uint64_t>(NUM_SET * NUM_WAY, 0);
  std::random_device rd;
  rng_engines[this] = std::mt19937(rd());
  // Initialize the LRU indicators
  is_in_lru_position[this] = std::vector<bool>(NUM_SET * NUM_WAY, true); // Initially, all are in LRU
}

uint32_t CACHE::find_victim(uint32_t triggering_cpu, uint64_t instr_id, uint32_t set, const BLOCK* current_set, uint64_t ip, uint64_t full_addr, uint32_t type)
{
  auto& cycles = last_used_cycles[this];
  auto begin = cycles.begin() + set * NUM_WAY;
  auto end = begin + NUM_WAY;

  // Find the way with the oldest (minimum) cycle, which is LRU
  auto victim_it = std::min_element(begin, end);
  assert(begin <= victim_it && victim_it < end);

  // Return the index of the victim within its set
  return static_cast<uint32_t>(std::distance(begin, victim_it));
}

void CACHE::update_replacement_state(uint32_t triggering_cpu, uint32_t set, uint32_t way, uint64_t full_addr, uint64_t ip, uint64_t victim_addr, uint32_t type,
                                     uint8_t hit)
{
  auto& cycles = ::last_used_cycles[this];
  auto& lru_position = is_in_lru_position[this];

  if (hit) {
    // If it's a hit and the line is in LRU position, promote to MRU
    if (lru_position[set * NUM_WAY + way]) {
      cycles[set * NUM_WAY + way] = current_cycle;
      lru_position[set * NUM_WAY + way] = false; // Now it's in MRU position
    }
    // If it's not in LRU position, don't change the last used cycle
  } else {
    // On miss, we need to decide if we will insert in LRU or MRU based on BIP policy
    double epsilon = 0.00; // Set epsilon for BIP
    std::uniform_real_distribution<double> dist(0.0, 1.0);
    bool insert_in_mru = dist(rng_engines[this]) > epsilon;

    if (insert_in_mru) {
      // Insert in MRU position
      cycles[set * NUM_WAY + way] = current_cycle;
      lru_position[set * NUM_WAY + way] = false; // It's in MRU position
    } else {
      // Insert in LRU position
      // Don't need to change the cycle as it should already be the minimum
      lru_position[set * NUM_WAY + way] = true; // It's in LRU position
    }
  }
}

void CACHE::replacement_final_stats()
{
  // Any final statistics can be printed here. For BIP, there might not be any additional stats to print.
}
