# Protocols

Each directory below lives under [cc/](../cc/) and has a single-call
`CMakeLists.txt` (see [cmake/ProtocolHelpers.cmake](../cmake/ProtocolHelpers.cmake)
for the declarative helper). All are driven from the top-level
[CMakeLists.txt](../CMakeLists.txt). Build instructions are in
[build.md](build_en.md).

| Directory | Protocol | YCSB | TPC-C | BoMB | Reference |
|---|---|:-:|:-:|:-:|---|
| [cc/silo](../cc/silo/)     | Silo                                | ✓ | ✓ | ✓ | Tu et al., SOSP 2013 |
| [cc/cicada](../cc/cicada/) | Cicada                              | ✓ | ✓ | ✓ | Lim et al., SIGMOD 2017 |
| [cc/mocc](../cc/mocc/)     | MOCC                                | ✓ | ✓ | ✓ | Wang et al., VLDB 2017 |
| [cc/tictoc](../cc/tictoc/) | TicToc                              | ✓ | ✓ | ✓ | Yu et al., SIGMOD 2016 |
| [cc/ermia](../cc/ermia/)   | ERMIA (with SSN / latch-free SSN)   | ✓ | ✓ | ✓ | Kim et al., SIGMOD 2016; Wang et al., VLDB 2017 |
| [cc/oze](../cc/oze/)       | Oze                                 | ✓ | ✓ | ✓ | Multi-version OCC variant |
| [cc/si](../cc/si/)         | Snapshot Isolation                  | ✓ | ✓ | ✓ | ERMIA without the SSN layer (no anti-dependency check) |
| [cc/ss2pl](../cc/ss2pl/)   | Strong Strict 2-Phase Locking       | ✓ | ✓ | ✓ | Baseline locking |
| [cc/d2pl](../cc/d2pl/)     | Deterministic 2PL                   | — | — | ✓ | sBoMB and dBoMB only; pre-declared lock entries make TPC-C templates inapplicable |
| [cc/mvto](../cc/mvto/)     | Multi-Version Timestamp Ordering    | — | ✓ | ✓ | Reed, 1978 |

`cc/si` was rewritten by stripping the SSN layer from `cc/ermia` (the
original `si/` was on the legacy uint64-key API and excluded from the
build). `cc/d2pl` deliberately doesn't support TPC-C — its deterministic
locking model needs pre-declared `lock_entries_`, which the optimistic-style
TPC-C templates don't provide.

## sBoMB / dBoMB support

Each protocol's support for sBoMB (single-threaded BoMB) and dBoMB
(deterministic BoMB) is in [build/PROTOCOL_MATRIX.md](../build/PROTOCOL_MATRIX.md),
which is **auto-generated** at CMake configure time from the `WORKLOADS`
argument of every `ccbench_add_protocol(...)` call. The current snapshot:

| Protocol | YCSB | TPC-C | BoMB | sBoMB | dBoMB |
|---|:-:|:-:|:-:|:-:|:-:|
| `cc/silo/`   | ✓ | ✓ | ✓ | ✓ | — |
| `cc/mocc/`   | ✓ | ✓ | ✓ | ✓ | — |
| `cc/cicada/` | ✓ | ✓ | ✓ | ✓ | — |
| `cc/ermia/`  | ✓ | ✓ | ✓ | ✓ | — |
| `cc/tictoc/` | ✓ | ✓ | ✓ | ✓ | — |
| `cc/oze/`    | ✓ | ✓ | ✓ | — | — |
| `cc/si/`     | ✓ | ✓ | ✓ | ✓ | — |
| `cc/ss2pl/`  | ✓ | ✓ | ✓ | — | — |
| `cc/mvto/`   | — | ✓ | ✓ | — | — |
| `cc/d2pl/`   | — | — | ✓ | ✓ | ✓ |

**Don't hand-edit a copy of this table** — re-run cmake and check
`build/PROTOCOL_MATRIX.md` if you need an up-to-date snapshot.

## Adding or tuning a protocol

A protocol's CMakeLists.txt is a single function call:

```cmake
ccbench_add_protocol(<name>
  SOURCES   <shared .cc files>                # do NOT list workload entry points
  WORKLOADS ycsb tpcc bomb sbomb              # any subset of these tags
  OPTIONS                                     # protocol-specific -D defines
    FOO=${CCBENCH_FOO}
    BAR                                       # bare flag = `-DBAR` (no value)
)
```

Universal `-D` flags (`KEY_SIZE`, `VAL_SIZE`, `BACK_OFF`, `ADD_ANALYSIS`,
`MASSTREE_USE`) and the link against `ccbench_common` +
`ccbench::masstree` + `ccbench::mimalloc` are added automatically. New
cache options go in [cmake/Options.cmake](../cmake/Options.cmake). An
`OPTIONS` entry whose value is empty is dropped — same as the old
`remove_definitions(-DFLAG)` path.

After adding a new protocol directory, list it in the `foreach(_proto …)`
loop in the top-level [CMakeLists.txt](../CMakeLists.txt).
