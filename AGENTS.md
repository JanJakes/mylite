# Agent Guidance

MyLite is an embedded MySQL compatibility layer built on SQLite. Keep changes
focused on that goal.

Read the [README.md](README.md) file of this repository and stick to the following rules:

- Preserve MySQL 8 LTS behavior, currently tested against MySQL 8.4.9.
- Design features before implementing them, then update compatibility docs and
  tests alongside the code.
- Prefer clear architecture, small binaries, and minimal dependencies.
- Treat the `.mylite` single-file format and SQLite offset model carefully.
- Keep unrelated refactors out of focused changes.

## Goals

MyLite should power MySQL-oriented applications without modifications. Always
stick to the project goals:

- **MySQL drop-in:** Work as an effortless drop-in replacement for MySQL.
- **Single file:** Keep the database portable as a single `.mylite` file.
- **Full grammar support:** Parse 100% of MySQL grammar.
- **Uncompromising compatibility:** Implement the MySQL API surface that real applications depend on.
- **Correctness:** Mirror MySQL semantics for expressions, statements, operations, types, values, errors, etc.
- **Extensive test suite:** Create and maintain a large test suite.
- **Coverage matrix:** Track MySQL functionality coverage in a detailed document.

## Communication

- Be direct, clear, accurate, and concise.
- Avoid overly enthusiastic affirmations.
- Explain your reasoning when making non-obvious choices.
- If uncertain, say so clearly rather than agreeing prematurely.

## Problem-solving approach

- Investigate and diagnose potential issues before confirming they exist.
- Ask clarifying questions only when the situation genuinely needs more context.
- Acknowledge complexity when it matters.
- Never skip or remove failing tests. Always address the underlying issue.
- Never ignore linter and static analysis checks. Always address the underlying issue.
- When asked to do a difficult task, do the difficult task. Do not pivot to an
  easier workaround unless explicitly asked.
- Do not stop until the requested work is done or a real blocker is reached.

## Coding practices

- Hold a high quality bar.
- Write clean, minimal, self-documenting code.
- Account for performance.
- Add comments when additional context is needed. Avoid them when the code is
  self-explanatory.
- Comments must be accurate and brief. Avoid long explanations.
- Preserve existing important comments when they still matter.
- Add extensive test coverage to verify the implementation.
- **Function ordering:** First caller, then callee. When function A calls function B, write first A, then B.
- **Method ordering:** First public, then protected, then private. Respect **Function ordering** as well.

## Engineering principles

- **Correctness first:** MyLite should produce MySQL-equivalent behavior, even in edge cases.
- **Design before implementation:** Each feature should be specified, evaluated, and tested before it becomes part of the core surface.
- **Clear architecture:** The codebase should stay carefully organized as compatibility grows.
- **Performance matters:** Compatibility work must not hide avoidable overhead.
- **Lean binary:** Third-party dependencies should be avoided unless they clearly earn their cost, so MyLite stays suitable for embedding alongside an application binary.
- **SQLite deliberately:** Built-in and optional SQLite extensions should be evaluated for size, behavior, and fit with MySQL semantics before being enabled by default.

## Compatibility management

Compatibility is a first-class product surface in MyLite. It is tracked
explicitly rather than inferred from implementation progress.

1. **Catalog:** Every MySQL feature must be cataloged in [COMPATIBILITY.md](COMPATIBILITY.md), with its current support status.
2. **Tests:** Every supported behavior must be covered by tests that compare MyLite against a real MySQL runtime, including result sets, errors, warnings, metadata, and side effects.
3. **Explicit incompatibilities:** Features that do not fit MyLite's embedded design must be documented explicitly. Where possible, MyLite should accept the syntax and return a predictable warning or placeholder behavior so applications do not fail unexpectedly.

## Implementing a new feature

When a new feature is added, the following recipe must be followed:

1. **Research** MySQL documentation, repository, and other resources to understand the feature in depth.
2. **Design** the architecture and implementation and save it in a design document.
3. **Document** the feature in the compatibility matrix and in a guide document.
4. **Prepare** an extensive test suite covering the feature from standard paths to the most exotic edge cases. Expected outputs must be tested against real MySQL runtime.
5. **Implement** the feature end-to-end. Meticulously care about correctness, architecture, code quality, performance, and bundle size.
6. **Test** the feature extensively, extend the test coverage if needed, and make sure no regressions were introduced.
7. **Review** the implementation thoroughly and improve it if needed.

When a functionality is fundamentally incompatible with MyLite design,
it must be explicitly acknowledged, and a reasonable placeholder behavior
should be implemented, preventing applications from malfunctioning.

## Git guidelines

Make frequent, clearly defined, atomic commits with concise and clear commit
message headlines. Add brief details in the commit body if needed.

- One commit, one change. Commit a small unit of related changes.
- Make the subject of a commit message short but clear.
- Start with a verb and use present-tense, imperative style.
- Explain details in a commit body below, if needed. Be accurate, but brief.
  Avoid long descriptions.
- Include links in the body if the change relates to external sources.
- Make commits readable for humans, not machines.
