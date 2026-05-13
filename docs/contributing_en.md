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
