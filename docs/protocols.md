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
| [ss2pl](../ss2pl/)   | Strong Strict 2-Phase Locking       | — | ✓ | ✓ | Baseline locking |
| [d2pl](../d2pl/)     | Deterministic 2PL                   | — | — | ✓ | sBoMB and dBoMB only; pre-declared lock entries make TPC-C templates inapplicable |
| [mvto](../mvto/)     | Multi-Version Timestamp Ordering    | — | ✓ | ✓ | Reed, 1978 |

## Not built by default

These directories are present in the tree but commented out in the top-level
`CMakeLists.txt`:

| Directory | Notes |
|---|---|
| [si](../si/) | Snapshot Isolation. Was a baseline used for ERMIA analysis; build kept for reference. |
| [occ](../occ/) | K&R OCC (Kung & Robinson, 1981). Uses a plain Makefile, not wired into the CMake tree. |

To enable them, uncomment the corresponding `add_subdirectory(...)` line in
the top-level [CMakeLists.txt](../CMakeLists.txt).
