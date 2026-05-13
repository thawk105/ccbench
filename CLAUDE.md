# CLAUDE.md

Claude (AI) がこのリポジトリで作業するときの起動コンテキスト。**このファイル
は Claude (AI) 用**。リポジトリの仕様や規約そのものは `docs/` 配下に置いて
あって、ここからリンクしているだけ。人間 contributor が読むべき情報の本体は
各 `docs/*_ja.md` (原典) を見ること。英語 reader は [CLAUDE_en.md](CLAUDE_en.md)
を読む。

## Where to start (Claude も人間も)

| 知りたいこと | 読む場所 |
|---|---|
| リポジトリの全体像、アーキテクチャ、TxExecutor の API 契約、GC 規約 | [docs/architecture_ja.md](docs/architecture_ja.md) |
| ビルド手順 (cmake, devcontainer, GCC version, build modes) | [docs/build_ja.md](docs/build_ja.md) |
| 各プロトコルとワークロード対応表、新プロトコルの追加方法 | [docs/protocols_ja.md](docs/protocols_ja.md) |
| ワークロード仕様 (TPC-C / YCSB / BoMB) | [docs/workloads_ja.md](docs/workloads_ja.md) |
| 実行時引数 | [docs/runtime-args_ja.md](docs/runtime-args_ja.md) |
| **ファイル種別ごとのコーディング規約 (Dockerfile / CMake / GHA / C++ / Shell)** | [docs/coding-conventions_ja.md](docs/coding-conventions_ja.md) |
| **Issue / PR / commit / docs 言語規約 (1 issue = 1 context、`_ja`/`_en` セット更新等)** | [docs/contributing_ja.md](docs/contributing_ja.md) |

## Hard rules (出かける前に必ず確認)

1. **編集するファイル種別の規約を [coding-conventions_ja.md](docs/coding-conventions_ja.md)
   で確認してから書く**。指摘される前に Docker / CMake / C++ の業界 best
   practice を適用する。
2. **`tx.read` の戻り値は必ず check する**。
   [coding-conventions_ja.md § `tx.read` returns Status](docs/coding-conventions_ja.md)
   を参照。ここをサボると Debug+ASan で `HeapObject::cast_to` で死ぬか、
   Release で garbage data を読み出す。
3. **Issue を立てるときは 1 context に絞る**。
   [contributing_ja.md](docs/contributing_ja.md) を参照。`[P4+P8+P10]` 系の
   複数コンテキスト混ぜは過去にやって分割した経緯 (#37 → #60〜#64)。
4. **Werror promotion 系の作業は GCC 13 (CI と同じ) でローカル確認する**。
   devcontainer も CI も `ubuntu:24.04` ベースの GCC 13 に揃えてある
   ([docs/build_ja.md § コンパイラバージョン](docs/build_ja.md))。
5. **`README.md` 以外の docs / CLAUDE 系を編集するときは `_ja` と `_en` を
   セットで更新する** (Hard rule)。原典 (`_ja.md` / `CLAUDE.md`) を直し、
   同じ commit で翻訳 (`_en.md` / `CLAUDE_en.md`) も更新する。片方だけ更新
   する PR は merge 不可。詳細は
   [contributing_ja.md § 「原典」の責務](docs/contributing_ja.md) を参照。

## Where to record what you learn (CLAUDE.md / docs vs Claude memory)

Claude は **プロジェクトごとの private memory** を持つ
(`~/.claude/projects/.../memory/`)。これはセッションをまたいで永続するが、
**リポジトリには含まれない** — clone と一緒に配布されないので、他の
contributor や他の Claude インスタンスからは見えない。CLAUDE.md と `docs/`
は clone とともに配布され全員に届く。何かを記録するときは正しい行き先を選ぶ
こと:

- **リポジトリに書く (CLAUDE.md / docs/) もの**: 人間 contributor も知らないと
  困る情報
  - リポジトリの事実 (アーキテクチャ、build フロー、protocol matrix など)
    → `docs/architecture_ja.md`, `docs/build_ja.md`, `docs/protocols_ja.md`
  - プロジェクトで採用されたコーディング規約 → `docs/coding-conventions_ja.md`
  - process / governance (issue / PR / commit) → `docs/contributing_ja.md`
  - 将来のメンテナが調べたくなる決定の理由
- **Claude memory に書く (1 Claude インスタンスの私物)** もの: この Claude
  がこの user と協業する時固有の話
  - ユーザーの個人的な好み (「日本語で返信」「Co-Authored-By trailer を
    付けない」)
  - Claude 自身の行動ヒューリスティック (subagent 並列度、prompt 書き方、
    リソース判断)
  - 「前回ミス X をやって、ユーザーが Y で直してくれた」── 将来の Claude の
    ためのリマインダ、人間のレビュー対象ではない

**判断軸: 「new contributor がリポを clone したとき必要か?」** → YES なら
repo (`docs/*` 優先、本当に AI-only meta な話だけ `CLAUDE.md`)、NO なら
memory。個人の好みや Claude 側ヒューリスティクスは **絶対に** `CLAUDE.md` や
`docs/` に commit しない (= 一個人の taste で shared repo を汚染することに
なる)。

## このファイルを膨らませないこと

CLAUDE.md は **目次 + AI 向けメタ情報** で短く保つ。リポジトリの事実や規約を
書きたくなったら `docs/*_ja.md` に書き、ここからリンクする。長くなってきた
ら切り出すサイン。
