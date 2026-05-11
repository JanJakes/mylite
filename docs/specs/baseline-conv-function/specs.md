# Baseline CONV Function

## Summary

This phase admits a narrow MyLite-owned base-conversion scalar function for
no-source, `FROM DUAL`, and `DO` execution:

```sql
SELECT CONV(base_value, from_base, to_base)[, ...]
SELECT CONV(base_value, from_base, to_base)[, ...] FROM DUAL
DO CONV(base_value, from_base, to_base)[, ...]
```

For the admitted integer-only domain, `CONV()` evaluates `base_value`, formats
that value as decimal integer text, parses that text in `ABS(from_base)`, and
returns uppercase digits in `ABS(to_base)`. A positive `to_base` renders the
64-bit result as unsigned text; a negative `to_base` renders it as signed text.
Any `NULL` argument returns `NULL`. Invalid absolute base values outside
`2..36` return `NULL` without warning.

This is not a general expression, string, or table-column conversion engine.
The phase does not admit table-backed `CONV(column, ...)`, string/decimal/float/
hex/bit coercion, casts, user variables, parameters, subqueries, CTEs,
expression metadata, `CONV()` results nested inside parent expressions, or
arbitrary SQLite pass-through.

## Compatibility Authority

- Official MySQL 8.4 documentation:
  - Mathematical functions:
    <https://dev.mysql.com/doc/refman/8.4/en/mathematical-functions.html>
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
  `packages/libmylite/tests/mysql_baseline_conv_function_expectations.sh`.

The MySQL 8.4 manual documents `CONV(N, from_base, to_base)` as converting `N`
from one base to another with 64-bit precision, returning a string, accepting
bases `2..36`, returning `NULL` for `NULL` arguments, and using the signs of
`from_base` and `to_base` for signed interpretation/rendering. Runtime probes
against MySQL 8.4.9 establish these expectations for this slice:

- `CONV(NULL,10,2)`, `CONV(10,NULL,2)`, and `CONV(10,10,NULL)` return `NULL`;
- `TRUE` and `FALSE` behave as `1` and `0`;
- zero forms return `"0"`;
- output digits are uppercase, for example `CONV(35,10,36)` returns `"Z"`;
- the first argument is parsed as digits in `from_base`, so
  `CONV(1010,2,10)` returns `"10"` and `CONV(36,36,10)` returns `"114"`;
- parsing stops at the first invalid digit after at least one valid digit, so
  `CONV(12,2,10)` returns `"1"` without warning;
- a first invalid digit returns `"0"` and records warning `1292`, for example
  `CONV(2,2,10)` returns `"0"` and warns that the decimal value was truncated;
- positive `to_base` renders unsigned 64-bit text, so `CONV(-1,10,10)`
  returns `"18446744073709551615"`;
- negative `to_base` renders signed text, so `CONV(-1,10,-10)` returns `"-1"`;
- signed and unsigned boundary operands keep 64-bit behavior:
  `CONV(-9223372036854775808,10,10)` returns `"9223372036854775808"`,
  `CONV(-9223372036854775808,10,-10)` returns
  `"-9223372036854775808"`, and
  `CONV(18446744073709551615,10,16)` returns `"FFFFFFFFFFFFFFFF"`;
- invalid absolute base values return `NULL` and do not add warnings;
- evaluated child division by zero returns `NULL` and records warning `1365` /
  SQLSTATE `22012`, `Division by 0`;
- child signed arithmetic overflow raises MySQL error `1690` / SQLSTATE
  `22003`;
- `CONV()`, `CONV(1)`, `CONV(1,10)`, and `CONV(1,10,2,3)` raise MySQL error
  `1582` / SQLSTATE `42000`, with the native function name in the message; and
- MySQL accepts broader forms such as strings, binary strings, hex literals,
  bit literals, decimals, floats, casts, and table-backed columns. Those remain
  deferred by this MyLite baseline.

## Ownership Boundaries

- Public API: unchanged. Successful supported `SELECT` statements return one
  row through existing result conventions; successful supported `DO`
  statements return a non-row result.
- Statement context: preserves existing `SELECT` and `DO` row-count behavior.
  Warnings from evaluated child arithmetic and supported `CONV()` truncation
  are staged until the statement completes, so `@@warning_count` read inside
  the same scalar `SELECT` observes MySQL-compatible pre-statement warning
  state.
- Lexer/parser/AST: adds function-specific `CONV()` AST nodes plus wrong-arity
  AST nodes, following existing native-function parser patterns.
- Analyzer/runtime: admits only top-level supported `CONV()` scalar projection
  expressions and evaluates them in MyLite code. Arguments may use the admitted
  value domains described below.
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
SELECT conv_item[, conv_item ...]
SELECT conv_item[, conv_item ...] FROM DUAL
DO conv_scalar[, conv_scalar ...]

conv_item:
    conv_scalar
  | conv_scalar AS alias
  | conv_scalar alias

conv_scalar:
    CONV ( conv_value , conv_base , conv_base )
  | ( conv_scalar )

conv_value:
    decimal_integer_literal
  | + decimal_integer_literal
  | - decimal_integer_literal
  | TRUE
  | FALSE
  | NULL
  | scalar_arithmetic_expression
  | scalar_bitwise_expression

conv_base:
    decimal_integer_literal
  | + decimal_integer_literal
  | - decimal_integer_literal
  | TRUE
  | FALSE
  | NULL
  | scalar_arithmetic_expression
```

Direct nonnegative `conv_value` literals are admitted through the unsigned
64-bit envelope `0` through `18446744073709551615`. Direct negative
`conv_value` literals are admitted down to `-9223372036854775808`.
Broader direct literal coercions that MySQL accepts remain deferred.

`conv_base` is evaluated in the signed-64 scalar arithmetic domain. `TRUE` and
`FALSE` are `1` and `0`. `NULL` returns a `NULL` `CONV()` result. Absolute base
values `2..36` are valid; values outside that range return `NULL` without
warning.

`scalar_arithmetic_expression` is the current no-source/`DUAL` signed-64
arithmetic domain: decimal integer/boolean/`NULL` values, supported scalar
`IF()`/`IFNULL()`/`COALESCE()`/`NULLIF()`/`ISNULL()` values, parenthesized
admitted arithmetic, unary `+`/`-`, binary `+`, binary `-`, `*`, `%`, infix
`MOD`, `MOD(left, right)`, and infix `DIV`.

`scalar_bitwise_expression` is admitted only for `conv_value`, matching the
current top-level numeric bitwise domain: `~`, `&`, `|`, `^`, `<<`, and `>>`
over admitted signed-64 scalar arithmetic operands, with unsigned 64-bit
results.

`CONV()` is not admitted as a child of arithmetic, comparison, logical, `IS`,
`CASE`, control-flow functions, predicates, table-backed projection, ordering,
grouping, DML expressions, or another `CONV()` in this phase.

### MyLite Lemon Snippet

The parser grammar is authored independently and extends MyLite's expression
grammar:

```lemon
expression(A) ::= CONV(T) LPAREN expression(B) COMMA expression(C) COMMA expression(D) RPAREN(R). {
    A = mylite_sql_parser_make_three_argument_function(
        state, T, MYLITE_SQL_AST_CONV_FUNCTION, B, C, D, R);
}
expression(A) ::= CONV(T) LPAREN RPAREN(R). {
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_CONV_ARGUMENT_COUNT_ERROR, NULL, R);
}
expression(A) ::= CONV(T) LPAREN expression(B) RPAREN(R). {
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_CONV_ARGUMENT_COUNT_ERROR, B, R);
}
expression(A) ::= CONV(T) LPAREN expression(B) COMMA expression(C) RPAREN(R). {
    (void)B;
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_CONV_ARGUMENT_COUNT_ERROR, C, R);
}
expression(A) ::=
    CONV(T) LPAREN expression(B) COMMA expression(C) COMMA expression(D) COMMA function_argument_list(E) RPAREN(R). {
    (void)B;
    (void)C;
    (void)D;
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_CONV_ARGUMENT_COUNT_ERROR, E, R);
}
identifier(A) ::= CONV(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
```

The wrong-arity rule is implemented with care so the exact three-argument form
is parsed as `MYLITE_SQL_AST_CONV_FUNCTION`, while all other argument counts
produce `MYLITE_SQL_AST_CONV_ARGUMENT_COUNT_ERROR`. These snippets describe
MyLite's admitted grammar and are not copied from MySQL grammar text.

## Runtime Semantics

Runtime evaluation is MyLite-owned and proportional to AST size.

1. Admit a `SELECT` or `DO` statement only when each selected or evaluated
   expression is an existing scalar expression or a top-level supported
   `CONV()` expression.
2. Preserve existing native-function wrong-arity diagnostics before generic
   unsupported diagnostics.
3. Evaluate `base_value`, `from_base`, and `to_base` left to right, with
   MySQL-compatible `NULL` short-circuiting: a `NULL` `base_value` returns
   `NULL` without evaluating the base arguments, and a `NULL` `from_base`
   returns `NULL` without evaluating `to_base`.
4. If an evaluated argument is `NULL`, return `NULL` and preserve staged child
   warnings from arguments actually evaluated.
5. If either absolute base is outside `2..36`, return `NULL` and preserve
   staged child warnings without adding a warning for the invalid base.
6. Convert non-`NULL` `base_value` to MyLite's supported decimal integer text:
   direct nonnegative and bitwise values use unsigned decimal text; direct
   negative and signed arithmetic values use signed decimal text.
7. Parse that decimal text in `ABS(from_base)`. A leading `-` is accepted for
   negative values. Parsing stops at the first digit not valid in the input
   base. If no digit is accepted, return numeric zero and stage MySQL warning
   `1292` / SQLSTATE `22007`, `Truncated incorrect DECIMAL value: '<text>'`.
8. When `from_base` is negative, clamp positive parsed magnitudes above
   `9223372036854775807` to `9223372036854775807` for this baseline, matching
   the observed MySQL 8.4.9 behavior for supported integer text inputs.
   Negative parsed magnitudes down to `-9223372036854775808` are preserved.
9. Positive `to_base` formats the 64-bit result as unsigned text in
   `ABS(to_base)`.
10. Negative `to_base` formats the 64-bit result as signed text in
    `ABS(to_base)`, including `-9223372036854775808`.
11. Output digits `10..35` are uppercase `A..Z`; zero returns `"0"`.
12. Append staged warnings after scalar select-item or `DO` expression
    evaluation through the existing scalar warning path.

Supported in-range `CONV()` statements report `warning_count == 0` unless an
evaluated child expression stages an existing division-by-zero warning or an
otherwise supported input conversion has no valid leading digit. They do not
touch catalog state, SQLite schema generation, physical tables, or the
`.mylite` preamble.

## Diagnostics

Supported successful statements return through existing result conventions.

Diagnostics for this baseline:

- syntax errors use the existing parser diagnostic surface;
- wrong arity uses MySQL error `1582` / SQLSTATE `42000` with native function
  name `CONV`;
- unsupported operands use a deterministic MyLite unsupported-feature
  diagnostic describing the admitted `CONV()` subset;
- direct integer operands outside the admitted signed/unsigned literal envelope
  use a deterministic MyLite unsupported-feature diagnostic;
- invalid absolute base values return `NULL` without warning;
- an otherwise supported input conversion with no valid leading digit stages
  MySQL warning `1292` / SQLSTATE `22007`;
- child signed arithmetic overflow uses MySQL error `1690` / SQLSTATE
  `22003`;
- child division by zero stages MySQL warning `1365` / SQLSTATE `22012` and
  returns `NULL`;
- allocation failures use the existing out-of-memory diagnostic;
- public API misuse remains unchanged because no public surface is added; and
- physical SQLite failures are not expected because this feature emits no
  SQLite SQL.

## Unsupported Forms

The following remain unsupported and must be rejected deterministically by
parser syntax errors, native arity errors, or MyLite unsupported-feature
diagnostics depending on where they enter the existing grammar:

- table-backed `CONV(column, ...)` or any `FROM` source other than `DUAL`;
- string, binary string, hex, bit, decimal, float, scientific-notation, date,
  time, JSON, and spatial argument coercions;
- base arguments from bitwise expressions, session variables, system variables,
  parameters, casts, or user variables;
- `CONV()` nested inside arithmetic, bitwise, comparison, logical, `IS`,
  `CASE`, control-flow functions, predicates, ordering, grouping, or DML;
- expression metadata and MySQL character set/collation metadata for the
  returned string;
- subqueries, CTEs, window functions, aggregate arguments, stored functions,
  loadable functions, and arbitrary SQLite pass-through.

## Test Plan

Add `packages/libmylite/tests/mysql_baseline_conv_function_expectations.sh` to
record MySQL 8.4.9 expectations before implementation.

Add fast C coverage under `packages/libmylite/tests/`, preferably a new
`runtime_conv_function` binary:

- parser AST nodes for valid `CONV(expr, expr, expr)` and wrong-arity forms;
- no-source and `FROM DUAL` successful values, aliases, result column labels,
  row count, warning count, and absence of table access;
- `DO CONV(...)` with no result rows, affected rows `0`, and warning behavior;
- `NULL`, booleans, zero, positive integers, negative integers, base parsing,
  uppercase output digits, invalid bases, invalid leading input digit warnings,
  signed output, unsigned output, and signed/unsigned 64-bit boundaries;
- supported arithmetic and bitwise `base_value` children, supported arithmetic
  base children, division-by-zero warning staging, and arithmetic overflow
  diagnostics;
- unsupported strings, decimals, floats, hex, bit literals, table-backed
  expressions, `CONV()` nested in parent expressions, base bitwise expressions,
  parameters, functions outside the admitted scalar domain, subqueries, CTEs,
  and wrong arity;
- file preamble safety, catalog/schema-generation immutability, independent
  file-backed handles, and zero-initialized cleanup paths;
- focused parser/runtime CTest entries plus adjacent scalar-function and
  scalar-expression tests; and
- `cmake --workflow --preset check`.

## Compatibility Documentation

Update `COMPATIBILITY.md`,
`docs/compatibility/functions-numeric-math.md`,
`docs/compatibility/sql-query-expressions.md`,
`docs/compatibility/sql-stored-programs.md`, and any operator/literal detail
docs touched by the admitted scalar surface. Use partial/limited wording for
the exact no-source/`DUAL`/`DO` integer-domain `CONV()` subset. Do not imply
support for table-backed conversion, string/decimal/float/hex/bit conversion,
general expression support, arbitrary function nesting, full metadata,
collations, or SQLite pass-through.
