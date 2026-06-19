# Baseline GROUP BY Single-Column Aggregate

## Status

This feature specifies a narrow `GROUP BY` slice for descriptor-backed
single-table aggregate queries. It builds on `mylite_execute()`, statement
context, the parser scaffold, durable catalog descriptors, integer/`NULL` row
values, descriptor-driven `SELECT ... WHERE`, descriptor-driven
`SELECT ... ORDER BY ... LIMIT`, and the existing one-item aggregate paths for
`COUNT`, `MIN`, `MAX`, `SUM`, `AVG`, `BIT_AND`, `BIT_OR`, and `BIT_XOR`.

This is not full MySQL grouping support. It admits one nonaggregate descriptor
column and one aggregate result over one persistent base table, with optional
baseline `WHERE`, optional ordering by the grouped descriptor column, and the
existing SELECT `LIMIT`/`OFFSET` subset. It does not add `HAVING`, rollup,
multiple grouping keys, grouping expressions, functional-dependence analysis,
arbitrary expression projection, joins, subqueries, or MyLite-side
materialization of source rows.

## Sources

- MyLite README architecture: `README.md`
- MyLite engineering standards:
  `docs/architecture/engineering-standards.md`
- Baseline implementation strategy:
  `docs/specs/baseline-implementation-strategy/specs.md`
- Runtime handles and statement context:
  `docs/specs/runtime-handles-statement-context/specs.md`
- Baseline catalog foundation:
  `docs/specs/baseline-catalog-foundation/specs.md`
- Baseline row values:
  `docs/specs/baseline-row-values-lifecycle/specs.md`
- Baseline select where lifecycle:
  `docs/specs/baseline-select-where-lifecycle/specs.md`
- Baseline select order/limit lifecycle:
  `docs/specs/baseline-select-order-limit-lifecycle/specs.md`
- Baseline aggregate specs under `docs/specs/`
- MySQL lexer:
  `docs/specs/mysql-lexer/specs.md`
- MySQL parser scaffold:
  `docs/specs/mysql-parser-scaffold/specs.md`
- SQLite source snapshot notes: `third_party/sqlite/README.md`
- MySQL 8.4 Reference Manual, `SELECT` syntax:
  https://dev.mysql.com/doc/refman/8.4/en/select.html
- MySQL 8.4 Reference Manual, aggregate functions:
  https://dev.mysql.com/doc/refman/8.4/en/aggregate-functions.html
- MySQL 8.4 Reference Manual, MySQL handling of `GROUP BY`:
  https://dev.mysql.com/doc/refman/8.4/en/group-by-handling.html

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior,
public SQLite APIs, and existing MyLite source code. It does not copy MySQL,
MariaDB, Percona, SQLite implementation internals, or other restrictively
licensed implementation sources.

## MySQL 8.4.9 Runtime Observations

The expectation script
`packages/libmylite/tests/mysql_baseline_group_by_single_column_aggregate_expectations.sh`
records the runtime probes for this feature. Observed behavior:

- `GROUP BY` groups rows by the grouping expression after `WHERE` filtering
  and before result ordering and limiting.
- With `ONLY_FULL_GROUP_BY` enabled by default, nonaggregated selected columns
  must be grouped or functionally dependent on grouped columns. This slice does
  not implement functional-dependence analysis; it admits only the grouped
  descriptor column plus one aggregate result.
- `NULL` group keys form a single group. In ordered output, `NULL` sorts first
  for ascending order and last for descending order.
- Without `ORDER BY`, MySQL returned grouped rows in the probed key order, but
  this slice does not promise output order without an explicit supported
  `ORDER BY`.
- `COUNT(*)` counts all rows in each group. `COUNT(column)` ignores `NULL`
  argument values. `MIN`, `MAX`, `SUM`, `AVG`, and the bitwise aggregates keep
  their existing one-item aggregate semantics independently for each group.
- Groups where every aggregate argument is `NULL` return `0` for
  `COUNT(column)`, `NULL` for `MIN`, `MAX`, `SUM`, and `AVG`,
  `18446744073709551615` for `BIT_AND`, and `0` for `BIT_OR` and
  `BIT_XOR`.
- A `WHERE` predicate that matches no rows returns no grouped result rows.
- `ORDER BY grouped_column`, `ORDER BY grouped_alias`, `ASC`, `DESC`,
  `LIMIT row_count`, `LIMIT row_count OFFSET offset`, and
  `LIMIT offset, row_count` are valid MySQL syntax for grouped selects.
- `LIMIT 0` returns no result rows. Supported limit literals inherit the
  existing SELECT limit literal policy.
- Unknown selected columns fail with error `1054`, SQLSTATE `42S22`, using
  the `"field list"` context. Unknown grouping columns use the
  `"group statement"` context. Unknown ordering columns use the
  `"order clause"` context.
- A successful grouped select makes the following `ROW_COUNT()` return `-1`
  and leaves warning count `0`.

## Scope

The implementation must add:

- parser and AST support for one optional `GROUP BY qualified_identifier`
  clause in descriptor-backed table `SELECT` statements;
- `GROUP` as a parser keyword while retaining existing keyword-as-identifier
  behavior where the grammar already admits keyword identifiers;
- descriptor-driven
  `SELECT group_column [AS alias], aggregate [AS alias] FROM table_name
  [WHERE predicate] GROUP BY group_column [ORDER BY group_column [ASC|DESC]]
  [LIMIT ...]`;
- aggregate forms:
  - `COUNT(*)`;
  - `COUNT(column)`;
  - `COUNT(DISTINCT column)` for integer descriptor columns;
  - `MIN(column)`, `MAX(column)`, `SUM(column)`, `AVG(column)`;
  - `BIT_AND(column)`, `BIT_OR(column)`, `BIT_XOR(column)`;
- optional source table aliases matching the existing single-table SELECT
  source policy;
- unqualified and schema-qualified table-name resolution using the existing
  selected/default schema policy;
- one persistent MyLite base-table descriptor source only;
- grouping-column and aggregate-argument resolution from MyLite descriptors,
  including supported source-qualified descriptor references;
- explicit access to invisible descriptor columns, matching current explicit
  projection and aggregate behavior;
- reuse of the existing baseline `WHERE` predicate subset and conversion
  rules;
- optional ordering by the grouped descriptor column or its select-item alias;
- reuse of the existing SELECT `LIMIT`/`OFFSET` row-count and offset subset;
- generated SQLite physical SQL built only from descriptors and stable
  physical table names;
- prepared-statement binding for predicate and limit values;
- SQLite-owned source scanning, filtering, grouping, ordering, and limiting;
- MyLite-owned formatting for `AVG` and bitwise aggregate result values, using
  the existing aggregate result policies per group;
- MySQL-compatible result column labels for the grouped descriptor column and
  aggregate expression, including aliases;
- deterministic diagnostics for unsupported grouping syntax and wider MySQL
  forms;
- tests and a MySQL 8.4.9 expectation artifact for supported behavior and
  deliberately rejected wider forms.

Existing non-grouped SELECT and aggregate behavior must remain unchanged.

## Non-Goals

This feature must not implement:

- full `COUNT(DISTINCT expr[, expr...])` support beyond the separately
  specified integer descriptor-column grouped slice;
- literal or expression aggregate arguments in grouped queries;
- one-column grouped projection without an aggregate;
- aggregate-only grouped projection with a hidden grouping key;
- multiple selected aggregates or more than two select items;
- multiple `GROUP BY` keys, grouping by ordinal, alias, string literal,
  expression, parenthesized expression, aggregate result, or function result;
- `HAVING`, `WITH ROLLUP`, `ROLLUP(...)`, grouping sets, `GROUPING()`, window
  functions, joins, CTEs, subqueries, unions, locking clauses, query
  modifiers, optimizer hints, `INTO`, or arbitrary SQLite SQL pass-through;
- `ORDER BY` aggregate aliases, aggregate expressions, ordinals, multiple sort
  keys, or non-grouped descriptor columns in grouped queries;
- functional-dependence detection for `ONLY_FULL_GROUP_BY`;
- string, decimal, floating, temporal, JSON, enum, set, collation, or charset
  grouping and aggregate expression semantics;
- aggregate metadata parity, protocol column flags, exact optimizer behavior,
  temporary tables, views, privileges, SQL mode variants, indexes, or SQLite
  fork patches.

## Ownership Boundary

- The public API remains unchanged. `mylite_execute()` owns public validation,
  result-handle ownership, statement-boundary row-count state, and failure
  cleanup.
- Statement context owns diagnostics reset, warning count, and statement
  completion. Successful grouped selects are result-set statements and
  therefore store `-1` as the connection-local previous row count.
- Lexer/parser/AST own syntax admission, the `GROUP BY` clause node, keyword
  mapping, and source spans. They remain independent of runtime, catalog,
  storage, and SQLite.
- Analyzer/planner code recognizes the two-item grouped aggregate shape,
  resolves the source table, group column, aggregate argument column, optional
  predicate, optional order key, and optional limit descriptors, rejects
  unsupported shapes, and builds a descriptor-driven grouped aggregate plan.
- The catalog module remains authoritative for schema/table/column
  descriptors. Grouped aggregate queries read descriptors but do not mutate
  catalog rows, descriptor versions, descriptor caches, catalog generation, or
  `sqlite_schema_generation`.
- Runtime execution generates SQLite SQL against the descriptor-owned physical
  table and binds only predicate/limit parameters. SQLite owns scanning,
  filtering, grouping, ordering, and limiting. Existing MyLite aggregate
  callbacks own only fixed-size per-group state for bitwise aggregates.
- The result builder owns the final public result rows. MyLite may materialize
  output groups in the public result object, but it must not materialize
  source rows to implement grouping.
- Storage/VFS owns the `.mylite` preamble and shifted SQLite payload boundary.
  Grouped selects do not touch byte range `[0, 4096)`.

## Supported SQL Grammar

Supported subset:

```sql
SELECT group_column [AS alias], aggregate [AS alias]
FROM table_name [AS alias]
[WHERE predicate]
GROUP BY group_column
[ORDER BY group_column [ASC|DESC]]
[LIMIT row_count]

SELECT group_column [AS alias], aggregate [AS alias]
FROM table_name [AS alias]
[WHERE predicate]
GROUP BY group_column
[ORDER BY group_column [ASC|DESC]]
[LIMIT row_count OFFSET offset]

SELECT group_column [AS alias], aggregate [AS alias]
FROM table_name [AS alias]
[WHERE predicate]
GROUP BY group_column
[ORDER BY group_column [ASC|DESC]]
[LIMIT offset, row_count]
```

`table_name` uses the existing table lifecycle subset:

```sql
table_name:
    identifier
  | identifier.identifier
```

The grouped column, aggregate argument, predicate column, and order column use
the existing descriptor reference subset where applicable:

```sql
column_reference:
    column_name
  | table_name.column_name
  | schema_name.table_name.column_name
```

Supported aggregate forms:

```sql
aggregate:
    COUNT(*)
  | COUNT(column_reference)
  | COUNT(DISTINCT column_reference)
  | MIN(column_reference)
  | MAX(column_reference)
  | SUM(column_reference)
  | AVG(column_reference)
  | BIT_AND(column_reference)
  | BIT_OR(column_reference)
  | BIT_XOR(column_reference)
```

The supported predicate and limit subsets are exactly the existing SELECT
baseline subsets. The supported order subset is one grouped descriptor column
or its select-item alias.

MyLite Lemon-syntax snippets:

```lemon
select_statement(A) ::=
    SELECT(T) select_item_list(B) FROM(F) table_name(N) table_alias_opt(AL)
    where_clause_opt(W) group_clause_opt(G) order_clause_opt(O) limit_clause_opt(L). {
    A = mylite_sql_parser_make_select_statement(
        state, T, B, mylite_sql_parser_make_from_table(state, F, N, AL), W, G, O, L);
}

group_clause_opt(A) ::= . {
    A = NULL;
}
group_clause_opt(A) ::= GROUP(G) BY qualified_identifier(K). {
    A = mylite_sql_parser_make_group_by_clause(state, G, K);
}
```

## Name Resolution

Unqualified table names use the selected/default schema. If no default schema
is selected, MyLite must return the existing MySQL-compatible no-database
diagnostic. Schema-qualified target names resolve the explicit schema and do
not consult the default schema for the table name.

Reserved `_mylite_*` schema or table names are rejected before SQLite SQL is
generated. Unknown schemas and unknown tables use the existing table-read
diagnostics.

The grouped selected column is resolved first using field-list diagnostics.
The `GROUP BY` column is then resolved using group-statement diagnostics. The
aggregate argument uses field-list diagnostics. The optional `WHERE` predicate
uses where-clause diagnostics. The optional `ORDER BY` key uses order-clause
diagnostics unless it resolves to the grouped select-item alias.

All descriptor name matching follows the current catalog identifier policy:
ASCII case-insensitive matching for identifiers and no collation-aware name
resolution. The grouped selected descriptor column and `GROUP BY` descriptor
column must resolve to the same catalog column. MyLite does not infer
functional dependencies.

## Semantics

Execution order is:

1. resolve descriptors and supported syntax;
2. bind predicate values;
3. let SQLite scan the physical table and apply `WHERE`;
4. let SQLite group by the physical grouped column;
5. let SQLite evaluate built-in or registered aggregate functions per group;
6. let SQLite apply the supported `ORDER BY` and `LIMIT`/`OFFSET`;
7. copy final grouped rows into `mylite_result`.

`NULL` group keys form one group. `ORDER BY` defaults to ascending order.
Ascending order places `NULL` before non-`NULL` values, and descending order
places `NULL` after non-`NULL` values. Duplicate group values are impossible
because the grouping key is the group identity. Ties outside the admitted
single grouping key are not specified.

When `ORDER BY` is omitted, MyLite does not promise result row order. Tests
must order grouped results when row order matters.

`LIMIT` applies after grouping and ordering. `LIMIT 0` returns no rows.
`LIMIT` offset forms inherit existing SELECT semantics. Unsupported signed,
negative, out-of-range, non-integer, parameter, expression, function, string,
decimal, float, hex, bit, `NULL`, `TRUE`, and `FALSE` limit forms remain
outside this slice.

Aggregate result semantics are inherited from the existing non-grouped
aggregate slices, evaluated per group. `AVG` keeps the current four
fractional digit formatting and signed-64 intermediate sum envelope. Bitwise
aggregates keep numeric unsigned-64 text semantics and neutral values for
empty/all-`NULL` argument inputs. A grouped select with no matched rows returns
no rows, not one neutral aggregate row.

## Physical SQLite Handling

Generated SQL must use descriptor-owned physical table names such as
`_mylite_user_table_<table_id>`. Every generated SQLite identifier must be
quoted. Predicate and limit values must be bound with prepared statement
parameters. User SQL literals must not be interpolated into generated SQLite
SQL.

Representative physical SQL shapes:

```sql
SELECT "g", COUNT(*)
FROM "_mylite_user_table_1"
WHERE "id" >= ?1
GROUP BY "g"
ORDER BY "g" ASC
LIMIT ?2 OFFSET ?3
```

```sql
SELECT "g", SUM("n"), COUNT("n")
FROM "_mylite_user_table_1"
GROUP BY "g"
ORDER BY "g" DESC
```

The second shape is used for grouped `AVG(column)` so MyLite can preserve its
existing average formatting policy. Bitwise grouped aggregates use the
registered `_mylite_bit_and`, `_mylite_bit_or`, and `_mylite_bit_xor`
aggregate functions. SQLite owns per-group aggregate invocation; MyLite
callbacks own only the fixed-size per-group aggregate state.

## Results

A successful grouped aggregate select returns a normal public result set. It
sets `affected_rows` to `0`, `warning_count` to `0`, and statement row-count
state as a result-set statement (`ROW_COUNT()` returns `-1` after success).

Result columns:

- grouped descriptor column label is the select-item alias when present, or
  the descriptor column name otherwise;
- aggregate result label is the select-item alias when present, or the source
  aggregate expression text otherwise.

## Diagnostics

MyLite must provide deterministic diagnostics for:

- syntax errors and unsupported grammar;
- missing default schema;
- unknown schema;
- unknown table;
- reserved `_mylite_*` schema/table names;
- unsupported object kind once non-base-table descriptors exist;
- unknown selected/grouped column in field list;
- unknown `GROUP BY` column in group statement;
- unknown aggregate argument column in field list;
- unknown predicate column in where clause;
- unknown order column in order clause;
- selected nonaggregate descriptor column that differs from the `GROUP BY`
  descriptor column;
- unsupported aggregate forms beyond the documented grouped aggregate subset;
- unsupported grouping expression, alias grouping, ordinal grouping, multiple
  grouping keys, `HAVING`, rollup, joins, query modifiers, and subqueries;
- unsupported order expression, aggregate alias ordering, table-qualified order
  forms outside the existing descriptor reference subset, ordinal ordering, and
  multiple sort keys;
- unsupported limit expression or out-of-range limit literal;
- SQLite physical execution failures;
- allocation failures;
- public API misuse through existing public-surface behavior.

Where the slice implements a MySQL-compatible behavior, diagnostics should
match observed MySQL 8.4.9 behavior. Where the slice deliberately rejects a
wider MySQL-valid form, a stable MyLite unsupported diagnostic is acceptable.

## Tests

Add a fast plain C runtime test, preferably
`runtime_group_by_single_column_aggregate_test.c`, registered with a dotted
CTest name. Tests must cover:

- grouped `COUNT(*)`, `COUNT(column)`, `MIN`, `MAX`, `SUM`, `AVG`,
  `BIT_AND`, `BIT_OR`, and `BIT_XOR`;
- integer families currently supported by row values, including `INT`,
  `INTEGER`, `BIGINT`, and their `UNSIGNED` forms within MyLite's physical
  range;
- nullable group and aggregate columns;
- all-`NULL` aggregate argument groups;
- no matched rows returning no grouped rows;
- baseline `WHERE` predicate reuse;
- unqualified, table-qualified, schema-qualified, and alias-qualified group
  and aggregate references;
- `ORDER BY` default, `ASC`, `DESC`, grouped select alias ordering, `NULL`
  ordering, `LIMIT 0`, exact limits, and offset forms;
- result labels, `affected_rows`, warning count, absence/presence of result
  rows, and post-select `ROW_COUNT()`;
- missing default schema, unknown schema, unknown table, reserved target names,
  unknown field/group/order columns, and selected non-grouped descriptor
  columns;
- deterministic rejections for aliases in `GROUP BY`, ordinals, expressions,
  multiple group keys, `HAVING`, rollup, aggregate alias ordering, multiple
  sort keys, expression assignments, functions, joins, CTEs, subqueries, and
  unsupported aggregate forms;
- reopen persistence, grouped selects after rename/drop where applicable,
  `.mylite` preamble preservation, independent file-backed handles, zero-init
  cleanup, and existing parser/runtime regression entries.

Run the MySQL expectation script, focused CTest entries for parser and grouped
aggregates, existing aggregate/select lifecycle tests, and
`cmake --workflow --preset check` before marking the implementation complete.
