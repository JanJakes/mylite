# libmylite_fork

`libmylite_fork` owns MyLite's private SQLite fork. It is intentionally
separate from `packages/libmylite` so SQLite parser, VDBE, pager, and file
format patches can evolve without being hidden inside MyLite's compatibility
layer.

This package vendors the canonical SQLite source tree files needed to build
SQLite from source form. It does not build or consume the SQLite amalgamation.

## Pinned SQLite Source

| Field | Value |
| --- | --- |
| Version | 3.53.0 |
| Version number | 3053000 |
| Download archive number | 3530000 |
| Source archive | `https://www.sqlite.org/2026/sqlite-src-3530000.zip` |
| SHA3-256 | `4ffbd00ba8db1e1172dbc69a5203a2c185556a32543e58585ba3713abf676fe5` |

## Build Shape

- `mylite_fork_sqlite` compiles SQLite's individual source files.
- SQLite generated sources are produced in the CMake build tree from the
  vendored SQLite inputs: `sqlite3.h`, `parse.c`, `opcodes.c`,
  `keywordhash.h`, `pragma.h`, and `ctime.c`.
- `mylite_fork` links the source-built SQLite library with MyLite fork
  primitives such as MySQL-compatible collations, scalar functions, and table
  maintenance helpers.

Vendored SQLite files under `upstream/` should remain pristine. Local MyLite
fork code belongs in `src/` and public/internal fork headers belong in
`include/`.
