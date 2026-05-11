# Baseline ATAN Functions

## Summary

This phase admits narrow MyLite-owned `ATAN()` and `ATAN2()` mathematical
functions for no-source, `FROM DUAL`, and `DO` scalar execution:

```sql
SELECT ATAN(angle_value)[, ATAN2(angle_value) ...]
SELECT ATAN(y_value, x_value)[, ATAN2(y_value, x_value) ...]
SELECT ATAN(...)[, ATAN2(...) ...] FROM DUAL
DO ATAN(...)[, ATAN2(...) ...]
```

For the admitted argument domain, one-argument `ATAN(value)` and `ATAN2(value)`
return the visible MySQL 8.4.9 arc-tangent text for `value`. Two-argument
`ATAN(y, x)` and `ATAN2(y, x)` return the visible MySQL 8.4.9 quadrant-aware
arc-tangent text. All admitted forms return `NULL` when the relevant evaluated
argument is `NULL`.

This phase deliberately does not introduce a general approximate-number
expression model. The functions are admitted only as top-level scalar select
items or `DO` expressions, with the integer/boolean/`NULL` operand domain
described below. Table-backed `ATAN(column)` / `ATAN2(column)`, decimal and
floating-point operands, string and binary-string conversion, expression
metadata, nested approximate arithmetic, predicates, DML assignments,
subqueries, CTEs, parameters, arbitrary SQLite pass-through, and SQLite fork
changes remain deferred.

## Compatibility Authority

- Official MySQL 8.4 documentation:
  - Mathematical functions:
    <https://dev.mysql.com/doc/refman/8.4/en/mathematical-functions.html>
  - Numeric functions and operators:
    <https://dev.mysql.com/doc/refman/8.4/en/numeric-functions.html>
  - Numeric literals:
    <https://dev.mysql.com/doc/refman/8.4/en/number-literals.html>
  - `SELECT` statement and `DUAL`:
    <https://dev.mysql.com/doc/refman/8.4/en/select.html>
  - `DO` statement:
    <https://dev.mysql.com/doc/refman/8.4/en/do.html>
- Observed MySQL 8.4.9 runtime behavior, captured by
  `packages/libmylite/tests/mysql_baseline_atan_functions_expectations.sh`.

The MySQL 8.4 manual documents `ATAN(X)` as returning the arc tangent of `X`
or `NULL` for `NULL`, and documents two-argument `ATAN(Y, X)` / `ATAN2(Y, X)`
as using both argument signs to choose the result quadrant and returning `NULL`
if either argument is `NULL`. Runtime probes against MySQL 8.4.9 establish
these expectations for this slice:

- `ATAN(NULL)` and `ATAN2(NULL)` return `NULL`;
- `TRUE` and `FALSE` behave as integer `1` and `0`;
- zero forms `0`, `-0`, and `+0` behave as zero for the admitted integer
  literal domain;
- `ATAN2(value)` is accepted by MySQL 8.4.9 and behaves like
  `ATAN(value)`;
- `ATAN(1)` / `ATAN2(1)` return `0.7853981633974483`;
- `ATAN(0)` / `ATAN2(0)` return `0`;
- `ATAN(-1)` / `ATAN2(-1)` return `-0.7853981633974483`;
- direct large signed and unsigned 64-bit integer values return values near
  positive or negative pi/2 without warnings;
- `ATAN(1,0)` / `ATAN2(1,0)` return `1.5707963267948966`;
- `ATAN(-1,0)` / `ATAN2(-1,0)` return `-1.5707963267948966`;
- `ATAN(0,-1)` / `ATAN2(0,-1)` return `3.141592653589793`;
- `ATAN(1,-1)` / `ATAN2(1,-1)` return `2.356194490192345`;
- `ATAN(-1,-1)` / `ATAN2(-1,-1)` return `-2.356194490192345`;
- one-argument bitwise child operands use the existing unsigned 64-bit numeric
  bitwise result, for example `ATAN(~0)` returns
  `1.5707963267948966` and `ATAN(1<<64)` returns `0`;
- two-argument bitwise child operands use the same unsigned numeric results,
  for example `ATAN(1,~0)` returns `5.421010862427522e-20`;
- evaluated child division by zero returns `NULL` and records warning `1365` /
  SQLSTATE `22012`, `Division by 0`;
- two-argument runtime behavior evaluates the first argument first. If the
  first argument is `NULL`, MySQL returns `NULL` without evaluating the second
  argument for warnings or errors. If the first argument is non-`NULL`, the
  second argument is evaluated and may stage warnings or raise errors;
- child signed arithmetic overflow raises MySQL error `1690` / SQLSTATE
  `22003`;
- when an earlier selected or `DO` expression stages a division-by-zero warning
  and a later expression raises overflow, MySQL preserves both diagnostics for
  the statement;
- `ATAN()`, `ATAN(1,2,3)`, `ATAN2()`, and `ATAN2(1,2,3)` raise MySQL error
  `1582` / SQLSTATE `42000`, with the matching native function name;
- bare `ATAN` and bare `ATAN2` in a select list are identifier lookups and
  raise MySQL error `1054` / SQLSTATE `42S22` when no such column is visible;
  and
- MySQL accepts broader forms such as string, binary-string, hex, bit, decimal,
  float, wider-decimal, system-variable, prepared-parameter, and table-backed
  column operands. Those remain deferred by this MyLite baseline.

## Ownership Boundaries

- Public API: unchanged. Successful supported `SELECT` statements return one
  row through existing result conventions; successful supported `DO` statements
  return a non-row result.
- Statement context: preserves existing `SELECT` and `DO` diagnostics,
  affected-row, and warning-count behavior. Warnings from evaluated child
  arithmetic are staged and appended through the existing scalar warning path.
- Lexer/parser/AST: adds `ATAN` and `ATAN2` token support, one- and
  two-argument function AST nodes, and wrong-arity AST nodes. Both names remain
  usable as unquoted identifiers where MyLite's keyword policy permits
  identifiers.
- Analyzer/runtime: admits only top-level supported scalar projection and `DO`
  expressions. It evaluates admitted operands and formats results in MyLite
  runtime code.
- Catalog: not involved. The feature must not read or mutate descriptors,
  descriptor caches, catalog generation, selected schema, or
  `sqlite_schema_generation`.
- Result builder: existing scalar result helpers append one column per selected
  expression. Explicit aliases continue to define result labels.
- Storage/VFS/file format: no storage writes, physical table access, or
  `.mylite` preamble changes.
- SQLite: no generated SQLite SQL, no SQLite function registration, and no
  SQLite fork patch. This is MyLite wrapper/runtime behavior. The
  implementation may use the host C math library for `atan()` and `atan2()`
  over the admitted integer-domain values and must keep vendored SQLite warning
  policy unchanged.

## Syntax

MyLite admits these source forms:

```sql
SELECT atan_item[, atan_item ...]
SELECT atan_item[, atan_item ...] FROM DUAL
DO atan_scalar[, atan_scalar ...]

atan_item:
    atan_scalar
  | atan_scalar AS alias
  | atan_scalar alias

atan_scalar:
    ATAN ( atan_value )
  | ATAN2 ( atan_value )
  | ATAN ( atan_value , atan_value )
  | ATAN2 ( atan_value , atan_value )
  | ( atan_scalar )

atan_value:
    decimal_integer_literal
  | + decimal_integer_literal
  | - decimal_integer_literal
  | TRUE
  | FALSE
  | NULL
  | scalar_arithmetic_expression
  | scalar_bitwise_expression
```

Direct signed integer literals are admitted up to the unsigned 64-bit magnitude
envelope. Direct integer literals outside that envelope are rejected with a
deterministic unsupported diagnostic until wider decimal precision exists.

`scalar_arithmetic_expression` is the current no-source/`DUAL` signed-64
arithmetic domain: decimal integer/boolean/`NULL` values, supported scalar
`IF()`/`IFNULL()`/`COALESCE()`/`NULLIF()`/`ISNULL()` values, parenthesized
admitted arithmetic, unary `+`/`-`, binary `+`, binary `-`, `*`, `%`, infix
`MOD`, `MOD(left, right)`, and infix `DIV`.

`scalar_bitwise_expression` is the current top-level numeric bitwise domain:
`~`, `&`, `|`, `^`, `<<`, and `>>` over admitted signed-64 scalar arithmetic
operands, with unsigned 64-bit results.

`ATAN()` and `ATAN2()` are not admitted as children of arithmetic, comparison,
logical, `IS`, `CASE`, control-flow functions, predicates, table-backed
projection, ordering, grouping, or DML expressions in this phase.

### MyLite Lemon Snippet

The parser grammar is authored independently and extends MyLite's expression
grammar:

```lemon
expression(A) ::= ATAN(T) LPAREN expression(B) RPAREN(R). {
    A = mylite_sql_parser_make_one_argument_function(
        state, T, MYLITE_SQL_AST_ATAN_FUNCTION, B, R);
}
expression(A) ::= ATAN(T) LPAREN expression(B) COMMA expression(C) RPAREN(R). {
    A = mylite_sql_parser_make_two_argument_function(
        state, T, MYLITE_SQL_AST_ATAN_FUNCTION, B, C, R);
}
expression(A) ::= ATAN(T) LPAREN RPAREN(R). {
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_ATAN_ARGUMENT_COUNT_ERROR, NULL, R);
}
expression(A) ::=
    ATAN(T) LPAREN expression(B) COMMA expression(C) COMMA function_argument_list(D) RPAREN(R). {
    (void)B;
    (void)C;
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_ATAN_ARGUMENT_COUNT_ERROR, D, R);
}
identifier(A) ::= ATAN(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}

expression(A) ::= ATAN2(T) LPAREN expression(B) RPAREN(R). {
    A = mylite_sql_parser_make_one_argument_function(
        state, T, MYLITE_SQL_AST_ATAN2_FUNCTION, B, R);
}
expression(A) ::= ATAN2(T) LPAREN expression(B) COMMA expression(C) RPAREN(R). {
    A = mylite_sql_parser_make_two_argument_function(
        state, T, MYLITE_SQL_AST_ATAN2_FUNCTION, B, C, R);
}
expression(A) ::= ATAN2(T) LPAREN RPAREN(R). {
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_ATAN2_ARGUMENT_COUNT_ERROR, NULL, R);
}
expression(A) ::=
    ATAN2(T) LPAREN expression(B) COMMA expression(C) COMMA function_argument_list(D) RPAREN(R). {
    (void)B;
    (void)C;
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_ATAN2_ARGUMENT_COUNT_ERROR, D, R);
}
identifier(A) ::= ATAN2(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
```

These snippets describe MyLite's admitted grammar and parser recognition for
wrong-arity native-function diagnostics. They are not copied from MySQL grammar
text.

## Runtime Semantics

Runtime evaluation is MyLite-owned and proportional to AST size.

1. Admit a `SELECT` or `DO` statement only when each selected or evaluated
   expression is an existing scalar expression or a top-level supported
   one- or two-argument `ATAN()` or `ATAN2()` expression.
2. Preserve existing native-function wrong-arity diagnostics before generic
   unsupported diagnostics.
3. Evaluate one-argument `ATAN(value)` and `ATAN2(value)` by evaluating the
   supported argument once. `NULL` input returns `NULL`; otherwise the result is
   `atan(value)`.
4. Evaluate two-argument `ATAN(y, x)` and `ATAN2(y, x)` left to right. If
   `y` evaluates to `NULL`, return `NULL` without evaluating `x`. Otherwise,
   evaluate `x`; if `x` evaluates to `NULL`, return `NULL`. Otherwise the
   result is `atan2(y, x)`.
5. Direct boolean input maps `TRUE` to `1` and `FALSE` to `0`.
6. Direct decimal integer literals are parsed in the unsigned 64-bit magnitude
   envelope. The sign is preserved separately, so the minimum direct admitted
   value is `-18446744073709551615` and the maximum is
   `18446744073709551615`.
7. Nonliteral arithmetic arguments are evaluated with the existing signed-64
   scalar arithmetic evaluator, preserving `NULL`, division-by-zero warnings,
   and overflow behavior.
8. Bitwise arguments are evaluated with the existing scalar bitwise evaluator,
   preserving its `NULL` propagation, left-`NULL` short-circuiting, unsigned
   result representation, shift behavior, warning staging, and overflow
   behavior for evaluated children.
9. Convert non-`NULL` integer-domain values through `double`.
10. Format exact zero as decimal integer text `0`. Format other non-`NULL`
    results through the existing scalar double text formatter used by the
    limited approximate numeric functions, with at most 17 significant digits.
11. Append staged division-by-zero warnings after scalar select-item or `DO`
    expression evaluation through the existing scalar warning path.

Supported in-range statements report `warning_count == 0` unless an evaluated
child arithmetic expression stages an existing division-by-zero warning. They
do not touch catalog state, SQLite schema generation, physical tables, or the
`.mylite` preamble.

## Diagnostics

Supported successful statements return through existing result conventions.

Diagnostics for this baseline:

- syntax errors use the existing parser diagnostic surface;
- wrong arity for zero arguments or three or more arguments uses MySQL error
  `1582` / SQLSTATE `42000` with native function name `ATAN` or `ATAN2`;
- bare `ATAN` and `ATAN2`: existing identifier/unknown-column behavior, MySQL
  error `1054`, SQLSTATE `42S22`, where the current scalar select path reaches
  name resolution;
- unsupported operands use a deterministic MyLite unsupported-feature
  diagnostic describing the admitted `ATAN()` and `ATAN2()` subset;
- direct integer operands outside the admitted unsigned 64-bit literal envelope
  use that same deterministic unsupported diagnostic;
- child signed arithmetic overflow uses MySQL error `1690` / SQLSTATE
  `22003`;
- evaluated child division or modulo by zero appends warning `1365` / SQLSTATE
  `22012`;
- allocation failure returns `MYLITE_NOMEM`; and
- public API misuse remains unchanged.

Unsupported for this slice:

- table-backed `ATAN(column)` / `ATAN2(column)` and any `FROM` source other
  than `DUAL`;
- nested use inside arithmetic, comparison, logical, `IS`, `CASE`,
  control-flow functions, predicates, `ORDER BY`, `GROUP BY`, aggregate
  arguments, defaults, generated columns, and DML assignment expressions;
- string, decimal, float, hex, bit, binary-string, temporal, JSON, parameter,
  user-variable, system-variable, session-function, cast, collation, and
  subquery operands;
- direct exact decimal magnitudes above unsigned 64-bit; and
- arbitrary SQLite pass-through.

## Tests

Add MySQL-runtime expectation coverage for:

- MySQL version guard;
- one-argument `ATAN(...)` and `ATAN2(...)` over `NULL`, booleans, zero forms,
  positive and negative values, and signed/unsigned boundary examples;
- two-argument `ATAN(...)` and `ATAN2(...)` over `NULL`, booleans, zero forms,
  quadrant cases, and signed/unsigned boundary examples;
- `SELECT ... FROM DUAL`;
- arithmetic and bitwise child operands;
- two-argument first-argument `NULL` short-circuit behavior;
- child division-by-zero warning staging;
- warning-before-error diagnostics for `SELECT` and `DO`;
- successful `DO ATAN(...)` / `DO ATAN2(...)` status and warnings;
- `SHOW WARNINGS` after successful and warning-producing statements;
- wrong-arity diagnostics;
- bare-name unknown-column diagnostics; and
- MySQL-accepted but deferred string, decimal, float, hex, bit, wider-decimal,
  system-variable, and table-backed forms.

Add plain C tests for:

- parser AST nodes for valid one-argument calls, two-argument calls, and
  wrong-arity forms;
- parser recognition that both function names remain usable as unquoted
  identifiers where MyLite's keyword policy permits them;
- no-source and `FROM DUAL` projection for admitted values and source labels;
- explicit aliases and mixed scalar select lists;
- `DO` with no result rows and existing affected-row behavior;
- two-argument first-argument `NULL` short-circuit behavior;
- child division-by-zero warning staging and child overflow diagnostics;
- warning-before-error diagnostics for `SELECT` and `DO`;
- deterministic rejection for strings, decimals, floats, hex literals, bit
  literals, table-backed columns, parameters, system variables, nested
  arithmetic use, comparison use, and `CASE` children;
- `.mylite` preamble preservation and unchanged catalog/schema generation for
  file-backed handles; and
- independent file-backed handles producing independent scalar results.

Verification commands:

```sh
./packages/libmylite/tests/mysql_baseline_atan_functions_expectations.sh
cmake --build --preset dev
ctest --test-dir build/dev --output-on-failure -R 'libmylite\.(parser|runtime\.atan_functions|runtime\.acos_asin_functions|runtime\.degrees_radians_functions)'
cmake --workflow --preset check
```

## Compatibility Documentation

Update `COMPATIBILITY.md`, `docs/compatibility/functions-numeric-math.md`,
`docs/compatibility/operators.md`,
`docs/compatibility/sql-query-expressions.md`,
`docs/compatibility/sql-stored-programs.md`, and
`docs/compatibility/type-system-literals-conversion.md` only for the exact
top-level integer-domain `ATAN()` and `ATAN2()` subset. Do not claim support
for general `DOUBLE`, approximate arithmetic, decimal/floating operands, string
conversion, table-backed expression evaluation, expression metadata, protocol
metadata, collations, casts, parameters, subqueries, CTEs, or SQLite function
pass-through.
