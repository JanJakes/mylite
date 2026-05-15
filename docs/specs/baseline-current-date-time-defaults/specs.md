# Baseline Current Date And Time Defaults

## Status

This feature closes the narrow DDL gap left by the current date/time function
slice: generated `DATE` defaults based on `CURDATE()` / `CURRENT_DATE` and
generated `TIME` defaults based on `CURTIME()` / `CURRENT_TIME`.

The slice is deliberately small. It supports only parenthesized zero-fractional
current date/time expression defaults on matching `DATE` and `TIME`
descriptors. It does not add general expression defaults, fractional time
function arguments, temporal arithmetic, cross-type coercion of current
timestamp/date/time expressions, generated columns, triggers, or SQLite default
expressions.

## Sources

- MyLite README architecture: `README.md`
- MyLite engineering standards:
  `docs/architecture/engineering-standards.md`
- Runtime handles and statement context:
  `docs/specs/runtime-handles-statement-context/specs.md`
- Current timestamp defaults:
  `docs/specs/baseline-current-timestamp-defaults/specs.md`
- Current date/time functions:
  `docs/specs/baseline-current-date-time-functions/specs.md`
- Baseline `DATE` and `TIME` descriptors under `docs/specs/`
- Official MySQL 8.4 Reference Manual, data type default values:
  https://dev.mysql.com/doc/refman/8.4/en/data-type-defaults.html
- Official MySQL 8.4 Reference Manual, date and time functions:
  https://dev.mysql.com/doc/refman/8.4/en/date-and-time-functions.html
- Official MySQL 8.4 Reference Manual, `CREATE TABLE`:
  https://dev.mysql.com/doc/refman/8.4/en/create-table.html
- MySQL runtime probes are captured in
  `packages/libmylite/tests/mysql_baseline_current_date_time_defaults_expectations.sh`
  and verified against MySQL 8.4.9.

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior, public
SQLite APIs, and existing MyLite source code. It does not copy MySQL, MariaDB,
Percona, SQLite implementation internals, or other restrictively licensed
implementation sources.

## MySQL 8.4.9 Observations

The expectation script records the probes that define this phase. Probes set
`time_zone = '+00:00'` and `timestamp` to deterministic Unix epoch seconds so
MyLite's current fixed-UTC statement-time model can compare values exactly.

- MySQL accepts `DATE DEFAULT (CURDATE())`, `DATE DEFAULT (CURRENT_DATE)`,
  `DATE DEFAULT (CURRENT_DATE())`, `TIME DEFAULT (CURTIME())`,
  `TIME DEFAULT (CURRENT_TIME)`, and `TIME DEFAULT (CURRENT_TIME())`.
- MySQL rejects nonparenthesized `DATE DEFAULT CURRENT_DATE`,
  `DATE DEFAULT CURDATE()`, `TIME DEFAULT CURRENT_TIME`, and
  `TIME DEFAULT CURTIME()` forms. The timestamp/datetime special case for
  unparenthesized `CURRENT_TIMESTAMP` does not extend to date/time defaults.
- `SHOW CREATE TABLE` renders all admitted date defaults as
  `DEFAULT (curdate())` and all admitted time defaults as `DEFAULT (curtime())`.
- `SHOW COLUMNS` and `INFORMATION_SCHEMA.COLUMNS` report `curdate()` or
  `curtime()` as the default value and include `DEFAULT_GENERATED` in `Extra`.
  `DATETIME_PRECISION` is `NULL` for `DATE` and `0` for the current
  zero-fractional `TIME` slice.
- Omitted columns and explicit `DEFAULT` values materialize the current
  statement date/time. Repeating `UPDATE ... SET col = DEFAULT` reports
  changed rows only when the newly materialized value differs from the stored
  value.
- `ALTER TABLE ... ADD COLUMN ... DEFAULT (CURDATE())` and
  `... DEFAULT (CURTIME())` backfill existing rows with the alter statement's
  current date/time while preserving generated-default metadata for future DML.
- `ALTER TABLE ... ALTER [COLUMN] ... SET DEFAULT (CURDATE())` and the
  corresponding time forms replace the descriptor default and affect later DML
  without rewriting existing rows.
- `CREATE TABLE ... LIKE` preserves the generated default metadata.
- MySQL accepts wider expression-default behavior, including cross-type
  coercions such as `INT DEFAULT (CURDATE())`, `DATE DEFAULT (CURTIME())`,
  `TIME DEFAULT (CURDATE())`, `DATE DEFAULT (NOW())`, and fractional
  `CURTIME(0)` / `CURRENT_TIME(0)` defaults. Those depend on broader MySQL
  expression evaluation and conversion, so they are explicitly deferred.

## Scope

Supported:

- persistent and shadowing session temporary base tables using the existing
  descriptor-backed table lifecycle;
- `CREATE TABLE`, `ALTER TABLE ... ADD [COLUMN]`, and
  `ALTER TABLE ... ALTER [COLUMN] ... SET DEFAULT` for:
  - `DATE DEFAULT (CURDATE())`;
  - `DATE DEFAULT (CURRENT_DATE)`;
  - `DATE DEFAULT (CURRENT_DATE())`;
  - `TIME DEFAULT (CURTIME())`;
  - `TIME DEFAULT (CURRENT_TIME)`;
  - `TIME DEFAULT (CURRENT_TIME())`;
- descriptor cloning through `CREATE TABLE ... LIKE`;
- descriptor-backed omitted-column and explicit `DEFAULT` materialization in
  `INSERT ... VALUES`, `INSERT ... SET`, `REPLACE ... VALUES`,
  `REPLACE ... SET`, and single-table `UPDATE`;
- generated-default metadata rendering through `SHOW CREATE TABLE`,
  `SHOW COLUMNS`, `DESCRIBE`, `EXPLAIN table`, `SHOW FULL COLUMNS`, and limited
  `INFORMATION_SCHEMA.COLUMNS`;
- statement-time evaluation through the existing fixed-UTC statement context
  and session `timestamp` override;
- successful supported statements report existing public result conventions,
  MySQL-compatible changed-row affected counts for current DML slices, and
  `warning_count == 0`;
- reopen persistence, independent file-backed handles, descriptor cache
  invalidation, table rename/drop, and `.mylite` preamble preservation through
  the existing storage layer.

Deferred:

- nonparenthesized current date/time defaults;
- `CURTIME(fsp)` / `CURRENT_TIME(fsp)` defaults, including exact `(0)`;
- `CURDATE` / `CURRENT_DATE` argument forms;
- `NOW()` / `CURRENT_TIMESTAMP` expression defaults for `DATE` or `TIME`;
- `CURDATE()` / `CURRENT_DATE` expression defaults for non-`DATE` descriptors;
- `CURTIME()` / `CURRENT_TIME` expression defaults for non-`TIME` descriptors;
- expression defaults for `TEXT`, `BLOB`, `JSON`, geometry, generated columns,
  or arbitrary scalar expressions beyond existing integer-expression defaults;
- expression-default references to other columns, subqueries, parameters,
  variables, stored or loadable functions, interval arithmetic, casts, mutable
  time zones, fractional seconds, triggers, cascades, privilege semantics, and
  protocol-grade default metadata.

## Ownership Boundaries

- Public API: unchanged. `mylite_execute()` owns misuse behavior, result
  ownership, and cleanup.
- Statement context: owns the statement-time snapshot. Generated date/time
  defaults read the same snapshot used by `NOW()` / `CURRENT_TIMESTAMP`,
  `CURDATE()`, and `CURTIME()`.
- Lexer/parser/AST: already admits current date/time scalar expressions. This
  feature relies on the existing parenthesized `DEFAULT (expr)` grammar and
  does not admit nonparenthesized date/time defaults.
- Analyzer/planner/runtime: validates that the generated current-date default
  targets a `DATE` descriptor and the generated current-time default targets a
  `TIME` descriptor, then materializes canonical text values before binding or
  generating SQLite physical DDL.
- Catalog: remains the durable authority for logical type, nullability,
  default kind, column order, and table identity. This feature requires a
  catalog schema bump and two new generated-default descriptor kinds. SQLite
  schema text is never metadata authority.
- Result and introspection builders: render descriptor metadata into MySQL-like
  `SHOW`, `DESCRIBE`, and `INFORMATION_SCHEMA.COLUMNS` rows.
- SQLite physical storage: stores materialized `DATE` and `TIME` values as
  canonical text in MyLite-generated rowid tables. MyLite does not rely on
  SQLite date/time functions, default expressions, triggers, or optional SQL
  syntax.
- Storage/VFS: unchanged. This feature writes only inside the shifted SQLite
  payload and must not touch the MyLite preamble.

## Supported SQL Grammar

Independently authored MyLite Lemon-syntax snippets:

```lemon
current_date_default_expression:
    LPAREN current_date_value RPAREN

current_time_default_expression:
    LPAREN current_time_value RPAREN

column_default:
    DEFAULT NULL
  | DEFAULT column_default_value
  | DEFAULT current_date_default_expression
  | DEFAULT current_time_default_expression

alter_table_set_default_statement:
    ALTER TABLE table_name ALTER column_keyword_opt identifier SET DEFAULT NULL
  | ALTER TABLE table_name ALTER column_keyword_opt identifier SET DEFAULT column_default_value
  | ALTER TABLE table_name ALTER column_keyword_opt identifier SET DEFAULT
        current_date_default_expression
  | ALTER TABLE table_name ALTER column_keyword_opt identifier SET DEFAULT
        current_time_default_expression
```

In the existing parser shape, `column_default_value ::= LPAREN expression
RPAREN` can carry the admitted date/time values because `expression` already
contains `current_date_value` and `current_time_value`. Runtime validation must
therefore distinguish these generated date/time defaults from the existing
integer-expression default subset.

The grammar must continue to reject:

- `DEFAULT CURRENT_DATE`, `DEFAULT CURRENT_DATE()`, `DEFAULT CURDATE()`;
- `DEFAULT CURRENT_TIME`, `DEFAULT CURRENT_TIME()`, `DEFAULT CURTIME()`;
- date argument forms and time fractional forms;
- arbitrary expression defaults not already supported by earlier slices.

## Runtime Semantics

Generated default descriptor kinds:

- `CURRENT_DATE`: no stored default text or integer value; canonical rendering
  uses `curdate()` for MySQL metadata and `DEFAULT (curdate())` for
  `SHOW CREATE TABLE`;
- `CURRENT_TIME`: no stored default text or integer value; canonical rendering
  uses `curtime()` for MySQL metadata and `DEFAULT (curtime())` for
  `SHOW CREATE TABLE`;
- both are generated defaults and therefore contribute `DEFAULT_GENERATED` to
  the `Extra` column in relevant introspection surfaces.

Materialization:

1. Resolve the current statement epoch from the existing statement context and
   session `timestamp` override.
2. Convert the epoch to UTC broken-down time.
3. Materialize `CURRENT_DATE` defaults as canonical `YYYY-MM-DD` text.
4. Materialize `CURRENT_TIME` defaults as canonical `HH:MM:SS` text.
5. Bind the text value to generated SQLite DML statements, or quote a concrete
   snapshot value only for the internal SQLite `ALTER TABLE ... ADD COLUMN`
   backfill default.

DML behavior:

- omitted-column and explicit `DEFAULT` paths materialize a fresh statement
  value for each statement;
- multi-row inserts and replaces use one statement value for all rows, matching
  existing statement-time behavior;
- single-table `UPDATE ... SET d = DEFAULT` / `SET tm = DEFAULT` follows the
  existing changed-row policy after materialization;
- `LIMIT 0`, no-match updates, and failed statements do not rewrite rows;
- supported generated date/time defaults never produce `NULL`, so nullable and
  `NOT NULL` targets share the same successful materialization behavior.

`ALTER TABLE ... ADD COLUMN`:

- the descriptor stores the generated default kind;
- the physical SQLite column is added with a concrete quoted text default
  calculated from the alter statement's timestamp, so existing rows are
  backfilled with the correct MySQL value without relying on SQLite dynamic
  defaults;
- future DML uses the descriptor-generated value, not the SQLite default text.

## Diagnostics

Successful supported statements return `warning_count == 0`.

Required diagnostics:

- syntax errors and unsupported nonparenthesized defaults: existing parser
  syntax diagnostics;
- unsupported current date/time argument forms: existing parser/native-function
  diagnostics where already available;
- `CURRENT_DATE` generated defaults on non-`DATE` descriptors: deterministic
  invalid-default diagnostics using the current invalid-default class;
- `CURRENT_TIME` generated defaults on non-`TIME` descriptors: deterministic
  invalid-default diagnostics using the current invalid-default class;
- duplicate column default attributes: existing duplicate-attribute diagnostic;
- generated date/time defaults on text/blob/json/generic expression-default
  targets outside this scope: deterministic invalid-default or unsupported
  diagnostics, not SQLite coercion;
- unknown schema/table/column names, reserved `_mylite_*` names, unsupported
  object kinds, physical SQLite failures, allocation failures, and public API
  misuse: existing diagnostics.

## Compatibility Documentation

Update `COMPATIBILITY.md`, `docs/compatibility/functions-temporal.md`,
`docs/compatibility/sql-table-ddl.md`,
`docs/compatibility/sql-table-dml.md`, and
`docs/compatibility/type-system-literals-conversion.md` with limited wording.
Do not claim general expression defaults, fractional time defaults, date/time
coercions outside matching descriptors, mutable time zones, generated columns,
triggers, or full protocol metadata.

## Tests

Add a focused C runtime test, preferably
`packages/libmylite/tests/runtime_current_date_time_defaults_test.c`, and
register it with a dotted CTest name.

Coverage must include:

- MySQL 8.4.9 expectation script version check and deterministic `SET
  timestamp` probes;
- `CREATE TABLE` current date/time defaults for all admitted syntax variants;
- nonparenthesized date/time default rejection;
- `SHOW CREATE TABLE`, `SHOW COLUMNS`, `SHOW FULL COLUMNS`, and
  `INFORMATION_SCHEMA.COLUMNS` metadata;
- omitted-column and explicit `DEFAULT` materialization for `INSERT`,
  `INSERT ... SET`, `REPLACE`, and single-table `UPDATE`;
- changed-row and no-op affected-row counts for `UPDATE ... SET col = DEFAULT`;
- nullable and `NOT NULL` descriptor targets;
- `ALTER TABLE ... ADD COLUMN` backfill and future DML behavior;
- `ALTER TABLE ... ALTER [COLUMN] ... SET DEFAULT`;
- `CREATE TABLE ... LIKE` descriptor cloning;
- deterministic rejection of date generated defaults on non-`DATE` targets and
  time generated defaults on non-`TIME` targets;
- deliberately deferred wider MySQL behavior such as `INT DEFAULT (CURDATE())`,
  `DATE DEFAULT (NOW())`, `TIME DEFAULT (CURRENT_TIMESTAMP)`,
  `CURRENT_TIME(0)`, and temporal arithmetic;
- reopen persistence, independent handles, table rename/drop, descriptor cache
  behavior, and `.mylite` preamble preservation;
- existing current date/time scalar, current timestamp default, `DATE`, `TIME`,
  DML, parser, statement-context, storage, VFS, and compatibility tests.

Verification before completion:

1. `cmake --build --preset dev`
2. Focused CTest entries for parser, current date/time functions, current
   timestamp defaults, current date/time defaults, date, time, DML, alter-table
   default, and catalog/persistence lifecycle tests.
3. `packages/libmylite/tests/mysql_baseline_current_date_time_defaults_expectations.sh`
4. `cmake --workflow --preset check`
