# CLAUDE_en.md

Startup context for Claude when working in this repository (English mirror of [CLAUDE.md](CLAUDE.md)). **This file is for Claude (AI).** Repository specs and conventions live under `docs/`; this file just links to them. Human contributors should read the `docs/*_en.md` files for the actual content.

> **Note**: [CLAUDE.md](CLAUDE.md) (Japanese) is the canonical source. Update it first, then refresh this English version. Don't update only one side.

## Where to start (Claude and humans alike)

| What you want | Where to read |
|---|---|
| Repo overview, architecture, TxExecutor API contract, GC rules | [docs/architecture_en.md](docs/architecture_en.md) |
| Build flow (cmake, devcontainer, GCC version, build modes) | [docs/build_en.md](docs/build_en.md) |
| Protocol × workload matrix, how to add a new protocol | [docs/protocols_en.md](docs/protocols_en.md) |
| Workload specs (TPC-C / YCSB / BoMB) | [docs/workloads_en.md](docs/workloads_en.md) |
| Runtime arguments | [docs/runtime-args_en.md](docs/runtime-args_en.md) |
| **Per-file-type coding conventions (Dockerfile / CMake / GHA / C++ / Shell)** | [docs/coding-conventions_en.md](docs/coding-conventions_en.md) |
| **Issue / PR / commit / docs-language rules (1 issue = 1 context, `_ja`/`_en` paired-update, etc.)** | [docs/contributing_en.md](docs/contributing_en.md) |

## Hard rules (check before doing anything)

1. **Check [coding-conventions_en.md](docs/coding-conventions_en.md) for the
   conventions for the file type you're editing**, before you write
   anything. Apply industry best practices for Docker / CMake / C++ etc.
   without being asked.
2. **Always check `tx.read`'s return value**. See
   [coding-conventions_en.md § `tx.read` returns Status](docs/coding-conventions_en.md).
   Skipping this kills the program with a `HeapObject::cast_to` assertion
   under Debug+ASan, or reads garbage data under Release.
3. **One issue = one context** when opening issues. See
   [contributing_en.md](docs/contributing_en.md). Mixing `[P4+P8+P10]`
   style multi-context issues happened once (#37 → split into #60–#64);
   don't repeat it.
4. **Werror-promotion work has to be verified locally on the CI's compiler
   (GCC 13)**. Both the devcontainer and CI are now on `ubuntu:24.04` →
   GCC 13 ([docs/build_en.md § Compiler version](docs/build_en.md)).
5. **When editing any doc / CLAUDE-family file other than `README.md`,
   update the `_ja` and `_en` sides together** (Hard rule). Edit the
   canonical (`_ja.md` / `CLAUDE.md`) and update the translation
   (`_en.md` / `CLAUDE_en.md`) in the same commit. Single-side updates
   are not mergeable. Details in
   [contributing_en.md § Canonical responsibilities](docs/contributing_en.md).

## Where to record what you learn (CLAUDE.md / docs vs Claude memory)

Claude maintains a **per-project private memory**
(`~/.claude/projects/.../memory/`) that persists across sessions but
**is not part of the repository** — it doesn't ship with the clone, no
other contributor or other Claude instance sees it. CLAUDE.md and
`docs/` ship with every clone and reach everyone. Pick the right
destination when recording something new:

- **Goes in this repo (CLAUDE.md / docs/)** if a human contributor would
  also need it to work effectively on the codebase:
  - Repository facts (architecture, build flow, protocol matrix, etc.) →
    `docs/architecture_en.md`, `docs/build_en.md`, `docs/protocols_en.md`
  - Coding conventions adopted by the project → `docs/coding-conventions_en.md`
  - Process / governance (issue・PR・commit) → `docs/contributing_en.md`
  - Decision rationale that future maintainers will want to look up
- **Goes in Claude memory (private to one Claude instance)** if it's
  personal to how *this* Claude collaborates with *this* user:
  - User preferences ("reply in Japanese", "no Co-Authored-By trailer")
  - Claude's own behavioral heuristics (parallelization rules,
    prompt-writing patterns, resource judgement)
  - "Last time I made mistake X, the user's fix was Y" — reminders for
    future Claude turns, not for human review

Rule of thumb: **"would a new contributor cloning the repo need this?"**
→ yes → repo (`docs/*` first, fall back to `CLAUDE_en.md` only if it's
genuinely AI-only meta); → no → memory. Personal preferences and
Claude-side heuristics specifically **should not** be committed to
CLAUDE.md or `docs/` — that pollutes the shared repo with one person's
taste.

## Don't bloat this file

CLAUDE.md (and CLAUDE_en.md) are **a table of contents plus AI-specific
meta** — keep them short. When you want to record repo facts or
conventions, write them in `docs/*_ja.md` (or `_en.md`) and link from
here. Length growing is a signal to split out.
