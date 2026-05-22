# Baseline Insert Select Non-Strict Coercion

## Summary

This phase extends the existing descriptor-driven `INSERT ... SELECT` path with
the first MySQL-compatible non-strict and `IGNORE` conversion behavior for
selected rows. It builds on the ordinary non-strict DML coercion slice without
changing the parser grammar or public API.

The slice is deliberately narrow:

- table-backed, row-scalar, and compound `INSERT ... SELECT` envelopes already
  admitted by MyLite continue to define the statement surface;
- omitted no-default non-`AUTO_INCREMENT` target columns are adjusted when the
  session is non-strict or the statement is table-backed `INSERT IGNORE ...
  SELECT`;
- selected `NULL` into supported `NOT NULL` target descriptors is adjusted
  under the same policy;
- selected integer-family values outside the target integer descriptor range
  are clipped under the same policy;
- strict non-`IGNORE` behavior remains unchanged.

No new syntax, source expressions, target descriptors, SQLite fork patches, or
public result APIs are introduced.

## Sources And Evidence

- MyLite architecture and standards:
  - `README.md`
  - `AGENTS.md`
  - `docs/architecture/engineering-standards.md`
- Existing MyLite feature specs:
  - `docs/specs/baseline-insert-select-lifecycle/specs.md`
  - `docs/specs/baseline-insert-select-dual-source/specs.md`
  - `docs/specs/baseline-insert-select-union-source/specs.md`
  - `docs/specs/baseline-insert-ignore-lifecycle/specs.md`
  - `docs/specs/baseline-nonstrict-dml-coercion/specs.md`
- Official MySQL 8.4 Reference Manual:
  - `INSERT ... SELECT`: <https://dev.mysql.com/doc/refman/8.4/en/insert-select.html>
  - `INSERT`: <https://dev.mysql.com/doc/refman/8.4/en/insert.html>
  - Server SQL modes: <https://dev.mysql.com/doc/refman/8.4/en/sql-mode.html>
  - Out-of-range handling:
    <https://dev.mysql.com/doc/refman/8.4/en/out-of-range-and-overflow.html>
- MySQL runtime probes are captured in
  `packages/libmylite/tests/mysql_baseline_insert_select_nonstrict_coercion_expectations.sh`
  and verified against MySQL 8.4.9.

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior, public
SQLite APIs, and existing MyLite source code. It does not copy MySQL, MariaDB,
Percona, SQLite implementation internals, or other restrictively licensed
implementation sources.

## MySQL 8.4.9 Runtime Observations

MySQL 8.4.9 establishes these expectations for the supported slice:

- Strict non-`IGNORE` `INSERT ... SELECT` reports `1364 / HY000` when a
  selected row would omit a no-default `NOT NULL` target column.
- Strict non-`IGNORE` `INSERT ... SELECT` reports `1048 / 23000` when a
  selected `NULL` value targets a `NOT NULL` column.
- Strict non-`IGNORE` `INSERT ... SELECT` reports `1264 / 22003` when a
  selected integer value is outside the target integer column range.
- A zero-row source succeeds without omitted-column warnings or errors.
- In non-strict mode, omitted no-default `NOT NULL` target columns are filled
  from implicit type defaults and warn once per omitted column per statement,
  not once per selected row.
- In non-strict mode, selected `NULL` into a `NOT NULL` target column stores
  the implicit descriptor value and warns once per adjusted selected row.
- In non-strict mode, selected integer values outside the target descriptor
  range clip to the descriptor endpoint and warn once per adjusted selected
  row.
- `INSERT IGNORE ... SELECT` performs the same adjustment even while strict SQL
  mode remains active, for the existing table-backed non-`AUTO_INCREMENT`
  `IGNORE` envelope.
- When one statement has omitted-column warnings and selected-row warnings,
  omitted-column warnings appear first, followed by row-order selected-value
  warnings.
- Successful adjusted statements return no result rows, report affected rows as
  inserted rows, and expose warning count through the existing diagnostics
  result conventions.

## Scope

The feature applies only to `INSERT ... SELECT`, not `REPLACE ... SELECT`.

Supported statement envelopes are the ones already admitted before this phase:

- descriptor-backed single-table source `SELECT`;
- existing row-scalar no-source and `FROM DUAL` sources;
- existing unparenthesized compound sources whose branches are already admitted;
- table-backed non-`AUTO_INCREMENT` `INSERT IGNORE ... SELECT`, where that
  shape is already supported.

The new behavior is limited to:

- omitted target columns with no explicit descriptor default, or a nonnullable
  explicit `DEFAULT NULL`;
- selected SQL `NULL` for a nonnullable target;
- selected SQLite integer values for target descriptor columns whose physical
  storage is MyLite's integer-family `INTEGER` representation.

Implicit values match the ordinary non-strict DML and `INSERT IGNORE` policy:

- integer `NOT NULL`: `0`, or the closest range endpoint for adjusted
  out-of-range selected integers;
- nonbinary string `NOT NULL`: empty string;
- binary string `NOT NULL`: zero-padded fixed binary value or zero-length
  variable binary/blob value;
- `BIT NOT NULL`: zero-bit value for the descriptor width;
- `YEAR NOT NULL`: `0000`;
- `DATE NOT NULL`: `0000-00-00`;
- `TIME NOT NULL`: `00:00:00`;
- `DATETIME NOT NULL` / `TIMESTAMP NOT NULL`: `0000-00-00 00:00:00`;
- `DECIMAL`, approximate, `ENUM`, `SET`, and `JSON` `NOT NULL`: the same
  descriptor-owned implicit value used by the existing insert adjustment path;
- nullable dropped-default columns: SQL `NULL`.

## Non-Goals

This phase does not add:

- new `INSERT ... SELECT` grammar;
- selected-row string-to-number, string-to-temporal, temporal-to-string,
  decimal-to-integer, approximate-to-integer, binary, `BIT`, `ENUM`, `SET`,
  JSON, or spatial conversion beyond shapes already admitted before this phase;
- row-scalar or compound-source `INSERT IGNORE ... SELECT`;
- table-backed `INSERT IGNORE ... SELECT` on `AUTO_INCREMENT` targets;
- `REPLACE ... SELECT` warning demotion;
- `ON DUPLICATE KEY UPDATE` changes;
- expression projection, parameters, subqueries beyond the already admitted
  source envelopes, triggers, generated columns, cascades, privilege semantics,
  protocol info strings, or SQLite fork patches.

## Ownership Boundaries

- Public API: unchanged. `mylite_execute()` keeps ownership of public
  validation, result-handle ownership, diagnostics reset, and cleanup.
- Statement context: owns warnings, affected rows, and non-row result
  finalization.
- Session state: owns the handle-local SQL-mode bitset. A statement is strict
  when `STRICT_TRANS_TABLES` or `STRICT_ALL_TABLES` is active; otherwise it is
  non-strict.
- Parser/AST: unchanged. The existing `INSERT ... SELECT` AST envelopes remain
  the admitted surface.
- Analyzer/planner: resolves target/source descriptors and decides whether the
  statement's existing `IGNORE` bit or current SQL mode permits adjustment.
- Catalog: remains authoritative for target nullability, default kind, logical
  type, physical type, visible-column expansion, generated auto-increment
  descriptors, stable physical table names, and key descriptors. This feature
  does not mutate catalog rows, descriptor versions, descriptor caches, catalog
  generation, or `sqlite_schema_generation`.
- Runtime conversion: validates and materializes selected values from
  descriptors, not SQLite schema text. Omitted-column warnings are emitted once
  after the first selected row is observed; selected-row warnings are emitted
  while streaming the materialized selected rows into the target.
- SQLite physical storage: continues to scan/filter/order/limit sources into
  internal temporary storage and perform final physical inserts with quoted
  identifiers and bound values. SQLite does not decide MySQL strictness,
  warning demotion, nullability, or integer clipping.
- Storage/VFS/file format: unchanged. Adjusted rows are normal SQLite payload
  writes inside the shifted payload and must not touch the `.mylite` preamble.

## Grammar

No grammar changes are required. This feature reuses the existing MyLite
`INSERT ... SELECT` grammar. The relevant independently authored Lemon-syntax
shape is:

```lemon
insert_select_statement ::= insert_prefix table_name insert_column_list_opt select_statement.
insert_prefix ::= INSERT insert_modifier_list_opt INTO_opt.
insert_modifier_list_opt ::= .
insert_modifier_list_opt ::= insert_modifier_list.
insert_modifier_list ::= insert_modifier.
insert_modifier_list ::= insert_modifier_list insert_modifier.
insert_modifier ::= LOW_PRIORITY.
insert_modifier ::= HIGH_PRIORITY.
insert_modifier ::= DELAYED.
insert_modifier ::= IGNORE.
```

Runtime support remains narrower than this grammar where existing feature specs
say so: unsupported source envelopes, target descriptors, and `IGNORE` forms
are rejected before physical SQL is generated.

## Semantics

Adjustment is enabled when:

- the statement is an admitted `INSERT IGNORE ... SELECT`; or
- the session does not contain `STRICT_TRANS_TABLES` or `STRICT_ALL_TABLES`.

For strict non-`IGNORE` statements:

- omitted no-default required columns still error only when at least one source
  row is selected;
- selected `NULL` into `NOT NULL` still errors;
- selected integer out-of-range still errors with the selected row number.

For adjusted statements:

- omitted target columns are checked only once the first selected row is
  observed, preserving zero-row source behavior;
- omitted target columns warn once per omitted column per statement;
- selected `NULL` into a nonnullable target warns once per adjusted selected
  row and stores the descriptor implicit value;
- selected integer overflow or signedness violation warns once per adjusted
  selected row and stores the clipped descriptor endpoint;
- successfully inserted adjusted rows contribute to affected rows exactly like
  other inserted rows.

Warnings are retained through the existing MyLite diagnostics object. Ordinary
nondiagnostic result statements may clear or replace them according to the
existing diagnostics-count feature.

## Physical SQL Handling

The existing `INSERT ... SELECT` table-backed path keeps its standard shape:

1. build descriptor-owned source `SELECT` SQL with quoted physical identifiers
   and bound predicate/limit values;
2. materialize the selected source rows into an internal SQLite temporary table;
3. stream that temporary table once for MyLite validation;
4. stream it again for descriptor-driven target-row materialization and
   physical insert.

This feature changes only steps 3 and 4:

- validation accepts selected `NULL` and selected integer range violations when
  adjustment is active;
- target-row materialization converts the accepted value into the same
  `planned_value` representation used by ordinary inserts;
- omitted no-default target materialization uses descriptor implicit defaults
  without emitting per-row omitted-column warnings.

The runtime must not buffer the full selected row set in C memory and must not
delegate MySQL warning or strictness semantics to SQLite.

## Diagnostics

Existing diagnostics are preserved unless adjustment applies:

- `1364 / HY000`: required no-default target column in strict non-`IGNORE`
  statements; warning under adjustment.
- `1048 / 23000`: selected `NULL` into `NOT NULL` in strict non-`IGNORE`
  statements; warning under adjustment.
- `1264 / 22003`: selected integer out of range in strict non-`IGNORE`
  statements; warning under adjustment.
- Existing unsupported-shape diagnostics remain unchanged for unsupported
  source conversions, `AUTO_INCREMENT` `IGNORE` targets, row-scalar/compound
  `IGNORE`, `REPLACE ... SELECT` row-scalar or compound sources, aliases,
  partitions, joins, expressions, and unsupported descriptors.
- Allocation and physical SQLite failures use existing runtime diagnostics and
  preserve statement cleanup.

## Compatibility Documentation

Update `COMPATIBILITY.md`, `docs/compatibility/sql-table-dml.md`,
`docs/compatibility/runtime-session-sql-modes.md`, and
`docs/compatibility/type-system-literals-conversion.md` only for the exact
selected-row and omitted-column behavior described here. Do not claim general
`INSERT ... SELECT` conversion.

## Tests

Add MySQL 8.4.9 expectation coverage for:

- strict errors for omitted no-default, selected `NULL`, and selected integer
  out-of-range;
- zero-row source behavior;
- non-strict ordinary `INSERT ... SELECT` adjustment for omitted defaults,
  selected `NULL`, and integer clipping;
- strict `INSERT IGNORE ... SELECT` adjustment for the existing table-backed
  non-`AUTO_INCREMENT` envelope;
- warning counts, warning rows, affected rows, and inserted values.

Add C runtime coverage for:

- adjusted values and warning rows;
- strict guardrails;
- existing `INSERT IGNORE ... SELECT` lifecycle expectations updated from
  errors to warnings where MySQL adjusts;
- reopen persistence and `.mylite` preamble preservation through adjusted
  insert-select writes;
- focused regression that existing parser/runtime insert-select lifecycle
  tests still pass.
