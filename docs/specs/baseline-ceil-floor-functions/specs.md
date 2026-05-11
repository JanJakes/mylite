# Baseline CEIL, CEILING, and FLOOR Functions

## Summary

This phase admits a narrow MyLite-owned integer-domain rounding function
surface for no-source, `FROM DUAL`, and `DO` scalar execution:

```sql
SELECT CEIL(round_value)[, CEILING(round_value), FLOOR(round_value) ...]
SELECT CEIL(round_value)[, CEILING(round_value), FLOOR(round_value) ...] FROM DUAL
DO CEIL(round_value)[, CEILING(round_value), FLOOR(round_value) ...]
```

`CEIL()` and `CEILING()` are synonyms. For the admitted integer-only argument
domain, `CEIL()`, `CEILING()`, and `FLOOR()` return the argument unchanged, or
`NULL` when the argument is `NULL`. This is not a general numeric expression
engine. The phase does not admit decimal or floating-point rounding,
table-backed `CEIL(column)` / `FLOOR(column)`, string/hex/bit conversion,
casts, user variables, parameters, subqueries, CTEs, expression metadata,
nesting these functions inside arithmetic/comparison/logical/`CASE` parents,
or arbitrary SQLite pass-through.

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
  `packages/libmylite/tests/mysql_baseline_ceil_floor_functions_expectations.sh`.

The MySQL 8.4 manual documents `CEIL(X)` as a synonym for `CEILING(X)`,
documents `CEILING(X)` as returning the smallest integer value not less than
`X`, documents `FLOOR(X)` as returning the largest integer value not greater
than `X`, and documents `NULL` input as returning `NULL`.

Runtime probes against MySQL 8.4.9 establish these expectations for this slice:

- `CEIL(NULL)`, `CEILING(NULL)`, and `FLOOR(NULL)` return `NULL`;
- `TRUE` and `FALSE` return `1` and `0`;
- `CEIL(0)`, `CEIL(-0)`, `CEIL(+0)`, `FLOOR(-0)`, and `FLOOR(+0)` return
  `0`;
- exact integer literals keep their integer value, including signed and
  unsigned boundary examples such as `CEIL(9223372036854775808)`,
  `FLOOR(-9223372036854775809)`, `CEIL(18446744073709551615)`, and
  `FLOOR(-18446744073709551615)`;
- observed direct decimal integer literals beyond unsigned 64-bit can keep
  their text value in MySQL, while broader very-large numeric inputs enter
  MySQL's wider numeric precision behavior. This baseline admits only the
  existing MyLite scalar literal-projection envelope of at most 81 significant
  decimal digits and verifies the admitted cases against MySQL 8.4.9;
- bitwise operands keep their unsigned 64-bit integer result, for example
  `CEILING(~0)` returns `18446744073709551615` and `CEIL(1<<64)` returns `0`;
- `CEIL()`, `CEILING()`, `FLOOR()` and their two-argument forms raise MySQL
  error `1582` / SQLSTATE `42000`, with the called native function name in the
  message;
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
  `ABS()`, `SIGN()`, and `BIT_COUNT()`.
- Lexer/parser/AST: adds function-specific AST nodes and wrong-arity AST nodes
  for `CEIL()`, `CEILING()`, and `FLOOR()`, following existing native-function
  parser patterns.
- Analyzer/runtime: admits only top-level rounding scalar projection
  expressions and evaluates them in MyLite code. The function argument may use
  the admitted integer round-value domain described below.
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
SELECT rounding_item[, rounding_item ...]
SELECT rounding_item[, rounding_item ...] FROM DUAL
DO rounding_scalar[, rounding_scalar ...]

rounding_item:
    rounding_scalar
  | rounding_scalar AS alias
  | rounding_scalar alias

rounding_scalar:
    CEIL ( round_value )
  | CEILING ( round_value )
  | FLOOR ( round_value )
  | ( rounding_scalar )

round_value:
    decimal_integer_literal
  | + decimal_integer_literal
  | - decimal_integer_literal
  | TRUE
  | FALSE
  | NULL
  | scalar_arithmetic_expression
  | scalar_bitwise_expression
```

Direct `decimal_integer_literal` input is normalized with the same current
MyLite literal projection rule: leading zeroes are removed, negative zero
normalizes to `0`, and at most 81 significant decimal digits are admitted.
That keeps this slice inside the fixed `session_scalar_cell` storage contract.
Broader exact/approximate numeric precision behavior remains deferred.

`scalar_arithmetic_expression` is the current no-source/`DUAL` signed-64
arithmetic domain: decimal integer/boolean/`NULL` values, supported scalar
`IF()`/`IFNULL()`/`COALESCE()`/`NULLIF()`/`ISNULL()` values, parenthesized
admitted arithmetic, unary `+`/`-`, binary `+`, binary `-`, `*`, `%`, infix
`MOD`, `MOD(left, right)`, and infix `DIV`.

`scalar_bitwise_expression` is the current top-level numeric bitwise domain:
`~`, `&`, `|`, `^`, `<<`, and `>>` over admitted signed-64 scalar arithmetic
operands, with unsigned 64-bit results.

`CEIL()`, `CEILING()`, and `FLOOR()` themselves are not admitted as children of
arithmetic, comparison, logical, `IS`, `CASE`, control-flow functions,
predicates, table-backed projection, ordering, grouping, or DML expressions in
this phase.

### MyLite Lemon Snippet

The parser grammar is authored independently and extends MyLite's expression
grammar:

```lemon
expression(A) ::= CEIL(T) LPAREN expression(B) RPAREN(R). {
    A = mylite_sql_parser_make_one_argument_function(
        state, T, MYLITE_SQL_AST_CEIL_FUNCTION, B, R);
}
expression(A) ::= CEILING(T) LPAREN expression(B) RPAREN(R). {
    A = mylite_sql_parser_make_one_argument_function(
        state, T, MYLITE_SQL_AST_CEILING_FUNCTION, B, R);
}
expression(A) ::= FLOOR(T) LPAREN expression(B) RPAREN(R). {
    A = mylite_sql_parser_make_one_argument_function(
        state, T, MYLITE_SQL_AST_FLOOR_FUNCTION, B, R);
}
expression(A) ::= CEIL(T) LPAREN RPAREN(R). {
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_CEIL_ARGUMENT_COUNT_ERROR, NULL, R);
}
expression(A) ::= CEIL(T) LPAREN expression(B) COMMA function_argument_list(C) RPAREN(R). {
    (void)B;
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_CEIL_ARGUMENT_COUNT_ERROR, C, R);
}
```

Equivalent wrong-arity productions are added for `CEILING()` and `FLOOR()`.
These snippets describe MyLite's admitted grammar and are not copied from
MySQL grammar text.

## Runtime Semantics

Runtime evaluation is MyLite-owned and proportional to AST size.

1. Admit a `SELECT` or `DO` statement only when each selected or evaluated
   expression is an existing scalar expression or a top-level supported
   rounding function expression.
2. Preserve existing native-function wrong-arity diagnostics before generic
   unsupported diagnostics.
3. Evaluate the function argument once.
4. Direct decimal integer literals are normalized using the existing MyLite
   literal projection normalizer, within the 81-significant-digit envelope.
5. Nonliteral arithmetic arguments are evaluated with the existing signed-64
   scalar arithmetic evaluator, preserving `NULL`, division-by-zero warnings,
   and overflow behavior. Non-`NULL` signed values are returned as canonical
   signed decimal text.
6. Bitwise arguments are evaluated with the existing scalar bitwise evaluator,
   preserving its `NULL` propagation, left-`NULL` short-circuiting, unsigned
   result representation, shift behavior, warning staging, and overflow
   behavior for evaluated children. Non-`NULL` bitwise results are returned as
   canonical unsigned decimal text.
7. `NULL` input returns `NULL` and preserves staged child warnings.
8. Append staged division-by-zero warnings after scalar select-item or `DO`
   expression evaluation through the existing scalar warning path.

Supported in-range rounding statements report `warning_count == 0` unless an
evaluated child arithmetic expression stages an existing division-by-zero
warning. They do not touch catalog state, SQLite schema generation, physical
tables, or the `.mylite` preamble.

## Diagnostics

Supported successful statements return through existing result conventions.

Diagnostics for this baseline:

- syntax errors use the existing parser diagnostic surface;
- wrong arity uses MySQL error `1582` / SQLSTATE `42000` with the called
  native function name: `CEIL`, `CEILING`, or `FLOOR`;
- unsupported operands use a deterministic MyLite unsupported-feature
  diagnostic describing the admitted rounding-function subset;
- direct integer operands outside the admitted 81-significant-digit literal
  envelope use the existing MyLite unsupported literal-projection diagnostic;
- child signed arithmetic overflow uses MySQL error `1690` / SQLSTATE
  `22003`;
- evaluated child division or modulo by zero appends warning `1365` / SQLSTATE
  `22012`;
- allocation failure returns `MYLITE_NOMEM`; and
- public API misuse remains unchanged.

Unsupported for this slice:

- table-backed `CEIL(column)` / `CEILING(column)` / `FLOOR(column)` and any
  `FROM` source other than `DUAL`;
- these functions nested inside arithmetic, comparison, logical, `IS`, `CASE`,
  control-flow functions, predicates, `ORDER BY`, `GROUP BY`, aggregate
  arguments, defaults, generated columns, and DML assignment expressions;
- string, decimal, float, hex, bit, binary-string, temporal, JSON, parameter,
  user-variable, system-variable, session-function, cast, collation, and
  subquery operands;
- direct exact decimal magnitudes above 81 significant digits; and
- arbitrary SQLite pass-through.

## Tests

The test suite should cover:

- parser AST nodes for valid `CEIL(expr)`, `CEILING(expr)`, `FLOOR(expr)`, and
  wrong-arity forms;
- no-source and `FROM DUAL` projection for `NULL`, booleans, zero forms,
  positive integer values, negative integer values, signed boundaries,
  unsigned direct literal boundaries, direct exact decimal integer literals in
  the admitted 81-significant-digit envelope, arithmetic children, and bitwise
  children;
- `DO CEIL(...)` / `DO FLOOR(...)` with no result rows and correct affected-row
  behavior;
- child division-by-zero warning staging and child overflow diagnostics;
- explicit aliases and generated column labels;
- deterministic rejection for strings, decimals, floats, hex literals, bit
  literals, table-backed columns, parameters, system variables, session
  functions, subqueries, CTEs, arithmetic over these functions, comparison over
  these functions, `CASE` results containing these functions, and `/`;
- direct exact decimal literal inputs above the admitted 81-significant-digit
  envelope;
- file-backed preamble/catalog-generation/schema-generation safety;
- independent handles; and
- existing lexer, parser, scalar projection, arithmetic, modulo, `DIV`,
  bitwise, comparison, logical, `CASE`, `DO`, runtime, storage, and catalog
  tests.

Verification commands:

1. `cmake --build --preset dev`
2. focused parser/runtime CTest entries;
3. `packages/libmylite/tests/mysql_baseline_ceil_floor_functions_expectations.sh`
4. `cmake --workflow --preset check`

## Compatibility Documentation

Update `COMPATIBILITY.md`,
`docs/compatibility/functions-numeric-math.md`,
`docs/compatibility/operators.md`,
`docs/compatibility/sql-query-expressions.md`, and
`docs/compatibility/sql-stored-programs.md` only for this limited
no-source/`DUAL`/`DO` integer-domain `CEIL()` / `CEILING()` / `FLOOR()`
subset. Do not imply support for full MySQL rounding, decimal/float/string
conversion, table-backed expression evaluation, expression metadata, or
general function composition.
