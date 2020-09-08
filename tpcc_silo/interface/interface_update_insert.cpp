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

namespace {


write_set_obj* prepare_insert_or_update_or_upsert(Token token, Storage storage, std::string_view key, session_info*& ti)
{
  ti = static_cast<session_info *>(token);
  if (!ti->get_txbegan()) tx_begin(token);
  return ti->search_write_set(storage, key);
}


template <typename KeyFunc, typename TupleFunc, typename ObjFunc>
Status insert_detail(
    Token token, Storage st,
    KeyFunc&& key_func, TupleFunc&& tuple_func, ObjFunc&& obj_func,
    Tuple** tuple_out)
{
  session_info *ti;
  write_set_obj* inws = prepare_insert_or_update_or_upsert(token, st, key_func(), ti);
  if (inws != nullptr) {
    inws->reset_tuple_value(obj_func());
    if (tuple_out != nullptr) *tuple_out = &inws->get_tuple();
    return Status::WARN_WRITE_TO_LOCAL_WRITE;
  }

  masstree_wrapper<Record>::thread_init(cached_sched_getcpu());
  if (kohler_masstree::find_record(st, key_func()) != nullptr) {
    return Status::WARN_ALREADY_EXISTS;
  }

  Record *rec = new Record(tuple_func());
  assert(rec != nullptr);
  Status rr = kohler_masstree::insert_record(st, rec->get_tuple().get_key(), rec);
  if (rr != Status::OK) {
      delete rec;  // NOLINT
      return Status::WARN_ALREADY_EXISTS;
  }
  auto& ws = ti->get_write_set();
  ws.emplace_back(OP_TYPE::INSERT, st, rec);
  if (tuple_out != nullptr) *tuple_out = &ws.back().get_tuple();
  return Status::OK;
}


} // unnamed namepsace


/**
 * insert by copy.
 */
Status insert(
    Token token, Storage st,
    std::string_view key, std::string_view val, std::align_val_t val_align,
    Tuple** tuple_out)
{
  return insert_detail(
    token, st,
    [&]() -> std::string_view { return key; },
    [&]() -> Tuple { return Tuple(key, HeapObject(val.data(), val.size(), val_align)); },
    [&]() -> HeapObject { return HeapObject(val.data(), val.size(), val_align); },
    tuple_out);
}


/**
 * insert by move.
 */
Status insert(Token token, Storage st, Tuple&& tuple, Tuple** tuple_out)
{
  return insert_detail(
    token, st,
    [&]() -> std::string_view { return tuple.get_key(); },
    [&]() -> Tuple { return std::move(tuple); },
    [&]() -> HeapObject { return std::move(tuple.get_value()); },
    tuple_out);
}


namespace {

template <typename KeyFunc, typename TupleFunc, typename ObjFunc>
Status update_detail(
    Token token, Storage st,
    KeyFunc&& key_func, TupleFunc&& tuple_func, ObjFunc&& obj_func,
    Tuple** tuple_out)
{
  session_info *ti;
  write_set_obj* inws = prepare_insert_or_update_or_upsert(token, st, key_func(), ti);
  if (inws != nullptr) {
    inws->reset_tuple_value(obj_func());
    if (tuple_out != nullptr) *tuple_out = &inws->get_tuple();
    return Status::WARN_WRITE_TO_LOCAL_WRITE;
  }

  Record* rec_ptr;
  masstree_wrapper<Record>::thread_init(cached_sched_getcpu());
  rec_ptr = kohler_masstree::get_mtdb(st).get_value(key_func());
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

  auto& ws = ti->get_write_set();
  ws.emplace_back(OP_TYPE::UPDATE, st, rec_ptr, tuple_func());
  if (tuple_out != nullptr) *tuple_out = &ws.back().get_tuple();
  return Status::OK;
}

} // unnamed namespace


Status update(
    Token token, Storage st,
    std::string_view key, std::string_view val, std::align_val_t val_align,
    Tuple** tuple_out)
{
  return update_detail(
    token, st,
    [&]() -> std::string_view { return key; },
    [&]() -> Tuple { return Tuple(key, HeapObject(val.data(), val.size(), val_align)); },
    [&]() -> HeapObject { return HeapObject(val.data(), val.size(), val_align); },
    tuple_out);
}


Status update(Token token, Storage st, Tuple&& tuple, Tuple** tuple_out)
{
  return update_detail(
    token, st,
    [&]() -> std::string_view { return tuple.get_key(); },
    [&]() -> Tuple { return std::move(tuple); },
    [&]() -> HeapObject { return std::move(tuple.get_value()); },
    tuple_out);
}


namespace {


template <typename KeyFunc, typename TupleFunc, typename ObjFunc>
Status upsert_detail(Token token, Storage st, KeyFunc&& key_func, TupleFunc&& tuple_func, ObjFunc&& obj_func)
{
  session_info *ti;
  write_set_obj* inws = prepare_insert_or_update_or_upsert(token, st, key_func(), ti);
  if (inws != nullptr) {
    inws->reset_tuple_value(obj_func());
    return Status::WARN_WRITE_TO_LOCAL_WRITE;
  }

  masstree_wrapper<Record>::thread_init(cached_sched_getcpu());
  Record *rec_ptr{static_cast<Record *>(kohler_masstree::kohler_masstree::find_record(st, key_func()))};
  if (rec_ptr == nullptr) {
    rec_ptr = new Record(tuple_func());
    Status insert_result = kohler_masstree::insert_record(st, rec_ptr->get_tuple().get_key(), rec_ptr);
    if (insert_result == Status::OK) {
      ti->get_write_set().emplace_back(OP_TYPE::INSERT, st, rec_ptr);
      return Status::OK;
    }
    // else insert_result == Status::WARN_ALREADY_EXISTS
    // so goto update.
    delete rec_ptr;  // NOLINT
  }

  ti->get_write_set().emplace_back(OP_TYPE::UPDATE, st, rec_ptr, tuple_func());

  return Status::OK;
}


} // unnamed namespace


/**
 * Upsert by copy.
 */
Status upsert(Token token, Storage st, std::string_view key, std::string_view val, std::align_val_t val_align)
{
  return upsert_detail(
    token, st,
    [&]() -> std::string_view { return key; },
    [&]() -> Tuple { return Tuple(key, HeapObject(val.data(), val.size(), val_align)); },
    [&]() -> HeapObject { return HeapObject(val.data(), val.size(), val_align); });
}


/**
 * Upsert by move.
 */
Status upsert(Token token, Storage st, Tuple&& tuple)
{
  return upsert_detail(
    token, st,
    [&]() -> std::string_view { return tuple.get_key(); },
    [&]() -> Tuple { return std::move(tuple); },
    [&]() -> HeapObject { return std::move(tuple.get_value()); });
}


}  // namespace ccbench
