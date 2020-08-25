#include "interface_helper.h"
#include "session_info_table.h"
#include "index/masstree_beta/include/masstree_beta_wrapper.h"
#include "interface.h"  // NOLINT

namespace ccbench {


Status search_key_local_set(session_info *ti, Storage storage, std::string_view key, Tuple **tuple)
{
  write_set_obj *inws = ti->search_write_set(storage, key);
  if (inws != nullptr) {
    if (inws->get_op() == OP_TYPE::DELETE) {
      return Status::WARN_ALREADY_DELETE;
    }
    *tuple = &inws->get_tuple(inws->get_op());
    return Status::WARN_READ_FROM_OWN_OPERATION;
  }

  read_set_obj *inrs = ti->search_read_set(storage, key);
  if (inrs != nullptr) {
    *tuple = &inrs->get_rec_read().get_tuple();
    return Status::WARN_READ_FROM_OWN_OPERATION;
  }
  return Status::OK;
}


Status search_key(Token token, Storage storage,  // NOLINT
                  std::string_view key, Tuple **tuple) {
  auto *ti = static_cast<session_info *>(token);
  if (!ti->get_txbegan()) tx_begin(token);

  masstree_wrapper<Record>::thread_init(cached_sched_getcpu());

  Status sta = search_key_local_set(ti, storage, key, tuple);
  if (sta != Status::OK) return sta;

  Record *rec_ptr{kohler_masstree::get_mtdb(storage).get_value(key.data(), key.size())};
  if (rec_ptr == nullptr) {
    *tuple = nullptr;
    return Status::WARN_NOT_FOUND;
  }
  tid_word chk_tid(loadAcquire(rec_ptr->get_tidw().get_obj()));
  if (chk_tid.get_absent()) {
    // The second condition checks
    // whether the record you want to read should not be read by parallel
    // insert / delete.
    *tuple = nullptr;
    return Status::WARN_NOT_FOUND;
  }

  read_set_obj rs_ob(storage, rec_ptr);
  Status rr = read_record(rs_ob.get_rec_read(), rec_ptr);
  if (rr == Status::OK) {
    ti->get_read_set().emplace_back(std::move(rs_ob));
    *tuple = &ti->get_read_set().back().get_rec_read().get_tuple();
  }
  return rr;
}

}  // namespace ccbench
