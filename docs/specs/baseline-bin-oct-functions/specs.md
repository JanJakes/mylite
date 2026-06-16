# Baseline BIN and OCT Functions

## Summary

This scalar phase admits a narrow MyLite-owned string-valued numeric conversion
surface for no-source, `FROM DUAL`, and `DO` execution:

```sql
SELECT BIN(base_value)[, OCT(base_value) ...]
SELECT BIN(base_value)[, OCT(base_value) ...] FROM DUAL
DO BIN(base_value)[, OCT(base_value) ...]
```

For the admitted integer-only argument domain, `BIN(value)` returns the
argument's unsigned 64-bit representation in base 2, `OCT(value)` returns it in
base 8, and either function returns `NULL` when the argument is `NULL`. This
matches the MySQL 8.4.9 behavior verified for the integer, boolean, `NULL`,
signed-64 scalar arithmetic, and limited numeric bitwise values already
admitted by MyLite's current scalar runtime.

This is not a general string or expression engine. A later row-backed slice
adds single-table integer-domain `BIN()` / `OCT()` projection and nesting. The
combined baseline still does not admit string/decimal/float/hex/bit row
coercion, casts outside the documented scalar subset, user variables,
parameters, subqueries, CTEs, exact expression metadata, or arbitrary SQLite
pass-through.

## Compatibility Authority

- Official MySQL 8.4 documentation:
  - String functions and operators:
    <https://dev.mysql.com/doc/refman/8.4/en/string-functions.html>
  - Arithmetic operators:
    <https://dev.mysql.com/doc/refman/8.4/en/arithmetic-functions.html>
  - Bit functions and operators:
    <https://dev.mysql.com/doc/refman/8.4/en/bit-functions.html>
  - Numeric literals:
    <https://dev.mysql.com/doc/refman/8.4/en/number-literals.html>
  - `SELECT` statement and `DUAL`:
    <https://dev.mysql.com/doc/refman/8.4/en/select.html>
  - `DO` statement:
    <https://dev.mysql.com/doc/refman/8.4/en/do.html>
- Observed MySQL 8.4.9 runtime behavior, captured by
  `packages/libmylite/tests/mysql_baseline_bin_oct_functions_expectations.sh`.

The MySQL 8.4 manual documents `BIN(N)` and `OCT(N)` as returning string
representations of a longlong integer argument in base 2 and base 8, and
returning `NULL` when `N` is `NULL`. Runtime probes against MySQL 8.4.9
establish these expectations for this slice:

- `BIN(NULL)` and `OCT(NULL)` return `NULL`;
- `TRUE` and `FALSE` behave as `1` and `0`;
- zero forms return `"0"`;
- positive values use no leading zeroes, for example `BIN(12)` returns
  `"1100"` and `OCT(12)` returns `"14"`;
- negative integer operands are converted through their unsigned 64-bit
  representation, so `BIN(-1)` returns 64 one bits and `OCT(-1)` returns
  `1777777777777777777777`;
- signed and unsigned boundary operands use unsigned 64-bit output:
  `BIN(-9223372036854775808)` and `BIN(9223372036854775808)` both return a
  one followed by 63 zeroes, while `OCT(-9223372036854775808)` and
  `OCT(9223372036854775808)` both return `1000000000000000000000`;
- bitwise child operands preserve their existing unsigned 64-bit results, for
  example `BIN(~0)` returns 64 one bits and `OCT(1<<64)` returns `0`;
- evaluated child division by zero returns `NULL` and records warning `1365` /
  SQLSTATE `22012`, `Division by 0`;
- child signed arithmetic overflow raises MySQL error `1690` / SQLSTATE
  `22003`;
- `BIN()`, `BIN(1,2)`, `OCT()`, and `OCT(1,2)` raise MySQL error `1582` /
  SQLSTATE `42000`, with the native function name in the message; and
- MySQL accepts broader forms such as strings, binary strings, hex literals,
  bit literals, decimals, floats, casts, and very large direct nonnegative
  literals that can emit truncation warning `1292`. Those remain deferred by
  this MyLite baseline. Single-table integer-domain row-backed columns are
  covered by `baseline-row-base-conversion-functions`.

## Ownership Boundaries

- Public API: unchanged. Successful supported `SELECT` statements return one
  row through existing result conventions; successful supported `DO`
  statements return a non-row result.
- Statement context: preserves existing `SELECT` and `DO` row-count behavior.
  Division-by-zero warnings from evaluated child arithmetic are staged and
  appended through the existing scalar warning path.
- Lexer/parser/AST: adds function-specific `BIN()` and `OCT()` AST nodes plus
  wrong-arity AST nodes, following existing native-function parser patterns.
- Analyzer/runtime: admits only top-level supported `BIN()` and `OCT()` scalar
  projection expressions and evaluates them in MyLite code. Each argument may
  use the admitted base-value domain described below.
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
SELECT base_conversion_item[, base_conversion_item ...]
SELECT base_conversion_item[, base_conversion_item ...] FROM DUAL
DO base_conversion_scalar[, base_conversion_scalar ...]

base_conversion_item:
    base_conversion_scalar
  | base_conversion_scalar AS alias
  | base_conversion_scalar alias

base_conversion_scalar:
    BIN ( base_value )
  | OCT ( base_value )
  | ( base_conversion_scalar )

base_value:
    decimal_integer_literal
  | + decimal_integer_literal
  | - decimal_integer_literal
  | TRUE
  | FALSE
  | NULL
  | scalar_arithmetic_expression
  | scalar_bitwise_expression
```

Direct nonnegative `decimal_integer_literal` input is admitted through the
unsigned 64-bit envelope `0` through `18446744073709551615`. Direct negative
integer literals are admitted down to `-9223372036854775808`, matching the
current signed source-literal envelope for numeric bitwise conversion.
Broader direct literal coercions that MySQL accepts, including unsigned-maximum
clamping with truncation warnings for very large nonnegative exact literals,
remain deferred.

`scalar_arithmetic_expression` is the current no-source/`DUAL` signed-64
arithmetic domain: decimal integer/boolean/`NULL` values, supported scalar
`IF()`/`IFNULL()`/`COALESCE()`/`NULLIF()`/`ISNULL()` values, parenthesized
admitted arithmetic, unary `+`/`-`, binary `+`, binary `-`, `*`, `%`, infix
`MOD`, `MOD(left, right)`, and infix `DIV`.

`scalar_bitwise_expression` is the current top-level numeric bitwise domain:
`~`, `&`, `|`, `^`, `<<`, and `>>` over admitted signed-64 scalar arithmetic
operands, with unsigned 64-bit results.

In this scalar phase, `BIN()` and `OCT()` are not admitted as children of
arithmetic, comparison, logical, `IS`, `CASE`, control-flow functions,
predicates, table-backed projection, ordering, grouping, or DML expressions.

### MyLite Lemon Snippet

The parser grammar is authored independently and extends MyLite's expression
grammar:

```lemon
expression(A) ::= BIN(T) LPAREN expression(B) RPAREN(R). {
    A = mylite_sql_parser_make_one_argument_function(
        state, T, MYLITE_SQL_AST_BIN_FUNCTION, B, R);
}
expression(A) ::= BIN(T) LPAREN RPAREN(R). {
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_BIN_ARGUMENT_COUNT_ERROR, NULL, R);
}
expression(A) ::= BIN(T) LPAREN expression(B) COMMA function_argument_list(C) RPAREN(R). {
    (void)B;
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_BIN_ARGUMENT_COUNT_ERROR, C, R);
}
expression(A) ::= OCT(T) LPAREN expression(B) RPAREN(R). {
    A = mylite_sql_parser_make_one_argument_function(
        state, T, MYLITE_SQL_AST_OCT_FUNCTION, B, R);
}
expression(A) ::= OCT(T) LPAREN RPAREN(R). {
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_OCT_ARGUMENT_COUNT_ERROR, NULL, R);
}
expression(A) ::= OCT(T) LPAREN expression(B) COMMA function_argument_list(C) RPAREN(R). {
    (void)B;
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_OCT_ARGUMENT_COUNT_ERROR, C, R);
}
identifier(A) ::= BIN(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
identifier(A) ::= OCT(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
```

These snippets describe MyLite's admitted grammar and are not copied from
MySQL grammar text.

## Runtime Semantics

Runtime evaluation is MyLite-owned and proportional to AST size.

1. Admit a `SELECT` or `DO` statement only when each selected or evaluated
   expression is an existing scalar expression or a top-level supported
   `BIN()` / `OCT()` expression.
2. Preserve existing native-function wrong-arity diagnostics before generic
   unsupported diagnostics.
3. Evaluate the function argument once.
4. Direct decimal integer literals are converted to an unsigned 64-bit value
   using the bounds in the syntax section. Nonnegative values bind directly;
   negative values use the same two's-complement unsigned 64-bit
   representation observed in MySQL.
5. Nonliteral arithmetic arguments are evaluated with the existing signed-64
   scalar arithmetic evaluator, preserving `NULL`, division-by-zero warnings,
   and overflow behavior. Non-`NULL` signed values are then converted to their
   unsigned 64-bit representation.
6. Bitwise arguments are evaluated with the existing scalar bitwise evaluator,
   preserving its `NULL` propagation, left-`NULL` short-circuiting, unsigned
   result representation, shift behavior, warning staging, and overflow
   behavior for evaluated children.
7. `NULL` input returns `NULL` and preserves staged child warnings.
8. `BIN()` formats non-`NULL` input as lowercase base-2 digits with no leading
   zeroes except that zero returns `"0"`.
9. `OCT()` formats non-`NULL` input as base-8 digits with no leading zeroes
   except that zero returns `"0"`.
10. Append staged division-by-zero warnings after scalar select-item or `DO`
    expression evaluation through the existing scalar warning path.

Supported in-range `BIN()` and `OCT()` statements report `warning_count == 0`
unless an evaluated child arithmetic expression stages an existing
division-by-zero warning. They do not touch catalog state, SQLite schema
generation, physical tables, or the `.mylite` preamble.

## Diagnostics

Supported successful statements return through existing result conventions.

Diagnostics for this baseline:

- syntax errors use the existing parser diagnostic surface;
- wrong arity uses MySQL error `1582` / SQLSTATE `42000` with native function
  name `BIN` or `OCT`;
- unsupported operands use a deterministic MyLite unsupported-feature
  diagnostic describing the admitted `BIN()` / `OCT()` subset;
- direct integer operands outside the admitted signed/unsigned literal
  envelope use a deterministic MyLite unsupported-feature diagnostic;
- child signed arithmetic overflow uses MySQL error `1690` / SQLSTATE
  `22003`;
- evaluated child division or modulo by zero appends warning `1365` / SQLSTATE
  `22012`;
- allocation failure returns `MYLITE_NOMEM`; and
- public API misuse remains unchanged.

Unsupported for this slice:

- table-backed `BIN(column)` / `OCT(column)` outside the later single-table
  integer-domain row-backed slice and any scalar `FROM` source other than
  `DUAL`;
- scalar-phase function results nested inside arithmetic, comparison, logical,
  `IS`, `CASE`, control-flow functions, predicates, `ORDER BY`, `GROUP BY`,
  aggregate arguments, defaults, generated columns, and DML assignment
  expressions, except where the later row-backed slice explicitly admits
  integer-domain projection/nesting;
- string, decimal, float, hex, bit, binary-string, temporal, JSON, parameter,
  user-variable, system-variable, session-function, cast, collation, and
  subquery operands;
- MySQL's broader direct exact numeric coercion for magnitudes outside the
  admitted signed/unsigned 64-bit literal envelope, including warning `1292`
  for very large nonnegative literals; and
- arbitrary SQLite pass-through.

## Tests

The test suite should cover:

- parser AST nodes for valid `BIN(expr)` / `OCT(expr)` and wrong-arity forms;
- no-source and `FROM DUAL` projection for `NULL`, booleans, zero forms,
  positive integer values, negative integer values, signed boundaries,
  unsigned direct literal boundaries, arithmetic children, and bitwise
  children;
- string result text and generated column labels;
- `DO BIN(...)` / `DO OCT(...)` with no result rows and correct affected-row
  behavior;
- child division-by-zero warning staging and child overflow diagnostics;
- explicit aliases;
- deterministic rejection for strings, decimals, floats, hex literals, bit
  literals, table-backed columns, parameters, system variables, session
  functions, subqueries, CTEs, arithmetic over `BIN()` / `OCT()`, comparison
  over `BIN()` / `OCT()`, `CASE` results containing `BIN()` / `OCT()`, and `/`;
- direct literal values just outside the admitted envelope;
- file-backed preamble/catalog-generation/schema-generation safety;
- independent handles; and
- existing lexer, parser, scalar projection, arithmetic, modulo, `DIV`,
  bitwise, comparison, logical, `CASE`, `DO`, runtime, storage, and catalog
  tests.

Verification commands:

1. `cmake --build --preset dev`
2. focused parser/runtime CTest entries;
3. `packages/libmylite/tests/mysql_baseline_bin_oct_functions_expectations.sh`
4. `cmake --workflow --preset check`

## Compatibility Documentation

Update `COMPATIBILITY.md`,
`docs/compatibility/functions-numeric-math.md`,
`docs/compatibility/functions-string.md`,
`docs/compatibility/operators.md`,
`docs/compatibility/sql-query-expressions.md`, and
`docs/compatibility/sql-stored-programs.md` for this limited scalar subset and
the later `baseline-row-base-conversion-functions` row subset. Do not imply
support for full MySQL string functions, string/decimal/float/hex/bit
conversion, broad table-backed expression evaluation, exact expression
metadata, or general function composition.
