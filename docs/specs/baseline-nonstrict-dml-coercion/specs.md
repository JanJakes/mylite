# Baseline Non-Strict DML Coercion

## Summary

This phase gives the existing descriptor-driven `INSERT`, `REPLACE`, and
single-table `UPDATE` paths their first ordinary non-strict SQL-mode coercion
behavior. It is intentionally narrower than full MySQL non-strict conversion:
when the session does not include `STRICT_TRANS_TABLES` or `STRICT_ALL_TABLES`,
MyLite will materialize implicit descriptor defaults for omitted no-default
non-`AUTO_INCREMENT` columns and DML `DEFAULT` values that have no explicit
descriptor default, and will adjust matched `UPDATE ... SET not_null_column =
NULL` assignments.

The main goal is common application bootstrap compatibility after statements
such as `SET sql_mode = ''` or `SET sql_mode = 'NO_ENGINE_SUBSTITUTION'`.
MyLite keeps strict-mode behavior unchanged for the default MySQL 8.4.9 mode.

## Sources And Evidence

- MyLite architecture and standards:
  - `README.md`
  - `AGENTS.md`
  - `docs/architecture/engineering-standards.md`
- Existing MyLite feature specs:
  - `docs/specs/baseline-sql-mode-session-state/specs.md`
  - `docs/specs/baseline-empty-insert-values/specs.md`
  - `docs/specs/baseline-dml-default-keyword-values/specs.md`
  - `docs/specs/baseline-insert-ignore-lifecycle/specs.md`
  - `docs/specs/baseline-zero-temporal-sql-modes/specs.md`
- Official MySQL 8.4 Reference Manual:
  - `INSERT`: <https://dev.mysql.com/doc/refman/8.4/en/insert.html>
  - `REPLACE`: <https://dev.mysql.com/doc/refman/8.4/en/replace.html>
  - `UPDATE`: <https://dev.mysql.com/doc/refman/8.4/en/update.html>
  - Data type default values:
    <https://dev.mysql.com/doc/refman/8.4/en/data-type-defaults.html>
  - Server SQL modes: <https://dev.mysql.com/doc/refman/8.4/en/sql-mode.html>
- MySQL runtime probes are captured in
  `packages/libmylite/tests/mysql_baseline_nonstrict_dml_coercion_expectations.sh`
  and verified against MySQL 8.4.9.

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior, public
SQLite APIs, and existing MyLite source code. It does not copy MySQL, MariaDB,
Percona, SQLite implementation internals, or other restrictively licensed
implementation sources.

## MySQL 8.4.9 Runtime Observations

MySQL 8.4.9 establishes these expectations for this slice:

- The default session mode includes `STRICT_TRANS_TABLES`; strict behavior for
  omitted no-default non-`AUTO_INCREMENT` `NOT NULL` columns remains an error.
- `SET sql_mode = ''` and `SET sql_mode = 'NO_ENGINE_SUBSTITUTION'` disable
  strict conversion for ordinary DML.
- In non-strict mode, `INSERT ... VALUES`, `INSERT ... SET`,
  `REPLACE ... VALUES`, and `REPLACE ... SET` insert implicit type defaults for
  omitted non-`AUTO_INCREMENT` `NOT NULL` columns that have no explicit default
  and record warning `1364` once per omitted column per statement.
- Omitted `AUTO_INCREMENT` targets keep the existing generated-value path in
  non-strict modes such as `NO_AUTO_VALUE_ON_ZERO`; explicit `0` stores zero
  only because that SQL mode says so, not because of implicit-default
  adjustment.
- In non-strict mode, DML `DEFAULT` in an `INSERT` or `REPLACE` value position
  stores the implicit type default when the target `NOT NULL` column has no
  explicit default and records warning `1364`.
- In non-strict mode, a nullable column whose explicit default was removed with
  `ALTER TABLE ... ALTER column DROP DEFAULT` stores SQL `NULL` for omitted
  insert values or DML `DEFAULT` and records warning `1364`.
- In non-strict mode, explicit `NULL` in an ordinary `INSERT` or `REPLACE`
  value position for a `NOT NULL` column still fails with `1048 / 23000`.
  `IGNORE` remains the path that demotes that insert-side explicit `NULL`.
- In non-strict mode, a matched `UPDATE ... SET not_null_column = NULL` stores
  the implicit type default and records warning `1048` once per matched row per
  adjusted assignment target.
- In non-strict mode, a matched `UPDATE ... SET column = DEFAULT` for a column
  without an explicit default stores the implicit type default, or SQL `NULL`
  for a nullable dropped-default column, and records warning `1364` once per
  matched row per adjusted assignment target.
- `UPDATE` conversion for `NULL` or `DEFAULT` is evaluated only when the update
  matches rows. No-match updates and `LIMIT 0` updates record no warnings.
- Successful adjusted statements return through the normal non-row DML result
  shape. Affected rows keep MySQL changed-row behavior for `UPDATE`.
- `DEFAULT(column_name)` still fails for no-default columns even in non-strict
  mode; this slice admits only the DML `DEFAULT` keyword already supported by
  MyLite.

## Scope

This feature supports the existing statement shapes only:

- `INSERT [LOW_PRIORITY | HIGH_PRIORITY | DELAYED] [INTO] table ... VALUES ...`
- `INSERT [LOW_PRIORITY | HIGH_PRIORITY | DELAYED] [INTO] table SET ...`
- `REPLACE [LOW_PRIORITY | DELAYED] [INTO] table ... VALUES ...`
- `REPLACE [LOW_PRIORITY | DELAYED] [INTO] table SET ...`
- single-table `UPDATE table SET ... [WHERE ...] [ORDER BY ...] [LIMIT ...]`

For those shapes, the new behavior is limited to sessions where neither
`STRICT_TRANS_TABLES` nor `STRICT_ALL_TABLES` is active:

- omitted no-explicit-default non-`AUTO_INCREMENT` columns in `INSERT` and
  `REPLACE`;
- DML `DEFAULT` in `INSERT`, `REPLACE`, and matched `UPDATE` assignments when
  the target column has no explicit descriptor default;
- explicit `NULL` in matched `UPDATE` assignments to `NOT NULL` target columns.

The implicit values are the same descriptor-owned values already used by
MyLite's `INSERT IGNORE` adjustment policy:

- numeric, approximate, and decimal `NOT NULL`: zero;
- `YEAR NOT NULL`: `0000`;
- `DATE NOT NULL`: `0000-00-00`;
- `TIME NOT NULL`: `00:00:00`;
- `DATETIME NOT NULL` and `TIMESTAMP NOT NULL`: `0000-00-00 00:00:00`;
- nonbinary string `NOT NULL`: empty string;
- `BINARY NOT NULL`: zero-padded bytes to the declared fixed width;
- `VARBINARY` and BLOB-family `NOT NULL`: zero-length bytes;
- `BIT NOT NULL`: zero-bit value for the descriptor width;
- `ENUM NOT NULL`: first label;
- `SET NOT NULL`: empty string;
- `JSON NOT NULL`: JSON `null`;
- nullable dropped-default columns: SQL `NULL`.

## Non-Goals

This feature does not add:

- `UPDATE IGNORE`, `REPLACE IGNORE`, or broader `IGNORE` behavior;
- non-strict explicit `NULL` demotion for ordinary `INSERT` or `REPLACE`;
- full expression coercion, expression defaults, `DEFAULT(column_name)` changes,
  generated columns, triggers, privileges, or protocol info strings;
- selected-row warning demotion for `INSERT ... SELECT`;
- string-to-number, string-to-temporal, or arbitrary expression conversion;
- overlength nonspace text truncation, full decimal/float warning parity, or
  complete MySQL non-strict conversion coverage;
- SQLite default execution, SQLite conflict algorithms as the compatibility
  authority, or SQLite fork patches.

## Ownership Boundaries

- Public API: unchanged. `mylite_execute()` continues to own public validation,
  result-handle ownership, diagnostics snapshot setup, and failure cleanup.
- Statement context: owns adjusted warnings, affected rows, and the existing
  non-row result shape.
- Session state: owns the handle-local SQL-mode bitset that decides whether
  strict mode is active. The mode remains nonpersistent.
- Parser/AST: unchanged. The current grammar already admits the supported
  DML `DEFAULT` and `NULL` value positions.
- Analyzer/planner: owns deciding whether a statement is in strict or
  non-strict mode, resolving target descriptors, and materializing implicit
  values before SQLite SQL is generated.
- Catalog: remains authoritative for column default kind, nullability, logical
  type, physical type, stable physical table names, and generated values. This
  DML feature does not mutate catalog rows, descriptor versions, catalog
  generation, descriptor caches, or `sqlite_schema_generation`.
- SQLite physical storage: receives only descriptor-built SQL with quoted
  identifiers and bound values. It does not decide MySQL strictness, warning
  demotion, or implicit defaults.
- Storage/VFS/file format: unchanged. Adjusted DML writes only inside the
  shifted SQLite payload and must not touch the `.mylite` preamble.

## Grammar

No new grammar is required. The feature reuses the current MyLite grammar for
`INSERT`, `REPLACE`, and `UPDATE`. The relevant Lemon-syntax sketch is:

```lemon
insert_value ::= NULL.
insert_value ::= DEFAULT.

replace_value ::= insert_value.

update_assignment ::= qualified_identifier EQUAL update_value.
update_value ::= NULL.
update_value ::= DEFAULT.
```

Runtime narrows those parsed shapes exactly as the existing DML features do:
only the currently supported table targets, assignment targets, predicates,
ordering, and limit forms participate.

## Semantics

Strictness is evaluated from the connection's current session SQL mode at
execution time:

- strict if `STRICT_TRANS_TABLES` or `STRICT_ALL_TABLES` is active;
- non-strict otherwise.

Default MySQL 8.4.9 mode remains strict. `SET sql_mode = DEFAULT` restores that
strict mode through the existing SQL-mode session-state feature.

For `INSERT` and `REPLACE`:

- omitted no-default non-`AUTO_INCREMENT` columns fail as before in strict mode;
- omitted no-default non-`AUTO_INCREMENT` columns materialize implicit
  descriptor values in non-strict mode and append warning `1364`;
- omitted `AUTO_INCREMENT` columns continue to materialize generated values
  through the existing auto-increment planner;
- warning `1364` is recorded once per omitted column per statement, not once
  per inserted row;
- explicit DML `DEFAULT` for a no-default column records warning `1364` for
  each value occurrence that is actually planned;
- explicit `NULL` into `NOT NULL` remains an error unless the statement is
  `INSERT IGNORE`, preserving current MySQL behavior and MyLite scope.

For `UPDATE`:

- conversion of assignment values happens only after the statement is known to
  match at least one row, preserving no-match and `LIMIT 0` behavior;
- strict mode preserves current errors for `NULL` into `NOT NULL` and
  no-default DML `DEFAULT`;
- non-strict `NULL` into `NOT NULL` appends warning `1048` and stores the
  implicit descriptor value;
- non-strict DML `DEFAULT` for a no-default column appends warning `1364` and
  stores the implicit descriptor value or SQL `NULL` for nullable
  dropped-default columns;
- affected rows report rows whose stored values changed after conversion;
- warnings are still recorded when a matched row converts to the current stored
  value and therefore changes zero rows.

## Diagnostics

Supported warning diagnostics:

| Case | Code | SQLSTATE | Message |
| --- | --- | --- | --- |
| missing explicit default adjusted | `1364` | `HY000` | `Field '<column>' doesn't have a default value` |
| matched update `NULL` into `NOT NULL` adjusted | `1048` | `23000` | `Column '<column>' cannot be null` |

Strict-mode errors reuse existing MyLite diagnostics:

- `1364 / HY000` for missing default;
- `1048 / 23000` for `NULL` into `NOT NULL`;
- existing syntax, name, shape, unsupported grammar, reserved target, unknown
  schema/table/column, unsupported object kind, physical SQLite failure,
  allocation failure, and public API misuse diagnostics.

Successful supported adjusted statements may have a positive warning count.
Supported in-range statements that do not require adjustment keep
`warning_count == 0`.

## Physical SQLite Handling

The implementation remains a MyLite wrapper/translation layer. No public
SQLite extension function or SQLite fork hook is needed.

Generated SQL shape is unchanged:

- physical table names come from MyLite table descriptors;
- identifiers are quoted by MyLite's existing dynamic-string helpers;
- implicit default values are bound as prepared-statement parameters, not
  interpolated SQL literals;
- text and blob implicit values use existing descriptor conversion helpers;
- catalog descriptors, `SHOW CREATE TABLE`, `SHOW COLUMNS`,
  `INFORMATION_SCHEMA`, and table status metadata remain unchanged except for
  ordinary row-write timestamps already updated by DML.

## Tests

Fast C tests must cover:

- strict default-mode errors for omitted no-default non-`AUTO_INCREMENT`
  columns and matched `UPDATE ... SET column = NULL` / `DEFAULT`;
- `SET sql_mode = ''` non-strict adjustment for `INSERT ... VALUES`,
  empty-row inserts, `INSERT ... SET`, `REPLACE ... VALUES`, and
  `REPLACE ... SET`;
- `SET sql_mode = 'NO_ENGINE_SUBSTITUTION'` as a non-strict mode example;
- non-strict modes still generate omitted `AUTO_INCREMENT` values, including
  after an explicit zero stored under `NO_AUTO_VALUE_ON_ZERO`;
- non-strict explicit `NULL` into `NOT NULL` ordinary `INSERT` and `REPLACE`
  still error;
- non-strict matched `UPDATE ... SET column = NULL` warning `1048` and stored
  implicit values, including multi-row warning cardinality;
- non-strict matched `UPDATE ... SET column = DEFAULT` warning `1364` and
  stored implicit values;
- no-match and `LIMIT 0` updates produce zero warnings;
- failed adjusted updates preserve per-matched-row warnings, roll back physical
  row changes, and retain the final error diagnostic;
- nullable dropped-default columns warn and store SQL `NULL`;
- affected rows, warning count, `SHOW WARNINGS`, absence of result rows,
  close/reopen persistence, and `.mylite` preamble preservation.

MySQL expectation probes must verify the same user-visible SQL behavior against
MySQL 8.4.9 before implementation is considered complete.
