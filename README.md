# CCBench: Concurrency Control Protocol Workbench

<details>
<summary>CI badges</summary>

![cicada_build](https://github.com/thawk105/ccbench/workflows/cicada_build/badge.svg)
![ermia_build](https://github.com/thawk105/ccbench/workflows/ermia_build/badge.svg)
![mocc_build](https://github.com/thawk105/ccbench/workflows/mocc_build/badge.svg)
![si_build](https://github.com/thawk105/ccbench/workflows/si_build/badge.svg)
![silo_build](https://github.com/thawk105/ccbench/workflows/silo_build/badge.svg)
![ss2pl_build](https://github.com/thawk105/ccbench/workflows/ss2pl_build/badge.svg)
![tictoc_build](https://github.com/thawk105/ccbench/workflows/tictoc_build/badge.svg)
![tpcc_silo_build_and_ctest](https://github.com/thawk105/ccbench/workflows/tpcc_silo_build_and_ctest/badge.svg)

</details>

CCBench is a workbench that re-implements major in-memory concurrency-control
protocols (Silo, Cicada, MOCC, TicToc, ERMIA, SI, SS2PL, OCC) on a common
substrate, plus a TPC-C harness for Silo. The accompanying analysis paper is
at <http://www.vldb.org/pvldb/vol13/p3531-tanabe.pdf>.

## Quick start

CCBench targets x86_64 Linux. On macOS, use the [.devcontainer/](.devcontainer/)
(`Dev Containers: Reopen in Container`) — see [docs/build.md](docs/build.md)
for caveats.

```sh
git clone --recurse-submodules <repo>
cd ccbench
sudo apt-get install -y $(cat build_tools/ubuntu.deps)

./build_tools/bootstrap.sh             # masstree
./build_tools/bootstrap_mimalloc.sh    # mimalloc
./build_tools/bootstrap_googletest.sh  # googletest

cd silo && mkdir -p build && cd build
cmake -DCMAKE_BUILD_TYPE=Release .. && make -j
./silo.exe -help
```

## Repository layout

- `<protocol>/` — one directory per CC protocol (`silo`, `cicada`, `mocc`,
  `tictoc`, `ermia`, `si`, `ss2pl`, `occ`, `tpcc_silo`); each is built
  independently and has its own `README.md`. Index in
  [docs/protocols.md](docs/protocols.md).
- `include/`, `common/` — shared headers and sources (rwlock, masstree wrapper,
  zipf generator, etc.).
- `build_tools/` — bootstrap scripts and the apt dependency list.
- `third_party/` — submodules: `masstree`, `mimalloc`, `googletest`, `spdlog`.
- `instruction/` — micro-benchmarks for individual instructions.

## Documentation

- [docs/build.md](docs/build.md) — full build instructions (host & devcontainer)
- [docs/protocols.md](docs/protocols.md) — index of implemented protocols
- [docs/runtime-args.md](docs/runtime-args.md) — runtime argument reference
- Experimental data: <https://github.com/thawk105/ccdata>

## Contributing

Pull requests are welcome for performance improvements, bug fixes, doxygen-style
comments, portability, tests, and protocol extensions. See
[PR #7](https://github.com/thawk105/ccbench/pull/7) for an example of extending
CCBench.

## Acknowledgments

Takayuki Tanabe extends thanks to:

- Cybozu Labs Youth (8th term, 2018-04-10 – 2019-04-10) for supporting this work.
- Takashi Hoshino, advisor at Cybozu Labs Youth.
- Hideyuki Kawashima and Osamu Tatebe, supervisors.
