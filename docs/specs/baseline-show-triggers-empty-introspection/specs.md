# Baseline SHOW TRIGGERS Empty Introspection

## Status

This feature specifies a narrow trigger-introspection slice for MyLite
schemas that currently have no trigger descriptors. It adds `SHOW TRIGGERS`
parsing and result construction on top of `mylite_execute()`, statement
context, the MyLite parser scaffold, file-backed `.mylite` opening, durable
catalog descriptors, schema/table lifecycle, baseline DML, and existing
descriptor-driven `SHOW` statements.

The feature is intentionally not trigger support. It exposes MySQL's
`SHOW TRIGGERS` result column shape and returns zero rows for supported
schemas until `CREATE TRIGGER`, `DROP TRIGGER`, trigger descriptors, trigger
execution, privileges, and `INFORMATION_SCHEMA.TRIGGERS` exist.

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
  `docs/specs/baseline-show-character-set-collation/specs.md`, and
  `docs/specs/baseline-show-index-empty-introspection/specs.md`
- MySQL lexer:
  `docs/specs/mysql-lexer/specs.md`
- MySQL parser scaffold:
  `docs/specs/mysql-parser-scaffold/specs.md`
- SQLite source snapshot notes: `third_party/sqlite/README.md`
- MySQL 8.4 Reference Manual, `SHOW TRIGGERS`:
  https://dev.mysql.com/doc/refman/8.4/en/show-triggers.html
- MySQL 8.4 Reference Manual, extensions to `SHOW` statements:
  https://dev.mysql.com/doc/refman/8.4/en/extended-show.html
- MySQL 8.4 Reference Manual, `CREATE TRIGGER`:
  https://dev.mysql.com/doc/refman/8.4/en/create-trigger.html
- MySQL 8.4 Reference Manual, `INFORMATION_SCHEMA.TRIGGERS`:
  https://dev.mysql.com/doc/refman/8.4/en/information-schema-triggers-table.html

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior, public
SQLite APIs, and existing MyLite source code. It does not copy MySQL, MariaDB,
Percona, SQLite implementation internals, or other restrictively licensed
implementation sources.

## MySQL 8.4.9 Runtime Observations

Observed against the local `mysql:8.4.9` runtime:

- Official syntax admits `SHOW TRIGGERS [{FROM | IN} db_name] [LIKE 'pattern' | WHERE expr]`.
- MySQL 8.4.9 also accepts `SHOW FULL TRIGGERS` with the same result columns.
- `FROM` and `IN` are synonyms for the explicit schema clause.
- With no selected schema and no explicit schema, `SHOW TRIGGERS` fails with
  error `1046`, SQLSTATE `3D000`, and message `No database selected`.
- Unknown explicit schemas fail with error `1049`, SQLSTATE `42000`.
- An existing schema with no triggers returns a successful result set with the
  standard `SHOW TRIGGERS` columns and zero rows.
- Successful `SHOW TRIGGERS` leaves `@@warning_count == 0` and makes
  `ROW_COUNT()` return `-1`.
- `LIKE` matches table names associated with triggers, not trigger names.
  A trigger named `ins_sum` on table `account` is returned by
  `LIKE 'account'` and not by `LIKE 'ins%'`.
- With `@@lower_case_table_names == 0`, table-name matching is case-sensitive.
- `LIKE` supports `%`, `_`, and backslash escaping.
- `WHERE` is accepted by MySQL and evaluates against displayed column names,
  including backtick-quoted names such as `` `Trigger` ``,
  `` `Table` ``, and `` `Database Collation` ``.
- `EXTENDED`, `ORDER BY`, `LIMIT`, singular `SHOW TRIGGER`, and combining
  `LIKE ... WHERE ...` are syntax errors.

The standard result columns observed from MySQL 8.4.9 and documented in the
manual are:

```text
Trigger
Event
Table
Statement
Timing
Created
sql_mode
Definer
character_set_client
collation_connection
Database Collation
```

## Scope

The implementation must add:

- parser and AST support for `SHOW TRIGGERS`;
- optional `FULL`, because MySQL 8.4.9 accepts it and it has no extra visible
  effect while MyLite has no trigger descriptors;
- optional explicit schema resolution with `FROM` and `IN` synonyms;
- selected/default schema resolution for unqualified `SHOW TRIGGERS`;
- optional `LIKE 'pattern'` filters using the existing `SHOW LIKE` filter
  decoder, even though the current row set is empty;
- reserved `_mylite_*` schema-name rejection before descriptor lookup;
- the MySQL 8.4.9 `SHOW TRIGGERS` 11-column result shape;
- zero result rows for current schemas because no trigger descriptors are
  supported yet;
- deterministic diagnostics for unsupported syntax and unresolved names;
- result-set warning and row-count behavior matching existing MyLite result
  conventions and observed MySQL 8.4.9 behavior;
- fast C tests and a MySQL 8.4.9 expectation artifact for supported behavior
  and deliberately deferred wider forms.

## Non-Goals

This feature must not implement:

- `CREATE TRIGGER`, `DROP TRIGGER`, `SHOW CREATE TRIGGER`, trigger execution,
  trigger ordering, trigger descriptor storage, trigger persistence, trigger
  privileges, definers, stored-program SQL mode capture, trigger character-set
  metadata, or `INFORMATION_SCHEMA.TRIGGERS`;
- rows in `SHOW TRIGGERS` for any trigger kind;
- `SHOW TRIGGERS ... WHERE`;
- `EXTENDED`, `ORDER BY`, `LIMIT`, singular `SHOW TRIGGER`, or combined
  `LIKE ... WHERE ...`;
- temporary tables, views, `mysql` schema tables, arbitrary SQLite metadata
  reads, arbitrary SQLite SQL pass-through, or SQLite fork patches.

## Ownership Boundary

- The public API remains unchanged. `mylite_execute()` owns public validation,
  result-handle ownership, statement-boundary row-count state, and failure
  cleanup.
- Statement context owns diagnostics reset, warning count, and statement
  completion. Successful `SHOW TRIGGERS` is a result-set statement and stores
  `-1` as the connection-local previous row count.
- Lexer/parser/AST own syntax admission and source spans. They remain
  independent of runtime, catalog, storage, and SQLite.
- Analyzer/planner code resolves the target schema and rejects reserved names
  using MyLite catalog policy.
- The catalog module remains authoritative for schema descriptors. This slice
  reads schema descriptors but does not mutate catalog rows, descriptor
  versions, descriptor caches, catalog generation, or
  `sqlite_schema_generation`.
- Runtime execution builds a result directly from MyLite-owned static metadata
  and the schema descriptor. It does not query SQLite table metadata,
  `sqlite_schema`, pragma output, trigger metadata, or `INFORMATION_SCHEMA`.
- The result builder owns the 11 result columns and the empty row set.
- Storage/VFS owns the `.mylite` preamble and shifted SQLite payload boundary.
  This introspection does not touch byte range `[0, 4096)`.

## Supported SQL Grammar

Supported subset:

```sql
SHOW TRIGGERS
SHOW TRIGGERS {FROM | IN} identifier
SHOW FULL TRIGGERS
SHOW FULL TRIGGERS {FROM | IN} identifier
SHOW TRIGGERS [schema_clause] LIKE 'pattern'
SHOW FULL TRIGGERS [schema_clause] LIKE 'pattern'
```

`schema_clause` is:

```sql
{FROM | IN} identifier
```

The identifier is a schema name. It is not a table name and cannot be
schema-qualified in this slice.

MyLite Lemon-syntax grammar snippet:

```lemon
statement ::= show_triggers_statement.

show_triggers_statement ::=
    SHOW show_full_opt TRIGGERS show_schema_clause_opt show_like_clause_opt.

show_full_opt ::= .
show_full_opt ::= FULL.

show_schema_clause_opt ::= .
show_schema_clause_opt ::= FROM identifier.
show_schema_clause_opt ::= IN identifier.
```

`TRIGGERS` and `FULL` remain usable as identifiers where identifier grammar
admits nonreserved keywords. They are mapped explicitly only in the admitted
`SHOW TRIGGERS` syntax.

## Schema Resolution

When no explicit schema clause is present, MyLite resolves the current
selected/default schema. If no schema is selected, MyLite reports
`1046` / `3D000` (`No database selected`).

When `FROM schema_name` or `IN schema_name` is present, MyLite resolves that
schema directly. If the schema does not exist, MyLite reports
`1049` / `42000` (`Unknown database`).

Any explicit schema name beginning with MyLite's reserved `_mylite_` prefix is
rejected before descriptor lookup and before any SQLite SQL could be generated.
The diagnostic follows the existing reserved-name policy from the catalog
lifecycle slices.

Current MyLite catalog lookups use the existing descriptor name policy. This
slice does not add new case-folding, collation, or system-schema behavior.

## LIKE Semantics

MySQL applies `LIKE` to trigger table names rather than trigger names. Since
MyLite has no trigger descriptors and emits no rows, `LIKE` has no row-level
effect in this slice. MyLite still parses and decodes `LIKE 'pattern'` so that
unsupported pattern forms and NUL-producing escapes keep the same deterministic
diagnostics as other supported `SHOW LIKE` statements.

Supported patterns are ordinary string literals using the current MyLite
`SHOW LIKE` subset: `%`, `_`, and backslash escaping without NUL-producing
escapes. National string literals, character-set introducers, numeric literals,
`NULL`, parameters, expressions, and functions are unsupported syntax.

## Result Semantics

Successful `SHOW TRIGGERS` appends the standard 11 result columns and zero
rows. The columns are:

| Ordinal | Column |
| --- | --- |
| 1 | `Trigger` |
| 2 | `Event` |
| 3 | `Table` |
| 4 | `Statement` |
| 5 | `Timing` |
| 6 | `Created` |
| 7 | `sql_mode` |
| 8 | `Definer` |
| 9 | `character_set_client` |
| 10 | `collation_connection` |
| 11 | `Database Collation` |

Successful statements return through the existing public result API
conventions for row-producing statements. They have `warning_count == 0`,
`affected_rows == 0`, no result rows, and connection-local `ROW_COUNT()` state
of `-1`.

## Diagnostics

Required diagnostics:

- public API misuse: preserve existing public API behavior;
- syntax errors and unsupported grammar: existing parse diagnostic
  `1064` / `42000`;
- missing selected schema for unqualified `SHOW TRIGGERS`: `1046` / `3D000`;
- unknown explicit schema: `1049` / `42000`;
- reserved `_mylite_*` schema names: existing MyLite reserved-name diagnostic;
- unsupported `WHERE`, `EXTENDED`, `ORDER BY`, `LIMIT`, singular
  `SHOW TRIGGER`, combined `LIKE ... WHERE`, table-qualified schema names,
  unsupported `LIKE` literals, parameters, functions, or expressions:
  deterministic parse or `SHOW LIKE` diagnostics;
- allocation failures: existing out-of-memory diagnostic;
- unexpected catalog or physical SQLite failures: existing runtime diagnostic,
  without exposing SQLite internals as MySQL-compatible behavior.

## Physical SQLite and File Format Policy

This is a MyLite wrapper/translation feature. It uses public SQLite APIs only
through the existing catalog-opening path and does not require SQLite extension
APIs or SQLite fork hooks.

The implementation must not generate user SQLite SQL, inspect SQLite trigger
metadata, create SQLite triggers, create physical indexes, mutate physical
tables, or write catalog rows. It must preserve `.mylite` preamble bytes,
shifted SQLite payload invariants, catalog generation, descriptor cache state,
and `sqlite_schema_generation`.

## Tests

Fast C tests must cover:

- result column shape and zero rows for selected/default schema, `FROM`, and
  `IN` forms;
- accepted `FULL` forms;
- accepted `LIKE` patterns, including wildcard and escaped underscore
  patterns, with zero rows;
- no selected schema, unknown schema, and reserved `_mylite_*` schema
  diagnostics;
- unsupported syntax: `WHERE`, `EXTENDED`, `ORDER BY`, `LIMIT`, singular
  `SHOW TRIGGER`, combined `LIKE ... WHERE`, numeric `LIKE`, `NULL` `LIKE`,
  national-string `LIKE`, introducer `LIKE`, parameters, and functions;
- result API state: row result set present, zero rows, zero warnings, affected
  rows zero, and connection-local `ROW_COUNT()` `-1`;
- schema create/drop/reopen behavior;
- preamble and generation invariants;
- independent file-backed handles.

The MySQL expectation script must verify:

- MySQL 8.4.9 runtime version;
- exact result headers;
- empty-schema zero-row behavior and `@@warning_count`/`ROW_COUNT()` state;
- selected/default schema, `FROM`, `IN`, and `FULL` forms;
- `LIKE` matching table names rather than trigger names;
- case-sensitive `LIKE` behavior when `@@lower_case_table_names == 0`;
- upstream acceptance of `WHERE` and rejection of unsupported wider forms;
- missing default schema and unknown schema diagnostics.

## Compatibility Documentation

`COMPATIBILITY.md` and `docs/compatibility/sql-show-statements.md` must mark
`SHOW TRIGGERS` as limited support for schema-resolved empty trigger
introspection with MySQL 8.4.9 column labels, optional `FULL`, and `LIKE`
filters. They must explicitly say that trigger descriptors, trigger rows,
trigger DDL, `SHOW CREATE TRIGGER`, `WHERE`, privileges, and
`INFORMATION_SCHEMA.TRIGGERS` remain unsupported.

## Review Checklist

- The parser admits only the specified independent grammar subset.
- MySQL 8.4.9 evidence backs every user-visible behavior and documented
  incompatibility.
- Runtime uses schema descriptors and static result metadata only.
- No trigger storage, trigger execution, catalog mutation, SQLite trigger
  metadata, SQLite schema text, or SQLite fork patch is introduced.
- The result columns match MySQL 8.4.9 exactly.
- Diagnostics follow existing MyLite/MySQL-compatible policies.
- Tests cover successful forms, deferred forms, diagnostics, persistence,
  file-format safety, and independent handles.
