/**
 * @file interface_delete.cpp
 * @brief implement about transaction
 */

#include <bitset>

#include "atomic_wrapper.h"
#include "garbage_collection.h"
#include "interface_helper.h"
#include "index/masstree_beta/include/masstree_beta_wrapper.h"

namespace ccbench {

[[maybe_unused]] Status delete_all_records() {  // NOLINT
  /**
   * INDEX_KOHLER_MASSTREE case
   * Since the destructor of the stored value is also called by the destructor
   * of kohler masstree, there is no need to do anything with
   * INDEX_KOHLER_MASSTREE.
   */
  return Status::OK;
}

Status delete_record(Token token, Storage st, std::string_view key) {
  auto *ti = static_cast<session_info *>(token);
  if (!ti->get_txbegan()) tx_begin(token);
  Status check = ti->check_delete_after_write(st, key);

  masstree_wrapper<Record>::thread_init(cached_sched_getcpu());
  Record *rec_ptr{
          static_cast<Record *>(kohler_masstree::get_mtdb(st).get_value(key))};
  if (rec_ptr == nullptr) {
    return Status::WARN_NOT_FOUND;
  }
  tid_word check_tid(loadAcquire(rec_ptr->get_tidw().get_obj()));
  if (check_tid.get_absent()) {
    // The second condition checks
    // whether the record you want to read should not be read by parallel
    // insert / delete.
    return Status::WARN_NOT_FOUND;
  }

  ti->get_write_set().emplace_back(OP_TYPE::DELETE, st, rec_ptr);
  return check;
}

}  // namespace ccbench
