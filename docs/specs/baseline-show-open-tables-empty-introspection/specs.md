# Baseline SHOW OPEN TABLES Empty Introspection

## Status

This feature specifies a narrow embedded introspection slice for
`SHOW OPEN TABLES`. It adds parser and result-builder support on top of
`mylite_execute()`, statement context, the MyLite parser scaffold,
file-backed `.mylite` opening, durable catalog descriptors, schema/table
lifecycle, baseline DML, and existing descriptor-driven `SHOW` statements.

The feature is intentionally not a MySQL table-cache implementation. MySQL
reports rows for tables present in its server table cache. MyLite currently has
no server-wide table cache, explicit table-lock surface, `HANDLER` lifecycle,
temporary tables, or table-open accounting. This slice therefore exposes the
MySQL 8.4.9 result column shape and accepted syntax while returning zero rows.
That is an embedded-compatible placeholder, not a claim of full table-cache
introspection.

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
  `docs/specs/baseline-show-triggers-empty-introspection/specs.md`, and
  `docs/specs/baseline-show-events-empty-introspection/specs.md`
- MySQL lexer:
  `docs/specs/mysql-lexer/specs.md`
- MySQL parser scaffold:
  `docs/specs/mysql-parser-scaffold/specs.md`
- SQLite source snapshot notes: `third_party/sqlite/README.md`
- MySQL 8.4 Reference Manual, `SHOW OPEN TABLES`:
  https://dev.mysql.com/doc/refman/8.4/en/show-open-tables.html
- MySQL 8.4 Reference Manual, extensions to `SHOW` statements:
  https://dev.mysql.com/doc/refman/8.4/en/extended-show.html
- MySQL 8.4 Reference Manual, table cache behavior:
  https://dev.mysql.com/doc/refman/8.4/en/table-cache.html

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior, public
SQLite APIs, and existing MyLite source code. It does not copy MySQL, MariaDB,
Percona, SQLite implementation internals, or other restrictively licensed
implementation sources.

## MySQL 8.4.9 Runtime Observations

Observed against the local `mysql:8.4.9` runtime:

- Official syntax admits `SHOW OPEN TABLES [{FROM | IN} db_name]
  [LIKE 'pattern' | WHERE expr]`.
- Bare `SHOW OPEN TABLES` is global table-cache introspection. It does not
  require a selected default schema.
- `FROM` and `IN` are synonyms for an explicit schema restriction.
- `SHOW OPEN TABLES FROM missing_schema` and
  `SHOW OPEN TABLES IN missing_schema` succeed with an empty result set,
  `@@warning_count == 0`, and `ROW_COUNT() == -1`.
- An existing schema with no open cached tables returns a successful result
  set with the standard columns and zero rows.
- Successful `SHOW OPEN TABLES` leaves `@@warning_count == 0` and makes
  `ROW_COUNT()` return `-1`.
- `LIKE` matches table names, not database names. In the observed
  `@@lower_case_table_names == 0` runtime, matching is case-sensitive.
- `LIKE` supports `%`, `_`, and backslash escaping.
- A newly created table may not be visible after `FLUSH TABLES`, while a table
  selected through the server can appear with `In_use = 0` and
  `Name_locked = 0`. This reflects MySQL's server table cache, not durable
  table descriptors.
- `WHERE` is accepted by MySQL and evaluates against displayed column names,
  including `` `Table` ``.
- `FULL`, `EXTENDED`, `ORDER BY`, `LIMIT`, and combining
  `LIKE ... WHERE ...` are syntax errors.

The standard result columns observed from MySQL 8.4.9 and documented in the
manual are:

```text
Database
Table
In_use
Name_locked
```

## Scope

The implementation must add:

- parser and AST support for `SHOW OPEN TABLES`;
- optional explicit schema syntax with `FROM` and `IN` synonyms;
- optional `LIKE 'pattern'` filters using the existing `SHOW LIKE` filter
  decoder, even though the current row set is empty;
- MySQL-compatible success without a selected/default schema;
- MySQL-compatible empty success for unknown explicit schema names;
- reserved `_mylite_*` explicit schema-name rejection before descriptor lookup;
- the MySQL 8.4.9 `SHOW OPEN TABLES` 4-column result shape;
- zero result rows because MyLite does not yet expose server table-cache rows;
- deterministic diagnostics for unsupported syntax and unsupported `LIKE`
  forms;
- result-set warning and row-count behavior matching existing MyLite result
  conventions and observed MySQL 8.4.9 behavior;
- fast C tests and a MySQL 8.4.9 expectation artifact for supported behavior
  and deliberately deferred wider forms.

## Non-Goals

This feature must not implement:

- server-wide or connection-local table-cache accounting;
- rows for currently accessed base tables, temporary tables, views,
  `HANDLER ... OPEN`, table locks, pending lock requests, or name locks;
- `LOCK TABLES`, `UNLOCK TABLES`, `HANDLER`, temporary-table lifecycle,
  privilege filtering, or performance-schema metadata;
- `SHOW OPEN TABLES ... WHERE`;
- `FULL`, `EXTENDED`, `ORDER BY`, `LIMIT`, or combined `LIKE ... WHERE`;
- arbitrary SQLite metadata reads, arbitrary SQLite SQL pass-through, or
  SQLite fork patches.

## Ownership Boundary

- The public API remains unchanged. `mylite_execute()` owns public validation,
  result-handle ownership, statement-boundary row-count state, and failure
  cleanup.
- Statement context owns diagnostics reset, warning count, and statement
  completion. Successful `SHOW OPEN TABLES` is a result-set statement and
  stores `-1` as the connection-local previous row count.
- Lexer/parser/AST own syntax admission and source spans. They remain
  independent of runtime, catalog, storage, and SQLite.
- Analyzer/planner code does not resolve a default schema for bare
  `SHOW OPEN TABLES`, because MySQL treats it as global table-cache
  introspection. Explicit schema names are copied and checked for MyLite
  reserved-name collisions, but an unknown explicit schema is a
  MySQL-compatible empty success for this slice.
- The catalog module remains authoritative for schema and table descriptors,
  but this slice does not need descriptor iteration because it has no table
  cache rows to report. It does not mutate catalog rows, descriptor versions,
  descriptor caches, catalog generation, or `sqlite_schema_generation`.
- Runtime execution builds a result directly from MyLite-owned static metadata.
  It does not query SQLite table metadata, `sqlite_schema`, pragma output,
  lock state, or performance-schema state.
- The result builder owns the 4 result columns and the empty row set.
- Storage/VFS owns the `.mylite` preamble and shifted SQLite payload boundary.
  This introspection does not touch byte range `[0, 4096)`.

## Supported SQL Grammar

Supported subset:

```sql
SHOW OPEN TABLES
SHOW OPEN TABLES {FROM | IN} identifier
SHOW OPEN TABLES [schema_clause] LIKE 'pattern'
```

`schema_clause` is:

```sql
{FROM | IN} identifier
```

The identifier is a schema name. It is not a table name and cannot be
schema-qualified in this slice.

MyLite Lemon-syntax grammar snippet:

```lemon
statement ::= show_open_tables_statement.

show_open_tables_statement ::=
    SHOW OPEN TABLES show_schema_clause_opt show_like_clause_opt.

show_schema_clause_opt ::= .
show_schema_clause_opt ::= FROM identifier.
show_schema_clause_opt ::= IN identifier.
```

`OPEN` remains usable as an identifier where identifier grammar admits
nonreserved keywords. It is mapped explicitly only in the admitted
`SHOW OPEN TABLES` syntax.

## Schema Handling

Bare `SHOW OPEN TABLES` succeeds even when no schema is selected. MyLite does
not resolve the selected/default schema and does not report
`No database selected` for this statement.

When `FROM schema_name` or `IN schema_name` is present, MyLite accepts the
schema token as a display scope for the empty table-cache row set. If the
schema is unknown, MyLite returns the standard columns with zero rows and no
warning, matching observed MySQL 8.4.9 behavior.

Any explicit schema name beginning with MyLite's reserved `_mylite_` prefix is
rejected before descriptor lookup and before any SQLite SQL could be generated.
The diagnostic follows the existing reserved-name policy from the catalog
lifecycle slices. This is a MyLite-specific guardrail for internal object
names.

This slice does not add new case-folding, collation, system-schema, or
privilege behavior.

## LIKE Semantics

MySQL applies `LIKE` to table names in the open-table row set. Since MyLite has
no open-table descriptors and emits no rows, `LIKE` has no row-level effect in
this slice. MyLite still parses and decodes `LIKE 'pattern'` so that
unsupported pattern forms and NUL-producing escapes keep the same
deterministic diagnostics as other supported `SHOW LIKE` statements.

Supported patterns are ordinary string literals using the current MyLite
`SHOW LIKE` subset: `%`, `_`, and backslash escaping without NUL-producing
escapes. National string literals, character-set introducers, numeric
literals, `NULL`, parameters, expressions, and functions are unsupported
syntax.

## Result Semantics

Successful `SHOW OPEN TABLES` appends the standard 4 result columns and zero
rows. The columns are:

| Ordinal | Column |
| --- | --- |
| 1 | `Database` |
| 2 | `Table` |
| 3 | `In_use` |
| 4 | `Name_locked` |

Successful statements return through the existing public result API
conventions for row-producing statements. They have `warning_count == 0`,
`affected_rows == 0`, no result rows, and connection-local `ROW_COUNT()` state
of `-1`.

## Diagnostics

Required diagnostics:

- public API misuse: preserve existing public API behavior;
- syntax errors and unsupported grammar: existing parse diagnostic
  `1064` / `42000`;
- bare `SHOW OPEN TABLES` without a selected schema: successful empty result
  set, not an error;
- unknown explicit schema: successful empty result set, not an error;
- reserved `_mylite_*` schema names: existing MyLite reserved-name diagnostic;
- unsupported `WHERE`, `FULL`, `EXTENDED`, `ORDER BY`, `LIMIT`, combined
  `LIKE ... WHERE`, table-qualified schema names, unsupported `LIKE` literals,
  parameters, functions, or expressions: deterministic parse or `SHOW LIKE`
  diagnostics;
- allocation failures: existing out-of-memory diagnostic;
- unexpected catalog or physical SQLite failures: existing runtime diagnostic,
  without exposing SQLite internals as MySQL-compatible behavior.

## Physical SQLite and File Format Policy

This is a MyLite wrapper/translation feature. It uses public SQLite APIs only
through the existing catalog-opening path and does not require SQLite extension
APIs or SQLite fork hooks.

The implementation must not generate user SQLite SQL, inspect SQLite schema or
lock metadata, create physical indexes, mutate physical tables, or write
catalog rows. It must preserve `.mylite` preamble bytes, shifted SQLite payload
invariants, catalog generation, descriptor cache state, and
`sqlite_schema_generation`.

## Tests

Fast C tests must cover:

- result column shape and zero rows for bare, `FROM`, and `IN` forms, with and
  without a selected schema;
- unknown explicit schema success with zero rows and no warnings;
- accepted `LIKE` patterns, including wildcard and escaped underscore
  patterns, with zero rows;
- reserved `_mylite_*` explicit schema diagnostics;
- unsupported syntax: `WHERE`, `FULL`, `EXTENDED`, `ORDER BY`, `LIMIT`,
  combined `LIKE ... WHERE`, numeric `LIKE`, `NULL` `LIKE`, national-string
  `LIKE`, introducer `LIKE`, parameters, and functions;
- result API state: row result set present, zero rows, zero warnings, affected
  rows zero, and connection-local `ROW_COUNT()` `-1`;
- schema create/drop/reopen behavior;
- preamble and generation invariants;
- independent file-backed handles.

The MySQL expectation script must verify:

- MySQL 8.4.9 runtime version;
- exact result headers;
- bare statement success without a selected schema;
- empty-schema and unknown-schema zero-row behavior and
  `@@warning_count`/`ROW_COUNT()` state;
- selected/default schema, `FROM`, and `IN` forms;
- `LIKE` matching table names and case-sensitive matching in the observed
  runtime;
- upstream acceptance of `WHERE` and rejection of unsupported wider forms;
- the observed table-cache row behavior after a table is selected, recorded as
  the deliberate compatibility gap for this empty MyLite slice.

## Compatibility Documentation

`COMPATIBILITY.md` and `docs/compatibility/sql-show-statements.md` must mark
`SHOW OPEN TABLES` as limited support for embedded empty open-table
introspection with MySQL 8.4.9 column labels and `LIKE` filters. They must
explicitly say that table-cache rows, lock counts, name-lock state, temporary
tables, `HANDLER`, `LOCK TABLES`, `WHERE`, privileges, and performance-schema
metadata remain unsupported.

## Review Checklist

- The parser admits only the specified independent grammar subset.
- MySQL 8.4.9 evidence backs every user-visible behavior and documented
  incompatibility.
- Runtime uses static result metadata and does not require selected-schema
  state for bare `SHOW OPEN TABLES`.
- Unknown explicit schemas are empty successes.
- No open-table accounting, table-cache rows, lock state, catalog mutation,
  SQLite metadata, SQLite schema text, or SQLite fork patch is introduced.
- The result columns match MySQL 8.4.9 exactly.
- Diagnostics follow existing MyLite/MySQL-compatible policies.
- Tests cover successful forms, deferred forms, diagnostics, persistence,
  file-format safety, and independent handles.
