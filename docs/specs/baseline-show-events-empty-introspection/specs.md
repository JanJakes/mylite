# Baseline SHOW EVENTS Empty Introspection

## Status

This feature specifies a narrow event-introspection slice for MyLite schemas
that currently have no event descriptors. It adds `SHOW EVENTS` parsing and
result construction on top of `mylite_execute()`, statement context, the
MyLite parser scaffold, file-backed `.mylite` opening, durable catalog
descriptors, schema/table lifecycle, baseline DML, and existing
descriptor-driven `SHOW` statements.

The feature is intentionally not event support. It exposes MySQL's
`SHOW EVENTS` result column shape and returns zero rows until `CREATE EVENT`,
`ALTER EVENT`, `DROP EVENT`, event descriptors, the Event Scheduler, event
execution, privileges, and `INFORMATION_SCHEMA.EVENTS` exist.

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
  `docs/specs/baseline-show-character-set-collation/specs.md`,
  `docs/specs/baseline-show-index-empty-introspection/specs.md`, and
  `docs/specs/baseline-show-triggers-empty-introspection/specs.md`
- MySQL lexer:
  `docs/specs/mysql-lexer/specs.md`
- MySQL parser scaffold:
  `docs/specs/mysql-parser-scaffold/specs.md`
- SQLite source snapshot notes: `third_party/sqlite/README.md`
- MySQL 8.4 Reference Manual, `SHOW EVENTS`:
  https://dev.mysql.com/doc/refman/8.4/en/show-events.html
- MySQL 8.4 Reference Manual, extensions to `SHOW` statements:
  https://dev.mysql.com/doc/refman/8.4/en/extended-show.html
- MySQL 8.4 Reference Manual, `CREATE EVENT`:
  https://dev.mysql.com/doc/refman/8.4/en/create-event.html
- MySQL 8.4 Reference Manual, `INFORMATION_SCHEMA.EVENTS`:
  https://dev.mysql.com/doc/refman/8.4/en/information-schema-events-table.html

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior, public
SQLite APIs, and existing MyLite source code. It does not copy MySQL, MariaDB,
Percona, SQLite implementation internals, or other restrictively licensed
implementation sources.

## MySQL 8.4.9 Runtime Observations

Observed against the local `mysql:8.4.9` runtime:

- Official syntax admits `SHOW EVENTS [{FROM | IN} schema_name] [LIKE 'pattern' | WHERE expr]`.
- `FROM` and `IN` are synonyms for the explicit schema clause.
- With no selected schema and no explicit schema, `SHOW EVENTS` fails with
  error `1046`, SQLSTATE `3D000`, and message `No database selected`.
- With no selected schema, `SHOW EVENTS LIKE 'x%'` fails with the same
  `1046` / `3D000` diagnostic.
- Unlike many other schema-scoped `SHOW` statements, `SHOW EVENTS FROM
  missing_schema` and `SHOW EVENTS IN missing_schema` succeed with an empty
  result set, `@@warning_count == 0`, and `ROW_COUNT() == -1`.
- An existing schema with no events returns a successful result set with the
  standard `SHOW EVENTS` columns and zero rows.
- Successful `SHOW EVENTS` leaves `@@warning_count == 0` and makes
  `ROW_COUNT()` return `-1`.
- `LIKE` matches event names. It does not match unrelated table names.
- Event-name `LIKE` matching is case-insensitive in the observed
  `@@lower_case_table_names == 0` runtime.
- `LIKE` supports `%`, `_`, and backslash escaping.
- `WHERE` is accepted by MySQL and evaluates against displayed column names,
  including names such as `Name` and backtick-quoted `` `Time zone` ``.
- `FULL`, `EXTENDED`, `ORDER BY`, `LIMIT`, singular `SHOW EVENT`, and
  combining `LIKE ... WHERE ...` are syntax errors.

The standard result columns observed from MySQL 8.4.9 and documented in the
manual are:

```text
Db
Name
Definer
Time zone
Type
Execute at
Interval value
Interval field
Starts
Ends
Status
Originator
character_set_client
collation_connection
Database Collation
```

## Scope

The implementation must add:

- parser and AST support for `SHOW EVENTS`;
- optional explicit schema syntax with `FROM` and `IN` synonyms;
- selected/default schema resolution for unqualified `SHOW EVENTS`;
- MySQL-compatible empty success for unknown explicit schema names;
- optional `LIKE 'pattern'` filters using the existing `SHOW LIKE` filter
  decoder, even though the current row set is empty;
- reserved `_mylite_*` explicit schema-name rejection before descriptor lookup;
- the MySQL 8.4.9 `SHOW EVENTS` 15-column result shape;
- zero result rows for current schemas because no event descriptors are
  supported yet;
- deterministic diagnostics for unsupported syntax and missing default schema;
- result-set warning and row-count behavior matching existing MyLite result
  conventions and observed MySQL 8.4.9 behavior;
- fast C tests and a MySQL 8.4.9 expectation artifact for supported behavior
  and deliberately deferred wider forms.

## Non-Goals

This feature must not implement:

- `CREATE EVENT`, `ALTER EVENT`, `DROP EVENT`, `SHOW CREATE EVENT`, event
  descriptor storage, event scheduling, event execution, Event Scheduler status,
  privileges, definers, stored-program SQL mode capture, event character-set
  metadata, time zones, intervals, statuses, replication originators, or
  `INFORMATION_SCHEMA.EVENTS`;
- rows in `SHOW EVENTS` for any event kind;
- `SHOW EVENTS ... WHERE`;
- `FULL`, `EXTENDED`, `ORDER BY`, `LIMIT`, singular `SHOW EVENT`, or combined
  `LIKE ... WHERE ...`;
- `mysql.event`, arbitrary SQLite metadata reads, arbitrary SQLite SQL
  pass-through, or SQLite fork patches.

## Ownership Boundary

- The public API remains unchanged. `mylite_execute()` owns public validation,
  result-handle ownership, statement-boundary row-count state, and failure
  cleanup.
- Statement context owns diagnostics reset, warning count, and statement
  completion. Successful `SHOW EVENTS` is a result-set statement and stores
  `-1` as the connection-local previous row count.
- Lexer/parser/AST own syntax admission and source spans. They remain
  independent of runtime, catalog, storage, and SQLite.
- Analyzer/planner code resolves the selected/default schema only for
  unqualified `SHOW EVENTS`. Explicit schema names are copied and checked for
  MyLite reserved-name collisions, but an unknown explicit schema is a
  MySQL-compatible empty success for this slice.
- The catalog module remains authoritative for known schema descriptors. This
  slice may read a selected schema descriptor but does not mutate catalog rows,
  descriptor versions, descriptor caches, catalog generation, or
  `sqlite_schema_generation`.
- Runtime execution builds a result directly from MyLite-owned static metadata.
  It does not query SQLite table metadata, `sqlite_schema`, pragma output,
  event metadata, `mysql.event`, or `INFORMATION_SCHEMA`.
- The result builder owns the 15 result columns and the empty row set.
- Storage/VFS owns the `.mylite` preamble and shifted SQLite payload boundary.
  This introspection does not touch byte range `[0, 4096)`.

## Supported SQL Grammar

Supported subset:

```sql
SHOW EVENTS
SHOW EVENTS {FROM | IN} identifier
SHOW EVENTS [schema_clause] LIKE 'pattern'
```

`schema_clause` is:

```sql
{FROM | IN} identifier
```

The identifier is a schema name. It is not an event name and cannot be
schema-qualified in this slice.

MyLite Lemon-syntax grammar snippet:

```lemon
statement ::= show_events_statement.

show_events_statement ::=
    SHOW EVENTS show_schema_clause_opt show_like_clause_opt.

show_schema_clause_opt ::= .
show_schema_clause_opt ::= FROM identifier.
show_schema_clause_opt ::= IN identifier.
```

`EVENTS` remains usable as an identifier where identifier grammar admits
nonreserved keywords. It is mapped explicitly only in the admitted
`SHOW EVENTS` syntax. `EVENT` remains reserved and unsupported in this slice.

## Schema Resolution

When no explicit schema clause is present, MyLite resolves the current
selected/default schema. If no schema is selected, MyLite reports
`1046` / `3D000` (`No database selected`).

When `FROM schema_name` or `IN schema_name` is present, MyLite accepts the
schema token as the display scope for the empty event row set. If the schema is
unknown, MyLite returns the standard columns with zero rows and no warning,
matching observed MySQL 8.4.9 behavior.

Any explicit schema name beginning with MyLite's reserved `_mylite_` prefix is
rejected before descriptor lookup and before any SQLite SQL could be generated.
The diagnostic follows the existing reserved-name policy from the catalog
lifecycle slices. This is a MyLite-specific guardrail for internal object
names.

Current MyLite catalog lookups use the existing descriptor name policy. This
slice does not add new case-folding, collation, or system-schema behavior.

## LIKE Semantics

MySQL applies `LIKE` to event names. Since MyLite has no event descriptors and
emits no rows, `LIKE` has no row-level effect in this slice. MyLite still parses
and decodes `LIKE 'pattern'` so that unsupported pattern forms and
NUL-producing escapes keep the same deterministic diagnostics as other
supported `SHOW LIKE` statements.

Supported patterns are ordinary string literals using the current MyLite
`SHOW LIKE` subset: `%`, `_`, and backslash escaping without NUL-producing
escapes. National string literals, character-set introducers, numeric literals,
`NULL`, parameters, expressions, and functions are unsupported syntax.

## Result Semantics

Successful `SHOW EVENTS` appends the standard 15 result columns and zero rows.
The columns are:

| Ordinal | Column |
| --- | --- |
| 1 | `Db` |
| 2 | `Name` |
| 3 | `Definer` |
| 4 | `Time zone` |
| 5 | `Type` |
| 6 | `Execute at` |
| 7 | `Interval value` |
| 8 | `Interval field` |
| 9 | `Starts` |
| 10 | `Ends` |
| 11 | `Status` |
| 12 | `Originator` |
| 13 | `character_set_client` |
| 14 | `collation_connection` |
| 15 | `Database Collation` |

Successful statements return through the existing public result API
conventions for row-producing statements. They have `warning_count == 0`,
`affected_rows == 0`, no result rows, and connection-local `ROW_COUNT()` state
of `-1`.

## Diagnostics

Required diagnostics:

- public API misuse: preserve existing public API behavior;
- syntax errors and unsupported grammar: existing parse diagnostic
  `1064` / `42000`;
- missing selected schema for unqualified `SHOW EVENTS`: `1046` / `3D000`;
- unknown explicit schema: successful empty result set, not an error;
- reserved `_mylite_*` schema names: existing MyLite reserved-name diagnostic;
- unsupported `WHERE`, `FULL`, `EXTENDED`, `ORDER BY`, `LIMIT`, singular
  `SHOW EVENT`, combined `LIKE ... WHERE`, table-qualified schema names,
  unsupported `LIKE` literals, parameters, functions, or expressions:
  deterministic parse or `SHOW LIKE` diagnostics;
- allocation failures: existing out-of-memory diagnostic;
- unexpected catalog or physical SQLite failures: existing runtime diagnostic,
  without exposing SQLite internals as MySQL-compatible behavior.

## Physical SQLite and File Format Policy

This is a MyLite wrapper/translation feature. It uses public SQLite APIs only
through the existing catalog-opening path and does not require SQLite extension
APIs or SQLite fork hooks.

The implementation must not generate user SQLite SQL, inspect SQLite event or
trigger metadata, create SQLite triggers, create physical indexes, mutate
physical tables, or write catalog rows. It must preserve `.mylite` preamble
bytes, shifted SQLite payload invariants, catalog generation, descriptor cache
state, and `sqlite_schema_generation`.

## Tests

Fast C tests must cover:

- result column shape and zero rows for selected/default schema, `FROM`, and
  `IN` forms;
- unknown explicit schema success with zero rows and no warnings;
- accepted `LIKE` patterns, including wildcard and escaped underscore patterns,
  with zero rows;
- no selected schema and reserved `_mylite_*` schema diagnostics;
- unsupported syntax: `WHERE`, `FULL`, `EXTENDED`, `ORDER BY`, `LIMIT`,
  singular `SHOW EVENT`, combined `LIKE ... WHERE`, numeric `LIKE`, `NULL`
  `LIKE`, national-string `LIKE`, introducer `LIKE`, parameters, and functions;
- result API state: row result set present, zero rows, zero warnings, affected
  rows zero, and connection-local `ROW_COUNT()` `-1`;
- schema create/drop/reopen behavior;
- preamble and generation invariants;
- independent file-backed handles.

The MySQL expectation script must verify:

- MySQL 8.4.9 runtime version;
- exact result headers;
- empty-schema and unknown-schema zero-row behavior and
  `@@warning_count`/`ROW_COUNT()` state;
- selected/default schema, `FROM`, and `IN` forms;
- `LIKE` matching event names and case-insensitive matching in the observed
  runtime;
- upstream acceptance of `WHERE` and rejection of unsupported wider forms;
- missing default schema diagnostics.

## Compatibility Documentation

`COMPATIBILITY.md` and `docs/compatibility/sql-show-statements.md` must mark
`SHOW EVENTS` as MySQL-runtime-verified empty event introspection with MySQL
8.4.9 column labels, `LIKE` filters, documented empty-result `WHERE`
predicates, and queryable empty `INFORMATION_SCHEMA.EVENTS` metadata. They must
explicitly say that event descriptors, event rows, event DDL, `SHOW CREATE
EVENT`, non-empty row predicate evaluation, privileges, and the Event Scheduler
remain unsupported.

## Review Checklist

- The parser admits only the specified independent grammar subset.
- MySQL 8.4.9 evidence backs every user-visible behavior and documented
  incompatibility.
- Runtime uses static result metadata and only the minimal selected-schema
  check required by MySQL behavior.
- Unknown explicit schemas are empty successes, while missing selected schema
  is still an error.
- No event storage, event scheduling, event execution, catalog mutation, SQLite
  metadata, SQLite schema text, or SQLite fork patch is introduced.
- The result columns match MySQL 8.4.9 exactly.
- Diagnostics follow existing MyLite/MySQL-compatible policies.
- Tests cover successful forms, deferred forms, diagnostics, persistence,
  file-format safety, and independent handles.
