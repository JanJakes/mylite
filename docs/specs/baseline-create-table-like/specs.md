# Baseline CREATE TABLE LIKE

## Status

This feature specifies a narrow `CREATE TABLE ... LIKE` slice for MyLite's
current persistent base-table descriptors:

```sql
CREATE TABLE [IF NOT EXISTS] target_table LIKE source_table
CREATE TABLE [IF NOT EXISTS] target_table (LIKE source_table)
```

The statement creates an empty persistent base table whose descriptor columns
are cloned from another persistent base table. It does not copy rows, indexes,
constraints, triggers, partitions, table options, temporary-table state, or
engine-specific metadata beyond the currently supported MyLite descriptors.

## Sources

- MyLite README architecture: `README.md`
- MyLite engineering standards:
  `docs/architecture/engineering-standards.md`
- Baseline implementation strategy:
  `docs/specs/baseline-implementation-strategy/specs.md`
- Existing catalog, table lifecycle, row-values, visibility, default, DDL,
  storage, result, and parser specs under `docs/specs/`
- MySQL lexer and parser scaffold specs:
  `docs/specs/mysql-lexer/specs.md`,
  `docs/specs/mysql-parser-scaffold/specs.md`
- SQLite source snapshot notes: `third_party/sqlite/README.md`
- MySQL 8.4 Reference Manual, `CREATE TABLE ... LIKE`:
  https://dev.mysql.com/doc/refman/8.4/en/create-table-like.html

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior,
public SQLite APIs, and existing MyLite source code. It does not copy MySQL,
MariaDB, Percona, SQLite implementation internals, or other restrictively
licensed implementation sources.

## MySQL 8.4.9 Observations

The expectation script
`packages/libmylite/tests/mysql_baseline_create_table_like_expectations.sh`
records runtime probes for this feature. Observed behavior:

- `CREATE TABLE dst LIKE src` succeeds for an InnoDB base table, reports
  `ROW_COUNT() == 0`, `@@warning_count == 0`, and `@@error_count == 0`, and
  creates no rows in the destination.
- Source column metadata visible through `SHOW COLUMNS` is cloned for the
  tested integer, nullable, default, and invisible-column subset.
- `CREATE TABLE dst (LIKE src)` is accepted and has the same result shape.
- Schema-qualified target and source names work without a selected default
  database.
- An unqualified target without a selected default database fails with error
  `1046`, SQLSTATE `3D000`, even when the source is schema-qualified.
- An unqualified source without a selected default database fails with error
  `1046`, SQLSTATE `3D000`, even when the target is schema-qualified.
- Unknown explicit target or source schemas fail with error `1049`, SQLSTATE
  `42000`.
- Unknown source tables fail with error `1146`, SQLSTATE `42S02`.
- If the target exists and the source exists, `IF NOT EXISTS` is a no-op with
  `ROW_COUNT() == 0`, `@@warning_count == 1`, and `@@error_count == 0`.
- Source resolution happens before target `IF NOT EXISTS` no-op handling:
  `CREATE TABLE IF NOT EXISTS existing LIKE missing` fails with error `1146`.
- `CREATE TABLE ... LIKE view_name` fails with error `1347`, SQLSTATE
  `HY000`, because the source is not a base table. MyLite has no views yet but
  keeps this diagnostic boundary for future object kinds.

## Scope

The implementation must add:

- parser and AST support for the two `CREATE TABLE ... LIKE` statement forms;
- optional `IF NOT EXISTS`;
- unqualified and schema-qualified target and source table-name resolution
  using the existing selected/default schema policy;
- reserved `_mylite_*` schema and table name rejection before physical SQL
  generation;
- persistent base-table source validation through MyLite descriptors;
- cloning of current descriptor columns into a new table descriptor, including
  column names, logical types, physical types, nullability, default metadata,
  visibility, and ordinal order;
- an empty physical SQLite table generated from the cloned descriptors and a
  new stable physical table name;
- successful result reporting with the existing non-row result shape,
  `affected_rows == 0`, and `warning_count == 0`;
- `IF NOT EXISTS` existing-target no-op with warning count `1` when the source
  is valid;
- compatibility documentation for the exact limited surface.

## Non-Goals

This feature must not implement:

- temporary tables;
- views or non-base-table sources;
- `CREATE TABLE ... SELECT`;
- `CREATE TABLE target (LIKE source, column_definition...)` or other mixed
  column-list forms;
- table options, partition options, storage-engine options, algorithm/lock
  options, or charset/collation changes on the new table;
- row copying from the source table;
- indexes, primary keys, unique keys, foreign keys, check constraints,
  generated columns, invisible generated primary keys, triggers, cascades,
  comments, tablespaces, encryption, compression, or full metadata cloning;
- privilege checks, metadata locks, implicit commit behavior, binary logging,
  or replication semantics;
- SQLite fork patches.

## Ownership Boundary

- The public API remains unchanged. `mylite_execute()` owns the statement
  boundary and result handle lifetime.
- Statement context owns diagnostics reset, warning count, affected rows, and
  `ROW_COUNT()` state.
- Lexer/parser/AST own syntax admission for the two `LIKE` forms. Parser code
  remains independent of runtime, catalog, storage, and SQLite.
- Analyzer/planner code resolves target and source table names against MyLite
  catalog descriptors before any generated SQLite SQL is prepared.
- The catalog remains authoritative for schemas, table object kinds, stable
  physical names, descriptor columns, defaults, visibility, and descriptor
  ordering. SQLite schema text is not consulted to infer logical metadata.
- Runtime execution generates SQLite DDL only from the cloned descriptors and
  a newly allocated stable physical table name.
- Storage/VFS owns the `.mylite` preamble and shifted SQLite payload. This
  feature must not write before the shifted SQLite payload.
- SQLite owns the empty physical rowid table. MyLite does not materialize or
  scan source rows.

## Supported SQL Grammar

The feature extends the existing limited `CREATE TABLE` grammar with two
forms:

```sql
CREATE TABLE [IF NOT EXISTS] table_name LIKE table_name
CREATE TABLE [IF NOT EXISTS] table_name (LIKE table_name)
```

Both names may be unqualified or schema-qualified. `LIKE` is a keyword in
this context, not an expression operator.

MyLite Lemon-syntax snippet:

```lemon
statement(A) ::= create_table_like_statement(B). {
    A = B;
}

create_table_like_statement(A) ::=
    CREATE(C) TABLE create_if_not_exists_opt(E) table_name(T) LIKE table_name(S). {
    A = mylite_sql_parser_make_create_table_like_statement(
        state, C, E, T, S, S);
}

create_table_like_statement(A) ::=
    CREATE(C) TABLE create_if_not_exists_opt(E) table_name(T) LPAREN LIKE table_name(S)
    RPAREN(R). {
    A = mylite_sql_parser_make_create_table_like_statement(
        state, C, E, T, S, R);
}
```

The second span argument records the end of the statement-specific syntax; the
actual implementation may use token/span helpers matching existing parser
style.

## Resolution Semantics

Target and source names are resolved independently:

- An unqualified target requires the current selected schema.
- A schema-qualified target uses the explicit schema and does not require a
  selected schema.
- An unqualified source requires the current selected schema.
- A schema-qualified source uses the explicit schema and does not require a
  selected schema.

Explicit schemas that do not exist fail with the current unknown-database
diagnostic. Missing selected schema fails with the current no-database
diagnostic.

The target schema/table name and source schema/table name are checked for
reserved `_mylite_*` names before generated SQLite SQL is built. The target
must not already exist unless `IF NOT EXISTS` is present and the source table
has already resolved successfully. The source must exist and must be a base
table descriptor.

Descriptor catalog identifier matching follows MyLite's current
case-insensitive catalog name policy. Case-only spelling behavior, if any,
comes from the existing catalog resolver and is not expanded by this feature.

## Descriptor Cloning Semantics

For each source descriptor column, in catalog ordinal order, the target table
receives a new column descriptor with:

- the same logical column name;
- the same logical type string;
- the same physical type string;
- the same nullable flag;
- the same visibility flag;
- the same default kind and default integer payload;
- a fresh descriptor row owned by the new target table;
- an ordinal matching the source descriptor order.

The target table receives a fresh table id, fresh table descriptor row, and
fresh stable physical name. The source descriptor rows and physical table are
unchanged. The target table is empty even if the source contains rows.

The clone only supports descriptor kinds that the current physical table
builder can reproduce. Today that is MyLite's integer-family physical
`INTEGER` storage with nullable and `NOT NULL` flags. If a future descriptor
type appears before physical creation can reproduce it, planning must reject
the source before mutating the catalog.

Default metadata is copied as metadata. The physical SQLite schema continues
to omit descriptor defaults, matching the existing MyLite policy where
descriptor-owned defaults are applied by MyLite DML paths.

## Runtime Semantics

For a supported statement, MyLite:

1. Resolves the source table descriptor and loads its column descriptors.
2. Resolves the target schema/table name.
3. If `IF NOT EXISTS` is present and the valid target already exists, appends
   the existing table note and returns a successful no-op result.
4. Allocates a new table id and physical table name.
5. Inserts new target table and column catalog rows in a catalog mutation.
6. Executes generated SQLite `CREATE TABLE` for the new physical table.
7. Commits the catalog mutation and increments `sqlite_schema_generation`.

Generated SQL uses quoted SQLite identifiers for the new physical table name
and descriptor column names. The generated physical table shape uses the same
standard `CREATE TABLE` form as normal MyLite persistent base tables:
descriptor columns are SQLite `INTEGER` columns with `NOT NULL` clauses where
needed. No source rows are read and no values are bound.

Successful non-noop statements return the existing non-row statement result:

- zero result columns;
- zero result rows;
- `affected_rows == 0`;
- `warning_count == 0`.

Successful `IF NOT EXISTS` no-op statements return the existing create-table
no-op result shape with warning count `1` and no result rows.

## Diagnostics

The implementation must preserve or add deterministic diagnostics for:

- syntax errors and unsupported grammar;
- missing default schema for unqualified target or source;
- unknown explicit target or source schema;
- unknown source table;
- existing target table without `IF NOT EXISTS`;
- reserved target or source schema/table names;
- unsupported target object kind once non-table target descriptors exist;
- unsupported source object kind once non-base descriptors exist;
- unsupported mixed column-list, table-option, CTAS, temporary, partition,
  engine, charset/collation, or generated-column forms;
- unsupported descriptor source types before mutation;
- physical SQLite failures while creating the target physical table;
- allocation failures;
- public API misuse if the public surface changes.

Successful supported statements produce no warnings. Existing-target
`IF NOT EXISTS` no-ops append one warning/note using the existing table-exists
diagnostic path after the source has resolved successfully.

## Storage, File Format, and SQLite Policy

This feature uses public SQLite SQL through MyLite's existing connection. It
does not require SQLite extension APIs or fork hooks. The `.mylite` preamble
and shifted SQLite payload invariants are preserved by the existing
file-backed VFS and SQLite transaction machinery.

Generated MyLite user tables remain ordinary SQLite rowid tables. The target
physical table is empty at creation time, so there is no row materialization
or source-table scan.

## Tests

Add MySQL-runtime-verified expectations and C tests covering:

- successful `CREATE TABLE dst LIKE src` and `CREATE TABLE dst (LIKE src)`;
- zero copied rows and source row preservation;
- result shape, `affected_rows == 0`, `ROW_COUNT() == 0`, and warning count;
- cloned integer-family columns, nullability, defaults, dropped defaults, and
  invisible-column descriptors;
- schema-qualified and unqualified target/source resolution, including no
  selected schema, unknown schemas, unknown source tables, existing target
  tables, and existing target with `IF NOT EXISTS`;
- source validation before existing-target `IF NOT EXISTS` no-op;
- reserved `_mylite_*` target and source names;
- unsupported syntax such as CTAS, mixed column lists, table options after
  `LIKE`, temporary tables, partition clauses, and trailing options;
- unsupported source object kinds when available;
- parser coverage for accepted and rejected forms;
- reopen persistence of source rows, empty clone rows, cloned descriptors, and
  subsequent inserts into the clone;
- create after source table rename and failure after source drop;
- `.mylite` preamble preservation and independent file-backed handles;
- cleanup/zero-initialized deinit paths for new statement/planner objects;
- existing parser, runtime lifecycle, storage, VFS, bootstrap, diagnostics,
  result, statement-context, catalog, default, visibility, and DDL tests.

## Compatibility Documentation

Update `COMPATIBILITY.md` and `docs/compatibility/sql-table-ddl.md` to describe
only the limited persistent-base-table descriptor clone. Do not claim full
metadata cloning, row copying, temporary tables, views, CTAS, indexes,
constraints, generated columns, triggers, engine options, implicit commits, or
privileges.
