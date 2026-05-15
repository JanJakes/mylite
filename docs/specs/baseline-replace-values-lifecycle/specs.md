# Baseline Replace Values Lifecycle

## Status

This feature specifies a narrow `REPLACE ... VALUES` DML slice for persistent
base tables. It builds on `mylite_execute()`, statement context, parser
scaffolding, shifted `.mylite` storage, durable catalog descriptors,
create/drop/rename table lifecycle, row-value inserts, descriptor-driven
selects, single-table deletes, and single-table updates.

This historical baseline specified the initial no-key `REPLACE ... VALUES`
slice. Key-bearing duplicate-key delete-insert behavior for the current
primary-key and unique-index descriptor subset is specified separately in
`docs/specs/baseline-replace-key-lifecycle/specs.md`.

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
- File-backed MyLite opening VFS:
  `docs/specs/file-backed-mylite-opening-vfs/specs.md`
- MyLite file-format preamble:
  `docs/specs/mylite-file-format/specs.md`
- Baseline catalog foundation:
  `docs/specs/baseline-catalog-foundation/specs.md`
- Baseline basic table lifecycle:
  `docs/specs/baseline-basic-table-lifecycle/specs.md`
- Baseline table rename lifecycle:
  `docs/specs/baseline-table-rename-lifecycle/specs.md`
- Baseline row values lifecycle:
  `docs/specs/baseline-row-values-lifecycle/specs.md`
- Baseline insert set lifecycle:
  `docs/specs/baseline-insert-set-lifecycle/specs.md`
- Baseline update lifecycle:
  `docs/specs/baseline-update-lifecycle/specs.md`
- MySQL lexer:
  `docs/specs/mysql-lexer/specs.md`
- MySQL parser scaffold:
  `docs/specs/mysql-parser-scaffold/specs.md`
- SQLite source snapshot notes: `third_party/sqlite/README.md`
- MySQL 8.4 Reference Manual, `REPLACE`:
  https://dev.mysql.com/doc/refman/8.4/en/replace.html
- MySQL 8.4 Reference Manual, `INSERT`:
  https://dev.mysql.com/doc/refman/8.4/en/insert.html
- MySQL 8.4 Reference Manual, integer type ranges:
  https://dev.mysql.com/doc/refman/8.4/en/integer-types.html
- MySQL 8.4 Reference Manual, out-of-range handling:
  https://dev.mysql.com/doc/refman/8.4/en/out-of-range-and-overflow.html
- MySQL 8.4 Reference Manual, data type defaults:
  https://dev.mysql.com/doc/refman/8.4/en/data-type-defaults.html
- MySQL 8.4 Reference Manual, `ROW_COUNT()`:
  https://dev.mysql.com/doc/refman/8.4/en/information-functions.html

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior, public
SQLite APIs, and existing MyLite source code. It does not copy MySQL, MariaDB,
Percona, SQLite implementation internals, or other restrictively licensed
implementation sources.

## MySQL 8.4.9 Observations

The expectation script
`packages/libmylite/tests/mysql_baseline_replace_values_lifecycle_expectations.sh`
records the runtime probes for this feature. Observed behavior:

- `REPLACE INTO t VALUES (...)` and `REPLACE t VALUES (...)` both succeed.
- `REPLACE` without a selected default schema fails with error `1046`,
  SQLSTATE `3D000`, and message `No database selected`.
- Schema-qualified targets work without a selected default schema.
- Unknown schemas fail with error `1049`, SQLSTATE `42000`; unknown tables
  fail with error `1146`, SQLSTATE `42S02`.
- On tables without a primary key or unique index, repeated `REPLACE` values
  do not delete matching-looking rows; each row is inserted and the affected
  row count is one per inserted row.
- On a table with a primary key, replacing an existing key deletes the old row
  and inserts the new row. A single-row replacement in that case reports two
  affected rows.
- Successful no-key replaces report `ROW_COUNT()` equal to the number of
  inserted rows and `@@warning_count` equal to `0`.
- Omitted nullable columns receive `NULL`; omitted `NOT NULL` columns without
  an explicit default fail with error `1364`, SQLSTATE `HY000`.
- Duplicate column-list targets fail with error `1110`, SQLSTATE `42000`.
- Unknown column-list targets fail with error `1054`, SQLSTATE `42S22`.
- Assigning `NULL` to a `NOT NULL` column fails with error `1048`, SQLSTATE
  `23000`.
- Strict-mode integer out-of-range values fail with error `1264`, SQLSTATE
  `22003`.
- MySQL accepts wider forms that this slice defers, including the `VALUE`
  synonym, row constructors, `SET` form, `SELECT` form, `LOW_PRIORITY`, and
  `DELAYED`. In MySQL 8.4, `DELAYED` is accepted with a warning and handled as
  an ordinary non-delayed replace.

## Scope

The implementation must add:

- parser and AST support for a limited `REPLACE [INTO] table_name
  insert_column_list_opt VALUES insert_row_list` subset;
- reuse of the existing row-value insert row list and value literal grammar;
- single-row and multi-row value lists with statement-level all-or-nothing
  behavior;
- unqualified and schema-qualified target table resolution using the existing
  selected/default schema policy;
- persistent MyLite base-table descriptors only;
- descriptor-driven target column resolution, duplicate target detection, and
  omitted-column handling;
- supported value conversion for decimal integer literals with optional unary
  sign, `TRUE`, `FALSE`, and `NULL`, before SQLite binding;
- range checks for currently supported integer-family descriptors within
  MyLite's current signed 64-bit physical integer range;
- MySQL-shaped diagnostics for schema/table/column/nullability/count/range
  errors, plus deterministic MyLite diagnostics for unsupported grammar and
  values;
- affected-row reporting equal to the inserted row count for the no-key
  baseline case;
- `warning_count == 0` for supported in-range replaces;
- tests and MySQL 8.4.9 expectation artifacts for supported behavior and
  deliberately rejected wider MySQL forms.

## Non-Goals

This feature must not implement:

- primary-key or unique-key descriptors, duplicate-key lookup, delete-before-
  insert replacement, replacement affected-row counts greater than inserted
  row count, cascades, triggers, or foreign keys;
- `REPLACE ... SET`, `REPLACE ... SELECT`, `REPLACE ... TABLE`,
  `LOW_PRIORITY`, `DELAYED`, `PARTITION`, `VALUE` synonym, row constructors,
  aliases, row aliases, `RETURNING`, or arbitrary SQLite SQL pass-through;
- table-qualified column-list names, expression values, column-to-column
  values, arithmetic values, `DEFAULT` DML keyword values, `DEFAULT(col)`,
  parameters, variables, functions, subqueries, string/decimal/float/hex/bit/
  date/time/json values, or general expression evaluation;
- auto-increment, `LAST_INSERT_ID()` changes, generated columns, check
  constraints, indexes, temporary tables, views, privileges, warning demotion,
  non-strict SQL modes, or SQLite fork patches.

## Ownership Boundary

- The public API remains unchanged. `mylite_execute()` owns call validation,
  statement dispatch, result-handle ownership, public misuse behavior, and
  failure cleanup.
- Statement context owns the top-level statement boundary: diagnostics reset,
  warning count, affected rows, backend status, and transaction completion.
  Successful baseline `REPLACE` copies the inserted row count to the public
  result and to `ROW_COUNT()`.
- Lexer/parser/AST own syntax admission and source spans. Parser code remains
  independent of runtime, catalog, storage, and SQLite.
- Analyzer/planner code resolves schemas, target tables, and columns against
  MyLite catalog descriptors; rejects unsupported shapes; converts supported
  values; and builds a descriptor-driven physical write plan.
- The catalog module owns `_mylite_catalog_*` rows, descriptor versions,
  catalog generation, and descriptor-cache invalidation. Baseline `REPLACE`
  must not mutate catalog rows, descriptor versions, catalog generation, or
  `sqlite_schema_generation`.
- SQLite owns durable b-tree row storage and rollback durability for generated
  prepared statements. SQLite schema text, rowid values, and physical
  constraints are not MySQL metadata authority.
- Storage/VFS owns the `.mylite` preamble and shifted SQLite payload boundary.
  Baseline `REPLACE` writes occur only inside the shifted SQLite payload and
  must not touch byte range `[0, 4096)`.

## Supported SQL Grammar

The feature admits one top-level statement per `mylite_execute()` call.

Supported subset:

```sql
REPLACE [INTO] table_name VALUES (value[, ...])[, (value[, ...])] ...
REPLACE [INTO] table_name (column_name[, ...])
    VALUES (value[, ...])[, (value[, ...])] ...

value:
    integer_literal
  | + integer_literal
  | - integer_literal
  | TRUE
  | FALSE
  | NULL
```

`table_name` uses the existing table lifecycle subset:

```sql
table_name:
    identifier
  | identifier.identifier
```

Only unqualified column-list names are supported. The parser should reject
deferred MySQL forms syntactically unless a deterministic analyzer diagnostic
is already available.

### MyLite Lemon-Syntax Snippet

This snippet describes MyLite's intended grammar extension, not MySQL's full
grammar:

```lemon
statement(A) ::= replace_values_statement(B). {
    A = B;
}

replace_values_statement(A) ::=
    REPLACE(R) INTO table_name(T) insert_column_list_opt(C) VALUES insert_row_list(V). {
    A = mylite_sql_parser_make_replace_values_statement(state, R, T, C, V);
}
replace_values_statement(A) ::=
    REPLACE(R) table_name(T) insert_column_list_opt(C) VALUES insert_row_list(V). {
    A = mylite_sql_parser_make_replace_values_statement(state, R, T, C, V);
}
```

The existing `insert_column_list_opt`, `insert_row_list`, `insert_row`, and
`insert_value` nonterminals are reused so `REPLACE ... VALUES` accepts the same
literal subset and column-list shape as baseline row-value inserts.

## Schema And Table Resolution

`REPLACE INTO t ...` uses the selected schema. If no selected schema exists,
the statement fails with MySQL error `1046`, SQLSTATE `3D000`, and message
`No database selected`.

`REPLACE INTO schema_name.t ...` resolves the schema explicitly and does not
require a selected default schema. Unknown schema names fail with MySQL error
`1049`, SQLSTATE `42000`. Unknown table names fail with MySQL error `1146`,
SQLSTATE `42S02`.

MyLite reserved schema and table names with `_mylite_*` spelling must be
rejected before any SQLite SQL is generated. Name matching follows the current
catalog descriptor policy: schema, table, and column lookup use the existing
case-insensitive descriptor lookup, with descriptor spelling used for generated
SQL and result metadata where applicable.

Only persistent base-table descriptors are supported. If future descriptors add
views, temporary tables, virtual tables, or other object kinds, baseline
`REPLACE` must reject them before physical SQL generation until explicitly
specified.

## Column Resolution And Value Conversion

If the column list is omitted, target columns are the visible descriptor
columns in catalog ordinal order. If a column list is present, each name is
resolved against MyLite column descriptors, including invisible columns when
explicitly named. Duplicate target columns fail with MySQL error `1110`.
Unknown target columns fail with MySQL error `1054`.

Every target value is converted by MyLite before SQLite binding:

- `NULL` is accepted only for nullable columns.
- `TRUE` and `FALSE` convert to integer `1` and `0`.
- Decimal integer literals with optional unary `+` or `-` convert according to
  the target descriptor family.
- `INT`, `INTEGER`, `TINYINT`, `SMALLINT`, `MEDIUMINT`, `BIGINT`, their
  supported aliases, and their `UNSIGNED` forms use the existing MyLite
  descriptor ranges.
- `BIGINT UNSIGNED` remains capped to MyLite's current signed 64-bit physical
  integer range.
- Range failures use MySQL error `1264`, SQLSTATE `22003`, with the target
  column name and failing row number.

Omitted columns receive their descriptor default if one is present. Omitted
nullable columns without an explicit default receive effective SQL `NULL`.
Omitted `NOT NULL` columns without an explicit default fail with MySQL error
`1364`, SQLSTATE `HY000`. Explicit `NULL` for a `NOT NULL` column fails with
MySQL error `1048`, SQLSTATE `23000`.

Unsupported value shapes such as strings, decimals, floats, hex, bit literals,
arithmetic expressions, functions, parameters, variables, subqueries, and DML
`DEFAULT` keyword values fail deterministically and must not reach SQLite.

## Physical SQLite Handling

The supported no-key baseline must not generate SQLite `REPLACE` or
`INSERT OR REPLACE`. Those SQLite forms are tied to SQLite physical constraints,
while MySQL replacement semantics are governed by MyLite's future MySQL key
descriptors.

The generated physical SQL shape is the same standard SQLite insert shape used
by row-value inserts:

```sql
INSERT INTO "<physical_table_name>" ("col1", "col2", ...) VALUES (?1, ?2, ...)
```

Every identifier is quoted. The physical table name is the stable descriptor
physical name such as `_mylite_user_table_<table_id>`. Every value is bound as
either `sqlite3_bind_int64()` or `sqlite3_bind_null()` after MyLite conversion.
No user SQL literal text is interpolated into generated SQLite SQL.

Multi-row baseline `REPLACE` statements execute inside a MyLite-owned SQLite
transaction and reuse one prepared statement per row, preserving the existing
row-value insert all-or-nothing behavior. This is a MyLite wrapper/translation
approach over public SQLite prepared-statement APIs; it requires no SQLite fork
patches.

## Result And Statement State

Successful baseline `REPLACE` returns through the existing public non-row
result conventions:

- no result rows;
- no result columns;
- `affected_rows` equals the number of inserted rows;
- `ROW_COUNT()` after the statement returns the same value;
- `warning_count == 0` for supported in-range statements;
- `LAST_INSERT_ID()` remains unchanged because auto-increment is not supported;
- catalog generation and SQLite schema generation do not change.

For future duplicate-key support, affected rows must become the sum of deleted
and inserted rows, matching MySQL. This baseline must not encode a generic
"replace always equals insert" invariant outside the no-key path.

## Diagnostics

The implementation must cover:

- syntax errors and unsupported grammar: deterministic parser or unsupported
  diagnostics;
- missing default schema: MySQL `1046`, SQLSTATE `3D000`;
- unknown schema: MySQL `1049`, SQLSTATE `42000`;
- unknown table: MySQL `1146`, SQLSTATE `42S02`;
- reserved `_mylite_*` schema/table names: deterministic MyLite reserved-name
  diagnostic before generated SQLite SQL;
- unsupported object kind: deterministic unsupported diagnostic;
- unknown column-list target: MySQL `1054`, SQLSTATE `42S22`;
- duplicate column-list target: MySQL `1110`, SQLSTATE `42000`;
- column count mismatch: MySQL `1136`, SQLSTATE `21S01`;
- omitted `NOT NULL` no-default column: MySQL `1364`, SQLSTATE `HY000`;
- explicit `NULL` into `NOT NULL`: MySQL `1048`, SQLSTATE `23000`;
- integer out-of-range: MySQL `1264`, SQLSTATE `22003`;
- unsupported literal/expression/default/parameter forms: deterministic
  MyLite unsupported diagnostic;
- physical SQLite failures: mapped to existing MyLite diagnostics and rollback
  behavior;
- allocation failures: `MYLITE_NOMEM` with no leaked partially owned objects.

## Tests

Add MySQL-runtime expectation coverage for:

- selected-schema and schema-qualified target resolution;
- full-row, explicit-column, no-`INTO`, multi-row, `NULL`, `TRUE`, `FALSE`,
  signed literal, and integer-family boundary success cases;
- no-key duplicate-looking rows inserting as independent rows;
- primary-key MySQL replacement affected-row behavior as a documented future
  gap;
- affected rows, warning count, `ROW_COUNT()`, and absence of result rows;
- unknown schema, unknown table, reserved target names, unknown columns,
  duplicate columns, count mismatches, omitted required columns, explicit
  `NULL` into `NOT NULL`, and range failures;
- deterministic rejection of `VALUE`, row constructors, `SET`, `SELECT`,
  modifiers, partitions, qualified column-list names, expression values,
  `DEFAULT`, parameters, string/decimal/float/hex/bit values, functions, and
  subqueries;
- persistence after close/reopen, DML after table rename, behavior after drop,
  independent file-backed handles, `.mylite` preamble preservation, catalog
  generation stability, SQLite schema generation stability, and zero-initialized
  cleanup for new planner/result objects;
- preservation of existing lexer, parser, runtime handle, diagnostics,
  statement context, row values, insert set, select, delete, update, file-backed
  opening, VFS, and catalog tests.
