# Baseline ISNULL Function

## Summary

This phase admits a narrow MySQL `ISNULL(expr)` scalar function surface in the
existing no-source and `FROM DUAL` scalar `SELECT` path:

```sql
SELECT ISNULL(value)[, ...]
SELECT ISNULL(value)[, ...] FROM DUAL
```

The admitted input value domain is the same side-effect-free scalar domain used
by the current baseline control-flow functions: signed-64 decimal integer
literals, `TRUE`, `FALSE`, `NULL`, and nested supported `IF()`, `IFNULL()`,
`COALESCE()`, `NULLIF()`, and `ISNULL()` calls. `ISNULL(value)` returns integer
`1` when `value` is SQL `NULL`, otherwise integer `0`.

This is a comparison-function scalar projection slice, not a general expression
engine. It does not add table-backed `ISNULL()`, predicate use, DML assignment
values, expression metadata, arithmetic/string/subquery operands, or arbitrary
SQLite pass-through.

## Sources And Evidence

- Official MySQL 8.4 Reference Manual:
  - Comparison functions and operators:
    <https://dev.mysql.com/doc/refman/8.4/en/comparison-operators.html>
  - Built-in function and operator reference:
    <https://dev.mysql.com/doc/refman/8.4/en/built-in-function-reference.html>
  - Function name parsing and resolution:
    <https://dev.mysql.com/doc/refman/8.4/en/function-resolution.html>
  - `SELECT` statement and `DUAL`:
    <https://dev.mysql.com/doc/refman/8.4/en/select.html>
  - Information functions and `ROW_COUNT()`:
    <https://dev.mysql.com/doc/refman/8.4/en/information-functions.html>
  - `INFORMATION_SCHEMA.KEYWORDS`:
    <https://dev.mysql.com/doc/refman/8.4/en/information-schema-keywords-table.html>
- MySQL runtime probes are captured in
  `packages/libmylite/tests/mysql_baseline_isnull_function_expectations.sh`
  and verified against MySQL 8.4.9.

The MySQL manual documents `ISNULL(expr)` as returning `1` if `expr` is `NULL`
and `0` otherwise. It is a built-in comparison function. Runtime probes against
MySQL 8.4.9 confirm:

- `ISNULL(NULL)` returns `1`;
- integer, zero, signed integer, boolean, and signed-64 maximum operands return
  `0`;
- `TRUE` and `FALSE` render as `1` and `0` when used as values, and both are
  non-`NULL` for `ISNULL()`;
- `ISNULL (NULL)` with a space before `(` is accepted;
- `(ISNULL(NULL))` is accepted and uses the parenthesized source text as the
  default result-column label;
- nested supported `IF()`, `IFNULL()`, `COALESCE()`, `NULLIF()`, and
  `ISNULL()` calls are accepted by MySQL and are admitted by this slice over
  the same value domain;
- `SELECT ISNULL(NULL) FROM DUAL` behaves like no-source scalar projection;
- successful scalar `ISNULL()` projection reports `@@warning_count = 0` for
  admitted in-range values and makes a following `ROW_COUNT()` return `-1`;
- `ISNULL()` with zero or more than one argument reports MySQL native function
  error 1582 / SQLSTATE `42000`;
- malformed forms such as `ISNULL(,1)` report syntax error 1064 / SQLSTATE
  `42000`; and
- `ISNULL` is not present in `INFORMATION_SCHEMA.KEYWORDS` and remains usable
  as an unquoted nonreserved identifier.

MySQL accepts broader forms such as string operands, arithmetic operands,
variables, subqueries, table-backed `ISNULL()`, no-source `WHERE` / `ORDER BY`
/ `LIMIT`, decimal/float/hex/bit operands, and division-by-zero warning
behavior. Those are deferred until MyLite owns a broader expression system and
warning/type semantics for them.

## Ownership Boundaries

- Public API: no ABI or public-header changes. `mylite_execute()` continues to
  own result-handle ownership, diagnostics, and statement-boundary behavior.
- Statement context: scalar `ISNULL()` `SELECT` statements use existing
  row-returning `SELECT` result conventions, including previous row-count state
  `-1` and warning count storage.
- Lexer/parser/AST: the parser admits a new one-argument `ISNULL()` expression
  node and a wrong-arity diagnostic node. It preserves source spans for default
  result-column labels and diagnostics. `ISNULL` remains usable as an
  unquoted nonreserved identifier outside the admitted function-call grammar.
- Analyzer/runtime: the scalar projection analyzer accepts `ISNULL()` only in
  the existing no-source and `FROM DUAL` scalar select path. Evaluation is
  MyLite-owned and walks nested admitted scalar functions without SQLite SQL.
- Catalog: not involved. This feature must not read or mutate schema/table
  descriptors, descriptor versions, catalog generation, descriptor caches, or
  `sqlite_schema_generation`.
- Result builder: appends columns and one row through existing public result
  helpers. Successful scalar `ISNULL()` returns one result row and no DML side
  effects.
- Storage/VFS/file format: no file-format or VFS changes. Scalar `ISNULL()`
  must not touch user-table storage or the `.mylite` preamble.
- SQLite physical execution: no generated SQLite SQL and no SQLite fork patch.
  The feature is a MyLite scalar evaluator in front of the existing public
  result API.

## Supported SQL

Supported statement shapes:

```sql
SELECT scalar_isnull_item[, scalar_isnull_item ...]
SELECT ALL scalar_isnull_item[, scalar_isnull_item ...]
SELECT scalar_isnull_item[, scalar_isnull_item ...] FROM DUAL
SELECT ALL scalar_isnull_item[, scalar_isnull_item ...] FROM DUAL
```

Each select item may use the existing alias surface:

```sql
scalar_isnull_item:
    isnull_expression
  | isnull_expression AS alias
  | isnull_expression alias
```

The admitted expression subset is:

```sql
isnull_expression:
    ISNULL ( scalar_value )

scalar_value:
    scalar_integer
  | TRUE
  | FALSE
  | NULL
  | if_expression
  | ifnull_expression
  | coalesce_expression
  | nullif_expression
  | isnull_expression

scalar_integer:
    unsigned_decimal_integer_literal
  | + unsigned_decimal_integer_literal
  | - unsigned_decimal_integer_literal
```

Integer operands are limited to the warning-free signed-64 baseline envelope:
`-9223372036854775807` through `9223372036854775807`. MySQL's broader exact,
unsigned, floating, string, and warning-producing expression behavior is
deferred.

### MyLite Lemon-Syntax Snippet

This snippet describes MyLite's intended grammar, not MySQL's full grammar:

```lemon
expression(A) ::= ISNULL(T) LPAREN expression(B) RPAREN(R). {
    A = mylite_sql_parser_make_one_argument_function(
        state, T, MYLITE_SQL_AST_ISNULL_FUNCTION, B, R);
}
expression(A) ::= ISNULL(T) LPAREN RPAREN(R). {
    A = mylite_sql_parser_make_one_argument_function(
        state, T, MYLITE_SQL_AST_ISNULL_ARGUMENT_COUNT_ERROR, NULL, R);
}
expression(A) ::=
    ISNULL(T) LPAREN expression(B) COMMA function_argument_list(C) RPAREN(R). {
    (void)B;
    A = mylite_sql_parser_make_one_argument_function(
        state, T, MYLITE_SQL_AST_ISNULL_ARGUMENT_COUNT_ERROR, C, R);
}
identifier(A) ::= ISNULL(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
```

Wrong-arity `ISNULL()` parses into a diagnostic AST node so execution can
return MySQL-compatible native-function error 1582 for zero or more than one
argument. Malformed token sequences remain parser syntax errors.

## Semantics

Evaluation walks the admitted expression tree:

1. Evaluate the single operand to a scalar cell.
2. Return integer `1` when the operand is SQL `NULL`.
3. Return integer `0` when the operand is non-`NULL`.

Returned values render as canonical integer text `1` or `0`, matching MySQL for
the admitted scalar projection subset. The function's result is never `NULL`
inside this slice.

Default result-column labels use the existing source-span label convention:

- `SELECT ISNULL(NULL)` labels the column `ISNULL(NULL)`;
- `SELECT ISNULL (NULL)` labels the column `ISNULL (NULL)`;
- `SELECT (ISNULL(NULL))` labels the column `(ISNULL(NULL))`;
- explicit select-item aliases override the default label.

Successful supported statements return:

- one row;
- one column per select item;
- `affected_rows == 0`;
- `warning_count == 0`; and
- a following `ROW_COUNT()` result of `-1`.

## Unsupported And Diagnostics

Unsupported forms must fail deterministically without falling through to
SQLite:

- `ISNULL()` with zero or more than one argument as error 1582 / SQLSTATE
  `42000` with native function name `ISNULL`;
- malformed token sequences such as `ISNULL(,1)` as syntax error 1064 /
  SQLSTATE `42000`;
- table-backed `ISNULL()` projection;
- `ISNULL()` in `WHERE`, `ORDER BY`, `GROUP BY`, `HAVING`, DML assignments,
  defaults, or predicates;
- no-source `WHERE` / `ORDER BY` / `LIMIT` around scalar `ISNULL()`
  projection;
- mixed literal, `IF()`, `IFNULL()`, `COALESCE()`, `NULLIF()`, and `ISNULL()`
  top-level scalar projection items unless the implementation explicitly
  admits and tests a broader scalar expression-list shape;
- arithmetic, comparison, logical, bitwise, cast, string, decimal, float, hex,
  bit, temporal, JSON, variable, parameter, subquery, column-reference, or
  aggregate arguments;
- `CASE`, `DEFAULT(col_name)`, and general function calls; and
- integers outside the admitted signed-64 warning-free envelope.

Unsupported but parseable forms should use the existing MyLite
unsupported-feature diagnostic style. Parser-only forms may surface as syntax
errors when MyLite has no admitted AST shape for the unsupported form.

## Physical Handling And Performance

Scalar `ISNULL()` projection is constant per statement. MyLite should evaluate
it directly into the result object without opening catalog descriptors,
scanning SQLite tables, or generating SQLite SQL. Nested scalar evaluation
should stay linear in the admitted expression tree size and use the existing
explicit stack machinery rather than recursive evaluation.

No SQLite extension API or fork patch is needed. A future general expression
engine may introduce shared expression planning or SQLite hooks, but this slice
keeps `ISNULL()` under the current MyLite scalar path.

## Tests

The implementation must add fast C tests under `packages/libmylite/tests/`,
preferably a new `runtime_isnull_function` test binary. Coverage must include:

- no-source and `FROM DUAL` `ISNULL()` projections;
- `SELECT ALL`;
- `NULL`, integer, zero, signed values, signed-64 maximum, and booleans;
- nested `ISNULL()`, supported `IF()`, `IFNULL()`, `COALESCE()`, and
  `NULLIF()` operands;
- `ISNULL()` nested inside supported `IF()`, `IFNULL()`, `COALESCE()`, and
  `NULLIF()`;
- source-span labels including spaced `ISNULL (` and parenthesized
  `ISNULL()`;
- `ISNULL` as an unquoted table and column identifier;
- warning count, affected rows, row count after scalar select, and no catalog
  mutation;
- file-backed preamble preservation and independent handles;
- deterministic rejection for wrong argument counts, malformed syntax,
  table-backed `ISNULL()`, no-source `WHERE` / `ORDER BY` / `LIMIT`, mixed
  top-level scalar forms if deferred, arithmetic/string/decimal/float/hex/bit
  arguments, parameters, variables, subqueries, column arguments, `ISNULL()` in
  predicates or DML assignment values, and out-of-range integer operands; and
- no regression in existing parser, scalar select, literal projection, `IF()`,
  `IFNULL()`, `COALESCE()`, `NULLIF()`, system function, result metadata,
  statement-context, storage, and file-format tests.

## Compatibility Documentation

Update:

- `COMPATIBILITY.md` `ISNULL()` row from unsupported to limited support;
- `docs/compatibility/functions-comparison.md`;
- `docs/compatibility/sql-query-expressions.md` projection rows for the exact
  scalar-only subset.

Do not overclaim general `ISNULL`, table-backed expression evaluation,
expression metadata, warning-producing expression behavior, string or decimal
behavior, subqueries, DML assignment support, or arbitrary expression
evaluation.
