# Baseline SHOW FULL TABLES

## Status

This feature extends the existing descriptor-driven `SHOW TABLES` slice with
the optional `FULL` modifier for persistent MyLite base-table descriptors. It
does not add `EXTENDED`, `WHERE`, view descriptors, privilege filtering,
temporary-table rows, or new catalog storage.

## Sources

- MyLite README architecture: `README.md`
- MyLite engineering standards:
  `docs/architecture/engineering-standards.md`
- Baseline implementation strategy:
  `docs/specs/baseline-implementation-strategy/specs.md`
- Baseline catalog, schema, table, `SHOW TABLES`, and `SHOW LIKE` slices:
  `docs/specs/baseline-catalog-foundation/specs.md`,
  `docs/specs/baseline-schema-lifecycle/specs.md`,
  `docs/specs/baseline-basic-table-lifecycle/specs.md`, and
  `docs/specs/baseline-show-like-filters/specs.md`
- MySQL parser scaffold: `docs/specs/mysql-parser-scaffold/specs.md`
- SQLite source snapshot notes: `third_party/sqlite/README.md`
- MySQL 8.4 Reference Manual, `SHOW` statements:
  https://dev.mysql.com/doc/refman/8.4/en/show.html
- MySQL 8.4 Reference Manual, `SHOW TABLES`:
  https://dev.mysql.com/doc/refman/8.4/en/show-tables.html

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior,
public SQLite APIs, and existing MyLite source code. It does not copy MySQL,
MariaDB, Percona, SQLite implementation internals, or other restrictively
licensed implementation sources.

## MySQL 8.4.9 Runtime Observations

Observed against `mysql:8.4.9` with `@@lower_case_table_names = 0`:

- `SHOW FULL TABLES`, `SHOW FULL TABLES FROM db`, and
  `SHOW FULL TABLES IN db` return two columns.
- The first column name is the existing `SHOW TABLES` name:
  `Tables_in_<db>` or `Tables_in_<db> (<decoded pattern>)` when a `LIKE`
  filter is present.
- The second column name is `Table_type`.
- Ordinary base tables report `BASE TABLE`.
- MySQL also reports `VIEW` for views and `SYSTEM VIEW` for
  `INFORMATION_SCHEMA` system views. MyLite defers these object kinds until
  those descriptor families exist.
- Non-temporary tables are listed. Temporary tables do not appear in
  `SHOW FULL TABLES`.
- Existing `LIKE` behavior is unchanged: the pattern filters table names,
  table-name matching is case-sensitive under `lower_case_table_names = 0`,
  and unsupported non-string pattern forms are syntax errors.
- Missing default schema remains `1046 / 3D000`; unknown explicit schemas
  remain `1049 / 42000`.
- Successful statements return result sets, leave `@@warning_count == 0` and
  `@@error_count == 0`, and make the following `ROW_COUNT()` return `-1`.

## Scope

The implementation must add:

- parser support for `SHOW FULL TABLES` with the existing optional
  `FROM`/`IN` schema and `LIKE 'pattern'` clauses;
- a statement-level AST bit that records whether the `FULL` modifier was
  present without changing the public ABI;
- descriptor-driven result construction for persistent base-table descriptors;
- MySQL-shaped result metadata: `Tables_in_<schema>` and `Table_type`;
- `BASE TABLE` values for the admitted descriptor kind;
- preservation of existing schema resolution, reserved-name diagnostics,
  `LIKE` decoding/matching, result statement row-count semantics, and cleanup
  on failure.

## Non-Goals

This feature must not implement:

- `SHOW EXTENDED TABLES` or `SHOW EXTENDED FULL TABLES`;
- `SHOW TABLES ... WHERE`;
- view descriptors, `SYSTEM VIEW`, `TEMPORARY`, privilege filtering, hidden
  failed-ALTER tables, `INFORMATION_SCHEMA.TABLES` changes, or broader
  metadata object kinds;
- NUL-producing `LIKE` pattern escapes beyond the existing MyLite policy;
- arbitrary SQLite metadata reads, arbitrary SQLite SQL pass-through, or
  SQLite fork patches.

## Ownership Boundary

- Public API: unchanged. `mylite_execute()` continues to return a normal
  metadata result set for successful statements.
- Statement context: successful `SHOW FULL TABLES` is result-producing, stores
  affected rows `0`, warning count `0`, and previous row count `-1`.
- Lexer/parser/AST: admit only the `FULL` modifier before `TABLES` and store
  the flag on the statement node. The parser does not resolve schemas or
  catalog descriptors.
- Runtime: resolve the selected or explicit schema, build result columns,
  apply the existing `SHOW LIKE` filter, and append descriptor-derived rows.
- Catalog: descriptors remain authoritative. This feature reads table
  descriptors and their object kind only; it does not mutate catalog rows,
  descriptor versions, descriptor caches, catalog generation, or
  `sqlite_schema_generation`.
- Result builder: owns the MySQL-visible column names and row values.
- Storage/VFS: unchanged. The `.mylite` preamble and shifted SQLite payload are
  not read or written by this metadata-only feature.
- SQLite: no generated SQLite SQL, custom SQLite function, or fork hook is
  required.

## Supported SQL Grammar

Supported subset:

```sql
SHOW [FULL] TABLES [{FROM | IN} schema_name] [LIKE 'pattern']
```

`schema_name` uses the existing identifier subset. The pattern must be a
single regular string literal token accepted by the existing `SHOW LIKE`
subset.

MyLite Lemon-syntax grammar snippets:

```lemon
show_tables_statement ::=
    SHOW show_full_opt TABLES show_schema_clause_opt show_like_clause_opt.

show_full_opt ::= .
show_full_opt ::= FULL.
```

The snippets describe MyLite's intended grammar and are not MySQL grammar
text.

## Semantics

`SHOW TABLES` without `FULL` keeps the existing one-column result shape.

`SHOW FULL TABLES` uses the same schema resolution and filtering path, then
emits two columns:

1. `Tables_in_<schema>` or `Tables_in_<schema> (<pattern>)`;
2. `Table_type`.

Each current persistent base-table descriptor that passes the optional `LIKE`
filter contributes one row:

```text
<table_name>    BASE TABLE
```

Descriptors for object kinds outside the current base-table surface are
skipped until those object kinds have independent specs and tests. Temporary
table descriptors remain omitted, matching current `SHOW TABLES` behavior and
MySQL's non-temporary `SHOW TABLES` surface.

## Diagnostics

This feature preserves existing diagnostics:

- missing default schema: `1046 / 3D000`;
- unknown explicit schema: `1049 / 42000`;
- reserved `_mylite_*` explicit schema names: existing reserved-name error;
- unsupported non-string `LIKE` operands: parse errors;
- NUL-producing decoded `LIKE` patterns: existing deterministic unsupported
  diagnostic;
- `WHERE`, `EXTENDED`, and other unsupported syntax: parse errors or existing
  unsupported diagnostics;
- allocation failures: existing out-of-memory diagnostics.

Successful supported statements return a result set, warning count `0`,
affected rows `0`, and no catalog or file-format mutation.

## Tests

Tests must cover:

- parser acceptance for `SHOW FULL TABLES`, explicit `FROM`/`IN`, and `LIKE`;
- parser rejection for deferred `SHOW EXTENDED FULL TABLES`,
  `SHOW FULL TABLES WHERE ...`, and wrong modifier order;
- MySQL 8.4.9 expectation script for headers, rows, `BASE TABLE`, `LIKE`
  headers, no-match results, missing default schema, unknown schema, successful
  warning/error counts, and following `ROW_COUNT()`;
- C runtime coverage for selected schema, explicit schema, `LIKE`, no-match,
  row-count/warning behavior, reopen persistence, rename/drop behavior where
  existing `SHOW TABLES` tests already exercise the same descriptor list, and
  independent handle behavior;
- regression coverage that ordinary `SHOW TABLES` keeps its one-column result.

## Compatibility

`SHOW TABLES` remains partial. This feature adds the `FULL` modifier for
currently supported persistent base-table descriptors only. Views, system
views, temporary table rows, privilege filtering, `EXTENDED`, and `WHERE` stay
documented as unsupported.
