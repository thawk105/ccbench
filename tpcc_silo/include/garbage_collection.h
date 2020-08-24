/**
 * @file garbage_collection.h
 * @brief about garbage collection
 */

#pragma once

#include <array>
#include <mutex>
#include <string>
#include <utility>
#include <deque>

#include "cpu.h"
#include "epoch.h"
#include "record.h"

namespace ccbench::garbage_collection {

using RecPtrContainer = std::deque<Record *>;

using ObjInfo = std::tuple<void *, std::size_t, std::align_val_t>;
constexpr std::size_t ptr_index = 0;
constexpr std::size_t size_index = 1;
constexpr std::size_t align_index = 2;


inline void delete_object(ObjInfo oinfo)
{
    ::operator delete(std::get<ptr_index>(oinfo),
                      std::get<size_index>(oinfo),
                      std::get<align_index>(oinfo));
}


using ObjEpochInfo = std::pair<ObjInfo, epoch::epoch_t>;
using ObjEpochContainer = std::deque<ObjEpochInfo>;


alignas(CACHE_LINE_SIZE) inline std::array< // NOLINT
    RecPtrContainer, KVS_NUMBER_OF_LOGICAL_CORES> kGarbageRecords; // NOLINT
alignas(CACHE_LINE_SIZE) inline std::array< // NOLINT
    ObjEpochContainer, KVS_NUMBER_OF_LOGICAL_CORES> kGarbageValues; // NOLINT

/**
 * @brief Delete RecPtrContainer kGarbageRecords at
 * ../gcollection.cc
 * @pre This function should be called at terminating db.
 * @return void
 */
extern void delete_all_garbage_records();

/**
 * @brief Delete first of ObjEpochContainer kGarbageValues at
 * ../gcollection.cc
 * @pre This function should be called at terminating db.
 * @return void
 */
extern void delete_all_garbage_values();

[[maybe_unused]] static RecPtrContainer &get_garbage_records_at(std::size_t index) {  // NOLINT
  return kGarbageRecords.at(index);
}

[[maybe_unused]] static ObjEpochContainer &get_garbage_values_at(std::size_t index) {  // NOLINT
  return kGarbageValues.at(index);
}

/**
 * @brief Release all heap objects in this system.
 * @details Do three functions: delete_all_garbage_values(),
 * delete_all_garbage_records(), and remove_all_leaf_from_mt_db_and_release().
 * @pre This function should be called at terminating db.
 * @return void
 */
[[maybe_unused]] extern void release_all_heap_objects();

/**
 * @brief Remove all leaf nodes from MTDB and release those heap objects.
 * @pre This function should be called at terminating db.
 * @return void
 */
extern void remove_all_leaf_from_mt_db_and_release();

}  // namespace ccbench::garbage_collection
