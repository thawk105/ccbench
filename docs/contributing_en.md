# Contributing

Rules for opening issues, sending PRs, and writing commits in this repo. Coding rules (per-file-type best practices) are in [coding-conventions_en.md](coding-conventions_en.md).

Currently this doc **starts from issue-opening rules**. Add more as conventions emerge.

> **Note**: The Japanese version [contributing_ja.md](contributing_ja.md) is the canonical source. Update it first, then refresh this English version. Don't update only one side.

---

## Opening an issue

### Principle: 1 issue = 1 context

**One issue covers exactly one context (one work goal / one impact area).** Mixing different contexts in the same issue breaks the following:

- **Reviews are harder**: comments lose track of which sub-task they refer to
- **Progress gets tangled**: "half done, half not" is hard to express
- **Closing becomes complicated**: "it's half done, can I close it?" recurs
- **PRs no longer map 1:1**: PR titles end up as "the P4 part of issue #X"

### Anti-pattern

An issue titled like `[P4+P8+P10]` with **multiple independent P-numbers** — a real example is #37, later split into separate context-specific issues and closed. The bracketed prefix just lists tracking IDs; the actual content is three independent tasks.

→ Open them as **three separate issues from the start**.

### Issue title

- Describe the content. Don't add tracking-only prefixes like `[P4]` / `[Phase 2]` — GitHub already tracks via numbers, and the prefix is meaningless to later readers
- Exception: roadmap roll-up issues (see below)
- Japanese / English are both OK; mixing in a single issue is fine

### Standard issue-body structure

```markdown
## Background
- Why this issue exists
- Links to related PRs / issues / existing code
- How it was discovered (= what surfaced it)

## Proposal
- What to change (concretely)
- A diff-style snippet helps

## Done criteria
- [ ] Mechanically checkable checklist items

## Caveats / trade-offs (only if applicable)

## Related
- Related issues / PRs
```

The flow is "current state → proposal → done criteria → related". **Done criteria written in machine-checkable form** ("build passes", "`add_definitions` count is 0", etc.) keep "is it done?" from being subjective later.

### Exception: roadmap roll-up issues are OK

A "roadmap issue" that tracks a long-running multi-phase effort is the one allowed exception. The roadmap itself **is one context**; each phase's implementation goes in a separate PR / separate sub-issue.

Example: [#43](https://github.com/thawk105/ccbench/issues/43) (phased -Werror cleanup) — a roadmap for fixing 9 `-W<flag>` warnings across 4 phases. Each phase's fix is its own PR, independently addressable.

Rules for roll-up issues:

- The body must say explicitly that **sub-tasks ship as separate PRs**
- When a sub-task gets a dedicated PR / sub-issue, update the roadmap body to link to it
- "When does it belong in the roll-up vs a separate issue?" — if the sub-task depends on other sub-tasks, keep it in the roll-up; if it's independent, file a separate issue

### When you find a context-mixing issue

- If you find that an existing issue contains multiple contexts, **split it**:
  1. Open a new issue per context (with a short body each)
  2. On the original issue, leave a closing comment listing the split destinations and close it with `state_reason: not_planned`
  3. `gh api repos/.../issues/N -X PATCH -f state=closed -f state_reason=not_planned` substitutes for `gh issue close` (older `gh` versions lack `--reason`)
- Real example: #37 was split into #60〜#64

### When you close an issue

- Closed because implementation merged → use `completed` (default)
- "We decided not to do this", "Replaced by another issue", "Duplicate" → use `not_planned` + a comment explaining why
- When closing, **explicitly comment which PR / issue replaces it**

---

## Document language and naming conventions

This repo is maintained primarily in Japanese and uses AI to provide an English mirror. To preserve a single source of truth, **suffix-based naming makes the canonical language explicit**.

### Per-file rules

| File | Language | Canonical / generated | Notes |
|---|---|---|---|
| `README.md` | English | **Canonical** | GitHub home picks this up, and citations in papers refer to it — so the canonical version is English. Keep the headings short and world-facing |
| `README_ja.md` | Japanese | Generated (optional) | Create via AI translation if needed |
| `CLAUDE.md` | Japanese | **Canonical** | Claude Code reads the fixed path `CLAUDE.md` — filename can't change. Content is Japanese, treated as canonical |
| `CLAUDE_en.md` | English | Generated | For English-environment Claude / overseas contributors |
| `docs/<name>_ja.md` | Japanese | **Canonical** | Canonical for every doc. Edits start here |
| `docs/<name>_en.md` | English | Generated | AI-translated from the `_ja.md` |

`README.md` and `CLAUDE.md` are the only **fixed-path exceptions** (external spec from GitHub home / Claude Code). Their canonical languages differ on purpose: `README` is world-facing (English), `CLAUDE` is primary-maintainer-facing (Japanese).

### "Canonical" responsibilities — **`_ja` / `_en` paired-update is a Hard rule**

All docs / CLAUDE-family files **except** `README.md` (which stands alone) are treated as **`_ja` / `_en` (or canonical / translated) pairs**. **Updating only one side is forbidden.** Without this rule, single-language PRs slowly accumulate "canonical vs translation" drift.

Concretely:

- Spec / convention changes are made **against the canonical side**
  - `CLAUDE.md` (Japanese, canonical) ↔ `CLAUDE_en.md` (English, translated)
  - `docs/<name>_ja.md` (canonical) ↔ `docs/<name>_en.md` (translated)
- **Update the translation side in the same PR / same commit.** Single-side updates are not mergeable
  - Reviewers must verify both sides are updated
  - We may enforce via CI in the future (e.g. check that `_ja` and `_en` always exist as a pair, or that their last-modified times match)
- Review the canonical. The translation is only checked for "does it follow the canonical?"
- If translation and canonical drift, **the canonical wins** (= fix the translation to follow)
- Exception: `README.md` is a **standalone English canonical** with an optional `README_ja.md` mirror. The pair-update rule applies only when the mirror exists

### Translation policy

- Translate via AI (Claude / DeepL / etc.) to generate `_en.md` (or `_ja.md`)
- Don't translate technical terms (`rebase`, `force-push`, `Werror`, `target_compile_options`, `Tuple`, `Status`, etc.)
- Never translate inside code blocks or command lines
- Markdown links **point to the same-language file on both sides** (e.g. a `_ja.md` linking to another doc uses `_ja.md`)
  - Exception: external URLs, images, etc.
- Future option: automate translation in a CI workflow (currently done manually during a Claude turn)

### Don't create suffix-less files under `docs/`

- Don't create `docs/architecture.md` style **suffix-less** files under `docs/` (the language is ambiguous)
- `README.md` and `CLAUDE.md` at the repo root are the exceptions (see above)

### When adding a new doc

1. **Write `docs/<name>_ja.md` first** (canonical)
2. Generate `docs/<name>_en.md` via AI translation
3. Commit both in the same commit
4. Links pointing to the new doc use the **same suffix as the source file's language**
   - From `CLAUDE.md` (Japanese): link to `docs/<name>_ja.md`
   - From `CLAUDE_en.md` (English): link to `docs/<name>_en.md`
   - From `README.md` (English, canonical): link to `docs/<name>_en.md` (for English readers)

## Adding a new protocol

How to add a new concurrency-control (CC) protocol as `cc/<name>/`. **Line-number-based instructions are deliberately avoided** — they go stale on every internal refactor. Instead this section is conceptual: "what the real work is", "which file plays which role", and "which mechanism you plug into". For the full architecture overview and the TxExecutor contract details, see [architecture_en.md](architecture_en.md).

### The real work: implement the CC algorithm in the tx operations

**Adding a new protocol means implementing the concurrency-control algorithm itself in `TxExecutor`'s transaction operations.** ccbench's mission is to *compare CC protocols on a shared foundation* (the Masstree index, the workload templates, the measurement harness), and the differences between protocols live precisely in the bodies of these operations. Copying an existing protocol's directory only **erects the scaffolding (a starting point)** — it is not the work itself. Copying, renaming, and registering with CMake does not produce a "new protocol" if the bodies are still those of the original protocol.

The operations to implement, their responsibilities, and how their bodies change with the CC algorithm:

| Operation | Common responsibility | What changes with the algorithm |
|---|---|---|
| `read` | Fetch the visible data for a key and return it to the caller. Read-own-writes (return from the local write set if present) also belongs here | **What counts as "visible"**. OCC reads the latest version while recording its TID into the read set / 2PL takes a read lock at access time / MVCC picks, from the version chain, the latest version at or before its own timestamp |
| `update` | Reserve an update to an existing record (`WARN_NOT_FOUND` if it does not exist) | **When the update becomes visible**. OCC just stages it into the write set, deferred until commit / 2PL takes a write lock here (upgrading if it holds a read lock) / MVCC creates and links a new version in the pending state |
| `insert` | Create a new record (`WARN_ALREADY_EXISTS` if it exists) | How the new `Tuple` is initialized. OCC creates it with the `absent` bit and sets it at commit / 2PL creates it already write-locked / MVCC gives it a first version. May need a Masstree node version check (silo's `node_map_`) |
| `delete_record` | Reserve a record deletion | OCC sets `absent` and pushes to the GC queue / 2PL takes a write lock and unlinks from the index at commit / MVCC stages a delete-marked new version |
| `scan` | Walk a key range and return each visible record. **Both the `int64_t limit` overload and the one without are required** (the concept demands it) | For each tuple in the range, apply essentially the same decision as `read`, per the CC algorithm |
| `commit` | Finalize the transaction; return success/failure as `bool` | **The heart of the protocol**. OCC validates the read set → locks the write set → write phase / 2PL (assuming it already holds every lock) applies writes and releases locks / MVCC runs a version consistency check → promotes pending versions to committed |
| `abort` | Roll the transaction back and release the resources it acquired | OCC removes inserted rows and clears the read/write sets / 2PL releases every held lock / MVCC discards pending versions |

`begin` (timestamp allocation, etc.), the tuple/version memory layout, the lock/validation logic, and GC (garbage collection) are **likewise things you redesign to fit the algorithm**. Take the tuple layout alone: silo's `Tuple` carries a `Tidword` (lock bit + epoch + TID), ss2pl's carries a `ReaderWriteLock`, and mvto's `Tuple` carries the head of an `atomic<Version*>` version chain plus `min_wts_`. Designing "what to write in `read`/`commit`, and what to put in the tuple, for my algorithm" is the center of this work.

The CC families in one line each:

- **OCC (optimistic, e.g. [cc/silo/](../cc/silo/))** — takes no locks during access and validates everything at `commit`. `read` records the TID, and `commit` only writes after confirming "the versions I read have not changed".
- **2PL (pessimistic, e.g. [cc/ss2pl/](../cc/ss2pl/))** — takes a lock the moment it accesses (shared for read, exclusive for update). All `commit` does is apply the writes and release the locks.
- **MVCC (multi-version, e.g. [cc/cicada/](../cc/cicada/), [cc/mvto/](../cc/mvto/))** — keeps a version chain per record and decides visibility by timestamp order. `read` selects a version, `update` creates a new version, `commit` runs a version consistency check. GC of old versions is mandatory.

For the deterministic family, see [cc/d2pl/](../cc/d2pl/).

### Starting point: copy an existing protocol as scaffolding

Doing the real work above *together with* creating files from scratch is painful, so **copy the whole directory (`cc/<proto>/`) of the existing protocol closest to your goal and use it as scaffolding (a starting point)**. What the copy gives you is "a `TxExecutor` skeleton that builds", "the wiring to the workload templates", and "boilerplate like `result.cc` / `util.cc`" — **the CC algorithm does not come with it**. Right after copying, you have nothing but "a working copy of the original protocol"; the work is to actually rewrite the tx operations, tuple layout, lock/validation logic, and GC from the table above to fit the CC algorithm you want to implement. "Copy and rename and done" does not yield a new protocol.

Pick what to scaffold from by which family your target algorithm is closest to:

- Optimistic (OCC) family → [cc/silo/](../cc/silo/) — the most straightforward reference implementation, supports all four workloads
- Multi-version (MVCC) family → [cc/cicada/](../cc/cicada/) or [cc/mvto/](../cc/mvto/)
- Pessimistic / lock-based → [cc/ss2pl/](../cc/ss2pl/)
- Deterministic → [cc/d2pl/](../cc/d2pl/)

There used to be a `cc_format/` "single-version template" directory, but it was Makefile-based and diverged from the main CMake build, and its `README.md` steps were line-number-based and went stale — so it was removed (#83; see #34 for the discussion history). The template's role is replaced by "copy an existing protocol" plus, if needed, AI scaffolding (have it generate a skeleton that satisfies the TxExecutor contract).

### Directory components

Rewrite the scaffolded `cc/<name>/` to match the new protocol name and the CC algorithm. Each file's role:

| File | Role |
|---|---|
| `transaction.cc` / `include/transaction.hh` | **The protocol's core**. `include/transaction.hh` defines the `TxExecutor` class (with `static_assert(TxExecutorLike<TxExecutor>);` right after the class definition — [include/tx_executor_concept.hh](../include/tx_executor_concept.hh)), and `transaction.cc` holds the implementations of `read` / `update` / `insert` / `delete_record` / `scan` / `commit` / `abort`. The "real work" above is mostly rewriting here |
| `include/tuple.hh` / `include/version.hh`, etc. | The `Tuple` / `Version` memory layout. Design here the metadata your algorithm needs — lock bits, timestamps, version chains, etc. |
| `include/*_op_element.hh` | The read/write set element types. Determined by what `commit`'s validation needs to remember |
| `CMakeLists.txt` | Just one call to `ccbench_add_protocol(<name> ...)`. Details below |
| `<workload>_<name>.cc` | Per-workload entry point (`ycsb_*`, `tpcc_*`, `bomb_*`, `sbomb_*`). Defines `worker()` and drives the matching workload template ([include/ycsb.hh](../include/ycsb.hh), [include/tpcc.hh](../include/tpcc.hh), [include/bomb.hh](../include/bomb.hh), etc.). You need one per tag listed in the `CMakeLists.txt` `WORKLOADS` |
| `result.cc` | Defines the per-thread result buffer (the `<Name>Result` vector) and `initResult()` |
| `util.cc` / `include/util.hh` | DB initialization, record initial-value setup, the leader thread's job (`leaderWork`), etc. |

### Wiring after the scaffolding: plug into `ccbench_add_protocol()`

The build is assembled declaratively by the `ccbench_add_protocol()` helper in [cmake/ProtocolHelpers.cmake](../cmake/ProtocolHelpers.cmake). A `cc/<name>/CMakeLists.txt` is just a single call to this helper:

```cmake
ccbench_add_protocol(<name>
  SOURCES   transaction.cc util.cc result.cc   # .cc shared across workloads; don't put entry points here
  WORKLOADS ycsb tpcc bomb sbomb               # a subset of the supported workload tags
  OPTIONS                                      # protocol-specific -D defines (optional)
    FOO=${CCBENCH_FOO}
)
```

- For each tag `W` in `WORKLOADS`, the helper builds `W_<name>.exe` from `W_<name>.cc` + `SOURCES` and links `ccbench_common` + `ccbench::masstree` + `ccbench::mimalloc`.
- The universal `-D` flags (`KEY_SIZE`, `VAL_SIZE`, `BACK_OFF`, `ADD_ANALYSIS`, `MASSTREE_USE`, etc.) and `-Wall -Wextra -Werror` are added automatically. To add a protocol-specific cache option, add it to [cmake/Options.cmake](../cmake/Options.cmake).
- Finally, add the new directory name to the `foreach(_proto …)` loop in the top-level [CMakeLists.txt](../CMakeLists.txt). This also makes a row appear automatically in [build/PROTOCOL_MATRIX.md](../build/PROTOCOL_MATRIX.md) at configure time.

### Enforced by the TxExecutor contract

The tx operations you rewrite in "the real work" above must satisfy the `TxExecutor` API the workload templates expect. This contract is expressed as the `TxExecutorLike` concept in [include/tx_executor_concept.hh](../include/tx_executor_concept.hh), and each protocol **enforces it at compile time** via `static_assert(TxExecutorLike<TxExecutor>);` at the end of `transaction.hh`. A missing method or a signature mismatch (e.g. forgetting the `int64_t limit` overload of `scan`) fails that protocol's own build with a named diagnostic, so it never becomes a runtime crash. The concept only constrains "the outward shape, the signatures" — it does *not* guarantee that the body of each operation contains a correct CC algorithm; that is the implementer's responsibility. For the meaning of each contract method, see [architecture_en.md](architecture_en.md).

### Checklist

CC algorithm implementation (the real work):

- [ ] Rewrote `read` / `update` / `insert` / `delete_record` / `scan` / `commit` / `abort` to fit the CC algorithm being implemented (no logic left over from the copy source)
- [ ] Redesigned the tuple/version layout, lock/validation logic, and GC to fit the algorithm
- [ ] Have both the `int64_t limit` overload of `scan` and the one without

Scaffolding / wiring:

- [ ] Copied `cc/<name>/` from the existing protocol closest to the goal and renamed it
- [ ] `cc/<name>/CMakeLists.txt` calls `ccbench_add_protocol(<name> ...)`
- [ ] Added `<name>` to the `foreach(_proto …)` loop in the top-level `CMakeLists.txt`
- [ ] `transaction.hh` ends with `static_assert(TxExecutorLike<TxExecutor>);`
- [ ] `cmake -S . -B build` passes and each binary listed in `WORKLOADS` builds
- [ ] Added a row to the protocol table in [docs/protocols_en.md](protocols_en.md) (canonical is `_ja`, so update it as a pair)

## Sending a PR (TODO)

PR conventions are not yet written up. Add as you notice patterns.

PRs worth looking at as reference for title / body style:

- [#44](https://github.com/thawk105/ccbench/pull/44) (`-Werror=maybe-uninitialized`) — best-practice "explain the why" style
- [#50](https://github.com/thawk105/ccbench/pull/50) (`-Werror=unused-label`) — dead-code removal + real-bug fix in the same PR
- [#52](https://github.com/thawk105/ccbench/pull/52) (introducing `docs/coding-conventions.md`) — convention-meta PR

---

## Commit message (TODO)

No formal rule yet. Follow the look of existing history.

The `Co-Authored-By` trailer is **not used** (avoid bloating commits with avatar icons — applies to Claude too).
