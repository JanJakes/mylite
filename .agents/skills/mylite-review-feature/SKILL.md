---
name: mylite-review-feature
description: Review a completed or near-complete MyLite MySQL-compatibility feature as a release gate for MySQL behavior, tests, docs, architecture, and readiness. Use only for substantive feature reviews, not simple amend/push commands, status checks, or small documentation copyedits.
---

# MyLite review feature

Review as a release gate. Find gaps, fix what is safe to fix, and make the
feature line up with the design, tests, and compatibility matrix.

## Trigger boundary

Use this skill when the user asks for a thorough, final, release-gate, or
compatibility-focused review of a MyLite feature, implementation diff, or
substantial compatibility area.

Do not use this skill for simple git commands, amend/push requests, status
checks, lightweight Q&A, or small documentation copyedits. For ordinary code
reviews that are not MyLite compatibility release gates, use the normal review
stance without loading this skill.

Do not infer subagent permission from this skill. Use subagents only when the
active session, tool policy, and user request explicitly allow delegation. When
allowed, cap live subagents at two: one active worker for a bounded verification
surface and one reviewer/verifier. Do not run many concurrent reviewers.

Treat a subagent as stale when a wait times out with no final status and no
actionable output. After one timeout, ask for a concise finish/status only if
the result still matters; after a second timeout, or if the response still does
not materially advance the task, close that agent and continue locally.

## Source discipline

Use official MySQL 8.4 Reference Manual pages, preferably URLs under
`https://dev.mysql.com/doc/refman/8.4/`, and real MySQL 8.4.9 runtime
probes as the compatibility authority. Do not accept MySQL 8.3, 8.0,
Innovation release, MariaDB, Percona, or copied implementation sources as
normative behavior. If a review uncovers expectations based on the wrong MySQL
documentation version, treat that as a blocking evidence gap until replaced by
8.4 documentation or MySQL 8.4.9 runtime probes.

## Review checklist

1. Read `README.md`, `AGENTS.md`, `COMPATIBILITY.md`, the design document, and
   the implementation diff.
2. Confirm the feature has `docs/specs/<feature-name>/specs.md` with an
   independently authored feature spec, MyLite Lemon-syntax grammar snippets
   where applicable, MySQL-runtime-verified test expectations, and
   compatibility links.
3. Verify the feature against MySQL 8.4.9 behavior: results, errors, warnings,
   metadata, side effects, SQL modes, and edge cases.
4. When syntax is involved, check grammar coverage first. MyLite's grammar
   should be independently implemented from official MySQL docs and runtime
   behavior, cover the Lemon-syntax snippets in `specs.md` for the feature
   scope, and avoid copied restrictively licensed documentation, grammar text,
   or implementation sources.
5. Check architecture boundaries: parser/AST, analysis, metadata, translation,
   runtime hooks, SQLite extension APIs, targeted SQLite extension points,
   storage format, and integration packages.
   If SQLite fork changes are present, confirm they are narrow hooks, documented
   in `third_party/sqlite/patches/`, apply to `third_party/sqlite/upstream/`,
   are reflected in `third_party/sqlite/amalgamation/`, and keep MyLite
   compatibility logic on the MyLite side.
6. Check tests for depth and relevance. Add tests for missing MySQL cases,
   regressions, or high-risk paths when the fix is clear; otherwise report the
   gap as blocking.
7. Check documentation: compatibility matrix, guide/design docs, comments, and
   intentionally unsupported behavior.
8. Run relevant tests and checks. Fix failures when the fix is clear; otherwise
   report the blocker with the failing command and evidence.
9. Review final diff quality: focused scope, no unrelated refactors, performance
   awareness, lean dependency footprint, readable commit shape.

## Output

- Lead with blocking findings, then fixes made.
- State tests/checks run.
- State remaining risks, if any.
- Do not mark complete until docs, tests, and compatibility status agree.
