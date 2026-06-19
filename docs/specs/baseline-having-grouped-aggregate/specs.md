# Baseline HAVING Grouped Aggregate Specification

## Status

This phase adds the next narrow query-baseline slice: a descriptor-driven
`HAVING` clause for the existing single-column grouped aggregate `SELECT`
subset. It is intentionally limited to one persistent base table, one grouped
descriptor integer column, and one selected aggregate result.

The design is based on independently authored MyLite behavior, official MySQL
8.4 documentation, and observed MySQL 8.4.9 runtime probes. The relevant MySQL
documentation is:

- https://dev.mysql.com/doc/refman/8.4/en/select.html
- https://dev.mysql.com/doc/refman/8.4/en/group-by-handling.html
- https://dev.mysql.com/doc/refman/8.4/en/aggregate-functions-and-modifiers.html

The MySQL 8.4.9 expectation script for this phase is:

- `packages/libmylite/tests/mysql_baseline_having_grouped_aggregate_expectations.sh`

## Scope

Supported statement shape:

```sql
SELECT group_column [AS alias], aggregate_result [AS alias]
FROM table_name [[AS] table_alias]
[WHERE baseline_where_predicate]
GROUP BY group_column
[HAVING having_predicate]
[ORDER BY group_column_or_group_alias [ASC | DESC]]
[LIMIT select_limit_form]
```

The supported source, selected group column, selected aggregate result, `WHERE`,
`ORDER BY`, and `LIMIT` behavior is inherited from
`baseline-group-by-single-column-aggregate`.

`HAVING` is admitted only on the grouped aggregate path. MyLite may parse a
`HAVING` clause on other table-backed `SELECT` shapes so it can reject them
deterministically, but execution support is limited to the grouped aggregate
shape above.

Supported `HAVING` predicates are one predicate atom, optionally wrapped in
parentheses:

```sql
having_operand comparison_operator having_integer_value
having_operand IS NULL
having_operand IS NOT NULL
```

`comparison_operator` is one of:

```sql
=  <=>  <>  !=  <  <=  >  >=
```

`having_integer_value` is an unsigned decimal integer literal in MyLite's
current signed-64 expression envelope, optionally prefixed by unary `+` or
`-`, or a `TRUE` / `FALSE` boolean literal. Boolean literals convert to `1`
and `0`.

Supported `having_operand` forms:

- The selected grouped descriptor column, using the same supported column
  qualifier forms as the grouped aggregate selected column.
- The selected grouped descriptor column's unqualified select-item alias.
- The selected aggregate result's unqualified select-item alias.
- The selected aggregate function expression itself, for `COUNT(*)`,
  `COUNT(column)`, `COUNT(DISTINCT column)`, `MIN(column)`, `MAX(column)`,
  `SUM(column)`, and `AVG(column)`.

The aggregate function expression in `HAVING` must refer to the selected
aggregate result. MyLite does not yet support aggregate expressions that are
only present in `HAVING`.

`BIT_AND()`, `BIT_OR()`, and `BIT_XOR()` remain valid selected grouped
aggregate results. Their result predicates in `HAVING` are deferred for this
slice because the current MyLite bitwise aggregate callback returns
unsigned-64 decimal text and needs a separate comparison contract for
unsigned values above the signed-64 physical storage range. Group-column
`HAVING` predicates may still be used with a selected bitwise aggregate.

## Out Of Scope

This phase does not add:

- `HAVING` without `GROUP BY`;
- `HAVING` on non-aggregate table selects or scalar selects;
- boolean composition such as `AND`, `OR`, `XOR`, or `NOT`;
- bare truth tests such as `HAVING COUNT(*)`;
- literal-left predicates;
- `NULL` comparison literals such as `HAVING SUM(col) <=> NULL`;
- string, decimal, float, hex, bit, parameter, subquery, or function literals;
- arithmetic or arbitrary expressions in `HAVING`;
- arbitrary `COUNT(DISTINCT ...)` expressions outside the selected
  integer-descriptor grouped count-distinct slice;
- aggregate expressions in `HAVING` that differ from the selected aggregate;
- bitwise aggregate result predicates;
- multiple grouping keys, grouping aliases in the `GROUP BY` clause, ordinals,
  rollup, `GROUPING()`, functional-dependence inference, joins, windows, CTEs,
  derived tables, collations, or general MySQL expression evaluation.

## MySQL Behavior Verified

Runtime probes against MySQL 8.4.9 verify the following visible behavior for
the admitted subset:

- `HAVING` is evaluated after `WHERE` filtering and grouping.
- `ORDER BY` and `LIMIT` apply after `HAVING`.
- `HAVING` can refer to the selected aggregate result by expression or
  unqualified alias.
- `HAVING` can refer to the selected group column and its unqualified alias.
- `HAVING` returns only groups where the predicate evaluates to true; false
  and `NULL` predicate results discard the group.
- `IS NULL` and `IS NOT NULL` behave normally over nullable grouped columns and
  nullable aggregate results such as `SUM()` over all-`NULL` groups.
- Unknown `HAVING` names use MySQL error 1054 / SQLSTATE `42S22` with
  `Unknown column '<name>' in 'having clause'`.
- A source-table column that is not the grouped column and is not selected as a
  valid `HAVING` alias is not admitted by this slice; MySQL reports it as an
  unknown `HAVING` column in the probed grouped aggregate shapes.

MySQL accepts broader forms, including aggregate expressions that appear only
in `HAVING`, expression predicates such as `COUNT(*) + 1 > 2`, arbitrary
`COUNT(DISTINCT ...)` expressions, bitwise aggregate predicates, and
non-grouped aggregate `HAVING`. MyLite intentionally defers those forms until
the expression and aggregate planner can own them explicitly.

## Name Resolution

The table target follows the existing selected/default schema policy for
table-backed `SELECT`:

- unqualified table names use the selected schema;
- schema-qualified table names use the explicit schema;
- missing default schema, unknown schema, unknown table, and reserved
  `_mylite_*` names use the existing grouped aggregate diagnostics.

Column and alias resolution for `HAVING` is deliberately smaller than MySQL's
general resolver:

1. A qualified column reference must resolve to the selected grouped descriptor
   column through the current single-source table or table alias.
2. An unqualified identifier first resolves to the selected grouped descriptor
   column when it matches the descriptor column name. This descriptor-column
   match takes precedence over selected aliases.
3. If it does not resolve to the grouped descriptor column, it may resolve to
   the selected aggregate alias, then the selected group column alias. When the
   selected group and aggregate aliases are identical, the aggregate alias wins,
   matching observed MySQL 8.4.9 behavior for this slice.
4. If the identifier matches a real source descriptor column that is not the
   selected grouped column, MyLite reports it as an unknown `HAVING` column for
   this slice, matching the observed MySQL 8.4.9 behavior for the probed
   default `ONLY_FULL_GROUP_BY` grouped aggregate shapes.
5. Ambiguous alias-only cases that cannot be resolved with the rules above are
   rejected with a deterministic MyLite unsupported diagnostic.

Descriptor lookup uses the current catalog case-insensitive identifier
comparison used by the existing baseline table and column resolution paths.
MyLite descriptors remain authoritative; SQLite schema text is not consulted
for MySQL-visible names.

## Value Conversion

Group-column `HAVING` comparisons reuse the existing descriptor-owned integer
predicate conversion:

- signed integer descriptor columns admit values in their signed type range;
- unsigned descriptor columns admit nonnegative values in their unsigned type
  range, limited by MyLite's current signed-64 physical storage envelope where
  applicable;
- `TRUE` and `FALSE` convert to `1` and `0`;
- out-of-range values produce a deterministic MyLite range diagnostic.

Aggregate-result `HAVING` comparisons use MyLite's signed-64 expression
envelope for this slice. This covers the currently supported physical integer
storage range and the tested grouped aggregate results. Larger MySQL numeric
expression values are deferred.

`AVG(column)` result predicates compare the SQLite numeric aggregate result
against the admitted integer literal while the public selected result keeps
the existing MyLite four-fractional-digit formatting.

`NULL` is not admitted as a comparison literal in this phase. Use `IS NULL` or
`IS NOT NULL` for supported null tests.

## Execution And SQLite Handling

The implementation remains a MyLite wrapper/translation layer over public
SQLite APIs. It does not require SQLite fork patches.

The grouped aggregate planner resolves all MySQL-visible names against MyLite
catalog descriptors, then emits SQLite SQL using stable physical table and
column names:

```sql
SELECT "group_column", AGG("aggregate_column")
FROM "_mylite_user_table_<table_id>"
[WHERE "predicate_column" <op> ?]
GROUP BY "group_column"
[HAVING <descriptor_or_aggregate_expression> <op> ?]
[ORDER BY "group_column" ASC|DESC]
[LIMIT ? [OFFSET ?]]
```

`AVG()` selected results continue to use the existing internal `SUM()` plus
`COUNT()` projection so MyLite can preserve its result formatting. A `HAVING
AVG(column) ...` predicate emits a SQLite `AVG("column")` expression only for
the filter predicate.

Every generated SQLite identifier is quoted. Predicate, `HAVING`, `LIMIT`, and
`OFFSET` values are bound parameters. MyLite never interpolates user literals
into generated SQLite SQL.

SQLite performs the source scan, `WHERE` filtering, grouping, aggregate
calculation, `HAVING` filtering, sorting, and limiting. MyLite materializes
only the final public `mylite_result` rows.

Successful `HAVING` selects:

- return through the existing row-result API;
- set `ROW_COUNT()` to `-1`, like other supported `SELECT` statements;
- report warning count `0` for supported in-range forms;
- do not mutate catalog rows, descriptor versions, catalog generation, or
  `sqlite_schema_generation`;
- preserve the `.mylite` preamble and shifted SQLite payload invariants.

## Parser Snippet

The MyLite Lemon grammar is independently authored and intentionally narrower
than MySQL's grammar:

```lemon
select_statement(A) ::=
    SELECT(T) select_item_list(B) FROM(F) table_name(N) table_alias_opt(AL)
    where_clause_opt(W) group_clause_opt(G) having_clause_opt(H)
    order_clause_opt(O) limit_clause_opt(L).

having_clause_opt(A) ::= .
having_clause_opt(A) ::= HAVING(H) having_predicate(P).

having_predicate(A) ::= having_predicate_atom(B).
having_predicate(A) ::= LPAREN(L) having_predicate(B) RPAREN(R).

having_predicate_atom(A) ::= having_operand(C) EQUAL(O) having_integer_value(V).
having_predicate_atom(A) ::= having_operand(C) NULL_SAFE_EQUAL(O) having_integer_value(V).
having_predicate_atom(A) ::= having_operand(C) NOT_EQUAL(O) having_integer_value(V).
having_predicate_atom(A) ::= having_operand(C) LESS(O) having_integer_value(V).
having_predicate_atom(A) ::= having_operand(C) LESS_EQUAL(O) having_integer_value(V).
having_predicate_atom(A) ::= having_operand(C) GREATER(O) having_integer_value(V).
having_predicate_atom(A) ::= having_operand(C) GREATER_EQUAL(O) having_integer_value(V).
having_predicate_atom(A) ::= having_operand(C) IS(I) NULL(N).
having_predicate_atom(A) ::= having_operand(C) IS(I) NOT NULL(N).

having_operand(A) ::= qualified_identifier(B).
having_operand(A) ::= selected_grouped_aggregate_expression(B).

selected_grouped_aggregate_expression(A) ::= COUNT(T) LPAREN(L) STAR RPAREN(R).
selected_grouped_aggregate_expression(A) ::= COUNT(T) LPAREN(L) qualified_identifier(B) RPAREN(R).
selected_grouped_aggregate_expression(A) ::= COUNT(T) LPAREN(L) DISTINCT qualified_identifier(B) RPAREN(R).
selected_grouped_aggregate_expression(A) ::= COUNT(T) LPAREN(L) DISTINCT LPAREN qualified_identifier(B) RPAREN RPAREN(R).
selected_grouped_aggregate_expression(A) ::= MIN(T) LPAREN(L) qualified_identifier(B) RPAREN(R).
selected_grouped_aggregate_expression(A) ::= MAX(T) LPAREN(L) qualified_identifier(B) RPAREN(R).
selected_grouped_aggregate_expression(A) ::= SUM(T) LPAREN(L) qualified_identifier(B) RPAREN(R).
selected_grouped_aggregate_expression(A) ::= AVG(T) LPAREN(L) qualified_identifier(B) RPAREN(R).

having_integer_value(A) ::= INTEGER(T).
having_integer_value(A) ::= PLUS(P) INTEGER(T).
having_integer_value(A) ::= MINUS(M) INTEGER(T).
having_integer_value(A) ::= TRUE(T).
having_integer_value(A) ::= FALSE(T).
```

## Diagnostics

Expected diagnostics for this phase:

- Syntax not admitted by the grammar: existing parse error 1064 / `42000`.
- `HAVING` on unsupported `SELECT` shapes: MyLite unsupported diagnostic.
- Unknown `HAVING` identifier: 1054 / `42S22`, `Unknown column '<name>' in
  'having clause'`.
- Source descriptor column that is not the selected group column: same unknown
  `HAVING` column diagnostic for this slice.
- Unsupported `HAVING` operand: MyLite unsupported diagnostic.
- Unsupported aggregate result predicate, including bitwise result predicates:
  MyLite unsupported diagnostic.
- Aggregate expression in `HAVING` that does not match the selected aggregate:
  MyLite unsupported diagnostic.
- Unsupported right-hand literal or out-of-range integer literal: deterministic
  MyLite diagnostic.
- Physical SQLite failures: existing physical row/aggregate failure mapping.
- Allocation failures: existing `MYLITE_NOMEM` handling.

## Tests

Runtime tests must cover:

- successful aggregate-alias and aggregate-expression predicates for
  `COUNT(*)`, `COUNT(column)`, `MIN`, `MAX`, `SUM`, and `AVG`;
- group-column predicates, including `IS NULL`, `<=>`, comparisons, and aliases;
- `WHERE` before grouping combined with `HAVING`;
- `ORDER BY` and `LIMIT` after `HAVING`, including `LIMIT 0`;
- nullable groups and all-`NULL` aggregate results;
- schema-qualified table names and table aliases;
- `ROW_COUNT() == -1`, warning count `0`, result column names, and result rows;
- unknown `HAVING` columns, non-group source columns, unsupported aggregate-only
  `HAVING` expressions, unsupported bitwise aggregate result predicates, and
  unsupported expression predicates;
- reopen persistence and independence of file-backed handles;
- generation stability and `.mylite` preamble preservation;
- existing parser, grouped aggregate, aggregate, select, DML, catalog, storage,
  and full workflow regressions.
