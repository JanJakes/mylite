# Baseline NULLIF Function

## Summary

This phase admits a narrow MySQL `NULLIF(expr1, expr2)` scalar function surface
in the existing no-source and `FROM DUAL` scalar `SELECT` path:

```sql
SELECT NULLIF(value, value)[, ...]
SELECT NULLIF(value, value)[, ...] FROM DUAL
```

The admitted values are MyLite-owned signed-64 decimal integer, boolean, and
`NULL` scalar values, plus nested supported scalar `NULLIF()`, `COALESCE()`,
`IFNULL()`, and `IF()` calls over the same value domain. `NULLIF(a, b)` returns
SQL `NULL` when both admitted operands are non-`NULL` and compare equal in this
numeric/boolean domain; otherwise it returns the first operand.

This is still a baseline scalar path, not a general expression subsystem. It
does not add table-backed `NULLIF()`, predicates, DML assignment values,
arithmetic/string arguments, subqueries, expression metadata, or arbitrary
SQLite pass-through.

Later row-scalar predicate work admits supported `NULLIF()` expressions as
direct descriptor-column comparison RHS values, such as
`WHERE id = NULLIF(1,0)`. The later
[baseline row-scalar truth predicates](../baseline-row-scalar-truth-predicates/specs.md)
slice also admits supported `WHERE NULLIF(...)` truth predicates. Other
unsupported expression contexts remain outside this baseline scalar slice.

## Sources And Evidence

- Official MySQL 8.4 Reference Manual:
  - Flow control functions:
    <https://dev.mysql.com/doc/refman/8.4/en/flow-control-functions.html>
  - Function name parsing and resolution:
    <https://dev.mysql.com/doc/refman/8.4/en/function-resolution.html>
  - `SELECT` statement and `DUAL`:
    <https://dev.mysql.com/doc/refman/8.4/en/select.html>
  - Information functions and `ROW_COUNT()`:
    <https://dev.mysql.com/doc/refman/8.4/en/information-functions.html>
  - `INFORMATION_SCHEMA.KEYWORDS`:
    <https://dev.mysql.com/doc/refman/8.4/en/information-schema-keywords-table.html>
- MySQL runtime probes are captured in
  `packages/libmylite/tests/mysql_baseline_nullif_function_expectations.sh`
  and verified against MySQL 8.4.9.

The MySQL manual describes `NULLIF(expr1, expr2)` as returning `NULL` when
`expr1 = expr2` is true, otherwise returning `expr1`; the result type follows
the first argument. It also documents that MySQL evaluates the first argument
twice when the arguments are not equal. Full MySQL expression evaluation,
side effects, type aggregation, collations, and broader operand forms are out
of scope for this baseline. MyLite admits only side-effect-free constant values
and nested supported control-flow functions, so the double-evaluation rule has
no visible effect inside this slice.

Runtime probes against MySQL 8.4.9 confirm:

- equal integer and boolean forms return SQL `NULL`, including `NULLIF(1,1)`,
  `NULLIF(0,0)`, `NULLIF(+0,-0)`, `NULLIF(-1,-1)`, `NULLIF(TRUE,1)`, and
  `NULLIF(FALSE,0)`;
- unequal integer and boolean forms return the first argument, including
  `NULLIF(1,2)`, `NULLIF(0,1)`, `NULLIF(-1,1)`, and
  `NULLIF(TRUE,FALSE)`;
- if the first argument is SQL `NULL`, the result is SQL `NULL`;
- if the second argument is SQL `NULL` and the first is not, the first
  argument is returned;
- `TRUE` and `FALSE` render as `1` and `0`;
- decimal integer values render in canonical decimal text for the supported
  warning-free signed-64 subset;
- `NULLIF (1,1)` with a space before `(` is accepted;
- `(NULLIF(1,2))` is accepted and uses the parenthesized source text as the
  default column label;
- nested supported `NULLIF()`, `IF()`, `IFNULL()`, and `COALESCE()` calls are
  accepted by MySQL and are admitted by this slice over the same value domain;
- `SELECT NULLIF(NULL,10) FROM DUAL` behaves like no-source scalar projection;
- successful scalar `NULLIF()` projection reports `@@warning_count = 0` for
  admitted in-range values and makes a following `ROW_COUNT()` return `-1`;
- `NULLIF()` with zero, one, or more than two arguments reports MySQL native
  function error 1582 / SQLSTATE `42000`;
- malformed forms such as `NULLIF(1,,2)` report syntax error 1064 / SQLSTATE
  `42000`; and
- `NULLIF` is not present in `INFORMATION_SCHEMA.KEYWORDS` and remains usable
  as an unquoted nonreserved identifier.

MySQL accepts broader forms such as string operands, decimal/float/hex/bit
operands, arithmetic operands, user/system variables, subqueries, table-backed
`NULLIF()`, no-source `WHERE` / `ORDER BY` / `LIMIT`, and side-effecting
assignments. Those are deferred by this MyLite slice.

## Ownership Boundaries

- Public API: no ABI or public-header changes. `mylite_execute()` continues to
  own result-handle ownership, diagnostics, and statement-boundary behavior.
- Statement context: scalar `NULLIF()` `SELECT` statements use existing
  row-returning `SELECT` result conventions, including previous row-count state
  `-1` and warning count storage.
- Lexer/parser/AST: the parser admits a new two-argument `NULLIF()` expression
  node and a wrong-arity diagnostic node. It preserves source spans for default
  result-column labels and diagnostics. `NULLIF` remains usable as an unquoted
  nonreserved identifier outside the admitted function-call grammar.
- Analyzer/runtime: the scalar projection analyzer accepts `NULLIF()` only in
  the existing no-source and `FROM DUAL` scalar select path. Evaluation is
  MyLite-owned and walks nested admitted `NULLIF()` / `COALESCE()` /
  `IFNULL()` / `IF()` expressions without SQLite SQL.
- Catalog: not involved. This feature must not read or mutate schema/table
  descriptors, descriptor versions, catalog generation, descriptor caches, or
  `sqlite_schema_generation`.
- Result builder: appends columns and one row through existing public result
  helpers. Successful scalar `NULLIF()` returns no affected rows and one result
  row.
- Storage/VFS/file format: no file-format or VFS changes. Scalar `NULLIF()`
  must not touch user-table storage or the `.mylite` preamble.
- SQLite physical execution: no generated SQLite SQL and no SQLite fork patch.
  The feature is a MyLite scalar evaluator in front of the existing public
  result API.

## Supported SQL

Supported statement shapes:

```sql
SELECT scalar_nullif_item[, scalar_nullif_item ...]
SELECT ALL scalar_nullif_item[, scalar_nullif_item ...]
SELECT scalar_nullif_item[, scalar_nullif_item ...] FROM DUAL
SELECT ALL scalar_nullif_item[, scalar_nullif_item ...] FROM DUAL
```

Each select item may use the existing alias surface:

```sql
scalar_nullif_item:
    nullif_expression
  | nullif_expression AS alias
  | nullif_expression alias
```

The admitted expression subset is:

```sql
nullif_expression:
    NULLIF ( scalar_value , scalar_value )

scalar_value:
    scalar_integer
  | TRUE
  | FALSE
  | NULL
  | nullif_expression
  | coalesce_expression
  | ifnull_expression
  | if_expression

scalar_integer:
    unsigned_decimal_integer_literal
  | + unsigned_decimal_integer_literal
  | - unsigned_decimal_integer_literal
```

Integer operands are limited to the warning-free signed-64 baseline envelope:
`-9223372036854775807` through `9223372036854775807`. MySQL's larger unsigned
and exact numeric behavior is deferred until MyLite owns expression numeric
types and warnings more generally.

### MyLite Lemon-Syntax Snippet

This snippet describes MyLite's intended grammar, not MySQL's full grammar:

```lemon
expression(A) ::= NULLIF(T) LPAREN expression(B) COMMA expression(C) RPAREN(R). {
    A = mylite_sql_parser_make_two_argument_function(
        state, T, MYLITE_SQL_AST_NULLIF_FUNCTION, B, C, R);
}
expression(A) ::= NULLIF(T) LPAREN RPAREN(R). {
    A = mylite_sql_parser_make_one_argument_function(
        state, T, MYLITE_SQL_AST_NULLIF_ARGUMENT_COUNT_ERROR, NULL, R);
}
expression(A) ::= NULLIF(T) LPAREN expression(B) RPAREN(R). {
    A = mylite_sql_parser_make_one_argument_function(
        state, T, MYLITE_SQL_AST_NULLIF_ARGUMENT_COUNT_ERROR, B, R);
}
expression(A) ::=
    NULLIF(T) LPAREN expression(B) COMMA expression(C) COMMA
    function_argument_list(D) RPAREN(R). {
    (void)B;
    (void)C;
    A = mylite_sql_parser_make_one_argument_function(
        state, T, MYLITE_SQL_AST_NULLIF_ARGUMENT_COUNT_ERROR, D, R);
}
identifier(A) ::= NULLIF(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
```

Wrong-arity `NULLIF()` parses into a diagnostic AST node so execution can return
MySQL-compatible native-function error 1582 for zero, one, or more than two
arguments. Malformed token sequences remain parser syntax errors.

## Semantics

Evaluation walks the admitted expression tree:

1. Evaluate the first operand to a scalar cell.
2. Evaluate the second operand to a scalar cell.
3. If both operands are non-`NULL` and their canonical numeric/boolean values
   compare equal, return SQL `NULL`.
4. Otherwise return the first operand. If the first operand is SQL `NULL`, the
   returned value is SQL `NULL`.

Returned boolean values render as `1` or `0`. Returned integer values render in
canonical decimal text, matching the existing scalar literal projection
convention for the admitted signed-64 subset.

MyLite evaluates both operands once for this baseline. The MySQL-documented
double evaluation of the first argument when arguments are unequal is deferred
with variables, assignments, user functions, and side-effecting expression
support. Since all admitted operands are constants or supported side-effect-free
nested functions, this difference is not visible inside the slice.

Default result-column labels use the existing source-span label convention:

- `SELECT NULLIF(1,1)` labels the column `NULLIF(1,1)`;
- `SELECT NULLIF (1,1)` labels the column `NULLIF (1,1)`;
- `SELECT (NULLIF(1,2))` labels the column `(NULLIF(1,2))`;
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

- `NULLIF()` with zero, one, or more than two arguments as error 1582 /
  SQLSTATE `42000` with native function name `NULLIF`;
- malformed token sequences such as `NULLIF(1,,2)` as syntax error 1064 /
  SQLSTATE `42000`;
- table-backed `NULLIF()` projection;
- `ORDER BY`, `GROUP BY`, `HAVING`, DML assignments, defaults, or unsupported
  predicate positions;
- no-source `WHERE` / `ORDER BY` / `LIMIT` around scalar `NULLIF()` projection;
- mixed literal, `IF()`, `IFNULL()`, `COALESCE()`, and `NULLIF()` top-level
  scalar projection items unless the implementation explicitly admits and tests
  a broader scalar expression-list shape;
- arithmetic, comparison, logical, bitwise, cast, string, decimal, float, hex,
  bit, temporal, JSON, variable, parameter, subquery, column-reference, or
  aggregate arguments;
- `CASE`, `DEFAULT(col_name)`, and general function calls; and
- integers outside the admitted signed-64 warning-free envelope.

Unsupported but parseable forms should use the existing MyLite
unsupported-feature diagnostic style. Parser-only forms may surface as syntax
errors when MyLite has no admitted AST shape for the unsupported form.

## Physical Handling And Performance

Scalar `NULLIF()` projection is constant per statement. MyLite should evaluate
it directly into the result object without opening catalog descriptors,
scanning SQLite tables, or generating SQLite SQL. Nested control-flow depth is
bounded by parser input and AST allocation; validation and evaluation should be
linear in the admitted expression tree size and should use existing explicit
stack machinery rather than recursive evaluation.

No SQLite extension API or fork patch is needed. A future general expression
engine may introduce shared expression planning or SQLite hooks, but this slice
keeps `NULLIF()` under the current MyLite scalar path.

## Tests

The implementation must add fast C tests under `packages/libmylite/tests/`,
preferably a new `runtime_nullif_function` test binary. Coverage must include:

- no-source and `FROM DUAL` `NULLIF()` projections;
- `SELECT ALL`;
- equal and unequal integer values, including zero, signed values, and the
  signed-64 maximum;
- boolean comparisons and boolean/integer equality;
- first-`NULL`, second-`NULL`, and both-`NULL` behavior;
- integer, boolean, and `NULL` returned values;
- nested `NULLIF()`, nested supported `COALESCE()`, nested supported
  `IFNULL()`, and nested supported `IF()` operands;
- `NULLIF()` nested inside supported `IF()`, `IFNULL()`, and `COALESCE()`;
- source-span labels including spaced `NULLIF (` and parenthesized `NULLIF()`;
- `NULLIF` as an unquoted table and column identifier;
- warning count, affected rows, row count after scalar select, and no catalog
  mutation;
- file-backed preamble preservation and independent handles;
- deterministic rejection for wrong argument counts, malformed syntax,
  table-backed `NULLIF()`, no-source `WHERE` / `ORDER BY` / `LIMIT`, mixed
  top-level scalar forms if deferred, arithmetic/string/decimal/float/hex/bit
  arguments, parameters, variables, subqueries, unsupported column arguments,
  unsupported predicate positions, DML assignment values, and out-of-range
  integer operands; and
- no regression in existing parser, scalar select, literal projection, `IF()`,
  `IFNULL()`, `COALESCE()`, system function, result metadata,
  statement-context, storage, and file-format tests.

## Compatibility Documentation

Update:

- `COMPATIBILITY.md` `NULLIF()` row from unsupported to limited support;
- `docs/compatibility/functions-control-flow.md`;
- `docs/compatibility/sql-query-expressions.md` projection rows for the exact
  scalar-only subset.

Do not overclaim general `NULLIF`, table-backed expression evaluation, type
aggregation, collations, expression metadata, side effects, string or decimal
behavior, subqueries, DML assignment support, or arbitrary expression
evaluation.
