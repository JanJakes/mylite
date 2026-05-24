# Baseline ROUND Places Function

## Summary

This phase expands the existing MyLite-owned `ROUND()` scalar slice from
one-argument integer identity to a narrow two-argument integer-domain subset:

```sql
SELECT ROUND(round_value, round_places)[, ...]
SELECT ROUND(round_value, round_places)[, ...] FROM DUAL
DO ROUND(round_value, round_places)[, ...]
```

The admitted form remains a top-level no-source, `FROM DUAL`, or `DO` scalar
expression. `round_value` reuses the current one-argument integer-domain
`ROUND()` input envelope. `round_places` reuses the current signed-64 scalar
arithmetic envelope. When `round_places` is nonnegative, the result is the
unchanged integer value, matching MySQL's exact-integer behavior. When
`round_places` is negative, this phase rounds only values that can be evaluated
inside MyLite's signed-64 scalar arithmetic envelope; broader exact decimal,
unsigned-boundary, string, decimal, float, hex, bit-literal, table-backed, and
expression-typed `ROUND(value, places)` behavior remains deferred.

## Compatibility Authority

- Official MySQL 8.4 documentation:
  - Mathematical functions:
    <https://dev.mysql.com/doc/refman/8.4/en/mathematical-functions.html>
  - Arithmetic operators:
    <https://dev.mysql.com/doc/refman/8.4/en/arithmetic-functions.html>
  - Numeric literals:
    <https://dev.mysql.com/doc/refman/8.4/en/number-literals.html>
  - `SELECT` and `DUAL`:
    <https://dev.mysql.com/doc/refman/8.4/en/select.html>
  - `DO`:
    <https://dev.mysql.com/doc/refman/8.4/en/do.html>
- Observed MySQL 8.4.9 runtime behavior, captured by
  `packages/libmylite/tests/mysql_baseline_round_function_expectations.sh`.

MySQL documents `ROUND(X)` and `ROUND(X,D)`, with `D` defaulting to zero. Runtime
probes against MySQL 8.4.9 establish the behavior this phase implements:

- `ROUND(123, 0)`, `ROUND(123, 2)`, `ROUND(123, TRUE)`, and
  `ROUND(123, FALSE)` return `123`;
- `ROUND(NULL, 1)` and `ROUND(123, NULL)` return `NULL`;
- negative places round left of the decimal point using half-away-from-zero
  behavior for exact integer inputs, such as `ROUND(123,-1) = 120`,
  `ROUND(999,-2) = 1000`, `ROUND(5,-1) = 10`, and
  `ROUND(-5,-1) = -10`;
- very negative places may round a signed-64 value to `0`, such as
  `ROUND(123,-31) = 0`;
- child expressions are evaluated and their existing warnings are staged; for
  example `ROUND(5 DIV 0, NULL)` returns `NULL` and appends the existing
  division-by-zero warning;
- signed result overflow raises MySQL error `1690` / SQLSTATE `22003`; and
- MySQL accepts broader exact decimal and unsigned-boundary forms, but those
  require a larger decimal numeric layer than this phase introduces.

## Ownership Boundaries

- Public API: unchanged. Successful supported `SELECT` statements return rows
  through existing scalar result conventions; successful supported `DO`
  statements return non-row results.
- Statement context and diagnostics: existing scalar warning staging is reused.
  Warnings from evaluated `round_value` and `round_places` children are
  preserved.
- Lexer/parser/AST: unchanged from the previous `ROUND()` phase. The parser
  already recognizes one-argument, two-argument, and wrong-arity `ROUND()`
  forms.
- Analyzer/runtime: expands only top-level two-argument `ROUND()` evaluation.
  Rounding is evaluated in MyLite code, not by passing a MySQL expression to
  SQLite.
- Catalog/result/storage/VFS: not involved. The feature must not read or mutate
  descriptors, catalog generations, SQLite schema generations, physical tables,
  or the `.mylite` preamble.
- SQLite: no generated SQL, no SQLite extension dependency, and no SQLite fork
  patch.

## Syntax

The admitted syntax is:

```sql
round_scalar:
    ROUND ( round_value )
  | ROUND ( round_value , round_places )
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

round_places:
    decimal_integer_literal
  | + decimal_integer_literal
  | - decimal_integer_literal
  | TRUE
  | FALSE
  | NULL
  | scalar_arithmetic_expression
```

When `round_places >= 0`, `round_value` may use the full current one-argument
`ROUND()` envelope, including direct exact integer literals in the 81
significant-digit literal-projection envelope and limited unsigned-64 bitwise
results.

When `round_places < 0`, `round_value` is narrowed to values that can be
evaluated as signed-64 integers by MyLite: signed-64 decimal integer literals,
booleans, `NULL`, current signed-64 scalar arithmetic, and current bitwise
results no larger than `INT64_MAX`. Direct exact integer literals or bitwise
results outside that signed-64 negative-place subset are rejected with a
deterministic MyLite unsupported diagnostic.

### MyLite Lemon Snippet

No grammar change is required in this phase. The relevant MyLite grammar shape
already exists:

```lemon
expression(A) ::= ROUND(T) LPAREN expression(B) RPAREN(R). {
    A = mylite_sql_parser_make_one_argument_function(
        state, T, MYLITE_SQL_AST_ROUND_FUNCTION, B, R);
}
expression(A) ::= ROUND(T) LPAREN expression(B) COMMA expression(C) RPAREN(R). {
    A = mylite_sql_parser_make_two_argument_function(
        state, T, MYLITE_SQL_AST_ROUND_FUNCTION, B, C, R);
}
```

## Runtime Semantics

Runtime evaluation is MyLite-owned and proportional to AST size.

1. Evaluate `round_value` once through the existing one-argument `ROUND()`
   operand path, preserving staged child warnings.
2. Evaluate `round_places` once through the existing signed-64 scalar
   arithmetic path, preserving staged child warnings.
3. If either evaluated child is `NULL`, return `NULL` after staging warnings
   from both children.
4. If `round_places >= 0`, return the normalized `round_value` text unchanged.
5. If `round_places < 0`, evaluate `round_value` in the signed-64 subset,
   compute half-away-from-zero rounding at `10 ** -round_places`, and return
   canonical signed decimal text.
6. If the rounded signed result would exceed the current signed-64 negative
   places envelope, raise `1690 / 22003`.
7. The statement reports `warning_count == 0` for supported in-range values
   unless an evaluated child already staged warnings.

## Diagnostics

- Wrong arity remains MySQL error `1582` / SQLSTATE `42000`.
- Unsupported operands or broad two-argument forms outside this phase use the
  deterministic MyLite `ROUND()` unsupported diagnostic.
- Signed negative-place result overflow uses MySQL error `1690` / SQLSTATE
  `22003`, message containing `BIGINT value is out of range`.
- Child arithmetic overflow uses the existing scalar arithmetic
  `1690 / 22003` diagnostic.
- Child division or modulo by zero appends warning `1365 / 22012`.
- Allocation failure returns `MYLITE_NOMEM`; public API misuse remains
  unchanged.

Unsupported for this phase:

- table-backed `ROUND(column, places)`;
- decimal/floating rounding, string/hex/bit-literal conversion, casts,
  parameters, user variables, system variables, subqueries, and broader scalar
  expression composition;
- negative-place direct exact integer literals outside the signed-64 subset;
- unsigned-boundary negative-place behavior such as MySQL's mixed exact
  decimal/unsigned handling near `18446744073709551615`;
- nested `ROUND()` inside arithmetic, predicates, DML, defaults, generated
  columns, ordering, grouping, or aggregate arguments; and
- arbitrary SQLite pass-through.

## Tests

Update the existing `ROUND()` MySQL artifact and C runtime test to cover:

- nonnegative places: zero, positive, `TRUE`, `FALSE`;
- `NULL` value and `NULL` places;
- negative places for positive, negative, half, below-half, and very-negative
  signed integer inputs;
- signed result overflow;
- child warning staging from value and places expressions;
- `DO ROUND(value, places)`;
- deterministic rejection for unsupported negative-place unsigned and
  broad-literal forms; and
- file safety and unchanged catalog/schema generations through the existing
  `ROUND()` test harness.

Verification commands:

```sh
packages/libmylite/tests/mysql_baseline_round_function_expectations.sh
cmake --build --preset dev --target mylite_parser_test mylite_runtime_round_function_test
ctest --test-dir build/dev --output-on-failure -R 'libmylite\.(parser|runtime\.round_function|runtime\.scalar_arithmetic_projection|runtime\.scalar_bitwise_projection)'
cmake --workflow --preset check
```

## Compatibility Documentation

Update `COMPATIBILITY.md`, `docs/compatibility/functions-numeric-math.md`,
`docs/compatibility/sql-query-expressions.md`, and
`docs/compatibility/type-system-literals-conversion.md` only for this limited
two-argument integer-domain subset. Do not claim general `ROUND(value, places)`
support or broader decimal/floating expression evaluation.
