# Baseline Insert Select Keyed Targets

## Status

This feature extends the existing descriptor-driven table-backed
`INSERT ... SELECT` path so it can target persistent base tables with MyLite
primary-key, unique-index, foreign-key child, and supported `AUTO_INCREMENT`
descriptors. It builds on the existing `INSERT ... SELECT` lifecycle, row
validation and materialization, primary-key and unique-index enforcement,
foreign-key checks, and auto-increment allocation.

This is not full MySQL `INSERT ... SELECT`. It preserves the current source
`SELECT` envelope and admits only the currently supported target descriptor
families and row conversions.

## Sources

- MyLite README architecture: `README.md`
- MyLite engineering standards:
  `docs/architecture/engineering-standards.md`
- Baseline insert-select lifecycle:
  `docs/specs/baseline-insert-select-lifecycle/specs.md`
- Baseline primary-key lifecycle:
  `docs/specs/baseline-primary-key-lifecycle/specs.md`
- Baseline unique-index lifecycle:
  `docs/specs/baseline-unique-index-lifecycle/specs.md`
- Baseline foreign-key constraints:
  `docs/specs/baseline-foreign-key-constraints/specs.md`
- Baseline auto-increment lifecycle:
  `docs/specs/baseline-auto-increment-lifecycle/specs.md`
- MySQL 8.4 Reference Manual, `INSERT`:
  <https://dev.mysql.com/doc/refman/8.4/en/insert.html>
- MySQL 8.4 Reference Manual, `INSERT ... SELECT`:
  <https://dev.mysql.com/doc/refman/8.4/en/insert-select.html>
- MySQL 8.4 Reference Manual, `AUTO_INCREMENT` usage:
  <https://dev.mysql.com/doc/refman/8.4/en/example-auto-increment.html>

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior,
public SQLite APIs, and existing MyLite source code. It does not copy MySQL,
MariaDB, Percona, SQLite implementation internals, or other restrictively
licensed implementation sources.

## MySQL 8.4.9 Observations

The expectation script
`packages/libmylite/tests/mysql_baseline_insert_select_keyed_targets_expectations.sh`
records runtime probes for this feature. Observed behavior:

- `INSERT INTO pk_target SELECT ...` succeeds when selected key values are
  unique and compatible. `ROW_COUNT()` is the inserted row count and
  `@@warning_count == 0`.
- Primary-key and unique-index duplicate conflicts fail with `1062 / 23000`.
  InnoDB rolls back rows already inserted by the same statement.
- Unique indexes permit multiple `NULL` key parts as in existing MyLite unique
  index support.
- Inserting child rows with existing parents succeeds. A missing non-`NULL`
  parent key fails with `1452 / 23000` and rolls back the statement.
- `INSERT IGNORE ... SELECT` skips duplicate-key rows and missing-parent child
  rows, records warnings, and inserts nonconflicting rows. This feature admits
  that demotion only for duplicate keys and supported foreign-key child checks;
  other selected-row conversion warnings remain deferred.
- `AUTO_INCREMENT` columns work in table-backed `INSERT ... SELECT`.
  Generated values are assigned in selected row order. `LAST_INSERT_ID()`
  reports the first generated value in a multi-row statement. Explicit positive
  auto-increment values advance the next generated value when they exceed the
  current counter. `NULL` and zero generate values unless
  `NO_AUTO_VALUE_ON_ZERO` makes zero explicit.
- MySQL also supports `INSERT IGNORE ... SELECT` into `AUTO_INCREMENT`
  targets, but skipped duplicate-key and missing-parent rows can affect the
  next generated value in pattern-specific ways. MyLite defers that combined
  behavior and rejects table-backed `INSERT IGNORE ... SELECT` into
  `AUTO_INCREMENT` targets for this slice.
- MySQL allows selecting from the same persistent target table. It uses an
  internal temporary table so the selected row set is fixed before insertion.
  MyLite already has the same high-level shape through its internal SQLite
  temporary table.

## Scope

The implementation must add table-backed `INSERT ... SELECT` support for:

- persistent base-table targets with supported primary-key descriptors;
- persistent base-table targets with supported unique-index descriptors,
  including composite unique indexes and existing prefix/string key slices;
- persistent base-table foreign-key child targets using the currently
  supported integer-family child descriptors;
- supported indexed `AUTO_INCREMENT` targets, including omitted generated
  values, explicit positive values, `NULL`, zero, and
  `NO_AUTO_VALUE_ON_ZERO` behavior already implemented for `INSERT`, except
  when combined with table-backed `INSERT IGNORE ... SELECT`;
- strict duplicate-key and missing-parent diagnostics with all-or-nothing
  statement rollback;
- limited `INSERT IGNORE ... SELECT` duplicate-key and missing-parent demotion
  for non-`AUTO_INCREMENT` targets;
- affected rows, warnings, `ROW_COUNT()`, and `LAST_INSERT_ID()` following the
  verified MySQL behavior for the admitted subset;
- same-table persistent source/target statements through the existing internal
  temporary table materialization;
- ordinary row-only writes leaving table descriptors and SQLite schema
  generation unchanged, while successful generated or explicit
  `AUTO_INCREMENT` values may persist the existing catalog-owned next counter.

The source `SELECT` grammar, target column-list grammar, conversion rules, and
unsupported source shapes remain those of the existing table-backed
`INSERT ... SELECT` slice.

## Non-Goals

This feature does not implement:

- `INSERT ... SELECT ... ON DUPLICATE KEY UPDATE`;
- `REPLACE ... SELECT` into key-bearing targets;
- table-backed `INSERT IGNORE ... SELECT` into `AUTO_INCREMENT` targets;
- table-backed `INSERT IGNORE ... SELECT` warning demotion for selected-row
  range, nullability, default, string length, temporal, decimal, approximate,
  binary, `BIT`, `ENUM`, `SET`, or JSON conversion issues beyond paths already
  cleanly accepted by the existing slice;
- source joins, CTEs, unions, grouping, arbitrary expressions, subqueries,
  `TABLE`, row constructors, target partitions, aliases for the target,
  triggers, generated columns, cascades beyond the currently implemented
  direct FK checks, privileges, or protocol-specific metadata changes.

## Ownership Boundary

- The public API remains unchanged. `mylite_execute()` continues to dispatch a
  non-row statement and return an existing result object with affected-row and
  diagnostics state.
- Statement context owns diagnostics reset, warning count, affected rows,
  previous `ROW_COUNT()`, `LAST_INSERT_ID()`, and cleanup on failure.
- Parser/AST ownership is unchanged. This phase does not add grammar; it
  removes runtime restrictions for shapes already parsed.
- Analyzer/planner resolves target and source descriptors from the MyLite
  catalog, not SQLite schema text. Key descriptors, foreign-key descriptors,
  and auto-increment descriptors remain catalog-owned.
- Runtime materializes the selected source rows into an internal SQLite
  temporary table, validates selected values, then streams rows through the
  same MyLite insert machinery used by `INSERT ... VALUES` and row-scalar
  `INSERT ... SELECT`. This keeps duplicate checks, foreign-key checks,
  string-key validation, auto-increment allocation, warnings, and
  `LAST_INSERT_ID()` in one code path.
- SQLite owns source scanning, filtering, ordering, limiting, temporary-table
  storage, physical row insertion, and physical unique indexes. MyLite owns all
  compatibility validation and diagnostics before exposing results.
- Storage/VFS owns the `.mylite` preamble and shifted SQLite payload boundary.
  This phase writes only SQLite payload pages and does not touch the preamble.

## Grammar

No grammar extension is required. The existing independently authored MyLite
subset remains:

```lemon
insert_select_statement(A) ::=
    INSERT(I) insert_modifier_list_opt(M) INTO table_name(T)
    insert_column_list_opt(C) select_statement(S). {
    A = mylite_sql_parser_make_insert_select_statement(state, I, T, C, S, M, NULL);
}
```

Runtime planning continues to reject source and target shapes outside the
existing `INSERT ... SELECT` subset.

## Runtime Semantics

Planning initializes the target `planned_insert` with loaded primary key,
secondary indexes, child foreign keys, and auto-increment metadata for both
row-scalar and table-backed sources.

For table-backed sources:

1. Build an internal temporary table from the descriptor-backed source
   projection.
2. Stream the temporary rows once for validation. The existing selected-value
   conversion and nullability checks remain strict for this phase.
3. Stream the same temporary rows for insertion. Each row is materialized into
   a one-row `planned_insert` view over the shared target descriptors.
4. For auto-increment targets, allocate generated values against a
   statement-local next-value counter. Update the durable counter only after
   all rows succeed and the statement is ready to commit.
5. Validate string key values before physical insertion.
6. Execute the insert row through the existing constraint-aware insert helper
   so duplicate-key, `IGNORE`, and check-constraint paths stay shared.
7. For `INSERT IGNORE` on non-`AUTO_INCREMENT` targets, skip supported
   missing-parent FK child rows before insertion and append the existing
   MyLite warning.
8. Validate child foreign keys after successful writes, matching the existing
   insert path.
9. Commit the statement transaction only after row insertion, auto-increment
   counter persistence, table timestamp update, foreign-key validation, and
   temporary-table cleanup all succeed.

Strict failures roll back inserted rows and do not advance durable
auto-increment counters. Generated successful rows update `LAST_INSERT_ID()` to
the first generated value after commit.

## Diagnostics

Supported strict failures use existing MyLite diagnostics:

- duplicate primary or unique key: `1062 / 23000`;
- missing parent row: `1452 / 23000`;
- unsupported key-bearing `REPLACE ... SELECT`: existing `1064 / 42000`
  unsupported diagnostics;
- unsupported table-backed `INSERT IGNORE ... SELECT` into `AUTO_INCREMENT`
  targets: existing `1064 / 42000` unsupported diagnostics;
- selected-row conversion failures: existing `INSERT ... SELECT` diagnostics;
- physical SQLite or allocation failures: existing internal diagnostics.

Supported `INSERT IGNORE` duplicate and missing-parent rows append warnings and
continue. Unsupported `IGNORE` conversion-demotion cases remain errors for this
phase.

## Performance

The implementation must not materialize the selected row set in C memory. The
row set is materialized once in an internal SQLite temporary table so same-table
source/target statements and unordered limits operate on a stable selected row
set. Validation and insertion stream rows from SQLite. Per-row C allocations
are limited to the existing `planned_value` materialization already required by
the descriptor-owned insert path.

## Tests

Add MySQL-runtime expectation coverage and C runtime tests for:

- primary-key target success, duplicate error, statement rollback, and
  same-table duplicate rollback;
- unique-index target success for single-column, composite, and prefix string
  indexes, duplicate error, duplicate-`NULL` allowance, and `INSERT IGNORE`
  duplicate skip for non-`AUTO_INCREMENT` targets;
- foreign-key child success, missing-parent error rollback, nullable child key,
  and `INSERT IGNORE` missing-parent skip;
- auto-increment omitted/generated, explicit values, mixed explicit/generated
  table-backed input, `NO_AUTO_VALUE_ON_ZERO`, `LAST_INSERT_ID()`, and durable
  counter persistence after reopen, plus deterministic rejection when combined
  with table-backed `INSERT IGNORE ... SELECT`;
- same-table persistent source/target behavior through temporary
  materialization;
- affected rows, warning count, absence of result rows, remaining rows after
  failures, preamble preservation, and independent file-backed handles;
- existing non-key `INSERT ... SELECT`, row-scalar `INSERT ... SELECT`,
  `INSERT IGNORE`, primary-key, unique-index, foreign-key, auto-increment, and
  full check workflow tests.
