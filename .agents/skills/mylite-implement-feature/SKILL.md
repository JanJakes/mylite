---
name: mylite-implement-feature
description: Implement a specific MyLite MySQL-compatibility feature end to end after design/test expectations exist, including code, tests, docs, verification, and self-review. Use only for substantive feature implementation, not simple git operations, small docs edits, generic reviews, or status requests.
---

# MyLite Implement Feature

Implement the feature completely. Do not stop at scaffolding, partial behavior,
or a TODO trail when the requested feature can be finished.

## Trigger boundary

Use this skill when the user asks to implement a specific MyLite compatibility
feature or materially expand an existing compatibility feature through code,
tests, docs, and verification.

Do not use this skill for simple commands, routine commits, amend/push requests,
small compatibility-matrix edits, lightweight bug triage, review-only prompts,
or broad batch work that belongs to `mylite-work-hard`.

## Planning and context

Begin substantial implementation work in Plan mode when it is available in the
active session. The plan must confirm the design, test expectations, affected
layers, verification commands, and subagent ownership before implementation
edits. If Plan mode is unavailable, write the same plan as a concise working
checklist before editing.

Use subagents only when the active session and tool policy allow them and the
implementation can be split into disjoint responsibilities, such as parser/AST,
runtime semantics, metadata, tests, or documentation. Do not delegate the next
critical-path task when local progress depends on its result.

## Workflow

1. Start in Plan mode for substantial implementation work when the active
   session supports it; otherwise write the implementation checklist locally
   before editing.
2. Read `README.md`, `AGENTS.md`, `COMPATIBILITY.md`, and the feature design.
   If the design, compatibility entry, or MySQL-verified test expectations are
   missing, use `mylite-start-feature` first instead of improvising.
3. Reconfirm the intended MySQL 8.4.9 behavior and test expectations before
   touching implementation code.
4. Implement through the right layer: parser/AST, analyzer, metadata,
   translation, SQLite runtime hook, C function, file format, or integration
   package. Keep dependencies minimal and architecture explicit.
5. Add or refine tests while implementing. Cover happy paths, edge cases,
   errors, warnings, metadata, side effects, and regression-sensitive behavior.
6. Run the relevant test suite and static checks. Fix failures at the root.
7. Update `COMPATIBILITY.md`, feature docs, and comments where behavior changed.
8. Self-review the diff for MySQL equivalence, architecture, performance, binary
   size, missing tests, and accidental broad changes.

## Done

- Feature works end to end.
- Tests pass and are compared to MySQL where required for compatibility.
- Documentation and compatibility status match the implementation.
- The final diff is focused, reviewed, and ready to commit.
