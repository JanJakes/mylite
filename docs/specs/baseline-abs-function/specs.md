# Baseline ABS Function

## Summary

This phase admits a narrow MyLite-owned numeric `ABS()` expression surface for
no-source, `FROM DUAL`, and `DO` scalar execution:

```sql
SELECT ABS(abs_value)[, ABS(abs_value) ...]
SELECT ABS(abs_value)[, ABS(abs_value) ...] FROM DUAL
DO ABS(abs_value)[, ABS(abs_value) ...]
```

The function returns the absolute value of the admitted argument, or `NULL` for
a `NULL` argument. This is not a general function or expression engine. The
phase does not admit table-backed `ABS(column)`, decimal/float/string/hex/bit
conversion, casts, user variables, parameters, subqueries, CTEs, expression
metadata, `ABS()` nested inside arithmetic/comparison/logical/`CASE` parents,
or arbitrary SQLite pass-through.

## Compatibility Authority

- Official MySQL 8.4 documentation:
  - Mathematical functions:
    <https://dev.mysql.com/doc/refman/8.4/en/mathematical-functions.html>
  - `SELECT` statement and `DUAL`:
    <https://dev.mysql.com/doc/refman/8.4/en/select.html>
  - `DO` statement:
    <https://dev.mysql.com/doc/refman/8.4/en/do.html>
- Observed MySQL 8.4.9 runtime behavior, captured by
  `packages/libmylite/tests/mysql_baseline_abs_function_expectations.sh`.

The MySQL 8.4 manual documents `ABS(X)` as returning the absolute value of
`X`, or `NULL` when `X` is `NULL`. It also documents that
`ABS(-9223372036854775808)` errors because the result cannot fit in a signed
`BIGINT`.

Runtime probes against MySQL 8.4.9 establish these expectations for this slice:

- `ABS(NULL)` returns `NULL`;
- `ABS(0)`, `ABS(1)`, `ABS(-1)`, `ABS(TRUE)`, and `ABS(FALSE)` return `0`,
  `1`, `1`, `1`, and `0`;
- signed in-range boundaries behave as integer values:
  `ABS(-9223372036854775807)` and `ABS(9223372036854775807)` both return
  `9223372036854775807`;
- `ABS(-9223372036854775808)` raises MySQL error `1690` / SQLSTATE `22003`;
- direct unsigned decimal literal magnitudes within the unsigned 64-bit range
  are admitted for this baseline: `ABS(9223372036854775808)`,
  `ABS(-9223372036854775809)`, `ABS(18446744073709551615)`, and
  `ABS(-18446744073709551615)` return their nonnegative decimal magnitudes;
- `ABS()` and `ABS(a, b)` raise MySQL error `1582` / SQLSTATE `42000`,
  "Incorrect parameter count in the call to native function 'ABS'";
- evaluated child division by zero returns `NULL` and records warning `1365` /
  SQLSTATE `22012`, `Division by 0`;
- child signed arithmetic overflow raises MySQL error `1690` / SQLSTATE
  `22003`; and
- MySQL accepts broader forms such as strings, binary strings, hex literals,
  bit literals, decimals, floats, casts, and table-backed columns. Those remain
  deferred by this MyLite baseline.

## Ownership Boundaries

- Public API: unchanged. Successful supported `SELECT` statements return one
  row through existing result conventions; successful `DO` statements return a
  non-row result.
- Statement context: preserves existing `SELECT` and `DO` row-count behavior.
  Division-by-zero warnings from evaluated child arithmetic are staged and
  appended through the same scalar warning path as modulo, `DIV`, bitwise, and
  `BIT_COUNT()`.
- Lexer/parser/AST: adds a function-specific `ABS()` AST node plus a
  wrong-arity AST node, following existing native-function parser patterns.
- Analyzer/runtime: admits only top-level `ABS()` scalar projection
  expressions and evaluates them in MyLite code. The function argument may use
  the admitted absolute-value domain described below.
- Catalog: not involved. The feature must not read or mutate descriptors,
  descriptor caches, catalog generation, or `sqlite_schema_generation`.
- Result builder: existing scalar result helpers append one column per selected
  expression. Explicit aliases continue to define result labels.
- Storage/VFS/file format: no storage writes, physical table access, or
  `.mylite` preamble changes.
- SQLite: no generated SQLite SQL and no SQLite fork patch. This is MyLite
  wrapper/runtime behavior.

## Syntax

MyLite admits these source forms:

```sql
SELECT abs_item[, abs_item ...]
SELECT abs_item[, abs_item ...] FROM DUAL
DO abs_scalar[, abs_scalar ...]

abs_item:
    abs_scalar
  | abs_scalar AS alias
  | abs_scalar alias

abs_scalar:
    ABS ( abs_value )
  | ( abs_scalar )

abs_value:
    decimal_integer_literal
  | + decimal_integer_literal
  | - decimal_integer_literal
  | TRUE
  | FALSE
  | NULL
  | scalar_arithmetic_expression
  | scalar_bitwise_expression
```

`decimal_integer_literal` is admitted directly up to
`18446744073709551615` for nonnegative forms. Negative direct literal
magnitudes are admitted up to `18446744073709551615`, except that the exact
signed `BIGINT` minimum source form `-9223372036854775808` returns the
documented MySQL overflow error. Values beyond the unsigned 64-bit range remain
deferred even though MySQL can represent broader exact decimal forms.

`scalar_arithmetic_expression` is the current no-source/`DUAL` signed-64
arithmetic domain: decimal integer/boolean/`NULL` values, supported scalar
`IF()`/`IFNULL()`/`COALESCE()`/`NULLIF()`/`ISNULL()` values, parenthesized
admitted arithmetic, unary `+`/`-`, binary `+`, binary `-`, `*`, `%`, infix
`MOD`, `MOD(left, right)`, and infix `DIV`.

`scalar_bitwise_expression` is the current top-level numeric bitwise domain:
`~`, `&`, `|`, `^`, `<<`, and `>>` over admitted signed-64 scalar arithmetic
operands, with unsigned 64-bit results.

`ABS()` itself is not admitted as a child of arithmetic, comparison, logical,
`IS`, `CASE`, control-flow functions, predicates, table-backed projection,
ordering, grouping, or DML expressions in this phase.

### MyLite Lemon Snippet

The parser grammar is authored independently and extends MyLite's expression
grammar:

```lemon
expression(A) ::= ABS(T) LPAREN expression(B) RPAREN(R). {
    A = mylite_sql_parser_make_one_argument_function(
        state, T, MYLITE_SQL_AST_ABS_FUNCTION, B, R);
}
expression(A) ::= ABS(T) LPAREN RPAREN(R). {
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_ABS_ARGUMENT_COUNT_ERROR, NULL, R);
}
expression(A) ::= ABS(T) LPAREN expression(B) COMMA function_argument_list(C) RPAREN(R). {
    (void)B;
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_ABS_ARGUMENT_COUNT_ERROR, C, R);
}
```

These snippets describe MyLite's admitted grammar and are not copied from
MySQL grammar text.

## Runtime Semantics

Runtime evaluation is MyLite-owned and proportional to AST size.

1. Admit a `SELECT` or `DO` statement only when each selected or evaluated
   expression is an existing scalar expression or a top-level supported
   `ABS()` expression.
2. Preserve existing native-function wrong-arity diagnostics before generic
   unsupported diagnostics.
3. Evaluate the `ABS()` argument once.
4. Direct decimal integer literals are converted to an unsigned 64-bit
   magnitude using the bounds in the syntax section.
5. The exact direct source value `-9223372036854775808` raises MySQL-compatible
   overflow diagnostics for this baseline.
6. Nonliteral arithmetic arguments are evaluated with the existing signed-64
   scalar arithmetic evaluator, preserving `NULL`, division-by-zero warnings,
   and overflow behavior. If the non-`NULL` signed value is the signed 64-bit
   minimum, return MySQL-compatible overflow diagnostics; otherwise return its
   absolute magnitude.
7. Bitwise arguments are evaluated with the existing scalar bitwise evaluator,
   preserving its `NULL` propagation, left-`NULL` short-circuiting, unsigned
   result representation, shift behavior, warning staging, and overflow
   behavior for evaluated children. Non-`NULL` bitwise results are already
   unsigned magnitudes.
8. `NULL` input returns `NULL` and preserves staged child warnings.
9. Non-`NULL` input returns the absolute value as canonical unsigned decimal
   text.
10. Append staged division-by-zero warnings after scalar select-item or `DO`
    expression evaluation through the existing scalar warning path.

Supported in-range `ABS()` statements report `warning_count == 0` unless an
evaluated child arithmetic expression stages an existing division-by-zero
warning. They do not touch catalog state, SQLite schema generation, physical
tables, or the `.mylite` preamble.

## Diagnostics

Supported successful statements return through existing result conventions.

Diagnostics for this baseline:

- syntax errors use the existing parser diagnostic surface;
- `ABS()` wrong arity uses MySQL error `1582` / SQLSTATE `42000`;
- unsupported operands use a deterministic MyLite unsupported-feature
  diagnostic describing the admitted `ABS()` subset;
- direct integer operands outside the admitted unsigned 64-bit literal envelope
  use a deterministic MyLite unsupported-feature diagnostic;
- direct or evaluated signed `BIGINT` minimum input uses MySQL error `1690` /
  SQLSTATE `22003`;
- child signed arithmetic overflow uses MySQL error `1690` / SQLSTATE `22003`;
- evaluated child division or modulo by zero appends warning `1365` / SQLSTATE
  `22012`;
- allocation failure returns `MYLITE_NOMEM`; and
- public API misuse remains unchanged.

Unsupported for this slice:

- table-backed `ABS(column)` and any `FROM` source other than `DUAL`;
- `ABS()` nested inside arithmetic, comparison, logical, `IS`, `CASE`,
  control-flow functions, predicates, `ORDER BY`, `GROUP BY`, aggregate
  arguments, defaults, generated columns, and DML assignment expressions;
- string, decimal, float, hex, bit, binary-string, temporal, JSON, parameter,
  user-variable, system-variable, session-function, cast, collation, and
  subquery operands;
- direct exact decimal magnitudes above `18446744073709551615`; and
- arbitrary SQLite pass-through.

## Tests

The test suite should cover:

- parser AST nodes for valid `ABS(expr)` and wrong-arity forms;
- no-source and `FROM DUAL` projection for `NULL`, booleans, positive integer
  values, negative integer values, signed boundaries, unsigned direct literal
  boundaries, arithmetic children, and bitwise children;
- `DO ABS(...)` with no result rows and correct affected-row behavior;
- child division-by-zero warning staging and child overflow diagnostics;
- `ABS(-9223372036854775808)` overflow diagnostics;
- explicit aliases and generated column labels;
- deterministic rejection for strings, decimals, floats, hex literals, bit
  literals, table-backed columns, parameters, system variables, session
  functions, subqueries, CTEs, arithmetic over `ABS()`, comparison over
  `ABS()`, `CASE` results containing `ABS()`, and `/`;
- file-backed preamble/catalog-generation/schema-generation safety;
- independent handles; and
- existing lexer, parser, scalar projection, arithmetic, modulo, `DIV`,
  bitwise, comparison, logical, `CASE`, `DO`, runtime, storage, and catalog
  tests.

Verification commands:

1. `cmake --build --preset dev`
2. focused parser/runtime CTest entries;
3. `packages/libmylite/tests/mysql_baseline_abs_function_expectations.sh`
4. `cmake --workflow --preset check`

## Compatibility Documentation

Update `COMPATIBILITY.md`,
`docs/compatibility/functions-numeric-math.md`,
`docs/compatibility/operators.md`,
`docs/compatibility/sql-query-expressions.md`, and
`docs/compatibility/sql-stored-programs.md` only for this limited
no-source/`DUAL`/`DO` numeric `ABS()` subset. Do not imply support for full
MySQL `ABS()`, decimal/float/string conversion, table-backed expression
evaluation, expression metadata, or general function composition.
