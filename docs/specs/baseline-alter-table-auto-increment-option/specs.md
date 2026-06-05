# Baseline ALTER TABLE AUTO_INCREMENT Option

## Summary

This phase adds the narrow table-option form:

```sql
ALTER TABLE table_name AUTO_INCREMENT [=] nonnegative_integer_literal
```

for persistent MyLite base tables. It builds on the existing descriptor-owned
auto-increment lifecycle: one integer-family auto-increment column on the
current single-column primary key, durable table-level next counters, generated
insert values, `LAST_INSERT_ID()`, `TRUNCATE` reset behavior, `SHOW CREATE
TABLE`, `SHOW TABLE STATUS`, and limited `INFORMATION_SCHEMA.TABLES`.

The goal is the common migration/import operation that resets the next generated
value after data has been loaded or deleted. This phase does not add or remove
the `AUTO_INCREMENT` column attribute and does not broaden the auto-increment
column model.

## Sources

- MyLite README architecture: `README.md`
- MyLite engineering standards:
  `docs/architecture/engineering-standards.md`
- Baseline auto-increment lifecycle:
  `docs/specs/baseline-auto-increment-lifecycle/specs.md`
- Baseline primary key lifecycle:
  `docs/specs/baseline-primary-key-lifecycle/specs.md`
- Baseline ALTER TABLE ADD PRIMARY KEY:
  `docs/specs/baseline-alter-table-add-primary-key/specs.md`
- MySQL lexer: `docs/specs/mysql-lexer/specs.md`
- MySQL parser scaffold:
  `docs/specs/mysql-parser-scaffold/specs.md`
- MySQL 8.4 Reference Manual, `ALTER TABLE`:
  <https://dev.mysql.com/doc/refman/8.4/en/alter-table.html>
- MySQL 8.4 Reference Manual, `CREATE TABLE` table options:
  <https://dev.mysql.com/doc/refman/8.4/en/create-table.html>
- MySQL 8.4 Reference Manual, using `AUTO_INCREMENT`:
  <https://dev.mysql.com/doc/refman/8.4/en/example-auto-increment.html>
- Observed MySQL 8.4.9 runtime behavior recorded by
  `packages/libmylite/tests/mysql_baseline_alter_table_auto_increment_option_expectations.sh`.

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior, public
SQLite APIs, and existing MyLite source code. It does not copy MySQL, MariaDB,
Percona, SQLite implementation internals, or other restrictively licensed
implementation sources.

## MySQL 8.4.9 Observations

The expectation script for this feature records runtime probes for the exact
supported and deferred surface. Observed behavior that defines this slice:

- `ALTER TABLE t AUTO_INCREMENT = 10` succeeds for an InnoDB table with an
  auto-increment primary key, reports `ROW_COUNT() == 0`, and reports
  `@@warning_count == 0`.
- The `=` token is optional: `ALTER TABLE t AUTO_INCREMENT 10` is accepted.
- `SHOW CREATE TABLE` renders `AUTO_INCREMENT=N` when the effective next value
  is greater than the default next value.
- `SHOW TABLE STATUS` and `INFORMATION_SCHEMA.TABLES.AUTO_INCREMENT` report the
  first positive effective table auto-increment status value. If the requested
  value is below the next value after the current maximum row value, the
  metadata cell reports that higher effective value. Later generated inserts
  and later `ALTER TABLE ... AUTO_INCREMENT=N` statements can change
  `SHOW CREATE TABLE`'s effective next value without changing this status cell.
- The next generated insert uses the effective next value and then advances the
  durable counter through the existing auto-increment lifecycle.
- Setting the option lower than the current durable counter is allowed after
  deletes. The effective next value is constrained by existing row values, not
  by the previous counter. For example, after rows `1..10` are deleted down to
  `1,2`, `ALTER TABLE t AUTO_INCREMENT=5` makes the next generated row `5`.
- Setting the option lower than or equal to the current maximum auto-increment
  column value does not create a duplicate next value. The effective next value
  becomes the current maximum value's next generated value.
- `AUTO_INCREMENT=0` is accepted and normalizes to the default lower bound, then
  is still constrained by existing row values.
- `ALTER TABLE no_auto_table AUTO_INCREMENT=5` succeeds with zero affected rows
  and warnings, but has no visible `SHOW CREATE TABLE` or `SHOW TABLE STATUS`
  `Auto_increment` effect when the table has no auto-increment column.
- Negative literals, unary-plus literals, string literals, `NULL`, and missing
  option values fail with `1064 / 42000`.
- MySQL accepts wider forms such as decimal numeric literals, very large
  unsigned numeric literals, duplicate table options, and multi-action
  `ALTER TABLE` statements containing `AUTO_INCREMENT`; those remain deferred.

## Scope

Supported:

- persistent base tables only;
- unqualified and schema-qualified table names using the existing selected
  schema policy;
- a single `AUTO_INCREMENT [=] nonnegative_decimal_integer_literal` table
  option as the whole `ALTER TABLE` action;
- decimal integer literals in the current MyLite signed-64 positive range,
  with `0` accepted as the MySQL lower-bound normalization form;
- auto-increment tables that use the existing supported single-column
  integer-family primary-key auto-increment descriptor;
- non-auto-increment persistent base tables as accepted no-ops with zero
  affected rows and warnings;
- effective next-value calculation from descriptor metadata and physical row
  values:
  - normalize requested `0` to `1`;
  - read the current maximum non-`NULL` auto-increment column value from the
    physical table when the table has an auto-increment column;
  - set the durable next counter to the greater of the normalized requested
    value and the next generated value after the current maximum row value;
  - preserve the existing max-value exhaustion behavior from the
    auto-increment lifecycle;
- descriptor-backed `SHOW CREATE TABLE` through the durable next counter and
  `SHOW TABLE STATUS` / limited `INFORMATION_SCHEMA.TABLES.AUTO_INCREMENT`
  through the table descriptor status value;
- generated inserts, explicit inserts, updates, `CREATE TABLE ... LIKE`,
  `TRUNCATE`, reopen persistence, table rename/drop behavior, independent
  file-backed handles, and `.mylite` preamble preservation through existing
  auto-increment paths.

Deferred:

- adding, dropping, or modifying the `AUTO_INCREMENT` column attribute with
  `ALTER TABLE`;
- auto-increment columns outside the current single-column integer primary-key
  descriptor model;
- temporary tables, views, partitions, privileges, metadata locks, online DDL
  algorithms, locks, and implicit-commit emulation;
- multi-action `ALTER TABLE`, including combinations with `ADD COLUMN`,
  `DROP COLUMN`, `ADD PRIMARY KEY`, charset/collation options, or index DDL;
- decimal, float, string, hex, bit, signed, expression, parameter, or function
  table-option values;
- values above MyLite's current signed-64 physical counter range;
- `NO_AUTO_VALUE_ON_ZERO`, `auto_increment_increment`,
  `auto_increment_offset`, replication, concurrency lock-mode behavior, and
  protocol insert-id metadata;
- SQLite fork patches.

## Ownership Boundaries

- Public API: no ABI change. Applications use `mylite_execute()` and existing
  result and diagnostic accessors.
- Statement context: owns diagnostics, warning count, affected rows, result
  object conventions, transaction completion, and cleanup on failure.
- Parser/AST: admits only the narrow `ALTER TABLE ... AUTO_INCREMENT [=] N`
  shape and preserves source spans. It does not inspect descriptors, counters,
  rows, or SQLite schema.
- Analyzer/planner/runtime: resolves the target table from MyLite descriptors,
  rejects unsupported shapes, computes the effective next value from the
  requested literal and descriptor-backed physical rows, and applies the counter
  update.
- Catalog module: owns the durable `auto_increment_next` table descriptor
  field. The catalog remains the MySQL metadata authority; SQLite schema text,
  `sqlite_sequence`, and rowid state are not metadata sources.
- Result/introspection builders: render the changed counter through existing
  descriptor-driven `SHOW CREATE TABLE`, `SHOW TABLE STATUS`, and limited
  `INFORMATION_SCHEMA.TABLES` paths.
- Storage/VFS: owns the `.mylite` preamble and shifted SQLite payload. This
  feature writes only SQLite payload catalog data and must not touch the
  preamble.
- SQLite physical storage: stores user rows. MyLite may use a quoted,
  descriptor-built `MAX()` query over the physical column to compute the row
  constraint, then updates catalog state through prepared statements.

## Supported Grammar

The feature extends the existing single-action `ALTER TABLE` grammar with one
table option:

```sql
ALTER TABLE table_name AUTO_INCREMENT [=] nonnegative_decimal_integer_literal
```

MyLite Lemon-style snippet:

```lemon
statement(A) ::= alter_table_auto_increment_statement(B). {
    A = B;
}

alter_table_auto_increment_statement(A) ::=
    ALTER(A1) TABLE table_name(T) AUTO_INCREMENT(O) equal_opt INTEGER(V). {
    A = mylite_sql_parser_make_alter_table_auto_increment_statement(
        state,
        A1,
        T,
        mylite_sql_parser_make_table_auto_increment_option(
            state,
            O,
            mylite_sql_parser_make_literal(state, V, MYLITE_SQL_AST_LITERAL_INTEGER)));
}

equal_opt ::= .
equal_opt ::= EQUAL.
```

The implementation may reuse the existing
`MYLITE_SQL_AST_TABLE_AUTO_INCREMENT_OPTION` node. Unsupported literals remain
syntax errors or deterministic unsupported diagnostics for this slice.

## Name Resolution

The target table follows existing MyLite table-name policy:

- unqualified table names require a selected/default schema;
- schema-qualified names use the explicit schema;
- missing default schema, unknown schema, unknown table, unsupported object
  kind, and reserved `_mylite_*` names use existing diagnostics;
- only persistent base-table descriptors are supported.

Non-auto-increment base tables are accepted no-ops because MySQL accepts the
table option without visible effect when no auto-increment column exists.

## Effective Counter Semantics

Let `requested` be the parsed table-option literal after conversion:

- if the literal is `0`, `requested = 1`;
- otherwise `requested = literal`.

For tables without an auto-increment column, the statement succeeds and leaves
visible metadata unchanged.

For tables with an auto-increment column:

1. Resolve the single descriptor column where `is_auto_increment = true`.
2. Verify it is still an integer-family descriptor column.
3. Query the physical table for the maximum non-`NULL` value in the column,
   using quoted descriptor names.
4. Compute `row_next` as the next generated value after that maximum according
   to the existing auto-increment integer range helper. If there are no rows,
   or only nonpositive values are present, `row_next = 1`.
5. Set `auto_increment_next = max(requested, row_next)`.

The statement does not insert rows and does not change `LAST_INSERT_ID()`.
Successful statements report zero affected rows and zero warnings.

If the existing maximum value is the positive maximum for the column's
supported range, the next value stays at that maximum so the next generated
insert reaches the same duplicate-key/exhaustion behavior already specified by
the baseline auto-increment lifecycle.

## Physical SQLite Handling

No SQLite fork patch is required.

The physical maximum query shape is descriptor-built and internal:

```sql
SELECT MAX("<physical_column_name>")
FROM "<physical_table_name>"
WHERE "<physical_column_name>" IS NOT NULL
```

Rules:

- quote every generated SQLite identifier;
- do not interpolate user SQL text as a SQL literal;
- prepare and finalize statements through existing runtime helpers;
- read only one aggregate row, not all user rows;
- update `_mylite_catalog_tables.auto_increment_next` through the existing
  prepared catalog API;
- keep row storage, physical indexes, descriptor versions, and
  `sqlite_schema_generation` unchanged;
- leave `.mylite` preamble bytes unchanged.

## Diagnostics

Diagnostics are MySQL-compatible where verified for the admitted subset.
Unsupported wider MySQL forms may use deterministic MyLite diagnostics.

- Syntax errors and unsupported grammar: `1064`, SQLSTATE `42000`.
- Missing default schema: `1046`, SQLSTATE `3D000`.
- Unknown schema: `1049`, SQLSTATE `42000`.
- Unknown table: `1146`, SQLSTATE `42S02`.
- Reserved `_mylite_*` target names: existing reserved-name diagnostics.
- Unsupported object kind: MyLite unsupported persistent-base-table diagnostic.
- Unsupported literal kind: `1064 / 42000` syntax error when rejected by the
  parser, or the existing unsupported-error path when admitted but outside the
  signed-64 counter range.
- Physical SQLite failures while reading the maximum row value or updating the
  catalog counter: existing physical/internal diagnostics after cleanup.
- Allocation failures: `MYLITE_NOMEM` plus handle diagnostics.

Supported in-range operations must report `warning_count == 0`.

## Tests

Add a fast C runtime test, preferably
`libmylite.runtime.alter_table_auto_increment_option`, plus parser coverage and
a MySQL 8.4.9 expectation script.

Cover:

- parser acceptance for `AUTO_INCREMENT=N` and `AUTO_INCREMENT N`;
- parser rejection for negative, unary-plus, string, `NULL`, missing-value,
  decimal/float, expression, and multi-action forms if deferred;
- successful alter on auto-increment tables, including zero affected rows,
  zero warnings, no result rows, and unchanged `LAST_INSERT_ID()`;
- next generated insert after setting the counter upward;
- lowering the counter after deleting high rows;
- `AUTO_INCREMENT=0` normalization constrained by existing row values;
- schema-qualified and unqualified target resolution;
- accepted no-op on tables without an auto-increment column;
- missing default schema, unknown schema, unknown table, reserved target names,
  and unsupported object kind diagnostics;
- generated insert, explicit insert/update counter advancement, `TRUNCATE`,
  `CREATE TABLE ... LIKE`, table rename/drop, reopen persistence, and
  independent file-backed handles after the alter;
- `SHOW CREATE TABLE`, `SHOW TABLE STATUS`, and limited
  `INFORMATION_SCHEMA.TABLES.AUTO_INCREMENT` metadata after the alter, including
  status persistence after later counter changes;
- `.mylite` preamble preservation;
- zero-initialized cleanup for any new statement/planner objects;
- existing lexer, parser, runtime handle, diagnostics, statement context,
  result metadata, file-backed opening, VFS, catalog, primary key,
  auto-increment, and ALTER TABLE lifecycle tests still pass.

## Compatibility Documentation

Update `COMPATIBILITY.md` and `docs/compatibility/sql-table-ddl.md` only for
the exact supported `ALTER TABLE ... AUTO_INCREMENT` subset. Do not imply full
table-option support, auto-increment attribute changes, multi-action ALTER,
temporary tables, generated invisible primary keys, or replication/concurrency
semantics.

## Verification

Before marking implementation complete:

1. `cmake --build --preset dev`
2. Focused CTest entries for parser, auto-increment lifecycle, and the new
   alter-table auto-increment option lifecycle
3. `packages/libmylite/tests/mysql_baseline_alter_table_auto_increment_option_expectations.sh`
4. `cmake --workflow --preset check`

Review the final diff for parser scope, descriptor authority, physical
SQLite-side `MAX()` use, counter lowering correctness after deletes,
transaction cleanup, metadata accuracy, file-format safety, compatibility docs,
and test relevance.
