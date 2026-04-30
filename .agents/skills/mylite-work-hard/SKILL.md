---
name: mylite-work-hard
description: Continuously advance MyLite compatibility by selecting and implementing batches of unimplemented features from COMPATIBILITY.md with subagents. Use only when the user explicitly asks for ongoing/batch compatibility work such as "work hard", "next batch", "keep advancing compatibility", or continuing beyond one completed feature. Do not use for simple git commands or one-off edits.
---

# MyLite work hard

Move MyLite compatibility forward continuously. Pick a batch, finish it end to
end, commit and push, then pick the next batch.

## Trigger boundary

Use this skill only when the user explicitly asks for ongoing or batch MyLite
compatibility progress: "work hard", "next batch", "continue with the next
batch", "keep advancing compatibility", "pick features", or similar wording
that clearly means more than one isolated feature or command.

Do not use this skill for simple commands, routine commits, amend/push requests,
single-feature implementation, review-only prompts, small docs edits, or status
checks.

## Planning and context

Use Plan mode at the start of a batch when it is available in the active
session. Choose the feature batch, define ownership, identify verification
commands, and decide which work is suitable for subagents before editing files.

Use subagents only when the active session and tool policy allow them. Give each
subagent a disjoint feature or file ownership, and keep local work on the
critical path moving while they run.

## Preflight

- Read the current branch and `git status` before starting.
- Do not overwrite unrelated user changes. If the worktree is mixed, isolate the
  batch or ask one focused question.
- Ensure the MySQL 8.4.9 comparison runtime is available. If it is missing,
  set it up, usually with Docker, and record the command used.
- Commit and push completed batch work.

## Operating loop

1. Read `README.md`, `AGENTS.md`, and `COMPATIBILITY.md`.
2. Pick a small coherent batch of not-yet-implemented features. Prefer related
   parser/metadata/runtime areas so tests and design work reinforce each other.
3. For each feature, run the full path:
   - use `mylite-start-feature` to research, design, document, and prepare
     MySQL-verified tests, including the required
     `docs/specs/<feature-name>/specs.md`;
   - use `mylite-implement-feature` to implement grammar support first, then
     runtime behavior and verification;
   - use `mylite-review-feature` to close documentation, tests, and review gaps.
4. Use subagents for feature work in this skill when the active session allows
   them. Give each subagent a disjoint feature and file ownership. Tell each one
   the repo has other active work and it must not revert others' changes.
5. Integrate completed work locally. Run relevant tests and checks for the whole
   batch, fix failures, commit, and push.
6. When a batch is complete, immediately select the next batch and continue.

## Persistence rule

Do not stop because the work is broad, repetitive, or difficult. Set up missing
local tooling when possible, including MySQL 8.4.9 via Docker. Stop only for a
real blocker that cannot be solved locally: missing credentials, unavailable
network or runtime infrastructure, an unclear user constraint that cannot be
defaulted, or a failing external dependency that cannot be worked around
honestly. Otherwise keep implementing, testing, reviewing, committing, pushing,
and continuing.
