# SQLite Source-Tree Fork Package

## Status

This slice makes the SQLite fork concrete at the build and package boundary.
MyLite now has a dedicated `packages/libmylite_fork` package that builds a
private SQLite engine from SQLite source-tree files instead of the SQLite
amalgamation.

Implemented scope:

- vendor the pinned SQLite 3.53.0 source-tree files needed for the library
  build under `packages/libmylite_fork/upstream`
- generate SQLite's derived source files in the CMake build tree from vendored
  inputs: `sqlite3.h`, `parse.c`, `parse.h`, `opcodes.c`, `opcodes.h`,
  `keywordhash.h`, `pragma.h`, and `ctime.c`
- compile SQLite as individual source files in `mylite_fork_sqlite`
- move the current MyLite fork primitives into `mylite_fork`, leaving the older
  `packages/libmylite/src/fork` copy unbuilt
- link `packages/libmylite` against `MyLite::mylite_fork`
- exclude vendored SQLite files from first-party format and clang-tidy globs
- add a fork-package smoke test that opens the source-built SQLite engine,
  checks the pinned SQLite version number, registers MyLite fork primitives,
  and executes a primitive on that connection

Deferred scope:

- direct SQLite parser grammar changes for MySQL syntax
- VDBE-native assignment coercion opcodes
- SQLite schema objects carrying MySQL column descriptors
- `.mylite` shifted-file-offset pager/VFS patches
- removal of the old amalgamation snapshot under `third_party/sqlite`
- direct MySQL DDL/DML execution through the fork parser

## Sources

- SQLite 3.53.0 source archive:
  `https://www.sqlite.org/2026/sqlite-src-3530000.zip`
- SQLite source archive SHA3-256:
  `4ffbd00ba8db1e1172dbc69a5203a2c185556a32543e58585ba3713abf676fe5`
- Existing MyLite native execution plan:
  `docs/architecture/native-sqlite-execution-plan.md`
- SQLite fork CRUD foundation:
  `docs/specs/sqlite-fork-crud/specs.md`
- SQLite fork type coercion foundation:
  `docs/specs/sqlite-fork-type-coercion/specs.md`

This specification is independently authored from official SQLite source-tree
layout, observed MyLite build behavior, and the current MyLite codebase.

## Build Design

The fork package is split into two targets:

- `mylite_fork_sqlite`: vendored SQLite source-tree build
- `mylite_fork`: MyLite-specific fork primitives linked on top of that private
  SQLite engine

The SQLite target does not use `sqlite3.c`. It compiles SQLite's source files
directly and suppresses warnings only for vendored upstream code. Generated
SQLite files are reproducible from checked-in SQLite inputs and generated under
`build/<preset>/generated/libmylite_fork/sqlite`.

MyLite-specific fork code remains first-party C and keeps normal project
formatting and static-analysis policy.

## Integration Semantics

`packages/libmylite` no longer links against the previous
`third_party/sqlite` amalgamation target. It links against
`MyLite::mylite_fork`, which exposes the source-built SQLite API and the
MyLite fork primitive registration helpers.

For now, MyLite still reaches fork primitives through registration hooks such as
`mylite_sqlite_fork_configure()`. The difference is architectural: those hooks
now live next to a private SQLite source-tree build where parser, VDBE, pager,
and file-format changes can be made directly.

## Tests

The fork package test must verify:

- the linked SQLite runtime is the pinned source-tree version
- a memory database can be opened through the source-built SQLite engine
- MyLite fork primitives can be registered
- a statement can use a MyLite primitive and MySQL collation on that connection

Existing MyLite runtime and SQLite-fork tests continue to verify that public
MyLite behavior still works through the new fork package boundary.

## Compatibility Status

This feature is `🟡` because MyLite now has a real source-tree SQLite fork
package and build boundary, but MySQL syntax and type behavior are not yet
implemented inside SQLite parser/VDBE internals.
