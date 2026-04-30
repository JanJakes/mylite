# Agent guidance

MyLite is an embedded MySQL compatibility layer built on SQLite. Keep changes
focused on that goal.

Read the [README.md](README.md) file of this repository and stick to the following rules:

- Preserve MySQL 8 LTS behavior, currently tested against MySQL 8.4.9.
- Design and specify features before implementing them, then update
  compatibility docs and tests alongside the code.
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
- **Freedom:** Keep the implementation independently authored and free of
  restrictions that would limit our license choices.

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

The project workflows for this recipe are split across `mylite-start-feature`,
`mylite-implement-feature`, and `mylite-review-feature`. Use `mylite-work-hard`
for batch compatibility work and `mylite-dont-stop` when substantial work must
continue through multiple phases.

1. **Start** the work by creating a feature spec directory at
   `docs/specs/<feature-name>/`, where `<feature-name>` is lower-cased and
   hyphenated, such as `select-base` or `select-cte`.
2. **Research** the feature from official MySQL 8.4.9 documentation and
   observed MySQL runtime behavior. Use other sources only when their licenses
   keep MIT, BSD, and similar licensing options available.
3. **Specify** the feature by creating `specs.md` in the feature directory with
   MyLite's independently authored feature specification. Include functionality,
   syntax, semantics, metadata, errors, warnings, SQL modes, edge cases,
   interactions, storage/runtime implications, and MyLite-specific
   compatibility decisions. Do not copy documentation, grammar text, or
   restrictively licensed implementation sources. When applicable, include
   MyLite Lemon-syntax grammar snippets. These snippets describe MyLite's
   intended grammar and must be independently authored from MySQL docs and
   runtime behavior.
4. **Prepare** an extensive test suite covering the feature from standard paths
   to exotic edge cases. Expected outputs must be verified against a real MySQL
   8.4.9 runtime before implementation.
5. **Implement** the feature end-to-end, starting with grammar support, then
   AST/analyzer/runtime behavior, metadata, docs, and compatibility updates.
   Meticulously care about correctness, architecture, code quality,
   performance, and bundle size.
6. **Test** the feature extensively, extend coverage if needed, and make sure
   no regressions were introduced.
7. **Review** the implementation thoroughly and improve it if needed.

The feature directory may contain additional notes or artifacts only when
storing them does not violate any license or narrow future licensing options.

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
