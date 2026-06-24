# Baseline SHOW Warnings Diagnostics

## Status

This feature specifies a narrow diagnostics introspection slice for
`SHOW WARNINGS` and `SHOW COUNT(*) WARNINGS`. It builds on
`mylite_execute()`, statement context, handle-owned diagnostics, existing
result-set conventions, and the current baseline warning producer
(`SHOW PROCESSLIST`).

The feature is intentionally not a full MySQL diagnostics-area
implementation. MyLite exposes the previous statement's error condition and
warning records through MySQL's result shape, but it does not yet implement
notes, `max_error_count`, `sql_notes`, `GET DIAGNOSTICS`, `SHOW ERRORS`,
warning-producing DML conversions, or protocol-level warning packet details.

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
- Baseline `SHOW PROCESSLIST` warning source:
  `docs/specs/baseline-show-processlist-introspection/specs.md`
- Baseline DDL/DML and introspection slices:
  `docs/specs/baseline-schema-lifecycle/specs.md`,
  `docs/specs/baseline-basic-table-lifecycle/specs.md`,
  `docs/specs/baseline-row-values-lifecycle/specs.md`,
  `docs/specs/baseline-update-lifecycle/specs.md`,
  `docs/specs/baseline-delete-lifecycle/specs.md`, and
  `docs/specs/baseline-show-like-filters/specs.md`
- MySQL lexer:
  `docs/specs/mysql-lexer/specs.md`
- MySQL parser scaffold:
  `docs/specs/mysql-parser-scaffold/specs.md`
- SQLite source snapshot notes: `third_party/sqlite/README.md`
- MySQL 8.4 Reference Manual, `SHOW WARNINGS`:
  https://dev.mysql.com/doc/refman/8.4/en/show-warnings.html
- MySQL 8.4 Reference Manual, the diagnostics area:
  https://dev.mysql.com/doc/refman/8.4/en/diagnostics-area.html

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior,
public SQLite APIs, and existing MyLite source code. It does not copy MySQL,
MariaDB, Percona, SQLite implementation internals, or other restrictively
licensed implementation sources.

## MySQL 8.4.9 Runtime Observations

Observed against the local `mysql:8.4.9` runtime:

- Official syntax admits `SHOW WARNINGS [LIMIT [offset,] row_count]` and
  `SHOW COUNT(*) WARNINGS`.
- `SHOW WARNINGS` returns columns `Level`, `Code`, and `Message`.
- `SHOW COUNT(*) WARNINGS` returns one column named
  `@@session.warning_count`.
- `SHOW WARNINGS` and `SHOW COUNT(*) WARNINGS` are diagnostic statements.
  They read the current message list and do not clear it. A later ordinary
  nondiagnostic statement, such as `SELECT 1`, clears the message list.
- `SHOW PROCESSLIST` on the default 8.4.9 process-list implementation
  produces warning `1287` with level `Warning`; a following
  `SHOW WARNINGS` displays that row, and a following
  `SHOW COUNT(*) WARNINGS` displays `1`.
- `SHOW WARNINGS` itself returns a result set, so `ROW_COUNT()` is `-1`.
- A parse error followed by `SHOW WARNINGS` in the same session displays one
  row with `Level = Error`, code `1064`, and the parse-error message.
- `SHOW WARNINGS LIMIT 0` succeeds and displays no rows.
- `SHOW WARNINGS LIMIT 1`, `LIMIT 0,1`, and `LIMIT 1 OFFSET 0` display the
  first warning row when one warning exists.
- `SHOW WARNINGS LIMIT 1,1` and `LIMIT 1 OFFSET 1` display no rows when only
  one warning exists.
- `SHOW WARNINGS LIMIT 18446744073709551615` is accepted.
- `SHOW WARNINGS LIMIT -1`, `LIMIT +1`, and
  `LIMIT 18446744073709551616` are syntax errors.
- `SHOW COUNT (*) WARNINGS`, `SHOW WARNINGS LIKE 'pattern'`, and
  `SHOW WARNINGS WHERE ...` are syntax errors.

The local MySQL container used for expectation scripts can start a new client
session with a preexisting warning from its initialization path. Expectation
scripts therefore first execute a harmless nondiagnostic statement before
checking an empty warning area.

## Scope

The implementation must add:

- parser and AST support for `SHOW WARNINGS`, `SHOW WARNINGS LIMIT ...`, and
  `SHOW COUNT(*) WARNINGS`;
- a connection-local previous-diagnostics snapshot that survives successful
  diagnostic statements but is replaced by ordinary statements and statement
  errors;
- result-set rendering for the previous error condition, when present, and
  the previous warning records;
- `SHOW COUNT(*) WARNINGS` total-count rendering for the previous error
  condition plus previous warning records;
- MySQL-compatible `LIMIT` display slicing for unsigned decimal integer
  literal row-count and offset forms through `UINT64_MAX`;
- result-set row-count behavior matching existing MyLite conventions
  (`ROW_COUNT() = -1`);
- `warning_count == 0` on successful `SHOW WARNINGS` and
  `SHOW COUNT(*) WARNINGS`, because those successful diagnostic statements do
  not create new warning records;
- fast C tests and a MySQL 8.4.9 expectation artifact for supported behavior
  and deliberately deferred wider forms.

## Non-Goals

This feature must not implement:

- `SHOW ERRORS`, `SHOW COUNT(*) ERRORS`, or `GET DIAGNOSTICS`;
- diagnostics-area stacking, stored program condition handlers, `SIGNAL`,
  `RESIGNAL`, or condition item names;
- `max_error_count`, `sql_notes`, note records, warning truncation policy, or
  counted-but-not-stored messages;
- new warning-producing DML conversions, protocol warning packets,
  `@@warning_count` as a general system variable, or `mysql_warning_count()`;
- `SHOW WARNINGS WHERE`, `LIKE`, `ORDER BY`, aliases, expressions, functions,
  parameters, string/decimal/float/hex/bit limit literals, signed limit
  literals, or arbitrary SELECT-like clauses;
- catalog mutations, descriptor mutations, physical table mutations,
  arbitrary SQLite metadata reads, arbitrary SQLite SQL pass-through, or
  SQLite fork patches.

## Ownership Boundary

- The public API remains unchanged. `mylite_execute()` owns public validation,
  parse/execution orchestration, result-handle ownership, statement-boundary
  row-count state, diagnostics snapshot replacement, and failure cleanup.
- Statement context owns the live diagnostics reset at statement start and the
  active statement lifecycle. It continues to reset live diagnostics for every
  statement; preservation is handled by the connection-owned previous snapshot.
- Connection/session state owns the previous diagnostics snapshot because
  MySQL diagnostics are session-local and `SHOW WARNINGS` reads the previous
  statement's conditions.
- Lexer/parser/AST own syntax admission, source spans, and `COUNT(*)`
  no-space enforcement. They remain independent of runtime, catalog, storage,
  and SQLite.
- Runtime execution builds `SHOW WARNINGS` results directly from MyLite-owned
  diagnostics records. It does not query SQLite, catalog tables,
  `INFORMATION_SCHEMA`, Performance Schema, or system variables.
- The result builder owns the `Level`, `Code`, `Message`, and
  `@@session.warning_count` result shapes.
- Storage/VFS owns the `.mylite` preamble and shifted SQLite payload boundary.
  This diagnostics introspection does not touch byte range `[0, 4096)`.

## Supported SQL Grammar

Supported subset:

```sql
SHOW WARNINGS
SHOW WARNINGS LIMIT row_count
SHOW WARNINGS LIMIT offset, row_count
SHOW WARNINGS LIMIT row_count OFFSET offset
SHOW COUNT(*) WARNINGS
```

`row_count` and `offset` are unsigned decimal integer literals with no sign.
The parser rejects signed literals and non-integer literals.

MyLite Lemon-syntax grammar snippet:

```lemon
statement ::= show_warnings_statement.

show_warnings_statement ::= SHOW WARNINGS limit_clause_opt.

show_warnings_statement ::= SHOW COUNT LPAREN STAR RPAREN WARNINGS.
```

The parser builder for the `SHOW COUNT(*) WARNINGS` production must verify
that `COUNT` and `(` are adjacent in the source text, matching the existing
`COUNT(*)` scalar-expression policy and observed MySQL behavior.

`WARNINGS` is tokenized as an unreserved keyword by the lexer. This slice maps
it to an explicit parser token only for the admitted `SHOW` grammar; it remains
available as an identifier where the parser permits ordinary identifiers.

## Diagnostics Snapshot Semantics

MyLite maintains two diagnostics areas:

- the existing live diagnostics area for the statement currently being
  executed;
- a previous-diagnostics snapshot used by `SHOW WARNINGS` and
  `SHOW COUNT(*) WARNINGS`.

Starting any statement resets the live area as it does today. After a
successful nondiagnostic statement, MyLite replaces the previous snapshot with
the final live diagnostics from that statement. After a failed statement,
including parse errors and unsupported syntax, MyLite replaces the previous
snapshot with the final error condition. After a successful `SHOW WARNINGS` or
`SHOW COUNT(*) WARNINGS`, MyLite leaves the previous snapshot unchanged.

Successful diagnostic statements leave live `mylite_errcode()` as `MYLITE_OK`
and live public result warning count as `0`; preserving the previous snapshot
does not mean preserving the previous live error condition.

The snapshot is connection-local and in-memory. It is not stored in the
`.mylite` file and is not shared between independent handles.

## Result Semantics

`SHOW WARNINGS` returns:

| Ordinal | Column | Type in this slice |
| --- | --- | --- |
| 1 | `Level` | `Error` for a previous error condition, `Warning` for warning records |
| 2 | `Code` | decimal MySQL diagnostic code text |
| 3 | `Message` | MyLite's diagnostic message text |

If the previous snapshot has both warning records and an error condition, the
warning records are displayed first in recorded order, followed by the final
error condition. This matches the later `sql_slave_skip_counter` mixed
warning/error scalar-select evidence.

`SHOW COUNT(*) WARNINGS` returns one row whose value is:

```text
(previous error condition present ? 1 : 0) + previous warning record count
```

Both statements are result-set statements and set connection-local
`ROW_COUNT()` to `-1`.

## LIMIT Semantics

`SHOW WARNINGS LIMIT` slices only the displayed rows. It does not alter the
previous diagnostics snapshot and does not alter the count returned by
`SHOW COUNT(*) WARNINGS`.

Supported literal forms are unsigned decimal integer tokens through
`18446744073709551615`. `LIMIT 0` succeeds and displays no rows. Offsets at or
beyond the number of stored diagnostic rows display no rows. Values larger
than the stored row count are accepted and clamped by natural result
exhaustion.

Signed forms such as `LIMIT +1` and `LIMIT -1`, non-integer literals, and
values above `UINT64_MAX` are rejected with deterministic syntax diagnostics.

## Diagnostics and Unsupported Forms

Diagnostics follow existing MyLite parser and execution policy:

- syntax errors for unsupported grammar such as `SHOW COUNT (*) WARNINGS`,
  `SHOW WARNINGS LIKE`, `SHOW WARNINGS WHERE`, `SHOW WARNINGS ORDER BY`,
  aliases, expressions, parameters, functions, signed limit literals, and
  non-integer limit literals;
- deterministic parse diagnostics for limit literals above `UINT64_MAX`;
- allocation failures while copying the previous diagnostics snapshot,
  creating result columns, formatting diagnostic codes/counts, or appending
  result rows are reported as `HY001`;
- public API misuse remains handled by the existing `mylite_execute()` and
  result APIs;
- physical SQLite failures are not expected because this slice does not query
  or mutate SQLite.

No schema, table, column, or reserved `_mylite_*` name diagnostics are added
because this statement has no schema or object operands.

## Physical SQLite and File-Format Policy

No SQLite SQL is generated. The implementation must not inspect SQLite schema
text, prepare statements, create tables, update rows, mutate catalog rows,
change descriptor versions, invalidate descriptor caches, increment catalog
generation, change `sqlite_schema_generation`, or add SQLite fork patches.

File-backed tests must continue to verify that diagnostics statements leave the
MyLite preamble unchanged.

## Tests

Required C coverage:

- parser acceptance for `SHOW WARNINGS`, all supported `LIMIT` forms, and
  `SHOW COUNT(*) WARNINGS`;
- parser rejection for `SHOW COUNT (*) WARNINGS`, signed/non-integer limit
  forms, `LIKE`, `WHERE`, `ORDER BY`, unsupported diagnostic statements, and
  combined unsupported clauses;
- empty diagnostics after a harmless nondiagnostic statement;
- warning rows and count after `SHOW PROCESSLIST`;
- diagnostic chaining: `SHOW WARNINGS` and `SHOW COUNT(*) WARNINGS` do not
  clear the previous snapshot, while a later ordinary statement does;
- parse-error snapshot exposure as a `Level = Error` row;
- `LIMIT 0`, exact row counts, offset forms, and `UINT64_MAX`;
- successful diagnostic result warning counts, row counts, and absence of
  catalog/storage mutations;
- file-backed preamble preservation and independent handle snapshots;
- zero-initialized cleanup for the new diagnostics snapshot storage;
- existing parser, diagnostics, statement-context, process-list, result,
  file-format, VFS, catalog, DDL, DML, and lifecycle tests still pass.

Required MySQL expectation artifact:

- verify MySQL 8.4.9 version;
- verify result column labels, warning rows, count column label, diagnostic
  chaining, ordinary-statement clearing, limit slicing, syntax rejections, and
  parse-error exposure.

## Compatibility Documentation

Update `COMPATIBILITY.md` and
`docs/compatibility/sql-show-statements.md` to mark the verified
`SHOW WARNINGS` and `SHOW COUNT(*) WARNINGS` retained-diagnostics baseline
green. Keep `GET DIAGNOSTICS`, diagnostics stacks, stored-program condition
handling, and broader diagnostics-area behavior in separate compatibility rows.
