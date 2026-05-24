# Baseline INSERT Values UNIX_TIMESTAMP Arithmetic

## Goal

Support the common WordPress-style DML value shape:

```sql
INSERT INTO wp_options (...) VALUES (..., UNIX_TIMESTAMP() + 3600, ...)
```

This is a narrow extension of the existing descriptor-driven `INSERT` /
`REPLACE` row-value conversion path. It is not a general DML expression engine.

## Sources

- Official MySQL 8.4 Reference Manual, `INSERT` statement:
  <https://dev.mysql.com/doc/refman/8.4/en/insert.html>
- Official MySQL 8.4 Reference Manual, date and time functions:
  <https://dev.mysql.com/doc/refman/8.4/en/date-and-time-functions.html>
- Official MySQL 8.4 Reference Manual, arithmetic operators:
  <https://dev.mysql.com/doc/refman/8.4/en/arithmetic-functions.html>
- Existing MyLite designs:
  - `docs/specs/baseline-row-values-lifecycle/specs.md`
  - `docs/specs/baseline-unix-timestamp-function/specs.md`
  - `docs/specs/baseline-scalar-arithmetic-projection/specs.md`
  - `docs/specs/baseline-insert-set-lifecycle/specs.md`
  - `docs/specs/baseline-insert-on-duplicate-key-update/specs.md`
- Observed MySQL 8.4.9 runtime behavior captured by
  `packages/libmylite/tests/mysql_baseline_insert_values_unix_timestamp_arithmetic_expectations.sh`.

The MyLite grammar and implementation are independently authored from official
documentation, observed MySQL 8.4.9 behavior, public SQLite APIs, and existing
MyLite code. Do not copy MySQL, MariaDB, Percona, SQLite implementation
internals, or restrictively licensed sources.

## MySQL 8.4.9 Runtime Observations

Runtime probes establish the behavior used by this baseline:

- In `INSERT ... VALUES`, no-argument `UNIX_TIMESTAMP()` uses the statement
  timestamp. With `SET timestamp = 1704067200`, every occurrence in a multi-row
  statement observes `1704067200`.
- `UNIX_TIMESTAMP() + signed_integer_literal` and `UNIX_TIMESTAMP() -
  signed_integer_literal` produce signed integer arithmetic results.
- `UNIX_TIMESTAMP() + NULL` and `UNIX_TIMESTAMP() - NULL` produce `NULL`.
- Assigning a `NULL` arithmetic result to a `NOT NULL` target fails with
  `1048 / 23000`; `INSERT IGNORE` demotes it to a warning and stores the
  descriptor implicit value for the target.
- Assigning the arithmetic result to an integer column uses normal column range
  conversion. A value outside `INT` range fails with `1264 / 22003` under strict
  mode.
- Arithmetic overflow before assignment fails with `1690 / 22003`.
- `INSERT ... SET`, `REPLACE ... VALUES`, `REPLACE ... SET`, and admitted
  `ON DUPLICATE KEY UPDATE` assignments accept the same value expressions in
  MySQL.
- MySQL treats `UNIX_TIMESTAMP() + NULL` assigned explicitly to an
  `AUTO_INCREMENT` target as a generated `AUTO_INCREMENT` value. MyLite defers
  that shape until explicit generated-target expression handling is specified.
- Successful supported statements produce `warning_count == 0`, no row result
  set, normal affected-row counts, and existing generated `AUTO_INCREMENT`
  behavior for omitted auto-increment targets.

MySQL accepts much broader expressions, including function arguments,
column-to-column expressions in duplicate updates, multiplication, division,
user variables, parameters, subqueries, and string/decimal/float coercion. Those
remain deferred in this slice.

## Supported Surface

MyLite supports the following value forms anywhere the existing `insert_value`
nonterminal is used by current supported DML:

```sql
UNIX_TIMESTAMP()
UNIX_TIMESTAMP() + integer_literal
UNIX_TIMESTAMP() + +integer_literal
UNIX_TIMESTAMP() + -integer_literal
UNIX_TIMESTAMP() + NULL
UNIX_TIMESTAMP() - integer_literal
UNIX_TIMESTAMP() - +integer_literal
UNIX_TIMESTAMP() - -integer_literal
UNIX_TIMESTAMP() - NULL
```

The supported statement envelopes are the current descriptor-driven:

- `INSERT ... VALUES`;
- `INSERT ... SET`;
- `REPLACE ... VALUES`;
- `REPLACE ... SET`;
- currently admitted `INSERT ... ON DUPLICATE KEY UPDATE` assignment values
  using the same `insert_value` grammar.

The supported target columns are non-`AUTO_INCREMENT` integer-family descriptor
targets whose physical storage is currently MyLite's integer path, including
signed and unsigned `INT`, `INTEGER`, and `BIGINT` within MyLite's signed-64
physical storage envelope. `NULL` arithmetic results follow the existing
nullable, strict, non-strict, and `IGNORE` handling for explicit `NULL` values.
Omitted `AUTO_INCREMENT` targets keep the existing generated-id behavior;
explicit `UNIX_TIMESTAMP()` arithmetic assigned to the `AUTO_INCREMENT` target
itself remains deferred because MySQL treats expression-`NULL` results there as
generated values.

The result is evaluated once per expression during MyLite planning/conversion,
then bound through existing prepared SQLite `INSERT` / duplicate-update
statements as a planned integer or `NULL` value. SQLite does not parse or
execute the user expression.

## Deferred Surface

This slice intentionally does not support:

- `UNIX_TIMESTAMP(value)` with arguments in DML values;
- `1 + UNIX_TIMESTAMP()`, multiplication, division, modulo, bitwise operators,
  logical operators, comparisons, or nested general scalar arithmetic in DML
  values;
- string, decimal, float, bit, hex, parameter, user-variable, system-variable,
  subquery, column, aggregate, or arbitrary function operands;
- string, decimal, approximate, temporal, `YEAR`, `BIT`, binary string, `ENUM`,
  `SET`, `JSON`, or spatial target conversion for these expressions;
- explicit `AUTO_INCREMENT` target assignment with these expressions;
- `UPDATE` assignments, defaults, generated columns, predicates, `ORDER BY`,
  `GROUP BY`, `HAVING`, indexes, constraints, triggers, privilege semantics, or
  arbitrary SQLite pass-through.

## Grammar

The existing `expression` grammar already handles `UNIX_TIMESTAMP()` and
arithmetic for scalar `SELECT` contexts. This phase keeps `INSERT` grammar
narrow by extending only `insert_value`:

```lemon
insert_value(A) ::= insert_unix_timestamp_value(B).

insert_unix_timestamp_value(A) ::= insert_unix_timestamp_now(B).
insert_unix_timestamp_value(A) ::= insert_unix_timestamp_now(B) PLUS(T) insert_unix_timestamp_delta(C).
insert_unix_timestamp_value(A) ::= insert_unix_timestamp_now(B) MINUS(T) insert_unix_timestamp_delta(C).

insert_unix_timestamp_now(A) ::= UNIX_TIMESTAMP(T) LPAREN RPAREN(R).

insert_unix_timestamp_delta(A) ::= INTEGER(T).
insert_unix_timestamp_delta(A) ::= PLUS(P) INTEGER(T).
insert_unix_timestamp_delta(A) ::= MINUS(M) INTEGER(T).
insert_unix_timestamp_delta(A) ::= NULL(T).
```

These snippets describe MyLite's supported subset, not MySQL's full grammar.

## Runtime Semantics

Planning/conversion:

1. Resolve the target table and columns using the existing descriptor-driven
   schema/table/column policy for `INSERT`, `REPLACE`, and duplicate updates.
2. Detect the admitted `UNIX_TIMESTAMP()` value expression before ordinary
   literal conversion.
3. Reject non-integer target descriptors with a MyLite-specific unsupported
   diagnostic.
4. Evaluate no-argument `UNIX_TIMESTAMP()` using the existing statement-context
   timestamp helper.
5. Evaluate the optional signed integer or `NULL` delta through MyLite-owned
   signed-64 arithmetic. If the delta is `NULL`, the expression result is
   `NULL`.
6. Convert the resulting signed integer or `NULL` through the existing
   descriptor integer/null conversion policy for the target column, preserving
   strict, non-strict, and `IGNORE` behavior.
7. Bind the resulting planned value with existing SQLite prepared statement
   parameter binding.

Successful expression values report no warnings. Diagnostics produced by target
conversion, such as out-of-range integer assignment or `NULL` into `NOT NULL`,
match the existing descriptor DML path.

## Ownership Boundaries

- Public API: unchanged. Callers use `mylite_execute()` and existing result
  accessors.
- Statement context: remains the source of no-argument `UNIX_TIMESTAMP()` time.
- Lexer/parser/AST: reuse the existing `UNIX_TIMESTAMP` token and AST nodes;
  add only narrow `insert_value` productions.
- Analyzer/planner: descriptor-driven table and column resolution remains
  authoritative. The value expression is reduced to a planned integer or `NULL`
  before SQLite SQL generation.
- Catalog: read-only for value conversion. This feature does not mutate table
  descriptors, descriptor versions, catalog generation, or
  `sqlite_schema_generation`.
- Result builder: successful statements continue to return non-row results with
  affected rows and warning counts through existing conventions.
- Storage/VFS/file format: unchanged. The `.mylite` preamble and shifted SQLite
  payload invariants are preserved.
- SQLite: receives stable physical table names, quoted identifiers, and bound
  values. No SQLite fork patch is required.

## Diagnostics

Required diagnostics:

- unsupported target descriptor:
  `INSERT UNIX_TIMESTAMP arithmetic supports only integer targets`;
- explicit `AUTO_INCREMENT` target descriptor:
  `INSERT UNIX_TIMESTAMP arithmetic does not yet support AUTO_INCREMENT targets`;
- unsupported operand, overflow, or syntax outside the admitted grammar:
  deterministic parse or unsupported-expression diagnostics;
- wrong target conversion, including integer out of range:
  existing descriptor conversion diagnostics such as `1264 / 22003`;
- `NULL` into `NOT NULL`: existing `1048 / 23000`, with current non-strict or
  `IGNORE` warning adjustment where already supported;
- arithmetic overflow: existing MyLite scalar arithmetic `1690 / 22003`
  diagnostic;
- allocation failure: existing `MYLITE_NOMEM` behavior;
- physical SQLite failure: existing wrapped SQLite diagnostics;
- public API misuse: no public API changes.

Supported in-range statements return `warning_count == 0`.

## Performance And Storage

The expression is statement-local and source-free, so MyLite evaluates it once
per value occurrence while planning row values. It does not materialize table
data, add scans, allocate temporary SQLite tables, or ask SQLite to parse the
expression. Final row writes use the same prepared SQLite insert/update shapes
as existing descriptor-driven DML.

## Tests

Add fast C tests and a MySQL-runtime expectation artifact covering:

- `INSERT ... VALUES` with `UNIX_TIMESTAMP()`, plus, minus, signed delta forms,
  multi-row statement-time consistency, affected rows, `LAST_INSERT_ID()`, and
  zero warnings;
- nullable `NULL` arithmetic results and `NULL` into `NOT NULL` strict and
  `IGNORE` behavior;
- `INSERT ... SET`, `REPLACE ... VALUES`, and admitted duplicate-key update
  assignment values;
- `REPLACE ... SET` assignment values and the documented MySQL-only
  `AUTO_INCREMENT` expression-`NULL` behavior that remains deferred in MyLite;
- target integer range errors and signed-64 arithmetic overflow diagnostics;
- unsupported target descriptors and unsupported expression shapes;
- reopen persistence and `.mylite` preamble preservation.

Update:

- `COMPATIBILITY.md`;
- `docs/compatibility/sql-table-dml.md`;
- `docs/compatibility/functions-temporal.md`;
- `docs/compatibility/sql-query-expressions.md` and
  `docs/compatibility/type-system-literals-conversion.md` only for the exact
  DML expression/literal surface.
