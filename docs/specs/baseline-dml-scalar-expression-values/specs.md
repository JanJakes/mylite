# Baseline DML Scalar Expression Values

This phase broadens descriptor-backed DML value slots from the existing narrow
constant-scalar subset to source-free scalar expressions already supported by
MyLite's scalar evaluator.

The target is compatibility for common MySQL application and corpus shapes such
as:

```sql
INSERT INTO t VALUES (1 + 2 * 3)
INSERT INTO t VALUES (GREATEST('alpha', 'beta'), UNHEX('4142'))
REPLACE INTO t SET js = JSON_OBJECT('a', 1)
UPDATE t SET c = GREATEST('alpha', 'beta'), n = BIT_COUNT(15)
INSERT INTO t VALUES (DATE_ADD('2024-01-02 03:04:05', INTERVAL 1 SECOND))
```

This does not introduce a table-backed DML expression executor. Expressions
that depend on source rows, subqueries, aggregates, windows, spatial
constructors, stored routines, or loadable functions remain outside this slice.

## Compatibility Evidence

Primary references:

- MySQL 8.4 Reference Manual, "INSERT Statement":
  <https://dev.mysql.com/doc/refman/8.4/en/insert.html>
- MySQL 8.4 Reference Manual, "UPDATE Statement":
  <https://dev.mysql.com/doc/refman/8.4/en/update.html>
- MySQL 8.4 Reference Manual, "REPLACE Statement":
  <https://dev.mysql.com/doc/refman/8.4/en/replace.html>
- MySQL 8.4 Reference Manual, "Expressions":
  <https://dev.mysql.com/doc/refman/8.4/en/expressions.html>
- MySQL 8.4 Reference Manual, "Functions and Operators":
  <https://dev.mysql.com/doc/refman/8.4/en/functions.html>
- Observed MySQL runtime: Docker container `mylite-mysql-849`, `SELECT
  VERSION()` = `8.4.9`.

The MySQL statement syntax defines `INSERT`, `REPLACE`, and single-table
`UPDATE` assignment values as either an expression or `DEFAULT`. Runtime probes
verify that MySQL evaluates those expressions first, then applies target-column
storage conversion. For example, `1 + 2 * 3` assigned to `INT` stores `7`,
`UNHEX('4142')` assigned to `VARBINARY` stores bytes `41 42`,
`JSON_OBJECT('a', 1)` assigned to `JSON` stores a JSON object, and
`GREATEST('abc', 'def')` assigned to `INT` raises `1366 / HY000` in strict
mode but inserts `0` with one warning under `INSERT IGNORE`.

## Ownership Boundaries

- Public API: no ABI change.
- Parser/AST: add a DML-only source-free scalar expression nonterminal that
  reuses existing AST node constructors.
- Runtime: evaluate admitted expressions through the existing no-source scalar
  evaluator, append scalar warnings, and route the result through the normal
  descriptor conversion helpers.
- Catalog/storage/SQLite: no descriptor format, SQLite SQL, SQLite fork, VFS,
  or `.mylite` file-format changes.

## Grammar

MyLite Lemon-syntax snippets:

```lemon
insert_value(A) ::= dml_scalar_expression(B).
insert_value(A) ::= update_constant_arithmetic_value(B).
update_value(A) ::= dml_scalar_expression(B).

dml_scalar_expression(A) ::= dml_scalar_atom(B).

dml_scalar_atom(A) ::= supported_scalar_function(B).
dml_scalar_atom(A) ::= cast_convert_expression(B).
dml_scalar_atom(A) ::= current_timestamp_value(B).
dml_scalar_atom(A) ::= current_date_value(B).
dml_scalar_atom(A) ::= current_time_value(B).
dml_scalar_atom(A) ::= utc_date_value(B).
dml_scalar_atom(A) ::= utc_time_value(B).
dml_scalar_atom(A) ::= utc_timestamp_value(B).
dml_scalar_atom(A) ::= sysdate_value(B).
dml_scalar_atom(A) ::= literal(B).
dml_scalar_atom(A) ::= charset_introducer STRING|HEX_LITERAL|BIT_LITERAL.
dml_scalar_atom(A) ::= TEMPORAL_LITERAL_INTRODUCER STRING.

supported_scalar_function(A) ::= IF(integer_control_arguments).
supported_scalar_function(A) ::= GREATEST(function_argument_list).
supported_scalar_function(A) ::= LEAST(function_argument_list).
supported_scalar_function(A) ::= CONCAT(function_argument_list).
supported_scalar_function(A) ::= CONCAT_WS(flat_function_argument_list).
supported_scalar_function(A) ::= HEX(expression).
supported_scalar_function(A) ::= UNHEX(expression).
supported_scalar_function(A) ::= ABS(expression).
supported_scalar_function(A) ::= FLOOR(expression).
supported_scalar_function(A) ::= ROUND(expression, expression).
supported_scalar_function(A) ::= ACOS(expression).
supported_scalar_function(A) ::= LOG2(expression).
supported_scalar_function(A) ::= POW(expression, expression).
supported_scalar_function(A) ::= BIT_COUNT(expression).
supported_scalar_function(A) ::= JSON_ARRAY(function_argument_list).
supported_scalar_function(A) ::= JSON_OBJECT(function_argument_list).
supported_scalar_function(A) ::= TIMESTAMP(expression[, expression]).
supported_scalar_function(A) ::= DATE_ADD(STRING, INTERVAL expression unit).
supported_scalar_function(A) ::= ADDTIME(expression, expression).
```

The implemented parser may keep existing literal, `DEFAULT`, `RAND()`, and
legacy constant-scalar productions for conflict control. Runtime admission is
still gated by the source-free scalar-expression validator. A broader
`insert_value ::= expression` or `update_value ::= expression` grammar was
explicitly avoided because it increases ambiguity with the existing MyLite
statement grammar.

## Semantics

- Accepted scalar expressions are evaluated once per DML row value or
  assignment expression before descriptor storage conversion.
- `INSERT ... VALUES`, `INSERT ... SET`, `REPLACE ... VALUES`, `REPLACE ...
  SET`, supported `ON DUPLICATE KEY UPDATE` assignments, and supported
  single-table `UPDATE` assignments share the same source-free scalar value
  envelope.
- `NULL` scalar results follow the current DML `NULL` storage rules.
- String-like scalar results enter existing `CHAR`, `VARCHAR`, `TEXT`, binary
  string, JSON, `ENUM`, `SET`, numeric, `YEAR`, `DATE`, `TIME`, `DATETIME`,
  `TIMESTAMP`, and `BIT` conversion where that target already supports
  compatible literal or scalar conversion.
- `INSERT IGNORE`, `REPLACE`, duplicate-key assignment, non-strict SQL mode,
  and `UPDATE IGNORE` continue to determine whether descriptor conversion
  errors are fatal or warning-adjusted.
- Scalar warnings are appended before storage conversion warnings.
- Supported expressions must be source-free after unwrapping parentheses. This
  excludes table-column references and scalar subqueries in this slice even
  when the grammar can represent them.

## Compatibility Limits

- No row-dependent assignment expressions, column references, scalar subqueries
  in insert/replace values, aggregates, windows, stored functions, loadable
  functions, spatial constructors, or table-backed expression planning.
- No broader `UPDATE` multi-assignment behavior: multi-assignment updates still
  admit only assignment values classified as statement-constant by the planner.
- No broad DML `DEFAULT(column_name)` expansion beyond the existing
  descriptor-compatible baseline.
- No new character-set transcoding beyond existing MyLite scalar charset
  support.
- No SQLite fork hook is needed; the feature is a MyLite parser/runtime wrapper
  over existing scalar evaluation and descriptor conversion.

## Tests

Add MySQL-runtime expectations and focused runtime tests for:

- constant integer arithmetic assigned to integer targets;
- integer control-flow values through `IF()`;
- string functions such as `GREATEST()`, `LEAST()`, and flat `CONCAT_WS()`;
- binary functions such as `HEX()` and `UNHEX()` assigned to binary targets;
- numeric functions such as `ABS()`, `POW()`, `BIT_COUNT()`, `ROUND()`, and
  `ACOS()`;
- JSON constructors assigned to JSON targets;
- temporal functions such as `TIMESTAMP()`, `DATE_ADD()`, `ADDTIME()`, and
  `SEC_TO_TIME()`;
- duplicate-key assignment and `REPLACE` paths;
- strict and `INSERT IGNORE` conversion diagnostics after scalar evaluation;

Verification before marking done:

1. `packages/libmylite/tests/mysql_baseline_dml_scalar_expression_values_expectations.sh`
2. Focused CTest entry for the runtime test.
3. Parse-corpus benchmark comparison.
4. `git diff --check`
5. `cmake --workflow --preset check`
