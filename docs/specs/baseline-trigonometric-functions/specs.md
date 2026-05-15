# Baseline Trigonometric Functions

## Summary

This phase completes the current scalar trigonometric math batch by adding
limited `SIN()`, `COS()`, `TAN()`, and `COT()` support on top of the existing
no-source, `FROM DUAL`, and `DO` scalar expression path:

```sql
SELECT SIN(value), COS(value), TAN(value), COT(value)
SELECT SIN(value), COS(value), TAN(value), COT(value) FROM DUAL
DO SIN(value), COS(value), TAN(value), COT(value)
```

The slice deliberately follows the existing `ACOS()` / `ASIN()` / `ATAN()`
architecture. It is MyLite-owned runtime evaluation over the current admitted
integer/boolean/`NULL`, signed-64 scalar arithmetic, and unsigned-64 bitwise
operand domain. It does not add table-backed trigonometric expressions,
approximate literal operands, string conversion, expression metadata,
predicates, DML assignment support, SQLite function registration, or SQLite
fork changes.

## Compatibility Authority

- MyLite architecture and engineering standards:
  - `README.md`
  - `docs/architecture/engineering-standards.md`
- Existing scalar function specs:
  - `docs/specs/baseline-acos-asin-functions/specs.md`
  - `docs/specs/baseline-atan-functions/specs.md`
  - `docs/specs/baseline-degrees-radians-functions/specs.md`
  - `docs/specs/baseline-sqrt-function/specs.md`
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
  `packages/libmylite/tests/mysql_baseline_trigonometric_functions_expectations.sh`.

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior,
public SQLite APIs, and existing MyLite source code. It does not copy MySQL,
MariaDB, Percona, SQLite implementation internals, or other restrictively
licensed implementation sources.

## MySQL 8.4.9 Runtime Observations

Runtime probes against MySQL 8.4.9 establish these expectations for the
supported slice:

- `SIN(NULL)`, `COS(NULL)`, `TAN(NULL)`, and `COT(NULL)` return `NULL`;
- `TRUE` and `FALSE` behave as integer `1` and `0`;
- `+0`, `-0`, and `0` behave as zero for `SIN()`, `COS()`, and `TAN()`;
- `COT(0)`, `COT(FALSE)`, `COT(+0)`, `COT(-0)`, and other zero-valued
  arguments fail with `1690 / 22003`, `DOUBLE value is out of range`;
- `SIN()`, `COS()`, `TAN()`, and `COT()` each require exactly one argument and
  wrong arity fails with `1582 / 42000`;
- bare `SIN`, `COS`, `TAN`, and `COT` in a select list are identifier lookups
  and fail with `1054 / 42S22` when no such column is visible;
- direct signed and unsigned 64-bit boundary integer values are accepted and
  evaluated as approximate inputs;
- bitwise child operands use the existing unsigned 64-bit numeric bitwise
  result. `1 << 64` evaluates to zero; that is accepted by `SIN()`, `COS()`,
  and `TAN()` but fails for `COT()` because the effective input is zero;
- evaluated child division by zero returns `NULL` and records warning `1365` /
  SQLSTATE `22012`, `Division by 0`;
- child signed arithmetic overflow raises MySQL error `1690` / SQLSTATE
  `22003`;
- when an earlier selected or `DO` expression stages a division-by-zero warning
  and a later expression raises overflow or `COT(0)`, MySQL preserves both
  diagnostics for the statement;
- MySQL accepts broader forms such as string, binary-string, hex, bit, decimal,
  float, wider-decimal, system-variable, prepared-parameter, and table-backed
  column operands. Those remain deferred by this MyLite baseline.

## Ownership Boundaries

- Public API: unchanged. Successful supported `SELECT` statements return one
  row through existing result conventions; successful supported `DO`
  statements return a non-row result.
- Statement context: owns diagnostics reset, warning count, affected rows,
  `ROW_COUNT()`, and cleanup. Warnings from evaluated child arithmetic are
  staged and appended through the existing scalar warning path.
- Lexer/parser/AST: admits `SIN`, `COS`, `TAN`, and `COT` as one-argument
  function tokens, preserves wrong-arity AST nodes for native-function
  diagnostics, and keeps the names usable as unquoted identifiers where
  MyLite's keyword policy permits identifiers.
- Analyzer/runtime: admits only top-level supported scalar projection and `DO`
  expressions. It evaluates the admitted operand once and formats the result in
  MyLite runtime code.
- Catalog: not involved. The feature must not read or mutate descriptors,
  descriptor caches, catalog generation, selected schema, or
  `sqlite_schema_generation`.
- Result builder: existing scalar result helpers append one column per selected
  expression. Explicit aliases continue to define result labels.
- Storage/VFS/file format: no storage writes, physical table access, or
  `.mylite` preamble changes.
- SQLite: no generated SQLite SQL, no SQLite function registration, and no
  SQLite fork patch. This is MyLite wrapper/runtime behavior. The
  implementation may use the host C math library for `sin()`, `cos()`,
  `tan()`, and a checked `1 / tan()` over admitted inputs.

## Syntax

MyLite admits these source forms:

```sql
SELECT trig_item[, trig_item ...]
SELECT trig_item[, trig_item ...] FROM DUAL
DO trig_scalar[, trig_scalar ...]

trig_item:
    trig_scalar
  | trig_scalar AS alias
  | trig_scalar alias

trig_scalar:
    SIN ( trig_value )
  | COS ( trig_value )
  | TAN ( trig_value )
  | COT ( trig_value )
  | ( trig_scalar )

trig_value:
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
envelope. Values outside that envelope are rejected with deterministic
unsupported diagnostics until wider decimal precision exists.

`scalar_arithmetic_expression` and `scalar_bitwise_expression` are the existing
no-source/`DUAL` scalar domains used by `ACOS()` / `ASIN()` / `ATAN()`:
decimal integer/boolean/`NULL` values, current scalar control-flow values,
parenthesized admitted arithmetic, unary `+`/`-`, binary `+`, binary `-`, `*`,
`%`, infix `MOD`, `MOD(left, right)`, infix `DIV`, and numeric bitwise
operators `~`, `&`, `|`, `^`, `<<`, and `>>`.

`SIN()`, `COS()`, `TAN()`, and `COT()` are not admitted as children of
arithmetic, comparison, logical, `IS`, `CASE`, control-flow functions,
predicates, table-backed projection, ordering, grouping, or DML expressions in
this phase.

### MyLite Lemon Snippet

```lemon
expression(A) ::= SIN(T) LPAREN expression(B) RPAREN(R). {
    A = mylite_sql_parser_make_one_argument_function(
        state, T, MYLITE_SQL_AST_SIN_FUNCTION, B, R);
}
expression(A) ::= SIN(T) LPAREN RPAREN(R). {
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_SIN_ARGUMENT_COUNT_ERROR, NULL, R);
}
expression(A) ::= SIN(T) LPAREN expression(B) COMMA function_argument_list(C) RPAREN(R). {
    (void)B;
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_SIN_ARGUMENT_COUNT_ERROR, C, R);
}
identifier(A) ::= SIN(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
```

`COS`, `TAN`, and `COT` use the same one-argument and wrong-arity shape with
their own AST node kinds and native-function diagnostic names.

## Runtime Semantics

Runtime evaluation is MyLite-owned and proportional to AST size.

1. Admit a `SELECT` or `DO` statement only when each selected or evaluated
   expression is an existing scalar expression or a top-level supported
   one-argument trig expression.
2. Preserve existing native-function wrong-arity diagnostics before generic
   unsupported diagnostics.
3. Evaluate the supported function argument once.
4. Direct `NULL` input returns `NULL`.
5. Direct boolean input maps `TRUE` to `1` and `FALSE` to `0`.
6. Direct decimal integer literals are parsed in the unsigned 64-bit magnitude
   envelope with the sign preserved separately.
7. Nonliteral arithmetic arguments are evaluated with the existing signed-64
   scalar arithmetic evaluator, preserving `NULL`, division-by-zero warnings,
   and overflow behavior.
8. Bitwise arguments are evaluated with the existing scalar bitwise evaluator,
   yielding unsigned 64-bit magnitudes.
9. `SIN()`, `COS()`, and `TAN()` pass non-`NULL` admitted inputs to the host C
   math library and format the resulting double with the existing MyLite
   scalar-double formatter.
10. `COT()` first checks for an effective numeric zero. Zero fails with
    `1690 / 22003`, `DOUBLE value is out of range in 'cot(0)'` for the
    admitted direct-zero shape. Nonzero inputs compute `1.0 / tan(input)` and
    use the same formatter.
11. Successful supported functions add no warnings beyond warnings staged by
    evaluated child expressions. Error statements preserve earlier staged
    child warnings as MySQL does.
12. Explicit aliases determine result column labels; otherwise existing parser
    expression text labeling is used.

## Diagnostics

- Wrong argument count: `1582 / 42000`, native function name `SIN`, `COS`,
  `TAN`, or `COT`.
- Bare function name in select list: existing identifier-resolution path,
  usually `1054 / 42S22`.
- Unsupported operand forms: deterministic MyLite unsupported diagnostic,
  before runtime storage or SQLite interaction.
- Unsupported placement, including table-backed projection and nested
  expression use: deterministic MyLite unsupported diagnostic.
- Child signed arithmetic overflow: existing `1690 / 22003` arithmetic error.
- Child division by zero: existing warning `1365 / 22012` when the statement
  otherwise succeeds, or preserved before a later error.
- `COT()` zero input: `1690 / 22003`, double out-of-range error.
- Allocation failure: existing `MYLITE_NOMEM` / public error conventions.
- Public API misuse: unchanged.

## Tests

Add focused parser and runtime C tests plus the MySQL expectation script.
Coverage must include:

- parser acceptance for mixed-case function names, whitespace before `(`,
  aliases, `FROM DUAL`, `DO`, parenthesized forms, wrong arity, and bare
  identifier behavior;
- successful `SIN()`, `COS()`, `TAN()`, and nonzero `COT()` over `NULL`,
  booleans, signed zero forms, small integers, signed 64-bit boundaries,
  unsigned 64-bit boundaries, scalar arithmetic children, and bitwise children;
- `COT(0)` and zero-valued children with MySQL-compatible diagnostics;
- division-by-zero warning staging, child overflow, warning-before-error
  ordering, warning count, error count, affected rows, and `ROW_COUNT()`;
- `SELECT ... FROM DUAL` and `DO` result conventions;
- explicit alias column names;
- unsupported table-backed, nested, predicate, parameter, string, binary,
  hex, bit, decimal, float, system-variable, and wider-decimal operands;
- no catalog-generation, SQLite-schema-generation, selected-schema, storage,
  or `.mylite` preamble mutation.

## Compatibility Notes

This feature marks `SIN()`, `COS()`, `TAN()`, and `COT()` as limited support in
`COMPATIBILITY.md` and `docs/compatibility/functions-numeric-math.md`. The
status must say that support is confined to the current no-source/`DUAL`/`DO`
integer-domain scalar path and does not claim general expression support.
