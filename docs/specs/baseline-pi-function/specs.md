# Baseline PI Function

## Summary

This phase admitted a narrow MyLite-owned `PI()` scalar function for no-source,
`FROM DUAL`, and `DO` execution:

```sql
SELECT PI()[, ...]
SELECT PI()[, ...] FROM DUAL
DO PI()[, ...]
```

The supported result is the default visible MySQL 8.4.9 scalar value
`3.141593`. MySQL uses a double-precision value internally for `PI()`, but this
baseline deliberately does not introduce a general approximate numeric
expression engine. This scalar phase admitted `PI()` only as a top-level scalar
select item or `DO` expression, with optional parenthesization and aliases where
the scalar select path already supported them. The later
`baseline-row-numeric-extra-functions` slice adds limited single-table
row-scalar projection and `CONCAT()` nesting for `PI()`.

This phase did not admit arithmetic/comparison/logical/control-flow expression
generalization, predicates, DML assignments, expression metadata beyond current
public result names and text values, subqueries, CTEs, parameters, user
variables, arbitrary SQLite pass-through, or SQLite fork changes.

## Compatibility Authority

- Official MySQL 8.4 documentation:
  - Mathematical functions:
    <https://dev.mysql.com/doc/refman/8.4/en/mathematical-functions.html>
  - Built-in function reference:
    <https://dev.mysql.com/doc/refman/8.4/en/built-in-function-reference.html>
  - `SELECT` statement and `DUAL`:
    <https://dev.mysql.com/doc/refman/8.4/en/select.html>
  - `DO` statement:
    <https://dev.mysql.com/doc/refman/8.4/en/do.html>
- Observed MySQL 8.4.9 runtime behavior, captured by
  `packages/libmylite/tests/mysql_baseline_pi_function_expectations.sh`.

The MySQL 8.4 manual documents `PI()` as returning pi and notes that the value
is double precision internally while the default displayed value is
`3.141593`. Runtime probes against MySQL 8.4.9 establish these expectations for
this slice:

- `SELECT PI()` returns one row containing text `3.141593`;
- function name casing is not significant, so `PI()`, `pi()`, and `Pi()` return
  the same value;
- whitespace before the argument list is accepted, for example `PI ()`;
- parenthesized `PI()` is accepted and labels as `(PI())`;
- `SELECT PI() FROM DUAL` returns one row containing `3.141593`;
- `DO PI(), pi()` succeeds, returns no rows, sets affected rows to `0`, and
  produces no warnings;
- successful `PI()` evaluation produces no warnings;
- `PI(1)`, `PI(NULL)`, and `PI(1,2)` raise MySQL error `1582` / SQLSTATE
  `42000`, reporting an incorrect parameter count for native function `PI`;
- bare `PI` in a select list is an identifier lookup and raises MySQL error
  `1054` / SQLSTATE `42S22` when no such column is visible;
- MySQL accepts broader forms such as approximate arithmetic
  `PI()+0.000000000000000000`. Those remain deferred by this MyLite baseline;
  table-backed row projection is covered by
  `baseline-row-numeric-extra-functions`.

## Ownership Boundaries

- Public API: unchanged. Successful supported scalar `SELECT` statements return
  one row through existing `mylite_result` conventions; successful supported
  `DO` statements return a non-row result.
- Statement context: owns statement-boundary diagnostics, row-count behavior,
  warning counts, and result finalization. `PI()` itself does not add warnings.
- Lexer/parser/AST: adds function-specific `PI()` and `PI` wrong-arity AST
  nodes, following existing native-function parser patterns.
- Analyzer/runtime: admits only top-level supported `PI()` scalar projection
  and `DO` expressions in this phase. It evaluates the function as a MyLite-owned
  constant text value in the scalar runtime.
- Catalog: not involved. The feature must not read or mutate descriptors,
  descriptor caches, catalog generation, selected schema, or
  `sqlite_schema_generation`.
- Result builder: existing scalar result helpers append one column per selected
  expression. Explicit aliases continue to define result labels.
- Storage/VFS/file format: no storage writes, physical table access, or
  `.mylite` preamble changes.
- SQLite: no generated SQLite SQL, no SQLite function registration, and no
  SQLite fork patch. This is MyLite wrapper/runtime behavior.

## Syntax

MyLite admits these source forms:

```sql
SELECT pi_item[, pi_item ...]
SELECT pi_item[, pi_item ...] FROM DUAL
DO pi_scalar[, pi_scalar ...]

pi_item:
    pi_scalar
  | pi_scalar AS alias
  | pi_scalar alias

pi_scalar:
    PI ( )
  | ( pi_scalar )
```

`PI` remains usable as an ordinary identifier in identifier positions. Bare
`PI` is not an admitted `PI()` function call.

### MyLite Lemon Snippet

The parser grammar is authored independently and extends MyLite's expression
grammar:

```lemon
expression(A) ::= PI(T) LPAREN RPAREN(R). {
    A = mylite_sql_parser_make_zero_argument_function(
        state, T, MYLITE_SQL_AST_PI_FUNCTION, R);
}
expression(A) ::= PI(T) LPAREN function_argument_list(B) RPAREN(R). {
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_PI_ARGUMENT_COUNT_ERROR, B, R);
}
identifier(A) ::= PI(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
```

These snippets describe MyLite's admitted grammar and are not copied from
MySQL grammar text. The wrong-arity branch exists only to preserve MySQL's
native-function parameter-count diagnostic; it must not evaluate arguments or
make general function-call syntax successful.

## Runtime Semantics

Runtime evaluation is MyLite-owned and proportional to AST size.

1. Admit a `SELECT` or `DO` statement only when each selected or evaluated
   expression is an existing supported scalar expression or a top-level
   supported `PI()` expression.
2. Preserve existing native-function wrong-arity diagnostics before generic
   unsupported diagnostics.
3. Evaluate supported `PI()` as non-`NULL` text `3.141593`.
4. Preserve existing source-span column labels and explicit alias behavior.
5. Preserve existing scalar `SELECT` and `DO` row-count conventions:
   successful `SELECT PI()` returns a row-result with affected rows `0`;
   successful `DO PI()` returns no rows with affected rows `0`.
6. Do not stage warnings for supported `PI()` evaluation.
7. Reject unsupported nested scalar-expression forms deterministically before
   any SQLite SQL is generated.

`PI()` is not folded into integer scalar arithmetic, bitwise, comparison,
logical, `IS`, `CASE`, control-flow, aggregate, predicate, ordering, grouping,
or DML assignment expression evaluators in this phase. Single-table row-scalar
projection is added by `baseline-row-numeric-extra-functions`.

## Diagnostics

Supported `PI()` evaluation does not produce warnings.

Diagnostics for this phase:

- wrong argument count: MySQL error `1582`, SQLSTATE `42000`, message naming
  native function `PI`;
- bare `PI`: existing identifier/unknown-column behavior, MySQL error `1054`,
  SQLSTATE `42S22`, where the current scalar select path reaches name
  resolution;
- nested `PI()` inside arithmetic, comparison, logical, control-flow, `CASE`,
  predicates, DML assignments, grouping, ordering, or another function:
  deterministic unsupported scalar-expression diagnostic;
- allocation failure: existing `MYLITE_NOMEM` / SQLSTATE `HY001` path;
- public API misuse: unchanged existing `mylite_execute()` misuse behavior.

## Result And Metadata

Successful supported `SELECT PI()`:

- returns one result row;
- returns one text value, `3.141593`;
- uses source expression text as the default column name, preserving case,
  spacing, and parenthesization, for example `PI()`, `pi()`, `PI ()`, and
  `(PI())`;
- honors explicit aliases through the existing scalar select item alias path;
- reports `warning_count == 0`;
- returns no additional protocol-grade numeric type metadata beyond the
  existing public result object surface.

The absence of approximate numeric metadata is intentional for this baseline.
Future approximate-number phases must decide how MyLite represents DOUBLE
values internally, how it formats them at the public API and wire-protocol
boundaries, and how nested approximate expressions interact with existing
integer-domain scalar evaluators.

## SQLite And Performance

This feature does not call SQLite for evaluation. It is a constant-time MyLite
scalar runtime branch that writes a borrowed constant string into the existing
scalar cell. No SQLite SQL is generated, no table scan occurs for admitted
forms, and no `.mylite` file bytes change.

No SQLite fork hook is needed. Per the fork policy, public SQLite APIs and
MyLite wrapper code are sufficient; approximate numeric storage/execution
extension points remain future work.

## Tests

Add MySQL-runtime expectation coverage for:

- MySQL version guard;
- `SELECT PI()`, mixed casing, whitespace, parenthesization,
  `@@warning_count`, and `ROW_COUNT()`;
- `SELECT PI() FROM DUAL`;
- `DO PI(), pi()` status and warnings;
- `SHOW WARNINGS` after successful `PI()`;
- wrong arity diagnostics for one, `NULL`, and two arguments;
- bare `PI` unknown-column diagnostics;
- MySQL-accepted but deferred approximate arithmetic forms.

Add plain C tests for:

- successful no-source and `FROM DUAL` projection;
- default column labels and explicit aliases;
- mixed `PI()` with existing scalar functions and diagnostic count reads;
- successful `DO PI()`;
- wrong-arity native diagnostics;
- bare identifier and unsupported nested scalar forms;
- file-backed preamble preservation and catalog/schema-generation immutability;
- independent handles returning the same constant;
- zero-initialized cleanup coverage through existing result-free and close
  paths.

Run:

```sh
packages/libmylite/tests/mysql_baseline_pi_function_expectations.sh
cmake --build --preset dev
ctest --test-dir build/dev --output-on-failure -R 'libmylite\.(parser|runtime\.pi_function|runtime\.conv_function|runtime\.do_statement|runtime\.scalar_arithmetic_projection)'
cmake --workflow --preset check
```

## Compatibility Documentation

Update only the exact admitted subset:

- `COMPATIBILITY.md`: mark `PI()` as limited no-source/`DUAL`/`DO` support.
  Later row-scalar support is documented by
  `baseline-row-numeric-extra-functions`.
- `docs/compatibility/functions-numeric-math.md`: document the limited
  top-level scalar `PI()` subset.
- `docs/compatibility/sql-query-expressions.md`: mention `PI()` in the
  no-source/`DUAL` scalar projection domain.
- `docs/compatibility/sql-stored-programs.md`: mention `PI()` in the limited
  `DO` domain.

Do not claim general DOUBLE support, approximate arithmetic, broad function
nesting, expression metadata, protocol metadata, collations, casts, parameters,
subqueries, CTEs, or SQLite function pass-through.
