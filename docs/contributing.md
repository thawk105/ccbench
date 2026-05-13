# Contributing

このリポジトリで issue を立てる / PR を出す / コミットを書くときの規約。コーディングルール (ファイル種別ごとの best practice) は [coding-conventions.md](coding-conventions.md) を見ること。

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
