# Baseline Insert Select String Truncation

## Summary

This phase extends MyLite's existing descriptor-driven `INSERT ... SELECT`
selected-row conversion with the `CHAR` / `VARCHAR` truncation behavior already
supported by ordinary DML literals. It keeps the current `INSERT ... SELECT`
grammar and source envelopes, and only changes how selected text values are
validated and materialized for compatible `CHAR` / `VARCHAR` target
descriptors.

The slice is deliberately narrow:

- table-backed, row-scalar, and compound `INSERT ... SELECT` source envelopes
  already admitted by MyLite remain the full statement surface;
- source and target columns must already be compatible string-family
  descriptors for table-backed selected columns;
- selected scalar string literals remain the only row-scalar string source;
- selected `CHAR` and `VARCHAR` values are canonicalized through MyLite-owned
  target descriptors before SQLite insertion;
- non-strict mode and currently admitted table-backed `INSERT IGNORE ...
  SELECT` truncate nonspace overflow with warning `1265`;
- strict `VARCHAR` trailing-space overflow from row-scalar values succeeds with
  note `1265`;
- strict nonspace overflow remains an error and leaves the statement rolled
  back.

No parser grammar, public API, catalog format, file format, or SQLite fork
patch is introduced.

## Sources And Evidence

- MyLite architecture and standards:
  - `README.md`
  - `AGENTS.md`
  - `docs/architecture/engineering-standards.md`
- Existing MyLite specs:
  - `docs/specs/baseline-insert-select-lifecycle/specs.md`
  - `docs/specs/baseline-insert-select-dual-source/specs.md`
  - `docs/specs/baseline-insert-select-union-source/specs.md`
  - `docs/specs/baseline-insert-select-nonstrict-coercion/specs.md`
  - `docs/specs/baseline-nonstrict-string-truncation/specs.md`
  - `docs/specs/baseline-char-type/specs.md`
  - `docs/specs/baseline-varchar-type/specs.md`
- Official MySQL 8.4 Reference Manual:
  - `INSERT ... SELECT`: <https://dev.mysql.com/doc/refman/8.4/en/insert-select.html>
  - `INSERT`: <https://dev.mysql.com/doc/refman/8.4/en/insert.html>
  - `CHAR` and `VARCHAR`: <https://dev.mysql.com/doc/refman/8.4/en/char.html>
  - Server SQL modes: <https://dev.mysql.com/doc/refman/8.4/en/sql-mode.html>
- MySQL runtime probes are captured in
  `packages/libmylite/tests/mysql_baseline_insert_select_string_truncation_expectations.sh`
  and verified against MySQL 8.4.9.

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior, public
SQLite APIs, and existing MyLite source code. It does not copy MySQL, MariaDB,
Percona, SQLite implementation internals, or other restrictively licensed
implementation sources.

## MySQL 8.4.9 Runtime Observations

MySQL 8.4.9 establishes these expectations for the supported slice:

- Strict row-scalar `INSERT ... SELECT 'abcd'` into `VARCHAR(3)` fails with
  `1406 / 22001`.
- Strict table-backed `INSERT ... SELECT varchar_col` from a wider `VARCHAR`
  descriptor into `VARCHAR(3)` fails with `1265 / 01000` and rolls back the
  statement.
- Strict table-backed `INSERT ... SELECT char_col` or `text_col` into a shorter
  `VARCHAR` target fails with `1406 / 22001` for nonspace overflow.
- Strict table-backed `INSERT ... SELECT text_col` into a shorter `VARCHAR`
  target succeeds with note `1265 / 01000` when only trailing ASCII spaces are
  removed.
- Strict row-scalar `INSERT ... SELECT 'ab  '` into `VARCHAR(3)` stores
  `'ab '` and records note `1265`.
- Strict table-backed `INSERT ... SELECT varchar_col` where the selected value
  has only excess trailing spaces fails with `1265 / 01000` and rolls back.
- Strict row-scalar and table-backed `INSERT ... SELECT` into `CHAR(3)` fail
  with `1406 / 22001` when nonspace characters exceed the declared length.
- `CHAR` trailing spaces are silently trimmed for admitted row-scalar and
  table-backed values.
- Non-strict `INSERT ... SELECT` truncates selected nonspace-overlength
  `CHAR` / `VARCHAR` values to the target descriptor length, stores the
  canonical target value, and records warning `1265` per adjusted selected row.
- Strict admitted `INSERT IGNORE ... SELECT` follows the same truncation and
  warning behavior for the current table-backed non-`AUTO_INCREMENT` envelope.
- UTF-8 `CHAR` / `VARCHAR` lengths are measured in characters, not bytes, and
  truncation preserves complete code points.
- Zero-row sources succeed without string conversion diagnostics.
- Successful adjusted statements return no result rows and report affected
  rows as inserted row counts through the normal non-query result conventions.

## Scope

This feature applies only to `INSERT ... SELECT`, not `REPLACE ... SELECT`,
ordinary `INSERT`, `UPDATE`, or duplicate-key update assignments. Ordinary DML
literal truncation is covered by the earlier non-strict string truncation
phase.

Supported statement envelopes are exactly the ones already admitted before
this phase:

- descriptor-backed single-table source `SELECT`;
- existing row-scalar no-source and `FROM DUAL` sources;
- existing unparenthesized compound `UNION` / `UNION ALL` sources whose
  branches are already admitted;
- table-backed non-`AUTO_INCREMENT` `INSERT IGNORE ... SELECT`, where that
  shape is already supported.

The affected target descriptors are limited to:

- `CHAR` / `CHAR(n)` descriptors in the current supported range;
- `VARCHAR(n)` descriptors in the current supported range.

The affected selected values are limited to:

- selected table-backed string-family descriptor values already deemed source
  and target compatible by the current `INSERT ... SELECT` planner;
- selected row-scalar string literals already admitted by the current
  row-scalar projection subset;
- selected compound string outputs from the existing table-backed and
  row-scalar branch subsets.

## Non-Goals

This phase does not add:

- new grammar or new source envelopes;
- `TEXT` family truncation demotion;
- implicit string conversion from numeric, temporal, JSON, binary, `BIT`,
  `ENUM`, `SET`, spatial, or arbitrary expression sources;
- `REPLACE ... SELECT`, `INSERT ... SELECT ... ON DUPLICATE KEY UPDATE`, or
  duplicate-update assignment truncation changes;
- `INSERT IGNORE ... SELECT` beyond the existing admitted table-backed
  non-`AUTO_INCREMENT` envelope;
- string-to-number, string-to-temporal, character-set conversion, collation
  comparison, `PAD_CHAR_TO_FULL_LENGTH`, protocol metadata, or general
  expression evaluation changes;
- SQLite fork patches.

## Ownership Boundaries

- Public API: unchanged. `mylite_execute()` owns public misuse validation,
  diagnostics reset, result-handle ownership, and cleanup.
- Statement context and diagnostics: own warning/note/error recording,
  affected rows, and non-row result finalization.
- Session state: owns the handle-local SQL-mode bitset. A statement is strict
  when `STRICT_TRANS_TABLES` or `STRICT_ALL_TABLES` is active; otherwise it is
  non-strict.
- Parser/AST: unchanged. Existing `INSERT ... SELECT` AST shapes remain the
  admitted syntax.
- Analyzer/planner: continues to resolve target/source descriptors, default
  schema behavior, `IGNORE`, and compatible source/target descriptor pairs.
- Catalog: remains authoritative for logical type, declared length,
  nullability, defaults, stable physical table names, physical column names,
  keys, and generated auto-increment state. This feature does not mutate
  descriptors, catalog generation, descriptor caches, catalog rows, or
  `sqlite_schema_generation`.
- Runtime conversion: validates selected values against target descriptors and
  materializes adjusted `CHAR` / `VARCHAR` values before binding.
- SQLite physical storage: remains the row storage and scan/filter/order/limit
  engine. Table and compound sources keep using SQLite temporary storage, then
  MyLite streams rows through descriptor conversion one row at a time. SQLite
  does not decide MySQL string length, strictness, or diagnostics.
- Storage/VFS/file format: unchanged. Inserted rows are ordinary SQLite
  payload writes inside the shifted file and must not touch the `.mylite`
  preamble.

## Grammar

No grammar changes are required. The relevant independently authored
Lemon-syntax sketch remains:

```lemon
insert_select_statement ::= insert_prefix table_name insert_column_list_opt select_statement.
insert_prefix ::= INSERT insert_modifier_list_opt INTO_opt.
insert_modifier_list_opt ::= .
insert_modifier_list_opt ::= insert_modifier_list.
insert_modifier_list ::= insert_modifier.
insert_modifier_list ::= insert_modifier_list insert_modifier.
insert_modifier ::= IGNORE.
```

Runtime support remains narrower than the parser where earlier specs say so:
unsupported source envelopes, target descriptors, `IGNORE` forms, and
expression shapes are rejected before physical SQL is generated.

## Conversion Semantics

`CHAR` / `VARCHAR` descriptor lengths are interpreted as UTF-8 character
counts. Conversion must preserve complete UTF-8 code points.

For selected `VARCHAR` values:

- in-range values are stored unchanged;
- row-scalar values whose only excess characters are ASCII spaces are
  truncated to the target descriptor length and record note `1265`, including
  in strict mode;
- table-backed `VARCHAR` values whose only excess characters are ASCII spaces
  fail with `1265 / 01000` in strict non-`IGNORE` mode;
- table-backed `CHAR` / `TEXT` values whose only excess characters are ASCII
  spaces are truncated to the target descriptor length and record note `1265`,
  including in strict mode;
- nonspace overflow truncates with warning `1265` when adjustment is enabled;
- otherwise, row-scalar nonspace overflow fails with `1406 / 22001`, while
  table-backed `VARCHAR` source overflow fails with `1265 / 01000` and
  table-backed `CHAR` / `TEXT` source overflow fails with `1406 / 22001`.

For selected `CHAR` values:

- trailing ASCII spaces are trimmed before storage;
- if non-trailing-space characters fit the target descriptor length,
  conversion succeeds silently;
- nonspace overflow truncates and then trims trailing ASCII spaces when
  adjustment is enabled, recording warning `1265`;
- otherwise, nonspace overflow fails with `1406 / 22001`.

Adjustment is enabled when:

- the session is non-strict; or
- the statement is an already admitted `INSERT IGNORE ... SELECT`.

Zero-row sources do not validate or materialize selected values and therefore
produce no string conversion diagnostics.

## Physical SQL Handling

The existing generated SQL shape is preserved:

- table-backed and compound sources are materialized into a MyLite-owned
  temporary SQLite table, with compound rows retaining their branch-origin
  marker for source-type-sensitive validation;
- the temporary result is validated through a prepared SQLite `SELECT`;
- rows are streamed through MyLite-owned target conversion and then inserted
  through a prepared descriptor-built physical `INSERT`;
- physical table and column names are stable catalog names and quoted;
- adjusted strings are bound as SQLite `TEXT` parameters after conversion;
- selected string bytes are never interpolated into generated SQL.

This feature does not use SQLite's type affinity, constraints, triggers, or
optional grammar to implement MySQL string semantics.

## Diagnostics

Supported diagnostics are:

- `1406 / 22001`: strict row-scalar `CHAR` / `VARCHAR` nonspace overflow, and
  strict table-backed `CHAR` / `TEXT` nonspace overflow into `CHAR` /
  `VARCHAR` targets;
- `1265 / 01000` as an error: strict table-backed `VARCHAR` overflow;
- warning `1265 / 01000`: adjusted nonspace-overlength selected `CHAR` /
  `VARCHAR` value;
- note `1265 / 01000`: strict row-scalar `VARCHAR` and table-backed
  `CHAR` / `TEXT` excess trailing spaces into a `VARCHAR` target;
- existing unsupported-expression diagnostics for values outside this slice;
- existing allocation, physical SQLite, and public API diagnostics.

Warning row numbers use MySQL's selected-row numbering for the rows that reach
the admitted `INSERT ... SELECT` source after filtering, ordering, limiting,
and compound duplicate handling.

## Tests

Fast C runtime tests must cover:

- non-strict table-backed selected `VARCHAR` truncation;
- non-strict table-backed selected `CHAR` truncation;
- non-strict selected UTF-8 truncation at character boundaries;
- non-strict row-scalar selected `CHAR` / `VARCHAR` truncation;
- strict row-scalar `VARCHAR` trailing-space note and `CHAR` silent trimming;
- strict overlength guardrails for row-scalar `VARCHAR`, table-backed
  `VARCHAR`, and row-scalar/table-backed `CHAR`;
- strict `INSERT IGNORE ... SELECT` table-backed truncation;
- zero-row source with no diagnostics;
- stored values, warning rows, affected rows, absence of result rows, reopen
  persistence, and preamble preservation.

The MySQL expectation script records the corresponding MySQL 8.4.9 behavior.

## Compatibility Documentation

Update:

- `COMPATIBILITY.md`;
- `docs/compatibility/sql-table-dml.md`;
- `docs/compatibility/type-system-literals-conversion.md`;
- `docs/compatibility/runtime-session-sql-modes.md` if needed for the
  session-mode effects.

Do not claim full `INSERT ... SELECT` conversion, full string expression
support, `TEXT` truncation, duplicate-key assignment truncation, `REPLACE ...
SELECT`, character-set conversion, or collation parity.
