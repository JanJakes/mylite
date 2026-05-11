# Baseline BIT_COUNT Function

## Summary

This phase admits a narrow MyLite-owned numeric `BIT_COUNT()` expression surface
for no-source, `FROM DUAL`, and `DO` scalar execution:

```sql
SELECT BIT_COUNT(bit_count_value)[, BIT_COUNT(bit_count_value) ...]
SELECT BIT_COUNT(bit_count_value)[, BIT_COUNT(bit_count_value) ...] FROM DUAL
DO BIT_COUNT(bit_count_value)[, BIT_COUNT(bit_count_value) ...]
```

The function returns the number of set bits in the admitted argument's unsigned
64-bit representation, or `NULL` for a `NULL` argument. This is not a general
function or expression engine. The phase does not admit table-backed
`BIT_COUNT(column)`, string/decimal/float/hex/bit/binary-string conversions,
casts, user variables, parameters, subqueries, CTEs, expression metadata,
`BIT_COUNT()` nested inside arithmetic/comparison/logical/`CASE` parents, or
arbitrary SQLite pass-through.

## Compatibility Authority

- Official MySQL 8.4 documentation:
  - Bit functions and operators:
    <https://dev.mysql.com/doc/refman/8.4/en/bit-functions.html>
  - `SELECT` statement and `DUAL`:
    <https://dev.mysql.com/doc/refman/8.4/en/select.html>
  - `DO` statement:
    <https://dev.mysql.com/doc/refman/8.4/en/do.html>
- Observed MySQL 8.4.9 runtime behavior, captured by
  `packages/libmylite/tests/mysql_baseline_bit_count_function_expectations.sh`.

The MySQL 8.4 manual documents `BIT_COUNT(N)` as returning the set-bit count of
`N` as an unsigned 64-bit integer, or `NULL` when `N` is `NULL`. Runtime probes
against MySQL 8.4.9 establish these expectations for this slice:

- `BIT_COUNT(NULL)` returns `NULL`;
- `BIT_COUNT(0)`, `BIT_COUNT(1)`, `BIT_COUNT(64)`, and `BIT_COUNT(127)` return
  `0`, `1`, `1`, and `7`;
- negative integer operands are counted in their unsigned 64-bit
  representation, so `BIT_COUNT(-1)` returns `64` and `BIT_COUNT(-2)` returns
  `63`;
- signed boundary operands behave as unsigned 64-bit values:
  `BIT_COUNT(-9223372036854775808)` returns `1` and
  `BIT_COUNT(9223372036854775807)` returns `63`;
- unsigned decimal literals outside the signed range are admitted directly for
  this function, so `BIT_COUNT(9223372036854775808)` returns `1` and
  `BIT_COUNT(18446744073709551615)` returns `64`;
- `TRUE` and `FALSE` behave as `1` and `0`;
- `BIT_COUNT()` and `BIT_COUNT(a, b)` raise MySQL error `1582` / SQLSTATE
  `42000`, "Incorrect parameter count in the call to native function
  'BIT_COUNT'";
- evaluated child division by zero returns `NULL` and records warning `1365` /
  SQLSTATE `22012`, `Division by 0`;
- MySQL accepts broader forms such as strings, binary strings, hex literals,
  bit literals, decimals, floats, casts, and table-backed columns. Those remain
  deferred by this MyLite baseline.

## Ownership Boundaries

- Public API: unchanged. Successful supported `SELECT` statements return one
  row through existing result conventions; successful `DO` statements return a
  non-row result.
- Statement context: preserves existing `SELECT` and `DO` row-count behavior.
  Division-by-zero warnings from evaluated child arithmetic are staged and
  appended through the same scalar warning path as modulo, `DIV`, and bitwise.
- Lexer/parser/AST: adds a function-specific `BIT_COUNT()` AST node plus a
  wrong-arity AST node, following existing native-function parser patterns.
- Analyzer/runtime: admits only top-level `BIT_COUNT()` scalar projection
  expressions and evaluates them in MyLite code. The function argument may use
  the admitted bit-count value domain described below.
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
SELECT bit_count_item[, bit_count_item ...]
SELECT bit_count_item[, bit_count_item ...] FROM DUAL
DO bit_count_scalar[, bit_count_scalar ...]

bit_count_item:
    bit_count_scalar
  | bit_count_scalar AS alias
  | bit_count_scalar alias

bit_count_scalar:
    BIT_COUNT ( bit_count_value )
  | ( bit_count_scalar )

bit_count_value:
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
`18446744073709551615` for nonnegative forms. Negative direct literals are
admitted down to `-9223372036854775808`, matching the signed 64-bit minimum
source form for this phase. Other arithmetic expressions retain the existing
signed-64 scalar arithmetic range and overflow policy.

`scalar_arithmetic_expression` is the current no-source/`DUAL` signed-64
arithmetic domain: decimal integer/boolean/`NULL` values, supported scalar
`IF()`/`IFNULL()`/`COALESCE()`/`NULLIF()`/`ISNULL()` values, parenthesized
admitted arithmetic, unary `+`/`-`, binary `+`, binary `-`, `*`, `%`, infix
`MOD`, `MOD(left, right)`, and infix `DIV`.

`scalar_bitwise_expression` is the current top-level numeric bitwise domain:
`~`, `&`, `|`, `^`, `<<`, and `>>` over admitted signed-64 scalar arithmetic
operands, with unsigned 64-bit results.

`BIT_COUNT()` itself is not admitted as a child of arithmetic, comparison,
logical, `IS`, `CASE`, control-flow functions, predicates, table-backed
projection, ordering, grouping, or DML expressions in this phase.

### MyLite Lemon Snippet

The parser grammar is authored independently and extends MyLite's expression
grammar:

```lemon
expression(A) ::= BIT_COUNT(T) LPAREN expression(B) RPAREN(R). {
    A = mylite_sql_parser_make_one_argument_function(
        state, T, MYLITE_SQL_AST_BIT_COUNT_FUNCTION, B, R);
}
expression(A) ::= BIT_COUNT(T) LPAREN RPAREN(R). {
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_BIT_COUNT_ARGUMENT_COUNT_ERROR, NULL, R);
}
expression(A) ::= BIT_COUNT(T) LPAREN expression(B) COMMA function_argument_list(C) RPAREN(R). {
    (void)B;
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_BIT_COUNT_ARGUMENT_COUNT_ERROR, C, R);
}
```

These snippets describe MyLite's admitted grammar and are not copied from
MySQL grammar text.

## Runtime Semantics

Runtime evaluation is MyLite-owned and proportional to AST size.

1. Admit a `SELECT` or `DO` statement only when each selected or evaluated
   expression is an existing scalar expression or a top-level supported
   `BIT_COUNT()` expression.
2. Preserve existing native-function wrong-arity diagnostics before generic
   unsupported diagnostics.
3. Evaluate the `BIT_COUNT()` argument once.
4. Direct decimal integer literals are converted to an unsigned 64-bit value
   using the bounds in the syntax section.
5. Nonliteral arithmetic arguments are evaluated with the existing signed-64
   scalar arithmetic evaluator, preserving `NULL`, division-by-zero warnings,
   and overflow behavior. Non-`NULL` signed values are then converted to their
   unsigned 64-bit representation.
6. Bitwise arguments are evaluated with the existing scalar bitwise evaluator,
   preserving its `NULL` propagation, left-`NULL` short-circuiting, unsigned
   result representation, shift behavior, warning staging, and overflow
   behavior for evaluated children.
7. `NULL` input returns `NULL` and preserves staged child warnings.
8. Non-`NULL` input returns the population count as canonical unsigned decimal
   text. The maximum result is `64`.
9. Append staged division-by-zero warnings after scalar select-item or `DO`
   expression evaluation through the existing scalar warning path.

Supported in-range `BIT_COUNT()` statements report `warning_count == 0` unless
an evaluated child arithmetic expression stages an existing division-by-zero
warning. They do not touch catalog state, SQLite schema generation, physical
tables, or the `.mylite` preamble.

## Diagnostics

Supported successful statements return through existing result conventions.

Diagnostics for this baseline:

- syntax errors use the existing parser diagnostic surface;
- `BIT_COUNT()` wrong arity uses MySQL error `1582` / SQLSTATE `42000`;
- unsupported operands use a deterministic MyLite unsupported-feature
  diagnostic describing the admitted `BIT_COUNT()` subset;
- direct integer operands outside the admitted signed/unsigned literal envelope
  use a deterministic MyLite unsupported-feature diagnostic;
- child signed arithmetic overflow uses MySQL error `1690` / SQLSTATE `22003`;
- evaluated child division or modulo by zero appends warning `1365` / SQLSTATE
  `22012`;
- allocation failure returns `MYLITE_NOMEM`; and
- public API misuse remains unchanged.

Unsupported for this slice:

- table-backed `BIT_COUNT(column)` and any `FROM` source other than `DUAL`;
- `BIT_COUNT()` nested inside arithmetic, comparison, logical, `IS`, `CASE`,
  control-flow functions, predicates, `ORDER BY`, `GROUP BY`, aggregate
  arguments, defaults, generated columns, and DML assignment expressions;
- string, decimal, float, hex, bit, binary-string, temporal, JSON, parameter,
  user-variable, system-variable, session-function, cast, collation, and
  subquery operands;
- binary-string set-bit counting for more than 64 bits; and
- arbitrary SQLite pass-through.

## Tests

The test suite should cover:

- parser AST nodes for valid `BIT_COUNT(expr)` and wrong-arity forms;
- no-source and `FROM DUAL` projection for `NULL`, booleans, positive integer
  values, negative integer values, signed boundaries, unsigned max, arithmetic
  children, and bitwise children;
- `DO BIT_COUNT(...)` with no result rows and correct affected-row behavior;
- child division-by-zero warning staging and child overflow diagnostics;
- explicit aliases and generated column labels;
- deterministic rejection for strings, decimals, floats, hex literals, bit
  literals, table-backed columns, parameters, system variables, session
  functions, subqueries, CTEs, arithmetic over `BIT_COUNT()`, comparison over
  `BIT_COUNT()`, `CASE` results containing `BIT_COUNT()`, and `/`;
- file-backed preamble/catalog-generation/schema-generation safety;
- independent handles; and
- existing lexer, parser, scalar projection, arithmetic, modulo, `DIV`, bitwise,
  comparison, logical, `CASE`, `DO`, runtime, storage, and catalog tests.

Verification commands:

1. `cmake --build --preset dev`
2. focused parser/runtime CTest entries;
3. `packages/libmylite/tests/mysql_baseline_bit_count_function_expectations.sh`
4. `cmake --workflow --preset check`

## Compatibility Documentation

Update `COMPATIBILITY.md`,
`docs/compatibility/functions-numeric-math.md`,
`docs/compatibility/operators.md`,
`docs/compatibility/sql-query-expressions.md`, and
`docs/compatibility/sql-stored-programs.md` only for this limited
no-source/`DUAL`/`DO` numeric `BIT_COUNT()` subset. Do not imply support for
full MySQL `BIT_COUNT()`, binary-string operands, table-backed expression
evaluation, expression metadata, or general function composition.
