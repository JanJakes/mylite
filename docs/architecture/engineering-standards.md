# Engineering standards

This document records MyLite's first engineering standards for C code, public
ABI, dependencies, tests, and compatibility work. These rules are intended to
keep MyLite embeddable, portable, and MySQL-compatible as the implementation
grows.

## Public C API

- Public functions use the `mylite_*` prefix.
- Public constants and macros use the `MYLITE_*` prefix.
- Public handles are opaque lowercase C types:

  ```c
  typedef struct mylite_db mylite_db;
  typedef struct mylite_stmt mylite_stmt;
  ```

- Public functions return integer status codes unless they are simple
  infallible accessors.
- Detailed diagnostics are retrieved from the relevant handle, not from
  process-global state.
- Public structs are exposed only when their layout is intentionally stable.
  Growable public structs should include a size or version field.
- `mylite/mylite.h` is the main public umbrella header. Smaller public headers
  may be added when the API surface grows, but users should be able to include
  `mylite/mylite.h`.
- Public headers should be C++ compatible where relevant.

## Symbol Visibility

- First-party library symbols are hidden by default.
- Public ABI functions are exported with an explicit `MYLITE_API` macro.
- Internal functions are not exported.
- Static helpers remain `static`.

## Internal Naming

- Internal functions, structs, and enums use module-prefixed snake case:

  ```c
  struct mylite_sql_token;
  enum mylite_sql_token_kind;
  int mylite_sql_lexer_next(struct mylite_sql_lexer *lexer,
                            struct mylite_sql_token *out_token);
  ```

- Internal structs and enums are not typedefed by default.
- File-local static helpers may use shorter unprefixed names when context is
  clear.
- C source and header files use `snake_case.c` and `snake_case.h`.
- Documentation slugs may use hyphens when they are primarily human-facing.
- Upstream filenames under `third_party/` are preserved.

## Headers And Includes

- Public headers live under `packages/libmylite/include/mylite/`.
- Internal headers live under `packages/libmylite/src/` or its subdirectories.
- Every header is self-contained.
- Files include the headers that define the symbols they use.
- Public headers are included with angle brackets:

  ```c
  #include <mylite/mylite.h>
  ```

- Internal headers are included with quotes:

  ```c
  #include "source_span.h"
  ```

- Use traditional include guards, not `#pragma once`:

  ```c
  #ifndef MYLITE_SQL_LEXER_H
  #define MYLITE_SQL_LEXER_H

  #endif
  ```

## Formatting And Comments

- `clang-format` is authoritative for first-party C formatting.
- Current first-party style uses 4-space indentation, LF line endings,
  traditional C function braces, and a 100-column limit.
- Do not format vendored SQLite or Lemon source.
- Comments should explain rationale, constraints, invariants, or compatibility
  surprises.
- Avoid comments that narrate obvious mechanics.
- Long semantic explanations belong in specs, tests, or architecture docs.
- Stable public APIs should eventually get concise header comments describing
  ownership, lifetime, nullability, return codes, and important side effects.

## Function Organization

- Public functions come first.
- Internal non-static functions come next.
- Static helpers follow their callers.
- Prefer caller-before-callee ordering.
- File-local helper prototypes are allowed when they make the file clearer, but
  cycles should usually be refactored first.
- Group code by behavior, not alphabetically.

## Error Handling And Diagnostics

- `0` means success for general public APIs.
- Public status constants use explicit `MYLITE_*` values.
- Invalid public API usage returns `MYLITE_MISUSE`.
- Allocation failure returns `MYLITE_NOMEM`.
- Public APIs validate required pointer arguments.
- Cleanup functions such as `mylite_close(NULL)` and
  `mylite_finalize(NULL)` should be no-ops unless a strong reason says
  otherwise.
- MySQL-compatible diagnostics must eventually include numeric error codes,
  SQLSTATE, messages, warnings, affected rows, insert ids, metadata, and side
  effects where relevant.
- Detailed diagnostics are handle-owned rather than process-global.

## Memory And Ownership

- Inputs are borrowed for the duration of the call unless documented otherwise.
- Handles returned by MyLite are owned by the caller and released with matching
  MyLite cleanup functions.
- Heap memory returned to callers must be freed through `mylite_free()`, not
  plain `free()`.
- Internal allocations should go through project allocator wrappers once memory
  management grows beyond trivial scaffolding.
- Output parameters use the `out_` prefix.
- Required output parameters must be non-NULL.
- On success, output parameters are initialized.
- On failure, output parameters are set to safe defaults where practical.
- Non-trivial objects use explicit init/deinit functions.
- Deinit functions should tolerate zero-initialized objects where practical.
- Use early returns for simple validation and `goto cleanup` for functions that
  own multiple resources.

## Types And Values

- Use `<stdbool.h>` `bool` internally.
- Use `int` booleans in the public ABI.
- Use fixed-width integers from `<stdint.h>` when exact width matters.
- Use `size_t` for memory sizes and buffer lengths.
- Avoid plain `long` in public APIs and serialized formats.
- Use public MyLite integer typedefs only when the API needs them.
- Use internal enums freely for typed domains.
- Public constants and enum values must have explicit stable numeric values.
- Public flags use integer bit masks, not C bitfields.

## Strings

- Internal code should prefer length-aware borrowed string views.
- Public APIs may provide NUL-terminated convenience forms where useful.
- Core APIs should have length-aware forms for SQL text, identifiers, blobs,
  diagnostics, and protocol data.
- Treat string lengths as byte lengths unless a specific character-set semantic
  is being implemented.
- Returned text must document its lifetime.

## Assertions

- Use `assert()` for internal invariants.
- Do not use `assert()` for public API validation, SQL input validation, file
  corruption, allocation failure, or MySQL compatibility errors.
- Avoid side effects inside `assert()` expressions.
- Correctness must not depend on assertions being enabled.

## Build System

- Use target-based CMake only.
- Avoid global `include_directories()` and global `add_definitions()`.
- Shared first-party compile policy belongs in CMake helper functions.
- Vendored targets do not automatically inherit first-party warnings or tidy
  policy.
- Use standard CMake build types. Do not hardcode global optimization flags.
- Build artifacts and generated files stay in CMake build directories.
- Do not use in-source CMake builds.

## Warnings, Analysis, And Sanitizers

- First-party targets use strict compiler warnings.
- CI and documented local commands use warnings as errors.
- Do not apply first-party warning policy to vendored SQLite or Lemon.
- `clang-tidy` runs on first-party code only, with warnings as errors.
- Disable static-analysis checks only when they are wrong for this project.
- Prefer fixing code over suppressing checks.
- Add ASan/UBSan as explicit build and CI modes before substantial runtime code
  lands. Sanitizers should not be the default local build.

## Dependencies And Vendored Code

- Keep dependencies lean.
- Do not introduce a general C package manager unless a real need justifies it.
- Every dependency needs a license review, version pin, update process, and
  rationale.
- Runtime-critical upstream source may be vendored when pinning and patching are
  important.
- Vendored upstream code should stay pristine where possible.
- Local patches must be explicit and documented.
- Generated code should be reproducible from checked-in inputs and build tools.
- Generated parser output should not be checked in unless a later workflow
  justifies it.

## Tests

- Package-local unit and smoke tests live next to the package they exercise.
- Cross-package and integration tests live under `tests/`.
- C tests are simple executables registered with CTest until a heavier harness
  is justified.
- Test failures should print concise diagnostics to `stderr`.
- CTest names use dotted names, such as `libmylite.version`.
- MySQL compatibility tests must compare against real MySQL 8.4.9 behavior.
- Compatibility expectations must cover result rows, metadata, errors,
  SQLSTATE, warnings, affected rows, insert ids, and side effects where
  relevant.
- MySQL-runtime compatibility tests should be labeled separately from fast unit
  tests.

## Compatibility Work

- `COMPATIBILITY.md` is the high-level compatibility matrix.
- Substantive MySQL feature work should start with a feature spec under
  `docs/specs/<feature-slug>/`.
- A feature is not marked supported until implementation, MySQL-runtime-verified
  tests, and compatibility docs are complete.
- Parser acceptance alone is not support unless the intended behavior is
  explicitly parse-only, placeholder, no-op, warning, or diagnostic.
- Deliberate embedded-design incompatibilities must be documented explicitly.

## State, Threading, And Module Boundaries

- Design for multiple independent MyLite handles.
- Avoid mutable process-global state unless it is immutable after
  initialization or carefully synchronized.
- Session state, SQL modes, diagnostics, warnings, and default schema belong to
  handles or explicit context objects.
- Keep modules layered.
- Lexer/parser code must not depend on SQLite execution.
- Parser output should feed later analysis; it should not directly lower SQL to
  SQLite.
- Diagnostics and source spans should be reusable by lexer, parser, analyzer,
  and runtime modules.

## Commits

- Keep implementation commits atomic and reviewable.
- Commit subjects use concise imperative present tense.
- Commit bodies explain non-obvious rationale briefly.
- Do not add generated assistant or co-author footers.
- Local amend/squash is fine before publication.
- Once shared for review, avoid rewriting history unless agreed.
