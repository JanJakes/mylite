# Baseline utf8mb4 0900 Binary Collation

## Status

This phase admits MySQL 8.4.9's `utf8mb4_0900_bin` collation as a
metadata-preserving `utf8mb4` collation in MyLite. It extends the existing
table/column charset and collation surface, static collation catalogs, and
session `SET NAMES ... COLLATE` readback without adding full binary collation
comparison semantics.

The slice is deliberately narrow. `utf8mb4_0900_bin` becomes visible through
descriptor-driven metadata and can be stored as a table or supported string
column collation. Row storage remains MyLite-owned UTF-8 text over SQLite
storage, and string predicates, ordering, grouping, distinct, primary-key, and
unique-key semantics continue to use the already documented MyLite comparison
subset.

## Sources

- MyLite README architecture: `README.md`
- MyLite engineering standards:
  `docs/architecture/engineering-standards.md`
- SQLite fork policy: `third_party/sqlite/README.md`
- Existing charset/collation designs:
  - `docs/specs/baseline-show-character-set-collation/specs.md`
  - `docs/specs/baseline-utf8mb4-legacy-collations/specs.md`
  - `docs/specs/baseline-table-charset-collation-surface/specs.md`
  - `docs/specs/baseline-column-charset-collation-attributes/specs.md`
  - `docs/specs/baseline-information-schema-collation-character-set-applicability/specs.md`
  - `docs/specs/baseline-set-connection-character-set/specs.md`
- Official MySQL 8.4 Reference Manual, Unicode character sets:
  https://dev.mysql.com/doc/refman/8.4/en/charset-unicode-sets.html
- Official MySQL 8.4 Reference Manual, column character set and collation:
  https://dev.mysql.com/doc/refman/8.4/en/charset-column.html
- Official MySQL 8.4 Reference Manual, `SHOW COLLATION`:
  https://dev.mysql.com/doc/refman/8.4/en/show-collation.html
- Official MySQL 8.4 Reference Manual, `INFORMATION_SCHEMA.COLLATIONS`:
  https://dev.mysql.com/doc/refman/8.4/en/information-schema-collations-table.html
- MySQL runtime probes are captured in
  `packages/libmylite/tests/mysql_baseline_utf8mb4_0900_bin_collation_expectations.sh`
  and verified against MySQL 8.4.9.

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior,
public SQLite APIs, and existing MyLite source code. It does not copy MySQL,
MariaDB, Percona, SQLite implementation internals, or other restrictively
licensed implementation sources.

## MySQL 8.4.9 Runtime Observations

Observed against the local `mysql:8.4.9` runtime:

- `SHOW COLLATION WHERE Collation = 'utf8mb4_0900_bin'` returns one row with
  charset `utf8mb4`, id `309`, default empty string, compiled `Yes`, sort
  length `1`, and pad attribute `NO PAD`.
- `INFORMATION_SCHEMA.COLLATIONS` exposes the same row values.
- `SET NAMES utf8mb4 COLLATE utf8mb4_0900_bin` succeeds with warning count
  `0`; `@@character_set_client`, `@@character_set_connection`, and
  `@@character_set_results` read back `utf8mb4`, and
  `@@collation_connection` reads back `utf8mb4_0900_bin`.
- `CREATE TABLE ... DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_bin`
  succeeds and `SHOW CREATE TABLE` preserves the table default collation.
- Character columns may inherit `utf8mb4_0900_bin` from the table default or
  declare it explicitly with `COLLATE utf8mb4_0900_bin`. `SHOW FULL COLUMNS`
  and `INFORMATION_SCHEMA.COLUMNS` report the effective collation for
  `CHAR`, `VARCHAR`, and `TEXT` family columns.
- A collation that belongs to `utf8mb4` but is paired with a non-`utf8mb4`
  character set fails with MySQL error `1253`, SQLSTATE `42000`.
- MySQL documents `utf8mb4_0900_bin` as a `NO PAD` binary collation. Full
  trailing-space-sensitive comparison behavior is outside this MyLite slice
  and remains explicitly unsupported here.

## Scope

Supported:

- static `SHOW COLLATION` row for `utf8mb4_0900_bin`;
- static `INFORMATION_SCHEMA.COLLATIONS` row for `utf8mb4_0900_bin`;
- static `INFORMATION_SCHEMA.COLLATION_CHARACTER_SET_APPLICABILITY` mapping
  from `utf8mb4_0900_bin` to `utf8mb4`;
- `SET NAMES utf8mb4 COLLATE utf8mb4_0900_bin` session readback;
- `CREATE TABLE`, `CREATE TEMPORARY TABLE`, `CREATE TABLE ... LIKE`, and the
  current `ALTER TABLE ... [DEFAULT] COLLATE` table-default metadata paths;
- supported `CHAR`, `VARCHAR`, and bare `TEXT` family column
  `COLLATE utf8mb4_0900_bin` metadata where column collation attributes are
  already admitted;
- `ALTER TABLE ... ADD COLUMN`, `MODIFY COLUMN`, and `CHANGE COLUMN` metadata
  paths that already admit `utf8mb4` string column collation attributes;
- descriptor-driven `SHOW CREATE TABLE`, `SHOW FULL COLUMNS`,
  `INFORMATION_SCHEMA.COLUMNS`, and public result metadata collation id `309`;
- ASCII case-insensitive recognition of the collation name with canonical
  lowercase descriptor storage;
- warning count `0` for supported in-range metadata operations;
- reopen persistence, independent handle behavior, temporary table cleanup, and
  `.mylite` preamble preservation through existing storage boundaries.

Deferred:

- full `utf8mb4_0900_bin` comparison, ordering, grouping, distinct, `LIKE`,
  regex, `FIELD`, aggregate, and expression semantics;
- `NO PAD` trailing-space comparison behavior;
- Unicode code-point weight or bytewise weight ordering;
- primary-key, unique-key, and prefix-key semantics under
  `utf8mb4_0900_bin`; existing supported string key enforcement continues to
  use the documented MyLite ASCII `utf8mb4_0900_ai_ci` subset;
- non-`utf8mb4` character sets and the `binary` character set/collation;
- additional `utf8mb4_0900_*` collations beyond `utf8mb4_0900_ai_ci` and
  `utf8mb4_0900_bin`;
- `COLLATE` expression operators, coercibility, charset introducers,
  conversion, protocol negotiation, `mysql.collations`, generated columns,
  views, triggers, and privileges;
- SQLite fork patches.

## Ownership Boundaries

- Public API: unchanged. `mylite_execute()` and existing result/diagnostic APIs
  own statement execution, result lifetime, misuse behavior, and cleanup.
- Statement context: owns diagnostics reset, affected rows, warning count,
  `ROW_COUNT()`, and statement completion. This metadata slice adds no new
  statement state.
- Lexer/parser/AST: no new grammar is required. Existing charset/collation
  option nodes preserve source tokens for analyzer validation.
- Analyzer/planner/runtime: widens the admitted `utf8mb4` collation descriptor
  table, validates names before catalog mutation or generated SQLite SQL, and
  canonicalizes the admitted name to `utf8mb4_0900_bin`.
- Catalog: remains the durable authority for table defaults and explicit column
  charset/collation metadata. SQLite schema text is not metadata authority.
- Result builders and introspection: render static collation rows and effective
  descriptor metadata, including public result column collation id `309` and
  binary metadata flags for this binary collation.
- SQLite physical storage: unchanged. No SQLite collation registration or fork
  patch is required because this slice does not execute
  `utf8mb4_0900_bin` comparison semantics through SQLite.
- Storage/VFS: unchanged. The MyLite preamble and shifted SQLite payload
  invariant must remain intact.

## Grammar

No new parser productions are needed. This phase extends runtime validation for
the existing independently authored grammar:

```lemon
table_option ::= default_opt COLLATE equal_opt option_name.
column_attribute ::= COLLATE option_name.
set_statement ::= SET NAMES option_name COLLATE option_name.
```

Analyzer acceptance for `option_name` adds:

```lemon
admitted_utf8mb4_collation ::= utf8mb4_0900_bin.
```

## Diagnostics

- Unknown collation names continue to fail with error `1273`, SQLSTATE
  `HY000`.
- Known non-`utf8mb4` collations paired with `utf8mb4` continue to fail with
  `1253`, SQLSTATE `42000`.
- `utf8mb4_0900_bin` paired with a known non-`utf8mb4` character set fails
  with `1253`, SQLSTATE `42000`.
- Unsupported character-set syntax, unsupported type targets, duplicate column
  charset/collation attributes, and misplaced attributes continue to use the
  existing parser/analyzer diagnostics from the column charset/collation slice.
- Allocation failures and physical SQLite failures continue through existing
  MyLite diagnostics.

## Verification

Fast C tests must cover static catalog rows, table/column metadata
preservation, public result metadata id and binary flag, persistence,
independent handles, and unchanged diagnostics. The MySQL expectation script
must verify the visible MySQL 8.4.9 row values and DDL/readback behavior before
implementation is considered complete.
