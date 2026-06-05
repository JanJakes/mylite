# Baseline GROUP BY Primary-Key Projection

## Purpose

This feature extends the descriptor-backed grouped `SELECT` baseline with two
mode-sensitive rules:

- with `ONLY_FULL_GROUP_BY` enabled, a selected descriptor column is legal when
  the `GROUP BY` list contains every primary-key column of that selected
  column's source table;
- with `ONLY_FULL_GROUP_BY` disabled, MyLite admits descriptor projections that
  MySQL accepts as nondeterministic grouped values.

The main user-visible target is the common WordPress shape that joins a base
row to one-to-many metadata rows, groups by the base row primary key, and still
selects base-row columns:

```sql
SELECT p.ID, p.post_title, COUNT(m.meta_id)
FROM wp_posts AS p
LEFT JOIN wp_postmeta AS m ON p.ID = m.post_id
GROUP BY p.ID
ORDER BY p.post_title
```

This is not a full implementation of MySQL's functional-dependence engine. It
does not infer dependencies from unique `NOT NULL` indexes, equality
predicates, transitive joins, views, derived tables, or `WHERE` single-value
filters.

## Compatibility Authorities

- MySQL 8.4 Reference Manual, `GROUP BY` handling:
  <https://dev.mysql.com/doc/refman/8.4/en/group-by-handling.html>
- MySQL 8.4 Reference Manual, functional dependence detection:
  <https://dev.mysql.com/doc/refman/8.4/en/group-by-functional-dependence.html>
- Runtime observations from MySQL 8.4.9, captured by
  `packages/libmylite/tests/mysql_baseline_group_by_primary_key_projection_expectations.sh`.

The spec text and grammar snippets below are independently authored for MyLite.

## MySQL 8.4.9 Observations

With default `ONLY_FULL_GROUP_BY` enabled:

- `GROUP BY pk` permits selecting other columns from the same table when `pk`
  is the complete primary key.
- `GROUP BY pk_part_1, pk_part_2` permits selecting other columns from the same
  table when those keys are the complete composite primary key.
- `GROUP BY pk_part_1` does not permit selecting a column from a table whose
  primary key is `(pk_part_1, pk_part_2)`.
- In a `LEFT JOIN`, `GROUP BY` over the left source primary key permits
  selecting left-source columns, but it does not permit selecting arbitrary
  right-source columns.
- With `ONLY_FULL_GROUP_BY` disabled, MySQL permits nonaggregated descriptor
  columns and wildcard-expanded columns even when they are not functionally
  dependent on the grouping keys. This includes WordPress's grouped outer-join
  maintenance query shape that groups by a joined table primary key while
  selecting a stable column from the outer source.
- `SELECT p.* ... GROUP BY p.pk` is accepted when every visible `p` column is
  from the primary-key-determined source.
- `SELECT * ... GROUP BY p.pk` over a joined source fails when the wildcard
  expands any visible column from a source that is not functionally determined.
- `ORDER BY` may refer to a nonselected descriptor column that is functionally
  dependent on the grouping columns.
- A nondependent selected column fails with `1055 / 42000` and a SELECT-list
  diagnostic. A nondependent `ORDER BY` column fails with `1055 / 42000` and an
  ORDER BY diagnostic.
- Successful in-range queries produce `warning_count = 0`.

## Supported SQL Surface

This phase applies only to grouped descriptor-backed `SELECT` statements already
handled by the grouped aggregate planner:

```sql
SELECT select_item [, select_item ...]
FROM source
[WHERE baseline_predicate]
GROUP BY group_key [, group_key ...]
[HAVING baseline_grouped_having]
[ORDER BY order_key [ASC|DESC]]
[LIMIT baseline_limit]
```

Where:

- `source` is the existing one-table source or current descriptor joined
  source envelope admitted by grouped aggregate execution, including
  descriptor-backed left outer join chains.
- `group_key` is one to four descriptor column references already supported by
  the multiple-key `GROUP BY` baseline.
- `select_item` may be:
  - a grouped descriptor column;
  - a descriptor column from a source whose complete primary key is present in
    the `GROUP BY` list;
  - when `ONLY_FULL_GROUP_BY` is disabled, any resolved descriptor column in
    the current grouped source envelope;
  - an unqualified `*` as the whole select list, expanded source by source,
    only when every expanded visible column is legal by the same rule;
  - a qualified `source.*`, only when every visible column of that source is
    legal by the same rule;
  - an existing grouped aggregate result.
- Nonaggregate selected descriptor columns and qualified wildcard expansions
  must precede aggregate select items in this slice. This preserves the
  existing grouped result-planning shape and keeps diagnostics deterministic.
  The existing parser admits unqualified `*` only as the whole select list, so
  `SELECT *, COUNT(*) ...` remains outside this slice.
- Aggregate select items remain limited to the existing grouped aggregate
  functions and type families.
- `ORDER BY` may refer to:
  - one selected nonaggregate descriptor projection or its unique alias;
  - one nonselected descriptor column that is either grouped or functionally
    dependent on the grouped primary key;
  - one existing selected aggregate alias supported by the grouped aggregate
    baseline.

`HAVING`, `WHERE`, and `LIMIT` retain their current grouped aggregate limits.

## Explicit Non-Goals

- Full MySQL functional-dependence inference from unique `NOT NULL` indexes,
  equalities in `WHERE` or join conditions, transitive dependencies, constants,
  outer-join special cases, views, derived tables, CTEs, or scalar subqueries.
- `ANY_VALUE()`, grouping expressions, grouping aliases, grouping ordinals,
  `ROLLUP`, grouping sets, window functions, or deterministic selection of
  nondeterministic nondependent projection values.
- Arbitrary expression projections, expression `ORDER BY`, ordinal `ORDER BY`,
  multiple sort keys, full `HAVING` expressions, or unbounded aggregate
  function coverage.
- Metadata or catalog mutation. This feature reads descriptors only.

## Ownership And Architecture

- Public API: no ABI changes. Applications continue to call `mylite_execute()`
  and inspect the existing row-result object.
- Statement context: unchanged; grouped statement execution continues through
  the current runtime statement path.
- Parser/AST: no new grammar tokens are required. Existing select items,
  wildcards, qualified wildcards, column references, grouped clauses, joined
  sources, ordering, and limits are reused.
- Analyzer/planner: owns descriptor resolution, primary-key coverage checks,
  wildcard expansion, projection ordering, `ONLY_FULL_GROUP_BY` diagnostics, and
  generated SQLite SQL shape.
- Catalog module: remains the authority for table, column, and primary-key
  descriptors. The planner reads catalog descriptors and does not inspect
  SQLite metadata.
- Result builder: emits projected descriptor columns followed by aggregate
  result columns, using existing result object conventions.
- Storage/VFS: unchanged. The `.mylite` preamble and shifted SQLite payload are
  not touched.
- SQLite physical row storage: SQLite performs the physical `GROUP BY`,
  aggregation, ordering, and limit on descriptor-selected physical columns.
  MyLite validates the MySQL legality before SQL generation and keeps generated
  identifiers quoted.

## Grammar Snippet

No parser expansion is needed. The relevant admitted shapes are:

```lemon
select_statement ::= SELECT select_list FROM from_source where_opt group_by_clause
                     having_opt order_by_opt limit_opt.

select_list ::= STAR.
select_list ::= select_item_list.

select_item ::= expression alias_opt.
expression ::= qualified_identifier.
expression ::= qualified_identifier DOT STAR.
expression ::= grouped_aggregate_function.

group_by_clause ::= GROUP BY group_key_list.
group_key_list ::= qualified_identifier.
group_key_list ::= group_key_list COMMA qualified_identifier.

order_by_opt ::= .
order_by_opt ::= ORDER BY qualified_identifier order_direction_opt.
order_by_opt ::= ORDER BY identifier order_direction_opt.
```

These productions describe the MyLite subset. They are not copied from MySQL
grammar.

## Descriptor Resolution Rules

- Unqualified and qualified column references reuse the current descriptor
  column resolver and its case-insensitive identifier matching.
- In a single-table source, `*` expands visible columns from that table.
- In a joined source, `*` expands visible columns from each current source in
  source order, matching existing non-grouped `SELECT *` behavior.
- `source.*` matches a one-part alias or table name, or a two-part
  `schema.table` name when no alias shadows the table name.
- A selected descriptor column is accepted when either:
  - it exactly matches one of the resolved `GROUP BY` keys for the same source;
  - the same source has a primary-key descriptor and every primary-key part
    exactly matches one resolved `GROUP BY` key for that source.
- If `ONLY_FULL_GROUP_BY` is disabled, descriptor-column and wildcard legality
  checks accept every resolved descriptor column that fits the existing grouped
  planner shape. SQLite chooses the representative grouped value, matching
  MySQL's documented nondeterministic relaxed-mode behavior.
- A wildcard expansion applies the same rule to each visible column it expands.
  The first illegal expanded column determines the `1055 / 42000` diagnostic.
- Composite primary keys require all parts in the `GROUP BY` list. Order does
  not matter for legality, although result grouping uses the user's key order.
- Primary-key coverage is per source. A primary key grouped from source `p` does
  not determine columns from source `c`.
- This phase does not treat descriptor unique indexes as functional
  determinants, even when they are `NOT NULL`.

## ORDER BY Semantics

- Default direction remains ascending. `ASC` and `DESC` retain current behavior.
- Integer, temporal, `BIT`, and ASCII nonbinary string ordering behavior remains
  the behavior of the existing grouped aggregate/order implementations.
- `ORDER BY grouped_column` continues to work.
- `ORDER BY selected_projection_alias` works only when the alias is unique among
  selected nonaggregate projections.
- `ORDER BY selected_projection_column` works when the unqualified name is
  unambiguous among selected nonaggregate projections.
- `ORDER BY nonselected_dependent_column` is accepted when descriptor resolution
  identifies exactly one grouped or primary-key-determined source column.
- `ORDER BY nondependent_column` returns the MySQL-compatible `1055 / 42000`
  ORDER BY diagnostic for this slice.
- Ties remain unspecified unless the user supplies additional supported sort
  keys in a future phase. This slice does not add tie-breakers.

## Generated SQLite Shape

For:

```sql
SELECT p.id, p.title, COUNT(c.id)
FROM posts AS p LEFT JOIN comments AS c ON p.id = c.post_id
GROUP BY p.id
ORDER BY p.title
LIMIT 2
```

MyLite generates a descriptor-built SQLite statement shaped like:

```sql
SELECT _mylite_s0."id", _mylite_s0."title", COUNT(_mylite_s1."id")
FROM "_mylite_user_table_<posts_id>" AS _mylite_s0
LEFT JOIN "_mylite_user_table_<comments_id>" AS _mylite_s1
  ON _mylite_s0."id" = _mylite_s1."post_id"
GROUP BY _mylite_s0."id"
ORDER BY _mylite_s0."title" COLLATE "utf8mb4_0900_ai_ci" ASC
LIMIT ?1
```

The actual physical names and parameter numbers are chosen by the existing
runtime helpers. Every generated identifier is quoted. Predicates, aggregate
options, and limits continue to bind values through prepared statements.

## Result Behavior

- Successful statements return a row result set.
- Result columns are the selected nonaggregate projection labels followed by
  selected aggregate result labels.
- Projection labels use explicit aliases when present, otherwise descriptor
  column names. Wildcard-expanded columns use descriptor column names.
- Aggregate labels retain the existing grouped aggregate label behavior.
- `affected_rows` / row count metadata follow current result-set conventions.
- `warning_count` is `0` for supported in-range successful queries.

## Diagnostics

- Unsupported select-list shapes: `GROUP BY supports selected descriptor group
  columns followed by aggregate results`.
- More than four group keys: current grouped-key limit diagnostic.
- More than sixteen aggregate results: current aggregate-result limit
  diagnostic.
- Unknown or ambiguous columns: existing resolver diagnostics for the relevant
  clause.
- Nondependent selected descriptor column or wildcard-expanded column:
  `1055 / 42000` with `Expression #N of SELECT list ...`.
- Nondependent `ORDER BY` descriptor column: `1055 / 42000` with
  `Expression #N of ORDER BY clause ...`.
- Duplicate selected projection aliases in `ORDER BY`: deterministic MyLite
  unsupported diagnostic for nonunique selected projection aliases.
- Allocation failures: existing out-of-memory diagnostics.
- SQLite physical failures: existing grouped aggregate execution diagnostics.

## Test Plan

- MySQL expectation script verifying success and error behavior against MySQL
  8.4.9.
- C runtime test for single-table primary-key projection, joined projection,
  qualified and unqualified wildcard expansion, composite primary keys,
  unselected dependent `ORDER BY`, persistence after reopen, and independent
  file-backed handles.
- C runtime negative tests for no primary key, partial composite primary key,
  right-source nondependent projection, joined `*` expansion containing
  nondependent columns, nondependent `ORDER BY`, unknown columns, duplicate
  projection aliases, and unsupported post-aggregate descriptor projection.
- Existing grouped aggregate, joined aggregate, primary-key, select-order-limit,
  file-backed, VFS, and catalog lifecycle tests must continue to pass.
