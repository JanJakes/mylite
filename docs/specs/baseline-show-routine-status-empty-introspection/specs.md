# Baseline SHOW Routine Status Empty Introspection

## Status

This feature specifies a narrow routine-status introspection slice for MyLite
files that currently have no stored routine descriptors. It adds parser and
result-builder support for `SHOW PROCEDURE STATUS` and
`SHOW FUNCTION STATUS` on top of `mylite_execute()`, statement context, the
MyLite parser scaffold, file-backed `.mylite` opening, durable catalog
descriptors, schema/table lifecycle, baseline DML, and existing
descriptor-driven `SHOW` statements.

The feature is intentionally not stored routine support. It exposes MySQL's
routine-status result column shape and returns zero rows until `CREATE
PROCEDURE`, `CREATE FUNCTION`, `ALTER`/`DROP` routine lifecycle, routine
descriptors, definers, privileges, and routine metadata rows exist.

## Sources

- MyLite README architecture: `README.md`
- MyLite engineering standards:
  `docs/architecture/engineering-standards.md`
- Baseline implementation strategy:
  `docs/specs/baseline-implementation-strategy/specs.md`
- Runtime handles and statement context:
  `docs/specs/runtime-handles-statement-context/specs.md`
- SQLite connection bootstrap policy:
  `docs/specs/sqlite-connection-bootstrap-policy/specs.md`
- File-backed MyLite opening and VFS:
  `docs/specs/file-backed-mylite-opening-vfs/specs.md`
- MyLite file format: `docs/specs/mylite-file-format/specs.md`
- Baseline catalog foundation:
  `docs/specs/baseline-catalog-foundation/specs.md`
- Baseline schema, table, row, write, and introspection slices:
  `docs/specs/baseline-schema-lifecycle/specs.md`,
  `docs/specs/baseline-basic-table-lifecycle/specs.md`,
  `docs/specs/baseline-table-rename-lifecycle/specs.md`,
  `docs/specs/baseline-row-values-lifecycle/specs.md`,
  `docs/specs/baseline-update-lifecycle/specs.md`,
  `docs/specs/baseline-delete-lifecycle/specs.md`,
  `docs/specs/baseline-show-like-filters/specs.md`,
  `docs/specs/baseline-show-table-status-introspection/specs.md`,
  `docs/specs/baseline-show-index-empty-introspection/specs.md`,
  `docs/specs/baseline-show-triggers-empty-introspection/specs.md`,
  `docs/specs/baseline-show-events-empty-introspection/specs.md`, and
  `docs/specs/baseline-show-open-tables-empty-introspection/specs.md`
- MySQL lexer:
  `docs/specs/mysql-lexer/specs.md`
- MySQL parser scaffold:
  `docs/specs/mysql-parser-scaffold/specs.md`
- SQLite source snapshot notes: `third_party/sqlite/README.md`
- MySQL 8.4 Reference Manual, `SHOW PROCEDURE STATUS`:
  https://dev.mysql.com/doc/refman/8.4/en/show-procedure-status.html
- MySQL 8.4 Reference Manual, `SHOW FUNCTION STATUS`:
  https://dev.mysql.com/doc/refman/8.4/en/show-function-status.html
- MySQL 8.4 Reference Manual, extensions to `SHOW` statements:
  https://dev.mysql.com/doc/refman/8.4/en/extended-show.html
- MySQL 8.4 Reference Manual, stored object access control:
  https://dev.mysql.com/doc/refman/8.4/en/stored-objects-security.html
- MySQL 8.4 Reference Manual, `INFORMATION_SCHEMA.ROUTINES`:
  https://dev.mysql.com/doc/refman/8.4/en/information-schema-routines-table.html

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior, public
SQLite APIs, and existing MyLite source code. It does not copy MySQL, MariaDB,
Percona, SQLite implementation internals, or other restrictively licensed
implementation sources.

## MySQL 8.4.9 Runtime Observations

Observed against the local `mysql:8.4.9` runtime:

- Official syntax admits `SHOW PROCEDURE STATUS [LIKE 'pattern' | WHERE expr]`
  and `SHOW FUNCTION STATUS [LIKE 'pattern' | WHERE expr]`.
- The statements are global routine listings. They do not accept `FROM` or
  `IN` schema clauses.
- Bare statements do not require a selected/default schema.
- `USE other_schema` does not restrict the row set. A routine in another schema
  is still returned when its routine name matches the `LIKE` filter.
- Successful statements leave `@@warning_count == 0` and make `ROW_COUNT()`
  return `-1`.
- `LIKE` matches routine names, not database names.
- In the observed `@@lower_case_table_names == 0` runtime, routine-name
  `LIKE` matching is case-insensitive.
- `LIKE` supports `%`, `_`, and backslash escaping.
- `WHERE` is accepted by MySQL and evaluates against displayed column names,
  including `Db` and `Name`.
- `FROM`, `IN`, `FULL`, `EXTENDED`, `ORDER BY`, `LIMIT`, non-string `LIKE`
  operands, character-set introducer `LIKE` operands, national string `LIKE`
  operands, and combining `LIKE ... WHERE ...` are syntax errors.
- Creating a stored procedure produces a `SHOW PROCEDURE STATUS` row with
  `Type = PROCEDURE`; creating a stored function produces a
  `SHOW FUNCTION STATUS` row with `Type = FUNCTION`.

The standard result columns observed from MySQL 8.4.9 are identical for both
statements:

```text
Db
Name
Type
Language
Definer
Modified
Created
Security_type
Comment
character_set_client
collation_connection
Database Collation
```

## Scope

The implementation must add:

- parser and AST support for `SHOW PROCEDURE STATUS` and
  `SHOW FUNCTION STATUS`;
- optional `LIKE 'pattern'` filters using the existing `SHOW LIKE` filter
  decoder, even though the current row set is empty;
- MySQL-compatible success without a selected/default schema;
- the MySQL 8.4.9 12-column routine-status result shape;
- zero result rows because MyLite does not support stored routine descriptors
  yet;
- deterministic diagnostics for unsupported syntax and unsupported `LIKE`
  forms;
- result-set warning and row-count behavior matching existing MyLite result
  conventions and observed MySQL 8.4.9 behavior;
- fast C tests and a MySQL 8.4.9 expectation artifact for supported behavior,
  upstream non-empty routine rows, and deliberately deferred wider forms.

## Non-Goals

This feature must not implement:

- `CREATE PROCEDURE`, `CREATE FUNCTION`, routine invocation, routine body
  parsing, compound statements, variables, cursors, handlers, routine
  parameter descriptors, deterministic metadata, comments, definers, SQL
  security, routine SQL modes, character-set capture, privileges, or
  `INFORMATION_SCHEMA.ROUTINES` rows;
- rows in `SHOW PROCEDURE STATUS` or `SHOW FUNCTION STATUS`;
- `SHOW CREATE PROCEDURE`, `SHOW CREATE FUNCTION`, `SHOW PROCEDURE CODE`, or
  `SHOW FUNCTION CODE`;
- `SHOW ... STATUS WHERE`;
- `FROM`, `IN`, `FULL`, `EXTENDED`, `ORDER BY`, `LIMIT`, or combined
  `LIKE ... WHERE`;
- arbitrary SQLite metadata reads, arbitrary SQLite SQL pass-through, or
  SQLite fork patches.

## Ownership Boundary

- The public API remains unchanged. `mylite_execute()` owns public validation,
  result-handle ownership, statement-boundary row-count state, and failure
  cleanup.
- Statement context owns diagnostics reset, warning count, and statement
  completion. Successful routine-status statements are result-set statements
  and store `-1` as the connection-local previous row count.
- Lexer/parser/AST own syntax admission and source spans. They remain
  independent of runtime, catalog, storage, and SQLite.
- Analyzer/planner code does not resolve a default schema for routine-status
  statements because MySQL treats them as global routine listings.
- The catalog module remains authoritative for schema/table descriptors, but
  this slice does not need descriptor iteration because MyLite has no routine
  descriptors to report. It does not mutate catalog rows, descriptor versions,
  descriptor caches, catalog generation, or `sqlite_schema_generation`.
- Runtime execution builds a result directly from MyLite-owned static metadata.
  It does not query SQLite table metadata, `sqlite_schema`, pragma output,
  `mysql.proc`, `INFORMATION_SCHEMA`, or performance-schema state.
- The result builder owns the 12 result columns and the empty row set.
- Storage/VFS owns the `.mylite` preamble and shifted SQLite payload boundary.
  This introspection does not touch byte range `[0, 4096)`.

## Supported SQL Grammar

Supported subset:

```sql
SHOW PROCEDURE STATUS
SHOW PROCEDURE STATUS LIKE 'pattern'
SHOW FUNCTION STATUS
SHOW FUNCTION STATUS LIKE 'pattern'
```

MyLite Lemon-syntax grammar snippet:

```lemon
statement ::= show_routine_status_statement.

show_routine_status_statement ::=
    SHOW PROCEDURE STATUS show_like_clause_opt.

show_routine_status_statement ::=
    SHOW FUNCTION STATUS show_like_clause_opt.
```

`PROCEDURE` and `FUNCTION` are mapped as explicit parser tokens for the
admitted `SHOW` routine-status syntax. This slice does not make those reserved
routine keywords usable as unquoted identifiers. `STATUS` remains usable as an
identifier where the existing identifier grammar admits it.

## Schema Handling

Routine-status statements succeed even when no schema is selected. MyLite does
not resolve the selected/default schema and does not report
`No database selected` for these statements.

There is no explicit schema clause in the supported grammar or in MySQL 8.4.9
syntax. `SHOW PROCEDURE STATUS FROM db`, `SHOW FUNCTION STATUS IN db`, and
schema-qualified routine-status targets are syntax errors.

This slice does not add new case-folding, collation, system-schema, or
privilege behavior.

## LIKE Semantics

MySQL applies `LIKE` to routine names in the routine-status row set. Since
MyLite has no routine descriptors and emits no rows, `LIKE` has no row-level
effect in this slice. MyLite still parses and decodes `LIKE 'pattern'` so that
unsupported pattern forms and NUL-producing escapes keep the same deterministic
diagnostics as other supported `SHOW LIKE` statements.

Supported patterns are ordinary string literals using the current MyLite
`SHOW LIKE` subset: `%`, `_`, and backslash escaping without NUL-producing
escapes. National string literals, character-set introducers, numeric
literals, `NULL`, parameters, expressions, and functions are unsupported
syntax.

## Result Semantics

Successful `SHOW PROCEDURE STATUS` and `SHOW FUNCTION STATUS` append the
standard 12 result columns and zero rows. The columns are:

| Ordinal | Column |
| --- | --- |
| 1 | `Db` |
| 2 | `Name` |
| 3 | `Type` |
| 4 | `Language` |
| 5 | `Definer` |
| 6 | `Modified` |
| 7 | `Created` |
| 8 | `Security_type` |
| 9 | `Comment` |
| 10 | `character_set_client` |
| 11 | `collation_connection` |
| 12 | `Database Collation` |

Successful execution reports `warning_count == 0`, no result rows, and
connection-local `ROW_COUNT()` behavior equivalent to other MyLite result-set
statements (`-1`). The public result object follows the existing row-result
API conventions.

## Diagnostics

Diagnostics follow existing MyLite parser and execution policy:

- syntax errors for unsupported grammar such as `FROM`, `IN`, `FULL`,
  `EXTENDED`, `ORDER BY`, `LIMIT`, combined `LIKE ... WHERE`, unsupported
  `LIKE` operands, parameters, expressions, functions, or routine-body syntax;
- deterministic `SHOW LIKE` decode diagnostics for accepted grammar whose
  literal value is outside the current MyLite pattern subset, including
  NUL-producing escapes;
- allocation failure diagnostics from existing parser/runtime allocation
  paths;
- public API misuse diagnostics remain unchanged.

No missing-default-schema, unknown-schema, unknown-table, reserved-name,
catalog lookup, or physical SQLite diagnostics are introduced by this slice.

## Physical SQLite and File Format Policy

Routine-status statements are implemented as MyLite wrapper/runtime behavior.
They do not generate SQLite SQL, prepare SQLite statements, bind values, read
SQLite schema metadata, or call SQLite extension APIs beyond the normal handle
plumbing already used by `mylite_execute()`.

The implementation must not create, update, delete, or rename any SQLite
objects. It must not mutate MyLite catalog rows, descriptor versions,
descriptor caches, catalog generation, `sqlite_schema_generation`, the
`.mylite` preamble, or shifted SQLite payload invariants. No SQLite fork patch
is required.

## Tests

Design-time MySQL expectation coverage lives in
`packages/libmylite/tests/mysql_baseline_show_routine_status_empty_introspection_expectations.sh`.
It verifies MySQL 8.4.9 version, result columns, no-default-schema success,
selected-schema independence, non-empty upstream rows after creating one
procedure and one function, `LIKE` matching, `WHERE` acceptance upstream,
warnings, row-count behavior, and rejected unsupported syntax.

Implementation tests must add a fast C test under `packages/libmylite/tests/`
and register it with a dotted CTest name. Coverage must include:

- `SHOW PROCEDURE STATUS`, `SHOW PROCEDURE STATUS LIKE 'pattern'`,
  `SHOW FUNCTION STATUS`, and `SHOW FUNCTION STATUS LIKE 'pattern'`;
- exact 12-column result shape and zero rows for both routine kinds;
- success without a selected/default schema;
- success with an unrelated selected schema;
- no catalog generation or `sqlite_schema_generation` mutation;
- persistence/reopen and independent file-backed handles;
- `.mylite` preamble preservation;
- warning count, affected/row-count conventions, and absence of row data;
- zero-initialized cleanup for result handles;
- unsupported forms: `FROM`, `IN`, `FULL`, `EXTENDED`, `ORDER BY`, `LIMIT`,
  `WHERE`, combined `LIKE ... WHERE`, numeric/`NULL`/national/charset
  introducer `LIKE`, parameters, and function/expression `LIKE` operands.

Existing lexer, parser, runtime handle, diagnostics, statement context, result
metadata, SQLite bootstrap policy, file-backed opening, VFS, catalog
foundation, schema/table lifecycle, row values, update, delete, and existing
`SHOW` introspection tests must continue to pass.

## Compatibility Documentation

`COMPATIBILITY.md` and `docs/compatibility/sql-show-statements.md` must mark
`SHOW PROCEDURE STATUS` and `SHOW FUNCTION STATUS` as MySQL-runtime-verified
empty routine introspection with documented `LIKE` and empty-result `WHERE`
filters. They must not claim stored routine support, routine rows, privileges,
routine DDL, `SHOW CREATE`, code listings, definers, security metadata, or
non-empty row predicate evaluation. The related `INFORMATION_SCHEMA.ROUTINES`
metadata-only surface is documented separately without changing this feature's
empty `SHOW` behavior.
