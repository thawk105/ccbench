#pragma once
//
// Izanagi correctness-trace hook for CCBench.
//
// Compiled in ONLY when the build defines TRACE != 0 (set via the CMake cache
// var CCBENCH_TRACE -> target-private -DTRACE=<v>, see cmake/Options.cmake).
// Absolute discipline #1 (observer-effect separation): EVERYTHING in this
// header is guarded by `#if TRACE`, so the trace-disabled (performance) build
// contains none of it -- not the includes, not the buffers, not the I/O.
//
// We use `#if TRACE` (NOT `#ifdef TRACE`) deliberately: the CMake convention
// always defines the macro (-DTRACE=0 for perf builds), so `#ifdef TRACE`
// would be true even in the perf build and leak the observer effect. With
// `#if TRACE`, -DTRACE=0 (or an undefined TRACE) compiles to nothing.
// See izanagi docs/decisions.md D14 and docs/ccbench-anatomy.md  5.
//
// Trace record schema (per-thread file trace_<thid>.log, one event per line):
//   C <txid> <thid> <epoch> <tid>            committed txn, its commit TID
//   R <txid> <key_hex> <ver_epoch> <ver_tid> a read; version (epoch,tid) seen
//   W <txid> <key_hex> <op> <epoch> <tid>    a write; new version = commit TID
// A txn's records are contiguous (C then its R/W lines). The verifier joins a
// read's (key,ver) to the producing txn whose commit (epoch,tid) wrote that
// key, and orders per-key versions by (epoch,tid) to recover ww/wr/rw edges.

#if TRACE

#include <atomic>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <ios>
#include <string>

namespace izanagi_trace {

// Global, monotonically increasing transaction id. Groups a committed txn's
// C/R/W lines together. Silo's commit (epoch,tid) is NOT globally unique
// across non-conflicting txns, so we need a separate id for grouping; the
// (epoch,tid) is still emitted as the per-key version identifier.
inline std::atomic<uint64_t>& txid_counter() {
  static std::atomic<uint64_t> c{0};
  return c;
}
inline uint64_t next_txid() {
  return txid_counter().fetch_add(1, std::memory_order_relaxed);
}

// One append-only file per worker thread. The thread_local ofstream is
// flushed and closed by its destructor when the worker thread exits, so no
// explicit teardown hook is needed. Output dir from $IZANAGI_TRACE_DIR (".").
inline std::ofstream& stream(std::size_t thid) {
  thread_local std::ofstream ofs;
  if (!ofs.is_open()) {
    const char* dir = std::getenv("IZANAGI_TRACE_DIR");
    std::string path = (dir ? std::string(dir) : std::string("."));
    path += "/trace_";
    path += std::to_string(thid);
    path += ".log";
    ofs.open(path, std::ios::out | std::ios::trunc);
  }
  return ofs;
}

// Render an opaque key (raw bytes, may contain NUL; YCSB keys are 8-byte
// big-endian) as lowercase hex so the verifier gets one unambiguous token.
inline std::string key_to_hex(const std::string& key) {
  static const char H[] = "0123456789abcdef";
  std::string out;
  out.reserve(key.size() * 2);
  for (unsigned char c : key) {
    out.push_back(H[c >> 4]);
    out.push_back(H[c & 0x0f]);
  }
  return out;
}

inline void emit_commit(std::size_t thid, std::uint64_t txid,
                        std::uint64_t epoch, std::uint64_t tid) {
  stream(thid) << "C " << txid << ' ' << thid << ' ' << epoch << ' ' << tid
               << '\n';
}

inline void emit_read(std::size_t thid, std::uint64_t txid,
                      const std::string& key_hex, std::uint64_t ver_epoch,
                      std::uint64_t ver_tid) {
  stream(thid) << "R " << txid << ' ' << key_hex << ' ' << ver_epoch << ' '
               << ver_tid << '\n';
}

inline void emit_write(std::size_t thid, std::uint64_t txid,
                       const std::string& key_hex, char op,
                       std::uint64_t epoch, std::uint64_t tid) {
  stream(thid) << "W " << txid << ' ' << key_hex << ' ' << op << ' ' << epoch
               << ' ' << tid << '\n';
}

}  // namespace izanagi_trace

#endif  // TRACE
