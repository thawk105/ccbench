/**
 * @file epoch.cpp
 * @brief implement about epoch
 */

#include "epoch.h"

#include <xmmintrin.h>  // NOLINT

#include "session_info_table.h"
#include "clock.h"
#include "../include/util.hh"
#include <ctime>


namespace ccbench::epoch {

uint32_t atomic_add_global_epoch() {
  std::uint32_t expected = load_acquire_global_epoch();
  for (;;) {
    std::uint32_t desired = expected + 1;
    if (__atomic_compare_exchange_n(&(kGlobalEpoch), &(expected), desired,
                                    false, __ATOMIC_ACQ_REL,
                                    __ATOMIC_ACQUIRE)) {
      return desired;
    }
  }
}

bool check_epoch_loaded() {  // NOLINT
  std::uint64_t curEpoch = load_acquire_global_epoch();

  for (auto &&itr : session_info_table::get_thread_info_table()) {  // NOLINT
    if (itr.get_visible() && itr.get_epoch() != curEpoch) {
      return false;
    }
  }

  return true;
}

void epocher() {
  /**
   * Increment global epoch in each 40ms.
   * To increment it,
   * all the worker-threads need to read the latest one.
   */
  while (likely(!kEpochThreadEnd.load(std::memory_order_acquire))) {
    sleepMs(KVS_EPOCH_TIME);

    {
        // reset timestamp.
        struct timespec ts;
        [[maybe_unused]] int ret = ::clock_gettime(CLOCK_MONOTONIC, &ts);
        assert(ret == 0);
        if (get_lightweight_timestamp() != ts.tv_sec) {
            storeRelease(timestamp_, ts.tv_sec);
        }
    }

    /**
     * check_epoch_loaded() checks whether the
     * latest global epoch is read by all the threads
     */
    std::size_t exp_ctr{1};
    while (!check_epoch_loaded()) {
      if (kEpochThreadEnd.load(std::memory_order_acquire)) return;
      usleep(exp_ctr);
      exp_ctr = std::min<size_t>(exp_ctr * 2, 1000);
    }

    uint32_t curEpoch = atomic_add_global_epoch();
    storeRelease(kReclamationEpoch, curEpoch - 2);
  }
}

}  // namespace ccbench::epoch
