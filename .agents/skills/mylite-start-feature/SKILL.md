---
name: mylite-start-feature
description: Research and design a new MyLite MySQL-compatibility feature before implementation, producing a design note, compatibility-matrix update, and MySQL-runtime-verified test expectations. Use only for substantial feature-start work, not simple docs edits, reviews, commits, pushes, or status commands.
---

# MyLite Start Feature

Use this skill before implementation. Produce the research, design, docs, and
test plan that make the implementation straightforward and verifiable.

## Trigger boundary

Use this skill when the user asks to start, research, design, specify, or
prepare tests for a new or materially expanded MyLite compatibility feature.
The expected output should include feature research, design decisions,
compatibility documentation, and MySQL-verified test expectations.

Do not use this skill for simple commands, routine git operations, small
documentation corrections, compatibility-matrix copy edits, post-implementation
reviews, or questions that can be answered without opening a new feature plan.

## Planning and context

Use Plan mode for substantial start-feature work when it is available in the
active session. The plan should make the source research, runtime verification,
design artifact, test strategy, and implementation handoff explicit before
editing files.

Use subagents only when the active session and tool policy allow them and the
work has independent research streams, such as MySQL docs, MySQL source/tests,
runtime behavior, and existing MyLite architecture. Keep subagent tasks bounded
and avoid delegating the immediate blocking task.

## Workflow

1. Read `README.md`, `AGENTS.md`, and `COMPATIBILITY.md`.
2. Identify the exact MySQL 8 LTS feature surface: syntax, modes, metadata,
   errors, warnings, edge cases, and interactions with DDL, casts, functions,
   and information schema.
3. Research primary sources first: official MySQL docs, MySQL source/tests, and
   real MySQL 8.4.9 behavior. Use external sources only as secondary context.
4. Write a concise design document in the repo-appropriate docs location,
   covering semantics, parser/AST needs, metadata requirements, SQLite
   translation strategy, runtime hooks, storage impact, performance concerns,
   and known incompatibilities.
5. Update `COMPATIBILITY.md` with the feature, status, scope, and links to the
   design/test artifacts.
6. Prepare tests before implementation. Expected results must be verified
   against a real MySQL runtime, including result sets, errors, warnings,
   metadata, and side effects.
7. If MySQL 8.4.9 is unavailable, set it up, usually with Docker. Do not invent
   expected compatibility results.

## Done

- Design exists and is specific enough to implement.
- Compatibility status is documented.
- Tests or test fixtures are prepared with MySQL-verified expectations.
- MySQL-runtime verification is performed against MySQL 8.4.9, not guessed.
- Remaining risks and intentionally unsupported behavior are explicit.
