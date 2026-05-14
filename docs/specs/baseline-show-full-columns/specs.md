# Baseline SHOW FULL COLUMNS

## Status

This feature extends the existing descriptor-driven `SHOW COLUMNS` /
`SHOW FIELDS` slice with the `FULL` modifier. It builds on MyLite-owned
catalog descriptors, current table/schema resolution, the parser scaffold,
session temporary-table shadowing, descriptor type metadata, current
`SHOW COLUMNS` result construction, and limited charset/collation metadata.

The slice is intentionally narrow. It supports `SHOW FULL COLUMNS` and
`SHOW FULL FIELDS` for the same persistent base tables and shadowing temporary
tables currently admitted by ordinary `SHOW COLUMNS`. It does not add
`EXTENDED`, `WHERE`, column-filtered `DESCRIBE`, execution-plan `EXPLAIN`,
views, hidden MySQL storage columns, privileges beyond MyLite's fixed embedded
placeholder, column comments, generated columns, or new `INFORMATION_SCHEMA`
surfaces.

## Sources

- MyLite README architecture: `README.md`
- MyLite engineering standards:
  `docs/architecture/engineering-standards.md`
- Existing SHOW COLUMNS feature:
  `docs/specs/baseline-show-columns-introspection/specs.md`
- SHOW LIKE filters:
  `docs/specs/baseline-show-like-filters/specs.md`
- Character set and collation support:
  `docs/specs/baseline-show-character-set-collation/specs.md`
- Temporary tables:
  `docs/specs/baseline-temporary-table-lifecycle/specs.md`
- MySQL parser scaffold:
  `docs/specs/mysql-parser-scaffold/specs.md`
- SQLite source snapshot notes: `third_party/sqlite/README.md`
- MySQL 8.4 Reference Manual, `SHOW COLUMNS`:
  https://dev.mysql.com/doc/refman/8.4/en/show-columns.html
- MySQL 8.4 Reference Manual, `SHOW` statements:
  https://dev.mysql.com/doc/refman/8.4/en/show.html

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior,
public SQLite APIs, and existing MyLite source code. It does not copy MySQL,
MariaDB, Percona, SQLite implementation internals, or restrictively licensed
implementation sources.

## MySQL 8.4.9 Runtime Observations

Observed against the local `mysql:8.4.9` runtime using TCP:

- `SHOW FULL COLUMNS FROM t` and `SHOW FULL FIELDS FROM t` return the same
  nine-column result shape for this slice's table shapes.
- `SHOW FULL COLUMNS FROM db.t`, `SHOW FULL COLUMNS FROM t FROM db`, and
  equivalent `IN` forms use the same schema-resolution behavior as ordinary
  `SHOW COLUMNS`; when both the table token and trailing clause name schemas,
  the trailing schema wins.
- The result columns are exactly `Field`, `Type`, `Collation`, `Null`, `Key`,
  `Default`, `Extra`, `Privileges`, and `Comment`.
- `Collation` is the column collation for nonbinary character string
  descriptors. For the currently verified default MySQL table charset, `CHAR`,
  `VARCHAR`, `TEXT`, `ENUM`, and `SET` report `utf8mb4_0900_ai_ci`.
- Numeric, temporal, `JSON`, binary string, and `BIT` columns report SQL `NULL`
  in `Collation`.
- The `Privileges` value for the root test user is
  `select,insert,update,references` for every visible user column in this
  slice.
- `Comment` is the column comment text. Tables without column comments report
  an empty string.
- `LIKE` filters use the same column-name pattern behavior as ordinary
  `SHOW COLUMNS`.
- Successful `SHOW FULL COLUMNS` returns a result set, leaves warning count
  `0`, and makes the following `ROW_COUNT()` return `-1`.
- `SHOW EXTENDED FULL COLUMNS` is accepted by MySQL and includes hidden InnoDB
  columns. MyLite keeps `EXTENDED` unsupported in this phase.

## Scope

The implementation must add:

- parser and AST support for `SHOW FULL COLUMNS` and `SHOW FULL FIELDS`;
- `FROM` and `IN` synonyms in the same table and schema positions already
  supported by ordinary `SHOW COLUMNS`;
- `LIKE 'pattern'` filters using the existing SHOW LIKE implementation;
- unqualified, schema-qualified, and explicit-schema target resolution through
  the existing selected/default schema policy;
- descriptor-driven full result rows for persistent base tables and shadowing
  temporary tables;
- nine-column MySQL result shape with descriptor-owned `Type`, `Null`, `Key`,
  `Default`, and `Extra`, added `Collation`, fixed embedded `Privileges`, and
  empty `Comment`;
- deterministic diagnostics for unsupported syntax and unresolved names;
- MySQL-compatible warning count and row-count behavior for successful
  result-set statements;
- fast C tests and a MySQL 8.4.9 expectation artifact.

## Non-Goals

This feature must not implement:

- `SHOW EXTENDED COLUMNS`, `SHOW EXTENDED FULL COLUMNS`, hidden physical or
  storage-engine columns, generated invisible primary keys, or internal MySQL
  columns;
- `SHOW COLUMNS ... WHERE`, column-filtered `DESCRIBE`, wildcard
  `DESCRIBE`, `DESCRIBE SELECT`, execution-plan `EXPLAIN`, `EXPLAIN ANALYZE`,
  `FORMAT`, or `FOR CONNECTION`;
- views, trigger metadata, privilege filtering, mutable privilege rows, column
  comments, generated columns, per-column charset/collation clauses, non-utf8
  character sets, or full `INFORMATION_SCHEMA` parity;
- arbitrary SQLite metadata reads, SQLite pragma introspection, arbitrary
  SQLite SQL pass-through, storage-format changes, or SQLite fork patches.

## Ownership Boundary

- The public API remains unchanged. `mylite_execute()` owns public validation,
  result-handle ownership, statement-boundary row-count state, and cleanup.
- Statement context owns diagnostics reset and statement completion. Successful
  `SHOW FULL COLUMNS` is a result-set statement, so the previous row-count
  state is `-1` and warning count is `0`.
- Lexer/parser/AST own syntax admission and source spans. A distinct AST kind
  records the `FULL` modifier without leaking runtime state into parsing.
- Analyzer/planner code reuses existing table-name resolution and object-kind
  rejection. It must reject reserved `_mylite_*` target names before SQLite SQL
  generation; this statement does not generate SQLite SQL.
- The catalog module remains authoritative for schema/table/column/index
  descriptors. This slice reads descriptors only and does not mutate catalog
  rows, descriptor versions, descriptor caches, catalog generation, or
  `sqlite_schema_generation`.
- Runtime execution builds rows directly from MyLite descriptors. It does not
  query SQLite table metadata, `sqlite_schema`, or pragma output.
- Storage/VFS owns the `.mylite` preamble and shifted SQLite payload boundary.
  This introspection slice reads catalog rows only and must not touch file bytes
  outside ordinary catalog reads.

## Supported SQL Grammar

Supported subset:

```sql
SHOW FULL {COLUMNS | FIELDS} {FROM | IN} table_name
SHOW FULL {COLUMNS | FIELDS} {FROM | IN} table_name {FROM | IN} identifier
SHOW FULL {COLUMNS | FIELDS} {FROM | IN} table_name LIKE string_literal
SHOW FULL {COLUMNS | FIELDS} {FROM | IN} table_name {FROM | IN} identifier LIKE string_literal
```

`table_name` uses the existing table-name subset:

```sql
table_name:
    identifier
  | identifier.identifier
```

MyLite Lemon-syntax snippets:

```lemon
show_columns_statement ::=
    SHOW FULL show_columns_keyword show_table_name_keyword table_name show_like_clause_opt.
show_columns_statement ::=
    SHOW FULL show_columns_keyword show_table_name_keyword table_name
    show_schema_name_keyword identifier show_like_clause_opt.

show_columns_keyword ::= COLUMNS.
show_columns_keyword ::= FIELDS.
show_table_name_keyword ::= FROM.
show_table_name_keyword ::= IN.
show_schema_name_keyword ::= FROM.
show_schema_name_keyword ::= IN.
```

`FULL`, `COLUMNS`, and `FIELDS` remain usable as identifiers where the existing
identifier grammar admits them. `DESC` / `DESCRIBE` remain aliases for the
ordinary six-column table introspection subset and are not extended by this
feature.

## Resolution

Unqualified table names resolve through the current selected/default schema.
If no schema is selected, MyLite reports `1046` / `3D000` (`No database
selected`).

Schema-qualified table names and explicit `SHOW ... FROM table FROM schema`
forms resolve the named schema directly. When both are present, the trailing
explicit schema wins and any qualifier on the table name is ignored for schema
resolution. Unknown explicit schemas report `1049` / `42000`. Unknown tables
in a known schema report `1146` / `42S02` with the resolved `schema.table`
name. Unsupported object kinds must be rejected once descriptors for those
kinds exist.

Descriptor lookup uses MyLite's current identifier matching policy. This phase
does not add new collation or case-folding behavior for catalog names.

## Result Mapping

Ordinary `SHOW COLUMNS` keeps the existing six columns. `SHOW FULL COLUMNS`
returns these nine columns:

| Column | MyLite value |
| --- | --- |
| `Field` | Descriptor column name |
| `Type` | Existing descriptor display type text |
| `Collation` | The table descriptor's current default collation for `CHAR`, `VARCHAR`, `TEXT`, `ENUM`, and `SET`; SQL `NULL` for all other supported descriptors |
| `Null` | `YES` for nullable descriptors, otherwise `NO` |
| `Key` | Existing descriptor key marker: `PRI`, `UNI`, `MUL`, or empty string |
| `Default` | Existing descriptor default display value or SQL `NULL` |
| `Extra` | Existing descriptor extra text |
| `Privileges` | Fixed embedded placeholder `select,insert,update,references` |
| `Comment` | Empty string |

The collation value intentionally mirrors the current descriptor model. MyLite
has table-level utf8mb4 collation metadata but no per-column charset/collation
clauses yet. Once per-column collations exist, `SHOW FULL COLUMNS` must be
updated to read the column descriptor rather than the table default.

`LIKE` filters apply to descriptor column names before row construction and use
the same limitations as ordinary `SHOW COLUMNS`, including no NUL-producing
pattern escapes.

## Physical Storage

This feature is a MyLite wrapper/metadata feature. It uses public SQLite APIs
only for catalog reads already used by `SHOW COLUMNS`; it does not add a SQLite
function, virtual table, SQL rewrite, storage mutation, or SQLite fork patch.

## Diagnostics

The implementation must preserve existing diagnostics for:

- syntax errors and unsupported `WHERE` / `EXTENDED` / column-filtered
  `DESCRIBE` forms;
- missing default schema (`1046` / `3D000`);
- unknown schema (`1049` / `42000`);
- unknown table (`1146` / `42S02`);
- reserved `_mylite_*` schema/table names;
- unsupported object kinds;
- unsupported descriptor logical types;
- allocation failures and public API misuse through existing result and execute
  conventions.

Successful supported statements emit no warnings.

## Tests

Tests must cover:

- parser acceptance for `SHOW FULL COLUMNS` and `SHOW FULL FIELDS` with
  `FROM`/`IN`, schema-qualified targets, explicit trailing schemas, and `LIKE`;
- ordinary `SHOW COLUMNS` and table-only `DESCRIBE` remaining unchanged;
- nine-column result labels, collation, privileges, comment, defaults, key
  markers, extra text, warning count, and row-count behavior;
- integer, decimal, approximate, year, date, time, datetime, timestamp, string,
  text, binary string, bit, enum, set, JSON, invisible, auto-increment, and
  current-timestamp descriptors where already supported;
- temporary-table shadowing, reopen persistence for persistent descriptors,
  table rename/drop effects, independent handles, and preamble safety;
- missing default schema, unknown schema, unknown table, reserved target names,
  unsupported `WHERE`, and unsupported `EXTENDED`;
- compatibility expectation generation/comparison against MySQL 8.4.9.

## Compatibility Documentation

Update `COMPATIBILITY.md` and `docs/compatibility/sql-show-statements.md` to
state that `FULL` is supported only for the current `SHOW COLUMNS` /
`SHOW FIELDS` descriptor subset, with fixed embedded privileges and empty
comments. Do not claim `EXTENDED`, `WHERE`, views, privilege filtering, hidden
columns, generated columns, column comments, or full `INFORMATION_SCHEMA`
parity.
