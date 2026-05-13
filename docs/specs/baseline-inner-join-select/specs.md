# Baseline Inner Join SELECT

## Status

This feature specifies the first multi-table query slice for descriptor-backed
base tables. It extends the existing `SELECT` column-list and wildcard path
from one source table to two source tables joined by MySQL's inner/cartesian
join family.

This is intentionally not full MySQL join support. It supports two readable
MyLite table descriptors, optional source aliases, optional one-column
descriptor equality `ON`, the existing descriptor `WHERE` predicate subset,
one descriptor `ORDER BY` key, and the existing `LIMIT` forms. It does not
implement outer joins, `USING`, natural joins, derived tables, join updates, or
general expression projection.

## Sources

- MyLite README architecture: `README.md`
- MyLite engineering standards:
  `docs/architecture/engineering-standards.md`
- Baseline select, table alias, qualified-column, predicate, ordering, row
  value, temporary-table, and catalog specs under `docs/specs/`
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

CREATE DATABASE mylite_join_probe;
USE mylite_join_probe;
CREATE TABLE lefts(id INT NOT NULL, k INT NULL, v INT NULL, name VARCHAR(20));
CREATE TABLE rights(id INT NOT NULL, k INT NULL, w INT NULL, name VARCHAR(20));
INSERT INTO lefts VALUES (1,10,100,'alpha'),(2,20,200,'Beta'),(3,NULL,300,'none');
INSERT INTO rights VALUES (7,10,700,'ALPHA'),(8,10,800,'beta'),(9,NULL,900,'none');

SELECT * FROM lefts JOIN rights ON lefts.k = rights.k ORDER BY lefts.id, rights.id;
SELECT l.id, r.w FROM lefts AS l INNER JOIN rights AS r ON l.k = r.k ORDER BY l.id, r.id;
SELECT l.id, r.id FROM lefts l CROSS JOIN rights r ORDER BY l.id, r.id LIMIT 4;
SELECT l.id, r.id FROM lefts l CROSS JOIN rights r ON l.k = r.k ORDER BY l.id, r.id;
SELECT l.id, r.id FROM lefts l JOIN rights r ORDER BY l.id, r.id LIMIT 4;
SELECT lefts.id, rights.id FROM lefts JOIN rights ON lefts.name = rights.name
ORDER BY lefts.id, rights.id;
```

Observed behavior:

- `JOIN`, `INNER JOIN`, and `CROSS JOIN` are accepted in this slice. With no
  `ON` condition, the result is a cartesian product.
- `CROSS JOIN ... ON` is accepted and behaves as an inner join for the tested
  equality condition.
- `SELECT *` returns visible columns from the left source followed by visible
  columns from the right source. Duplicate result labels are preserved.
- Table aliases hide the original table qualifier in the same source scope.
- Duplicate source aliases report `1066 / 42000`, `Not unique table/alias`.
- Unqualified column references that match both joined sources report
  `1052 / 23000`, `Column '<name>' in <clause> is ambiguous`.
- Unknown selected, predicate, order, or join-condition columns report
  `1054 / 42S22` in the active clause context.
- Explicit select-item aliases may be used by `ORDER BY`; duplicate matching
  aliases remain ambiguous as in the existing single-table path.
- Successful row-returning statements leave `@@warning_count = 0` and following
  `ROW_COUNT() = -1`.

## Scope

In scope:

- `SELECT column_list FROM left_source join_operator right_source ...`;
- `SELECT * FROM left_source join_operator right_source ...`;
- `join_operator` forms `JOIN`, `INNER JOIN`, and `CROSS JOIN`;
- optional `ON left_column = right_column` where both operands resolve to
  descriptor columns in the two-source scope;
- no `ON` condition, producing the cartesian product accepted by MySQL for the
  admitted join operators;
- unqualified, table-qualified, alias-qualified, and schema-table-qualified
  source references using the existing selected/default schema policy;
- optional source aliases using `AS alias` or bare `alias`;
- projection of descriptor columns only, plus wildcard expansion of visible
  descriptor columns from left then right;
- existing descriptor `WHERE`, `ORDER BY`, and `LIMIT` subsets, with column
  resolution performed across both joined sources;
- integer and ASCII string equality join keys covered by runtime comparison
  tests;
- persistent and shadowing session temporary readable table descriptors through
  the existing readable-table resolver;
- result rows, `affected_rows`, `ROW_COUNT()`, `FOUND_ROWS()`, and warning
  behavior matching existing descriptor-backed row-returning `SELECT`
  conventions for the supported subset.

Out of scope:

- more than two source tables;
- comma joins;
- outer joins, natural joins, `USING`, `STRAIGHT_JOIN` as a join operator,
  parenthesized table references, ODBC escape joins, lateral or derived tables,
  table functions, CTEs, partitions, and index hints;
- qualified wildcards such as `table.*` or `alias.*`;
- join predicates other than one descriptor-column equality;
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
  `ROW_COUNT()`, and `FOUND_ROWS()` state.
- Lexer/parser/AST own join syntax admission and source spans. They do not read
  catalog state.
- Analyzer/planner code resolves both table sources through MyLite schema and
  readable-table descriptors, rejects unsupported join shapes, resolves all
  projection, `ON`, `WHERE`, and `ORDER BY` columns against descriptor metadata,
  and builds a physical query plan with per-column source origin.
- The durable catalog and temporary catalog remain metadata authorities. SQLite
  schema text is never used to discover logical columns.
- SQLite owns physical join execution over generated stable table names. MyLite
  generates quoted SQL from descriptors and binds all literal predicate and
  limit values through prepared statements.
- Storage/VFS and `.mylite` file-format invariants are unchanged. This feature
  reads physical rows only and does not mutate the catalog or user data.

## Supported SQL Grammar

The feature admits one top-level statement per `mylite_execute()` call.

Independently authored MyLite Lemon-syntax snippets:

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

join_condition_opt ::= .
join_condition_opt ::= ON join_equality_condition.

join_equality_condition ::= qualified_identifier EQUAL qualified_identifier.
```

The existing `table_name`, `qualified_identifier`, `table_alias_opt`,
`where_clause_opt`, `order_clause_opt`, and `limit_clause_opt` nonterminals are
reused.

## Name Resolution

Each table source is resolved independently:

1. Resolve the effective schema from the explicit qualifier or selected default
   schema.
2. Reject reserved `_mylite_*` schema or table names before SQL generation.
3. Resolve a shadowing session temporary descriptor first, then a durable base
   table descriptor.
4. Reject missing schemas, missing tables, or unsupported object kinds with the
   existing descriptor diagnostics.

The source reference name is the alias when present; otherwise it is the table
name. Two sources in the same join scope must not have the same reference name
under the current ASCII case-insensitive descriptor comparison policy.

Column references resolve as follows:

- one-part `column` resolves only when exactly one joined source has a matching
  descriptor column;
- if both sources have a matching descriptor column, the active clause reports
  MySQL-compatible ambiguous-column `1052 / 23000`;
- two-part `source.column` resolves against the source alias when present, or
  the table name when no alias exists;
- after a source is aliased, the original table name no longer qualifies that
  source;
- three-part `schema.table.column` resolves only for an unaliased source whose
  resolved schema and table names match;
- a matched source with a missing column, or a qualifier that does not match any
  source, reports unknown-column `1054 / 42S22` in the active clause context.

## Semantics

For supported joins, SQLite executes the physical row join. MyLite's role is to
own parsing, descriptor resolution, diagnostics, conversion, SQL generation,
parameter binding, and result conversion.

- `JOIN` and `INNER JOIN` use standard inner-join behavior.
- `CROSS JOIN` without `ON` produces the cartesian product.
- `JOIN`, `INNER JOIN`, and `CROSS JOIN` with `ON left = right` return rows
  where the descriptor column equality is true. `NULL` values do not match for
  the admitted `=` operator.
- `SELECT *` expands visible columns from the left source, then visible columns
  from the right source, preserving duplicate labels.
- Explicit descriptor projections may name invisible columns as existing
  single-table projection does.
- `WHERE` filters joined rows after the join using the current predicate subset.
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
JOIN "_mylite_user_table_<right_id>" AS "_mylite_s1"
  ON "_mylite_s0"."k" = "_mylite_s1"."k"
WHERE "_mylite_s0"."v" = ?1
ORDER BY "_mylite_s1"."w" ASC
LIMIT ?2
```

Every generated SQLite identifier is quoted. User-supplied source names,
aliases, and qualifiers are never interpolated as physical SQL identifiers.
Predicate and limit literals are bound parameters. Join condition operands are
descriptor columns, not user literals.

This approach uses public SQLite SQL/prepared-statement APIs only. No SQLite
fork patch or optional SQLite `UPDATE/DELETE ... ORDER BY ... LIMIT` feature is
needed.

## Diagnostics

| Case | Diagnostic |
| --- | --- |
| Syntax outside admitted join grammar | existing parse error `1064 / 42000` |
| More than two sources | existing parse or unsupported diagnostic |
| Missing default schema | existing `1046 / 3D000` |
| Unknown schema or table | existing schema/table diagnostics |
| Reserved `_mylite_*` schema or table | existing reserved-name diagnostic |
| Duplicate source reference name | `1066 / 42000`, not unique table/alias |
| Unsupported join operator or join condition | deterministic unsupported diagnostic |
| Unknown selected column | `1054 / 42S22`, field list |
| Ambiguous selected column | `1052 / 23000`, field list |
| Unknown join-condition column | `1054 / 42S22`, on clause |
| Ambiguous join-condition column | `1052 / 23000`, on clause |
| Unknown predicate column | `1054 / 42S22`, where clause |
| Ambiguous predicate column | `1052 / 23000`, where clause |
| Unknown order column | `1054 / 42S22`, order clause |
| Ambiguous order column or alias | `1052 / 23000`, order clause |
| Qualified wildcard | existing parse or unsupported diagnostic |
| Unsupported projection expression, aggregate, grouping, or distinct join | deterministic unsupported diagnostic |
| Physical SQLite failure | existing physical row-operation diagnostic |
| Allocation failure | existing allocation diagnostic |

## Tests

Add `runtime_inner_join_select_test.c` and register it as
`mylite.runtime.inner_join_select`.

Add MySQL runtime expectation script
`packages/libmylite/tests/mysql_baseline_inner_join_select_expectations.sh`.

Coverage:

- `JOIN`, `INNER JOIN`, `CROSS JOIN ... ON`, and cartesian `JOIN`/`CROSS JOIN`
  without `ON`;
- `SELECT *` left-then-right visible column expansion and duplicate labels;
- explicit qualified projections with aliases and schema qualifiers;
- integer equality joins, string equality joins under existing ASCII
  case-insensitive collation support, and `NULL` non-matching for `=`;
- existing `WHERE`, `ORDER BY`, `LIMIT`, and `OFFSET` subsets over joined
  sources;
- `ORDER BY` explicit select-item alias;
- `LIMIT 0`, exact row counts, and row counts larger than the joined set;
- warning count, affected rows, found-row count, and following `ROW_COUNT()`;
- missing default schema, unknown schema, unknown table, reserved names,
  duplicate source alias, unknown and ambiguous columns in field, `ON`, `WHERE`,
  and `ORDER BY` contexts;
- after table rename and after drop;
- temporary table shadowing in joined reads;
- reopen persistence and independent file-backed handles;
- no `.mylite` preamble mutation.

Existing parser, lexer, runtime lifecycle, catalog, storage, temporary table,
qualified-column, predicate, order/limit, aggregate, insert-select, and update
tests must continue passing.
