/**
 * @file epoch.cpp
 * @brief implement about epoch
 */

#include "cc/silo_variant/include/epoch.h"

#include <xmmintrin.h>  // NOLINT

#include "cc/silo_variant/include/session_info_table.h"
#include "clock.h"
#include "include/tuple_local.h"  // sizeof(Tuple)

namespace shirakami::cc_silo_variant::epoch {

void atomic_add_global_epoch() {
  std::uint32_t expected = load_acquire_global_epoch();
  for (;;) {
    std::uint32_t desired = expected + 1;
    if (__atomic_compare_exchange_n(&(kGlobalEpoch), &(expected), desired,
                                    false, __ATOMIC_ACQ_REL,
                                    __ATOMIC_ACQUIRE)) {
      break;
    }
  }
}

bool check_epoch_loaded() {  // NOLINT
  uint64_t curEpoch = load_acquire_global_epoch();

  for (auto&& itr : session_info_table::get_thread_info_table()) {  // NOLINT
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

    /**
     * check_epoch_loaded() checks whether the
     * latest global epoch is read by all the threads
     */
    while (!check_epoch_loaded()) {
      if (kEpochThreadEnd.load(std::memory_order_acquire)) return;
      _mm_pause();
    }

    atomic_add_global_epoch();
    storeRelease(kReclamationEpoch, loadAcquire(kGlobalEpoch) - 2);
  }
}

}  // namespace shirakami::cc_silo_variant::epoch
