---
name: mylite-dont-stop
description: Persist through substantial MyLite compatibility implementation, migration, or multi-step feature work when the user asks the agent to keep going despite broad scope. Do not use for simple git commands, status checks, one-off reviews, small documentation edits, or ordinary amend/push requests.
---

# MyLite don't stop

Use this skill when the next instinct is to pause, phase the work, propose a
smaller subset, or ask whether to continue. The default is to continue.

## Trigger boundary

Use this skill only for substantial MyLite compatibility work where persistence
is the point: broad implementation, difficult migrations, multi-step feature
completion, or requests to keep working across more than one natural stopping
point.

Do not use this skill for simple commands or narrow maintenance work, including
`git status`, `date`, `amend it`, `push`, routine commits, routine pushes,
single-file copy edits, small documentation corrections, lightweight Q&A, or
ordinary review prompts that fit `mylite-review-feature`. Execute those
requests directly.

## Hard-part first

Before acting, identify the hardest part in one sentence. Make one concrete
technical call in two lines. Then do the work.

## Rules

- Produce the artifact, not a proposal about the artifact.
- Attack the central compatibility/design/implementation problem first; let
  scaffolding follow.
- For new compatibility features, do not skip the `docs/specs/<feature-name>/`
  package, independently authored `specs.md`, applicable MyLite Lemon-syntax
  grammar snippets, or MySQL-runtime-verified test expectations.
- Use official MySQL 8.4 Reference Manual pages, preferably URLs under
  `https://dev.mysql.com/doc/refman/8.4/`, and real MySQL 8.4.9 runtime
  probes as the compatibility authority. Do not rely on MySQL 8.3, 8.0,
  Innovation release, MariaDB, Percona, or copied implementation sources for
  normative behavior.
- Persistence does not override subagent limits. Use subagents only when the
  active session, tool policy, and user request explicitly allow delegation; cap
  live subagents at one implementer plus one reviewer/verifier, and close a
  stale timed-out agent after one follow-up wait instead of blocking
  indefinitely.
- Do not split work across sessions unless the user explicitly asks.
- Do not offer a smaller version when the requested version can be done.
- Do not ask preference questions when a reasonable default exists.
- Do not stop after analysis when code, tests, docs, commits, or pushes are
  still needed for the user's request.

## Guardrail

Ask exactly one focused question only when progress literally requires
information only the user can provide: missing code or files, credentials,
endpoints, or an unspecified hard constraint. State what will be produced when
the answer arrives, then stop.

If no guardrail applies, keep working until the request is implemented,
verified, documented, committed, and pushed when that is part of the active
workflow.
