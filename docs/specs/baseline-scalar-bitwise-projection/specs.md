# Baseline Scalar Bitwise Projection

## Summary

This phase admits a narrow MyLite-owned numeric bitwise expression surface for
no-source, `FROM DUAL`, and `DO` scalar expression execution:

```sql
SELECT bitwise_scalar[, bitwise_scalar ...]
SELECT ALL bitwise_scalar[, bitwise_scalar ...]
SELECT bitwise_scalar[, bitwise_scalar ...] FROM DUAL
SELECT ALL bitwise_scalar[, bitwise_scalar ...] FROM DUAL
DO bitwise_scalar[, bitwise_scalar ...]
```

The admitted new operators are unary `~` and binary `&`, `|`, `^`, `<<`, and
`>>`. Operands use the current signed-64 scalar arithmetic domain: decimal
integer/boolean/`NULL` values, supported scalar
`IF()`/`IFNULL()`/`COALESCE()`/`NULLIF()`/`ISNULL()` values, parenthesized
admitted arithmetic, unary `+`/`-`, binary `+`, binary `-`, `*`, `%`, infix
`MOD`, `MOD(left, right)`, and infix `DIV`.

This is not a general expression engine. It does not admit table-backed
expression projection, bitwise expressions inside arithmetic/comparison/logical
parents, `/`, string/decimal/float/hex/bit operands, binary-string bit
operations, system/session values as bitwise operands, parameters, user
variables, subqueries, CTEs, expression assignments, predicates, DML
assignments, defaults, arbitrary SQLite pass-through, or expression metadata.

## Compatibility Authority

- Official MySQL 8.4 documentation:
  - Bit functions and operators:
    <https://dev.mysql.com/doc/refman/8.4/en/bit-functions.html>
  - Operator precedence:
    <https://dev.mysql.com/doc/refman/8.4/en/operator-precedence.html>
  - `SELECT` statement and `DUAL`:
    <https://dev.mysql.com/doc/refman/8.4/en/select.html>
  - `DO` statement:
    <https://dev.mysql.com/doc/refman/8.4/en/do.html>
- Observed MySQL 8.4.9 runtime behavior, captured by
  `packages/libmylite/tests/mysql_baseline_scalar_bitwise_projection_expectations.sh`.

The MySQL 8.4 manual documents numeric bit operations as unsigned 64-bit
results when arguments are not binary strings. It also documents bitwise
operator precedence from high to low as unary `~`, bitwise `^`, multiplicative
arithmetic, additive arithmetic, shifts, `&`, then `|`, before comparison and
logical operators.

Runtime probes against MySQL 8.4.9 establish these expectations for this
slice:

- numeric `&`, `|`, `^`, `~`, `<<`, and `>>` produce unsigned 64-bit decimal
  text;
- signed operands are converted into the numeric bit-operation domain by their
  unsigned 64-bit representation;
- `TRUE` and `FALSE` behave as `1` and `0`;
- a binary bitwise operation with a `NULL` left operand returns `NULL` without
  evaluating the right operand;
- a binary bitwise operation with a non-`NULL` left operand and a `NULL` right
  operand returns `NULL` after evaluating both sides;
- unary `~NULL` returns `NULL`;
- shift counts greater than or equal to 64, including negative signed values
  after unsigned conversion, return `0` without warning;
- bitwise operations themselves do not emit warnings for the supported
  in-range numeric forms;
- evaluated child `DIV` or modulo by zero still stages warning 1365;
- child signed arithmetic overflow raises MySQL error 1690 / SQLSTATE `22003`;
- MySQL accepts broader forms such as string, decimal, hex, bit, and
  table-backed operands, bitwise expressions nested inside arithmetic,
  comparisons over bitwise expressions, and binary-string bit operations. Those
  remain deferred by this MyLite slice.

## Ownership Boundaries

- Public API: unchanged. Successful supported `SELECT` statements return one
  row through the existing text-result conventions; successful `DO` returns an
  empty non-row result.
- Statement context: `SELECT` preserves existing row-returning behavior,
  including following `ROW_COUNT() == -1`; `DO` preserves existing affected-row
  state `0`. Staged division-by-zero warnings from evaluated child arithmetic
  are appended through the existing scalar expression warning path.
- Lexer/parser/AST: the lexer already recognizes the symbol tokens. The parser
  maps them to MyLite AST unary/binary operators and records MySQL-compatible
  precedence independently in `mylite_parse.y`.
- Analyzer/runtime: scalar projection admission accepts only top-level bitwise
  expression trees whose non-bitwise leaves are existing signed-64 scalar
  arithmetic expressions. Runtime evaluation is MyLite-owned and iterative,
  producing unsigned 64-bit text for non-`NULL` results.
- Catalog: not involved. The feature must not read or mutate schema/table
  descriptors, descriptor versions, descriptor caches, catalog generation, or
  `sqlite_schema_generation`.
- Result builder: appends one column per selected expression through existing
  scalar result helpers. Explicit aliases continue to define result labels.
- Storage/VFS/file format: no storage writes, no physical table access, and no
  `.mylite` preamble changes.
- SQLite: no generated SQLite SQL and no SQLite fork patch. This is MyLite
  wrapper/runtime behavior using public MyLite layers only.

## Syntax

MyLite admits these source forms:

```sql
SELECT bitwise_item[, bitwise_item ...]
SELECT ALL bitwise_item[, bitwise_item ...]
SELECT bitwise_item[, bitwise_item ...] FROM DUAL
SELECT ALL bitwise_item[, bitwise_item ...] FROM DUAL
DO bitwise_scalar[, bitwise_scalar ...]

bitwise_item:
    bitwise_scalar
  | bitwise_scalar AS alias
  | bitwise_scalar alias

bitwise_scalar:
    ~ bitwise_scalar
  | bitwise_scalar ^ bitwise_scalar
  | bitwise_scalar << bitwise_scalar
  | bitwise_scalar >> bitwise_scalar
  | bitwise_scalar & bitwise_scalar
  | bitwise_scalar | bitwise_scalar
  | ( bitwise_scalar )
  | scalar_arithmetic_expression
```

`scalar_arithmetic_expression` is the current admitted signed-64 arithmetic
domain:

- decimal integer literals with optional unary `+` or `-`, where source values
  fit the signed-64 operand envelope;
- `TRUE`, `FALSE`, and `NULL`;
- supported scalar `IF()`/`IFNULL()`/`COALESCE()`/`NULLIF()`/`ISNULL()` values;
- unary `+` and unary `-`;
- binary `+`, binary `-`, `*`, `%`, infix `MOD`, `MOD(left, right)`, and infix
  `DIV`; and
- parenthesized admitted arithmetic expressions.

The top-level admitted expression must be a bitwise operator or parenthesized
bitwise operator. Arithmetic with a bitwise child, such as `~1 + 2` or
`1 + (2 & 3)`, is deferred because it requires MySQL's broader unsigned exact
numeric expression typing.

### MyLite Lemon Snippet

The parser grammar is authored independently and extends MyLite's expression
grammar:

```lemon
%left OR.
%left XOR.
%left AND.
%right NOT.
%left EQUAL NULL_SAFE_EQUAL NOT_EQUAL LESS LESS_EQUAL GREATER GREATER_EQUAL IS.
%left BITWISE_OR.
%left BITWISE_AND.
%left LEFT_SHIFT RIGHT_SHIFT.
%left PLUS MINUS.
%left STAR SLASH DIV PERCENT MOD.
%left BITWISE_XOR.
%right UPLUS UMINUS BITWISE_NOT.

expression(A) ::= BITWISE_NOT(T) expression(B). [BITWISE_NOT] {
    A = mylite_sql_parser_make_unary_expression(
        state, T, MYLITE_SQL_AST_OPERATOR_BITWISE_NOT, B);
}
expression(A) ::= expression(B) BITWISE_XOR(T) expression(C). {
    A = mylite_sql_parser_make_binary_expression(
        state, B, T, MYLITE_SQL_AST_OPERATOR_BITWISE_XOR, C);
}
expression(A) ::= expression(B) LEFT_SHIFT(T) expression(C). {
    A = mylite_sql_parser_make_binary_expression(
        state, B, T, MYLITE_SQL_AST_OPERATOR_LEFT_SHIFT, C);
}
expression(A) ::= expression(B) RIGHT_SHIFT(T) expression(C). {
    A = mylite_sql_parser_make_binary_expression(
        state, B, T, MYLITE_SQL_AST_OPERATOR_RIGHT_SHIFT, C);
}
expression(A) ::= expression(B) BITWISE_AND(T) expression(C). {
    A = mylite_sql_parser_make_binary_expression(
        state, B, T, MYLITE_SQL_AST_OPERATOR_BITWISE_AND, C);
}
expression(A) ::= expression(B) BITWISE_OR(T) expression(C). {
    A = mylite_sql_parser_make_binary_expression(
        state, B, T, MYLITE_SQL_AST_OPERATOR_BITWISE_OR, C);
}
```

These snippets describe MyLite's admitted grammar and are not copied from
MySQL grammar text.

## Runtime Semantics

Evaluation uses a MyLite-owned unsigned 64-bit bitwise evaluator:

1. Admit the statement only when every relevant `SELECT` item or `DO`
   expression is an existing scalar expression or a top-level bitwise scalar.
2. Preserve existing native-function wrong-arity diagnostics before generic
   unsupported diagnostics.
3. Evaluate bitwise expressions left to right using an explicit stack.
4. Evaluate non-bitwise leaves with the current signed-64 scalar arithmetic
   evaluator, preserving its `NULL`, warning, and overflow behavior.
5. Convert non-`NULL` signed arithmetic leaf values to `uint64_t` before
   applying bitwise operators.
6. Unary `~` returns `NULL` for a `NULL` child, otherwise the unsigned 64-bit
   bit inversion.
7. For binary operators, evaluate the left operand first. If it is `NULL`,
   return `NULL` with the left operand's staged warnings and do not evaluate
   the right operand.
8. If the left operand is non-`NULL`, evaluate the right operand. If the right
   operand is `NULL`, return `NULL` with staged warnings from both evaluated
   operands.
9. For `<<` and `>>`, if the unsigned shift count is greater than or equal to
   64, return `0`; otherwise shift the unsigned 64-bit left value.
10. For `&`, `|`, and `^`, apply the corresponding unsigned 64-bit operation.
11. Format non-`NULL` results as canonical unsigned decimal text.
12. Append staged division-by-zero warnings after scalar select-item or `DO`
    expression evaluation using the existing scalar warning path.

Successful supported bitwise statements report `warning_count == 0` unless an
evaluated child arithmetic expression stages an existing division-by-zero
warning. They do not touch catalog state, SQLite schema generation, physical
tables, or the `.mylite` preamble.

## Diagnostics

Supported successful statements return through existing result conventions.

Diagnostics for this baseline:

- syntax errors use the existing parse diagnostic surface;
- unsupported expression forms use deterministic MyLite-specific unsupported
  diagnostics that describe the admitted scalar bitwise subset;
- unsupported bitwise operands that are rejected before arithmetic evaluation
  use the shared scalar projection unsupported diagnostic; unsupported admitted
  child arithmetic still uses the existing signed-64 arithmetic diagnostics;
- child signed arithmetic overflow uses MySQL error 1690 / SQLSTATE `22003`;
- evaluated child division or modulo by zero appends warning 1365 / SQLSTATE
  `22012`;
- allocation failure returns `MYLITE_NOMEM`; and
- public API misuse remains unchanged.

Unsupported for this slice:

- table-backed bitwise projection, predicates, `ORDER BY`, `GROUP BY`,
  `HAVING`, DML assignments, defaults, generated columns, and check
  expressions;
- bitwise expressions nested inside arithmetic, comparison, logical, `IS`, or
  `CASE` parents;
- string, decimal, float, hex, bit, temporal, JSON, parameter, user-variable,
  system-variable, and session-function operands;
- binary-string bit operations and binary string result typing;
- `/`, casts, collations, functions such as `BIT_COUNT()`, and general numeric
  conversion; and
- arbitrary SQLite pass-through.

## Tests

The test suite should cover:

- parser token mapping, AST operator kinds, precedence, associativity, and the
  distinction between keyword logical `XOR` and symbolic bitwise `^`;
- successful no-source and `FROM DUAL` projection for `&`, `|`, `^`, `~`, `<<`,
  and `>>`;
- successful `DO` evaluation with bitwise expressions and no result rows;
- unsigned 64-bit result formatting for `~0`, negative operands, signed
  boundary operands, and shifts into bit 63;
- `NULL` propagation and left-`NULL` short-circuiting, including skipped child
  division-by-zero warnings;
- child arithmetic warning and overflow behavior;
- explicit aliases and result labels;
- file safety: no catalog generation, SQLite schema generation, physical row,
  or `.mylite` preamble mutations;
- independent handles;
- deterministic rejection for table-backed expressions, arithmetic-over-bitwise
  expressions, comparisons over bitwise expressions, string/decimal/float/hex/
  bit literals, parameters, user variables, system variables, session
  functions, subqueries, CTEs, and `/`; and
- the existing lexer, parser, scalar projection, `DO`, arithmetic, modulo,
  `DIV`, comparison, logical, `CASE`, runtime, storage, and catalog tests still
  pass.

## Compatibility Updates

Update `COMPATIBILITY.md`, `docs/compatibility/operators.md`, and
`docs/compatibility/sql-query-expressions.md` only for this limited no-source /
`DUAL` / `DO` bitwise scalar expression subset. Do not mark full bitwise
operator support, binary-string bit operations, table-backed expression
evaluation, expression metadata, or general unsigned numeric expression typing
as supported.
