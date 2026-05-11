# Baseline ACOS And ASIN Functions

## Summary

This phase admits narrow MyLite-owned `ACOS()` and `ASIN()` mathematical
functions for no-source, `FROM DUAL`, and `DO` scalar execution:

```sql
SELECT ACOS(trig_value)[, ASIN(trig_value) ...]
SELECT ACOS(trig_value)[, ASIN(trig_value) ...] FROM DUAL
DO ACOS(trig_value)[, ASIN(trig_value) ...]
```

For the admitted argument domain, `ACOS(value)` returns the visible MySQL 8.4.9
arc-cosine text, `ASIN(value)` returns the visible MySQL 8.4.9 arc-sine text,
and both functions return `NULL` when the argument is `NULL` or outside the
domain `-1..1`.

This phase deliberately does not introduce a general approximate-number
expression model. Both functions are admitted only as top-level scalar select
items or `DO` expressions, with the integer/boolean/`NULL` operand domain
described below. Table-backed `ACOS(column)` and `ASIN(column)`, decimal and
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
  `packages/libmylite/tests/mysql_baseline_acos_asin_functions_expectations.sh`.

The MySQL 8.4 manual documents `ACOS(X)` as returning the arc cosine of `X`,
or `NULL` for `NULL` or out-of-range `X`, and `ASIN(X)` as returning the arc
sine of `X`, or `NULL` for `NULL` or out-of-range `X`. Runtime probes against
MySQL 8.4.9 establish these expectations for this slice:

- `ACOS(NULL)` and `ASIN(NULL)` return `NULL`;
- `TRUE` and `FALSE` behave as integer `1` and `0`;
- zero forms `0`, `-0`, and `+0` behave as zero;
- `ACOS(1)` returns `0`;
- `ACOS(0)` returns `1.5707963267948966`;
- `ACOS(-1)` returns `3.141592653589793`;
- `ASIN(1)` returns `1.5707963267948966`;
- `ASIN(0)` returns `0`;
- `ASIN(-1)` returns `-1.5707963267948966`;
- direct integer values outside `-1..1`, including signed and unsigned 64-bit
  boundary examples, return `NULL` and do not add warnings;
- bitwise child operands use the existing unsigned 64-bit numeric bitwise
  result, for example `ACOS(5&3)` returns `0`, `ASIN(5&3)` returns
  `1.5707963267948966`, `ACOS(~0)` returns `NULL`, and `ASIN(~0)` returns
  `NULL`;
- evaluated child division by zero returns `NULL` and records warning `1365` /
  SQLSTATE `22012`, `Division by 0`;
- child signed arithmetic overflow raises MySQL error `1690` / SQLSTATE
  `22003`;
- when an earlier selected or `DO` expression stages a division-by-zero warning
  and a later expression raises overflow, MySQL preserves both diagnostics for
  the statement;
- `ACOS()`, `ACOS(1,2)`, `ASIN()`, and `ASIN(1,2)` raise MySQL error `1582` /
  SQLSTATE `42000`, with the matching native function name;
- bare `ACOS` and bare `ASIN` in a select list are identifier lookups and raise
  MySQL error `1054` / SQLSTATE `42S22` when no such column is visible; and
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
- Lexer/parser/AST: adds `ACOS` and `ASIN` token support, one-argument function
  AST nodes, and wrong-arity AST nodes. Both names remain usable as unquoted
  identifiers where MyLite's keyword policy permits identifiers.
- Analyzer/runtime: admits only top-level supported scalar projection and `DO`
  expressions. It evaluates the admitted operand and formats the result in
  MyLite runtime code.
- Catalog: not involved. The feature must not read or mutate descriptors,
  descriptor caches, catalog generation, selected schema, or
  `sqlite_schema_generation`.
- Result builder: existing scalar result helpers append one column per selected
  expression. Explicit aliases continue to define result labels.
- Storage/VFS/file format: no storage writes, physical table access, or
  `.mylite` preamble changes.
- SQLite: no generated SQLite SQL, no SQLite function registration, and no
  SQLite fork patch. This is MyLite wrapper/runtime behavior. The
  implementation may use the host C math library for `acos()` and `asin()` over
  the admitted integer-domain values and must keep vendored SQLite warning
  policy unchanged.

## Syntax

MyLite admits these source forms:

```sql
SELECT inverse_trig_item[, inverse_trig_item ...]
SELECT inverse_trig_item[, inverse_trig_item ...] FROM DUAL
DO inverse_trig_scalar[, inverse_trig_scalar ...]

inverse_trig_item:
    inverse_trig_scalar
  | inverse_trig_scalar AS alias
  | inverse_trig_scalar alias

inverse_trig_scalar:
    ACOS ( trig_value )
  | ASIN ( trig_value )
  | ( inverse_trig_scalar )

trig_value:
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
envelope. Positive and negative values in that envelope outside `-1..1` return
`NULL` without warnings. Direct integer literals outside that envelope are
rejected with a deterministic unsupported diagnostic until wider decimal
precision exists.

`scalar_arithmetic_expression` is the current no-source/`DUAL` signed-64
arithmetic domain: decimal integer/boolean/`NULL` values, supported scalar
`IF()`/`IFNULL()`/`COALESCE()`/`NULLIF()`/`ISNULL()` values, parenthesized
admitted arithmetic, unary `+`/`-`, binary `+`, binary `-`, `*`, `%`, infix
`MOD`, `MOD(left, right)`, and infix `DIV`.

`scalar_bitwise_expression` is the current top-level numeric bitwise domain:
`~`, `&`, `|`, `^`, `<<`, and `>>` over admitted signed-64 scalar arithmetic
operands, with unsigned 64-bit results.

`ACOS()` and `ASIN()` are not admitted as children of arithmetic, comparison,
logical, `IS`, `CASE`, control-flow functions, predicates, table-backed
projection, ordering, grouping, or DML expressions in this phase.

### MyLite Lemon Snippet

The parser grammar is authored independently and extends MyLite's expression
grammar:

```lemon
expression(A) ::= ACOS(T) LPAREN expression(B) RPAREN(R). {
    A = mylite_sql_parser_make_one_argument_function(
        state, T, MYLITE_SQL_AST_ACOS_FUNCTION, B, R);
}
expression(A) ::= ACOS(T) LPAREN RPAREN(R). {
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_ACOS_ARGUMENT_COUNT_ERROR, NULL, R);
}
expression(A) ::= ACOS(T) LPAREN expression(B) COMMA function_argument_list(C) RPAREN(R). {
    (void)B;
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_ACOS_ARGUMENT_COUNT_ERROR, C, R);
}
identifier(A) ::= ACOS(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}

expression(A) ::= ASIN(T) LPAREN expression(B) RPAREN(R). {
    A = mylite_sql_parser_make_one_argument_function(
        state, T, MYLITE_SQL_AST_ASIN_FUNCTION, B, R);
}
expression(A) ::= ASIN(T) LPAREN RPAREN(R). {
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_ASIN_ARGUMENT_COUNT_ERROR, NULL, R);
}
expression(A) ::= ASIN(T) LPAREN expression(B) COMMA function_argument_list(C) RPAREN(R). {
    (void)B;
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_ASIN_ARGUMENT_COUNT_ERROR, C, R);
}
identifier(A) ::= ASIN(T). {
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
   one-argument `ACOS()` or `ASIN()` expression.
2. Preserve existing native-function wrong-arity diagnostics before generic
   unsupported diagnostics.
3. Evaluate the supported function argument once.
4. Direct `NULL` input returns `NULL`.
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
9. If the admitted integer-domain value is outside `-1..1`, return `NULL`
   without adding a warning.
10. Convert in-domain values through a double using `acos(value)` or
    `asin(value)`.
11. Format exact zero as decimal integer text `0`. Format other non-`NULL`
    results through the existing scalar double text formatter used by the
    limited approximate numeric functions, with at most 17 significant digits.
12. Append staged division-by-zero warnings after scalar select-item or `DO`
    expression evaluation through the existing scalar warning path.

Supported in-range statements report `warning_count == 0` unless an evaluated
child arithmetic expression stages an existing division-by-zero warning. They
do not touch catalog state, SQLite schema generation, physical tables, or the
`.mylite` preamble.

## Diagnostics

Supported successful statements return through existing result conventions.

Diagnostics for this baseline:

- syntax errors use the existing parser diagnostic surface;
- wrong arity for zero arguments or two or more arguments uses MySQL error
  `1582` / SQLSTATE `42000` with native function name `ACOS` or `ASIN`;
- bare `ACOS` and `ASIN`: existing identifier/unknown-column behavior, MySQL
  error `1054`, SQLSTATE `42S22`, where the current scalar select path reaches
  name resolution;
- unsupported operands use a deterministic MyLite unsupported-feature
  diagnostic describing the admitted `ACOS()` and `ASIN()` subset;
- direct integer operands outside the admitted unsigned 64-bit literal envelope
  use that same deterministic unsupported diagnostic;
- child signed arithmetic overflow uses MySQL error `1690` / SQLSTATE
  `22003`;
- evaluated child division or modulo by zero appends warning `1365` / SQLSTATE
  `22012`;
- allocation failure returns `MYLITE_NOMEM`; and
- public API misuse remains unchanged.

Unsupported for this slice:

- table-backed `ACOS(column)` / `ASIN(column)` and any `FROM` source other than
  `DUAL`;
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
- `SELECT ACOS(...)` and `SELECT ASIN(...)` over `NULL`, booleans, zero forms,
  in-domain values, out-of-domain values, and signed/unsigned boundary
  examples;
- `SELECT ... FROM DUAL`;
- arithmetic and bitwise child operands;
- child division-by-zero warning staging;
- warning-before-error diagnostics for `SELECT` and `DO`;
- successful `DO ACOS(...)` / `DO ASIN(...)` status and warnings;
- `SHOW WARNINGS` after successful and warning-producing statements;
- wrong-arity diagnostics;
- bare-name unknown-column diagnostics; and
- MySQL-accepted but deferred string, decimal, float, hex, bit, wider-decimal,
  system-variable, and table-backed forms.

Add plain C tests for:

- parser AST nodes for valid one-argument calls and wrong-arity forms;
- parser recognition that both function names remain usable as unquoted
  identifiers where MyLite's keyword policy permits them;
- no-source and `FROM DUAL` projection for admitted values and source labels;
- explicit aliases and mixed scalar select lists;
- `DO` with no result rows and existing affected-row behavior;
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
./packages/libmylite/tests/mysql_baseline_acos_asin_functions_expectations.sh
cmake --build --preset dev
ctest --test-dir build/dev --output-on-failure -R 'libmylite\.(parser|runtime\.acos_asin_functions|runtime\.degrees_radians_functions|runtime\.sqrt_function)'
cmake --workflow --preset check
```

## Compatibility Documentation

Update `COMPATIBILITY.md`, `docs/compatibility/functions-numeric-math.md`,
`docs/compatibility/sql-query-expressions.md`,
`docs/compatibility/sql-stored-programs.md`, and
`docs/compatibility/type-system-literals-conversion.md` only for the exact
top-level integer-domain `ACOS()` and `ASIN()` subset. Do not claim support for
general `DOUBLE`, approximate arithmetic, decimal/floating operands, string
conversion, table-backed expression evaluation, expression metadata, protocol
metadata, collations, casts, parameters, subqueries, CTEs, or SQLite function
pass-through.
