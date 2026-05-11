# Baseline SQRT Function

## Summary

This phase admits a narrow MyLite-owned `SQRT()` mathematical function for
no-source, `FROM DUAL`, and `DO` scalar execution:

```sql
SELECT SQRT(sqrt_value)[, SQRT(sqrt_value) ...]
SELECT SQRT(sqrt_value)[, SQRT(sqrt_value) ...] FROM DUAL
DO SQRT(sqrt_value)[, SQRT(sqrt_value) ...]
```

For the admitted argument domain, `SQRT(value)` returns the visible MySQL 8.4.9
text for the square root of a nonnegative integer-domain value, returns `NULL`
for `NULL`, and returns `NULL` for negative input. Negative input does not
produce a warning in MySQL 8.4.9.

This phase deliberately does not introduce a general `DOUBLE` expression model.
`SQRT()` is admitted only as a top-level scalar select item or `DO` expression,
with the integer/boolean/`NULL` operand domain described below. Table-backed
`SQRT(column)`, decimal and floating-point operands, string and binary-string
conversion, expression metadata, nested approximate arithmetic, predicates, DML
assignments, subqueries, CTEs, parameters, arbitrary SQLite pass-through, and
SQLite fork changes remain deferred.

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
  `packages/libmylite/tests/mysql_baseline_sqrt_function_expectations.sh`.

The MySQL 8.4 manual documents `SQRT(X)` as returning the square root of a
nonnegative number and returning `NULL` for `NULL`. Runtime probes against
MySQL 8.4.9 establish these expectations for this slice:

- `SQRT(NULL)` returns `NULL`;
- `SQRT(0)`, `SQRT(-0)`, and `SQRT(+0)` return `0`;
- `TRUE` and `FALSE` behave as integer `1` and `0`;
- exact square inputs such as `SQRT(1)`, `SQRT(4)`, and `SQRT(9)` return `1`,
  `2`, and `3`;
- non-square integer inputs return MySQL's visible shortest round-tripping
  double text, for example `SQRT(2)` returns `1.4142135623730951`,
  `SQRT(10)` returns `3.1622776601683795`, and `SQRT(20)` returns
  `4.47213595499958`;
- negative integer inputs return `NULL` and do not append warnings, including
  boundary examples such as `SQRT(-1)`, `SQRT(-16)`,
  `SQRT(-9223372036854775808)`, and `SQRT(-18446744073709551615)`;
- direct positive integer literals in the unsigned 64-bit envelope are
  admitted by this baseline, with observed examples such as
  `SQRT(9223372036854775807)` returning `3037000499.97605` and
  `SQRT(18446744073709551615)` returning `4294967296`;
- bitwise child operands use the existing unsigned 64-bit numeric bitwise
  result, for example `SQRT(~0)` returns `4294967296` and `SQRT(1<<63)`
  returns `3037000499.97605`;
- evaluated child division by zero returns `NULL` and records warning `1365` /
  SQLSTATE `22012`, `Division by 0`;
- child signed arithmetic overflow raises MySQL error `1690` / SQLSTATE
  `22003`;
- `SQRT()` and `SQRT(1,2)` raise MySQL error `1582` / SQLSTATE `42000`, with
  `SQRT` as the native function name;
- bare `SQRT` in a select list is an identifier lookup and raises MySQL error
  `1054` / SQLSTATE `42S22` when no such column is visible; and
- MySQL accepts broader forms such as `SQRT('64')`, `SQRT(X'40')`,
  `SQRT(b'1111')`, `SQRT(5.5)`, `SQRT(1e1)`, very large decimal literals, and
  table-backed `SQRT(column)`. Those remain deferred by this MyLite baseline.

## Ownership Boundaries

- Public API: unchanged. Successful supported `SELECT` statements return one
  row through existing result conventions; successful supported `DO` statements
  return a non-row result.
- Statement context: preserves existing `SELECT` and `DO` diagnostics,
  affected-row, and warning-count behavior. Warnings from evaluated child
  arithmetic are staged and appended through the existing scalar warning path.
- Lexer/parser/AST: adds `SQRT` token support, a one-argument `SQRT()` AST
  node, and a wrong-arity AST node. `SQRT` remains usable as an unquoted
  identifier where MyLite's keyword policy permits identifiers.
- Analyzer/runtime: admits only top-level supported `SQRT()` scalar projection
  and `DO` expressions. It evaluates the admitted operand and formats the result
  in MyLite runtime code.
- Catalog: not involved. The feature must not read or mutate descriptors,
  descriptor caches, catalog generation, selected schema, or
  `sqlite_schema_generation`.
- Result builder: existing scalar result helpers append one column per selected
  expression. Explicit aliases continue to define result labels.
- Storage/VFS/file format: no storage writes, physical table access, or
  `.mylite` preamble changes.
- SQLite: no generated SQLite SQL, no SQLite function registration, and no
  SQLite fork patch. This is MyLite wrapper/runtime behavior. The implementation
  may use the host C math library and must keep vendored SQLite warning policy
  unchanged.

## Syntax

MyLite admits these source forms:

```sql
SELECT sqrt_item[, sqrt_item ...]
SELECT sqrt_item[, sqrt_item ...] FROM DUAL
DO sqrt_scalar[, sqrt_scalar ...]

sqrt_item:
    sqrt_scalar
  | sqrt_scalar AS alias
  | sqrt_scalar alias

sqrt_scalar:
    SQRT ( sqrt_value )
  | ( sqrt_scalar )

sqrt_value:
    decimal_integer_literal
  | + decimal_integer_literal
  | - decimal_integer_literal
  | TRUE
  | FALSE
  | NULL
  | scalar_arithmetic_expression
  | scalar_bitwise_expression
```

Direct signed integer literals are admitted up to the unsigned 64-bit
magnitude envelope. Positive values are square-rooted as unsigned 64-bit
numeric magnitudes. Negative values in that magnitude envelope return `NULL`.
Direct integer literals outside that envelope are rejected with a deterministic
unsupported diagnostic until wider decimal precision exists.

`scalar_arithmetic_expression` is the current no-source/`DUAL` signed-64
arithmetic domain: decimal integer/boolean/`NULL` values, supported scalar
`IF()`/`IFNULL()`/`COALESCE()`/`NULLIF()`/`ISNULL()` values, parenthesized
admitted arithmetic, unary `+`/`-`, binary `+`, binary `-`, `*`, `%`, infix
`MOD`, `MOD(left, right)`, and infix `DIV`.

`scalar_bitwise_expression` is the current top-level numeric bitwise domain:
`~`, `&`, `|`, `^`, `<<`, and `>>` over admitted signed-64 scalar arithmetic
operands, with unsigned 64-bit results.

`SQRT()` is not admitted as a child of arithmetic, comparison, logical, `IS`,
`CASE`, control-flow functions, predicates, table-backed projection, ordering,
grouping, or DML expressions in this phase.

### MyLite Lemon Snippet

The parser grammar is authored independently and extends MyLite's expression
grammar:

```lemon
expression(A) ::= SQRT(T) LPAREN expression(B) RPAREN(R). {
    A = mylite_sql_parser_make_one_argument_function(
        state, T, MYLITE_SQL_AST_SQRT_FUNCTION, B, R);
}
expression(A) ::= SQRT(T) LPAREN RPAREN(R). {
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_SQRT_ARGUMENT_COUNT_ERROR, NULL, R);
}
expression(A) ::= SQRT(T) LPAREN expression(B) COMMA function_argument_list(C) RPAREN(R). {
    (void)B;
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_SQRT_ARGUMENT_COUNT_ERROR, C, R);
}
identifier(A) ::= SQRT(T). {
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
   one-argument `SQRT()` expression.
2. Preserve existing native-function wrong-arity diagnostics before generic
   unsupported diagnostics.
3. Evaluate the supported function argument once.
4. Direct `NULL` input returns `NULL`.
5. Direct boolean input maps `TRUE` to `1` and `FALSE` to `0`.
6. Direct decimal integer literals are parsed in the unsigned 64-bit magnitude
   envelope. Negative direct literals return `NULL`; nonnegative direct
   literals are evaluated as unsigned 64-bit magnitudes.
7. Nonliteral arithmetic arguments are evaluated with the existing signed-64
   scalar arithmetic evaluator, preserving `NULL`, division-by-zero warnings,
   and overflow behavior. Negative non-`NULL` results return `NULL`;
   nonnegative results are evaluated as unsigned magnitudes.
8. Bitwise arguments are evaluated with the existing scalar bitwise evaluator,
   preserving its `NULL` propagation, left-`NULL` short-circuiting, unsigned
   result representation, shift behavior, warning staging, and overflow
   behavior for evaluated children.
9. Format non-`NULL` square roots as the shortest decimal string that
   round-trips back to the computed double value, with at most 17 significant
   digits. This matches the observed MySQL 8.4.9 text for the admitted integer
   and bitwise examples while avoiding a broader approximate-number type
   system.
10. Append staged division-by-zero warnings after scalar select-item or `DO`
   expression evaluation through the existing scalar warning path.

Supported in-range `SQRT()` statements report `warning_count == 0` unless an
evaluated child arithmetic expression stages an existing division-by-zero
warning. They do not touch catalog state, SQLite schema generation, physical
tables, or the `.mylite` preamble.

## Diagnostics

Supported successful statements return through existing result conventions.

Diagnostics for this baseline:

- syntax errors use the existing parser diagnostic surface;
- wrong arity for zero arguments or two or more arguments uses MySQL error
  `1582` / SQLSTATE `42000` with native function name `SQRT`;
- bare `SQRT`: existing identifier/unknown-column behavior, MySQL error
  `1054`, SQLSTATE `42S22`, where the current scalar select path reaches name
  resolution;
- unsupported operands use a deterministic MyLite unsupported-feature
  diagnostic describing the admitted `SQRT()` subset;
- direct integer operands outside the admitted unsigned 64-bit literal envelope
  use that same deterministic unsupported diagnostic;
- child signed arithmetic overflow uses MySQL error `1690` / SQLSTATE
  `22003`;
- evaluated child division or modulo by zero appends warning `1365` / SQLSTATE
  `22012`;
- allocation failure returns `MYLITE_NOMEM`; and
- public API misuse remains unchanged.

Unsupported for this slice:

- table-backed `SQRT(column)` and any `FROM` source other than `DUAL`;
- `SQRT()` nested inside arithmetic, comparison, logical, `IS`, `CASE`,
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
- `SELECT SQRT(...)` over `NULL`, booleans, zero forms, positive exact squares,
  non-square integers, negative values, and signed/unsigned boundary examples;
- `SELECT SQRT(...) FROM DUAL`;
- arithmetic and bitwise child operands;
- child division-by-zero warning staging;
- successful `DO SQRT(...)` status and warnings;
- `SHOW WARNINGS` after successful and warning-producing `SQRT()` statements;
- wrong-arity diagnostics;
- bare `SQRT` unknown-column diagnostics; and
- MySQL-accepted but deferred string, decimal, float, hex, bit, wider-decimal,
  and table-backed forms.

Add plain C tests for:

- parser AST nodes for valid one-argument `SQRT(expr)` and wrong-arity forms;
- parser recognition that `SQRT` remains usable as an unquoted identifier where
  MyLite's keyword policy permits it;
- no-source and `FROM DUAL` projection for admitted values and source labels;
- explicit aliases and mixed scalar select lists;
- `DO SQRT(...)` with no result rows and existing affected-row behavior;
- child division-by-zero warning staging and child overflow diagnostics;
- deterministic rejection for strings, decimals, floats, hex literals, bit
  literals, table-backed columns, parameters, system variables, nested
  arithmetic use, comparison use, and `CASE` children;
- `.mylite` preamble preservation and unchanged catalog/schema generation for
  file-backed handles; and
- independent file-backed handles producing independent scalar results.

Verification commands:

```sh
./packages/libmylite/tests/mysql_baseline_sqrt_function_expectations.sh
cmake --build --preset dev
ctest --test-dir build/dev --output-on-failure -R 'libmylite\.(parser|runtime\.sqrt_function|runtime\.pi_function|runtime\.round_function|runtime\.scalar_division_projection|runtime\.scalar_bitwise_projection)'
cmake --workflow --preset check
```

## Compatibility Documentation

Update `COMPATIBILITY.md`, `docs/compatibility/functions-numeric-math.md`,
`docs/compatibility/sql-query-expressions.md`,
`docs/compatibility/sql-stored-programs.md`, and
`docs/compatibility/type-system-literals-conversion.md` only for the exact
top-level integer-domain `SQRT()` subset. Do not claim support for general
`DOUBLE`, approximate arithmetic, decimal/floating operands, string conversion,
table-backed expression evaluation, expression metadata, protocol metadata,
collations, casts, parameters, subqueries, CTEs, or SQLite function
pass-through.
