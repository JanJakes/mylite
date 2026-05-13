# Baseline LEFT JOIN SELECT

## Status

This feature extends the descriptor-backed two-source `SELECT` join path from
inner/cartesian joins to the first outer-join slice: `LEFT JOIN` and
`LEFT OUTER JOIN` with one descriptor equality `ON` condition.

This is intentionally not full MySQL outer-join support. It supports two
readable MyLite table descriptors, optional source aliases, required
one-column same-family integer or ASCII string descriptor equality `ON`, the
existing descriptor `WHERE` predicate subset, one descriptor `ORDER BY` key,
and the existing `SELECT` `LIMIT` forms. It does not implement `USING`,
natural joins, right joins, chains of more than two tables, derived tables,
join updates, or general expression projection.

## Sources

- MyLite README architecture: `README.md`
- MyLite engineering standards:
  `docs/architecture/engineering-standards.md`
- Baseline inner-join, select, table alias, qualified-column, predicate,
  ordering, row value, temporary-table, and catalog specs under `docs/specs/`
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

## Runtime Evidence

Observed against local Docker runtime `mylite-mysql-849`:

```sql
SELECT VERSION();
-- 8.4.9

CREATE DATABASE mylite_left_join_probe;
USE mylite_left_join_probe;
CREATE TABLE lefts(id INT NOT NULL, k INT NULL, v INT NULL, name VARCHAR(20));
CREATE TABLE rights(id INT NOT NULL, k INT NULL, w INT NULL, name VARCHAR(20));
INSERT INTO lefts
VALUES (1,10,100,'alpha'),(2,20,200,'Beta'),(3,NULL,300,'none'),(4,40,400,'onlyleft');
INSERT INTO rights
VALUES (7,10,700,'ALPHA'),(8,10,800,'beta'),(9,NULL,900,'none'),(10,50,1000,'onlyright');

SELECT l.id, l.k, r.id, r.k, r.w
FROM lefts AS l LEFT JOIN rights AS r ON l.k = r.k
ORDER BY l.id, r.id;

SELECT l.id, r.id
FROM lefts l LEFT OUTER JOIN rights r ON l.k = r.k
ORDER BY l.id, r.id;

SELECT l.id, r.id
FROM lefts l LEFT JOIN rights r ON l.k = r.k
WHERE r.id IS NULL
ORDER BY l.id;

SELECT *
FROM lefts LEFT JOIN rights ON lefts.k = rights.k
ORDER BY lefts.id, rights.id;
```

Observed behavior:

- `LEFT JOIN` and `LEFT OUTER JOIN` are accepted.
- `LEFT JOIN` with `ON` returns every left-source row. Matching right-source
  rows are joined normally; unmatched right-source descriptor columns read as
  `NULL`.
- `NULL = NULL` does not match for the admitted `=` operator, so a left row
  with a `NULL` key is preserved with `NULL` right columns rather than matched
  to a right row with a `NULL` key.
- `SELECT *` for `LEFT JOIN ... ON` returns visible columns from the left
  source followed by visible columns from the right source. Duplicate result
  labels are preserved.
- `WHERE` filters after join null-extension, so `WHERE right.id IS NULL`
  selects unmatched left rows for this dataset.
- `LEFT JOIN` without a join specification reports parse error
  `1064 / 42000`.
- MySQL accepts `LEFT JOIN ... USING (...)`, but it coalesces common columns and
  changes wildcard projection. MyLite defers it.
- Unqualified column references that match both joined sources report
  `1052 / 23000`, `Column '<name>' in <clause> is ambiguous`.
- Unknown selected, predicate, order, or join-condition columns report
  `1054 / 42S22` in the active clause context.
- Successful row-returning statements leave `@@warning_count = 0` and following
  `ROW_COUNT() = -1`.

## Scope

In scope:

- `SELECT column_list FROM left_source LEFT JOIN right_source ON left = right ...`;
- `SELECT column_list FROM left_source LEFT OUTER JOIN right_source ON left = right ...`;
- `SELECT * FROM left_source LEFT [OUTER] JOIN right_source ON left = right ...`;
- required `ON left_column = right_column` where both operands resolve to
  supported same-family integer-family columns or supported ASCII string-family
  columns in the two-source scope;
- unqualified, table-qualified, alias-qualified, and schema-table-qualified
  source references using the existing selected/default schema policy;
- optional source aliases using `AS alias` or bare `alias`;
- projection of descriptor columns only, plus wildcard expansion of visible
  descriptor columns from left then right;
- existing descriptor `WHERE`, `ORDER BY`, and `LIMIT` subsets, with column
  resolution performed across both joined sources;
- left-row preservation and right-side `NULL` extension for unmatched rows;
- persistent and shadowing session temporary readable table descriptors through
  the existing readable-table resolver;
- result rows, `affected_rows`, `ROW_COUNT()`, `FOUND_ROWS()`, and warning
  behavior matching existing descriptor-backed row-returning `SELECT`
  conventions for the supported subset.

Out of scope:

- right outer joins;
- full outer joins;
- more than two source tables or chained joins;
- outer joins without `ON`;
- `USING`, natural joins, comma joins, `STRAIGHT_JOIN` as a join operator,
  parenthesized table references, ODBC escape joins, lateral or derived tables,
  table functions, CTEs, partitions, and index hints;
- qualified wildcards such as `table.*` or `alias.*`;
- join predicates other than one descriptor-column equality;
- mixed-type join comparisons and MySQL's conversion/warning behavior for those
  comparisons;
- decimal, approximate, temporal, binary-string, and bit join key equality;
- literal, expression, function, arithmetic, scalar-subquery, row-constructor,
  or parameter join predicates;
- general table-backed expression projection, aggregate joins, grouped joins,
  `DISTINCT` joins, `SQL_CALC_FOUND_ROWS` joins, locking behavior changes, or
  arbitrary SQLite pass-through;
- collations beyond the existing registered MyLite ASCII `utf8mb4_0900_ai_ci`
  string-comparison support;
- optimizer hints, protocol-grade origin metadata, privileges, locks, and
  optimizer plan equivalence.

## Ownership Boundary

- The public API remains unchanged. `mylite_execute()` owns public call
  validation, result ownership, misuse behavior, and failure cleanup.
- Statement context owns statement diagnostics, warning count, result status,
  affected-row conventions, `ROW_COUNT()`, and `FOUND_ROWS()` state.
- Lexer/parser/AST own `LEFT [OUTER] JOIN` syntax admission and source spans.
  They do not read catalog metadata or decide column validity.
- Analyzer/planner owns selected/default schema resolution, temporary-table
  shadowing, readable-table descriptor loading, duplicate alias detection,
  join-kind classification, join-condition validation, projection resolution,
  predicate resolution, order resolution, and unsupported-shape diagnostics.
- Catalog owns logical schema/table/column descriptors. SQLite schema text is
  not authoritative for user-visible names, type families, visibility, or
  supported object kind.
- SQL generation owns stable physical table-name use, generated internal source
  aliases, identifier quoting, and bound predicate/limit parameters.
- SQLite owns physical left-outer-join execution over generated stable table
  names. MyLite does not materialize joined row sets in memory for the supported
  path.
- Result builders own MyLite result objects and descriptor-shaped row readback.
- Storage/VFS owns `.mylite` file opening, preamble offset handling, and
  shifted SQLite payload invariants; this feature does not mutate storage
  format or require SQLite fork patches.

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

joined_table_source ::= table_source.
joined_table_source ::= table_source join_operator table_source join_condition_opt.

table_source ::= table_name table_alias_opt.

join_operator ::= JOIN.
join_operator ::= INNER JOIN.
join_operator ::= CROSS JOIN.
join_operator ::= LEFT JOIN.
join_operator ::= LEFT OUTER JOIN.

join_condition_opt ::= .
join_condition_opt ::= ON join_equality_condition.

join_equality_condition ::= qualified_identifier EQUAL qualified_identifier.
```

Runtime then rejects missing `ON` for `LEFT [OUTER] JOIN` because MySQL's outer
join syntax requires a join specification. The optional condition remains only
for the existing inner/cartesian join operators.

## Name Resolution

Source table resolution reuses the existing readable-table resolver:

- unqualified names require a selected default schema;
- schema-qualified names use that schema directly;
- session temporary tables shadow persistent base tables in the same schema;
- reserved `_mylite_*` schema/table names are rejected before physical SQL is
  generated;
- unknown schemas and tables reuse existing MySQL-shaped diagnostics;
- once non-base-table descriptors exist, unsupported object kinds must be
  rejected before physical SQL is generated.

Each source reference name is the alias when present, otherwise the table name.
Two sources in the same join scope must not have the same reference name under
MyLite's current ASCII case-insensitive identifier comparison.

Column resolution for selected columns, `ON`, `WHERE`, and `ORDER BY` is
descriptor-driven:

- one-part `column` resolves only when exactly one joined source has a matching
  descriptor column;
- two-part `source.column` resolves through the source alias/table reference
  name;
- three-part `schema.table.column` resolves through unaliased
  schema-qualified sources;
- aliases hide original table qualifiers;
- unknown names and ambiguous names use the existing clause-aware diagnostics;
- current descriptor catalog comparison remains ASCII case-insensitive as in
  the existing baseline.

## Semantics

For supported left joins, SQLite executes the physical row join. MyLite's role
is to own parsing, descriptor resolution, diagnostics, conversion, SQL
generation, parameter binding, and result conversion.

- `LEFT JOIN` and `LEFT OUTER JOIN` preserve every left-source row.
- Matching right-source rows are returned for the admitted same-family
  descriptor-column equality.
- If no right row matches, each right-source projected descriptor column reads
  as `NULL`.
- `NULL` join-key values do not match for the admitted `=` operator.
- Mixed-type `ON` equality is rejected for now. MySQL applies type conversion
  and may produce warnings for those comparisons; this slice does not implement
  that conversion surface inside join predicates.
- `SELECT *` expands visible columns from the left source, then visible columns
  from the right source, preserving duplicate labels.
- Explicit descriptor projections may name invisible columns as existing
  single-table projection does.
- `WHERE` filters joined rows after null-extension using the current predicate
  subset. This includes `right_column IS NULL` filters over the `NULL`-extended
  side.
- `ORDER BY` uses one descriptor column or an explicit select-item alias.
  `ASC` is default. Existing `NULL` ordering and type-specific ordering
  behavior are preserved for the admitted order column families.
- `LIMIT` and `OFFSET` use the existing `SELECT` literal conversion and binding
  rules.
- Successful row-returning joins return through the existing public result
  object, with result rows, `affected_rows == 0`, and `warning_count == 0`.
  Following `ROW_COUNT()` returns `-1`.

For duplicate sort keys, MyLite claims only the key order verified by tests. It
does not promise deterministic tie order unless the statement supplies an
additional deterministic key; multiple order keys remain out of scope.

## Physical SQLite Handling

Generated SQL must use stable descriptor-owned physical table names and
internal SQLite aliases:

```sql
SELECT "_mylite_s0"."id", "_mylite_s1"."w"
FROM "_mylite_user_table_<left_id>" AS "_mylite_s0"
LEFT JOIN "_mylite_user_table_<right_id>" AS "_mylite_s1"
  ON "_mylite_s0"."k" = "_mylite_s1"."k"
WHERE "_mylite_s1"."id" IS NULL
ORDER BY "_mylite_s0"."id" ASC
LIMIT ?1
```

Every generated SQLite identifier is quoted. User-supplied source names,
aliases, and qualifiers are never interpolated as physical SQL identifiers.
Predicate and limit literals are bound parameters. Join condition operands are
descriptor columns, not user literals.

This approach uses public SQLite SQL/prepared-statement APIs only. No SQLite
fork patch or optional SQLite syntax is needed.

## Diagnostics

| Case | Diagnostic |
| --- | --- |
| Syntax outside admitted join grammar | existing parse error `1064 / 42000` |
| `LEFT [OUTER] JOIN` without `ON` | deterministic parse/unsupported diagnostic |
| More than two sources | existing parse or unsupported diagnostic |
| Missing default schema | existing `1046 / 3D000` |
| Unknown schema or table | existing schema/table diagnostics |
| Reserved `_mylite_*` schema or table | existing reserved-name diagnostic |
| Duplicate source reference name | `1066 / 42000`, not unique table/alias |
| Unsupported join operator or join condition | deterministic unsupported diagnostic |
| Unsupported join-condition column family | deterministic unsupported diagnostic |
| Unknown selected column | `1054 / 42S22`, field list |
| Ambiguous selected column | `1052 / 23000`, field list |
| Unknown join-condition column | `1054 / 42S22`, on clause |
| Ambiguous join-condition column | `1052 / 23000`, on clause |
| Unknown predicate column | `1054 / 42S22`, where clause |
| Ambiguous predicate column | `1052 / 23000`, where clause |
| Unknown order column | `1054 / 42S22`, order clause |
| Ambiguous order column or alias | `1052 / 23000`, order clause |
| Qualified wildcard | existing parse or unsupported diagnostic |
| `USING`, natural, right, or chained outer joins | existing parse or unsupported diagnostic |
| Unsupported projection expression, aggregate, grouping, or distinct join | deterministic unsupported diagnostic |
| Physical SQLite failure | existing physical row-operation diagnostic |
| Allocation failure | existing allocation diagnostic |

## Tests

Add `runtime_left_join_select_test.c` and register it as
`libmylite.runtime.left_join_select`.

Add MySQL expectation script:
`packages/libmylite/tests/mysql_baseline_left_join_select_expectations.sh`.

Coverage:

- parser acceptance for `LEFT JOIN` and `LEFT OUTER JOIN`;
- parser/runtime rejection for missing `ON`, `USING`, right joins, natural
  joins, and chained joins;
- left-row preservation with matching and unmatched rows;
- `SELECT *` left-then-right visible descriptor-column order with duplicate
  labels;
- right-side `NULL` extension for unmatched rows and for left `NULL` join keys;
- integer equality joins and string equality joins under existing ASCII
  collation;
- `WHERE` filters over left columns, right columns, and `right_column IS NULL`;
- existing one-column `ORDER BY`, including right-side `NULL` placement and
  `DESC`;
- `LIMIT 0`, exact row counts, and row counts larger than the joined set;
- schema-qualified and alias-qualified source and column references;
- temporary table shadowing in joined reads;
- duplicate aliases, ambiguous columns, unknown columns, unknown tables,
  unknown schemas, missing default schema, reserved names, and unsupported
  mixed-type join keys;
- reopen persistence, join after table rename, and join after drop diagnostics;
- `.mylite` preamble invariants and independent file-backed handle state;
- no result-row materialization in MyLite beyond existing result building;
- existing parser, runtime, VFS, catalog, row values, select/order/limit,
  inner-join, and lifecycle tests remain passing.

## Compatibility Documentation

Update `COMPATIBILITY.md` and `docs/compatibility/sql-joins.md` to say MyLite
supports a limited two-source descriptor-backed `LEFT [OUTER] JOIN ... ON`
subset in `SELECT`.

Do not claim support for right joins, full joins, `USING`, natural joins,
join chains, derived tables, arbitrary `ON` predicates, join updates, aggregate
joins, grouped joins, expression projections, full collation behavior, optimizer
plan equivalence, or protocol-grade origin metadata.

## Verification

Required before implementation is marked done:

1. `cmake --build --preset dev`
2. `ctest --preset dev --output-on-failure -R 'libmylite\.(parser|runtime\.(left_join_select|inner_join_select|select_where_lifecycle|select_order_limit_lifecycle|temporary_table_lifecycle))'`
3. `sh packages/libmylite/tests/mysql_baseline_left_join_select_expectations.sh`
4. `cmake --workflow --preset check`

The final diff must be reviewed for parser/AST ownership, MySQL evidence,
descriptor-driven planning, SQLite SQL generation, string/integer join-key
safety, `NULL` extension semantics, predicate placement, compatibility docs,
test relevance, file-format safety, cleanup on failure, and scope control.
