# Baseline ROUND Function

## Summary

This phase admits the next narrow MyLite-owned integer-domain mathematical
function for no-source, `FROM DUAL`, and `DO` scalar execution:

```sql
SELECT ROUND(round_value)[, ROUND(round_value) ...]
SELECT ROUND(round_value)[, ROUND(round_value) ...] FROM DUAL
DO ROUND(round_value)[, ROUND(round_value) ...]
```

For the admitted integer-only argument domain, `ROUND(value)` returns the
argument unchanged, or `NULL` when the argument is `NULL`. This matches MySQL
8.4.9 for one-argument `ROUND()` over integer, boolean, `NULL`, signed-64
scalar arithmetic, and limited numeric bitwise values already admitted by the
current scalar runtime.

This phase deliberately does not implement `ROUND(value, places)` semantics.
MySQL accepts that two-argument form, including negative `places` values that
round digits left of the decimal point. MyLite parses the form so it can reject
it deterministically as an unsupported broader `ROUND()` shape instead of
misclassifying it as wrong arity. Decimal, floating-point, table-backed,
string, hex, bit-literal, cast, parameter, subquery, nested expression, and
arbitrary SQLite pass-through behavior remain deferred.

Later work in
[`baseline-round-places-function`](../baseline-round-places-function/specs.md)
expands this initial slice with a limited two-argument signed-integer
`ROUND(value, places)` subset. The rest of this document describes the original
one-argument baseline and the deliberate deferrals at that time.

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
  `packages/libmylite/tests/mysql_baseline_round_function_expectations.sh`.

The MySQL 8.4 manual documents `ROUND(X)` and `ROUND(X,D)`, with `D`
defaulting to zero. It also documents that `ROUND()` returns `NULL` when either
argument is `NULL`, and that integer first arguments return integer results.

Runtime probes against MySQL 8.4.9 establish these expectations for this slice:

- `ROUND(NULL)` returns `NULL`;
- `TRUE` and `FALSE` return `1` and `0`;
- `ROUND(0)`, `ROUND(-0)`, and `ROUND(+0)` return `0`;
- one-argument exact integer inputs keep their integer value, including signed
  and unsigned boundary examples such as `ROUND(9223372036854775807)`,
  `ROUND(-9223372036854775808)`, `ROUND(9223372036854775808)`, and
  `ROUND(18446744073709551615)`;
- direct exact decimal integer literals beyond unsigned 64-bit can keep their
  text value in MySQL for admitted 81-significant-digit probes, while broader
  very-large numeric inputs enter MySQL's wider precision and truncation
  behavior. This baseline admits only the existing MyLite scalar
  literal-projection envelope of at most 81 significant decimal digits;
- bitwise operands keep their unsigned 64-bit integer result, for example
  `ROUND(~0)` returns `18446744073709551615` and `ROUND(1<<64)` returns `0`;
- evaluated child division by zero returns `NULL` and records warning `1365` /
  SQLSTATE `22012`, `Division by 0`;
- child signed arithmetic overflow raises MySQL error `1690` / SQLSTATE
  `22003`;
- `ROUND()` and `ROUND(1,2,3)` raise MySQL error `1582` / SQLSTATE `42000`,
  with `ROUND` as the native function name;
- MySQL accepts `ROUND(X,D)`, including `D = 0`, positive `D`, negative `D`,
  boolean `D`, and `NULL` `D`. MyLite defers that form in this phase; and
- MySQL accepts broader forms such as strings, binary strings, hex literals,
  bit literals, decimals, floats, casts, and table-backed columns. Those remain
  deferred by this MyLite baseline.

## Ownership Boundaries

- Public API: unchanged. Successful supported `SELECT` statements return one
  row through existing result conventions; successful supported `DO` statements
  return a non-row result.
- Statement context: preserves existing `SELECT` and `DO` row-count behavior.
  Division-by-zero warnings from evaluated child arithmetic are staged and
  appended through the existing scalar warning path.
- Lexer/parser/AST: adds `ROUND` token support, `ROUND()` AST nodes, and a
  wrong-arity AST node. `ROUND(value, places)` is parsed as a `ROUND` function
  node with two children so runtime can reject the valid-but-deferred MySQL
  form deterministically.
- Analyzer/runtime: admits only top-level one-argument `ROUND()` scalar
  projection expressions and evaluates them in MyLite code. The argument may
  use the admitted integer round-value domain described below.
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
SELECT round_item[, round_item ...]
SELECT round_item[, round_item ...] FROM DUAL
DO round_scalar[, round_scalar ...]

round_item:
    round_scalar
  | round_scalar AS alias
  | round_scalar alias

round_scalar:
    ROUND ( round_value )
  | ( round_scalar )

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

The parser also recognizes this MySQL-valid but deferred form:

```sql
ROUND ( round_value , round_places )
```

`round_places` is not evaluated in this phase. Any two-argument `ROUND()` form
is rejected by runtime with the deterministic unsupported `ROUND()` diagnostic.

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

`ROUND()` is not admitted as a child of arithmetic, comparison, logical, `IS`,
`CASE`, control-flow functions, predicates, table-backed projection, ordering,
grouping, or DML expressions in this phase.

### MyLite Lemon Snippet

The parser grammar is authored independently and extends MyLite's expression
grammar:

```lemon
expression(A) ::= ROUND(T) LPAREN expression(B) RPAREN(R). {
    A = mylite_sql_parser_make_one_argument_function(
        state, T, MYLITE_SQL_AST_ROUND_FUNCTION, B, R);
}
expression(A) ::= ROUND(T) LPAREN expression(B) COMMA expression(C) RPAREN(R). {
    A = mylite_sql_parser_make_two_argument_function(
        state, T, MYLITE_SQL_AST_ROUND_FUNCTION, B, C, R);
}
expression(A) ::= ROUND(T) LPAREN RPAREN(R). {
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_ROUND_ARGUMENT_COUNT_ERROR, NULL, R);
}
expression(A) ::=
    ROUND(T) LPAREN expression(B) COMMA expression(C) COMMA function_argument_list(D) RPAREN(R). {
    (void)B;
    (void)C;
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_ROUND_ARGUMENT_COUNT_ERROR, D, R);
}
identifier(A) ::= ROUND(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
```

These snippets describe MyLite's admitted grammar and parser recognition for a
deferred valid MySQL form. They are not copied from MySQL grammar text.

## Runtime Semantics

Runtime evaluation is MyLite-owned and proportional to AST size.

1. Admit a `SELECT` or `DO` statement only when each selected or evaluated
   expression is an existing scalar expression or a top-level supported
   one-argument `ROUND()` expression.
2. Preserve existing native-function wrong-arity diagnostics before generic
   unsupported diagnostics.
3. Reject two-argument `ROUND(value, places)` with the deterministic
   unsupported `ROUND()` diagnostic. It is a valid MySQL shape, but it requires
   decimal-place rounding semantics outside this phase.
4. Evaluate the one supported function argument once.
5. Direct decimal integer literals are normalized using the existing MyLite
   literal projection normalizer, within the 81-significant-digit envelope.
6. Nonliteral arithmetic arguments are evaluated with the existing signed-64
   scalar arithmetic evaluator, preserving `NULL`, division-by-zero warnings,
   and overflow behavior. Non-`NULL` signed values are returned as canonical
   signed decimal text.
7. Bitwise arguments are evaluated with the existing scalar bitwise evaluator,
   preserving its `NULL` propagation, left-`NULL` short-circuiting, unsigned
   result representation, shift behavior, warning staging, and overflow
   behavior for evaluated children. Non-`NULL` bitwise results are returned as
   canonical unsigned decimal text.
8. `NULL` input returns `NULL` and preserves staged child warnings.
9. Append staged division-by-zero warnings after scalar select-item or `DO`
   expression evaluation through the existing scalar warning path.

Supported in-range `ROUND()` statements report `warning_count == 0` unless an
evaluated child arithmetic expression stages an existing division-by-zero
warning. They do not touch catalog state, SQLite schema generation, physical
tables, or the `.mylite` preamble.

## Diagnostics

Supported successful statements return through existing result conventions.

Diagnostics for this baseline:

- syntax errors use the existing parser diagnostic surface;
- wrong arity for zero arguments or three or more arguments uses MySQL error
  `1582` / SQLSTATE `42000` with native function name `ROUND`;
- two-argument `ROUND(value, places)` uses a deterministic MyLite
  unsupported-feature diagnostic describing the admitted one-argument subset;
- unsupported one-argument operands use the same deterministic `ROUND()`
  unsupported-feature diagnostic;
- direct integer operands outside the admitted 81-significant-digit literal
  envelope use the existing MyLite unsupported literal-projection diagnostic;
- child signed arithmetic overflow uses MySQL error `1690` / SQLSTATE
  `22003`;
- evaluated child division or modulo by zero appends warning `1365` / SQLSTATE
  `22012`;
- allocation failure returns `MYLITE_NOMEM`; and
- public API misuse remains unchanged.

Unsupported for this slice:

- `ROUND(value, places)` evaluation;
- table-backed `ROUND(column)` and any `FROM` source other than `DUAL`;
- `ROUND()` nested inside arithmetic, comparison, logical, `IS`, `CASE`,
  control-flow functions, predicates, `ORDER BY`, `GROUP BY`, aggregate
  arguments, defaults, generated columns, and DML assignment expressions;
- string, decimal, float, hex, bit, binary-string, temporal, JSON, parameter,
  user-variable, system-variable, session-function, cast, collation, and
  subquery operands;
- direct exact decimal magnitudes above 81 significant digits; and
- arbitrary SQLite pass-through.

## Tests

The test suite should cover:

- parser AST nodes for valid one-argument `ROUND(expr)`, recognized
  two-argument `ROUND(expr, places)`, and wrong-arity forms;
- parser recognition that `ROUND` remains usable as an unquoted identifier
  where MyLite's keyword policy permits it;
- no-source and `FROM DUAL` projection for `NULL`, booleans, zero forms,
  positive integer values, negative integer values, signed boundaries,
  unsigned direct literal boundaries, direct exact decimal integer literals in
  the admitted 81-significant-digit envelope, arithmetic children, and bitwise
  children;
- `DO ROUND(...)` with no result rows and existing affected-row behavior;
- child division-by-zero warning staging and child overflow diagnostics;
- explicit aliases and generated column labels;
- deterministic rejection for two-argument `ROUND()`, strings, decimals,
  floats, hex literals, bit literals, table-backed columns, parameters, system
  variables, nested arithmetic use, comparison use, `CASE` children, and direct
  exact decimal integer literals outside the admitted envelope;
- `.mylite` preamble preservation and unchanged catalog/schema generation for
  file-backed handles; and
- independent file-backed handles producing independent scalar results.

Verification commands:

```sh
./packages/libmylite/tests/mysql_baseline_round_function_expectations.sh
cmake --build --preset dev
ctest --test-dir build/dev --output-on-failure -R 'libmylite\\.(parser|runtime\\.round_function|runtime\\.ceil_floor_functions|runtime\\.scalar_division_projection|runtime\\.scalar_bitwise_projection|runtime\\.scalar_expression_projection)'
cmake --workflow --preset check
```

## Compatibility Documentation

Update `COMPATIBILITY.md`, `docs/compatibility/functions-numeric-math.md`,
`docs/compatibility/operators.md`,
`docs/compatibility/sql-query-expressions.md`,
`docs/compatibility/sql-stored-programs.md`, and
`docs/compatibility/type-system-literals-conversion.md` only for the exact
one-argument integer-domain `ROUND()` subset. Do not claim support for
two-argument rounding, decimal/floating rounding, table-backed expression
evaluation, metadata inference, arbitrary expression composition, or SQLite
pass-through.
