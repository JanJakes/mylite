# Baseline ALTER TABLE FORCE

## Status

This feature specifies a narrow `ALTER TABLE ... FORCE` slice for MyLite's
current persistent base-table descriptors:

```sql
ALTER TABLE table_name FORCE
```

The statement forces a physical rebuild of the descriptor-backed table while
preserving MyLite logical descriptors and user row values. It does not add
general ALTER TABLE multi-action support, storage-engine algorithms, locks,
indexes, constraints, or table maintenance semantics beyond this single
baseline action.

## Sources

- MyLite README architecture: `README.md`
- MyLite engineering standards:
  `docs/architecture/engineering-standards.md`
- Baseline implementation strategy:
  `docs/specs/baseline-implementation-strategy/specs.md`
- Existing table lifecycle, row-values, DML, result, catalog, and ALTER TABLE
  specs under `docs/specs/`
- MySQL lexer and parser scaffold specs:
  `docs/specs/mysql-lexer/specs.md`,
  `docs/specs/mysql-parser-scaffold/specs.md`
- SQLite source snapshot notes: `third_party/sqlite/README.md`
- MySQL 8.4 Reference Manual, `ALTER TABLE`:
  https://dev.mysql.com/doc/refman/8.4/en/alter-table.html
- MySQL 8.4 Reference Manual, rebuilding tables:
  https://dev.mysql.com/doc/refman/8.4/en/rebuilding-tables.html

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior,
public SQLite APIs, and existing MyLite source code. It does not copy MySQL,
MariaDB, Percona, SQLite implementation internals, or other restrictively
licensed implementation sources.

## MySQL 8.4.9 Observations

The expectation script
`packages/libmylite/tests/mysql_baseline_alter_table_force_expectations.sh`
records runtime probes for this feature. Observed behavior:

- `ALTER TABLE t FORCE` succeeds for an InnoDB table.
- Successful standalone `FORCE` reports `ROW_COUNT() == 0`,
  `@@warning_count == 0`, and `@@error_count == 0`.
- Existing row values remain readable after the statement.
- Empty tables succeed and report row count `0`.
- Schema-qualified targets work without a selected default database.
- Unqualified targets without a selected default database fail with error
  `1046`, SQLSTATE `3D000`.
- Unknown explicit schemas fail with error `1049`, SQLSTATE `42000`.
- Unknown target tables fail with error `1146`, SQLSTATE `42S02`.
- Temporary-table forms work in MySQL, but MyLite keeps temporary tables out of
  this baseline slice.
- MySQL accepts repeated or mixed ALTER actions such as `FORCE, FORCE` and
  `FORCE, ALGORITHM=COPY`; MyLite intentionally defers multi-action ALTER and
  algorithm clauses.

## Scope

The implementation must add:

- parser and AST support for one single-action `ALTER TABLE ... FORCE`
  statement;
- unqualified and schema-qualified target table resolution through the existing
  selected/default schema policy;
- reserved `_mylite_*` target-name rejection before physical SQL generation;
- persistent base-table descriptor validation through the existing catalog
  ownership boundary;
- a SQLite-side physical table rebuild that copies all descriptor columns into
  a replacement rowid table;
- preservation of row values, column descriptors, table descriptors, physical
  stable table name, and public result shape;
- successful result reporting with `affected_rows == 0` and
  `warning_count == 0`;
- compatibility documentation for the exact limited surface.

## Non-Goals

This feature must not implement:

- temporary tables;
- views or non-base-table object kinds;
- multi-action `ALTER TABLE`;
- repeated `FORCE` actions;
- mixing `FORCE` with `ORDER BY`, `ENGINE`, charset/collation options,
  `ADD`, `DROP`, `CHANGE`, `MODIFY`, `RENAME`, `ALTER COLUMN`, `ALGORITHM`,
  `LOCK`, or partition options;
- `ALTER TABLE` with no action;
- storage-engine changes, defragmentation guarantees, optimizer statistics,
  or online DDL algorithm behavior;
- indexes, primary keys, clustered keys, constraints, triggers, cascades,
  auto-increment reset, or generated columns;
- metadata locks, implicit commit behavior, binary logging, privileges, or
  replication semantics;
- SQLite fork patches.

## Ownership Boundary

- The public API remains unchanged. `mylite_execute()` owns the statement
  boundary and result handle lifetime.
- Statement context owns diagnostics reset, warning count, affected rows, and
  `ROW_COUNT()` state.
- Lexer/parser/AST own syntax admission for this single ALTER action. Parser
  code remains independent of runtime, catalog, storage, and SQLite.
- Analyzer/planner code resolves the target table against MyLite descriptors
  before any generated SQLite SQL is prepared.
- The catalog remains authoritative for schemas, tables, object kinds,
  physical names, and descriptor column order. This feature does not mutate
  catalog rows, descriptor versions, catalog generation, or descriptor caches.
- Runtime execution generates SQLite DDL/DML only from descriptors and stable
  physical table names. It does not consult SQLite schema text to discover
  logical columns.
- Storage/VFS owns the `.mylite` preamble and shifted SQLite payload. This
  feature must not write before the shifted SQLite payload.
- SQLite owns physical row storage and performs the copy. MyLite does not
  materialize table rows in C memory.

## Supported SQL Grammar

The feature extends the existing limited `ALTER TABLE` grammar with one
single-action form:

```sql
ALTER TABLE table_name FORCE
```

`table_name` may be unqualified or schema-qualified.

MyLite Lemon-syntax snippet:

```lemon
statement(A) ::= alter_table_force_statement(B). {
    A = B;
}

alter_table_force_statement(A) ::= ALTER(A1) TABLE table_name(T) FORCE. {
    A = mylite_sql_parser_make_alter_table_force_statement(state, A1, T);
}
```

The grammar intentionally excludes trailing actions, comma-separated ALTER
actions, `ALGORITHM`, `LOCK`, partition options, and `ALTER TABLE t` with no
action.

## Resolution Semantics

Unqualified target table names require the currently selected schema.
Schema-qualified target table names use the explicit schema and do not require
a selected schema. Missing default schema, unknown explicit schema, and unknown
table diagnostics follow the existing table lifecycle policy.

Target schemas and tables with reserved `_mylite_*` names are rejected before
catalog lookup or physical SQL generation. Only persistent base-table
descriptors are supported.

Descriptor catalog identifier matching follows MyLite's current
case-insensitive catalog name policy. SQLite schema text is not consulted.

## Runtime Semantics

For a supported statement, MyLite rebuilds the physical table in a single
SQLite transaction:

1. Create a temporary physical rowid table with the same descriptor column
   names, integer physical type, and `NOT NULL` clauses as the current physical
   table.
2. Copy all descriptor columns using `INSERT INTO temp (...) SELECT ... FROM
   original`.
3. Drop the original physical table.
4. Rename the temporary physical table to the original stable physical name.

Generated SQL uses quoted SQLite identifiers for physical table names and
descriptor column names. The admitted grammar contains only identifiers, so no
value parameters are required.

The copy remains inside SQLite. MyLite does not allocate row buffers for table
contents or implement row copying in C. The operation changes physical SQLite
schema objects, so `sqlite_schema_generation` is incremented after success.
Catalog generation and descriptor versions do not change.

Successful statements return the existing non-row statement result:

- zero result columns;
- zero result rows;
- `affected_rows == 0`;
- `warning_count == 0`.

The baseline does not claim any user-visible row order change. Later unordered
reads remain unordered by SQL semantics; tests verify row preservation through
ordered selects.

## Diagnostics

The implementation must preserve or add deterministic diagnostics for:

- syntax errors and unsupported grammar;
- missing default schema;
- unknown explicit schema;
- unknown target table;
- reserved target schema or table names;
- unsupported object kind;
- unsupported multi-action or algorithm/lock forms;
- physical SQLite failures during create, copy, drop, or rename;
- allocation failures;
- public API misuse if the public surface changes.

Successful supported statements produce no warnings.

## Storage, File Format, and SQLite Policy

This feature uses public SQLite SQL through MyLite's existing connection. It
does not require SQLite extension APIs or fork hooks. The `.mylite` preamble
and shifted SQLite payload invariants are preserved by the existing
file-backed VFS and SQLite transaction machinery.

Generated MyLite user tables must remain ordinary SQLite rowid tables. The
temporary rebuild table is created without `WITHOUT ROWID`; after rename, the
stable physical table name again points to an ordinary rowid table.

## Tests

Add MySQL-runtime-verified expectations and C tests covering:

- successful `ALTER TABLE t FORCE` on nonempty and empty persistent base
  tables;
- schema-qualified and unqualified target resolution;
- result shape, `affected_rows == 0`, `ROW_COUNT() == 0`, and
  `warning_count == 0`;
- row preservation after force rebuild;
- descriptor preservation, catalog generation preservation, descriptor version
  preservation, and `sqlite_schema_generation` increment;
- reopen persistence and `.mylite` preamble preservation;
- independent file-backed handles;
- update after table rename and failure after drop;
- missing default schema, unknown schema, unknown table, and reserved
  `_mylite_*` target names;
- unsupported syntax such as repeated actions, mixed actions, algorithm/lock
  clauses, partition options, temporary tables, and `ALTER TABLE t` without an
  action;
- physical SQLite failure cleanup and row-operation diagnostics;
- parser coverage for accepted and rejected forms;
- existing parser, runtime lifecycle, storage, VFS, bootstrap, diagnostics,
  result, statement-context, and catalog tests.

## Compatibility Documentation

Update `COMPATIBILITY.md` and `docs/compatibility/sql-table-ddl.md` to describe
only the limited persistent-base-table `ALTER TABLE ... FORCE` rebuild. Do not
claim full table maintenance, storage-engine defragmentation, algorithms,
locks, multi-action ALTER, temporary tables, implicit commits, indexes,
constraints, or privileges.
