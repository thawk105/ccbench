/**
 * @file interface_update_insert.cpp
 * @brief implement about transaction
 */

#include <bitset>

#include "atomic_wrapper.h"
#include "garbage_collection.h"
#include "interface_helper.h"
#include "index/masstree_beta/include/masstree_beta_wrapper.h"

namespace ccbench {

Status insert(Token token, Storage st,  // NOLINT
              std::string_view key, std::string_view val, std::size_t val_align) {
  auto *ti = static_cast<session_info *>(token);
  if (!ti->get_txbegan()) tx_begin(token);
  write_set_obj *inws{ti->search_write_set(key)};
  if (inws != nullptr) {
    inws->reset_tuple_value(val, val_align);
    return Status::WARN_WRITE_TO_LOCAL_WRITE;
  }

  masstree_wrapper<Record>::thread_init(sched_getcpu());
  if (kohler_masstree::find_record(st, key) != nullptr) {
    return Status::WARN_ALREADY_EXISTS;
  }

  Record *rec_ptr = new Record(key, val, val_align);
  Status insert_result(
          kohler_masstree::insert_record(st, key, rec_ptr));
  if (insert_result == Status::OK) {
    ti->get_write_set().emplace_back(OP_TYPE::INSERT, st, rec_ptr);
    return Status::OK;
  }
  delete rec_ptr;  // NOLINT
  return Status::WARN_ALREADY_EXISTS;
}

Status update(Token token, Storage st, std::string_view key, std::string_view val, std::size_t val_align) {
  auto *ti = static_cast<session_info *>(token);
  if (!ti->get_txbegan()) tx_begin(token);

  write_set_obj *inws{ti->search_write_set(key)};
  if (inws != nullptr) {
    inws->reset_tuple_value(val, val_align);
    return Status::WARN_WRITE_TO_LOCAL_WRITE;
  }

  masstree_wrapper<Record>::thread_init(sched_getcpu());
  Record *rec_ptr{
          kohler_masstree::get_mtdb(st).get_value(key)};
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

  ti->get_write_set().emplace_back(key, val, val_align, OP_TYPE::UPDATE, st, rec_ptr);

  return Status::OK;
}

Status upsert(Token token, Storage st, std::string_view key, std::string_view val, std::size_t val_align) {
  auto *ti = static_cast<session_info *>(token);
  if (!ti->get_txbegan()) tx_begin(token);
  write_set_obj *in_ws{ti->search_write_set(key)};
  if (in_ws != nullptr) {
    in_ws->reset_tuple_value(val, val_align);
    return Status::WARN_WRITE_TO_LOCAL_WRITE;
  }

  masstree_wrapper<Record>::thread_init(sched_getcpu());
  Record *rec_ptr{
          static_cast<Record *>(kohler_masstree::kohler_masstree::find_record(st, key))};
  if (rec_ptr == nullptr) {
    rec_ptr = new Record(key, val, val_align);
    Status insert_result(
            kohler_masstree::insert_record(st, key, rec_ptr));
    if (insert_result == Status::OK) {
      ti->get_write_set().emplace_back(OP_TYPE::INSERT, st, rec_ptr);
      return Status::OK;
    }
    // else insert_result == Status::WARN_ALREADY_EXISTS
    // so goto update.
    delete rec_ptr;  // NOLINT
  }

  ti->get_write_set().emplace_back(key, val, val_align, OP_TYPE::UPDATE, st, rec_ptr);  // NOLINT

  return Status::OK;
}

}  // namespace ccbench
