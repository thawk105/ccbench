#pragma once

#include <concepts>
#include <cstdint>
#include <string_view>
#include <type_traits>
#include <vector>

#include "status.hh"
#include "tuple_body.hh"
#include "workload.hh"  // for `enum class Storage`

/**
 * @brief Compile-time contract every protocol's `TxExecutor` must satisfy.
 *
 * Each protocol pins down its `TxExecutor` with
 *
 *   static_assert(TxExecutorLike<TxExecutor>);
 *
 * right after the class definition (see `<protocol>/include/transaction.hh`).
 * A new protocol that forgets — for example — the `int64_t limit` overload of
 * `scan` (a real bug that has happened multiple times here, breaking TPC-C
 * delivery's `get_order_id`) will fail at the protocol's own compile, with a
 * named diagnostic, instead of crashing at runtime when a workload happens
 * to call it.
 *
 * Notes:
 *  - `update` (formerly `write`) is the SQL-UPDATE shaped operation: it
 *    requires the row to already exist and returns `WARN_NOT_FOUND` otherwise.
 *    `insert` is the SQL-INSERT shaped operation that creates a new row.
 *  - Both `scan` overloads are required.
 *  - The contract intentionally captures only the workload-template surface.
 *    Per-protocol additions (epoch_timer_start, leaderWork, isLeader,
 *    reconnoiter_*) are deliberately not constrained, so the same concept
 *    applies cleanly across optimistic / pessimistic / deterministic
 *    families.
 */
template <class T>
concept TxExecutorLike = requires(
    T t,
    Storage s,
    std::string_view k,
    TupleBody** body,
    std::vector<TupleBody*>& result) {
  { t.read(s, k, body) }                                    -> std::same_as<Status>;
  { t.update(s, k, std::declval<TupleBody&&>()) }           -> std::same_as<Status>;
  { t.insert(s, k, std::declval<TupleBody&&>()) }           -> std::same_as<Status>;
  { t.delete_record(s, k) }                                 -> std::same_as<Status>;
  { t.scan(s, k, false, k, false, result) }                 -> std::same_as<Status>;
  { t.scan(s, k, false, k, false, result, std::int64_t{}) } -> std::same_as<Status>;
  { t.commit() }                                            -> std::same_as<bool>;
  { t.abort() }                                             -> std::same_as<void>;
};
