/**
 * @file garbage_collection.cpp
 * @brief about garbage collection.
 */

#include "garbage_collection.h"

#include "index/masstree_beta/include/masstree_beta_wrapper.h"
#include "session_info.h"

namespace ccbench::garbage_collection {

[[maybe_unused]] void release_all_heap_objects() {
  //remove_all_leaf_from_mt_db_and_release();
  delete_all_garbage_records();
  delete_all_garbage_values();
}

void remove_all_leaf_from_mt_db_and_release() {
  std::vector<const Record *> scan_res;
  for (std::size_t i = 0; i < kohler_masstree::db_length; ++i) {
    kohler_masstree::get_mtdb(static_cast<Storage>(i)).scan(nullptr, 0, false, nullptr, 0, false,
                                                            &scan_res, false);  // NOLINT
    for (auto &&itr : scan_res) {
      std::string_view key_view = itr->get_tuple().get_key();
      kohler_masstree::get_mtdb(static_cast<Storage>(i)).remove_value(key_view.data(), key_view.size());
      delete itr;  // NOLINT
    }

    /**
     * check whether index_kohler_masstree::get_mtdb() is empty.
     */
    scan_res.clear();
    kohler_masstree::get_mtdb(static_cast<Storage>(i)).scan(nullptr, 0, false, nullptr, 0, false,
                                                            &scan_res, false);  // NOLINT
    if (!scan_res.empty()) std::abort();
  }
}

void delete_all_garbage_records() {
  for (auto i = 0; i < KVS_NUMBER_OF_LOGICAL_CORES; ++i) {
    for (auto &&itr : get_garbage_records_at(i)) {
      delete itr;  // NOLINT
    }
    get_garbage_records_at(i).clear();
  }
}

void delete_all_garbage_values() {
  for (auto i = 0; i < KVS_NUMBER_OF_LOGICAL_CORES; ++i) {
    for (auto &&itr : get_garbage_values_at(i)) {
      delete itr.first;  // NOLINT
    }
    get_garbage_values_at(i).clear();
  }
}

}  // namespace ccbench::garbage_collection
