# Baseline SOUNDS LIKE Operator

## Summary

This phase adds a narrow MySQL-compatible `expr1 SOUNDS LIKE expr2` operator
slice. MySQL defines the operator in terms of `SOUNDEX(expr1) =
SOUNDEX(expr2)`, so MyLite reuses its existing MySQL-observed `SOUNDEX()`
implementation rather than adding new SQLite behavior.

Supported behavior:

- no-source scalar `SELECT`, `SELECT ... FROM DUAL`, and `DO` expressions;
- single-table row-scalar `SELECT` projection;
- single-table descriptor-backed `WHERE` predicates;
- `NULL` propagation: if either operand soundex value is SQL `NULL`, the
  result is SQL `NULL`;
- result values are integer text `1` or `0` for non-`NULL` comparisons;
- row-backed forms lower to private SQLite `_mylite_soundex()` calls on both
  operands and compare the generated values with `=`.

The supported operand envelope matches the current `SOUNDEX()` baseline:
admitted scalar text-convertible values and descriptor-backed integer, exact
`DECIMAL`, nonbinary string, baseline `TEXT`, `YEAR`, and temporal columns.

## Sources And Evidence

- MyLite architecture and engineering standards:
  - `README.md`
  - `docs/architecture/engineering-standards.md`
- Existing `SOUNDEX()` feature:
  - `docs/specs/baseline-soundex-function/specs.md`
- Official MySQL 8.4 Reference Manual:
  - string functions and operators:
    <https://dev.mysql.com/doc/refman/8.4/en/string-functions.html#function_soundex>
- MySQL runtime probes are captured in
  `packages/libmylite/tests/mysql_baseline_soundex_function_expectations.sh`
  and verified against MySQL 8.4.9.

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior, public
SQLite APIs, and existing MyLite code. It does not copy MySQL, MariaDB,
Percona, SQLite implementation internals, or restrictively licensed sources.

## MySQL 8.4.9 Runtime Observations

Runtime probes establish the behavior used by this phase:

- `SELECT 'mood' SOUNDS LIKE 'mud', 'mood' SOUNDS LIKE 'xyz'` returns
  `1, 0`;
- `SELECT 'abc' SOUNDS LIKE NULL, NULL SOUNDS LIKE 'abc'` returns
  `NULL, NULL`;
- `SELECT 123 SOUNDS LIKE 'abc', TRUE SOUNDS LIKE '1'` returns `0, 1`;
- `SELECT 'Robert' SOUNDS LIKE 'Rupert', 'Robert' SOUNDS LIKE 'Rubin'`
  returns `1, 0`;
- table-backed operands compare the soundex values of converted column text;
- `WHERE v SOUNDS LIKE 'Robert'` keeps rows where the two soundex values are
  equal and filters out `NULL` results;
- exact `DECIMAL` descriptor operands compare after visible string conversion,
  for example a row value `-12.30 SOUNDS LIKE '-12.30'` returns `1`;
- binary literals participate in MySQL, but binary-string row-backed inputs
  remain outside the existing MyLite `SOUNDEX()` baseline and are not added in
  this phase.

Successful supported calls produce no warnings.

## Ownership Boundaries

- Public API: unchanged. Results are exposed through existing result objects as
  integer text or SQL `NULL`.
- Lexer/parser/AST: admits `expression SOUNDS LIKE expression` as a binary
  expression with a dedicated `sounds_like` operator.
- Scalar runtime: evaluates each operand through the same MyLite-owned soundex
  argument/result helper used by `SOUNDEX()`, then compares non-`NULL` soundex
  values byte-for-byte.
- Row planner/runtime: lowers row-backed projection and predicate forms to
  `_mylite_soundex(left) = _mylite_soundex(right)` inside generated SQLite SQL.
- SQLite execution: uses the existing private `_mylite_soundex()` scalar
  function registered through SQLite's public scalar-function API. No SQLite
  fork patch is required.
- Catalog/storage/file format: unchanged. The feature reads descriptors and
  rows only.

## Supported SQL

No-source and `DUAL` forms:

```sql
SELECT sounds_like_expr[, sounds_like_expr ...]
SELECT sounds_like_expr[, sounds_like_expr ...] FROM DUAL
```

`DO` form:

```sql
DO sounds_like_expr[, sounds_like_expr ...]
```

Single-table row-backed forms:

```sql
SELECT row_scalar_item[, row_scalar_item ...]
FROM table_name [AS alias]
[WHERE predicate]
[ORDER BY descriptor_column [ASC | DESC]]
[LIMIT row_count]
```

The admitted expression shape is:

```sql
sounds_like_expr:
    soundex_value SOUNDS LIKE soundex_value

soundex_value:
    string_literal
  | decimal_integer_literal
  | + decimal_integer_literal
  | - decimal_integer_literal
  | TRUE
  | FALSE
  | NULL
  | session_scalar_function
  | system_variable_reference
  | descriptor_column_reference        -- table-backed SELECT only
  | ( soundex_value )
```

Descriptor column references follow the existing single-source table alias
policy and may explicitly name invisible descriptor columns. Supported
descriptor column families are inherited from the current `SOUNDEX()` row
baseline:

- integer-family columns;
- exact `DECIMAL`;
- `YEAR`, `DATE`, `TIME`, `DATETIME`, and `TIMESTAMP`;
- `CHAR`, `VARCHAR`, and baseline `TEXT` family.

The following remain outside this phase:

- `NOT SOUNDS LIKE` or `expr NOT SOUNDS LIKE expr` syntax;
- binary-string or `BIT` row-backed inputs;
- approximate numeric row-backed inputs;
- scalar decimal and floating-point literal operands beyond the current
  `SOUNDEX()` scalar envelope;
- nested row functions other than the explicit two operand expressions;
- joins beyond the current row-scalar source envelope;
- DML assignment values, ordering/grouping expressions, aggregate arguments,
  parameters, stored functions, user-defined collations, and full expression
  metadata.

### MyLite Lemon-Syntax Snippet

The parser grammar is authored independently and extends MyLite's expression
grammar:

```lemon
expression(A) ::= expression(B) SOUNDS(S) LIKE expression(C). [LIKE] {
    A = mylite_sql_parser_make_binary_expression(
        state, B, S, MYLITE_SQL_AST_OPERATOR_SOUNDS_LIKE, C);
}
```

## Runtime Semantics

For no-source, `DUAL`, and `DO` expressions:

1. Unwrap supported parentheses.
2. Evaluate the left operand using the same conversion and soundex generation
   path as `SOUNDEX(left)`.
3. Evaluate the right operand using the same conversion and soundex generation
   path as `SOUNDEX(right)`.
4. If either generated value is SQL `NULL`, return SQL `NULL`.
5. Return `1` when both generated values have the same byte length and bytes;
   otherwise return `0`.

For row-backed projection and predicate expressions:

1. Plan each operand with the existing row-scalar `SOUNDEX()` operand rules.
2. Render each operand inside `_mylite_soundex(...)`.
3. Render the comparison as:

   ```sql
   (_mylite_soundex(left_sql) = _mylite_soundex(right_sql))
   ```

SQLite's normal three-valued comparison behavior makes either `NULL` operand
produce SQL `NULL` in projection and non-matching unknown in `WHERE`, matching
the observed MySQL behavior for this baseline.

## Diagnostics

Unsupported operand shapes use the same diagnostics as the current `SOUNDEX()`
baseline where practical. For example, binary-string and approximate numeric
row-backed inputs are rejected with the row-scalar `SOUNDEX()` unsupported
messages. Unknown descriptor columns keep the existing unknown-column
diagnostics.

`SOUNDS` remains usable as an identifier in contexts where MyLite admits
keyword identifiers.

## Test Plan

- Extend the MySQL expectation script for scalar, `NULL`, numeric, table
  projection, and table predicate `SOUNDS LIKE` behavior.
- Extend the runtime `SOUNDEX()` test to cover no-source, `DUAL`, `DO`,
  row-backed projection, row-backed `WHERE`, reopen persistence invariants, and
  unsupported row operand diagnostics.
- Update parser corpus runtime coverage so `SELECT 'mood' SOUNDS LIKE 'mud'`
  is no longer an unsupported placeholder.
- Update compatibility documentation in `COMPATIBILITY.md`,
  `docs/compatibility/functions-string.md`, and
  `docs/compatibility/operators.md`.

## Compatibility Status

This feature is `🟡` because it implements the practical baseline over the
current MyLite `SOUNDEX()` operand envelope but does not provide the full
expression, collation, binary-string, or DML placement surface.
