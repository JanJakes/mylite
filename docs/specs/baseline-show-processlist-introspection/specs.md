# Baseline SHOW Processlist Introspection

## Status

This feature specifies a narrow `SHOW PROCESSLIST` introspection slice for
MyLite embedded handles. It adds parser and result-builder support for
`SHOW PROCESSLIST` and `SHOW FULL PROCESSLIST` on top of `mylite_execute()`,
statement context, the parser scaffold, durable session state, file-backed
`.mylite` opening, and existing baseline `SHOW` result conventions.

The feature is intentionally not server-wide thread instrumentation. MyLite has
one embedded handle per public connection and no background SQL server, event
scheduler thread reporting, privilege system, Performance Schema process list,
`INFORMATION_SCHEMA.PROCESSLIST`, or `KILL` support. The result reports the
current executing MyLite handle only.

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
- Baseline catalog and schema/table/DML lifecycle slices:
  `docs/specs/baseline-catalog-foundation/specs.md`,
  `docs/specs/baseline-basic-table-lifecycle/specs.md`,
  `docs/specs/baseline-table-rename-lifecycle/specs.md`,
  `docs/specs/baseline-row-values-lifecycle/specs.md`,
  `docs/specs/baseline-select-where-lifecycle/specs.md`,
  `docs/specs/baseline-select-order-limit-lifecycle/specs.md`,
  `docs/specs/baseline-update-lifecycle/specs.md`, and
  `docs/specs/baseline-delete-lifecycle/specs.md`
- Baseline `SHOW` slices:
  `docs/specs/baseline-show-like-filters/specs.md`,
  `docs/specs/baseline-show-table-status-introspection/specs.md`,
  `docs/specs/baseline-show-index-empty-introspection/specs.md`,
  `docs/specs/baseline-show-triggers-empty-introspection/specs.md`,
  `docs/specs/baseline-show-events-empty-introspection/specs.md`,
  `docs/specs/baseline-show-open-tables-empty-introspection/specs.md`, and
  `docs/specs/baseline-show-routine-status-empty-introspection/specs.md`
- MySQL lexer:
  `docs/specs/mysql-lexer/specs.md`
- MySQL parser scaffold:
  `docs/specs/mysql-parser-scaffold/specs.md`
- SQLite source snapshot notes: `third_party/sqlite/README.md`
- MySQL 8.4 Reference Manual, `SHOW PROCESSLIST`:
  https://dev.mysql.com/doc/refman/8.4/en/show-processlist.html
- MySQL 8.4 Reference Manual, accessing the process list:
  https://dev.mysql.com/doc/refman/8.4/en/processlist-access.html

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior, public
SQLite APIs, and existing MyLite source code. It does not copy MySQL, MariaDB,
Percona, SQLite implementation internals, or other restrictively licensed
implementation sources.

## MySQL 8.4.9 Runtime Observations

Observed against the local `mysql:8.4.9` runtime with
`@@performance_schema_show_processlist = 0`:

- Official syntax admits only `SHOW [FULL] PROCESSLIST`.
- The result columns are `Id`, `User`, `Host`, `db`, `Command`, `Time`,
  `State`, and `Info`.
- A connection running `SHOW PROCESSLIST` appears as a row with
  `Command = Query`, `Time = 0`, `State = init`, and `Info = SHOW PROCESSLIST`.
- A connection running `SHOW FULL PROCESSLIST` appears with
  `Info = SHOW FULL PROCESSLIST`.
- Without a selected database, the current connection row has `db = NULL`.
  After `USE schema_name`, the current connection row has `db = schema_name`.
- Successful statements leave `ROW_COUNT() = -1`.
- The default information-schema-backed implementation reports
  `@@warning_count = 1` and warning code `1287` with the deprecation warning
  telling users to prefer `performance_schema.processlist`.
- Without `FULL`, the `Info` value is truncated to the first 100 characters of
  the statement text.
- With `FULL`, the `Info` value is not truncated. A `SHOW FULL /* long comment
  */ PROCESSLIST` probe returned the full 188-byte statement text.
- MySQL accepts comments before, between, and after `SHOW`, `FULL`, and
  `PROCESSLIST`; those comments are part of the `Info` text up to but not
  including the statement terminator.
- `LIKE`, `WHERE`, `ORDER BY`, `LIMIT`, `FROM`, `IN`, `EXTENDED`,
  `SHOW FULL PROCESSLIST LIMIT 1`, and query modifiers after the process-list
  statement are syntax errors.

## Scope

The implementation must add:

- parser and AST support for `SHOW PROCESSLIST` and
  `SHOW FULL PROCESSLIST`;
- the MySQL 8.4.9 8-column process-list result shape;
- one row for the current MyLite handle;
- current handle `Id` from the existing session connection id;
- `User` and `Host` derived from the existing embedded client user identity;
- `db` from the selected schema, or `NULL` when no schema is selected;
- `Command = Query`, `Time = 0`, and `State = init` for the currently
  executing statement;
- `Info` sourced from the executing statement text, truncated to 100 bytes for
  non-`FULL` output and untruncated for `FULL`;
- warning-count behavior matching the observed default MySQL 8.4.9 runtime for
  this statement;
- result-set row-count behavior matching existing MyLite and observed MySQL
  result-set conventions (`ROW_COUNT() = -1`);
- fast C tests and a MySQL 8.4.9 expectation artifact for supported behavior
  and deliberately deferred wider forms.

## Non-Goals

This feature must not implement:

- server-wide thread enumeration, background daemon rows, event scheduler rows,
  sleeping idle rows, thread state transitions, time accumulation, connection
  lifecycle telemetry, or concurrent statement observation;
- accounts, authentication, privileges, anonymous-user behavior, `PROCESS`,
  `CONNECTION_ADMIN`, definer semantics, or user/host resolution beyond the
  existing embedded session identity;
- Performance Schema, `INFORMATION_SCHEMA.PROCESSLIST`, `sys.processlist`,
  `sys.session`, `mysqladmin processlist`, `KILL`, or process-list status
  counters;
- `LIKE`, `WHERE`, `ORDER BY`, `LIMIT`, `FROM`, `IN`, `EXTENDED`, filters,
  projections, or expressions;
- arbitrary SQLite metadata reads, arbitrary SQLite SQL pass-through, storage
  mutations, catalog mutations, or SQLite fork patches.

## Ownership Boundary

- The public API remains unchanged. `mylite_execute()` owns public validation,
  parse/execution orchestration, result-handle ownership, statement-boundary
  row-count state, and failure cleanup.
- Statement context owns the executing SQL buffer, diagnostics reset, warning
  count, statement completion, and previous-row-count transition. The
  process-list result uses that active statement context as the source for the
  `Info` text instead of querying SQLite metadata.
- Lexer/parser/AST own syntax admission and source spans. They remain
  independent of runtime, catalog, storage, and SQLite.
- Analyzer/planner code does not resolve a target schema or object. The only
  schema state read is the selected/default schema value for the `db` output
  column.
- The catalog module remains authoritative for schema/table descriptors, but
  this slice does not need descriptor iteration and must not mutate catalog
  rows, descriptor versions, descriptor caches, catalog generation, or
  `sqlite_schema_generation`.
- Runtime execution builds a result directly from MyLite-owned session and
  statement-context state. It does not query `sqlite_schema`, pragma output,
  `INFORMATION_SCHEMA`, Performance Schema, or system tables.
- The result builder owns the eight result columns and one current-handle row.
- Storage/VFS owns the `.mylite` preamble and shifted SQLite payload boundary.
  This introspection does not touch byte range `[0, 4096)`.

## Supported SQL Grammar

Supported subset:

```sql
SHOW PROCESSLIST
SHOW FULL PROCESSLIST
```

MyLite Lemon-syntax grammar snippet:

```lemon
statement ::= show_processlist_statement.

show_processlist_statement ::= SHOW PROCESSLIST.

show_processlist_statement ::= SHOW FULL PROCESSLIST.
```

`PROCESSLIST` is already tokenized as a keyword by the MyLite lexer. This slice
maps it to an explicit parser token for the admitted `SHOW` syntax. The
existing parser policy for keyword-as-identifier use remains unchanged outside
this statement.

## Schema and Session Handling

`SHOW PROCESSLIST` and `SHOW FULL PROCESSLIST` succeed even when no schema is
selected. MyLite does not resolve a target schema and does not report
`No database selected`.

The `db` column is the selected schema name from session state when one exists;
otherwise it is `NULL`. Dropping the selected schema already clears the
session selection, so a later process-list statement returns `NULL`.

There is no explicit schema clause in the supported grammar. `SHOW PROCESSLIST
FROM db`, `SHOW PROCESSLIST IN db`, and schema-qualified process-list targets
are syntax errors.

The `Id` column is the existing MyLite session connection id. It must match the
value returned by `CONNECTION_ID()` for the same handle.

The `User` and `Host` columns are derived from the current embedded client
identity string. For the current default identity `root@%`, MyLite reports
`User = root` and `Host = %`. This is a deliberate embedded-session mapping;
MyLite does not report TCP peer names or ports.

## Result Semantics

Successful process-list statements append the standard eight result columns
and one row:

| Ordinal | Column | MyLite value |
| --- | --- | --- |
| 1 | `Id` | Session connection id as decimal text |
| 2 | `User` | Client identity before `@`, or the full identity if no `@` exists |
| 3 | `Host` | Client identity after `@`, or an empty string if no `@` exists |
| 4 | `db` | Selected schema text, or `NULL` |
| 5 | `Command` | `Query` |
| 6 | `Time` | `0` |
| 7 | `State` | `init` |
| 8 | `Info` | Current statement text, truncated for non-`FULL` |

The `Info` source is the current statement text from statement context, not
the AST node span. MyLite removes the final statement terminator and outer
trailing whitespace from that statement-context text, preserving leading
comments, between-token comments, trailing comments before the terminator, and
all other bytes inside the retained source range. For non-`FULL` output,
MyLite returns at most the first 100 bytes. For `FULL`, MyLite returns the
full retained source range.

Successful execution reports `warning_count == 1` for the default MySQL 8.4.9
process-list implementation observed by the expectation script. The warning
record uses MySQL code `1287` and a deprecation message that points users to
`performance_schema.processlist`. MyLite does not implement `SHOW WARNINGS` in
this slice, but the public result warning count must reflect the recorded
warning.

Successful execution has one result row and connection-local `ROW_COUNT()`
behavior equivalent to other MyLite result-set statements (`-1`). The public
result object follows the existing row-result API conventions.

## Diagnostics

Diagnostics follow existing MyLite parser and execution policy:

- syntax errors for unsupported grammar such as `LIKE`, `WHERE`, `ORDER BY`,
  `LIMIT`, `FROM`, `IN`, `EXTENDED`, filters, query modifiers, expressions,
  parameters, functions, or process-list source clauses;
- allocation failures while creating result columns, formatting row values,
  copying `Info`, or appending the warning are reported as `HY001`;
- public API misuse remains handled by the existing `mylite_execute()` and
  result APIs;
- physical SQLite failures are not expected because this slice does not query
  or mutate SQLite. If an unexpected physical dependency is introduced, it
  must use existing physical-error diagnostics and tests.

No reserved `_mylite_*` schema/table-name diagnostics are added because this
statement has no schema or table operands.

## Physical SQLite and File-Format Policy

No SQLite SQL is generated for process-list introspection. The implementation
must not inspect SQLite schema text, create tables, update rows, start physical
DML, or depend on SQLite compile-time process-list features. It must not add
indexes, constraints, triggers, virtual tables, VFS changes, or SQLite fork
patches.

The `.mylite` preamble and shifted SQLite payload invariants must remain
unchanged. Tests must compare the preamble before and after process-list
introspection on a file-backed handle.

## Tests

Add a fast plain C test under `packages/libmylite/tests/` and register it with
a dotted CTest name. Coverage must include:

- `SHOW PROCESSLIST` without a selected schema;
- `SHOW FULL PROCESSLIST` without a selected schema;
- exact result columns, one current-handle row, warning count, and row-count
  behavior;
- `Id` matching `CONNECTION_ID()` for the same handle;
- default embedded `User`/`Host` split from `root@%`;
- selected-schema and dropped-selected-schema `db` output;
- non-`FULL` 100-byte `Info` truncation and `FULL` untruncated `Info` using
  leading, between-token, and trailing comments;
- unsupported forms rejected deterministically: `LIKE`, `WHERE`, `ORDER BY`,
  `LIMIT`, `FROM`, `IN`, `EXTENDED`, `SHOW FULL PROCESSLIST LIMIT 1`, and
  expression/function/query-modifier forms;
- catalog generation and `sqlite_schema_generation` stability;
- file preamble preservation;
- independent handles with independent connection ids and selected schemas;
- zero-initialized cleanup for result objects and no public API surface
  changes.

Add a reproducible MySQL 8.4.9 expectation script that verifies official
syntax, result headers, selected/default database behavior, warning count and
warning code in the default process-list implementation, row-count behavior,
`FULL` versus non-`FULL` `Info` behavior, and unsupported syntax errors.

## Compatibility Documentation

Update `COMPATIBILITY.md` and `docs/compatibility/sql-show-statements.md` to
mark the verified current-handle `SHOW [FULL] PROCESSLIST` subset green and
keep server-wide process-list instrumentation, `INFORMATION_SCHEMA.PROCESSLIST`,
Performance Schema, sys-schema, process-list status-variable, privilege, and
server-management gaps separate.
