# Baseline Joined Aggregate SELECT

## Status

This phase extends the existing descriptor-backed grouped aggregate path from
one base table to the current two-source join envelope. It targets common app
queries such as parent rows with child counts:

```sql
SELECT p.id, COUNT(c.id)
FROM posts AS p LEFT JOIN comments AS c ON p.id = c.post_id
GROUP BY p.id;
```

This is intentionally not full aggregate join support. It admits one selected
integer descriptor group column and one selected aggregate result over the
existing two-source `JOIN` / `INNER JOIN` / `CROSS JOIN` / `LEFT [OUTER] JOIN`
subset. It reuses the current descriptor `ON`, `WHERE`, `HAVING`, grouped
`ORDER BY`, and `LIMIT` subsets. It does not add more than two sources,
multiple grouping keys, aggregate-only grouped projection, grouped
`COUNT(DISTINCT)`, expression projection, derived tables, subqueries, or
arbitrary SQLite pass-through.

## Sources

- MyLite README architecture: `README.md`
- MyLite engineering standards:
  `docs/architecture/engineering-standards.md`
- Baseline inner-join and left-join specs:
  `docs/specs/baseline-inner-join-select/specs.md` and
  `docs/specs/baseline-left-join-select/specs.md`
- Baseline grouped aggregate and HAVING specs:
  `docs/specs/baseline-group-by-single-column-aggregate/specs.md` and
  `docs/specs/baseline-having-grouped-aggregate/specs.md`
- SQLite source snapshot notes: `third_party/sqlite/README.md`
- MySQL 8.4 Reference Manual, `SELECT`:
  https://dev.mysql.com/doc/refman/8.4/en/select.html
- MySQL 8.4 Reference Manual, `JOIN` clause:
  https://dev.mysql.com/doc/refman/8.4/en/join.html
- MySQL 8.4 Reference Manual, aggregate functions:
  https://dev.mysql.com/doc/refman/8.4/en/aggregate-functions.html
- MySQL 8.4 Reference Manual, MySQL handling of `GROUP BY`:
  https://dev.mysql.com/doc/refman/8.4/en/group-by-handling.html

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior,
public SQLite APIs, and existing MyLite source code. It does not copy MySQL,
MariaDB, Percona, SQLite implementation internals, or other restrictively
licensed implementation sources.

## Runtime Evidence

The expectation script
`packages/libmylite/tests/mysql_baseline_joined_aggregate_select_expectations.sh`
records the MySQL 8.4.9 probes for this phase. Observed behavior:

- Grouped aggregates over joined rows run after `FROM` join evaluation and
  `WHERE` filtering.
- `LEFT JOIN` null-extension happens before grouping. `COUNT(*)` counts the
  null-extended joined row, while `COUNT(right_column)` ignores the
  null-extended `NULL` value.
- `COUNT(column)`, `MIN`, `MAX`, `SUM`, `AVG`, `BIT_AND`, `BIT_OR`, and
  `BIT_XOR` keep their usual per-group `NULL` handling over joined rows.
- A group column may come from either joined source. `NULL` group keys form one
  group and keep the existing `ORDER BY` `NULL` placement.
- `WHERE` predicates on joined columns filter rows after outer-join
  null-extension and before grouping.
- `HAVING` predicates are evaluated after grouping and before `ORDER BY` and
  `LIMIT`.
- `LIMIT 0`, `LIMIT row_count OFFSET offset`, and `LIMIT offset, row_count`
  behave like the existing grouped select subset.
- Unqualified names that match both joined sources are ambiguous with
  `1052 / 23000`.
- Unknown selected, grouped, ordered, joined, and having columns use the same
  clause-specific diagnostics as the existing single-table and joined paths.
- Successful row-returning statements leave `@@warning_count = 0` and make the
  following `ROW_COUNT()` return `-1`.

## Scope

In scope:

- `SELECT group_column [AS alias], aggregate [AS alias] FROM joined_source
  [WHERE ...] GROUP BY group_column [HAVING ...] [ORDER BY group_column]
  [LIMIT ...]`;
- `joined_source` from the current two-source descriptor-backed join subset:
  `JOIN`, `INNER JOIN`, `CROSS JOIN`, `LEFT JOIN`, and `LEFT OUTER JOIN`;
- optional one descriptor equality `ON` condition for inner/cartesian joins;
- required one descriptor equality `ON` condition for left outer joins;
- one selected group column from either source, limited to the current integer
  descriptor group-column family;
- one selected aggregate result from the existing grouped aggregate family:
  `COUNT(*)`, `COUNT(column)`, `MIN(column)`, `MAX(column)`, `SUM(column)`,
  `AVG(column)`, `BIT_AND(column)`, `BIT_OR(column)`, and `BIT_XOR(column)`;
- aggregate column arguments from either source, limited to existing integer
  descriptor aggregate rules;
- unqualified, table-qualified, alias-qualified, and
  schema-table-qualified group and aggregate column references through the
  current joined-source resolver;
- optional `WHERE` using the existing descriptor predicate subset over joined
  sources;
- optional `HAVING` using the existing selected group or selected aggregate
  predicate subset;
- optional `ORDER BY` by the selected group descriptor column or its alias;
- optional existing SELECT `LIMIT` and `OFFSET` forms;
- persistent and shadowing session temporary readable table descriptors through
  the existing readable-table resolver;
- result rows, result labels, `affected_rows`, `ROW_COUNT()`, `FOUND_ROWS()`,
  and warning behavior matching existing grouped and joined `SELECT`
  conventions for the supported subset.

Out of scope:

- `RIGHT JOIN`, full outer joins, natural joins, `USING`, comma joins,
  `STRAIGHT_JOIN` as a join operator, parenthesized table references,
  partitions, index hints, CTEs, derived tables, lateral tables, table
  functions, and more than two sources;
- multiple selected aggregates, aggregate-only grouped projection, multiple
  grouping keys, grouping by aliases, ordinals, literals, expressions,
  functions, or aggregate results;
- grouped `COUNT(DISTINCT column)`, expression aggregate arguments, literal
  aggregate arguments in joined grouped paths, `GROUP_CONCAT`, rollup,
  grouping sets, windows, and aggregate ordering;
- mixed-type join predicates, arbitrary `ON` predicates, join expressions,
  expression projection, scalar subqueries, correlated subqueries, and
  subquery predicates;
- decimal, approximate, string, binary string, temporal, JSON, enum, set,
  collation, or charset grouping and aggregate expression semantics beyond the
  existing join key and predicate slices;
- `SQL_CALC_FOUND_ROWS`, distinct grouped joins, locking behavior changes,
  optimizer hints, protocol-grade origin metadata, privileges, and exact
  optimizer plan equivalence.

## Ownership Boundary

- The public API remains unchanged. `mylite_execute()` owns public validation,
  result ownership, misuse behavior, and failure cleanup.
- Statement context owns diagnostics, warning count, result status,
  `ROW_COUNT()`, and `FOUND_ROWS()` state.
- Lexer/parser/AST already admit the relevant `SELECT`, join, aggregate,
  `GROUP BY`, `HAVING`, `ORDER BY`, and `LIMIT` syntax. This phase should not
  widen grammar beyond the current independently authored snippets.
- Analyzer/planner owns selected/default schema resolution, temporary-table
  shadowing, readable-table descriptor loading, duplicate alias detection,
  join-kind classification, join-condition validation, group-column
  resolution, aggregate-column resolution, predicate resolution, `HAVING`
  resolution, order resolution, and unsupported-shape diagnostics.
- Catalog owns logical schema/table/column descriptors. SQLite schema text is
  not authoritative for user-visible names, type families, visibility, or
  supported object kind.
- SQL generation owns stable physical table-name use, generated internal
  source aliases, identifier quoting, and bound predicate/limit parameters.
- SQLite owns physical join execution, `WHERE` filtering, grouping, aggregate
  calculation, `HAVING` filtering, sorting, and limiting. MyLite must not
  materialize joined source rows in memory for this path.
- Result builders own public result objects and descriptor-shaped aggregate
  readback. MyLite may materialize final result rows in `mylite_result`.
- Storage/VFS owns `.mylite` file opening, preamble offset handling, and
  shifted SQLite payload invariants. This feature does not mutate storage
  format or require SQLite fork patches.

## Syntax

MyLite Lemon-syntax sketch for the admitted shape:

```lemon
select_statement ::=
    SELECT select_modifiers grouped_aggregate_select_list FROM joined_table_source
    where_clause_opt group_clause_opt having_clause_opt order_clause_opt
    limit_clause_opt select_locking_clause_opt.

grouped_aggregate_select_list ::=
    select_item_group_column COMMA select_item_group_aggregate.

select_item_group_column ::= qualified_identifier select_item_alias_opt.

select_item_group_aggregate ::= grouped_aggregate_expression select_item_alias_opt.

grouped_aggregate_expression ::= COUNT LPAREN STAR RPAREN.
grouped_aggregate_expression ::= COUNT LPAREN qualified_identifier RPAREN.
grouped_aggregate_expression ::= MIN LPAREN qualified_identifier RPAREN.
grouped_aggregate_expression ::= MAX LPAREN qualified_identifier RPAREN.
grouped_aggregate_expression ::= SUM LPAREN qualified_identifier RPAREN.
grouped_aggregate_expression ::= AVG LPAREN qualified_identifier RPAREN.
grouped_aggregate_expression ::= BIT_AND LPAREN qualified_identifier RPAREN.
grouped_aggregate_expression ::= BIT_OR LPAREN qualified_identifier RPAREN.
grouped_aggregate_expression ::= BIT_XOR LPAREN qualified_identifier RPAREN.
```

The existing `joined_table_source`, `where_clause_opt`, `group_clause_opt`,
`having_clause_opt`, `order_clause_opt`, and `limit_clause_opt` grammar
nonterminals are reused. Runtime rejects unsupported combinations after AST
construction.

## Name Resolution

Source resolution reuses the existing joined readable-table resolver:

- unqualified source names require a selected default schema;
- schema-qualified source names use the explicit schema;
- session temporary tables shadow persistent base tables in the same schema;
- reserved `_mylite_*` schema/table names are rejected before SQL generation;
- unknown schemas and tables reuse existing MySQL-shaped diagnostics;
- unsupported object kinds must be rejected before physical SQL generation once
  non-base descriptors exist.

Each source reference name is the alias when present, otherwise the table name.
Two joined sources must not have the same reference name under MyLite's current
ASCII case-insensitive identifier comparison.

Column resolution for selected group columns, aggregate arguments, `ON`,
`WHERE`, `HAVING`, and `ORDER BY` is descriptor-driven:

- one-part `column` resolves only when exactly one joined source has a
  matching descriptor column;
- two-part `source.column` resolves through the source alias/table reference
  name;
- three-part `schema.table.column` resolves through unaliased
  schema-qualified sources;
- aliases hide original table qualifiers;
- unknown and ambiguous names use the existing clause-aware diagnostics;
- current descriptor catalog comparison remains ASCII case-insensitive as in
  the existing baseline.

The selected `GROUP BY` column and the `GROUP BY` clause column must resolve to
the same descriptor column on the same joined source. This same-source check is
observable when the same table is joined to itself under different aliases.

## Semantics

SQLite executes the joined grouped query after MyLite has planned and generated
descriptor-owned SQL.

- Inner/cartesian joins use the existing joined SELECT semantics.
- Left joins preserve every left-source row and null-extend right-source
  descriptor columns for unmatched rows.
- `GROUP BY` groups the joined rows after `WHERE` filtering.
- `COUNT(*)` counts joined rows, including null-extended left-join rows.
- `COUNT(column)` ignores `NULL` argument values, including null-extended
  unmatched right-side columns.
- `MIN`, `MAX`, `SUM`, `AVG`, and bitwise aggregates keep the existing
  MyLite grouped aggregate result semantics for each joined group.
- `HAVING` filters groups after aggregate calculation. It may refer only to the
  selected group column or selected aggregate result according to the current
  single-table `HAVING` subset.
- `ORDER BY` may refer only to the selected group column, the unaliased selected
  group column output label, or its explicit unqualified alias. `ASC` is
  default. Existing integer `NULL` ordering is preserved.
- `LIMIT` and `OFFSET` use the existing SELECT literal conversion and binding
  rules.
- Successful joined grouped selects return through the existing row-result API,
  with `affected_rows == 0`, `warning_count == 0`, and following
  `ROW_COUNT() == -1`.

For duplicate group keys, the aggregate result is deterministic. For groups
with duplicate order keys, MyLite claims only the explicit grouped key ordering
and does not promise additional tie order.

## Physical SQLite Handling

Generated SQL must use stable descriptor-owned physical table names and
internal SQLite aliases:

```sql
SELECT "_mylite_s0"."id", COUNT("_mylite_s1"."id")
FROM "_mylite_user_table_<posts_id>" AS "_mylite_s0"
LEFT JOIN "_mylite_user_table_<comments_id>" AS "_mylite_s1"
  ON "_mylite_s0"."id" = "_mylite_s1"."post_id"
WHERE "_mylite_s0"."id" >= ?1
GROUP BY "_mylite_s0"."id"
HAVING COUNT("_mylite_s1"."id") > ?2
ORDER BY "_mylite_s0"."id" ASC
LIMIT ?3 OFFSET ?4
```

Rules:

- physical table names come from MyLite descriptors;
- source aliases are generated, quoted, and never user-controlled;
- group, aggregate, join, predicate, having, and order columns come from MyLite
  descriptors and are qualified when the source is joined;
- every generated SQLite identifier is quoted;
- predicate, `HAVING`, `LIMIT`, and `OFFSET` values are converted by MyLite
  before execution and bound as prepared-statement parameters;
- no user SQL text or SQLite metadata lookup is used as authority;
- no SQLite optional syntax extension, virtual table, VFS change, or fork patch
  is required.

## Diagnostics

Supported joined grouped aggregates do not produce warnings.

Diagnostics reuse the existing joined SELECT and grouped aggregate conventions:

- syntax errors and unsupported grammar return deterministic parser or
  unsupported diagnostics;
- missing default schema, unknown schema, unknown table, reserved schema/table
  names, and duplicate aliases use current source-resolution diagnostics;
- missing `ON` for `LEFT [OUTER] JOIN` is a parse error before source
  resolution;
- unknown selected, grouped, aggregate, predicate, having, and order columns
  use their clause-specific existing diagnostics;
- ambiguous joined column references use `1052 / 23000`;
- unsupported group columns, aggregate arguments, `HAVING` operands, `ORDER BY`
  keys, and wider aggregate forms return deterministic unsupported errors;
- physical SQLite, allocation, and public API misuse failures use existing
  runtime conventions.

## Tests

Add fast C runtime tests for:

- `LEFT JOIN` grouped by left column with `COUNT(*)`, `COUNT(right_column)`,
  `MIN`, `MAX`, `SUM`, `AVG`, `BIT_AND`, `BIT_OR`, and `BIT_XOR`, verifying
  null-extended row behavior;
- inner join grouped by the right source;
- group column from either source and aggregate argument from either source;
- optional `WHERE`, `HAVING`, `ORDER BY`, `LIMIT 0`, exact limit, and offset
  forms;
- aliases, schema-qualified sources, and qualified column references;
- ambiguous names, unknown selected/group/aggregate/where/having/order columns,
  unknown schemas/tables, duplicate aliases, missing `ON` for left joins, and
  unsupported grouped join shapes;
- reopen persistence, table rename/drop behavior through descriptors, file
  preamble preservation, and independent file-backed handles where practical;
- existing parser, join, grouped aggregate, and lifecycle tests still passing.

The MySQL expectation script must pass against MySQL 8.4.9 before the feature
is considered implemented.
