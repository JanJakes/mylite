# Baseline SHOW COLUMNS Introspection

## Status

This feature specifies a narrow table-column introspection slice for
`SHOW COLUMNS`, `SHOW FIELDS`, `DESCRIBE`, and `DESC`. It builds on
`mylite_execute()`, statement context, the MyLite parser scaffold, file-backed
`.mylite` opening, durable catalog descriptors, schema/table lifecycle,
integer/`NULL` row values, descriptor-driven reads and writes, and the baseline
catalog foundation.

The feature is intentionally not full MySQL introspection support. It exposes
only descriptor-backed persistent base-table columns through the standard
six-column MySQL result shape. It does not add `FULL`, `EXTENDED`, `LIKE`,
`WHERE`, column-filtered `DESCRIBE`, execution-plan `EXPLAIN`, views, temporary
tables, privileges, indexes, defaults, generated columns, or
`INFORMATION_SCHEMA`.

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
- Baseline schema lifecycle:
  `docs/specs/baseline-schema-lifecycle/specs.md`
- Baseline basic table lifecycle:
  `docs/specs/baseline-basic-table-lifecycle/specs.md`
- Baseline table rename lifecycle:
  `docs/specs/baseline-table-rename-lifecycle/specs.md`
- Baseline row values:
  `docs/specs/baseline-row-values-lifecycle/specs.md`
- Baseline select, delete, update, and count slices:
  `docs/specs/baseline-select-where-lifecycle/specs.md`,
  `docs/specs/baseline-select-order-limit-lifecycle/specs.md`,
  `docs/specs/baseline-delete-lifecycle/specs.md`,
  `docs/specs/baseline-update-lifecycle/specs.md`, and
  `docs/specs/baseline-count-aggregate/specs.md`
- MySQL lexer:
  `docs/specs/mysql-lexer/specs.md`
- MySQL parser scaffold:
  `docs/specs/mysql-parser-scaffold/specs.md`
- SQLite source snapshot notes: `third_party/sqlite/README.md`
- MySQL 8.4 Reference Manual, `SHOW COLUMNS`:
  https://dev.mysql.com/doc/refman/8.4/en/show-columns.html
- MySQL 8.4 Reference Manual, `DESCRIBE`:
  https://dev.mysql.com/doc/refman/8.4/en/describe.html
- MySQL 8.4 Reference Manual, `EXPLAIN`:
  https://dev.mysql.com/doc/refman/8.4/en/explain.html

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior,
public SQLite APIs, and existing MyLite source code. It does not copy MySQL,
MariaDB, Percona, SQLite implementation internals, or other restrictively
licensed implementation sources.

## MySQL 8.4.9 Runtime Observations

Observed against the local `mysql:8.4.9` runtime using TCP:

- `SHOW COLUMNS FROM t`, `SHOW COLUMNS IN t`, `SHOW FIELDS FROM t`,
  `SHOW FIELDS IN t`, `DESCRIBE t`, and `DESC t` return the same table-column
  result for this slice's table shapes.
- `SHOW COLUMNS FROM db.t`, `DESCRIBE db.t`, `DESC db.t`,
  `SHOW COLUMNS FROM t FROM db`, and `SHOW COLUMNS FROM t IN db` resolve an
  explicit schema and do not require a selected default schema.
- When both the table token and trailing clause name a schema, as in
  `SHOW COLUMNS FROM db1.t FROM db2`, the trailing schema determines the schema
  and the rightmost table identifier determines the table name.
- Unqualified `SHOW COLUMNS FROM t` and unqualified `DESCRIBE t` fail with
  error `1046`, SQLSTATE `3D000`, when no default schema is selected.
- Unknown explicit schemas fail with error `1049`, SQLSTATE `42000`.
- Unknown tables in a known schema fail with error `1146`, SQLSTATE `42S02`,
  and the message includes the resolved `schema.table` name.
- The standard non-`FULL` result columns are exactly `Field`, `Type`, `Null`,
  `Key`, `Default`, and `Extra`.
- For `INT`, `INTEGER`, `INT UNSIGNED`, `BIGINT`, and `BIGINT UNSIGNED`, MySQL
  reports type strings `int`, `int unsigned`, `bigint`, and
  `bigint unsigned`.
- `Null` is `YES` for nullable columns and `NO` for `NOT NULL` columns.
- With no indexes or explicit defaults, `Key` and `Extra` are empty strings and
  `Default` is SQL `NULL`, including for `NOT NULL` integer columns that have
  no explicit default.
- Successful table-column introspection returns a result set, leaves warning
  count `0`, and makes the following `ROW_COUNT()` return `-1`.
- MySQL accepts `SHOW FULL COLUMNS`, `SHOW EXTENDED COLUMNS`, `SHOW COLUMNS
  ... LIKE`, `SHOW COLUMNS ... WHERE`, `DESCRIBE t column_name`, and
  `DESCRIBE SELECT ...`; those forms remain outside this slice.

## Scope

The implementation must add:

- parser and AST support for `SHOW COLUMNS`, `SHOW FIELDS`, `DESCRIBE`, and
  `DESC` table-column introspection;
- `FROM` and `IN` synonyms in the admitted `SHOW COLUMNS`/`SHOW FIELDS` forms;
- unqualified and schema-qualified table-name resolution using the existing
  selected/default schema policy;
- optional explicit schema resolution for `SHOW COLUMNS FROM table FROM schema`
  and the equivalent `IN` variants;
- descriptor-driven result construction from the MyLite catalog column
  descriptors for one persistent base table;
- MySQL-compatible six-column result shape for the descriptor integer-family
  columns currently creatable by MyLite;
- deterministic diagnostics for unsupported syntax and unresolved names;
- result-set warning and row-count behavior matching existing MyLite result
  conventions and observed MySQL 8.4.9 behavior;
- fast C tests and a MySQL 8.4.9 expectation artifact for supported behavior
  and deliberately deferred wider forms.

## Non-Goals

This feature must not implement:

- `SHOW FULL COLUMNS`, `SHOW EXTENDED COLUMNS`, `LIKE`, `WHERE`, hidden
  physical columns, column privileges, comments, collations, generated
  invisible primary keys, or `INFORMATION_SCHEMA.COLUMNS`;
- `DESCRIBE table column_name`, wildcard/pattern `DESCRIBE`, `DESCRIBE SELECT`,
  `EXPLAIN`, execution plans, `EXPLAIN ANALYZE`, `FORMAT`, `FOR CONNECTION`, or
  `FOR SCHEMA`;
- temporary tables, views, indexes, primary keys, unique keys, foreign keys,
  triggers, generated columns, explicit defaults, auto-increment metadata,
  privileges, SQL modes, arbitrary SQLite metadata reads, arbitrary SQLite SQL
  pass-through, or SQLite fork patches.

## Ownership Boundary

- The public API remains unchanged. `mylite_execute()` owns public validation,
  result-handle ownership, statement-boundary row-count state, and failure
  cleanup.
- Statement context owns diagnostics reset, warning count, and statement
  completion. Successful introspection statements are result-set statements and
  therefore store `-1` as the connection-local previous row count.
- Lexer/parser/AST own syntax admission and source spans. They remain
  independent of runtime, catalog, storage, and SQLite.
- Analyzer/planner code resolves the target schema/table and rejects unsupported
  object kinds using MyLite descriptors.
- The catalog module remains authoritative for schema/table/column descriptors.
  This slice reads descriptors but does not mutate catalog rows, descriptor
  versions, descriptor caches, catalog generation, or
  `sqlite_schema_generation`.
- Runtime execution builds a result directly from MyLite catalog descriptors.
  It does not query SQLite table metadata, `sqlite_schema`, or pragma output.
- The result builder owns the six text columns and SQL `NULL` default values.
- Storage/VFS owns the `.mylite` preamble and shifted SQLite payload boundary.
  Introspection reads catalog rows only and does not touch byte range
  `[0, 4096)`.

## Supported SQL Grammar

Supported subset:

```sql
SHOW {COLUMNS | FIELDS} {FROM | IN} table_name
SHOW {COLUMNS | FIELDS} {FROM | IN} table_name {FROM | IN} identifier
DESCRIBE table_name
DESC table_name
```

`table_name` uses the existing table lifecycle subset:

```sql
table_name:
    identifier
  | identifier.identifier
```

The second `SHOW` form is the explicit-schema form. The trailing identifier is
the schema name. If the first table name is qualified, the trailing schema
still determines the schema and the rightmost identifier in the first table
name determines the table name.

MyLite Lemon-syntax grammar snippets:

```lemon
statement ::= show_columns_statement.
statement ::= describe_table_statement.

show_columns_statement ::=
    SHOW show_columns_keyword show_table_name_keyword table_name.
show_columns_statement ::=
    SHOW show_columns_keyword show_table_name_keyword table_name
    show_schema_name_keyword identifier.

show_columns_keyword ::= COLUMNS.
show_columns_keyword ::= FIELDS.

show_table_name_keyword ::= FROM.
show_table_name_keyword ::= IN.
show_schema_name_keyword ::= FROM.
show_schema_name_keyword ::= IN.

describe_table_statement ::= DESCRIBE table_name.
describe_table_statement ::= DESC table_name.
```

`COLUMNS` and `FIELDS` remain ordinary identifiers where identifier grammar
admits nonreserved keywords. `DESCRIBE` and `DESC` are statement keywords in
this slice; `DESC` remains usable as the existing order-direction keyword.

## Schema and Table Resolution

Unqualified table names resolve through the current selected/default schema.
If no schema is selected, MyLite reports `1046` / `3D000` (`No database
selected`).

Schema-qualified table names and explicit `SHOW ... FROM table FROM schema`
forms resolve the named schema directly. When both are present, the trailing
explicit schema wins and any qualifier on the table name is ignored for schema
resolution. If the schema does not exist, MyLite reports `1049` / `42000`
(`Unknown database`). If the table does not exist in a known schema, MyLite
reports `1146` / `42S02` (`Table 'schema.table' doesn't exist`).

Any schema or table name beginning with MyLite's reserved `_mylite_` prefix is
rejected before descriptor lookup and before any SQLite SQL could be generated.
The diagnostic follows the existing reserved-name policy from the catalog
lifecycle slices. Once non-base-table descriptors exist, this slice must reject
them with a deterministic unsupported-object diagnostic.

Descriptor lookup uses the current catalog name comparison behavior. This slice
does not add MySQL filesystem-dependent table-name case folding or collation
semantics.

## Result Semantics

Successful introspection returns one result row per descriptor column ordered by
catalog `ordinal_position`. The result columns are:

| Column | MyLite value for current descriptors |
| --- | --- |
| `Field` | logical column name |
| `Type` | `int`, `int unsigned`, `bigint`, or `bigint unsigned` |
| `Null` | `YES` for nullable columns, `NO` otherwise |
| `Key` | empty string |
| `Default` | SQL `NULL` |
| `Extra` | empty string |

`INTEGER` is reported as `int`, matching MySQL's observed display for the
supported integer family. MyLite currently stores `INT`, `INTEGER`, `BIGINT`,
and their supported `UNSIGNED` forms as descriptor logical types; this slice
maps only those logical types. Unknown future descriptor types must produce a
deterministic unsupported diagnostic until their display strings are specified.

Successful statements:

- return a result set and no non-query row payload;
- leave `affected_rows == 0` under existing MyLite result conventions;
- use `warning_count == 0`;
- make following `ROW_COUNT()` return `-1`.

## Physical SQLite Handling

No user SQL is generated for SQLite in this slice. Runtime reads MyLite-owned
catalog descriptors through the catalog module and builds the result directly.
It does not depend on SQLite column metadata, SQLite table schema text,
`PRAGMA table_info`, or optional SQLite features.

The feature does not create, drop, rename, or modify physical user tables. It
does not mutate the MyLite catalog, descriptor versions, descriptor caches,
catalog generation, `sqlite_schema_generation`, the file preamble, or the VFS
offset mapping.

## Diagnostics

Diagnostics must be deterministic:

- syntax errors and unsupported grammar use the existing parse error behavior;
- missing default schema uses `1046` / `3D000`;
- unknown explicit schema uses `1049` / `42000`;
- unknown table uses `1146` / `42S02`;
- reserved `_mylite_*` schema/table names use the existing reserved-name
  diagnostics;
- unsupported object kind reports that the statement supports only persistent
  base tables;
- unsupported descriptor logical type reports an unsupported column type for
  `SHOW COLUMNS`;
- catalog iteration failure reports a runtime error for building the
  introspection result;
- allocation failure reports the existing out-of-memory diagnostic;
- public API misuse remains governed by the existing public execution and
  result-handle validation.

Unsupported but MySQL-accepted wider forms are intentionally rejected by the
parser for this slice where possible. This includes `FULL`, `EXTENDED`, `LIKE`,
`WHERE`, column-filtered `DESCRIBE`, `DESCRIBE SELECT`, `EXPLAIN`, execution
plans, and `INFORMATION_SCHEMA`.

## Tests

The C tests must cover:

- `SHOW COLUMNS`, `SHOW FIELDS`, `DESCRIBE`, and `DESC`;
- `FROM` and `IN` synonyms;
- unqualified, schema-qualified, and explicit-schema `SHOW` target resolution;
- the six-column result shape and descriptor rows for `INT`, `INTEGER`,
  `INT UNSIGNED`, `BIGINT`, and `BIGINT UNSIGNED`, including nullable and
  `NOT NULL` columns;
- SQL `NULL` default values, empty `Key` and `Extra`, warning count,
  affected-row convention, absence of non-query row payload, and following
  `ROW_COUNT()`;
- missing default schema, unknown schema, unknown table, and reserved target
  names;
- unsupported syntax including `FULL`, `EXTENDED`, `LIKE`, `WHERE`,
  column-filtered `DESCRIBE`, `DESCRIBE SELECT`, and `EXPLAIN`;
- reopen persistence, rename behavior, drop behavior, independent file-backed
  handles, and preamble preservation;
- zero-initialized cleanup for new statement/result paths.

The MySQL expectation artifact must verify every user-visible SQL behavior this
slice introduces against a MySQL 8.4.9 runtime. If that runtime is unavailable,
changing compatibility expectations is blocked.
