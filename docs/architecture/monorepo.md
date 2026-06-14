# Monorepo structure

MyLite is organized as a C17-first monorepo. The core library is the first
buildable package because parser, runtime, storage, and protocol integrations
will all depend on it.

## Layout

| Path | Purpose |
| --- | --- |
| `.agents/skills/` | Repo-local agent workflows for MyLite compatibility planning, implementation, review, and batch work. |
| `.github/workflows/` | GitHub Actions CI workflows. |
| `CMakePresets.json` | Shared configure, build, test, and check commands for local development and CI. |
| `cmake/` | Shared CMake helpers used by packages and tools. |
| `docs/architecture/` | Architecture notes and engineering standards for repository-wide design decisions. |
| `docs/specs/` | Feature specifications and MySQL-runtime-verified test expectations for substantive compatibility work. |
| `packages/libmylite/` | Embedded MyLite runtime library. This owns the public C API and core SQL pipeline implementation. |
| `packages/php-ext-mylite/` | Core PHP module for direct embedded MyLite access. |
| `packages/php-ext-mysqli-mylite/` | Drop-in PHP mysqli replacement backed by MyLite. |
| `packages/php-ext-pdo-mylite/` | PDO driver package registered as `mylite`. |
| `tests/` | Cross-package and integration tests. Package-local tests stay next to the package they exercise. |
| `third_party/` | Pinned upstream source snapshots and vendored build tools. |
| `tools/` | Command-line tools and developer utilities. Buildable tools should live in subdirectories. |

SQLite source is pinned in `third_party/sqlite/`. The pristine source tree
lives in `third_party/sqlite/upstream/`, the buildable amalgamation lives in
`third_party/sqlite/amalgamation/`, and local SQLite fork patches belong in
`third_party/sqlite/patches/`. Lemon is built from the same pinned source
snapshot so generated parser code is reproducible. Runtime SQLite integration
should prefer public extension APIs, with small local SQLite fork patches
reserved for missing extension points that MyLite needs for MySQL compatibility
or performance.

## Build

```sh
cmake --preset dev          # configure build/dev with Ninja and strict checks
cmake --build --preset dev  # build the default targets
ctest --preset dev          # run tests and print failures
```

The `dev` preset uses Ninja, enables warnings as errors, and writes
`compile_commands.json` for developer tools. Use `CMakeUserPresets.json` for
local machine-specific overrides; it is intentionally ignored by Git.
CI exercises Linux, macOS, and Windows so toolchain portability stays visible
from the scaffold onward.

`BUILD_TESTING` controls tests through standard CTest wiring.
`MYLITE_BUILD_TOOLS` controls command-line tools and defaults to enabled.
`MYLITE_WARNINGS_AS_ERRORS` is enabled by the shared `dev` preset so local and
CI builds use the same strict warning baseline.

Large package build registrations should stay split into package-local CMake
modules under the owning package's `cmake/` directory. The package root
`CMakeLists.txt` should show the high-level target/test structure, while
included modules own cohesive source lists or test-family registrations.

Within `packages/libmylite/src/sql/`, the parser driver owns the public parse
entry point, Lemon token feeding, version-comment expansion, and parser state
transitions. Lexer-token to Lemon-token policy lives in
`mylite_parser_token_map.c` so keyword mapping, SQL-mode token decisions,
shared token predicates, and token-history handling stay separate from the
parse driver. Syntax-retry and unsupported-placeholder fallback parsing lives
in `mylite_parser_placeholders.c`; its broad private scanner/classifier helper
graph is kept in ordered same-translation-unit `mylite_parser_placeholders_*.inc`
fragments until smaller true module APIs are worth exposing. Large AST builder
families may live in separate `mylite_parser_*_builders.c` modules when they
use the shared parser helper surface instead of reaching through file-local
state; DDL builders are grouped by create/view/procedure, index/table-option,
schema/drop/show/admin, and rename/alter-table surfaces.

Within `packages/libmylite/src/runtime/`, the execution runtime uses true C
modules for narrow helper surfaces and same-translation-unit `.inc` fragments
for broad planner/executor families that still depend on private static helper
linkage. The catalog runtime is split between state/migration/validation
helpers, direct object lifecycle helpers, schema/table/column mutation modules,
and read modules grouped by schema/table/view, column, index, and constraint
metadata so catalog lifecycle and materialization logic do not collapse into a
single catalog monolith. SQL text pre-normalization before parsing lives in
`mylite_execution_sql_normalization.c`; it is a narrow execution-front-door
module with explicit ownership of rewritten SQL buffers. MySQL-compatible
execution diagnostics live in `mylite_execution_diagnostics*.c` modules grouped
by behavior domain, with `mylite_execution_diagnostics_internal.h` carrying the
private shared diagnostic dependencies for those modules. Scalar execution
modules should keep cohesive function families together; for example,
base-conversion scalars live in `mylite_execution_scalar_base_conversion.c`
and binary scalar wrappers split specialized CHAR(), Base64, compression,
digest/CRC32, RANDOM_BYTES(), and UUID conversion/generation families out of
the remaining HEX/WEIGHT_STRING module, with only narrow binary-scalar helpers
exposed through `mylite_execution_scalar_binary_internal.h`, while numeric
scalars are split between exact arithmetic/rounding, approximate math, double
formatting, and decimal `FORMAT()`/`TRUNCATE()` support. Temporal scalar
wrappers keep format/parsing functions separate from DATE_ADD/SUB, TIMESTAMPADD,
ADDTIME, and SUBTIME arithmetic.
JSON scalar wrappers split constructors and mutation functions out of the
remaining validation/search/path/introspection module, sharing only the narrow
helpers declared in `mylite_execution_scalar_json_internal.h`.
String-position scalar wrappers keep UTF-8 slicing, padding, search, and set
comparison in the primary module while the bitmask-oriented EXPORT_SET() and
MAKE_SET() family lives in `mylite_execution_scalar_string_bitmask.c`.
`mylite_execution_declarations_*.inc` files are the ordered private declaration
hub for the remaining fragments. Large declaration hubs may fan out into
numbered same-directory subfragments when that keeps declaration groups
navigable without changing linkage; keep them grouped by execution family until
a family has a stable internal API that justifies a true module split.

When running Homebrew `clang-tidy` on macOS, use a build directory configured
with Homebrew `clang` so the compile database matches the analysis toolchain.
Set `CC` explicitly before configuring; putting LLVM on `PATH` alone may still
leave CMake using `/usr/bin/cc`:

```sh
LLVM_PREFIX="$(brew --prefix llvm)"
export PATH="$LLVM_PREFIX/bin:$PATH"
CC="$LLVM_PREFIX/bin/clang" cmake --preset dev
cmake --build --preset tidy
```

Run the complete local verification workflow with:

```sh
LLVM_PREFIX="$(brew --prefix llvm)"
CC="$LLVM_PREFIX/bin/clang" PATH="$LLVM_PREFIX/bin:$PATH" cmake --workflow --preset check
```
