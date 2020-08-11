#include <bitset>

#include "atomic_wrapper.h"
#include "garbage_collection.h"
#include "interface_helper.h"
#include "interface.h"        // NOLINT

namespace ccbench {

Status abort(Token token) {  // NOLINT
  auto *ti = static_cast<session_info *>(token);
  ti->remove_inserted_records_of_write_set_from_masstree();
  ti->clean_up_ops_set();
  ti->clean_up_scan_caches();
  ti->set_tx_began(false);
  ti->gc_records_and_values();
  return Status::OK;
}

Status commit(Token token) {  // NOLINT
  auto *ti = static_cast<session_info *>(token);
  tid_word max_rset;
  tid_word max_wset;

  // Phase 1: Sort lock list;
  std::sort(ti->get_write_set().begin(), ti->get_write_set().end());

  // Phase 2: Lock write set;
  tid_word expected;
  tid_word desired;
  for (auto itr = ti->get_write_set().begin(); itr != ti->get_write_set().end();
       ++itr) {
    if (itr->get_op() == OP_TYPE::INSERT) continue;
    // after this, update/delete
    expected.get_obj() = loadAcquire(itr->get_rec_ptr()->get_tidw().get_obj());
    for (;;) {
      if (expected.get_lock()) {
        expected.get_obj() =
                loadAcquire(itr->get_rec_ptr()->get_tidw().get_obj());
      } else {
        desired = expected;
        desired.set_lock(true);
        if (compareExchange(itr->get_rec_ptr()->get_tidw().get_obj(),
                            expected.get_obj(), desired.get_obj())) {
          break;
        }
      }
    }
    if (itr->get_op() == OP_TYPE::UPDATE &&  // NOLINT
        itr->get_rec_ptr()->get_tidw().get_absent()) {
      ti->unlock_write_set(ti->get_write_set().begin(), itr);
      abort(token);
      return Status::ERR_WRITE_TO_DELETED_RECORD;
    }

    max_wset = std::max(max_wset, expected);
  }

  // Serialization point
  asm volatile("":: : "memory");  // NOLINT
  ti->set_epoch(epoch::load_acquire_global_epoch());
  asm volatile("":: : "memory");  // NOLINT

  // Phase 3: Validation
  tid_word check;
  for (auto itr = ti->get_read_set().begin(); itr != ti->get_read_set().end();
       itr++) {
    const Record *rec_ptr = itr->get_rec_ptr();
    check.get_obj() = loadAcquire(rec_ptr->get_tidw().get_obj());
    if ((itr->get_rec_read().get_tidw().get_epoch() != check.get_epoch() ||
         itr->get_rec_read().get_tidw().get_tid() != check.get_tid()) ||
        check.get_absent() ||  // check whether it was deleted.
        (check.get_lock() &&
         (ti->search_write_set(itr->get_rec_ptr()) == nullptr))
            ) {
      ti->unlock_write_set();
      abort(token);
      return Status::ERR_VALIDATION;
    }
    max_rset = std::max(max_rset, check);
  }

// Phase 4: Write & Unlock

// exec_logging(write_set, myid);

  write_phase(ti, max_rset, max_wset
  );

  ti->set_tx_began(false);
  return
          Status::OK;
}

}  // namespace ccbench
