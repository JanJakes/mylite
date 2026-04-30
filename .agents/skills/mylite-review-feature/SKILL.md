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

Use subagents only when the active session and tool policy allow them and the
review has independent surfaces worth splitting, such as MySQL runtime
behavior, docs, and tests.

## Review checklist

1. Read `README.md`, `AGENTS.md`, `COMPATIBILITY.md`, the design document, and
   the implementation diff.
2. Confirm the feature has `docs/specs/<feature-name>/specs.md` with an
   independently authored feature spec, MyLite Lemon-syntax grammar snippets
   where applicable, MySQL-runtime-verified test expectations, and
   compatibility links.
3. Verify the feature against MySQL 8.4.9 behavior: results, errors, warnings,
   metadata, side effects, SQL modes, and edge cases.
4. Check grammar coverage first. MyLite's grammar should be independently
   implemented from official MySQL docs and runtime behavior, cover the
   Lemon-syntax snippets in `specs.md` for the feature scope, and avoid copied
   restrictively licensed documentation, grammar text, or implementation
   sources.
5. Check architecture boundaries: parser/AST, analysis, metadata, translation,
   runtime hooks, SQLite usage, storage format, and integration packages.
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
