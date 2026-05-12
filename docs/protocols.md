# Protocols

Each directory below is an independent implementation with its own
`CMakeLists.txt` and `README.md`, all driven from the top-level
[CMakeLists.txt](../CMakeLists.txt). Build instructions are in
[build.md](build.md).

| Directory | Protocol | YCSB | TPC-C | BoMB | Reference |
|---|---|:-:|:-:|:-:|---|
| [silo](../silo/)     | Silo                                | ✓ | ✓ | ✓ | Tu et al., SOSP 2013 |
| [cicada](../cicada/) | Cicada                              | ✓ | ✓ | ✓ | Lim et al., SIGMOD 2017 |
| [mocc](../mocc/)     | MOCC                                | ✓ | ✓ | ✓ | Wang et al., VLDB 2017 |
| [tictoc](../tictoc/) | TicToc                              | ✓ | ✓ | ✓ | Yu et al., SIGMOD 2016 |
| [ermia](../ermia/)   | ERMIA (with SSN / latch-free SSN)   | ✓ | ✓ | ✓ | Kim et al., SIGMOD 2016; Wang et al., VLDB 2017 |
| [oze](../oze/)       | Oze                                 | ✓ | ✓ | ✓ | Multi-version OCC variant |
| [si](../si/)         | Snapshot Isolation                  | ✓ | ✓ | ✓ | ERMIA without the SSN layer (no anti-dependency check) |
| [ss2pl](../ss2pl/)   | Strong Strict 2-Phase Locking       | — | ✓ | ✓ | Baseline locking |
| [d2pl](../d2pl/)     | Deterministic 2PL                   | — | — | ✓ | sBoMB and dBoMB only; pre-declared lock entries make TPC-C templates inapplicable |
| [mvto](../mvto/)     | Multi-Version Timestamp Ordering    | — | ✓ | ✓ | Reed, 1978 |

`si` was rewritten by stripping the SSN layer from `ermia` (the original
`si/` was on the legacy uint64-key API and excluded from the build).

## Not built by default

| Directory | Notes |
|---|---|
| [occ](../occ/) | K&R OCC (Kung & Robinson, 1981). Uses a plain Makefile, not wired into the CMake tree. |

To enable, uncomment the corresponding `add_subdirectory(...)` line in
the top-level [CMakeLists.txt](../CMakeLists.txt).
