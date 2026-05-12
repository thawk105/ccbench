# Protocols

Each directory below lives under [cc/](../cc/) and has a single-call
`CMakeLists.txt` (see [cmake/ProtocolHelpers.cmake](../cmake/ProtocolHelpers.cmake)
for the declarative helper). All are driven from the top-level
[CMakeLists.txt](../CMakeLists.txt). Build instructions are in
[build.md](build.md).

| Directory | Protocol | YCSB | TPC-C | BoMB | Reference |
|---|---|:-:|:-:|:-:|---|
| [cc/silo](../cc/silo/)     | Silo                                | ✓ | ✓ | ✓ | Tu et al., SOSP 2013 |
| [cc/cicada](../cc/cicada/) | Cicada                              | ✓ | ✓ | ✓ | Lim et al., SIGMOD 2017 |
| [cc/mocc](../cc/mocc/)     | MOCC                                | ✓ | ✓ | ✓ | Wang et al., VLDB 2017 |
| [cc/tictoc](../cc/tictoc/) | TicToc                              | ✓ | ✓ | ✓ | Yu et al., SIGMOD 2016 |
| [cc/ermia](../cc/ermia/)   | ERMIA (with SSN / latch-free SSN)   | ✓ | ✓ | ✓ | Kim et al., SIGMOD 2016; Wang et al., VLDB 2017 |
| [cc/oze](../cc/oze/)       | Oze                                 | ✓ | ✓ | ✓ | Multi-version OCC variant |
| [cc/si](../cc/si/)         | Snapshot Isolation                  | ✓ | ✓ | ✓ | ERMIA without the SSN layer (no anti-dependency check) |
| [cc/ss2pl](../cc/ss2pl/)   | Strong Strict 2-Phase Locking       | — | ✓ | ✓ | Baseline locking |
| [cc/d2pl](../cc/d2pl/)     | Deterministic 2PL                   | — | — | ✓ | sBoMB and dBoMB only; pre-declared lock entries make TPC-C templates inapplicable |
| [cc/mvto](../cc/mvto/)     | Multi-Version Timestamp Ordering    | — | ✓ | ✓ | Reed, 1978 |

`cc/si` was rewritten by stripping the SSN layer from `cc/ermia` (the
original `si/` was on the legacy uint64-key API and excluded from the
build).

## Not built by default

| Directory | Notes |
|---|---|
| [cc/occ](../cc/occ/) | K&R OCC (Kung & Robinson, 1981). Uses a plain Makefile (on the legacy uint64-key API), not wired into the CMake tree. |

To enable, port `cc/occ/` to a `ccbench_add_protocol(...)` entry and add
it to the `foreach(_proto …)` loop in the top-level
[CMakeLists.txt](../CMakeLists.txt).
