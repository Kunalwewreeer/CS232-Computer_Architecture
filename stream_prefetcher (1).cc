#include <algorithm>
#include <array>
#include <map>
#include <optional>
#include <vector>

#include "cache.h"

namespace
{

constexpr size_t PREFETCH_DEGREE = 3;     // Degree of prefetching
constexpr size_t PREFETCH_DISTANCE = 8;   // Distance of lookahead for prefetching
constexpr size_t MONITOR_TABLE_SIZE = 64; // Size of monitoring table

// Entry for the stream tracker
struct StreamTrackerEntry {
  uint64_t start_addr = 0;
  uint64_t end_addr = 0;
  int stream_direction = 0;
  int region_size = 0;
};

// Stream prefetcher tracker
class StreamPrefetcherTracker
{
  std::vector<StreamTrackerEntry> table;

public:
  // Start a stream on a cache miss
  void Start_stream(uint64_t missed_addr)
  {
    // If the table is full, evict the oldest entry (simple LRU policy)
    if (table.size() >= MONITOR_TABLE_SIZE) {
      table.erase(table.begin()); // Evicts the least recently used stream
    }

    // Start a new stream with just the initial miss
    table.push_back(StreamTrackerEntry{missed_addr, 0, 0, 0});
  }

  // Check if the address is within any monitoring region and prefetch if needed
  void check_and_prefetch(CACHE* cache, uint64_t missed_addr)
  {
    for (auto& entry : table) {
      // If we have a confirmed direction and the miss is within the region
      if (entry.stream_direction != 0 && is_within_region(entry, missed_addr)) {
        // Prefetch PREFETCH_DEGREE cache lines ahead
        for (size_t i = 1; i <= PREFETCH_DEGREE; ++i) {
          uint64_t prefetch_addr = entry.end_addr + entry.stream_direction * i;
          cache->prefetch_line(prefetch_addr << LOG2_BLOCK_SIZE, true, 0);
        }

        // Move the monitoring region
        entry.start_addr += entry.stream_direction * PREFETCH_DEGREE;
        entry.end_addr += entry.stream_direction * PREFETCH_DEGREE;
      }
    }
  }

  // Update stream direction based on misses
  void update_stream_direction(uint64_t missed_addr)
  {
    for (auto& entry : table) {
      if (entry.region_size == 0) { // If we haven't set a direction yet
        int64_t delta = static_cast<int64_t>(missed_addr) - static_cast<int64_t>(entry.start_addr);

        if (delta != 0) { // Ignore if the same address
          entry.region_size = std::abs(delta);
          entry.stream_direction = (delta > 0) ? 1 : -1;
          entry.end_addr = entry.start_addr + entry.stream_direction * PREFETCH_DISTANCE;
        }
      } else {
        // If we have a previous direction, check if the new miss confirms the direction
        int64_t delta = static_cast<int64_t>(missed_addr) - static_cast<int64_t>(entry.start_addr);

        // Z must be larger than previous delta (Y) and less than PREFETCH_DISTANCE
        if ((entry.stream_direction > 0 && delta > entry.region_size && delta < PREFETCH_DISTANCE)
            || (entry.stream_direction < 0 && delta < -entry.region_size && delta > -PREFETCH_DISTANCE)) {
          // Confirmed direction
          entry.region_size = std::abs(delta);
          entry.end_addr = entry.start_addr + entry.stream_direction * PREFETCH_DISTANCE;
        } else {
          // If the direction is not confirmed, reset the region size
          entry.region_size = 0;
        }
      }
    }
  }

private:
  // Helper to check if an address is within a monitoring region
  bool is_within_region(const StreamTrackerEntry& entry, uint64_t addr) const
  {
    if (entry.stream_direction > 0) {
      return addr > entry.start_addr && addr <= entry.end_addr;
    } else {
      return addr < entry.start_addr && addr >= entry.end_addr;
    }
  }
};

// Map each cache to its own stream prefetcher tracker
std::map<CACHE*, StreamPrefetcherTracker> stream_trackers;

} // namespace

void CACHE::prefetcher_initialize() { stream_trackers.emplace(std::make_pair(this, StreamPrefetcherTracker())); }

void CACHE::prefetcher_cycle_operate() {}

uint32_t CACHE::prefetcher_cache_operate(uint64_t addr, uint64_t ip, uint8_t cache_hit, bool useful_prefetch, uint8_t type, uint32_t metadata_in)
{
  // On a cache miss, we would start or update a stream
  if (!cache_hit) {
    auto& tracker = stream_trackers[this];
    tracker.update_stream_direction(addr >> LOG2_BLOCK_SIZE);
    tracker.check_and_prefetch(this, addr >> LOG2_BLOCK_SIZE);
  }
  return metadata_in;
}

uint32_t CACHE::prefetcher_cache_fill(uint64_t addr, uint32_t set, uint32_t way, uint8_t prefetch, uint64_t evicted_addr, uint32_t metadata_in)
{
  // If the incoming block is not a prefetch, start a stream
  if (!prefetch) {
    auto& tracker = stream_trackers[this];
    tracker.Start_stream(addr >> LOG2_BLOCK_SIZE);
  }
  return metadata_in;
}

void CACHE::prefetcher_final_stats() {}
