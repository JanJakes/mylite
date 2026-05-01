# Lemon TiDB Parser Port

This branch contains a syntax-only prototype parser for MyLite. It ports TiDB's
MySQL-oriented yacc grammar to SQLite Lemon and wraps it with a small C lexer and
driver. The goal of this milestone is broad MySQL syntax acceptance, not AST
construction or execution semantics.

## Architecture

- `third_party/tidb/parser.y` is the vendored TiDB parser grammar input.
- `scripts/port_tidb_lemon.py` converts the TiDB yacc grammar into a generated
  Lemon grammar and token table.
- `third_party/sqlite/lemon.c` and `third_party/sqlite/lempar.c` build the local
  Lemon parser generator and parser template.
- `src/parser/lexer.c` provides a MySQL lexer that emits the generated TiDB token
  IDs and handles MySQL context-sensitive lexical forms.
- `src/parser/parser.c` drives the Lemon parser and keeps temporary
  syntax-only recognizers for conflict-heavy corpus forms that should eventually
  move into grammar or AST-aware handling.
- `src/tools/mylite_parse.c` exposes the parser as `mylite-parse` for corpus and
  smoke testing.

The generated parser files live under the CMake build directory. They are not
checked in.

## Overlay Policy

The converter keeps TiDB productions as the base grammar and appends a MyLite
overlay for MySQL server syntax that is absent from TiDB or differs from MySQL
test coverage. Overlay rules should stay narrow and corpus-backed. When a rule
represents a known MySQL 8.4-specific difference, document that in this file or
in a later feature note before broadening behavior.

Temporary recognizers in `src/parser/parser.c` are acceptable for this prototype
when a Lemon conflict would otherwise block corpus progress. They are not the
long-term parser architecture. As AST construction becomes real, these
recognizers should be replaced by grammar productions or explicit statement
nodes.

## Verification

Build and smoke test:

```sh
cmake --build build -j4
ctest --test-dir build --output-on-failure
```

Run the WordPress MySQL server query corpus:

```sh
python3 tests/run_query_corpus.py \
  --parser build/mylite-parse \
  --corpus /tmp/mylite-parser-research/mysql-server-tests-queries.csv \
  --fail-fast
```

Current prototype result on May 1, 2026:

```text
parsed=69541 skipped=36 failed=0
```

The corpus source used for this prototype is:

`https://github.com/WordPress/sqlite-database-integration/blob/trunk/packages/mysql-on-sqlite/tests/mysql/data/mysql-server-tests-queries.csv`

## Current Limits

- The parser validates syntax only.
- It does not build a stable MyLite AST.
- It does not verify MySQL 8.4.9 semantic behavior.
- Some MySQL-only syntax is accepted through overlay rules or temporary
  recognizers while the port is still a prototype.
- The WordPress corpus is broad but is not a full MySQL grammar proof.
