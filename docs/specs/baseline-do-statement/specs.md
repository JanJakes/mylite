# Baseline DO Statement

## Summary

This phase admits a deliberately small MySQL `DO` statement surface:

```sql
DO scalar_expression[, scalar_expression ...]
```

Each expression is evaluated for diagnostics and side effects that MyLite
already owns, but no result set is returned. The admitted expression domain is
the current no-source scalar projection domain: session scalar functions and
supported system-variable reads, decimal integer/`TRUE`/`FALSE`/`NULL`
literals, supported scalar control-flow helpers, unary and binary signed-64
arithmetic already admitted by scalar projection, scalar comparisons, keyword
logical operators, scalar `IS`, and top-level searched/simple `CASE` with the
same composition limits as the `CASE` baseline.

This is not a general expression engine. It does not add table-backed
evaluation, aliases, variables, assignment, subqueries, parameters, stored
program blocks, user-defined functions, string/decimal/float/hex/bit values,
temporal values, expression metadata, or arbitrary SQLite pass-through.

## Compatibility Authority

- Official MySQL 8.4 documentation:
  - `DO` statement: <https://dev.mysql.com/doc/refman/8.4/en/do.html>
  - Expressions: <https://dev.mysql.com/doc/refman/8.4/en/expressions.html>
  - Functions and operators: <https://dev.mysql.com/doc/refman/8.4/en/functions.html>
  - Flow-control functions: <https://dev.mysql.com/doc/refman/8.4/en/flow-control-functions.html>
  - Operator precedence: <https://dev.mysql.com/doc/refman/8.4/en/operator-precedence.html>
- Observed MySQL 8.4.9 runtime behavior, captured by
  `packages/libmylite/tests/mysql_baseline_do_statement_expectations.sh`.

The MySQL manual defines `DO` as expression execution without returning
results and notes that it cannot use table-reference forms such as
`DO id FROM t1`.

Runtime probes against MySQL 8.4.9 establish these expectations for this
slice:

- `DO` requires at least one expression.
- one or more expressions are evaluated left to right;
- successful warning-free `DO` reports `@@warning_count = 0`;
- a successful `DO` makes a following `ROW_COUNT()` return `0`;
- `DO` does not return result columns or rows;
- evaluated `DIV` or modulo-by-zero expressions stage warnings in the same
  way as scalar `SELECT`;
- scalar logical and `CASE` short-circuiting suppresses skipped child
  warnings;
- arithmetic overflow raises MySQL error 1690 / SQLSTATE `22003`;
- wrong native-function arities preserve the existing MySQL-compatible
  parameter-count diagnostic;
- MySQL accepts broader expression forms such as scalar subqueries,
  user-variable assignments, string/decimal/float/hex/bit expressions, and
  aliases after `DO` expressions. MyLite defers those forms in this baseline.

## Ownership Boundaries

- Public API: no ABI or public-header changes. `mylite_execute()` continues to
  own result-handle lifetime, diagnostics, and statement-boundary behavior.
- Statement context: successful `DO` uses non-row statement conventions:
  zero result columns, zero result rows, affected rows `0`, statement warning
  count from evaluated expressions, and previous row count `0` after success.
- Lexer/parser/AST: the parser adds a statement-level `DO` production and an
  AST node whose children are the expression list. It does not add aliases,
  `FROM`, stored-program statements, or procedural `DO` variants.
- Analyzer/runtime: runtime validates each `DO` child against the current
  scalar projection expression domain, evaluates each admitted expression, and
  appends staged warnings. Values are discarded.
- Catalog: not involved. The feature must not read or mutate schemas, table
  descriptors, descriptor versions, descriptor caches, catalog generation, or
  `sqlite_schema_generation`.
- Result builder: creates an empty result object through existing non-row
  statement conventions and sets affected rows to `0`.
- Storage/VFS/file format: no storage writes, no physical table access, and no
  `.mylite` preamble changes.
- SQLite physical execution: no generated SQLite SQL and no SQLite fork patch.
  This is MyLite wrapper/runtime behavior.

## Syntax

The independent MyLite subset is:

```ebnf
do_statement:
    DO do_expression_list

do_expression_list:
    scalar_expression
  | do_expression_list , scalar_expression
```

`scalar_expression` is the same direct-expression domain admitted by
no-source scalar `SELECT` projection:

- session scalar functions and supported system-variable reads;
- decimal integer literals with optional unary sign;
- `TRUE`, `FALSE`, and `NULL`;
- parenthesized admitted expressions;
- supported scalar `IF()`, `IFNULL()`, `COALESCE()`, `NULLIF()`, and
  `ISNULL()` calls;
- supported signed-64 `+`, binary `-`, `*`, `%`, infix `MOD`, `MOD()`, and
  infix `DIV`;
- supported signed-64 scalar comparisons;
- supported keyword scalar logical `NOT`, `AND`, `XOR`, and `OR`;
- supported scalar `IS [NOT] NULL` / `TRUE` / `FALSE` / `UNKNOWN`; and
- top-level searched/simple `CASE` under the existing `CASE` slice limits.

### MyLite Lemon-Syntax Snippet

```lemon
statement(A) ::= do_statement(B). {
    A = B;
}

do_statement(A) ::= DO(T) do_expression_list(E). {
    A = mylite_sql_parser_make_do_statement(state, T, E);
}

do_expression_list(A) ::= expression(B). {
    A = mylite_sql_parser_make_do_expression_list(state, B);
}
do_expression_list(A) ::= do_expression_list(B) COMMA expression(C). {
    A = mylite_sql_parser_append_do_expression(state, B, C);
}
```

These snippets are independently authored for MyLite's admitted subset and are
not MySQL's full grammar.

## Semantics

Execution order:

1. Validate that every child expression is in the admitted scalar expression
   domain.
2. Preserve known native-function wrong-arity diagnostics.
3. Evaluate expressions from left to right.
4. Discard each resulting value.
5. Append staged division-by-zero warnings after all admitted expressions have
   been evaluated successfully.
6. Return an empty result object with affected rows `0`.
7. Set the connection's previous row-count state to `0`.

`DO` evaluates the same MyLite-owned scalar semantics as no-source scalar
`SELECT`, including:

- SQL `NULL` propagation for admitted arithmetic/comparison/logical forms;
- nonzero and non-`NULL` truth for scalar logical and `CASE` conditions;
- scalar logical short-circuiting;
- `CASE` branch selection and skipped-branch warning suppression; and
- `@@warning_count`, `@@error_count`, and `ROW_COUNT()` reads from the current
  MyLite diagnostics/session state rules.

Because `DO` returns no rows, expression labels and values are not visible.
The expression text is still used for parser spans and diagnostics.

## Diagnostics

Supported successful statements:

- return `MYLITE_OK`;
- return a result object with `column_count == 0` and `row_count == 0`;
- report `affected_rows == 0`;
- report the number of warnings staged by evaluated expressions; and
- leave catalog and storage state unchanged.

Diagnostics for this baseline:

- syntax errors use the existing MySQL-compatible parse diagnostic surface;
- unsupported expression forms use deterministic MyLite-specific unsupported
  diagnostics;
- wrong native-function arities use MySQL error 1582 / SQLSTATE `42000`;
- signed-64 arithmetic overflow uses MySQL error 1690 / SQLSTATE `22003`;
- evaluated division or modulo by zero appends warning 1365 / SQLSTATE
  `22012`;
- unsupported system variables preserve the existing system-variable
  diagnostic behavior;
- allocation failure returns `MYLITE_NOMEM`; and
- public API misuse remains unchanged.

Unsupported for this slice:

- empty `DO`;
- `DO ... FROM ...`;
- expression aliases such as `DO 1 AS x`;
- table-backed column references;
- scalar subqueries;
- user variables and assignment operators;
- parameters;
- string, decimal, float, hex, bit, and temporal literals;
- `/` division;
- symbolic logical operators `!`, `&&`, and `||`;
- table-backed expressions;
- stored-program compound statements; and
- arbitrary SQLite pass-through.

## Runtime And Storage Design

`DO` stays in the MyLite runtime. It should not be lowered to SQLite because
the supported expressions are already evaluated by MyLite and the statement
does not need physical row access.

Implementation shape:

1. Add a `DO` parser token mapping and AST statement node.
2. Store a dedicated expression-list child under the `DO` statement.
3. Add runtime dispatch for the new statement kind.
4. Validate each child with the same scalar-projection classifier used by
   no-source `SELECT`.
5. Evaluate each child with the existing `session_scalar_value()` path.
6. Accumulate staged division-by-zero warnings without materializing result
   rows.
7. Return a normal empty result with affected rows `0`.
8. Add `DO` to the completed-statement row-count rules as a non-row statement
   with row count `0`.

The implementation must not generate SQLite SQL, bind SQLite parameters,
touch descriptor catalog rows, mutate storage, or add a SQLite fork patch.

## Performance

The supported `DO` path is O(number of AST nodes in admitted expressions) and
allocates only the result handle plus any existing evaluator stack growth for
nested expressions. It does not scan tables, materialize row sets, or call
SQLite for execution. This is intentionally cheaper than a scalar `SELECT`
because it does not allocate result columns or append a result row.

## Test Plan

Add fast C coverage under `packages/libmylite/tests/`, preferably
`runtime_do_statement_test.c`, with a dotted CTest name
`libmylite.runtime.do_statement`.

Cover:

- one-expression and multi-expression successful `DO`;
- supported integer, boolean, `NULL`, scalar function, arithmetic,
  comparison, logical, scalar `IS`, and top-level `CASE` expressions;
- warning-free statements with affected rows `0`, warning count `0`, no
  result rows, and following `ROW_COUNT() = 0`;
- evaluated division-by-zero warnings and skipped-child warning suppression;
- arithmetic overflow;
- wrong function arity;
- unsupported strings, decimals, floats, hex, bit, parameters, user variables,
  assignment, table column references, scalar subqueries, `/`, symbolic
  logical operators, aliases, and `FROM`;
- catalog generation and SQLite schema generation remain unchanged;
- `.mylite` preamble preservation;
- independent file-backed handles keep independent row-count/diagnostics
  state; and
- zero-initialized cleanup for any new objects.

Run:

1. `packages/libmylite/tests/mysql_baseline_do_statement_expectations.sh`
2. `cmake --build --preset dev`
3. `ctest --preset dev -R 'libmylite\\.(parser|runtime\\.do_statement|runtime\\.case_operator|runtime\\.scalar_(arithmetic|comparison|logical|is)_projection|runtime\\.(if|ifnull|coalesce|nullif|isnull)_function)' --output-on-failure`
4. `cmake --workflow --preset check`
