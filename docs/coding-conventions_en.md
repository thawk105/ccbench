# Coding conventions

Repository-specific coding rules and per-file-type industry best practices that should be applied **without being asked**.

**How to use this doc**:

- Consult the relevant section before editing a file of that type
- If you spot existing code that violates a rule, fix it in a **separate PR** — don't grow the scope of the current PR
- When a new convention emerges, add it here (Claude or any contributor)

Starts thin. Add as you learn.

> **Note**: The Japanese version [coding-conventions_ja.md](coding-conventions_ja.md) is the canonical source. Update it first, then refresh this English version. Don't update only one side.

---

## Dockerfile

- **`apt-get install` multi-line args must be alphabetically sorted**
  ([Docker docs: Sort multi-line arguments](https://docs.docker.com/build/building/best-practices/#sort-multi-line-arguments)).
  Prevents duplicate additions, reduces merge conflicts, improves review.
- **Always use `--no-install-recommends`**. Recommends bring in extra dev
  tools that bloat the image and slow CI container init.
- **Leverage BuildKit cache mounts**:
  `--mount=type=cache,target=/var/cache/apt,sharing=locked` keeps the apt
  download cache between builds.
- **Use multi-stage to separate "dev" and "ci"**: human-facing tools (zsh,
  sudo, etc.) in the `dev` stage, minimal CI image in the `base` stage.
  One Dockerfile, switch via `target:` (see
  [.devcontainer/Dockerfile](../.devcontainer/Dockerfile)).

## CMake

- **Warning flags go on a target via `target_compile_options(... PRIVATE ...)`**.
  Avoid `add_definitions(...)` (directory scope) — flags leak into other
  targets you didn't intend.
- **Universal `-D` flags live in
  [cmake/Options.cmake](../cmake/Options.cmake) as `CCBENCH_*` CACHE
  variables**, so they can be overridden from the command line
  (e.g. `cmake -S . -B build -DCCBENCH_KEY_SIZE=16`).
- **Declare new protocols with `ccbench_add_protocol(...)`**
  (see [cmake/ProtocolHelpers.cmake](../cmake/ProtocolHelpers.cmake)).
  Don't write per-protocol `file(GLOB)` / `add_executable` /
  `add_definitions` blocks.
- **Don't bump `cmake_minimum_required(VERSION X.Y)` casually** — verify
  against both the devcontainer's cmake and CI's cmake.

## GitHub Actions workflow

- **Pin actions at the explicit major version** (e.g. `actions/checkout@v6`).
  Avoid floating (`@main`) or patch-level (`@v6.1.2`) pins.
- **Container jobs need `--user root` + a git `safe.directory` setup**.
  See [#46](https://github.com/thawk105/ccbench/pull/46) for the history.
- **Use `paths-ignore` so CI doesn't run on irrelevant changes**
  (docs/, README.md, etc.).
- **Reference secrets only via repository secrets** — never hardcode.

## C++

- **Always check `tx.read`'s return value**. See
  [`tx.read` returns Status — *check it*](#txread-returns-status--check-it) below.
- **Default-init local variables at declaration** (`int x = 0;` /
  `T* p = nullptr;`). GCC 13's `-Wmaybe-uninitialized` surfaced real
  bugs through [#44](https://github.com/thawk105/ccbench/pull/44).
- **Don't silently discard an `auto`-bound `Status`**. If you mean to
  ignore it, write `(void) func(...)` with a comment explaining why; if
  you don't, check it explicitly.

### `tx.read` returns Status — *check it*

`tx.read` returns `Status::OK` on hit and `Status::WARN_NOT_FOUND` on miss.
On miss, `*body` is **left unchanged** (it does not get nullified). Checking
only `tx.status_ == TransactionStatus::aborted` is not enough — a stale
`body` pointer from a previous read will silently get dereferenced, which
manifests as a `HeapObject::cast_to` assertion under Debug+ASan and as
garbage data in Release. Always:

```cpp
Status stat = tx.read(s, key, &body);
if (tx.status_ == TransactionStatus::aborted) return false;
if (stat != Status::OK) return false;   // do not skip this
```

This was actually missing in `get_material_cost()` and was surfaced by
`-Werror=unused-variable` in PR #58 — be aware of it on both the reading
and writing side, before review has to flag it.

## Shell scripts

- **`set -euo pipefail`** at the top (early failure + detect unset
  variables + don't miss failures inside a pipeline).
- **Prefer long flags** (`--no-install-recommends` / `--platform`).
  Unless it's a one-liner, the long form is much easier to read later.

## Markdown / docs

- Mixed Japanese/English is **OK overall**, but keep a single section
  consistent in one language.
- **Tag code blocks with the language** (` ```sh `, ` ```cmake `,
  ` ```cpp `, ...) so GitHub's syntax highlight kicks in.
- **Use document-relative paths for internal links** (e.g.
  [../include/foo.hh](../include/)), not repository-root-absolute paths.

## About this document itself

- **When you notice a new convention, add it here first** — writing it
  once is cheaper than calling it out in every PR review.
- **Keep the rationale short alongside each rule**: without "why", rules
  drift into being ignored.
- **If you find a case where the rule should be bent, document that
  exception here too**.
