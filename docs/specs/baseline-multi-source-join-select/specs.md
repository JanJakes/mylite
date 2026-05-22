# Baseline Multi-Source Join SELECT

## Status

This phase extends plain descriptor-backed `SELECT` from the current two-source
join envelope to inner/cartesian join chains and comma source lists with three
or more base-table sources.

This is intentionally not full MySQL table-reference support. It admits
persistent and shadowing session temporary base tables, aliases, descriptor
projection, qualified wildcards, the existing descriptor `WHERE`, `ORDER BY`,
`LIMIT`, and `SQL_CALC_FOUND_ROWS` behavior, and descriptor equality `ON`
conditions for each explicit inner/cartesian join edge. It does not add
multi-source outer joins, mixed comma/explicit precedence, derived tables,
`USING`, natural joins, expression projection, or wider grouped/DML join
sources.

## Sources

- MyLite README architecture: `README.md`
- MyLite engineering standards:
  `docs/architecture/engineering-standards.md`
- Existing inner, comma, left, right, qualified wildcard, grouped aggregate,
  and found-rows join specs under `docs/specs/`
- SQLite source snapshot notes: `third_party/sqlite/README.md`
- MySQL 8.4 Reference Manual, `SELECT`:
  https://dev.mysql.com/doc/refman/8.4/en/select.html
- MySQL 8.4 Reference Manual, `JOIN` clause:
  https://dev.mysql.com/doc/refman/8.4/en/join.html
- MySQL 8.4 Reference Manual, identifier qualifiers:
  https://dev.mysql.com/doc/refman/8.4/en/identifier-qualifiers.html

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior,
public SQLite APIs, and existing MyLite source code. It does not copy MySQL,
MariaDB, Percona, SQLite implementation internals, or other restrictively
licensed implementation sources.

## MySQL 8.4.9 Observations

The expectation script
`packages/libmylite/tests/mysql_baseline_multi_source_join_select_expectations.sh`
records the MySQL 8.4.9 probes for this phase. Observed behavior:

- `JOIN`, `INNER JOIN`, and `CROSS JOIN` chains are accepted left to right.
  `JOIN`, `INNER JOIN`, and `CROSS JOIN` are syntactic equivalents for the
  admitted inner/cartesian forms.
- An explicit join edge may omit `ON`, producing a cartesian product for that
  edge.
- A later `ON` condition can reference the newly joined source and any source
  already present on the left side of the join expression. It cannot reference
  a source that appears later.
- Pure comma lists such as `FROM a, b, c` produce a cartesian product before
  `WHERE` filtering.
- `SELECT *` expands visible columns in source order. Duplicate result labels
  are preserved.
- Source aliases hide original table names. Duplicate source reference names
  report `1066 / 42000`.
- Unqualified references that match more than one source report
  `1052 / 23000` in the active clause.
- Unknown selected, predicate, order, and join-condition names use the same
  clause-aware diagnostics as the existing two-source joined path.
- `SQL_CALC_FOUND_ROWS` over an admitted multi-source join returns the limited
  row result, emits MySQL deprecation warning `1287`, stores the pre-limit
  count in `FOUND_ROWS()`, and leaves `ROW_COUNT()` at `-1`.
- Successful row-returning multi-source joins without evaluated warning
  constructs leave `@@warning_count = 0` and make a following `ROW_COUNT()`
  return `-1`.

## Scope

In scope:

- `SELECT column_list FROM source JOIN source [ON left = right] JOIN source
  [ON left = right] ...`;
- `SELECT column_list FROM source INNER JOIN source ...`;
- `SELECT column_list FROM source CROSS JOIN source ...`;
- `SELECT column_list FROM source, source, source [, source ...] ...`;
- `SELECT *` and limited qualified wildcard projection over the admitted
  source lists;
- three or more sources, plus preserving the existing two-source behavior;
- persistent and shadowing session temporary readable table descriptors;
- unqualified and schema-qualified source table names using the existing
  selected/default schema policy;
- optional `AS alias` and bare aliases on every source;
- existing validated/no-op source index hints on every admitted source;
- optional one descriptor equality `ON` condition per explicit inner/cartesian
  join edge;
- `ON left_column = right_column` over supported same-family integer-family
  columns or supported ASCII nonbinary string-family columns;
- later `ON` edges may reference any source already in the joined left side and
  the newly joined right source;
- existing descriptor `WHERE` predicate subset over all joined sources,
  including the existing same-scope `left_column = right_column` equality
  predicate for plain joined `SELECT`;
- existing descriptor `ORDER BY` and `LIMIT` subsets over all joined sources;
- existing limited `SQL_CALC_FOUND_ROWS` behavior over the admitted plain
  multi-source source envelope;
- result rows, warning count, `ROW_COUNT()`, `FOUND_ROWS()`, temporary-table
  shadowing, persistence, independent file-backed handles, and file-format
  behavior matching existing descriptor-backed row-returning `SELECT`.

Out of scope:

- multi-source `LEFT JOIN`, `RIGHT JOIN`, or mixed outer/inner chains;
- mixed comma and explicit join precedence, such as `FROM a, b JOIN c ON ...`;
- `USING`, natural joins, full outer joins, `STRAIGHT_JOIN` as a join operator,
  parenthesized table references, ODBC escape joins, lateral or derived
  tables, table functions, CTEs, partitions, and optimizer hints;
- join predicates other than one descriptor-column equality;
- mixed-type join comparisons and MySQL conversion/warning behavior for those
  comparisons;
- decimal, approximate, temporal, binary-string, bit, enum, set, JSON, or
  spatial join key equality;
- literal, expression, function, arithmetic, scalar-subquery, row-constructor,
  or parameter join predicates;
- general table-backed expression projection, `DISTINCT` joins, grouped
  multi-source joins, aggregate multi-source joins, `GROUP_CONCAT()` over
  joined sources beyond existing limits, `INSERT ... SELECT`, `REPLACE ...
  SELECT`, CTAS, `TABLE`, joined `UPDATE`, joined `DELETE`, locking behavior
  changes, optimizer behavior, privileges, or arbitrary SQLite pass-through.

## Ownership Boundary

- Public API: unchanged. `mylite_execute()` returns the existing row-result
  object for successful joins and uses existing ownership and cleanup rules.
- Statement context: owns diagnostics, warning count, result status,
  `ROW_COUNT()`, and `FOUND_ROWS()` state.
- Lexer/parser/AST: owns admitting inner/cartesian join chains and pure comma
  source lists, retaining left-associative source order and edge-local `ON`
  conditions. It does not resolve descriptors.
- Analyzer/planner: owns selected/default schema resolution, temporary-table
  shadowing, descriptor loading, duplicate alias detection, join-edge
  validation, projection resolution, predicate planning, ordering, limiting,
  and unsupported-shape diagnostics.
- Catalog: remains authoritative for logical schema, table, column, and index
  metadata. SQLite schema text is not used as user-visible authority.
- SQL generation: owns stable physical table names, generated internal source
  aliases, identifier quoting, and bound predicate/limit parameters.
- SQLite: executes the physical join, filtering, sorting, and limiting through
  standard generated SQL. MyLite does not materialize joined source rows in
  memory and does not require a SQLite fork patch for this phase.
- Result builders own the public result object and descriptor-shaped row
  readback. Final result rows may be materialized in `mylite_result`, as in
  existing row-returning statements.
- Storage/VFS: unchanged. This read-only feature does not mutate the `.mylite`
  preamble, shifted SQLite payload, catalog descriptors, descriptor versions,
  catalog generation, or SQLite schema generation.

## Syntax

MyLite Lemon-syntax sketch for the admitted shape:

```lemon
select_statement ::=
    SELECT select_modifiers select_item_list FROM inner_join_table_source
    where_clause_opt group_clause_opt having_clause_opt select_order_clause_opt
    limit_clause_opt select_locking_clause_opt.

select_statement ::=
    SELECT select_modifiers STAR FROM inner_join_table_source
    where_clause_opt group_clause_opt having_clause_opt select_order_clause_opt
    limit_clause_opt select_locking_clause_opt.

select_statement ::=
    SELECT select_modifiers select_item_list FROM comma_table_sources
    where_clause_opt group_clause_opt having_clause_opt select_order_clause_opt
    limit_clause_opt select_locking_clause_opt.

select_statement ::=
    SELECT select_modifiers STAR FROM comma_table_sources
    where_clause_opt group_clause_opt having_clause_opt select_order_clause_opt
    limit_clause_opt select_locking_clause_opt.

inner_join_table_source ::=
    table_source inner_join_operator table_source join_condition_opt.

inner_join_table_source ::=
    inner_join_table_source inner_join_operator table_source join_condition_opt.

inner_join_operator ::= JOIN.
inner_join_operator ::= INNER JOIN.
inner_join_operator ::= CROSS JOIN.

comma_table_sources ::= table_source COMMA table_source.
comma_table_sources ::= comma_table_sources COMMA table_source.

join_condition_opt ::= .
join_condition_opt ::= ON join_equality_condition.

join_equality_condition ::= qualified_identifier EQUAL qualified_identifier.
```

The existing exactly two-source `LEFT [OUTER] JOIN` and `RIGHT [OUTER] JOIN`
SELECT grammar remains separate. Joined `UPDATE` and joined `DELETE` continue
to use their existing two-source grammar.

## Name Resolution

Each source is resolved independently:

1. Resolve the effective schema from the explicit qualifier or selected default
   schema.
2. Reject reserved `_mylite_*` schema or table names before physical SQL is
   generated.
3. Resolve a shadowing session temporary descriptor first, then a durable base
   table descriptor.
4. Reject missing schemas, missing tables, and unsupported object kinds before
   physical SQL is generated.

The source reference name is the alias when present, otherwise the table name.
Every source reference name in the joined scope must be unique under MyLite's
current ASCII case-insensitive identifier comparison.

Column references resolve as follows:

- one-part `column` resolves only when exactly one joined source has a matching
  descriptor column;
- if more than one source has a matching descriptor column, the active clause
  reports ambiguous-column `1052 / 23000`;
- two-part `source.column` resolves through the source alias when present, or
  the table name when no alias is present;
- aliases hide original table names;
- three-part `schema.table.column` resolves only for unaliased matching
  sources;
- unknown names report `1054 / 42S22` in the active clause.

For explicit join edges, the `ON` condition is resolved against the sources
already present on the joined left side plus the newly joined right source.
This preserves MySQL's left-to-right visibility for admitted join chains and
prevents a join edge from seeing sources introduced later.

## Semantics

For explicit inner/cartesian chains, MyLite plans each edge in source order:

- `JOIN`, `INNER JOIN`, and `CROSS JOIN` are generated as standard inner joins.
- If an edge has an admitted `ON` equality, SQLite applies it as the join
  predicate.
- If an edge omits `ON`, SQLite produces the cartesian product for that edge.
- `NULL = NULL` does not match for admitted equality joins.
- Supported string join keys use MyLite's registered ASCII
  `utf8mb4_0900_ai_ci` SQLite collation, matching the current two-source join
  slice.

Pure comma lists are generated as cartesian inner joins between all listed
sources. Existing descriptor `WHERE` predicates then filter the joined rows,
including admitted same-family descriptor-column equality predicates.

Wildcard projection expands visible descriptor columns in source order.
Qualified wildcards expand the matching source only. Explicit descriptor
references may name invisible columns, as in existing SELECT slices.

Ordering and limiting reuse the existing descriptor-backed `SELECT` semantics.
No tie order is promised when admitted order keys compare equal.

`SQL_CALC_FOUND_ROWS` runs the existing found-rows companion query over the same
multi-source FROM and WHERE shape without `ORDER BY` or `LIMIT`, records the
deprecation warning already implemented for the modifier, and updates
`FOUND_ROWS()`.

## Physical SQLite Handling

Generated SQL uses stable MyLite physical table names and generated source
aliases:

```sql
SELECT "_mylite_s0"."id", "_mylite_s2"."z"
FROM "_mylite_user_table_<a_id>" AS "_mylite_s0"
JOIN "_mylite_user_table_<b_id>" AS "_mylite_s1"
  ON "_mylite_s0"."k" = "_mylite_s1"."a_k"
JOIN "_mylite_user_table_<c_id>" AS "_mylite_s2"
  ON "_mylite_s1"."k" = "_mylite_s2"."b_k"
WHERE ...
ORDER BY ...
LIMIT ...
```

Every generated SQLite identifier is quoted. Predicate and limit literals
continue to be bound through prepared-statement parameters. Join predicates in
this phase are descriptor-column equality only and introduce no literal
parameters.

The implementation must not rely on SQLite schema text to resolve logical
columns. It must not add indexes, optimizer hints, temporary materialization,
triggers, constraints, or SQLite fork patches.

## Diagnostics

Diagnostics reuse existing joined SELECT conventions unless noted:

| Case | Diagnostic |
| --- | --- |
| Syntax outside admitted grammar | existing parse error `1064 / 42000` |
| Missing default schema for unqualified source | existing `1046 / 3D000` |
| Unknown schema | existing `1049 / 42000` |
| Unknown source table | existing `1146 / 42S02` |
| Reserved `_mylite_*` schema/table source | existing reserved-name diagnostic |
| Duplicate source reference name | `1066 / 42000` |
| Unknown selected / predicate / order / `ON` column | `1054 / 42S22` in the active clause |
| Ambiguous selected / predicate / order / `ON` column | `1052 / 23000` in the active clause |
| Unsupported `ON` expression | deterministic unsupported joined SELECT diagnostic |
| Unsupported join-key family | deterministic unsupported joined SELECT diagnostic |
| Multi-source outer join chain | deterministic unsupported diagnostic or syntax error |
| Mixed comma/explicit source list | syntax error or deterministic unsupported diagnostic |
| Unsupported grouped, aggregate, distinct, DML, CTAS, or insert-source shape | existing unsupported diagnostic |
| Physical SQLite failure | existing internal/SQLite failure path |
| Allocation failure | existing `MYLITE_NOMEM` path with cleanup |

## Tests

Extend parser and runtime coverage:

- parser acceptance for three-source and four-source explicit inner/cross join
  chains;
- parser acceptance for three-source pure comma lists;
- parser rejection or deterministic runtime rejection for mixed comma/explicit
  lists and multi-source outer joins;
- successful three-source explicit joins with chained `ON` conditions;
- successful edge without `ON` cartesian join in a chain;
- successful three-source and four-source comma joins filtered by existing
  `WHERE` predicates;
- `SELECT *` and qualified wildcard source-order behavior;
- aliases, schema-qualified sources/columns, index-hint validation, duplicate
  aliases, alias hiding, unknown source, unknown schema, unknown selected,
  unknown predicate, unknown order, unknown `ON`, and ambiguous names;
- `WHERE`, `ORDER BY`, `LIMIT 0`, exact limits, oversized limits, and
  `SQL_CALC_FOUND_ROWS`;
- `ROW_COUNT()`, `FOUND_ROWS()`, and warning-count behavior;
- temporary table shadowing, reopen persistence, table rename/drop behavior,
  `.mylite` preamble preservation, and independent file-backed handles through
  extensions to existing join lifecycle tests;
- existing lexer, parser, select, join, right/left/comma join, grouped
  aggregate, qualified wildcard, found-rows, DML, catalog, VFS, and file-format
  tests still passing.

## Verification

Required commands before completion:

```sh
cmake --build --preset dev
ctest --preset dev -R 'libmylite\.(parser|runtime\.(inner_join_select|left_join_select|right_join_select|select_qualified_columns|found_rows_function|joined_aggregate_select))$' --output-on-failure
packages/libmylite/tests/mysql_baseline_multi_source_join_select_expectations.sh
cmake --workflow --preset check
```

## Compatibility Documentation

Update `COMPATIBILITY.md`, `docs/compatibility/sql-joins.md`, and
`docs/compatibility/sql-query-expressions.md` to describe the exact
multi-source plain `SELECT` subset. Keep grouped joins, joined DML,
multi-source outer joins, mixed comma/explicit precedence, derived tables,
general expression projection, and full optimizer behavior documented as
unsupported.
