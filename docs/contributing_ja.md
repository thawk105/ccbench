# Contributing

このリポジトリで issue を立てる / PR を出す / コミットを書くときの規約。コーディングルール (ファイル種別ごとの best practice) は [coding-conventions_ja.md](coding-conventions_ja.md) を見ること。

最初は **issue を立てるルールから書き始めている**。新しい合意ができたら追記していく方針。

---

## Issue を立てるとき

### 原則: 1 issue = 1 context

**1 つの issue は 1 つの context (作業目的 / 影響範囲) に絞る。** 関連する作業でも、コンテキストが違うものを 1 つの issue に混ぜると以下が壊れる:

- **レビューしにくい**: コメントがどのサブタスクに対するものか追えなくなる
- **進捗が混ざる**: 一部だけ終わって他は未着手、という状態の表現が難しい
- **close 条件が複雑になる**: 「半分終わってるけどクローズしていいの?」が頻発する
- **PR と 1:1 対応しない**: PR タイトル / 説明が「issue #X の P4 部分だけ」のように不格好になる

### よくあるアンチパターン

`[P4+P8+P10]` のように **複数の独立した P 番号** をタイトルに並べた issue (実例: 過去にあった #37、後で context 分割して close)。これは tracking 番号を並べただけで、実体は 3 つの独立タスク。

→ 立てるなら **最初から 3 つの issue として** 立てる。

### Issue タイトル

- 内容で表現する。`[P4]` / `[Phase 2]` のような **tracking-only な prefix は付けない**。GitHub の番号で追跡できるし、後で読む人にとって意味がない
- 例外: ロードマップ系の集約 issue は OK (下記参照)
- 日本語 / 英語 はどちらでも良い。1 つの issue 内で混ざるのは構わない

### Issue 本文の標準構造

```markdown
## 背景
- なぜこの issue を立てるか
- 関連する PR / issue / 既存コードへのリンク
- 発見経緯 (= どの作業で表面化したか)

## 提案
- 何を変えるか (具体的に)
- diff 風のスニペットがあると読みやすい

## 完了条件
- [ ] チェックリスト形式の検証可能条件

## 注意点 / トレードオフ (該当する場合のみ)

## Related
- 関連 issue / PR
```

「現状把握 → 提案 → 完了条件 → 関連」の流れ。**完了条件は機械的にチェックできる形** (「ビルドが通る」「`add_definitions` が 0 件」など) で書くと、後で「完了したか?」がブレない。

### 例外: ロードマップ集約 issue は OK

複数フェーズに分かれる長期作業をまとめて追跡する「ロードマップ issue」は別扱い。これは **「ロードマップ全体」という 1 つの context** が中身で、各フェーズの実装は別 PR / 別 sub-issue で進める。

例: [#43](https://github.com/thawk105/ccbench/issues/43) (phased -Werror cleanup) — 9 つの `-W<flag>` を 4 phase で潰すロードマップ。各 phase の修正は別 PR、それぞれ独立に着手できる。

ロードマップ集約 issue を立てるときの注意:
- 本文に **「サブタスクは別 PR で出す」** ことを明記する
- 各サブタスクに対応する PR / 個別 issue が出来たら、ロードマップ側の本文をリンクで更新する
- 「ロードマップに混ぜていい話」と「別 issue にすべき話」の判断軸は、**そのサブタスクが他のサブタスクと依存しているか** — 依存していれば本体に書く、独立していれば別 issue

### Context が混ざった issue を見つけたら

- 既存の issue が複数 context を含んでいることに気付いたら、**分割する**:
  1. それぞれの context で新 issue を立てる (本体は短く)
  2. 元 issue にクロージングコメントで分割先一覧を載せ、`state_reason: not_planned` で close
  3. `gh api repos/.../issues/N -X PATCH -f state=closed -f state_reason=not_planned` で `gh issue close` の代替が可能 (古い `gh` だと `--reason` が無いため)
- 実例: #37 → #60〜#64 に分割

### Issue を close するとき

- 「実装完了して merge した」なら `completed` (デフォルト) で close
- 「やらないことにした」「別 issue に置き換えた」「重複だった」なら `not_planned` で close + 理由をコメント
- close 時は **どの PR / どの issue に置き換わったか** をコメントで明記する

---

## ドキュメントの言語と命名規約

このリポジトリは日本語ネイティブが主にメンテし、AI 翻訳で英語版を提供する戦略を取る。「真実の単一性」を保つため **原典がどちらの言語か** を suffix で明示する。

### ファイル別の規約

| ファイル | 言語 | 原典/生成 | 備考 |
|---|---|---|---|
| `README.md` | 英語 | **原典** | GitHub home が拾う + 論文引用に出てくる name face なので、原典は英語。見出しは世界向けに簡潔に保つ |
| `README_ja.md` | 日本語 | 生成 (任意) | 必要なら AI 翻訳で生成 |
| `CLAUDE.md` | 日本語 | **原典** | Claude Code が固定 path `CLAUDE.md` を読む仕様のためファイル名は変えない。中身を日本語にして原典扱い |
| `CLAUDE_en.md` | 英語 | 生成 | 英語環境の Claude / 海外 contributor 向け |
| `docs/<name>_ja.md` | 日本語 | **原典** | 全 docs の原典。修正はここから |
| `docs/<name>_en.md` | 英語 | 生成 | AI 翻訳で `_ja.md` から生成 |

`README.md` と `CLAUDE.md` だけが **外部仕様 (GitHub home / Claude Code) で固定 path を要求される** ため、suffix なしの例外扱い。原典の言語が違うのは、`README` は世界向け (英語) で `CLAUDE` は主メンテナ向け (日本語) という性質の違いを反映している。

### 「原典」の責務 — **`_ja` / `_en` セット更新義務 (Hard rule)**

`README.md` (= 単独で完結) **以外**のすべての docs / CLAUDE 系ファイルは、**`_ja` / `_en` (or 原典 / 翻訳) ペア** として扱う。**片方だけ更新するのは禁止**。これを守らないと PR が片言語で merge されて「原典 vs 翻訳のズレ」が必ず累積する。

具体的に:

- 仕様変更 / 規約更新は必ず **原典側に対して行う**
  - `CLAUDE.md` (日本語、原典) ↔ `CLAUDE_en.md` (英語、翻訳)
  - `docs/<name>_ja.md` (原典) ↔ `docs/<name>_en.md` (翻訳)
- **同一 PR / 同一 commit で翻訳側も更新する**。片方だけ更新は merge 不可
  - レビュアーは「両方更新されているか」を必ずチェック
  - CI で `_ja` と `_en` の組がペアで存在するか、最終更新時刻が同じかなどを将来的に enforce する余地あり
- レビューの対象は原典。翻訳側は「原典に従っているか」だけチェック
- 「翻訳と原典がズレている」と気付いたら、**原典を正とする** (= 原典に従って翻訳を直す)
- 例外: `README.md` は **単独で完結する英語原典** で、対応する `README_ja.md` は **任意** (= 作っても作らなくても良い)。作る場合のみペア更新義務が発生する

### 翻訳の生成方針

- AI (Claude / DeepL 等) で翻訳して `_en.md` (or `_ja.md`) を生成
- 技術用語は訳さない (`rebase`, `force-push`, `Werror`, `target_compile_options`, `Tuple`, `Status`, 等)
- コードブロック内・コマンドラインは絶対に翻訳しない
- markdown のリンク先 path は **両言語版で同じ言語に揃える** (例: `_ja.md` 内のリンクは `_ja.md` を指す)
  - 例外: 外部 URL、画像 etc.
- 将来的には CI workflow で自動翻訳する余地あり (現状は手動 / Claude のターン内で生成)

### `docs/` の中では suffix なしファイルを作らない

- `docs/architecture.md` のような **suffix なし** ファイルは `docs/` 配下では作らない (= どちらの言語かが曖昧)
- `README.md` と `CLAUDE.md` はリポジトリ root に置く例外 (上記)

### 新規 docs を立てるとき

1. **まず `docs/<name>_ja.md` を書く** (原典)
2. AI 翻訳で `docs/<name>_en.md` を生成
3. 両ファイルを 1 つの commit に含める
4. リンクを張る側は **自分の言語と同じ suffix のリンク** を張る
   - `CLAUDE.md` (日本語) → `docs/<name>_ja.md` にリンク
   - `CLAUDE_en.md` (英語) → `docs/<name>_en.md` にリンク
   - `README.md` (英語、原典) → `docs/<name>_en.md` にリンク (英語 reader 向け)

## 新しいプロトコルを追加する

新しい並行性制御 (CC) プロトコルを `cc/<name>/` として足すときの手順。**行番号ベースの指示は意図的に書かない** — 内部リファクタリングのたびに陳腐化するため。代わりに「作業の本体は何か」「どのファイルが何の役割を持つか」「どの仕組みに乗るか」を概念ベースで説明する。アーキテクチャ全体像と TxExecutor 契約の詳細は [architecture_ja.md](architecture_ja.md) を参照。

### 作業の本体: tx 操作に CC アルゴリズムを実装する

**新プロトコルの追加とは、`TxExecutor` のトランザクション操作に並行制御アルゴリズムそのものを実装すること。** ccbench の使命は「共通基盤 (Masstree インデックス、ワークロードテンプレート、計測ハーネス) の上で CC プロトコルを比較する」ことであり、プロトコル間の差分はまさにこれらの操作の中身に宿る。既存プロトコルのディレクトリをコピーするのは**足場 (出発点) を組むだけ**であって、それ自体は作業ではない。コピーして改名して CMake に登録しても、中身が元のプロトコルのままなら「新プロトコル」は存在しない。

実装すべき操作と、その責務、CC アルゴリズムによって中身がどう変わるか:

| 操作 | 共通の責務 | アルゴリズムによって変わるところ |
|---|---|---|
| `read` | キーから可視なデータを取り出して呼び出し側に返す。read-own-writes (自分の write set にあればそれを返す) もここ | **何を「可視」とみなすか**。OCC は最新版を読みつつ TID を read set に控える / 2PL はアクセス時に read ロックを取る / MVCC は自分のタイムスタンプ以前の最新バージョンをバージョンチェーンから選ぶ |
| `update` | 既存レコードへの更新を予約する (レコードが無ければ `WARN_NOT_FOUND`) | **更新をいつ可視にするか**。OCC は write set に積むだけで commit まで遅延 / 2PL はここで write ロック (read ロック保持中なら昇格) / MVCC は pending 状態の新バージョンを生成・連結 |
| `insert` | 新規レコードを作る (既存なら `WARN_ALREADY_EXISTS`) | 新規 `Tuple` の初期化方法。OCC は `absent` ビット付きで作り commit 時に立てる / 2PL は生成時に write ロック済み / MVCC は最初のバージョンを持たせる。Masstree ノードの版数チェックが要るかも (silo の `node_map_`) |
| `delete_record` | レコード削除を予約する | OCC は `absent` を立てて GC キューへ / 2PL は write ロックを取り commit 時にインデックスから外す / MVCC は削除マークの新バージョンを積む |
| `scan` | キー範囲を走査し可視な各レコードを返す。`int64_t limit` 付きと無しの**両方の overload が必須** (concept が要求) | 範囲内の各タプルに対して実質 `read` と同じ判断を、CC アルゴリズムごとに適用する |
| `commit` | トランザクションを確定し、成否を `bool` で返す | **プロトコルの心臓部**。OCC は read set 検証 → write ロック → write phase / 2PL は (ロックは既に全部持っている前提で) write を反映してロック解放 / MVCC は version consistency check → pending バージョンを committed に昇格 |
| `abort` | トランザクションを巻き戻し、確保した資源を解放する | OCC は insert した行を消し read/write set をクリア / 2PL は保持ロックを全解放 / MVCC は pending バージョンを破棄 |

`begin` (タイムスタンプ確保など) や、tuple/version のメモリレイアウト、ロック・検証ロジック、GC (Garbage Collection) も**同じくアルゴリズムに応じて設計し直す対象**。例えば tuple のレイアウトひとつ取っても: silo は `Tuple` に `Tidword` (ロックビット + epoch + TID) を持ち、ss2pl は `ReaderWriteLock` を持ち、mvto は `Tuple` が `atomic<Version*>` のバージョンチェーン先頭と `min_wts_` を持つ。「自分のアルゴリズムでは read/commit に何を書くか、tuple に何を持たせるか」を設計するのがこの作業の中心。

CC ファミリ間の違いを一言でまとめると:

- **OCC (楽観, 例: [cc/silo/](../cc/silo/))** — アクセス中はロックを取らず、`commit` 時にまとめて検証する。`read` は TID を控え、`commit` で「読んだ版が変わっていないか」を確かめて初めて書き込む。
- **2PL (悲観, 例: [cc/ss2pl/](../cc/ss2pl/))** — アクセスした瞬間にロックを取る (read は共有、update は排他)。`commit` 時にやることは write の反映とロック解放だけ。
- **MVCC (多版, 例: [cc/cicada/](../cc/cicada/), [cc/mvto/](../cc/mvto/))** — レコードごとにバージョンチェーンを持ち、タイムスタンプ順序で可視性を決める。`read` はバージョン選択、`update` は新バージョン生成、`commit` は version consistency check。古いバージョンの GC が必須。

決定論的なものは [cc/d2pl/](../cc/d2pl/) を参照。

### 出発点: 既存プロトコルをコピーして足場にする

上記の本体作業をゼロからのファイル作成と一緒にやると大変なので、**目的に一番近い既存プロトコルのディレクトリ (`cc/<proto>/`) を丸ごとコピーし、足場 (出発点) として使う**。コピーが提供してくれるのは「ビルドが通る `TxExecutor` の雛形」「ワークロードテンプレートとの配線」「`result.cc` / `util.cc` などの定型部分」であって、**CC アルゴリズムは付いてこない**。コピー直後の状態は「元のプロトコルの動くコピー」にすぎず、ここから上表の tx 操作・tuple レイアウト・ロック/検証ロジック・GC を、実装したい CC アルゴリズムに合わせて実際に書き換えていくのが作業。「コピーして改名して終わり」では新プロトコルにならない。

どれを足場にするかは、実装したいアルゴリズムが上記どのファミリに近いかで選ぶ:

- 楽観的 (OCC) 系なら [cc/silo/](../cc/silo/) — 一番素直で、ワークロード 4 種すべてに対応している参照実装
- 多版 (MVCC) 系なら [cc/cicada/](../cc/cicada/) や [cc/mvto/](../cc/mvto/)
- 悲観的 / ロックベースなら [cc/ss2pl/](../cc/ss2pl/)
- 決定論的なら [cc/d2pl/](../cc/d2pl/)

かつて `cc_format/` という「単版用テンプレート」ディレクトリがあったが、Makefile ベースで本体 CMake ビルドと乖離し、`README.md` の手順も行番号ベースで陳腐化していたため削除した (#83、議論の経緯は #34)。テンプレートの役割は「既存プロトコルのコピー」と、必要なら AI スキャフォルディング (TxExecutor 契約を満たす雛形を生成させる) で代替する。

### ディレクトリの構成要素

足場としてコピーした `cc/<name>/` を、新プロトコル名と CC アルゴリズムに合わせて書き換える。各ファイルの役割:

| ファイル | 役割 |
|---|---|
| `transaction.cc` / `include/transaction.hh` | **プロトコルの中核**。`include/transaction.hh` が `TxExecutor` クラスを定義し (クラス定義の直後で `static_assert(TxExecutorLike<TxExecutor>);` — [include/tx_executor_concept.hh](../include/tx_executor_concept.hh))、`transaction.cc` が `read` / `update` / `insert` / `delete_record` / `scan` / `commit` / `abort` の実装を持つ。上記「作業の本体」で書き換えるのは主にここ |
| `include/tuple.hh` / `include/version.hh` 等 | `Tuple` / `Version` のメモリレイアウト。ロックビット・タイムスタンプ・バージョンチェーンなど、アルゴリズムが必要とするメタデータをここで設計する |
| `include/*_op_element.hh` | read/write set の要素型。`commit` の検証で何を覚えておく必要があるかで決まる |
| `CMakeLists.txt` | `ccbench_add_protocol(<name> ...)` を 1 回呼ぶだけ。詳細は下記 |
| `<workload>_<name>.cc` | ワークロードごとのエントリポイント (`ycsb_*`, `tpcc_*`, `bomb_*`, `sbomb_*`)。`worker()` を定義し、対応するワークロードテンプレート ([include/ycsb.hh](../include/ycsb.hh), [include/tpcc.hh](../include/tpcc.hh), [include/bomb.hh](../include/bomb.hh) 等) を駆動する。`CMakeLists.txt` の `WORKLOADS` に挙げたタグ分だけ必要 |
| `result.cc` | スレッドごとの集計結果バッファ (`<Name>Result` ベクタ) と `initResult()` の定義 |
| `util.cc` / `include/util.hh` | DB 初期化、レコードの初期値設定、リーダースレッドの仕事 (`leaderWork`) など |

### 足場を組んだ後の配線: `ccbench_add_protocol()` に乗せる

ビルドは [cmake/ProtocolHelpers.cmake](../cmake/ProtocolHelpers.cmake) の `ccbench_add_protocol()` ヘルパーが宣言的に組み立てる。`cc/<name>/CMakeLists.txt` はこのヘルパーを 1 回呼ぶだけで済む:

```cmake
ccbench_add_protocol(<name>
  SOURCES   transaction.cc util.cc result.cc   # ワークロード間で共有する .cc。エントリポイントは入れない
  WORKLOADS ycsb tpcc bomb sbomb               # サポートするワークロードタグの部分集合
  OPTIONS                                      # プロトコル固有の -D defines (任意)
    FOO=${CCBENCH_FOO}
)
```

- ヘルパーは `WORKLOADS` の各タグ `W` について `W_<name>.exe` を `W_<name>.cc` + `SOURCES` からビルドし、`ccbench_common` + `ccbench::masstree` + `ccbench::mimalloc` をリンクする。
- 汎用 `-D` フラグ (`KEY_SIZE`, `VAL_SIZE`, `BACK_OFF`, `ADD_ANALYSIS`, `MASSTREE_USE` 等) と `-Wall -Wextra -Werror` は自動で付く。プロトコル固有の cache オプションを増やすときは [cmake/Options.cmake](../cmake/Options.cmake) に追加する。
- 最後に、トップレベル [CMakeLists.txt](../CMakeLists.txt) の `foreach(_proto …)` ループに新しいディレクトリ名を 1 つ追加する。これで configure 時に [build/PROTOCOL_MATRIX.md](../build/PROTOCOL_MATRIX.md) にも自動で行が増える。

### TxExecutor 契約による強制

上記「作業の本体」で書き換える tx 操作は、ワークロードテンプレートが期待する `TxExecutor` API を満たさなければならない。この契約は [include/tx_executor_concept.hh](../include/tx_executor_concept.hh) の `TxExecutorLike` concept として表現され、各プロトコルが `transaction.hh` 末尾の `static_assert(TxExecutorLike<TxExecutor>);` で**コンパイル時に強制**する。メソッドの欠落やシグネチャ不一致 (例: `scan` の `int64_t limit` 付き overload 忘れ) は、そのプロトコル自身のビルドが named diagnostic 付きで失敗するので、実行時クラッシュにはならない。concept はあくまで「シグネチャという外形」を縛るだけで、各操作の中身に正しい CC アルゴリズムが入っているかは保証しない — そこは実装者の責任。契約の各メソッドの意味は [architecture_ja.md](architecture_ja.md) を参照。

### チェックリスト

CC アルゴリズムの実装 (本体):

- [ ] `read` / `update` / `insert` / `delete_record` / `scan` / `commit` / `abort` を、実装する CC アルゴリズムに合わせて書き換えた (コピー元のロジックが残っていない)
- [ ] tuple/version レイアウト・ロック/検証ロジック・GC をアルゴリズムに合わせて設計し直した
- [ ] `scan` の `int64_t limit` 付き・無し両 overload がある

足場・配線:

- [ ] `cc/<name>/` を目的に近い既存プロトコルからコピーして名前を書き換えた
- [ ] `cc/<name>/CMakeLists.txt` が `ccbench_add_protocol(<name> ...)` を呼んでいる
- [ ] トップレベル `CMakeLists.txt` の `foreach(_proto …)` に `<name>` を追加した
- [ ] `transaction.hh` 末尾に `static_assert(TxExecutorLike<TxExecutor>);` がある
- [ ] `cmake -S . -B build` が通り、`WORKLOADS` に挙げた各バイナリがビルドできる
- [ ] [docs/protocols_ja.md](protocols_ja.md) のプロトコル表に行を足した (原典なので `_en` もセットで)

## PR を出すとき (TODO)

PR の出し方の規約はまだ書いていない。気付いた点があれば追記。

参考になる既存 PR (タイトル / body の書き方):
- [#44](https://github.com/thawk105/ccbench/pull/44) (`-Werror=maybe-uninitialized`) — 修正の "なぜ" を含む best-practice 系
- [#50](https://github.com/thawk105/ccbench/pull/50) (`-Werror=unused-label`) — dead code 削除 + 真のバグ修正の併記
- [#52](https://github.com/thawk105/ccbench/pull/52) (`docs/coding-conventions.md` 新設) — 規約系のメタな PR

---

## コミットメッセージ (TODO)

ルールは現状未文書化。既存履歴の見た目を踏襲することを推奨。

`Co-Authored-By` trailer は **付けない** (Claude を含めて、GitHub の avatar 付きで commit が膨らむのを避けるため)。
