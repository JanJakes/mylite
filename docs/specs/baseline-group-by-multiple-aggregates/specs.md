# Baseline GROUP BY Multiple Aggregates

## Goal

Expand the existing descriptor-driven grouped aggregate select path from one
aggregate result to a small, common MySQL-compatible shape:

```sql
SELECT group_column, aggregate_1 [, aggregate_2 ...]
FROM source
[WHERE predicate]
GROUP BY group_column
[HAVING grouped_or_selected_aggregate_predicate]
[ORDER BY group_column_or_selected_count_min_max_sum_avg_bitwise_alias [ASC|DESC]]
[LIMIT row_count [OFFSET offset]]
```

This remains a narrow grouped-query slice. MyLite still admits one descriptor
group column and the current one-source or two-source joined source envelope.
The new behavior is multiple selected count/numeric/bitwise aggregate results,
including the integer descriptor-column `COUNT(DISTINCT column)` slice for the
base-table grouped path, and one selected `COUNT`, `MIN`, `MAX`, `SUM`, `AVG`,
or bitwise aggregate-alias order key.
SQLite still performs source scanning, filtering, grouping, aggregate stepping,
ordering, and limiting through a generated physical query; MyLite owns
descriptor resolution, supported-surface validation, result metadata, and
MySQL-compatible formatting for current aggregate result types.

## Sources

- Official MySQL 8.4 `SELECT` syntax:
  <https://dev.mysql.com/doc/refman/8.4/en/select.html>
- Official MySQL 8.4 grouped-query handling:
  <https://dev.mysql.com/doc/refman/8.4/en/group-by-handling.html>
- Official MySQL 8.4 aggregate function descriptions:
  <https://dev.mysql.com/doc/refman/8.4/en/aggregate-functions.html>
- Observed MySQL 8.4.9 runtime behavior, captured by
  `packages/libmylite/tests/mysql_baseline_group_by_multiple_aggregates_expectations.sh`
  and the existing
  `packages/libmylite/tests/mysql_baseline_group_by_single_column_aggregate_expectations.sh`.

The MyLite grammar and implementation are independently authored from the
official documentation and MySQL 8.4.9 runtime observations. Do not copy MySQL
grammar or implementation sources.

## MySQL 8.4.9 Runtime Observations

Runtime probes against the local MySQL 8.4.9 container establish these
expectations for this slice:

- A grouped select may project more than one aggregate result with the grouped
  column, for example `COUNT(*)`, `COUNT(column)`, `MIN`, `MAX`, `SUM`, `AVG`,
  and bitwise aggregate functions in the same select list.
- `COUNT(*)` counts all rows in the group. `COUNT(column)` counts non-`NULL`
  argument values. `MIN`, `MAX`, `SUM`, and `AVG` ignore `NULL` argument
  values and return `NULL` for groups with no non-`NULL` argument values.
- Numeric bitwise aggregates return unsigned 64-bit results. An all-`NULL`
  group returns the neutral value for the function: all bits set for
  `BIT_AND`, zero for `BIT_OR`, and zero for `BIT_XOR`.
- The MySQL client displays grouped `AVG(integer_column)` with four fractional
  digits for the verified integer descriptor families in this baseline.
- `HAVING` can reference a selected aggregate by expression or by alias. In a
  multi-aggregate select, the verified baseline predicates use one selected
  non-bitwise aggregate result.
- MySQL accepts `ORDER BY selected_aggregate_alias` for `COUNT`, `MIN`, `MAX`,
  `SUM`, `AVG`, and bitwise aggregate aliases. MyLite admits selected `COUNT`,
  `MIN`, `MAX`, `SUM`, `AVG`, and bitwise aggregate aliases in this slice.
  `AVG` aliases order by exact signed `SUM()/COUNT()` rational value rather
  than SQLite's floating `AVG()`.
- Successful grouped selects report `ROW_COUNT() = -1` and `@@warning_count = 0`.
- With default `ONLY_FULL_GROUP_BY`, selecting a nonaggregate descriptor column
  that is not the group column fails with `1055 / 42000`.

## Supported Surface

MyLite supports this exact extension to the existing grouped aggregate path:

- one grouped descriptor column in the select list and matching `GROUP BY`;
- at least one and at most sixteen selected aggregate results after the grouped
  column;
- aggregate forms:
  - `COUNT(*)`;
  - `COUNT(column)`;
  - `COUNT(DISTINCT column)` for integer descriptor columns;
  - `MIN(column)`;
  - `MAX(column)`;
  - `SUM(column)`;
  - narrow `SUM(column + column)` where both operands are integer descriptor
    columns;
  - `AVG(column)`;
  - `BIT_AND(column)`;
  - `BIT_OR(column)`;
  - `BIT_XOR(column)`;
- aggregate arguments resolved from MyLite descriptors, with existing source
  qualification rules for the current one-source and two-source grouped source
  envelope;
- grouped `MIN`, `MAX`, `SUM`, `AVG`, and bitwise arguments limited to integer
  descriptor columns, matching the existing single-aggregate grouped slice;
- optional aliases on the grouped column and each aggregate result;
- optional `WHERE` using the existing grouped source predicate subset;
- optional `HAVING` on the grouped column, grouped-column alias, selected
  non-bitwise aggregate expression, or selected non-bitwise aggregate alias,
  using the existing one-predicate `HAVING` comparison/`IS NULL` subset;
- optional `ORDER BY` on the grouped column, grouped-column alias, or one
  selected `COUNT`, `MIN`, `MAX`, `SUM`, `AVG`, or bitwise aggregate alias;
- optional `ASC` / `DESC`; default direction is ascending;
- optional `LIMIT` and `OFFSET` using the existing select limit subset;
- `SELECT ALL` and no-op select modifiers keep their existing admission rules;
- successful statements produce a normal row result and do not mutate catalog
  descriptors, descriptor caches, catalog generation, `sqlite_schema_generation`,
  physical schema text, or the `.mylite` preamble.

The selected aggregate count limit is an internal guard for allocation and test
scope, not a MySQL compatibility claim. It is high enough for the baseline
application queries driving this slice and can be raised when broader grouped
projection coverage is implemented.

## Deferred Surface

This slice intentionally does not add:

- multiple group keys;
- grouping by alias, ordinal, string literal, expression, parenthesized
  expression, aggregate result, or function result;
- aggregate-only grouped projection without projecting the grouped column;
- full `COUNT(DISTINCT expr[, expr...])` support beyond the integer
  descriptor-column grouped slice;
- grouped `SUM(expr)` beyond the exact two-column integer addition shape;
- grouped `GROUP_CONCAT()` mixed with other aggregate results;
- `GROUP_CONCAT(DISTINCT ...)`, multiple `GROUP_CONCAT()` expressions, or wider
  joined-source grouped `GROUP_CONCAT()`;
- multiple `ORDER BY` keys;
- ordering by aggregate expression, ordinal, string literal, unselected alias,
  unaliased aggregate output label, or arbitrary expression;
- selected `GROUP_CONCAT()` alias ordering;
- `HAVING` boolean composition, unselected aggregate predicates, bitwise
  aggregate predicates, `GROUP_CONCAT()` predicates, arbitrary expressions,
  parameters, subqueries, or general alias ambiguity handling;
- string, binary, decimal, approximate numeric, enum, set, JSON, or temporal
  grouping or aggregate argument semantics beyond already-supported descriptors;
- `WITH ROLLUP`, window functions, derived tables, CTEs, subqueries, privilege
  semantics, protocol-grade metadata, or optimizer parity.

## Grammar

The parser already accepts the select-list and clause skeleton needed for this
feature. The independently authored MyLite subset is:

```lemon
select_statement ::= SELECT select_modifier_opt select_item_list
    FROM table_reference where_clause_opt group_clause having_clause_opt
    order_clause_opt limit_clause_opt locking_clause_opt.

select_item_list ::= select_item.
select_item_list ::= select_item_list COMMA select_item.

select_item ::= qualified_identifier alias_opt.
select_item ::= grouped_aggregate alias_opt.

group_clause ::= GROUP BY qualified_identifier.
order_clause ::= ORDER BY qualified_identifier order_direction_opt.

grouped_aggregate ::= COUNT LPAREN STAR RPAREN.
grouped_aggregate ::= COUNT LPAREN qualified_identifier RPAREN.
grouped_aggregate ::= COUNT LPAREN DISTINCT qualified_identifier RPAREN.
grouped_aggregate ::= COUNT LPAREN DISTINCT LPAREN qualified_identifier RPAREN RPAREN.
grouped_aggregate ::= MIN LPAREN qualified_identifier RPAREN.
grouped_aggregate ::= MAX LPAREN qualified_identifier RPAREN.
grouped_aggregate ::= SUM LPAREN sum_aggregate_argument RPAREN.
grouped_aggregate ::= AVG LPAREN qualified_identifier RPAREN.
grouped_aggregate ::= BIT_AND LPAREN qualified_identifier RPAREN.
grouped_aggregate ::= BIT_OR LPAREN qualified_identifier RPAREN.
grouped_aggregate ::= BIT_XOR LPAREN qualified_identifier RPAREN.
sum_aggregate_argument ::= qualified_identifier.
sum_aggregate_argument ::= qualified_identifier PLUS qualified_identifier.
```

Runtime validation requires exactly one descriptor group-column select item in
position one, followed by aggregate select items. The grammar remains broader
where existing parser support is broader; unsupported forms are rejected by the
planner with deterministic diagnostics.

## Name Resolution

Source table resolution, selected/default schema behavior, aliases, temporary
table shadowing, reserved `_mylite_*` names, and unknown schema/table
diagnostics reuse the existing grouped aggregate source planner.

The grouped column, aggregate arguments, `WHERE` predicate columns, `HAVING`
operands, and grouped-column `ORDER BY` keys are resolved from MyLite catalog
descriptors, not SQLite metadata. Descriptor lookup keeps current case
sensitivity and collation behavior. The optional aggregate-alias `ORDER BY` key
matches one selected `COUNT`, `MIN`, `MAX`, `SUM`, `AVG`, or bitwise aggregate alias
using the existing ASCII case-insensitive alias comparison helper. Duplicate
selected aggregate aliases are unsupported for aggregate-alias ordering in this
slice because MySQL alias ambiguity rules are broader than the current planner.

## Runtime Semantics

MyLite builds one SQLite statement with this physical shape:

```sql
SELECT "group_physical_column",
       AGGREGATE_1(...),
       AGGREGATE_2(...),
       ...
FROM "physical_table_or_join"
[WHERE ...]
GROUP BY "group_physical_column"
[HAVING ...]
[ORDER BY "group_physical_column" | COUNT/MIN/MAX/SUM_N(...) |
          _mylite_avg_order_key(SUM(...), COUNT(...)) |
          _mylite_uint64_decimal_order_key(_mylite_bit_or(...))]
[LIMIT ? [OFFSET ?]]
```

For `AVG(column)`, MyLite selects `SUM(column), COUNT(column)` internally and
formats the public result value with the existing grouped-average formatter.
This means an `AVG` select item consumes two SQLite result columns but produces
one public result cell. For the narrow `SUM(column + column)` form, MyLite
emits a row-scalar integer addition expression as the `SUM()` argument. Bitwise
aggregates call MyLite's registered SQLite aggregate callbacks and return
unsigned decimal text; bitwise aggregate-alias ordering wraps that decimal text
in an internal fixed-width unsigned order-key scalar so SQLite ordering matches
MySQL's unsigned numeric order. `AVG` aggregate-alias ordering wraps the
generated `SUM()` and `COUNT()` components in an internal exact signed-rational
order-key scalar so large integer averages do not sort through SQLite `REAL`.

All generated identifiers are quoted. Predicate, separator, `HAVING`, and
limit values are bound parameters. This feature adds no SQLite fork patch:
public SQLite aggregate execution and MyLite-side translation are sufficient.

Result labels follow existing select-item rules:

- grouped column label is its alias when present, otherwise the descriptor
  column name;
- aggregate label is its alias when present, otherwise the original aggregate
  expression text with existing aggregate-label spacing behavior.

Successful queries return one public result row per group after `HAVING`,
`ORDER BY`, and `LIMIT`. Public `affected_rows`/`ROW_COUNT()` remains `-1`;
`warning_count` remains zero for supported in-range statements.

## Diagnostics

Required diagnostics include:

- syntax errors: existing parser syntax diagnostics;
- missing/default schema, unknown schema, unknown table, reserved schema/table
  names, and unsupported object kind: existing grouped source diagnostics;
- select-list shape outside `group_column, aggregate...`:
  `GROUP BY supports one grouped descriptor column and one or more aggregate results`;
- too many aggregate select items:
  `GROUP BY supports at most sixteen aggregate results`;
- selected nonaggregate descriptor column that differs from the `GROUP BY`
  descriptor column: existing `ONLY_FULL_GROUP_BY`-style `1055 / 42000`;
- unknown grouped, aggregate, predicate, order, or having columns: existing
  MySQL-compatible column diagnostics for the matching clause;
- unsupported aggregate forms beyond the documented grouped aggregate subset
  and grouped `GROUP_CONCAT()` mixed with other aggregate results:
  deterministic unsupported diagnostics;
- unsupported grouped `SUM(column + column)` operand:
  `SUM(column + column) supports only integer descriptor columns`;
- unsupported noninteger aggregate argument for current numeric aggregates:
  existing aggregate integer-column diagnostics;
- unsupported `HAVING` predicate shape, unselected aggregate predicate, bitwise
  aggregate predicate, `GROUP_CONCAT()` predicate, or out-of-range literal:
  existing grouped `HAVING` diagnostics, extended to selected aggregate items;
- unsupported `ORDER BY` key, duplicate aggregate alias order key, selected
  `GROUP_CONCAT()` alias order key, aggregate expression order key, multiple
  order keys, ordinal, string literal, or
  expression: existing parser or planner unsupported diagnostics;
- physical SQLite failures: existing internal SQLite row-operation diagnostic;
- allocation failure: existing `MYLITE_NOMEM` diagnostic behavior;
- public API misuse: no public API changes.

## Tests

Extend the fast C grouped aggregate runtime test and add a focused MySQL
expectation script covering:

- `COUNT(*)`, `COUNT(column)`, `MIN`, `MAX`, `SUM`, `AVG`, `BIT_AND`, `BIT_OR`,
  and `BIT_XOR` in one grouped projection;
- existing signed/unsigned integer descriptor families;
- nullable group keys, nullable aggregate arguments, and all-`NULL` aggregate
  argument groups;
- grouped `SELECT DISTINCT` over supported grouped projection and aggregate
  items;
- `WHERE` filtering before grouping;
- `HAVING` on selected aggregate aliases and expressions in a multi-aggregate
  select;
- `ORDER BY` grouped column and selected `COUNT`, `MIN`, `MAX`, `SUM`, `AVG`,
  and bitwise aggregate aliases, including default, `ASC`, `DESC`, `NULL`
  aggregate ordering, exact large-integer `AVG` ordering, and `LIMIT`;
- result labels, warning count, row count, no catalog generation changes, no
  SQLite schema generation changes, file preamble preservation, reopen
  persistence, rename/drop behavior, independent file-backed handles, and
  zero-initialized cleanup;
- deterministic rejections for nonaggregate extra select items, arbitrary
  `COUNT(DISTINCT ...)` forms outside the integer descriptor-column slice,
  grouped `GROUP_CONCAT()` mixed with another aggregate, unknown
  aggregate/order/having columns, unselected aggregate `HAVING`, bitwise
  aggregate `HAVING`, aggregate expression ordering, duplicate aggregate alias
  ordering, multiple order keys, and unsupported expressions.
