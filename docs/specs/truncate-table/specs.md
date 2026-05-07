# TRUNCATE TABLE

## Scope

This slice implements executable `TRUNCATE TABLE` support for MyLite base
tables created by the currently supported `CREATE TABLE` subset.

In scope:

- `TRUNCATE table_name`
- `TRUNCATE TABLE table_name`
- selected-schema and schema-qualified table resolution
- row removal from the physical SQLite user table
- preservation of table, column, and index metadata
- preservation of the physical table object
- `AUTO_INCREMENT` reset for the supported metadata model
- affected rows reported as `0`
- deterministic diagnostics for no selected database, missing table, missing
  schema, and system-schema targets
- MySQL-compatible parent-table foreign-key restrictions for cataloged
  persistent foreign keys
- statement-level atomicity for MyLite's internal row and metadata state
- MySQL-compatible implicit commit before validation and execution

Deferred behavior is explicit: privilege checks, triggers, handlers, table
locks, partition-specific truncation, corrupted-file recovery, binary logging,
Performance Schema summary table behavior, and temporary-table shadowing are
not modeled in this slice.

## Sources

The intended behavior is independently specified from:

- MySQL 8.4 Reference Manual, `TRUNCATE TABLE`
  (`https://dev.mysql.com/doc/refman/8.4/en/truncate-table.html`)
- MySQL 8.4.9 runtime probes against `mylite-mysql-849`
- Existing MyLite `CREATE TABLE`, `DROP TABLE`, `RENAME TABLE`, transaction,
  and `AUTO_INCREMENT` specs

This document is independently authored. It does not copy MySQL documentation,
grammar text, or implementation sources.

## MySQL 8.4.9 Observations

The accepted syntax is one table target:

- `TRUNCATE t`
- `TRUNCATE TABLE t`
- `TRUNCATE schema_name.t`
- `TRUNCATE TABLE schema_name.t`

`TRUNCATE TABLE t1, t2` and `TRUNCATE TABLE IF EXISTS t` are syntax errors.
`truncate` remains valid as an unquoted nonreserved identifier and as a
function name in expression grammar contexts.

Schema resolution follows ordinary single-table DDL:

- an unqualified target requires a selected database;
- a qualified target can run without a selected database;
- a missing qualified schema or table reports that the table does not exist;
- a system-schema target is rejected before mutation.

Runtime side effects verified against MySQL 8.4.9:

- success reports `0` affected rows;
- all rows are removed;
- table, column, and index definitions remain;
- unique indexes still constrain later inserts;
- `AUTO_INCREMENT` allocation restarts at `1` for an empty truncated table,
  including tables originally created with `AUTO_INCREMENT=10`;
- truncating a parent table referenced by another table fails with error
  `1701` / `42000`, regardless of whether child rows currently exist;
- truncating a child table succeeds and leaves the parent rows and foreign-key
  metadata intact;
- truncating a self-referential table succeeds;
- with `foreign_key_checks=0`, truncating a referenced parent table succeeds,
  leaves child rows and foreign-key metadata in place, and later checked child
  writes fail until a matching parent row exists again;
- `TRUNCATE TABLE` causes an implicit commit in MySQL before validation and
  execution. A later `ROLLBACK` does not restore rows, work that preceded a
  successful truncate, or work that preceded a failed truncate attempt.
- The implicit commit boundary also applies when the active transaction is
  `READ ONLY`; MySQL commits the read-only transaction before truncating.

MyLite implements the MySQL implicit-commit boundary for this slice. The
operation remains internally atomic after validation: either all rows are
removed and the supported sequence metadata is reset, or no truncate mutation
is applied.

## MyLite Semantics

Preparing a parsed truncate statement creates a custom statement handle with one
target:

- optional `schema_name`
- required `table_name`

On the first `mylite_step()`:

1. Commit any active explicit transaction before validation. This includes
   read-only transactions and failing validation paths.
2. Resolve the schema. Qualified names use their explicit schema. Unqualified
   names require the selected schema.
3. Reject system schemas with the existing MyLite system-schema diagnostic.
4. Verify that the target base table exists in `__mylite_table_catalog`.
5. When `foreign_key_checks=1`, reject the target if any non-self foreign key
   catalog row references it. The diagnostic uses MySQL error `1701` and names
   the first child schema, child table, and constraint in deterministic catalog
   order.
6. Run the mutation inside MyLite statement atomicity.
7. Delete every row from the physical SQLite user table.
8. Set `__mylite_table_catalog.auto_increment` to `NULL` for the target table.

The metadata rows in `__mylite_table_catalog`, `__mylite_column_catalog`, and
`__mylite_index_catalog` remain in place. `INFORMATION_SCHEMA.TABLES`,
`COLUMNS`, and `STATISTICS` therefore continue to expose the same table
definition after truncation. The physical SQLite table remains present under
the same MyLite internal table name.

Resetting `auto_increment` to `NULL` matches MyLite's existing insert model:
subsequent inserts compute the next generated value from the maximum stored
auto-increment column value, which is `1` for an empty table. This mirrors the
verified MySQL behavior for this first slice and avoids preserving stale
catalog sequence values.

The statement returns `MYLITE_DONE` with `mylite_affected_rows(stmt) == 0` on
success. On failure it returns `MYLITE_EXEC_ERROR`, `MYLITE_UNSUPPORTED`, or
`MYLITE_NOMEM` as appropriate and sets affected rows to `-1`.

## Grammar Sketch

The grammar is authored for MyLite Lemon syntax:

```lemon
statement(A) ::= truncate_table_statement(B). {
    A = B;
}

truncate_table_statement(A) ::= TRUNCATE(T) opt_table table_name(N). {
    A = mylite_sql_parser_make_truncate_table_statement(state, T, N);
}

opt_table ::= .
opt_table ::= TABLE.
```

## Test Plan

Parser tests cover:

- `TRUNCATE t`
- `TRUNCATE TABLE t`
- `TRUNCATE TABLE db.t`
- quoted identifiers
- `truncate` as an unquoted table identifier and as a function name
- rejected multi-target syntax
- rejected `IF EXISTS`
- rejected missing targets and dangling punctuation

Runtime tests cover:

- no selected database for an unqualified target;
- qualified target without a selected database;
- missing selected-schema table;
- missing qualified schema/table;
- system-schema access-denied diagnostics;
- row removal with affected rows `0`;
- preservation of `INFORMATION_SCHEMA.TABLES`, `COLUMNS`, and `STATISTICS`;
- preservation of the physical SQLite table object;
- unique index enforcement after truncation;
- rejection of referenced parent truncation with MySQL-compatible `1701`
  diagnostics and no mutation;
- successful child-table truncation with parent rows preserved;
- successful self-referential table truncation;
- checks-off parent truncation that keeps child rows and metadata, then rejects
  later checked child writes without a matching parent;
- `AUTO_INCREMENT` reset to generated id `1` after a table originally created
  with a higher table option;
- ordinary transaction implicit-commit survival for successful and failed
  truncate statements;
- read-only transaction implicit commit before truncate execution;
- no truncate mutation after a failed target resolution.
