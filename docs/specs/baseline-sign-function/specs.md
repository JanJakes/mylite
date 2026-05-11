# Baseline SIGN Function

## Summary

This phase admits a narrow MyLite-owned numeric `SIGN()` expression surface for
no-source, `FROM DUAL`, and `DO` scalar execution:

```sql
SELECT SIGN(sign_value)[, SIGN(sign_value) ...]
SELECT SIGN(sign_value)[, SIGN(sign_value) ...] FROM DUAL
DO SIGN(sign_value)[, SIGN(sign_value) ...]
```

The function returns `-1`, `0`, or `1` when the admitted argument is negative,
zero, or positive, and returns `NULL` for a `NULL` argument. This is not a
general function or expression engine. The phase does not admit table-backed
`SIGN(column)`, string/decimal/float/hex/bit conversion, casts, user variables,
parameters, subqueries, CTEs, expression metadata, `SIGN()` nested inside
arithmetic/comparison/logical/`CASE` parents, or arbitrary SQLite pass-through.

## Compatibility Authority

- Official MySQL 8.4 documentation:
  - Mathematical functions:
    <https://dev.mysql.com/doc/refman/8.4/en/mathematical-functions.html>
  - Arithmetic operators:
    <https://dev.mysql.com/doc/refman/8.4/en/arithmetic-functions.html>
  - Numeric literals:
    <https://dev.mysql.com/doc/refman/8.4/en/number-literals.html>
  - `SELECT` statement and `DUAL`:
    <https://dev.mysql.com/doc/refman/8.4/en/select.html>
  - `DO` statement:
    <https://dev.mysql.com/doc/refman/8.4/en/do.html>
- Observed MySQL 8.4.9 runtime behavior, captured by
  `packages/libmylite/tests/mysql_baseline_sign_function_expectations.sh`.

The MySQL 8.4 manual documents `SIGN(X)` as returning `-1`, `0`, or `1` based
on whether `X` is negative, zero, or positive, and `NULL` when `X` is `NULL`.

Runtime probes against MySQL 8.4.9 establish these expectations for this slice:

- `SIGN(NULL)` returns `NULL`;
- `SIGN(TRUE)` and `SIGN(FALSE)` return `1` and `0`;
- `SIGN(0)`, `SIGN(-0)`, and `SIGN(+0)` return `0`;
- signed and unsigned integer boundary literals return only sign:
  `SIGN(-9223372036854775808)` returns `-1`,
  `SIGN(-9223372036854775809)` returns `-1`,
  `SIGN(9223372036854775808)` returns `1`, and
  `SIGN(18446744073709551615)` returns `1`;
- exact decimal integer literal magnitudes beyond unsigned 64-bit still return
  sign in MySQL, for example `SIGN(18446744073709551616)` returns `1`;
- `SIGN()` and `SIGN(a, b)` raise MySQL error `1582` / SQLSTATE `42000`,
  "Incorrect parameter count in the call to native function 'SIGN'";
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
  appended through the same scalar warning path as modulo, `DIV`, bitwise,
  `ABS()`, and `BIT_COUNT()`.
- Lexer/parser/AST: adds a function-specific `SIGN()` AST node plus a
  wrong-arity AST node, following existing native-function parser patterns.
- Analyzer/runtime: admits only top-level `SIGN()` scalar projection
  expressions and evaluates them in MyLite code. The function argument may use
  the admitted sign-value domain described below.
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
SELECT sign_item[, sign_item ...]
SELECT sign_item[, sign_item ...] FROM DUAL
DO sign_scalar[, sign_scalar ...]

sign_item:
    sign_scalar
  | sign_scalar AS alias
  | sign_scalar alias

sign_scalar:
    SIGN ( sign_value )
  | ( sign_scalar )

sign_value:
    decimal_integer_literal
  | + decimal_integer_literal
  | - decimal_integer_literal
  | TRUE
  | FALSE
  | NULL
  | scalar_arithmetic_expression
  | scalar_bitwise_expression
```

`decimal_integer_literal` is admitted directly regardless of digit count for
this function. Direct literal evaluation uses only the optional sign and
whether the decimal digits are all zero, matching MySQL's sign result for exact
integer literals beyond MyLite's current arithmetic storage envelope.

`scalar_arithmetic_expression` is the current no-source/`DUAL` signed-64
arithmetic domain: decimal integer/boolean/`NULL` values, supported scalar
`IF()`/`IFNULL()`/`COALESCE()`/`NULLIF()`/`ISNULL()` values, parenthesized
admitted arithmetic, unary `+`/`-`, binary `+`, binary `-`, `*`, `%`, infix
`MOD`, `MOD(left, right)`, and infix `DIV`.

`scalar_bitwise_expression` is the current top-level numeric bitwise domain:
`~`, `&`, `|`, `^`, `<<`, and `>>` over admitted signed-64 scalar arithmetic
operands, with unsigned 64-bit results.

`SIGN()` itself is not admitted as a child of arithmetic, comparison, logical,
`IS`, `CASE`, control-flow functions, predicates, table-backed projection,
ordering, grouping, or DML expressions in this phase.

### MyLite Lemon Snippet

The parser grammar is authored independently and extends MyLite's expression
grammar:

```lemon
expression(A) ::= SIGN(T) LPAREN expression(B) RPAREN(R). {
    A = mylite_sql_parser_make_one_argument_function(
        state, T, MYLITE_SQL_AST_SIGN_FUNCTION, B, R);
}
expression(A) ::= SIGN(T) LPAREN RPAREN(R). {
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_SIGN_ARGUMENT_COUNT_ERROR, NULL, R);
}
expression(A) ::= SIGN(T) LPAREN expression(B) COMMA function_argument_list(C) RPAREN(R). {
    (void)B;
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_SIGN_ARGUMENT_COUNT_ERROR, C, R);
}
```

These snippets describe MyLite's admitted grammar and are not copied from
MySQL grammar text.

## Runtime Semantics

Runtime evaluation is MyLite-owned and proportional to AST size.

1. Admit a `SELECT` or `DO` statement only when each selected or evaluated
   expression is an existing scalar expression or a top-level supported
   `SIGN()` expression.
2. Preserve existing native-function wrong-arity diagnostics before generic
   unsupported diagnostics.
3. Evaluate the `SIGN()` argument once.
4. Direct decimal integer literals are classified from source text using only
   optional sign and zero/nonzero digit content. Direct all-zero literals return
   `0`; negative nonzero literals return `-1`; positive nonzero literals
   return `1`.
5. Nonliteral arithmetic arguments are evaluated with the existing signed-64
   scalar arithmetic evaluator, preserving `NULL`, division-by-zero warnings,
   and overflow behavior. Non-`NULL` signed values are then mapped to `-1`,
   `0`, or `1`.
6. Bitwise arguments are evaluated with the existing scalar bitwise evaluator,
   preserving its `NULL` propagation, left-`NULL` short-circuiting, unsigned
   result representation, shift behavior, warning staging, and overflow
   behavior for evaluated children. Non-`NULL` bitwise values return `0` when
   the unsigned result is `0`, otherwise `1`.
7. `NULL` input returns `NULL` and preserves staged child warnings.
8. Non-`NULL` input returns canonical signed integer text: `-1`, `0`, or `1`.
9. Append staged division-by-zero warnings after scalar select-item or `DO`
   expression evaluation through the existing scalar warning path.

Supported in-range `SIGN()` statements report `warning_count == 0` unless an
evaluated child arithmetic expression stages an existing division-by-zero
warning. They do not touch catalog state, SQLite schema generation, physical
tables, or the `.mylite` preamble.

## Diagnostics

Supported successful statements return through existing result conventions.

Diagnostics for this baseline:

- syntax errors use the existing parser diagnostic surface;
- `SIGN()` wrong arity uses MySQL error `1582` / SQLSTATE `42000`;
- unsupported operands use a deterministic MyLite unsupported-feature
  diagnostic describing the admitted `SIGN()` subset;
- child signed arithmetic overflow uses MySQL error `1690` / SQLSTATE
  `22003`;
- evaluated child division or modulo by zero appends warning `1365` / SQLSTATE
  `22012`;
- allocation failure returns `MYLITE_NOMEM`; and
- public API misuse remains unchanged.

Unsupported for this slice:

- table-backed `SIGN(column)` and any `FROM` source other than `DUAL`;
- `SIGN()` nested inside arithmetic, comparison, logical, `IS`, `CASE`,
  control-flow functions, predicates, `ORDER BY`, `GROUP BY`, aggregate
  arguments, defaults, generated columns, and DML assignment expressions;
- string, decimal, float, hex, bit, binary-string, temporal, JSON, parameter,
  user-variable, system-variable, session-function, cast, collation, and
  subquery operands; and
- arbitrary SQLite pass-through.

## Tests

The test suite should cover:

- parser AST nodes for valid `SIGN(expr)` and wrong-arity forms;
- no-source and `FROM DUAL` projection for `NULL`, booleans, zero forms,
  positive integer values, negative integer values, signed boundaries,
  unsigned direct literal boundaries, direct exact decimal integer literals
  beyond unsigned 64-bit, arithmetic children, and bitwise children;
- `DO SIGN(...)` with no result rows and correct affected-row behavior;
- child division-by-zero warning staging and child overflow diagnostics;
- explicit aliases and generated column labels;
- deterministic rejection for strings, decimals, floats, hex literals, bit
  literals, table-backed columns, parameters, system variables, session
  functions, subqueries, CTEs, arithmetic over `SIGN()`, comparison over
  `SIGN()`, `CASE` results containing `SIGN()`, and `/`;
- file-backed preamble/catalog-generation/schema-generation safety;
- independent handles; and
- existing lexer, parser, scalar projection, arithmetic, modulo, `DIV`,
  bitwise, comparison, logical, `CASE`, `DO`, runtime, storage, and catalog
  tests.

Verification commands:

1. `cmake --build --preset dev`
2. focused parser/runtime CTest entries;
3. `packages/libmylite/tests/mysql_baseline_sign_function_expectations.sh`
4. `cmake --workflow --preset check`

## Compatibility Documentation

Update `COMPATIBILITY.md`,
`docs/compatibility/functions-numeric-math.md`,
`docs/compatibility/operators.md`,
`docs/compatibility/sql-query-expressions.md`, and
`docs/compatibility/sql-stored-programs.md` only for this limited
no-source/`DUAL`/`DO` numeric `SIGN()` subset. Do not imply support for full
MySQL `SIGN()`, decimal/float/string conversion, table-backed expression
evaluation, expression metadata, or general function composition.
