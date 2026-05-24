# Baseline Insert Ignore Select Auto Increment

## Status

This feature extends the descriptor-backed table-source `INSERT IGNORE ...
SELECT` path so it can target persistent base tables with the currently
supported indexed `AUTO_INCREMENT` descriptors. It builds on the existing
insert-select lifecycle, keyed-target support, selected-row materialization,
duplicate-key warning demotion, foreign-key child warning demotion, and
auto-increment value generation.

This is not full MySQL `INSERT IGNORE ... SELECT`. It keeps the current source
`SELECT`, target descriptor, selected-value conversion, warning-demotion, and
auto-increment fidelity limits.

## Sources

- MyLite README architecture: `README.md`
- MyLite engineering standards:
  `docs/architecture/engineering-standards.md`
- Baseline insert-select lifecycle:
  `docs/specs/baseline-insert-select-lifecycle/specs.md`
- Baseline insert-select keyed targets:
  `docs/specs/baseline-insert-select-keyed-targets/specs.md`
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
records runtime probes for the admitted behavior. Observed MySQL 8.4.9 behavior:

- `INSERT IGNORE INTO auto_table(non_auto_col) SELECT ...` inserts
  nonconflicting selected rows and skips duplicate unique-key rows.
  `ROW_COUNT()` reports rows actually inserted, `@@warning_count` includes the
  skipped duplicate rows, and `LAST_INSERT_ID()` reports the first inserted
  generated value.
- `INSERT IGNORE` into an auto-increment foreign-key child target skips selected
  rows with missing parents, inserts later valid rows, records warnings, and
  reports the first inserted generated value.
- Explicit positive auto-increment values selected from the source table keep
  their explicit values when inserted. `NULL` and zero generate values unless
  `NO_AUTO_VALUE_ON_ZERO` makes zero explicit, matching the existing
  non-`IGNORE` insert-select slice.
- When every selected generated row is skipped, `ROW_COUNT()` is `0` and
  `LAST_INSERT_ID()` remains unchanged.
- InnoDB's durable next auto-increment value after bulk `INSERT ... SELECT`
  can include reserve gaps that are not the same as the visible inserted row
  ids. MyLite's current insert-select auto-increment slice already does not
  emulate those storage-engine reserve gaps. This feature does not expand that
  gap emulation; it verifies row contents, affected rows, warnings, and
  `LAST_INSERT_ID()` for the admitted `IGNORE` cases.

## Scope

The implementation must admit table-backed:

- `INSERT IGNORE INTO target [(column_list)] SELECT ... FROM source` when the
  target is a persistent base table with one supported indexed
  `AUTO_INCREMENT` descriptor;
- generated auto-increment values for omitted, selected `NULL`, or selected
  zero target values under the existing `NO_AUTO_VALUE_ON_ZERO` policy;
- explicit positive auto-increment values selected from the source table;
- duplicate-key warning demotion for the current primary-key and unique-index
  descriptor subset, including non-auto unique keys on auto-increment targets;
- missing-parent warning demotion for current supported foreign-key child
  descriptors;
- same-table persistent source/target materialization through the existing
  internal SQLite temporary table;
- affected rows, warning count, and `LAST_INSERT_ID()` behavior for inserted
  generated rows in the admitted subset;
- file-backed persistence and unchanged `.mylite` preamble behavior.

The source `SELECT` grammar, target column-list grammar, selected-value
conversion rules, and unsupported source shapes remain those of the existing
table-backed `INSERT ... SELECT` slices.

## Non-Goals

This feature does not implement:

- row-scalar or compound-source `INSERT IGNORE ... SELECT`;
- `INSERT IGNORE ... SELECT ... ON DUPLICATE KEY UPDATE`;
- exact InnoDB bulk auto-increment reserve gaps after `INSERT ... SELECT`, for
  either `IGNORE` or non-`IGNORE` forms;
- selected-row warning demotion beyond the currently implemented omitted
  targets, selected `NULL`, selected integer range, selected
  `CHAR`/`VARCHAR`/text-family truncation, duplicate-key rows, and
  supported missing-parent rows;
- broader source expressions, joins, CTEs, row constructors, target
  partitions, target aliases, triggers, privileges, generated-column writes
  outside existing DEFAULT-only behavior, recursive cascades, or protocol info
  string reporting.

## Ownership Boundary

- Public API is unchanged. `mylite_execute()` returns the existing non-row
  result object shape for successful DML.
- Statement context owns diagnostics reset, warning count, affected rows,
  previous `ROW_COUNT()`, `LAST_INSERT_ID()`, and cleanup on failure.
- Parser/AST ownership is unchanged. No grammar is added; this feature removes
  a planner-level shape rejection for grammar already accepted.
- Analyzer/planner resolves target and source descriptors from MyLite catalog
  metadata, not SQLite schema text. Auto-increment, key, and foreign-key
  descriptors remain catalog-owned.
- Runtime keeps the existing SQLite temporary table materialization for the
  selected source rows, then streams one selected row at a time through the
  shared MyLite insert machinery. This preserves the current duplicate-key,
  foreign-key, selected-value conversion, generated-value, warning, and cleanup
  code paths.
- SQLite owns source scanning, filtering, ordering, limiting, temporary-table
  storage, physical row insertion, and physical unique-index checks. MyLite owns
  compatibility validation, descriptor resolution, generated value selection,
  diagnostics, and durable descriptor updates.
- Storage/VFS owns the `.mylite` preamble and shifted SQLite payload boundary.
  This feature writes normal SQLite payload pages and does not touch the
  preamble.

## Grammar

No grammar extension is required. The existing MyLite subset remains:

```lemon
insert_select_statement(A) ::=
    INSERT(I) insert_modifier_list_opt(M) INTO table_name(T)
    insert_column_list_opt(C) select_statement(S). {
    A = mylite_sql_parser_make_insert_select_statement(state, I, T, C, S, M, NULL);
}
```

Runtime planning continues to reject row-scalar and compound `IGNORE` sources
for this slice.

## Runtime Semantics

Planning initializes the normal `planned_insert` target with loaded
auto-increment, key, and foreign-key metadata. For table-backed sources:

1. Materialize the selected source rows into an internal SQLite temporary table.
2. Validate selected rows using the current insert-select descriptor conversion
   rules.
3. Stream selected rows from the temporary table and materialize one row into
   the shared insert row plan.
4. Generate an auto-increment value only through the existing MyLite
   auto-increment planning helper when the target column is omitted, selected
   as `NULL`, or selected as zero while `NO_AUTO_VALUE_ON_ZERO` is disabled.
5. Insert through the shared constraint-aware insert helper.
6. On duplicate-key conflicts under `IGNORE`, append the existing duplicate-key
   warning, reset the SQLite statement, and continue without increasing
   affected rows or replacing `LAST_INSERT_ID()`.
7. On supported missing-parent foreign-key rows under `IGNORE`, append the
   existing warning and skip the row before physical insertion.
8. Persist the durable auto-increment counter using the existing MyLite
   successful-row counter policy. Exact InnoDB reserve-gap emulation remains
   deferred.
9. Touch table update time and validate post-write child foreign keys through
   existing side-effect hooks.
10. Commit the statement transaction only after insertion, descriptor updates,
    table timestamp update, foreign-key validation, and temp-table cleanup
    succeed.

Strict failures roll back inserted rows and descriptor side effects. Skipped
`IGNORE` rows do not count as affected rows. The first successfully inserted
generated row sets `LAST_INSERT_ID()`.

## Diagnostics

Supported skipped rows append existing warnings:

- duplicate primary or unique key rows: duplicate-key warning with MySQL-shaped
  key/value text;
- missing parent row for a supported foreign-key child descriptor:
  no-referenced-row warning.

Unsupported or failing cases keep existing diagnostics:

- row-scalar `INSERT IGNORE ... SELECT`: `1064 / 42000` unsupported diagnostic;
- compound-source `INSERT IGNORE ... SELECT`: `1064 / 42000` unsupported
  diagnostic;
- selected-row conversion failures outside admitted warning demotion:
  existing insert-select diagnostics;
- unknown schema/table/column names, reserved targets, unsupported object
  kinds, and read-only system schemas: existing resolver diagnostics;
- allocation failure: existing out-of-memory diagnostic;
- physical SQLite failure without a specific compatibility diagnostic:
  existing physical row error.

## Performance

This feature does not materialize selected rows in C memory. The existing
SQLite temporary table provides a stable selected row set for same-table
source/target statements. Validation and insertion stream rows from SQLite and
reuse a single prepared physical insert statement.

The feature does not add SQLite fork patches. It uses MyLite-side translation,
catalog descriptors, prepared SQLite statements, and existing public SQLite
APIs.

## Tests

Coverage must include:

- generated auto-increment `INSERT IGNORE ... SELECT` with duplicate-key skips
  and a later inserted generated row;
- generated auto-increment foreign-key child target with missing-parent skips
  and a later inserted generated row;
- every-selected-row skipped cases preserving zero affected rows and no
  inserted generated id;
- explicit positive, `NULL`, and zero selected auto-increment values, including
  `NO_AUTO_VALUE_ON_ZERO`;
- affected rows, warning count, no row result set, and `LAST_INSERT_ID()`;
- close/reopen persistence and unchanged `.mylite` preamble invariants;
- the existing row-scalar and compound-source `IGNORE` rejections.

Expected user-visible behavior must be verified against MySQL 8.4.9. The
deferred bulk auto-increment reserve-gap behavior must not be asserted as
MySQL-compatible in tests or documentation.
