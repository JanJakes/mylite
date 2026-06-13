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
