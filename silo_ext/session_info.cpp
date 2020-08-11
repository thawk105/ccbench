/**
 * @file session_info.cpp
 * @brief about scheme
 */

#include "include/session_info.h"

#include "garbage_collection.h"
#include "index/masstree_beta/include/masstree_beta_wrapper.h"

namespace ccbench {

void session_info::clean_up_ops_set() {
  read_set.clear();
  write_set.clear();
}

void session_info::clean_up_scan_caches() {
  scan_handle_.get_scan_cache().clear();
  scan_handle_.get_scan_cache_itr().clear();
  scan_handle_.get_rkey().clear();
  scan_handle_.get_len_rkey().clear();
  scan_handle_.get_r_exclusive_().clear();
}

[[maybe_unused]] void session_info::display_read_set() {
  std::cout << "==========" << std::endl;
  std::cout << "start : session_info::display_read_set()" << std::endl;
  std::size_t ctr(1);
  for (auto &&itr : read_set) {
    std::cout << "Element #" << ctr << " of read set." << std::endl;
    std::cout << "rec_ptr_ : " << itr.get_rec_ptr() << std::endl;
    Record &record = itr.get_rec_read();
    Tuple &tuple = record.get_tuple();
    std::cout << "tidw_ :vv" << record.get_tidw() << std::endl;
    std::string_view key_view;
    std::string_view value_view;
    key_view = tuple.get_key();
    value_view = tuple.get_value();
    std::cout << "key : " << key_view << std::endl;
    std::cout << "key_size : " << key_view.size() << std::endl;
    std::cout << "value : " << value_view << std::endl;
    std::cout << "value_size : " << value_view.size() << std::endl;
    std::cout << "----------" << std::endl;
    ++ctr;
  }
  std::cout << "==========" << std::endl;
}

[[maybe_unused]] void session_info::display_write_set() {
  std::cout << "==========" << std::endl;
  std::cout << "start : session_info::display_write_set()" << std::endl;
  std::size_t ctr(1);
  for (auto &&itr : write_set) {
    std::cout << "Element #" << ctr << " of write set." << std::endl;
    std::cout << "rec_ptr_ : " << itr.get_rec_ptr() << std::endl;
    std::cout << "op_ : " << itr.get_op() << std::endl;
    std::string_view key_view;
    std::string_view value_view;
    key_view = itr.get_tuple().get_key();
    value_view = itr.get_tuple().get_value();
    std::cout << "key : " << key_view << std::endl;
    std::cout << "key_size : " << key_view.size() << std::endl;
    std::cout << "value : " << value_view << std::endl;
    std::cout << "value_size : " << value_view.size() << std::endl;
    std::cout << "----------" << std::endl;
    ++ctr;
  }
  std::cout << "==========" << std::endl;
}

Status session_info::check_delete_after_write(std::string_view key) {  // NOLINT
  for (auto itr = write_set.begin(); itr != write_set.end(); ++itr) {
    // It can't use lange-based for because it use write_set.erase.
    std::string_view key_view = itr->get_rec_ptr()->get_tuple().get_key();
    if (key_view.size() == key.size() &&
        memcmp(key_view.data(), key.data(), key.size()) == 0) {
      write_set.erase(itr);
      return Status::WARN_CANCEL_PREVIOUS_OPERATION;
    }
  }

  return Status::OK;
}

void session_info::gc_records_and_values() const {
  // for records
  {
    std::mutex &mutex_for_gc_list =
            garbage_collection::get_mutex_garbage_records_at(
                    this->gc_handle_.get_container_index());
    if (mutex_for_gc_list.try_lock()) {
      auto itr = this->gc_handle_.get_record_container()->begin();
      while (itr != this->gc_handle_.get_record_container()->end()) {
        if ((*itr)->get_tidw().get_epoch() <= epoch::get_reclamation_epoch()) {
          delete *itr;  // NOLINT
          itr = this->gc_handle_.get_record_container()->erase(itr);
        } else {
          break;
        }
      }
      mutex_for_gc_list.unlock();
    }
  }
  // for values
  {
    std::mutex &mutex_for_gc_list =
            garbage_collection::get_mutex_garbage_values_at(
                    this->gc_handle_.get_container_index());
    if (mutex_for_gc_list.try_lock()) {
      auto itr = this->gc_handle_.get_value_container()->begin();
      while (itr != this->gc_handle_.get_value_container()->end()) {
        if (itr->second <= epoch::get_reclamation_epoch()) {
          delete itr->first;  // NOLINT
          itr = this->gc_handle_.get_value_container()->erase(itr);
        } else {
          break;
        }
      }
      mutex_for_gc_list.unlock();
    }
  }
}

void session_info::remove_inserted_records_of_write_set_from_masstree() {
  for (auto &&itr : write_set) {
    if (itr.get_op() == OP_TYPE::INSERT) {
      Record *record = itr.get_rec_ptr();
      std::string_view key_view = record->get_tuple().get_key();
#ifdef INDEX_KOHLER_MASSTREE
      kohler_masstree::get_mtdb().remove_value(key_view.data(),
                                               key_view.size());
#elif INDEX_YAKUSHIMA
      yakushima::yakushima_kvs::remove(get_yakushima_token(), key_view);
#endif

      /**
       * create information for garbage collection.
       */
      std::mutex &mutex_for_gclist =
              garbage_collection::get_mutex_garbage_records_at(
                      gc_handle_.get_container_index());
      mutex_for_gclist.lock();
      gc_handle_.get_record_container()->emplace_back(itr.get_rec_ptr());
      mutex_for_gclist.unlock();
      tid_word deletetid;
      deletetid.set_lock(false);
      deletetid.set_latest(false);
      deletetid.set_absent(false);
      deletetid.set_epoch(this->get_epoch());
      storeRelease(record->get_tidw().obj_, deletetid.obj_);  // NOLINT
    }
  }
}

read_set_obj *session_info::search_read_set(std::string_view key) {  // NOLINT
  for (auto &&itr : read_set) {
    const std::string_view key_view = itr.get_rec_ptr()->get_tuple().get_key();
    if (key_view.size() == key.size() &&
        memcmp(key_view.data(), key.data(), key.size()) == 0) {
      return &itr;
    }
  }
  return nullptr;
}

read_set_obj *session_info::search_read_set(  // NOLINT
        const Record *const rec_ptr) {
  for (auto &&itr : read_set) {
    if (itr.get_rec_ptr() == rec_ptr) return &itr;
  }

  return nullptr;
}

write_set_obj *session_info::search_write_set(std::string_view key) {
  for (auto &&itr : write_set) {
    const Tuple *tuple;  // NOLINT
    if (itr.get_op() == OP_TYPE::UPDATE) {
      tuple = &itr.get_tuple_to_local();
    } else {
      // insert/delete
      tuple = &itr.get_tuple_to_db();
    }
    std::string_view key_view = tuple->get_key();
    if (key_view.size() == key.size() &&
        memcmp(key_view.data(), key.data(), key.size()) == 0) {
      return &itr;
    }
  }
  return nullptr;
}

const write_set_obj *session_info::search_write_set(  // NOLINT
        const Record *rec_ptr) {
  for (auto &itr : write_set) {
    if (itr.get_rec_ptr() == rec_ptr) return &itr;
  }

  return nullptr;
}

void session_info::unlock_write_set() {
  tid_word expected{};
  tid_word desired{};

  for (auto &itr : write_set) {
    Record *recptr = itr.get_rec_ptr();
    expected = loadAcquire(recptr->get_tidw().obj_);  // NOLINT
    desired = expected;
    desired.set_lock(false);
    storeRelease(recptr->get_tidw().obj_, desired.obj_);  // NOLINT
  }
}

void session_info::unlock_write_set(  // NOLINT
        std::vector<write_set_obj>::iterator begin,
        std::vector<write_set_obj>::iterator end) {
  tid_word expected;
  tid_word desired;

  for (auto itr = begin; itr != end; ++itr) {
    expected = loadAcquire(itr->get_rec_ptr()->get_tidw().obj_);  // NOLINT
    desired = expected;
    desired.set_lock(false);
    storeRelease(itr->get_rec_ptr()->get_tidw().obj_, desired.obj_);  // NOLINT
  }
}

void session_info::wal(uint64_t commit_id) {
  for (auto &&itr : write_set) {
    if (itr.get_op() == OP_TYPE::UPDATE) {
      log_handle_.get_log_set().emplace_back(commit_id, itr.get_op(),
                                             &itr.get_tuple_to_local());
    } else {
      // insert/delete
      log_handle_.get_log_set().emplace_back(commit_id, itr.get_op(),
                                             &itr.get_tuple_to_db());
    }
    log_handle_.get_latest_log_header().add_checksum(
            log_handle_.get_log_set().back().compute_checksum());  // NOLINT
    log_handle_.get_latest_log_header().inc_log_rec_num();
  }

  /**
   * This part includes many write system call.
   * Future work: if this degrades the system performance, it should prepare
   * some buffer (like char*) and do memcpy instead of write system call
   * and do write system call in a batch.
   */
  if (log_handle_.get_log_set().size() > KVS_LOG_GC_THRESHOLD) {
    // prepare write header
    log_handle_.get_latest_log_header().compute_two_complement_of_checksum();

    // write header
    log_handle_.get_log_file().write(
            static_cast<void *>(&log_handle_.get_latest_log_header()),
            sizeof(Log::LogHeader));

    // write log record
    for (auto &&itr : log_handle_.get_log_set()) {
      // write tx id, op(operation type)
      log_handle_.get_log_file().write(
              static_cast<void *>(&itr),
              sizeof(itr.get_tid()) + sizeof(itr.get_op()));

      // common subexpression elimination
      const Tuple *tupleptr = itr.get_tuple();

      std::string_view key_view = tupleptr->get_key();
      // write key_length
      // key_view.size() returns constexpr.
      std::size_t key_size = key_view.size();
      log_handle_.get_log_file().write(static_cast<void *>(&key_size),
                                       sizeof(key_size));

      // write key_body
      log_handle_.get_log_file().write(
              static_cast<const void *>(key_view.data()),
              key_size);  // NOLINT

      std::string_view value_view = tupleptr->get_value();
      // write value_length
      // value_view.size() returns constexpr.
      std::size_t value_size = value_view.size();
      log_handle_.get_log_file().write(
              static_cast<const void *>(value_view.data()),
              value_size);  // NOLINT

      // write val_body
      if (itr.get_op() != OP_TYPE::DELETE) {
        if (value_size != 0) {
          log_handle_.get_log_file().write(
                  static_cast<const void *>(value_view.data()),
                  value_size);  // NOLINT
        }
      }
    }
  }

  log_handle_.get_latest_log_header().init();
  log_handle_.get_log_set().clear();
}

}  // namespace ccbench
