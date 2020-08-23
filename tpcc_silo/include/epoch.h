/**
 * @file epoch.h
 * @brief header about epoch
 */

#pragma once

#include <atomic>
#include <thread>

#include "atomic_wrapper.h"
#include "cpu.h"


namespace ccbench::epoch {

/**
 * @brief epoch_t
 * @details
 * tid_word is composed of union ...
 * 1bits : lock
 * 1bits : latest
 * 1bits : absent
 * 29bits : tid
 * 32 bits : epoch.
 * So epoch_t should be uint32_t.
 */
using epoch_t = std::uint32_t;

[[maybe_unused]] alignas(CACHE_LINE_SIZE) inline epoch_t kGlobalEpoch;  // NOLINT
alignas(CACHE_LINE_SIZE) inline epoch_t kReclamationEpoch;              // NOLINT

// about epoch thread
[[maybe_unused]] inline std::thread kEpochThread;  // NOLINT
inline std::atomic<bool> kEpochThreadEnd;          // NOLINT

[[maybe_unused]] extern uint32_t atomic_add_global_epoch();

[[maybe_unused]] extern bool check_epoch_loaded();  // NOLINT

/**
 * @brief epoch thread
 * @pre this function is called by invoke_core_thread function.
 */
[[maybe_unused]] extern void epocher();

[[maybe_unused]] static epoch::epoch_t get_reclamation_epoch() {  // NOLINT
  return loadAcquire(kReclamationEpoch);
}

/**
 * @brief invoke epocher thread.
 * @post invoke fin() to join this thread.
 */
[[maybe_unused]] static void invoke_epocher() {
  kEpochThreadEnd.store(false, std::memory_order_release);
  kEpochThread = std::thread(epocher);
}

[[maybe_unused]] static void join_epoch_thread() { kEpochThread.join(); }

[[maybe_unused]] static std::uint32_t load_acquire_global_epoch() {  // NOLINT
  return loadAcquire(epoch::kGlobalEpoch);
}

[[maybe_unused]] static void set_epoch_thread_end(bool tf) {
  kEpochThreadEnd.store(tf, std::memory_order_release);
}


alignas(CACHE_LINE_SIZE) inline time_t timestamp_;


[[maybe_unused]] inline time_t get_lightweight_timestamp()
{
    return loadAcquire(timestamp_);
}


}  // namespace ccbench::epoch
