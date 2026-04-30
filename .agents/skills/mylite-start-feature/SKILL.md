---
name: mylite-start-feature
description: Research and design a new MyLite MySQL-compatibility feature before implementation, producing docs/specs/<feature-name>/specs.md with an independently authored spec, MyLite Lemon-syntax grammar snippets where applicable, a compatibility-matrix update, and MySQL-runtime-verified test expectations. Use only for substantial feature-start work, not simple docs edits, reviews, commits, pushes, or status commands.
---

# MyLite start feature

Use this skill before implementation. Produce the research, design, docs, and
test plan that make the implementation straightforward and verifiable.

## Trigger boundary

Use this skill when the user asks to start, research, design, specify, or
prepare tests for a new or materially expanded MyLite compatibility feature.
The expected output should include feature research, an independently authored
`specs.md`, MyLite Lemon-syntax grammar snippets where applicable, compatibility
documentation, and MySQL-verified test expectations.

Do not use this skill for simple commands, routine git operations, small
documentation corrections, compatibility-matrix copy edits, post-implementation
reviews, or questions that can be answered without opening a new feature plan.

## Planning and context

Use Plan mode for substantial start-feature work when it is available in the
active session. The plan should make the documentation research, runtime
verification, spec authoring, test strategy, and implementation handoff
explicit before editing files.

Do not infer subagent permission from this skill. Use subagents only when the
active session, tool policy, and user request explicitly allow delegation. When
allowed, cap live subagents at two: one active worker for a bounded research
stream and one reviewer/verifier. Do not run many concurrent reviewers. Keep
subagent tasks bounded and avoid delegating the immediate blocking task.

Treat a subagent as stale when a wait times out with no final status and no
actionable output. After one timeout, ask for a concise finish/status only if
the result still matters; after a second timeout, or if the response still does
not materially advance the task, close that agent and continue locally.

## Source discipline

Use official MySQL 8.4 Reference Manual pages, preferably URLs under
`https://dev.mysql.com/doc/refman/8.4/`, and real MySQL 8.4.9 runtime
probes as the compatibility authority. Do not use MySQL 8.3, 8.0, Innovation
release, MariaDB, Percona, or copied implementation sources as normative
behavior. If a search or browser opens the wrong MySQL documentation version,
discard that page and replace it with the matching 8.4 page before relying on
it. Record exact runtime probe commands and observed behavior in the feature
artifacts when they define expectations.

## Workflow

1. Read `README.md`, `AGENTS.md`, and `COMPATIBILITY.md`.
2. Choose the feature slug and create `docs/specs/<feature-name>/`, where
   `<feature-name>` is lower-cased and hyphenated, such as `select-base` or
   `select-cte`.
3. Identify the exact MySQL 8 LTS feature surface: syntax, modes, metadata,
   errors, warnings, edge cases, and interactions with DDL, casts, functions,
   and information schema.
4. Research official MySQL 8.4.9 documentation and verify real MySQL 8.4.9
   behavior for the feature surface. Use external sources only as secondary
   context and only when their licenses keep MIT, BSD, and similar licensing
   options available.
5. Write `specs.md` in the feature directory. It must be MyLite's
   independently authored specification, with functionality, syntax, semantics,
   metadata, errors, warnings, SQL modes, edge cases, interactions, parser/AST
   needs, runtime behavior, SQLite extension API fit, targeted SQLite extension
   point needs, storage impact, performance concerns, and known
   incompatibilities. Do not copy documentation, grammar text, or restrictively
   licensed implementation sources.
   When SQLite behavior is involved, explicitly classify the approach as:
   public SQLite extension API, MyLite wrapper/translation, or targeted SQLite
   fork hook. Consult `third_party/sqlite/README.md` for the fork layout and
   patch-stack rules.
6. Include MyLite Lemon-syntax grammar snippets in `specs.md` where grammar
   applies. These snippets describe MyLite's intended grammar and must be
   independently authored from MySQL docs and runtime behavior.
7. Prepare tests before implementation. Expected results must be verified
   against a real MySQL runtime, including result sets, errors, warnings,
   metadata, and side effects. Do not start implementation until `specs.md`
   exists and covers the intended feature scope.
8. Update `COMPATIBILITY.md` with the feature, status, scope, and links to the
   design/test artifacts.
9. If MySQL 8.4.9 is unavailable, set it up, usually with Docker. Do not invent
   expected compatibility results.
10. Add extra notes or artifacts under the feature directory only when storing
    them does not violate any license or narrow future licensing options.

## Done

- Design exists and is specific enough to implement.
- Feature docs live in `docs/specs/<feature-name>/specs.md`.
- `specs.md` is independently authored from official MySQL 8.4.9 docs and
  MySQL-runtime behavior, without copied documentation or grammar text.
- Applicable MyLite Lemon-syntax grammar snippets are included in `specs.md`.
- Compatibility status is documented.
- Tests or test fixtures are prepared with MySQL-verified expectations.
- MySQL-runtime verification is performed against MySQL 8.4.9, not guessed.
- Remaining risks and intentionally unsupported behavior are explicit.
