# Baseline Replace Select Lifecycle

## Status

This feature specifies a narrow descriptor-driven `REPLACE ... SELECT` DML
slice for persistent base tables. It builds on `mylite_execute()`, statement
context, parser scaffolding, shifted `.mylite` storage, durable catalog
descriptors, create/drop/rename table lifecycle, integer and `NULL` row
storage, descriptor-backed `SELECT ... WHERE ... ORDER BY ... LIMIT`,
`INSERT ... SELECT`, and the current no-key `REPLACE ... VALUES` /
`REPLACE ... SET` paths.

This is not full MySQL `REPLACE ... SELECT`. MyLite currently has no user
primary-key or unique-key descriptors, so there is no supported duplicate-key
surface where MySQL would delete old rows before inserting new rows. For this
baseline, `REPLACE [INTO] target [(columns)] SELECT ... FROM source` is
supported only for the no-key case, where MySQL is insert-equivalent. Future
key-descriptor work must extend this feature before MyLite can claim
delete-before-insert replacement semantics or affected-row counts that include
deleted rows.

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
- Baseline row values lifecycle:
  `docs/specs/baseline-row-values-lifecycle/specs.md`
- Baseline insert select lifecycle:
  `docs/specs/baseline-insert-select-lifecycle/specs.md`
- Baseline replace values lifecycle:
  `docs/specs/baseline-replace-values-lifecycle/specs.md`
- Baseline replace set lifecycle:
  `docs/specs/baseline-replace-set-lifecycle/specs.md`
- Baseline update lifecycle:
  `docs/specs/baseline-update-lifecycle/specs.md`
- MySQL lexer:
  `docs/specs/mysql-lexer/specs.md`
- MySQL parser scaffold:
  `docs/specs/mysql-parser-scaffold/specs.md`
- SQLite source snapshot notes: `third_party/sqlite/README.md`
- MySQL 8.4 Reference Manual, `REPLACE`:
  https://dev.mysql.com/doc/refman/8.4/en/replace.html
- MySQL 8.4 Reference Manual, `INSERT ... SELECT`:
  https://dev.mysql.com/doc/refman/8.4/en/insert-select.html
- MySQL 8.4 Reference Manual, `SELECT`:
  https://dev.mysql.com/doc/refman/8.4/en/select.html
- MySQL 8.4 Reference Manual, out-of-range handling:
  https://dev.mysql.com/doc/refman/8.4/en/out-of-range-and-overflow.html
- MySQL 8.4 Reference Manual, `ROW_COUNT()`:
  https://dev.mysql.com/doc/refman/8.4/en/information-functions.html

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior, public
SQLite APIs, and existing MyLite source code. It does not copy MySQL, MariaDB,
Percona, SQLite implementation internals, or other restrictively licensed
implementation sources.

## MySQL 8.4.9 Observations

The expectation script
`packages/libmylite/tests/mysql_baseline_replace_select_lifecycle_expectations.sh`
records the runtime probes for this feature. Observed behavior:

- `REPLACE INTO dst(cols) SELECT ...` and `REPLACE dst(cols) SELECT ...` both
  succeed.
- Successful statements return no result rows, set `ROW_COUNT()` to the
  affected-row count, and leave `@@warning_count == 0`.
- A source `SELECT` returning zero rows inserts zero rows and succeeds with
  `ROW_COUNT() == 0`.
- If the source `SELECT` returns zero rows, omitted target columns with no
  explicit default do not raise an error.
- If the source `SELECT` returns at least one row, omitted `NOT NULL`
  no-default target columns fail with error `1364`, SQLSTATE `HY000`.
- Assigning selected `NULL` into a `NOT NULL` target column fails with error
  `1048`, SQLSTATE `23000`.
- Selected integer values outside the target column range fail in strict mode
  with error `1264`, SQLSTATE `22003`; the row number in the message is the
  selected row position after filtering, ordering, and limiting.
- Target column count and selected column count mismatches fail with error
  `1136`, SQLSTATE `21S01`, message `Column count doesn't match value count at
  row 1`, even when the source `SELECT` returns zero rows.
- Duplicate target columns fail with error `1110`, SQLSTATE `42000`.
- Unknown target and source columns fail with error `1054`, SQLSTATE `42S22`,
  and `field list` context for this slice's supported projection forms.
- Target resolution happens before source resolution for observed missing
  schema/table combinations. If both explicit schemas are unknown, MySQL
  reports the target schema. If both target and source tables are unknown in an
  existing schema, MySQL reports the target table.
- Unqualified targets without a selected default schema fail with error `1046`,
  SQLSTATE `3D000`, before source resolution.
- Schema-qualified targets and sources work without a selected default schema.
- `SELECT *` omits invisible source columns. An omitted target column list maps
  to target visible columns. Explicit target and source column references may
  name invisible columns.
- On a table without a primary key or unique index, repeated
  `REPLACE ... SELECT` statements insert independent rows and report one
  affected row per inserted row.
- On a table with a primary key, replacing one existing row and inserting one
  new row in the same `REPLACE ... SELECT` reports three affected rows: one
  deleted row plus two inserted rows. This behavior is deferred until MyLite has
  primary-key or unique-key descriptors.
- MySQL accepts wider forms that this slice defers, including
  `LOW_PRIORITY`, `DELAYED`, `PARTITION`, table-qualified target columns, and
  `REPLACE ... TABLE`.

## Scope

The implementation must add:

- parser and AST support for limited `REPLACE [INTO] table_name
  insert_column_list_opt SELECT ...`;
- optional `INTO`;
- one persistent base-table target and one persistent base-table source;
- target resolution for unqualified and schema-qualified names using the
  existing selected/default schema policy;
- source `SELECT` limited to the existing descriptor-backed table subset,
  including optional table alias, `WHERE`, one-column `ORDER BY`, and `LIMIT`;
- target column-list handling using unqualified descriptor column names;
- omitted target column handling with descriptor integer defaults or effective
  nullable `NULL` defaults, applied only to rows actually inserted;
- no-column-list target mapping to visible descriptor target columns only;
- source `SELECT *` expansion to visible descriptor source columns only;
- explicit source column references, including currently supported qualified
  source-column forms and invisible columns;
- target/source column-count validation before mutation;
- MyLite-owned validation for selected `NULL` into `NOT NULL` target columns;
- MyLite-owned integer range validation for each selected source value against
  the target descriptor before physical insertion;
- generated SQLite physical statements built only from descriptors and stable
  physical table names;
- prepared-statement parameter binding for source predicates, source limits,
  and inserted descriptor default integer values;
- SQLite-side scan/filter/sort/limit execution into an internal temporary
  table, followed by MyLite streaming validation and SQLite-side insertion from
  that same temporary table, without buffering the selected row set in C
  memory;
- affected-row reporting equal to the inserted row count for the no-key
  baseline case;
- `warning_count == 0` for supported in-range statements;
- tests and MySQL 8.4.9 expectation artifacts for supported behavior and
  deliberately rejected wider MySQL forms.

## Non-Goals

This feature must not implement:

- primary-key or unique-key descriptors, duplicate-key lookup,
  delete-before-insert replacement, replacement affected-row counts greater
  than inserted rows, cascades, triggers, or foreign keys;
- `REPLACE ... TABLE`, `REPLACE ... VALUES` changes, `REPLACE ... SET`
  changes, `LOW_PRIORITY`, `DELAYED`, `PARTITION`, aliases, row aliases,
  `RETURNING`, or arbitrary SQLite SQL pass-through;
- table-qualified target column-list names, target aliases, duplicate target
  tables, user-visible temporary tables, views, or unsupported object kinds;
- source literal projection, `FROM DUAL` source projection, expression
  projection, arithmetic, functions, variables, parameters, subqueries,
  string/decimal/float/hex/bit/date/time/json selected values, or general
  expression evaluation;
- DML `DEFAULT` keyword values in the source `SELECT` or target list;
- primary/unique/foreign keys, duplicate-key handling, auto-increment,
  `LAST_INSERT_ID()` changes, generated columns, check constraints, triggers,
  cascades, privileges, warning demotion, non-strict SQL modes, or SQLite fork
  patches.

## Ownership Boundary

- The public API remains unchanged. `mylite_execute()` owns public call
  validation, statement dispatch, result-handle ownership, and failure cleanup.
- Statement context owns diagnostics reset, warning count, affected rows,
  previous `ROW_COUNT()` state, and successful non-row result finalization.
  Successful baseline `REPLACE ... SELECT` copies the inserted row count to the
  public result and to `ROW_COUNT()`.
- Lexer/parser/AST own syntax admission and source spans. Parser code remains
  independent of runtime, catalog, storage, and SQLite.
- Analyzer/planner code resolves the target table, target columns, source
  table, source projection, predicate, order key, and limit against MyLite
  catalog descriptors; rejects unsupported shapes; and builds a
  descriptor-driven physical plan.
- MyLite runtime owns conversion and validation semantics for target
  nullability and integer range. Validation is streaming over an internal
  SQLite temporary table that materializes the selected source values once, so
  validation and insertion consume the same selected row set even when `LIMIT`
  is unordered or `ORDER BY` has ties. MyLite must not copy the full selected
  row set into a C-side buffer.
- The catalog module owns `_mylite_catalog_*` rows, descriptor versions,
  catalog generation, and descriptor-cache invalidation.
  `REPLACE ... SELECT` must not mutate catalog rows, descriptor versions,
  catalog generation, or `sqlite_schema_generation`.
- SQLite owns physical b-tree row storage, source scans, filtering, sorting,
  limiting, internal temporary storage, and the final physical insert. SQLite
  schema text and PRAGMA output are not metadata authority.
- Storage/VFS owns the `.mylite` preamble and shifted SQLite payload boundary.
  Row writes occur only inside the shifted SQLite payload and must not touch
  byte range `[0, 4096)`.

## Supported SQL Grammar

The feature admits one top-level statement per `mylite_execute()` call.

Supported subset:

```sql
REPLACE [INTO] table_name [(column_name[, column_name] ...)]
SELECT select_item_list
FROM table_name [AS] alias
[WHERE predicate]
[ORDER BY order_key [ASC | DESC]]
[LIMIT row_count]
```

The source `SELECT` subset is exactly the descriptor-backed single-table
subset currently implemented for ordinary `SELECT`, except that no-source
literal projection and `FROM DUAL` projection are intentionally deferred for
`REPLACE ... SELECT`.

`table_name` uses the existing table lifecycle subset:

```sql
table_name:
    identifier
  | identifier.identifier
```

### MyLite Lemon-Syntax Snippet

This snippet describes the intended MyLite grammar extension, not MySQL's full
grammar:

```lemon
statement(A) ::= replace_select_statement(B). {
    A = B;
}

replace_select_statement(A) ::=
    REPLACE(R) INTO table_name(T) insert_column_list_opt(C) select_statement(S). {
    A = mylite_sql_parser_make_replace_select_statement(state, R, T, C, S);
}
replace_select_statement(A) ::=
    REPLACE(R) table_name(T) insert_column_list_opt(C) select_statement(S). {
    A = mylite_sql_parser_make_replace_select_statement(state, R, T, C, S);
}
```

The `select_statement` nonterminal is the existing MyLite descriptor-backed
`SELECT` grammar. Runtime planning rejects source `SELECT` shapes outside the
descriptor-backed table subset.

## Name Resolution and Descriptors

Unqualified target and source names use the selected/default schema policy from
the table lifecycle and existing DML specs. If no default schema is selected,
an unqualified target or source fails with the existing MySQL-shaped missing
schema diagnostic. Schema-qualified target and source names bypass the default
schema requirement.

Target resolution is performed before source resolution. This preserves the
observed MySQL precedence for missing target/source schemas and tables and
matches the existing `INSERT ... SELECT` planner order.

The planner must reject reserved MyLite schema and table names before any
SQLite SQL is generated. It must reject unknown schemas, unknown tables, and
unsupported object kinds through catalog descriptors, not through SQLite
metadata. Once MyLite gains non-base-table descriptors, this slice supports
only persistent base tables.

Target column-list names must be unqualified descriptor column names. The list
is case-sensitive according to the current descriptor catalog lookup policy and
does not add MySQL collation-sensitive identifier matching in this phase.
Duplicate target names fail deterministically before physical mutation.

If the target column list is omitted, MyLite maps selected source values to
visible target columns only. If the source projection uses `*`, MyLite expands
the star to visible source columns only. Explicit source projection columns may
name invisible columns using the source `SELECT` rules already implemented.

Unknown target, projection, predicate, and order columns are resolved from
MyLite descriptors and fail before generated SQLite DML mutates the target.

## Value Conversion, Defaults, and Nullability

`REPLACE ... SELECT` does not add new source expression conversion. It accepts
only the values produced by the existing descriptor-backed source `SELECT`
subset: integer-family descriptor values and `NULL` values. Target validation
uses the same conversion and range policy as `INSERT ... SELECT`:

- `INT` and `INTEGER` targets accept values in `[-2147483648, 2147483647]`.
- `INT UNSIGNED` and `INTEGER UNSIGNED` targets accept values in
  `[0, 4294967295]`, within MyLite's current signed 64-bit physical storage
  range.
- `BIGINT` targets accept values in
  `[-9223372036854775808, 9223372036854775807]`.
- `BIGINT UNSIGNED` targets currently accept only values in
  `[0, 9223372036854775807]`, because MyLite stores integer row values in the
  current signed 64-bit physical representation. Values above that physical
  range remain unsupported until the storage/type layer grows unsigned 64-bit
  value support.

If a selected value is `NULL` and the target descriptor is `NOT NULL`, the
statement fails before insertion. If a target column is omitted and at least
one source row is selected, required no-default target columns fail with the
existing MySQL-shaped no-default diagnostic. If zero source rows are selected,
omitted required target columns do not fail because no row is inserted.

Omitted nullable columns without explicit defaults receive effective `NULL`.
Omitted integer default columns receive the descriptor-stored default value and
are validated as part of the physical insert plan. DML `DEFAULT` expressions
are not admitted in the source projection or target column list.

## Source Ordering and Materialization

Source filtering, ordering, and limiting are exactly the currently supported
descriptor-backed `SELECT` subset. For `ORDER BY` ties without additional sort
keys, MyLite must not claim a deterministic tie order beyond what the existing
source `SELECT` subset specifies and tests.

The runtime must materialize the source result into an internal SQLite
temporary table before target validation and final insertion. This keeps the
selected row set stable for self-copy statements and for unordered or tied
`LIMIT` sources without buffering the full source row set in C memory.

## Physical SQLite Handling

Generated SQLite must be built from descriptors and stable physical table
names such as `_mylite_user_table_<table_id>`. Every generated SQLite
identifier must be quoted. Predicate and limit values from the source `SELECT`
and descriptor default values for omitted target columns must be bound through
prepared statements instead of interpolated into SQL text.

Because this baseline has no key descriptors, the final physical mutation is
insert-equivalent. The implementation should reuse the descriptor-built
`INSERT INTO physical_target (...) SELECT ... FROM internal_temp` shape from
`INSERT ... SELECT`. It must not use SQLite `REPLACE`, `INSERT OR REPLACE`, or
conflict clauses; those would make SQLite schema constraints authoritative and
would not match future MySQL key semantics.

The physical operation must be statement-atomic. On validation or SQLite
failure, no target rows from the statement may remain. The `.mylite` preamble
must remain unchanged.

## Result Behavior

Successful baseline `REPLACE ... SELECT` returns through the existing public
non-row statement result conventions:

- no row result set;
- affected rows equal to the number of rows physically inserted for the no-key
  baseline case;
- `ROW_COUNT()` returns that inserted-row count;
- `warning_count == 0` for supported in-range statements;
- no changed-column protocol metadata, insert-id behavior, trigger behavior,
  or privilege semantics are added.

## Diagnostics

The implementation must provide deterministic diagnostics for:

- syntax errors and unsupported grammar;
- missing default schema;
- unknown schema;
- unknown target table;
- unknown source table;
- reserved `_mylite_*` schema or table names;
- unsupported object kind;
- duplicate target column;
- unknown target column;
- unknown source projection, predicate, or ordering column;
- target/source column-count mismatch;
- omitted required target column when source rows exist;
- selected `NULL` into a `NOT NULL` target;
- selected integer value outside the target range;
- unsupported source `SELECT` shape, including no-source literal projections,
  `FROM DUAL`, expression projections, joins, CTEs, subqueries, and arbitrary
  expressions;
- unsupported modifiers and clauses such as `LOW_PRIORITY`, `DELAYED`,
  `PARTITION`, target aliases, table-qualified target columns, and
  `REPLACE ... TABLE`;
- physical SQLite prepare/bind/step failures;
- allocation failures;
- public API misuse, if any public surface changes are introduced.

Supported in-range statements must not emit warnings.

## Tests

Add a fast plain C runtime test under `packages/libmylite/tests/`, preferably
`runtime_replace_select_lifecycle_test.c`, and register it with a dotted CTest
name. Add parser coverage for the new AST kind and both optional-`INTO`
spellings. Add a MySQL-runtime expectation script for every user-visible
behavior introduced by this slice.

Coverage must include:

- successful `REPLACE INTO target(columns) SELECT ... FROM source` and
  `REPLACE target(columns) SELECT ... FROM source`;
- full-row replacement by omitted target column list over visible columns;
- no-key repeated `REPLACE ... SELECT` inserting independent rows;
- source `WHERE`, one-column `ORDER BY`, `ASC`, `DESC`, nullable ordering, and
  `LIMIT`;
- zero-row source success, exact source row counts, and larger-than-source
  limits through the existing source `SELECT` subset;
- descriptor integer families: `INT`, `INTEGER`, `BIGINT`, and unsigned forms
  within MyLite's current physical range;
- selected `NULL`, `NULL` into nullable targets, and selected `NULL` into
  `NOT NULL` diagnostics;
- omitted descriptor defaults and omitted required target diagnostics;
- range boundaries and out-of-range diagnostics;
- invisible source and target columns through star expansion and explicit
  column references;
- schema-qualified and unqualified target/source resolution, including missing
  default schema, unknown schema, unknown target/source table, and reserved
  `_mylite_*` target/source names;
- duplicate target columns, unknown target columns, unknown source columns,
  unknown predicate columns, and unknown order columns;
- affected-row count, `ROW_COUNT()`, warning count, and absence of result rows;
- persistence across close/reopen, update after table rename, after drop where
  applicable, independent file-backed handles, and `.mylite` preamble
  preservation;
- unsupported syntax rejected deterministically: modifiers, partitions,
  table-qualified target columns, target aliases, `REPLACE ... TABLE`,
  expression/literal source projections, joins, CTEs, subqueries, parameters,
  functions, and arbitrary SQLite pass-through;
- zero-initialized cleanup for any new parser/planner/result objects;
- existing lexer, parser, runtime handle, diagnostics, statement context,
  result metadata, SQLite bootstrap policy, file-backed opening, VFS, catalog
  foundation, create/drop/rename lifecycle, row values, select, update,
  delete, insert-select, replace-values, and replace-set tests continue to
  pass.
