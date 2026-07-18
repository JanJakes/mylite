# Baseline ORDER BY FIELD Function

## Goal

Add the first narrow expression-ordering slice for common application queries
that rank rows by an explicit list:

```sql
SELECT id, option_name
FROM options
ORDER BY FIELD(option_name, 'User 0000019', 'User 0000018', 'User 0000020');
```

This feature reuses MyLite's existing limited `FIELD()` expression support and
extends the descriptor-backed `SELECT` ordering path. It is not a general
expression-ordering engine, general function coverage project, optimizer
extension, or DML ordering feature. A later WordPress compatibility expansion
also admits the narrow joined `CAST(descriptor AS CHAR)` and
`descriptor + integer_literal` order expressions documented below.

## Sources

- Official MySQL 8.4 Reference Manual, `SELECT` statement:
  <https://dev.mysql.com/doc/refman/8.4/en/select.html>
- Official MySQL 8.4 Reference Manual, functions and operators:
  <https://dev.mysql.com/doc/refman/8.4/en/functions.html>
- Official MySQL 8.4 Reference Manual, string functions and operators:
  <https://dev.mysql.com/doc/refman/8.4/en/string-functions.html>
- Existing `FIELD()` design:
  `docs/specs/baseline-field-function/specs.md`
- Existing `ORDER BY` design:
  `docs/specs/baseline-select-order-by-multiple-columns/specs.md`
- Observed MySQL 8.4.9 runtime behavior, captured by
  `packages/libmylite/tests/mysql_baseline_order_by_field_function_expectations.sh`.

The MyLite grammar and implementation are independently authored from official
documentation, observed MySQL 8.4.9 behavior, public SQLite APIs, and existing
MyLite code. Do not copy MySQL, MariaDB, Percona, SQLite implementation
internals, or restrictively licensed sources.

## MySQL 8.4.9 Runtime Observations

Runtime probes establish the behavior used by this baseline:

- MySQL accepts `FIELD()` as an `ORDER BY` expression for ordinary `SELECT`.
- The expression returns the same integer rank as projection `FIELD()`: `0`
  for a `NULL` search value or no match, otherwise the 1-based first matching
  candidate position.
- Ascending order is the default; rows with rank `0` sort before positive
  ranks. `DESC` reverses that rank ordering.
- `ORDER BY FIELD(column, ...)` can refer to a column that is not present in the
  select list.
- `WHERE` filtering happens before ordering. `LIMIT` applies after ordering.
- Under MySQL's default `utf8mb4_0900_ai_ci` collation, ASCII case differences
  do not prevent string matches.
- Ties with the same `FIELD()` rank have no deterministic row order unless
  additional sort keys are supplied. This slice does not add secondary
  expression keys, so tests avoid overclaiming tie order.
- Unknown columns inside `FIELD()` used by `ORDER BY` fail with
  `1054 / 42S22` and message context `order clause`.
- MySQL accepts broader forms, including mixed-domain arguments, ordinal or
  additional expression sort keys, grouped queries, `DISTINCT`, `TABLE`, and
  DML `ORDER BY FIELD()`. Those forms are deferred by this slice.
- MySQL accepts joined `ORDER BY FIELD(table.column, ...)`, joined
  `ORDER BY CAST(table.column AS CHAR)`, and numeric coercion order keys such
  as `ORDER BY table.column + 0`.

## Supported Surface

MyLite supports:

- descriptor-backed persistent and shadowing temporary single-table `SELECT`;
- descriptor-backed joined `SELECT` over the existing plain joined source
  envelope for the supported order-expression shapes;
- row-scalar single-table `SELECT` when the select list already belongs to the
  existing row-scalar projection envelope;
- optional existing single-table `WHERE` predicate subset;
- optional existing `LIMIT` / `OFFSET` subset for `SELECT`;
- exactly one top-level `ORDER BY FIELD(search_column, value[, value ...])`
  item, optionally followed by `ASC` or `DESC`, including joined source
  column references for non-`DISTINCT` plain joined `SELECT`;
- existing row-scalar `CAST(descriptor_column AS CHAR)` order keys over
  single-table or joined descriptor sources;
- narrow signed integer arithmetic order keys in the form
  `descriptor_column + decimal_integer_literal`, used for WordPress
  `meta_value + 0` numeric sorting over admitted integer/string descriptors;
- parenthesized top-level `FIELD()` order keys;
- `search_column` as an unqualified, table-qualified, alias-qualified, or
  schema-qualified descriptor column that resolves to the one selected source;
- candidate values from the existing limited `FIELD()` argument domain:
  - string literals for ASCII `CHAR`, `VARCHAR`, and baseline `TEXT` family
    descriptor columns;
  - signed 64-bit decimal integer literals with optional unary sign for
    integer-family descriptor columns;
  - `TRUE` and `FALSE` as integer candidates;
  - `NULL` candidates, which never match non-`NULL` search values;
- fixed ASCII `utf8mb4_0900_ai_ci` string comparison behavior inherited from
  the existing `FIELD()` implementation;
- warning count `0` for supported in-range forms.

The generated physical query still lets SQLite do filtering, sorting, and
limiting. MyLite does not materialize rows into memory to sort them.

## Deferred Surface

This slice intentionally does not support:

- `ORDER BY FIELD()` in `UPDATE`, `DELETE`, `TABLE`, grouped aggregate queries,
  `DISTINCT` / `DISTINCTROW`, `UNION`, `CREATE TABLE ... SELECT`,
  `INSERT ... SELECT`, `REPLACE ... SELECT`, subqueries, CTEs, views, or
  arbitrary SQLite pass-through;
- multiple `ORDER BY` items when one item is `FIELD()`;
- nested `FIELD()`, nested functions, arithmetic, variables, parameters,
  subqueries, casts, collations, introducers, or arbitrary expressions inside
  `FIELD()` arguments;
- mixed string/numeric comparison domains;
- exact decimal, approximate, temporal, binary, blob, enum, set, JSON, or
  spatial comparison domains;
- non-ASCII collation parity, accent folding, or full Unicode collation
  weights;
- alias ordering where the alias names a `FIELD()` projection;
- ordinal order keys, string-literal order keys, expression secondary keys, or
  deterministic tie-breaking for equal `FIELD()` ranks;
- general arithmetic order expressions beyond the narrow
  `descriptor_column + decimal_integer_literal` form.

## Grammar

The parser already admits `FIELD()` as a general expression, but current
top-level `SELECT ORDER BY` grammar is narrower than general expressions. This
feature adds a narrow order-key alternative for `FIELD()` instead of widening
`ORDER BY` to arbitrary expression syntax. The supported MyLite subset is
described independently as:

```lemon
select_order_clause(A) ::= ORDER BY field_order_item(B).

field_order_item(A) ::= field_order_expression(B).
field_order_item(A) ::= field_order_expression(B) ASC.
field_order_item(A) ::= field_order_expression(B) DESC.

field_order_expression(A) ::=
    FIELD LPAREN field_order_search(B) COMMA field_order_value_list(C) RPAREN.
field_order_expression(A) ::= LPAREN field_order_expression(B) RPAREN.

field_order_search(A) ::= descriptor_column_reference(B).

field_order_value(A) ::= string_literal(T).
field_order_value(A) ::= decimal_integer_literal(T).
field_order_value(A) ::= PLUS decimal_integer_literal(T).
field_order_value(A) ::= MINUS decimal_integer_literal(T).
field_order_value(A) ::= TRUE(T).
field_order_value(A) ::= FALSE(T).
field_order_value(A) ::= NULL(T).
field_order_value(A) ::= LPAREN field_order_value(B) RPAREN.

field_order_value_list(A) ::= field_order_value(B).
field_order_value_list(A) ::= field_order_value_list(B) COMMA field_order_value(C).
```

The WordPress compatibility expansion keeps the same narrow `ORDER BY` grammar
shape and admits these additional row-scalar order keys through existing
expression nodes:

```lemon
select_order_key(A) ::= CAST LPAREN descriptor_column_reference(B) AS CHAR RPAREN.
select_order_key(A) ::= descriptor_column_reference(B) PLUS decimal_integer_literal(C).
```

The existing parser grammar is broader; analyzer/runtime validation narrows it
to the subset above. These snippets describe MyLite's supported subset, not
MySQL's full grammar.

## Runtime Semantics

Planning:

1. Resolve the source table or admitted joined source chain through the existing
   selected/default schema policy.
2. Reject reserved MyLite schema/table names and unsupported object kinds via
   existing table-resolution diagnostics before any SQLite SQL is generated.
3. Reject grouped, `DISTINCT`, DML, and table-statement `ORDER BY FIELD()`
   attempts for this slice.
4. Resolve `FIELD()` search columns, row-scalar conversion operands, and narrow
   integer-arithmetic operands through MyLite descriptors, not SQLite metadata.
5. Convert `FIELD()` candidate literals through the existing limited `FIELD()`
   conversion path.
6. Classify `FIELD()` non-`NULL` comparison domains as all string or all integer.
   Reject mixed domains deterministically.
7. Store planned `FIELD()` and row-scalar order expressions as hidden
   order-plan state.
8. Generate standard SQLite SQL using the same `CASE` expression shape as
   limited projection `FIELD()`, existing row-scalar expression renderers for
   `CAST(descriptor AS CHAR)` and `descriptor + integer_literal`, quoted
   identifiers, source aliases where needed, and numbered parameters.

Generated SQL shape:

```sql
SELECT "id", "option_name"
FROM "_mylite_user_table_1"
WHERE ...
ORDER BY (
  CASE
    WHEN "option_name" IS NULL THEN 0
    WHEN "option_name" COLLATE mylite_utf8mb4_0900_ai_ci = ?1 COLLATE mylite_utf8mb4_0900_ai_ci THEN 1
    WHEN "option_name" COLLATE mylite_utf8mb4_0900_ai_ci = ?2 COLLATE mylite_utf8mb4_0900_ai_ci THEN 2
    ELSE 0
  END
) ASC
LIMIT ?3
```

The exact collation name and physical table name are internal MyLite details.
The important invariants are descriptor-derived column references, stable
physical table names, quoted identifiers, and bound literal parameters.

Execution:

- SQLite evaluates the generated expression while scanning and sorting.
- MyLite binds all `FIELD()` candidate literals plus existing predicate,
  expression, and limit literals before stepping the statement.
- The result column list and result metadata remain unchanged by the hidden
  order expression.
- Successful statements follow existing `SELECT` result conventions:
  `affected_rows == 0`, `ROW_COUNT()` becomes `-1`, and warning count is `0`.

## Ownership Boundaries

- Public API: unchanged. Users call `mylite_execute()` and inspect the existing
  result object.
- Statement context: unchanged except for normal successful `SELECT` status.
- Lexer/parser/AST: extends the top-level `SELECT ORDER BY` key grammar only
  far enough to admit `FIELD()` order keys while keeping the existing
  expression AST representation.
- Analyzer/planner: owns narrowing `ORDER BY FIELD()` to this supported subset,
  resolving descriptors, and rejecting unsupported order expressions.
- Catalog: read-only descriptor authority. No descriptor rows, descriptor
  versions, descriptor caches, catalog generation, or
  `sqlite_schema_generation` are mutated.
- Result builder: unchanged; hidden order expressions do not create result
  columns.
- Storage/VFS/file format: unchanged. `.mylite` preamble and shifted SQLite
  payload invariants are preserved.
- SQLite: use generated standard SQLite `CASE` plus MyLite's registered limited Unicode
  string collation. No SQLite fork patch is required.

## Diagnostics

Required diagnostics:

- parser syntax errors through existing parse diagnostics;
- wrong `FIELD()` argument count through existing `1582 / 42000` native
  function argument-count diagnostics;
- missing default schema, unknown schema/table, reserved table names, and
  unsupported object kinds through existing source diagnostics;
- unknown search columns through MySQL-compatible
  `Unknown column 'name' in 'order clause'`;
- unsupported order expressions:
  `SELECT ORDER BY supports only descriptor columns or FIELD(column, value, ...)`;
- unsupported multiple keys with `FIELD()`:
  `SELECT ORDER BY FIELD() supports only one order key`;
- unsupported grouped/distinct/DML/table-statement use through existing parse or
  deterministic MyLite-specific unsupported diagnostics;
- unsupported `FIELD()` argument shapes:
  `FIELD() supports only string, integer, boolean, and NULL arguments`;
- unsupported mixed domain:
  `FIELD() does not support mixed string and numeric arguments`;
- unsupported non-ASCII string values:
  `FIELD() string literals support only ASCII values`;
- out-of-range integer literals through the existing signed-64 row-scalar
  diagnostic;
- allocation failures through existing `MYLITE_NOMEM` behavior;
- physical SQLite failures through existing runtime diagnostics;
- public API misuse: no public API changes.

## Tests

Add MySQL-runtime expectation coverage for:

- string-domain `ORDER BY FIELD()` default ascending and explicit `ASC`;
- explicit `DESC`;
- integer-domain ordering, including boolean candidates;
- `WHERE` before ordering and `LIMIT` after ordering;
- `ORDER BY FIELD()` over a column not present in the select list;
- ASCII case-insensitive string matching;
- one-row `NULL` and unmatched rank-`0` ordering without tie-order claims;
- table alias, table-qualified, and schema-qualified search columns;
- unknown search column diagnostics in `order clause`;
- MySQL-accepted but deferred broader forms: multiple sort keys with
  `FIELD()`, `DISTINCT`, `TABLE`, mixed domains, nested functions, parameters,
  scalar subqueries, and DML `ORDER BY FIELD()`;
- supported joined `FIELD()` over a descriptor column;
- supported joined `CAST(descriptor AS CHAR)` order key;
- supported joined `descriptor + 0` numeric string order key;
- relaxed-mode `DISTINCT` joined `descriptor + 0` order key.

Add fast C tests under `packages/libmylite/tests/`, preferably
`runtime_order_by_field_function`, plus parser coverage showing that the AST
already represents `ORDER BY FIELD(...)`.

## Compatibility Updates

Update:

- `COMPATIBILITY.md`
- `docs/compatibility/functions-string.md`
- `docs/compatibility/sql-query-expressions.md`

Use limited wording. Do not claim general expression ordering, full `FIELD()`,
full collation semantics, grouped ordering, broad distinct ordering, DML
ordering, secondary expression sort keys, or deterministic tie order.
