# CCBench: Concurrency Control Protocol Workbench

![build](https://github.com/thawk105/ccbench/workflows/build/badge.svg)

CCBench re-implements major in-memory concurrency-control protocols on a common substrate so they can be compared on identical workloads. The original analysis paper is [Tanabe et al., VLDB 2020][1].

The repository now also bundles the **TPC-C** and **BoMB** workloads, and several additional protocols, contributed by [@jnmt](https://github.com/jnmt) on the [`vldb-paper`](https://github.com/jnmt/ccbench/tree/vldb-paper) branch and merged here.

| Protocol | YCSB | TPC-C | BoMB |
|---|:-:|:-:|:-:|
| Silo, MOCC, Cicada, ERMIA, TicToc, Oze | ✓ | ✓ | ✓ |
| SI | ✓ | △ | ✓ |
| SS2PL, MVTO | — | ✓ | ✓ |
| D2PL | — | — | ✓ |

(SI's TPC-C is marked △ because multi-thread Release runs hit a
pre-existing UB inherited from ERMIA; see [docs/protocols.md](docs/protocols.md).)

See [docs/workloads.md](docs/workloads.md) for the workload specs (tables, transactions, parameters) and [docs/protocols.md](docs/protocols.md) for the full protocol matrix.

[1]: http://www.vldb.org/pvldb/vol13/p3531-tanabe.pdf

## Quick start (devcontainer)

The repo ships with a `linux/amd64` devcontainer at [.devcontainer/](.devcontainer/) that pulls a prebuilt image from GHCR. In VS Code, run **`Dev Containers: Reopen in Container`** — submodules and apt deps are set up automatically.

Then inside the container:

```sh
./build_tools/bootstrap.sh             # masstree
./build_tools/bootstrap_mimalloc.sh    # mimalloc
./build_tools/bootstrap_googletest.sh  # googletest
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

> On Apple Silicon the container runs under QEMU emulation: fine for development, **not** for benchmark numbers. Use a real x86_64 Linux host for measurements. See [docs/build.md](docs/build.md) for the full host setup.

## Run

Binaries land at `build/<protocol>/<workload>_<protocol>.exe`. Examples:

```sh
./build/silo/tpcc_silo.exe   -thread_num=8 -tpcc_num_wh=8
./build/cicada/ycsb_cicada.exe -thread_num=8 -ycsb_rratio=50
./build/silo/bomb_silo.exe   -thread_num=8 -bomb_mixed_mode -bomb_mixed_short_rate=1000
```

See `--help` for workload-specific flags, or [docs/runtime-args.md](docs/runtime-args.md) for the full reference.

## Documentation

- [docs/build.md](docs/build.md) — host & devcontainer build instructions
- [docs/workloads.md](docs/workloads.md) — TPC-C / YCSB / BoMB specs
- [docs/protocols.md](docs/protocols.md) — protocol matrix and references
- [docs/runtime-args.md](docs/runtime-args.md) — runtime flag reference
- [CLAUDE.md](CLAUDE.md) — repo-level context for AI assistants
- Original CCBench experimental data: <https://github.com/thawk105/ccdata>

## Acknowledgments

Takayuki Tanabe extends thanks to:

- Cybozu Labs Youth (8th term, 2018-04-10 – 2019-04-10) for supporting this work.
- Takashi Hoshino, advisor at Cybozu Labs Youth.
- Hideyuki Kawashima and Osamu Tatebe, supervisors.

## References

```
[1] Takayuki Tanabe, Takashi Hoshino, Hideyuki Kawashima, and Osamu Tatebe. 2020.
    An analysis of concurrency control protocols for in-memory databases with CCBench.
    Proc. VLDB Endow. 13, 13 (September 2020), 3531–3544.
    https://doi.org/10.14778/3424573.3424575
```
