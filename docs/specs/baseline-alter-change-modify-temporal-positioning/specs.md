# Baseline ALTER CHANGE/MODIFY Temporal Positioning

## Status

This feature extends the existing descriptor-driven
`ALTER TABLE ... CHANGE [COLUMN]` and `ALTER TABLE ... MODIFY [COLUMN]`
single-action lifecycle. It adds the next schema-migration slice needed by
WordPress-style DDL:

- optional `FIRST` and `AFTER column_name` positioning for the current
  `CHANGE`/`MODIFY` replacement subset;
- `DATETIME` and `TIMESTAMP` replacement descriptors in the same
  `CHANGE`/`MODIFY` path, including supported defaults and
  `ON UPDATE CURRENT_TIMESTAMP`;
- descriptor, `SHOW`, and `INFORMATION_SCHEMA.COLUMNS.ORDINAL_POSITION`
  ordering updates after successful repositioning.

The feature remains intentionally narrow. It is not general multi-action
`ALTER TABLE`, does not support indexed-table replacement, and does not copy or
reuse MySQL implementation grammar.

## Sources

- MyLite README architecture: `README.md`
- MyLite engineering standards:
  `docs/architecture/engineering-standards.md`
- Existing `CHANGE COLUMN` and `MODIFY COLUMN` specs:
  `docs/specs/baseline-alter-table-change-column/specs.md` and
  `docs/specs/baseline-alter-table-modify-column/specs.md`
- Current timestamp/default specs:
  `docs/specs/baseline-current-timestamp-defaults/specs.md`,
  `docs/specs/baseline-datetime-type/specs.md`, and
  `docs/specs/baseline-timestamp-type/specs.md`
- Existing parser, catalog, result, storage, and runtime lifecycle specs under
  `docs/specs/`
- MySQL 8.4 Reference Manual, `ALTER TABLE`:
  https://dev.mysql.com/doc/refman/8.4/en/alter-table.html
- MySQL 8.4 Reference Manual, automatic initialization and updating for
  `TIMESTAMP` and `DATETIME`:
  https://dev.mysql.com/doc/refman/8.4/en/timestamp-initialization.html
- MySQL 8.4 Reference Manual, `INFORMATION_SCHEMA.COLUMNS`:
  https://dev.mysql.com/doc/refman/8.4/en/information-schema-columns-table.html
- MySQL 8.4 Reference Manual, keywords and reserved words:
  https://dev.mysql.com/doc/refman/8.4/en/keywords.html
- MySQL runtime probes are captured in
  `packages/libmylite/tests/mysql_baseline_alter_change_modify_temporal_positioning_expectations.sh`
  and verified against MySQL 8.4.9.

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior, public
SQLite APIs, and existing MyLite source code. It does not copy MySQL, MariaDB,
Percona, SQLite implementation internals, or other restrictively licensed
implementation sources.

## MySQL 8.4.9 Observations

The expectation script records the probes that define this phase.

- `CHANGE [COLUMN]` and `MODIFY [COLUMN]` accept a trailing `FIRST` or
  `AFTER col_name` position clause after the replacement column definition.
- Repositioning changes visible column order in `SHOW COLUMNS`,
  `SHOW CREATE TABLE`, `SELECT *`, implicit row-value `INSERT`, and
  `INFORMATION_SCHEMA.COLUMNS.ORDINAL_POSITION`.
- Repositioning a column without changing its type or attributes reports
  `ROW_COUNT() = 0` and `@@warning_count = 0`.
- Type replacement continues to report the number of rows copied by MySQL's
  table rebuild path. A combined type change and reposition reports the copied
  row count.
- `AFTER col_name` resolves against the resulting column list after removing
  the changed column. `AFTER` therefore cannot name the same column being
  modified or the old name of a renamed column; MySQL reports `1054 / 42S22`
  with `Unknown column '<name>' in '<table>'`.
- Existing `NULL` values rejected by a replacement `NOT NULL` definition report
  `1138 / 22004`, `Invalid use of NULL value`.
- `DATETIME` and `TIMESTAMP` replacements admit supported zero-fractional
  `DEFAULT CURRENT_TIMESTAMP` and `ON UPDATE CURRENT_TIMESTAMP` clauses and
  render them through `SHOW COLUMNS`, `SHOW CREATE TABLE`, and
  `INFORMATION_SCHEMA.COLUMNS.EXTRA`.

## Scope

Supported:

- persistent MyLite base tables only;
- one `CHANGE [COLUMN]` or one `MODIFY [COLUMN]` action only;
- the current unindexed-table limitation from existing `CHANGE`/`MODIFY`
  execution: no primary-key, secondary-index, or CHECK-constrained target
  tables;
- unqualified and schema-qualified target table names using the existing
  selected/default schema policy;
- unqualified old, replacement, modified, and `AFTER` column names;
- `FIRST` and `AFTER` remain nonreserved identifier tokens outside the narrow
  position-clause slots, including table names, column names, replacement
  names, and `AFTER after_column_name` operands;
- optional `FIRST`;
- optional `AFTER after_column_name`;
- existing integer-family, limited `CHAR`, `VARCHAR`, national `CHAR`, and
  national `VARCHAR` replacements already supported by `CHANGE`/`MODIFY`;
- `DATETIME` and `TIMESTAMP` replacements when the existing column and target
  column are both supported temporal text descriptors;
- supported `DATETIME`/`TIMESTAMP` nullability, canonical string defaults,
  `DEFAULT NULL`, zero-fractional current-timestamp defaults, and
  `ON UPDATE CURRENT_TIMESTAMP` attributes already admitted by the column
  definition grammar;
- descriptor-driven row validation for existing integer, text, `DATETIME`,
  `TIMESTAMP`, and `NULL` values;
- descriptor ordinal replacement and physical table rebuild for every
  repositioning operation;
- no-row DDL result objects, MySQL-compatible affected-row counts for the
  admitted subset, and zero warnings for successful in-range operations.

Deferred:

- positioning for `ALTER TABLE ... ADD COLUMN`;
- multiple alter actions or mixed `CHANGE`/`MODIFY` plus other actions;
- table-qualified column references in replacement or position clauses;
- generated, invisible, auto-increment, primary-key, unique-key, foreign-key,
  CHECK-constrained, or indexed-table replacement;
- `DATE`, `TIME`, `YEAR`, decimal, approximate, binary string, `BIT`, `TEXT`,
  `ENUM`, `SET`, or JSON replacements beyond what existing `CHANGE`/`MODIFY`
  already supports;
- temporal fractional precision, time-zone conversion during `TIMESTAMP`
  replacement, relaxed temporal parsing, expression defaults beyond existing
  current timestamp forms, algorithms, locks, metadata locks, privilege
  semantics, triggers, cascades, or SQLite fork changes.

## Ownership Boundaries

- Public API: unchanged. `mylite_execute()` continues to own public misuse
  validation, result ownership, and failure cleanup.
- Statement context: owns diagnostics, warning count, affected rows, and the
  statement boundary.
- Lexer/parser/AST: admits only the explicit syntax subset and stores a
  position node. It does not resolve descriptors, inspect the catalog, or build
  SQLite SQL.
- Analyzer/planner: resolves schema, table, old/replacement/modified columns,
  the optional `AFTER` column, object kind, duplicate replacement names,
  supported replacement type, rowid-rebuild feasibility, and target ordinal
  order from MyLite descriptors before generated SQLite SQL exists.
- Catalog: remains the durable authority for logical type, physical type,
  nullability, defaults, auto-update metadata, visibility, column id, and
  ordinal positions. This feature updates column ordinals in the catalog inside
  the same mutation as the descriptor replacement.
- Result and introspection builders: render descriptor order through existing
  `SHOW`, `DESCRIBE`, and limited `INFORMATION_SCHEMA.COLUMNS` paths.
- SQLite physical storage: stores rows in MyLite-generated rowid tables. MyLite
  performs descriptor-built physical rebuilds with quoted identifiers and
  stable physical table names. SQLite schema text is not metadata authority.
- Storage/VFS: unchanged. The feature writes only inside the shifted SQLite
  payload and must not touch the `.mylite` preamble.

## Supported SQL Grammar

Independently authored MyLite Lemon-syntax snippets:

```lemon
alter_table_modify_column_statement ::=
    ALTER TABLE table_name MODIFY column_keyword_opt column_definition column_position_opt.

alter_table_change_column_statement ::=
    ALTER TABLE table_name CHANGE column_keyword_opt identifier column_definition column_position_opt.

column_position_opt ::= .
column_position_opt ::= FIRST.
column_position_opt ::= AFTER identifier.
```

The existing `column_definition` grammar provides the admitted replacement
types and attributes. This feature only adds the optional position clause after
that definition.

## Resolution And Ordering

Unqualified target tables use the selected schema. Schema-qualified targets use
the explicit schema and do not require a selected schema. Missing selected
schema, unknown schema, unknown table, reserved `_mylite_*` schema/table names,
and unsupported object kinds keep the existing `CHANGE`/`MODIFY` diagnostics.

The old `CHANGE` column and `MODIFY` column are resolved by descriptor name
using the current case-insensitive catalog comparison policy. Replacement-name
duplicate detection for `CHANGE` also uses descriptors and ignores the column
being changed.

`FIRST` sets the changed column's target ordinal to `1`.

`AFTER after_column_name` resolves `after_column_name` against the resulting
column list after removing the changed column. If no descriptor remains with
that name, MyLite reports MySQL error `1054`, SQLSTATE `42S22`, and message
`Unknown column '<after_column_name>' in '<table>'`. This matches MySQL's
behavior when `AFTER` names the column being changed or the old name of a
renamed column.

All other columns keep their relative order. The changed descriptor keeps its
`column_id`, table id, descriptor creation generation, visibility, and existing
row values.

## Temporal Replacement Semantics

`DATETIME` and `TIMESTAMP` replacement support is limited to existing
descriptor-backed temporal text columns. The target descriptor can be
`DATETIME` or `TIMESTAMP`:

- `DATETIME` replacement accepts existing canonical `DATETIME` and `TIMESTAMP`
  stored text, including values admitted by the current SQL mode policy.
- `TIMESTAMP` replacement accepts only existing canonical timestamp text within
  the current fixed-UTC MyLite `TIMESTAMP` range. Existing out-of-range or
  invalid text fails before catalog mutation.
- `NULL` remains valid only for nullable target descriptors.
- Supported defaults and `ON UPDATE CURRENT_TIMESTAMP` metadata come from the
  existing column-definition planner and are persisted in the same column
  descriptor row as other replacement attributes.

This slice does not convert integer, string-family, or other non-temporal
stored values into temporal values. Those broader MySQL conversions remain
future work.

## Physical SQLite Handling

Positioning always requires a descriptor-built physical table rebuild because
SQLite has no public column-reorder operation. MyLite must not use writable
schema editing or arbitrary SQLite pass-through.

The rebuild uses the existing `CHANGE`/`MODIFY` rowid-table invariant:

1. resolve and validate descriptors;
2. build the target descriptor column array in target ordinal order;
3. start a MyLite catalog mutation;
4. validate existing row values inside the mutation;
5. update the changed column descriptor and every changed ordinal in catalog
   rows;
6. create a temporary physical table from the target descriptor order;
7. copy rows by explicit descriptor column list, selecting the original
   physical column name for the changed column when it was renamed;
8. drop the old physical table;
9. rename the temporary physical table back to the stable physical table name;
10. update table identity, commit the mutation, and increment the
    connection-local SQLite schema generation.

Every generated SQLite identifier must be quoted. No user SQL literal text is
interpolated into generated SQL.

## Result Semantics

Successful statements return through existing non-row DDL result conventions:

- result column count `0`;
- result row count `0`;
- `@@warning_count = 0`;
- `ROW_COUNT()` / `affected_rows` follows MySQL for the admitted subset.

Exact descriptor no-ops with no position change preserve catalog rows,
descriptor versions, catalog generation, physical SQLite schema, and
`sqlite_schema_generation`.

Name, default, `ON UPDATE`, nullability, and position-only replacements report
`affected_rows = 0` when no type-domain rebuild count is reported by MySQL for
the admitted subset. Type replacements report the validated/copied row count.

## Diagnostics

Diagnostics to cover:

- syntax outside admitted grammar: existing parse diagnostic, `1064 / 42000`;
- missing default schema: `1046 / 3D000`;
- unknown schema: `1049 / 42000`;
- unknown table: `1146 / 42S02`;
- reserved target schema, table, old/replacement/modified/position column:
  existing MyLite reserved-name diagnostic;
- unsupported object kind: existing unsupported persistent-base-table
  diagnostic before generated SQLite SQL;
- unknown changed column: `1054 / 42S22`;
- duplicate replacement column: `1060 / 42S21`;
- unknown `AFTER` column, including `AFTER` self: `1054 / 42S22`;
- existing `NULL` value for target `NOT NULL`: `1138 / 22004`,
  `Invalid use of NULL value`;
- existing integer out of target range: `1264 / 22003`;
- invalid existing temporal value or out-of-range `TIMESTAMP`: current
  deterministic temporal diagnostics;
- unsupported replacement type or conversion path: deterministic MyLite
  unsupported diagnostic;
- physical SQLite failures, allocation failures, and public misuse through
  existing policies.

## Compatibility Documentation

Update `COMPATIBILITY.md` and `docs/compatibility/sql-table-ddl.md` for only:

- limited `FIRST`/`AFTER` positioning on `CHANGE`/`MODIFY`;
- limited temporal descriptor replacements with supported current timestamp
  defaults and `ON UPDATE CURRENT_TIMESTAMP`;
- corrected `NULL` to `NOT NULL` replacement diagnostic if the implementation
  updates the existing shared validator.

Do not overclaim full ALTER positioning, `ADD COLUMN` positioning, indexed
table replacement, multi-action ALTER, fractional temporal precision,
arbitrary type conversion, generated columns, trigger semantics, full metadata
locking, algorithms, locks, or privilege checks.

## Tests

Coverage must include:

- parser acceptance for `CHANGE ... FIRST`, `CHANGE ... AFTER col`,
  `MODIFY ... FIRST`, and `MODIFY ... AFTER col`;
- parser rejection for table-qualified position names and combined actions;
- MySQL expectation script version check and behavior probes;
- integer same-type reposition with `ROW_COUNT() = 0`;
- integer type-change plus position with copied-row affected count;
- `TIMESTAMP` rename/default/`ON UPDATE` plus `AFTER`;
- `DATETIME` to `TIMESTAMP` replacement plus `AFTER`;
- `MODIFY` temporal same-type plus `FIRST`;
- `SHOW COLUMNS`, `SHOW CREATE TABLE`, `SELECT *`, and
  `INFORMATION_SCHEMA.COLUMNS.ORDINAL_POSITION` after reposition;
- unknown `AFTER` column and `AFTER` self diagnostics;
- existing `NULL` to `NOT NULL` diagnostics;
- unsupported non-temporal conversion such as integer to timestamp;
- schema-qualified and unqualified targets;
- persistence after reopen and independent file-backed handles where practical;
- `.mylite` preamble preservation;
- zero-initialized cleanup for any new planner state.

Verification before completion:

1. `cmake --build --preset dev`
2. Focused CTest entries for parser, alter change/modify, current timestamp
   defaults, datetime, timestamp, show columns/full columns, show create,
   information schema columns, row values, update, delete, and table lifecycle.
3. `packages/libmylite/tests/mysql_baseline_alter_change_modify_temporal_positioning_expectations.sh`
4. `cmake --workflow --preset check`
