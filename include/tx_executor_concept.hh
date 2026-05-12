#pragma once

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
 *
 * Why SFINAE rather than a C++20 `concept`?
 *  The `<masstree>` header used pervasively here calls `std::allocator::
 *  construct/destroy`, which were removed in C++20. We can't bump the
 *  whole tree to C++20 until that upstream is patched, so we express the
 *  same contract with an `is_detected`-style helper that works in C++17.
 */

namespace ccbench::detail {

template <class, class = void> struct void_t_helper { using type = void; };

// is_detected: succeeds when EXPR is well-formed for type T.
#define CCBENCH_CONTRACT_HAS(NAME, EXPR)                                 \
  template <class T, class = void>                                       \
  struct NAME : std::false_type {};                                      \
  template <class T>                                                     \
  struct NAME<T, std::void_t<decltype(EXPR)>> : std::true_type {}

CCBENCH_CONTRACT_HAS(
    has_read,
    std::declval<T&>().read(std::declval<Storage>(),
                            std::declval<std::string_view>(),
                            std::declval<TupleBody**>()));

CCBENCH_CONTRACT_HAS(
    has_update,
    std::declval<T&>().update(std::declval<Storage>(),
                              std::declval<std::string_view>(),
                              std::declval<TupleBody&&>()));

CCBENCH_CONTRACT_HAS(
    has_insert,
    std::declval<T&>().insert(std::declval<Storage>(),
                              std::declval<std::string_view>(),
                              std::declval<TupleBody&&>()));

CCBENCH_CONTRACT_HAS(
    has_delete_record,
    std::declval<T&>().delete_record(std::declval<Storage>(),
                                     std::declval<std::string_view>()));

CCBENCH_CONTRACT_HAS(
    has_scan,
    std::declval<T&>().scan(std::declval<Storage>(),
                            std::declval<std::string_view>(), false,
                            std::declval<std::string_view>(), false,
                            std::declval<std::vector<TupleBody*>&>()));

CCBENCH_CONTRACT_HAS(
    has_scan_limit,
    std::declval<T&>().scan(std::declval<Storage>(),
                            std::declval<std::string_view>(), false,
                            std::declval<std::string_view>(), false,
                            std::declval<std::vector<TupleBody*>&>(),
                            std::declval<std::int64_t>()));

CCBENCH_CONTRACT_HAS(has_commit, std::declval<T&>().commit());
CCBENCH_CONTRACT_HAS(has_abort,  std::declval<T&>().abort());

#undef CCBENCH_CONTRACT_HAS

}  // namespace ccbench::detail

template <class T>
inline constexpr bool TxExecutorLike =
    ccbench::detail::has_read<T>::value &&
    ccbench::detail::has_update<T>::value &&
    ccbench::detail::has_insert<T>::value &&
    ccbench::detail::has_delete_record<T>::value &&
    ccbench::detail::has_scan<T>::value &&
    ccbench::detail::has_scan_limit<T>::value &&
    ccbench::detail::has_commit<T>::value &&
    ccbench::detail::has_abort<T>::value;
