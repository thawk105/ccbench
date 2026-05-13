# CLAUDE.md

Startup context for Claude when working in this repository. **このファイルは
Claude (AI) の起動コンテキスト**。リポジトリの仕様や規約そのものは
`docs/` 以下に置いてあって、ここからリンクしているだけ。人間 contributor が
読むべき情報の本体は各 `docs/*.md` を見ること。

## Where to start (Claude も人間も)

| 知りたいこと | 読む場所 |
|---|---|
| リポジトリの全体像、アーキテクチャ、TxExecutor の API 契約、GC 規約 | [docs/architecture.md](docs/architecture.md) |
| ビルド手順 (cmake, devcontainer, GCC version, build modes) | [docs/build.md](docs/build.md) |
| 各プロトコルとワークロード対応表、新プロトコルの追加方法 | [docs/protocols.md](docs/protocols.md) |
| ワークロード仕様 (TPC-C / YCSB / BoMB) | [docs/workloads.md](docs/workloads.md) |
| 実行時引数 | [docs/runtime-args.md](docs/runtime-args.md) |
| **ファイル種別ごとのコーディング規約 (Dockerfile / CMake / GHA / C++ / Shell)** | [docs/coding-conventions.md](docs/coding-conventions.md) |
| **Issue / PR / commit の運用規約 (1 issue = 1 context 等)** | [docs/contributing.md](docs/contributing.md) |

## Hard rules (これだけは出かける前に確認)

1. **編集するファイル種別の規約を [coding-conventions.md](docs/coding-conventions.md)
   で確認してから書く**。指摘される前に Docker / CMake / C++ の業界 best
   practice を適用する。
2. **`tx.read` の戻り値は必ず check する**。
   [coding-conventions.md § `tx.read` returns Status](docs/coding-conventions.md)
   を参照。ここをサボると Debug+ASan で `HeapObject::cast_to` で死ぬか、
   Release で garbage data を読み出す。
3. **Issue を立てるときは 1 context に絞る**。
   [contributing.md](docs/contributing.md) を参照。`[P4+P8+P10]` 系の
   複数コンテキスト混ぜは過去にやって分割した経緯 (#37 → #60〜#64)。
4. **Werror promotion 系の作業は GCC 13 (CI と同じ) でローカル確認する**。
   devcontainer も CI も `ubuntu:24.04` ベースの GCC 13 に揃えてある
   ([docs/build.md § Compiler version](docs/build.md))。

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
    `docs/architecture.md`, `docs/build.md`, `docs/protocols.md`
  - Coding conventions adopted by the project → `docs/coding-conventions.md`
  - Process / governance (issue・PR・commit) → `docs/contributing.md`
  - Decision rationale that future maintainers will want to look up
- **Goes in Claude memory (private to one Claude instance)** if it's
  personal to how *this* Claude collaborates with *this* user:
  - User preferences ("reply in Japanese", "no Co-Authored-By trailer")
  - Claude's own behavioral heuristics (parallelization rules,
    prompt-writing patterns, resource judgement)
  - "Last time I made mistake X, the user's fix was Y" — reminders for
    future Claude turns, not for human review

Rule of thumb: **"would a new contributor cloning the repo need this?"**
→ yes → repo (`docs/*` first, fall back to `CLAUDE.md` only if it's
genuinely AI-only meta); → no → memory. Personal preferences and
Claude-side heuristics specifically **should not** be committed to
CLAUDE.md or `docs/` — that pollutes the shared repo with one person's
taste.

## このファイルを膨らませないこと

CLAUDE.md は **目次 + AI 向けメタ情報** で短く保つ。リポジトリの事実
や規約を書きたくなったら `docs/*` に書き、ここからリンクする。長く
なってきたら切り出すサイン。
