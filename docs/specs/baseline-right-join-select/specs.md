# Baseline RIGHT JOIN SELECT

## Status

This feature extends the descriptor-backed two-source `SELECT` join path from
inner, comma, and left outer joins to the matching right outer join spelling:
`RIGHT JOIN` and `RIGHT OUTER JOIN`.

This is intentionally not full MySQL join support. It admits the same narrow
join-condition, projection, predicate, ordering, and limit envelope as the
current left-join path. It does not implement `USING`, natural joins, full
outer joins, chained joins, derived tables, joined DML right joins, or general
expression projection.

## Sources

- MyLite README architecture: `README.md`
- MyLite engineering standards:
  `docs/architecture/engineering-standards.md`
- Existing inner, comma, and left join specs under `docs/specs/`
- SQLite source snapshot notes: `third_party/sqlite/README.md`
- MySQL 8.4 Reference Manual, `SELECT`:
  https://dev.mysql.com/doc/refman/8.4/en/select.html
- MySQL 8.4 Reference Manual, `JOIN` clause:
  https://dev.mysql.com/doc/refman/8.4/en/join.html
- MySQL 8.4 Reference Manual, outer join simplification:
  https://dev.mysql.com/doc/refman/8.4/en/outer-join-simplification.html

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior,
public SQLite APIs, and existing MyLite source code. It does not copy MySQL,
MariaDB, Percona, SQLite implementation internals, or other restrictively
licensed implementation sources.

## MySQL 8.4.9 Observations

The expectation script
`packages/libmylite/tests/mysql_baseline_right_join_select_expectations.sh`
records the runtime probes for this feature. Observed against local MySQL
8.4.9:

- `RIGHT JOIN` and `RIGHT OUTER JOIN` are accepted with an `ON` join
  specification.
- `RIGHT JOIN` preserves every row from the source written after `RIGHT JOIN`.
  Matching left-source rows are joined normally; unmatched left-source
  descriptor columns read as `NULL`.
- `NULL = NULL` does not match for the admitted `=` join condition.
- `SELECT *` expands visible columns from the syntactic left source followed by
  visible columns from the syntactic right source, even when left columns are
  `NULL`-extended.
- `WHERE` filters after null extension, so `WHERE left.id IS NULL` can select
  unmatched right rows.
- `RIGHT JOIN` without `ON` reports parse error `1064 / 42000`.
- MySQL accepts `RIGHT JOIN ... USING (...)` and `NATURAL RIGHT JOIN`, but
  those forms change column coalescing and wildcard projection and remain
  deferred in this slice.
- `FULL OUTER JOIN` is not accepted as a full-outer operator and remains a
  syntax error. Plain `FULL JOIN` is not treated as a full-outer operator for
  this slice because MySQL can parse `FULL` as a source alias in otherwise valid
  joins.
- Successful row-returning right joins without `SQL_CALC_FOUND_ROWS` leave
  `@@warning_count = 0`, and a following `ROW_COUNT()` returns `-1`.
- `SELECT SQL_CALC_FOUND_ROWS ... RIGHT JOIN ... LIMIT ...` records MySQL
  deprecation warning `1287`, returns the limited visible rows, updates
  `FOUND_ROWS()` to the pre-limit matched row count, leaves `@@warning_count`
  at `1`, and leaves `ROW_COUNT()` at `-1`.

MySQL documentation also states that right outer joins are converted to
equivalent left joins during parsing for optimization. MyLite does not expose
that implementation detail; it uses the observation only to confirm that right
outer join semantics mirror left outer join semantics with the preserved side
swapped.

## Scope

In scope:

- `SELECT column_list FROM left_source RIGHT JOIN right_source ON left = right`;
- `SELECT column_list FROM left_source RIGHT OUTER JOIN right_source ON left =
  right`;
- `SELECT * FROM left_source RIGHT [OUTER] JOIN right_source ON left = right`;
- exactly two source tables;
- persistent and shadowing session temporary readable table descriptors;
- unqualified and schema-qualified source table names using the existing
  selected/default schema policy;
- optional source aliases using `AS alias` or bare alias;
- existing validated/no-op source index hints on each admitted source;
- required `ON left_column = right_column`, where both operands resolve in the
  joined source scope and are supported same-family integer-family columns or
  supported ASCII string-family columns;
- descriptor projection and wildcard expansion over both sources, preserving
  syntactic source order for `*`;
- the existing descriptor `WHERE`, `ORDER BY`, and `LIMIT` subsets over joined
  sources;
- existing limited `SQL_CALC_FOUND_ROWS` modifier behavior over the admitted
  right-join source envelope;
- right-row preservation and left-side `NULL` extension for unmatched rows;
- existing warning-count, `ROW_COUNT()`, `FOUND_ROWS()`, result metadata,
  temporary-table shadowing, persistence, and file-format behavior from
  descriptor-backed row-returning `SELECT`.

Out of scope:

- `RIGHT JOIN` in joined `UPDATE` or joined `DELETE`;
- full outer joins;
- more than two sources or mixed/chained joins;
- outer joins without `ON`;
- `USING`, natural joins, `STRAIGHT_JOIN` as a join operator, parenthesized
  table references, ODBC escape joins, lateral or derived tables, table
  functions, CTEs, and partitions;
- join predicates other than one descriptor-column equality;
- mixed-type join comparisons and MySQL conversion/warning behavior for those
  comparisons;
- decimal, approximate, temporal, binary-string, bit, enum, set, JSON, or
  spatial join key equality;
- literal, expression, function, arithmetic, scalar-subquery, row-constructor,
  or parameter join predicates;
- general table-backed expression projection, full grouped joins, `DISTINCT`
  join rows, locking behavior changes, optimizer behavior, or arbitrary SQLite
  pass-through.

## Ownership Boundary

- Public API: unchanged. `mylite_execute()` returns a normal row result for
  successful right joins and uses existing public result ownership rules.
- Statement context: owns diagnostics, warning count, result status,
  `ROW_COUNT()`, and `FOUND_ROWS()` state.
- Lexer/parser/AST: owns admitting `RIGHT [OUTER] JOIN`, preserving source
  order and join kind, and retaining source spans. It does not resolve
  descriptors.
- Analyzer/planner: owns selected/default schema resolution, temporary-table
  shadowing, descriptor loading, duplicate source-reference diagnostics,
  join-condition validation, projection resolution, predicate planning,
  ordering, limiting, and unsupported-shape diagnostics.
- Catalog: remains authoritative for logical schema, table, column, and index
  metadata. SQLite schema text is not user-visible authority.
- SQL generation: owns stable physical table names, generated internal aliases,
  identifier quoting, and bound predicate/limit parameters.
- SQLite: executes the physical right outer join using the bundled public
  SQLite engine. MyLite does not materialize joined row sets in memory for this
  path and does not need a SQLite fork patch.
- Storage/VFS: unchanged. This is a read-only feature and does not mutate the
  `.mylite` preamble, shifted SQLite payload format, or catalog descriptors.

## Syntax

MyLite Lemon-syntax sketch for this slice:

```lemon
select_statement ::=
    SELECT select_modifiers select_item_list FROM joined_table_source
    where_clause_opt group_clause_opt having_clause_opt order_clause_opt
    limit_clause_opt select_locking_clause_opt.

select_statement ::=
    SELECT select_modifiers STAR FROM joined_table_source
    where_clause_opt group_clause_opt having_clause_opt order_clause_opt
    limit_clause_opt select_locking_clause_opt.

joined_table_source ::= table_source join_operator table_source join_condition_opt.

join_operator ::= JOIN.
join_operator ::= INNER JOIN.
join_operator ::= CROSS JOIN.
join_operator ::= LEFT JOIN.
join_operator ::= LEFT OUTER JOIN.
join_operator ::= RIGHT JOIN.
join_operator ::= RIGHT OUTER JOIN.

join_condition_opt ::= .
join_condition_opt ::= ON join_equality_condition.

join_equality_condition ::= qualified_identifier EQUAL qualified_identifier.
```

Runtime rejects missing `ON` for `RIGHT [OUTER] JOIN`, matching the existing
left outer join policy and MySQL's syntax requirement. The optional condition
remains only for current inner/cartesian join operators.

`FULL OUTER JOIN`, `NATURAL RIGHT JOIN`, `RIGHT JOIN ... USING`, and chained
joins remain syntax errors or deterministic unsupported-shape errors, depending
on where the current parser first rejects them. Plain `FULL JOIN` is not an
admitted full-outer join spelling in this slice; it may still be accepted by
MySQL-compatible alias parsing when `FULL` is a table alias followed by an
ordinary `JOIN`.

## Name Resolution

Source table resolution reuses the current joined-select resolver:

- unqualified names require a selected default schema;
- schema-qualified names use the named schema directly;
- session temporary descriptors shadow persistent base tables in the same
  schema;
- reserved `_mylite_*` schema/table names are rejected before physical SQL is
  generated;
- unknown schemas and tables reuse existing MySQL-shaped diagnostics;
- unsupported object kinds are rejected before physical SQL is generated once
  non-base-table descriptors exist.

Each source reference name is the alias when present, otherwise the table name.
Two sources in the same scope must not share a reference name under the current
ASCII case-insensitive descriptor comparison.

Column resolution for selected columns, `ON`, `WHERE`, and `ORDER BY` matches
the current joined-select path:

- one-part `column` resolves only when exactly one joined source has a matching
  descriptor column;
- two-part `source.column` resolves through the source alias, or table name
  when the source is unaliased;
- three-part `schema.table.column` resolves only for an unaliased matching
  source;
- aliases hide original table qualifiers;
- unknown and ambiguous names use existing clause-aware diagnostics.

## Semantics

`RIGHT JOIN` preserves every row from the syntactic right source. Matching rows
from the syntactic left source are returned for the admitted equality predicate;
if no left row matches, every projected left-source descriptor column reads as
SQL `NULL`.

The admitted `ON` condition:

- supports `=` only;
- supports same-family integer-family columns and supported ASCII string-family
  columns;
- uses ordinary SQL three-valued equality semantics, so `NULL = NULL` is not
  true;
- uses MyLite's registered limited Unicode `utf8mb4_0900_ai_ci` collation for admitted
  string-family values;
- rejects mixed-type and unsupported-type column-to-column comparisons with the
  same deterministic unsupported diagnostic as the left-join path.

`SELECT *` expands visible descriptor columns from the syntactic left source,
then visible descriptor columns from the syntactic right source. This is true
even though right rows are the preserved side and left columns may be
`NULL`-extended.

`WHERE` filters after null extension. `ORDER BY`, `LIMIT`, select-item aliases,
source-qualified references, warning count, `ROW_COUNT()`, and `FOUND_ROWS()`
reuse existing descriptor-backed joined `SELECT` conventions.

For duplicate sort keys, MyLite claims only the sorted key behavior covered by
tests and does not promise tied-row order.

## Physical SQLite Handling

Generated SQL uses descriptor-owned stable physical names and internal aliases:

```sql
SELECT "_mylite_s0"."id", "_mylite_s1"."id"
FROM "_mylite_user_table_<left_id>" AS "_mylite_s0"
RIGHT JOIN "_mylite_user_table_<right_id>" AS "_mylite_s1"
ON "_mylite_s0"."k" = "_mylite_s1"."k"
WHERE "_mylite_s0"."id" IS NULL
ORDER BY "_mylite_s1"."id" ASC
LIMIT ?1
```

All identifiers are quoted. Scalar predicate and limit values are bound. Join
column references are generated from descriptors, not user text.

The bundled SQLite snapshot is version 3.53.0 and supports right joins through
the public SQL engine. This feature is therefore a MyLite wrapper/translation
feature over public SQLite prepare/bind/step APIs, not a targeted SQLite fork
hook.

## Diagnostics

Existing joined-select diagnostics remain:

- syntax errors: `1064 / 42000`;
- missing default schema: `1046 / 3D000`;
- unknown schema: `1049 / 42000`;
- unknown table: `1146 / 42S02`;
- duplicate source alias: `1066 / 42000`;
- ambiguous selected, `ON`, `WHERE`, or `ORDER BY` column: `1052 / 23000`;
- unknown selected, `ON`, `WHERE`, or `ORDER BY` column: `1054 / 42S22`;
- reserved `_mylite_*` schema/table names: current reserved-name diagnostics;
- unsupported mixed-type or unsupported-type join equality: deterministic
  MyLite unsupported diagnostic;
- unsupported `USING`, natural joins, full joins, join chains, derived tables,
  or broad expressions: syntax or deterministic unsupported diagnostics;
- physical SQLite failures and allocation failures: existing runtime/nomem
  diagnostics.

Successful supported right joins return a row result set and leave the next
`ROW_COUNT()` value at `-1`, following existing row-returning `SELECT`
conventions. Without `SQL_CALC_FOUND_ROWS`, `warning_count == 0`. With
`SQL_CALC_FOUND_ROWS`, MyLite records the existing deprecation warning and
updates connection-local `FOUND_ROWS()` state to the pre-limit joined row count.

## Tests

Add or extend fast C tests under `packages/libmylite/tests/` for:

- parser acceptance of `RIGHT JOIN` and `RIGHT OUTER JOIN`;
- parser/runtime rejection of `RIGHT JOIN` without `ON`, `FULL OUTER JOIN`,
  `NATURAL RIGHT JOIN`, `USING`, and join chains;
- successful right-row preservation with left-side `NULL` extension;
- `SELECT *` column order and duplicate labels;
- `WHERE left_column IS NULL` after null extension;
- default, `ASC`, `DESC`, `LIMIT 0`, exact, and oversized limits;
- string equality using the existing registered limited Unicode collation;
- source index hints, aliases, schema-qualified sources and columns, and
  temporary-table shadowing;
- ambiguous/unknown selected, `ON`, `WHERE`, and `ORDER BY` diagnostics;
- missing default schema, unknown schema/table, and reserved-name diagnostics;
- `ROW_COUNT()`, warning count, persistence/reopen, table rename/drop,
  independent file-backed handles, and `.mylite` preamble preservation.

Run the MySQL expectation script for this feature against MySQL 8.4.9 before
claiming support.
