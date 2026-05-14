# Baseline EXP, LOG, And POW Functions

## Summary

This phase admits a narrow MyLite-owned mathematical function batch for
no-source, `FROM DUAL`, and `DO` scalar execution:

```sql
SELECT EXP(value), LN(value), LOG(value), LOG(base, value),
       LOG10(value), LOG2(value), POW(value, exponent), POWER(value, exponent)
SELECT ... FROM DUAL
DO ...
```

The supported operand domain matches the current no-source scalar numeric
envelope used by `SQRT()`, `ACOS()`, `ASIN()`, and `ATAN()`:
integer/boolean/`NULL` operands, signed-64 scalar arithmetic, and limited
unsigned-64 bitwise operands. The implementation deliberately does not add a
general approximate-number expression model, table-backed math expressions,
string or decimal coercion, nested approximate arithmetic, predicates, DML
assignments, generated columns, subqueries, parameters, arbitrary SQLite
pass-through, or SQLite fork changes.

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
  `packages/libmylite/tests/mysql_baseline_exp_log_power_functions_expectations.sh`.

The MySQL 8.4 manual documents `EXP(X)`, `LN(X)`, `LOG(X)`,
`LOG(B, X)`, `LOG10(X)`, `LOG2(X)`, `POW(X, Y)`, and `POWER(X, Y)`.
It also documents that all mathematical functions return `NULL` for an error;
runtime probes define this slice's warning/error details.

Observed MySQL 8.4.9 behavior for this phase:

- `EXP(NULL)`, log functions with any `NULL` required operand, and
  `POW()`/`POWER()` with either operand `NULL` return `NULL`.
- `TRUE` and `FALSE` behave as integer `1` and `0`.
- `EXP(0)`, `EXP(1)`, `EXP(-1)`, `EXP(2)`, and `EXP(10)` return MySQL's
  visible double text. `EXP(710)` raises `1690 / 22003`
  `DOUBLE value is out of range`.
- `LN(0)`, `LN(-1)`, `LOG(0)`, `LOG(-1)`, `LOG10(0)`, `LOG10(-1)`,
  `LOG2(0)`, and `LOG2(-1)` return `NULL` and append warning `3020`
  `Invalid argument for logarithm`.
- `LOG(base, value)` returns `NULL` and appends warning `3020` when `base <= 0`,
  `base = 1`, or `value <= 0`.
- `LOG()` and `LOG(1,2,3)` are syntax errors in MySQL 8.4.9. Other wrong
  arities in this batch use native function count error `1582 / 42000`.
- `POW(2,3)`, `POW(2,-3)`, `POW(-2,3)`, `POW(-2,2)`, `POW(-2,-3)`,
  `POWER(3,2)`, and `POW(0,0)` return visible MySQL double/integer text.
- `POW(0,-1)` and `POW(10,309)` raise `1690 / 22003`
  `DOUBLE value is out of range`.
- Existing child arithmetic diagnostics are preserved. For example
  `EXP(5 DIV 0)` returns `NULL` and appends warning `1365 / 22012`,
  while `EXP(5 DIV 0), EXP(710)` records the division warning before the
  overflow error.
- MySQL accepts broader string, decimal, approximate, table-backed, and
  parameterized forms; those are intentionally deferred.

## Ownership Boundaries

- Public API: unchanged. Supported `SELECT` statements return one row through
  existing result conventions; supported `DO` statements return non-row
  results.
- Statement context: preserves existing scalar diagnostics, warning staging,
  affected-row, and row-count behavior.
- Lexer/parser/AST: adds tokens and AST variants for the admitted functions,
  plus native wrong-arity variants where MySQL uses `1582`. Function names
  remain usable as identifiers through the current keyword policy.
- Analyzer/runtime: admits only top-level supported scalar projection and `DO`
  expressions. Values are evaluated and formatted in MyLite runtime code.
- Catalog: not involved. The feature must not read or mutate descriptors,
  descriptor caches, catalog generation, selected schema, or
  `sqlite_schema_generation`.
- Result builder: existing scalar result helpers append one column per selected
  expression and respect aliases.
- Storage/VFS/file format: no storage writes, physical table access, or
  `.mylite` preamble changes.
- SQLite: no generated SQLite SQL, no SQLite function registration, and no
  SQLite fork patch. This is MyLite wrapper/runtime behavior using the host C
  math library over the admitted input domain.

## Syntax

MyLite admits these source forms:

```sql
SELECT math_item[, math_item ...]
SELECT math_item[, math_item ...] FROM DUAL
DO math_scalar[, math_scalar ...]

math_item:
    math_scalar
  | math_scalar AS alias
  | math_scalar alias

math_scalar:
    EXP ( math_value )
  | LN ( math_value )
  | LOG ( math_value )
  | LOG ( math_value , math_value )
  | LOG10 ( math_value )
  | LOG2 ( math_value )
  | POW ( math_value , math_value )
  | POWER ( math_value , math_value )
  | ( math_scalar )

math_value:
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
envelope, preserving sign separately for negative values. Direct integer
literals outside that envelope are rejected with a deterministic unsupported
diagnostic until wider decimal precision exists.

`scalar_arithmetic_expression` is the current no-source/`DUAL` signed-64
arithmetic domain. `scalar_bitwise_expression` is the current top-level
unsigned-64 bitwise domain.

These functions are not admitted as children of arithmetic, comparison,
logical, `IS`, `CASE`, control-flow functions, predicates, table-backed
projection, ordering, grouping, defaults, generated columns, or DML expressions
in this phase.

### MyLite Lemon Snippet

The parser grammar is authored independently and extends MyLite's expression
grammar:

```lemon
expression(A) ::= EXP(T) LPAREN expression(B) RPAREN(R). {
    A = mylite_sql_parser_make_one_argument_function(
        state, T, MYLITE_SQL_AST_EXP_FUNCTION, B, R);
}
expression(A) ::= EXP(T) LPAREN RPAREN(R). {
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_EXP_ARGUMENT_COUNT_ERROR, NULL, R);
}
expression(A) ::= EXP(T) LPAREN expression(B) COMMA function_argument_list(C) RPAREN(R). {
    (void)B;
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_EXP_ARGUMENT_COUNT_ERROR, C, R);
}

expression(A) ::= LN(T) LPAREN expression(B) RPAREN(R). {
    A = mylite_sql_parser_make_one_argument_function(
        state, T, MYLITE_SQL_AST_LN_FUNCTION, B, R);
}
expression(A) ::= LOG(T) LPAREN expression(B) RPAREN(R). {
    A = mylite_sql_parser_make_one_argument_function(
        state, T, MYLITE_SQL_AST_LOG_FUNCTION, B, R);
}
expression(A) ::= LOG(T) LPAREN expression(B) COMMA expression(C) RPAREN(R). {
    A = mylite_sql_parser_make_two_argument_function(
        state, T, MYLITE_SQL_AST_LOG_FUNCTION, B, C, R);
}
expression(A) ::= LOG10(T) LPAREN expression(B) RPAREN(R). {
    A = mylite_sql_parser_make_one_argument_function(
        state, T, MYLITE_SQL_AST_LOG10_FUNCTION, B, R);
}
expression(A) ::= LOG2(T) LPAREN expression(B) RPAREN(R). {
    A = mylite_sql_parser_make_one_argument_function(
        state, T, MYLITE_SQL_AST_LOG2_FUNCTION, B, R);
}

expression(A) ::= POW(T) LPAREN expression(B) COMMA expression(C) RPAREN(R). {
    A = mylite_sql_parser_make_two_argument_function(
        state, T, MYLITE_SQL_AST_POW_FUNCTION, B, C, R);
}
expression(A) ::= POWER(T) LPAREN expression(B) COMMA expression(C) RPAREN(R). {
    A = mylite_sql_parser_make_two_argument_function(
        state, T, MYLITE_SQL_AST_POWER_FUNCTION, B, C, R);
}
```

The final grammar must also include native wrong-arity variants for `LN`,
`LOG10`, `LOG2`, `POW`, and `POWER`, and identifier productions for every new
function token. `LOG()` and `LOG(a,b,c)` intentionally remain syntax errors to
match observed MySQL 8.4.9 behavior.

## Runtime Semantics

Runtime evaluation is MyLite-owned and proportional to AST size.

1. Admit a `SELECT` or `DO` statement only when each selected or evaluated
   expression is an existing scalar expression or a top-level supported
   function from this batch.
2. Preserve existing native-function wrong-arity diagnostics and MySQL-shaped
   syntax errors before generic unsupported diagnostics.
3. Evaluate required operands once, left to right, preserving existing child
   warning staging and overflow errors.
4. `NULL` input returns `NULL`, except that evaluated child warnings are still
   preserved.
5. Direct boolean input maps `TRUE` to `1` and `FALSE` to `0`.
6. Convert admitted non-`NULL` integer-domain values to `double` for function
   evaluation.
7. `EXP(value)` returns `exp(value)` unless the result is non-finite or outside
   MySQL's admitted visible double range, in which case it raises
   `1690 / 22003`.
8. `LN(value)`, one-argument `LOG(value)`, `LOG10(value)`, and `LOG2(value)`
   return the corresponding logarithm for positive values. Nonpositive
   non-`NULL` values return `NULL` and append warning `3020`.
9. Two-argument `LOG(base, value)` returns `log(value) / log(base)` when
   `base > 0`, `base != 1`, and `value > 0`. Invalid non-`NULL` base or value
   returns `NULL` and appends warning `3020`.
10. `POW(base, exponent)` and `POWER(base, exponent)` return `pow(base,
    exponent)`. Non-finite results or errno-domain/range failures that MySQL
    reports as out-of-range raise `1690 / 22003`.
11. Format non-`NULL` results through the existing scalar double text formatter
    used by adjacent math functions, preserving MySQL 8.4.9 visible text for
    the admitted probes.
12. Append staged warnings after scalar select-item or `DO` expression
    evaluation through the existing scalar warning path.

Supported in-range statements report `warning_count == 0` unless evaluated
child expressions or invalid logarithm inputs stage warnings. They do not touch
catalog state, SQLite schema generation, physical tables, or the `.mylite`
preamble.

## Diagnostics

Supported successful statements return through existing result conventions.

Diagnostics for this baseline:

- syntax errors use the existing parser diagnostic surface;
- wrong arity for `EXP`, `LN`, `LOG10`, `LOG2`, `POW`, and `POWER` uses MySQL
  error `1582 / 42000` with the matching native function name;
- `LOG()` and `LOG(1,2,3)` use syntax error `1064 / 42000`;
- bare function names use existing identifier/unknown-column behavior,
  `1054 / 42S22`, where the scalar select path reaches name resolution;
- invalid logarithm inputs append warning `3020 / HY000`;
- out-of-range `EXP()` and `POW()` / `POWER()` results raise
  `1690 / 22003`;
- child signed arithmetic overflow uses existing `1690 / 22003`;
- evaluated child division or modulo by zero appends warning `1365 / 22012`;
- unsupported operands use deterministic MyLite unsupported-feature
  diagnostics naming the admitted function subset;
- allocation failure returns `MYLITE_NOMEM`; and
- public API misuse remains unchanged.

Unsupported for this slice:

- table-backed function calls and any `FROM` source other than `DUAL`;
- nested use inside arithmetic, comparison, logical, `IS`, `CASE`,
  control-flow functions, predicates, `ORDER BY`, `GROUP BY`, aggregate
  arguments, defaults, generated columns, and DML assignments;
- string, decimal, float, hex, bit literal, binary-string, temporal, JSON,
  parameter, user-variable, system-variable, session-function, cast, collation,
  and subquery operands;
- direct exact decimal magnitudes above unsigned 64-bit; and
- arbitrary SQLite pass-through.

## Tests

The MySQL expectation script covers:

- MySQL version guard;
- core `EXP`, `LN`, `LOG`, `LOG10`, `LOG2`, `POW`, and `POWER` values over
  `NULL`, booleans, zero, positive integers, negative integers, and aliases;
- two-argument `LOG(base, value)`;
- arithmetic and bitwise child operands;
- invalid-log warning staging and `SHOW WARNINGS`;
- child division-by-zero warning staging;
- out-of-range `EXP()` and `POW()` errors, including warning-before-error
  ordering;
- `DO` status and diagnostics;
- native wrong-arity diagnostics and `LOG` syntax errors;
- bare-name unknown-column diagnostics; and
- MySQL-accepted but deferred string, decimal, approximate, table-backed, and
  parameter-like forms where useful.

Implementation tests must add:

- parser AST coverage for valid calls, aliases, identifier fallback, and
  wrong-arity or syntax-error forms;
- runtime value, warning, error, `DO`, `DUAL`, alias, mixed select-list,
  independent-handle, file-format, and zero-initialized cleanup coverage;
- compatibility docs updates in `COMPATIBILITY.md`,
  `docs/compatibility/functions-numeric-math.md`,
  `docs/compatibility/operators.md`,
  `docs/compatibility/sql-query-expressions.md`, and literal/conversion docs
  only where the admitted operand surface changes; and
- the standard focused and full workflow verification commands.
