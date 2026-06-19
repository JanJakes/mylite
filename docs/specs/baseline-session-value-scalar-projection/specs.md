# Baseline Session Value Scalar Projection

## Summary

This phase admits one deliberately small mixed scalar `SELECT` surface:

```sql
SELECT scalar_projection_item[, scalar_projection_item ...]
SELECT ALL scalar_projection_item[, scalar_projection_item ...]
SELECT scalar_projection_item[, scalar_projection_item ...] FROM DUAL
SELECT ALL scalar_projection_item[, scalar_projection_item ...] FROM DUAL
```

Each item may be either:

- an already-supported session scalar expression, such as `DATABASE()`,
  `USER()`, `CURRENT_USER`, `CURRENT_ROLE()`, `CONNECTION_ID()`, `VERSION()`,
  `ROW_COUNT()`, `LAST_INSERT_ID()`, or a supported `@@` system variable; or
- an already-supported scalar value expression from
  `baseline-scalar-expression-projection`: top-level integer/boolean/`NULL`
  literals and scalar `IF()`/`IFNULL()`/`COALESCE()`/`NULLIF()`/`ISNULL()`
  values over the current warning-free operand subset.

The user-visible addition is mixed lists such as
`SELECT VERSION(), 1, IF(1,2,3), @@warning_count FROM DUAL`. The architectural
addition is one scalar projection classifier for the existing session-scalar
and scalar-value domains. Evaluation remains fully MyLite-owned and still
returns a single synthesized row; no table-backed expression projection or
SQLite expression delegation is introduced.

This is not a general expression engine. It does not admit arithmetic,
comparison, logical operators, variables as scalar function operands, table
columns, broad table-backed constant projection, no-source
`WHERE`/`ORDER BY`/`LIMIT`, subqueries, CTEs, expression metadata, or arbitrary
SQLite pass-through. Later focused row-scalar slices admit selected
source-backed session constants such as `DATABASE()`, `VERSION()`, and
`CONNECTION_ID()` in the documented row-scalar SELECT envelope.

## Sources And Evidence

- Official MySQL 8.4 Reference Manual:
  - `SELECT` statement and `DUAL`:
    <https://dev.mysql.com/doc/refman/8.4/en/select.html>
  - Expression syntax:
    <https://dev.mysql.com/doc/refman/8.4/en/expressions.html>
  - Information functions:
    <https://dev.mysql.com/doc/refman/8.4/en/information-functions.html>
  - Server system variables:
    <https://dev.mysql.com/doc/refman/8.4/en/server-system-variables.html>
  - Flow-control functions:
    <https://dev.mysql.com/doc/refman/8.4/en/flow-control-functions.html>
  - Comparison functions and `ISNULL()`:
    <https://dev.mysql.com/doc/refman/8.4/en/comparison-operators.html>
- MySQL runtime probes are captured in
  `packages/libmylite/tests/mysql_baseline_session_value_scalar_projection_expectations.sh`
  and verified against MySQL 8.4.9.

Runtime probes against MySQL 8.4.9 confirm:

- mixed session/value scalar select lists return one row with one column per
  expression;
- `FROM DUAL` has the same result for expressions that reference no table;
- explicit `ALL` remains the duplicate-preserving default;
- default labels preserve source text for functions, system variables, and most
  parenthesized expressions, while existing literal label exceptions such as
  `(1)` and `(NULL)` continue to apply;
- `ROW_COUNT()` inside the mixed select returns the previous statement's row
  count, while a following `ROW_COUNT()` returns `-1`;
- `@@warning_count` inside a mixed select observes warnings produced by that
  select, including `@@sql_slave_skip_counter` deprecation warnings;
- a mixed select containing both `@@sql_slave_skip_counter` and
  `@@global.sql_slave_skip_counter` reports two warnings; and
- MySQL accepts broader forms such as arithmetic, no-source predicates,
  no-source ordering/limiting, and table-backed constant/function projection.
  Those are deferred by this original MyLite slice except where later focused
  row-scalar slices explicitly admit them.

## Ownership Boundaries

- Public API: no ABI or public-header changes. `mylite_execute()` continues to
  own result-handle lifetime, diagnostics, and statement-boundary behavior.
- Statement context: successful mixed scalar `SELECT` statements use the
  existing row-returning result conventions: one row, zero affected rows,
  statement warning count, and previous row-count state updated to `-1`.
- Lexer/parser/AST: no new tokens are required. Existing expression AST nodes,
  system-variable nodes, function nodes, parenthesized expressions, source
  spans, `FROM DUAL`, `ALL`, and aliases are reused.
- Analyzer/runtime: the scalar projection analyzer accepts a no-source or
  `FROM DUAL` select only when every select item is in the admitted session or
  scalar-value domain. Evaluation is performed by the existing MyLite scalar
  evaluator.
- Catalog: not involved. The feature must not read or mutate schema/table
  descriptors, descriptor versions, descriptor caches, catalog generation, or
  `sqlite_schema_generation`.
- Result builder: appends one column per select item and one row through
  existing result helpers. Explicit aliases continue to define result labels.
- Storage/VFS/file format: no storage writes, no physical table access, and no
  `.mylite` preamble changes.
- SQLite physical execution: no generated SQLite SQL and no SQLite fork patch.
  This is wrapper/runtime behavior.

## Supported SQL

Supported statement shapes:

```sql
SELECT scalar_projection_item[, scalar_projection_item ...]
SELECT ALL scalar_projection_item[, scalar_projection_item ...]
SELECT scalar_projection_item[, scalar_projection_item ...] FROM DUAL
SELECT ALL scalar_projection_item[, scalar_projection_item ...] FROM DUAL
```

Each select item may use the existing alias surface:

```sql
scalar_projection_item:
    scalar_projection_value
  | scalar_projection_value AS alias
  | scalar_projection_value alias
```

The admitted projection values are:

```sql
scalar_projection_value:
    session_scalar
  | scalar_value

session_scalar:
    DATABASE ( )
  | SCHEMA ( )
  | USER ( )
  | SESSION_USER ( )
  | SYSTEM_USER ( )
  | CURRENT_USER
  | CURRENT_USER ( )
  | CURRENT_ROLE ( )
  | CONNECTION_ID ( )
  | VERSION ( )
  | ROW_COUNT ( )
  | LAST_INSERT_ID ( )
  | supported_system_variable

scalar_value:
    baseline_scalar_expression_projection.scalar_value
```

Parentheses around admitted projection values are accepted through the existing
parenthesized-expression AST node.

Top-level scalar integer select items keep the existing literal-projection
envelope and diagnostics, including the current 81-significant-digit exact
integer limit. Scalar function operands remain limited to the warning-free
signed-64 baseline envelope.

### MyLite Lemon-Syntax Snippet

No new parser production is required for this phase. The parser already accepts
the relevant expression forms. The analyzer/runtime acceptance grammar is:

```lemon
scalar_projection_value(A) ::= session_scalar(B).
scalar_projection_value(A) ::= scalar_value(B).

session_scalar(A) ::= DATABASE LPAREN RPAREN.
session_scalar(A) ::= SCHEMA LPAREN RPAREN.
session_scalar(A) ::= USER LPAREN RPAREN.
session_scalar(A) ::= SESSION_USER LPAREN RPAREN.
session_scalar(A) ::= SYSTEM_USER LPAREN RPAREN.
session_scalar(A) ::= CURRENT_USER.
session_scalar(A) ::= CURRENT_USER LPAREN RPAREN.
session_scalar(A) ::= CURRENT_ROLE LPAREN RPAREN.
session_scalar(A) ::= CONNECTION_ID LPAREN RPAREN.
session_scalar(A) ::= VERSION LPAREN RPAREN.
session_scalar(A) ::= ROW_COUNT LPAREN RPAREN.
session_scalar(A) ::= LAST_INSERT_ID LPAREN RPAREN.
session_scalar(A) ::= system_variable_reference.
scalar_projection_value(A) ::= LPAREN scalar_projection_value(B) RPAREN.
```

These snippets are independently authored for MyLite's admitted subset and are
not MySQL's full grammar.

## Semantics

Evaluation is one row wide:

1. Validate every select item against the admitted session-scalar or
   scalar-value subset.
2. Resolve system variables using the existing MyLite system-variable resolver.
3. Append existing system-variable read warnings before value extraction, so
   `@@warning_count` in the same select observes warnings raised by that select.
4. Evaluate each select item independently from left to right.
5. Preserve existing session scalar values and scalar value function semantics.
6. Preserve SQL `NULL`.
7. Render booleans as `1` and `0`.
8. Render admitted integers in canonical decimal text.
9. Return one result row with one value per select item.

Successful supported statements return:

- one row;
- one column per select item;
- `affected_rows == 0`;
- the statement warning count, including existing `@@sql_slave_skip_counter`
  deprecation warnings when selected; and
- a following `ROW_COUNT()` result of `-1`.

`ROW_COUNT()` selected inside the statement returns the previous statement's
row-count state, not `-1` from the current select.

Default result-column labels continue to use the existing source-span rules:

- `VERSION()` labels as `VERSION()`;
- `(@@warning_count)` labels as `(@@warning_count)`;
- `(DATABASE())` labels as `(DATABASE())`;
- `(1)` labels as `1`;
- `(NULL)` labels as `NULL`;
- `(IF(1,2,3))` labels as `(IF(1,2,3))`; and
- explicit aliases override the default label.

## Unsupported And Diagnostics

Unsupported forms must fail deterministically without falling through to
SQLite:

- broad table-backed scalar projection outside later focused row-scalar
  session-constant slices;
- no-source or `DUAL` scalar projection with `WHERE`, `ORDER BY`, `GROUP BY`,
  `HAVING`, or `LIMIT`;
- arithmetic, comparison, logical, bitwise, cast, string, decimal, float, hex,
  bit, temporal, JSON, parameter, subquery, column-reference, aggregate,
  window, or arbitrary function values;
- user variables and system variables as operands inside scalar value
  functions, such as `IF(@@warning_count,1,0)`;
- expression use in DML assignments, defaults, predicates, table `ORDER BY`,
  `GROUP BY`, `HAVING`, or aggregate arguments; and
- scalar function integer operands outside the admitted signed-64
  warning-free envelope.

Wrong arities for existing native functions retain the existing
function-specific MySQL diagnostics. Unsupported expressions that look like a
mixed scalar projection but exceed the admitted domain should use a
MyLite-specific diagnostic that mentions supported session scalar expressions
and the current scalar value subset. Existing lower-level diagnostics may
remain where they are more precise.

Unknown system variables, disallowed scopes, allocation failures, parse errors,
and public API misuse preserve the existing diagnostics.

## Tests

Fast C tests should cover:

- mixed no-source and `FROM DUAL` projection containing session functions,
  system variables, literals, and scalar value functions;
- explicit `ALL`;
- explicit aliases and default labels, including parenthesized session
  expressions and parenthesized scalar values;
- `ROW_COUNT()` inside a mixed statement and after it;
- `@@warning_count`, `@@error_count`, and `ROW_COUNT()` with a mixed select
  containing `@@sql_slave_skip_counter`;
- two warning-producing variables in one mixed projection;
- selected-schema and no-selected-schema `DATABASE()` behavior in a mixed list;
- independent handles and file-backed preamble/catalog-generation safety;
- deterministic rejection of broad table-backed scalar projection outside later
  focused row-scalar session-constant slices, arithmetic, clauses around
  no-source scalar projection, variables inside scalar value functions,
  subqueries, column references, unsupported functions, and wrong arities; and
- preservation of existing session-scalar-only, scalar-value-only, parser,
  result, statement-context, storage, and lifecycle tests.

Focused verification:

1. build parser/runtime test targets touched by the implementation;
2. run focused parser/runtime CTest entries;
3. run
   `packages/libmylite/tests/mysql_baseline_session_value_scalar_projection_expectations.sh`;
4. run `cmake --workflow --preset check`.

## Compatibility Notes

This phase reduces another expression-lane split but intentionally does not
claim general expression support. General expressions, broad table-backed
expression projection, subqueries, type metadata, and optimizer-grade
expression pushdown still require a larger planner/evaluator design and may
need SQLite extension hooks or targeted fork hooks where public SQLite APIs
cannot expose
MySQL-compatible semantics or avoidable overhead.
