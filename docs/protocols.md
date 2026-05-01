# Protocols

Each directory below is an independent implementation with its own build
files and `README.md`. Build instructions are in [build.md](build.md).

| Directory | Protocol | Reference |
|---|---|---|
| [silo](../silo/) | Silo | Tu et al., SOSP 2013 |
| [cicada](../cicada/) | Cicada | Lim et al., SIGMOD 2017 |
| [mocc](../mocc/) | MOCC | Wang et al., VLDB 2017 |
| [tictoc](../tictoc/) | TicToc | Yu et al., SIGMOD 2016 |
| [ermia](../ermia/) | ERMIA (with SSN / latch-free SSN) | Kim et al., SIGMOD 2016; Wang et al., VLDB 2017 |
| [si](../si/) | Snapshot Isolation | Baseline used for ERMIA analysis |
| [ss2pl](../ss2pl/) | Strong Strict 2-Phase Locking | Baseline locking |
| [occ](../occ/) | Optimistic Concurrency Control | Kung & Robinson, 1981 |
| [tpcc_silo](../tpcc_silo/) | Silo running TPC-C | TPC-C harness over Silo |
