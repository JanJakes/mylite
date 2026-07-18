# Baseline Comma Join SELECT

## Status

This feature specifies the next descriptor-backed table-reference slice:
two-source comma joins in `SELECT`. MyLite accepts `FROM left_source,
right_source` as a MySQL-compatible cartesian inner join over two readable
base-table descriptors, with the existing projection, predicate, ordering, and
limit envelope used by the current two-source explicit join path.

The plain joined `SELECT` slice also admits one narrow same-query-block
descriptor column equality predicate in `WHERE`, such as `WHERE lefts.k =
rights.k`, so old-style comma joins can express their common join condition
without relying on `ON`.

This is intentionally not full MySQL table-reference support. It does not
implement more than two sources, mixed comma/explicit join precedence,
parenthesized table references, `USING`, natural joins, derived tables,
arbitrary column-to-column predicates, or optimizer behavior.

## Sources

- MyLite README architecture: `README.md`
- MyLite engineering standards:
  `docs/architecture/engineering-standards.md`
- Existing baseline inner-join, left-join, select, predicate, ordering, and
  catalog specs under `docs/specs/`
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

CREATE DATABASE mylite_comma_probe;
USE mylite_comma_probe;
CREATE TABLE lefts(id INT NOT NULL, k INT NULL, v INT NULL, name VARCHAR(20));
CREATE TABLE rights(id INT NOT NULL, k INT NULL, w INT NULL, name VARCHAR(20));
INSERT INTO lefts VALUES
  (1,10,100,'alpha'),(2,20,200,'Beta'),(3,NULL,300,'none');
INSERT INTO rights VALUES
  (7,10,700,'ALPHA'),(8,10,800,'beta'),(9,NULL,900,'none');

SELECT * FROM lefts, rights WHERE lefts.k = rights.k ORDER BY rights.id;
SELECT l.id, r.w FROM lefts AS l, rights AS r
  WHERE l.k = r.k AND l.v = 100 ORDER BY r.w LIMIT 1 OFFSET 1;
SELECT l.id, r.id FROM lefts l, rights r
  WHERE l.id = 1 ORDER BY r.id LIMIT 2;
SELECT lefts.id, rights.id FROM lefts, rights
  WHERE lefts.name = rights.name ORDER BY rights.id;
SELECT l.id AS left_id, r.id AS right_id FROM lefts AS l, rights AS r
  WHERE l.k = r.k ORDER BY right_id DESC;
DO 0;
SELECT l.id FROM lefts AS l, rights AS r
  WHERE l.k = r.k ORDER BY r.id LIMIT 0;
SELECT @@warning_count, ROW_COUNT();
```

Observed behavior:

- `FROM lefts, rights` is accepted and produces a cartesian product before
  `WHERE` filtering.
- `WHERE lefts.k = rights.k` filters the product like an inner equality join.
  `NULL = NULL` does not match for the admitted `=` predicate.
- `SELECT *` returns visible columns from the left source followed by visible
  columns from the right source. Duplicate result labels are preserved.
- Source aliases use the same rules as explicit joins. Aliases hide original
  table names, and duplicate aliases report `1066 / 42000`.
- Unqualified references that match both sources report `1052 / 23000` in the
  active clause.
- Unknown selected, predicate, order, and source names report the same
  clause-aware diagnostics as the existing explicit join path.
- Successful row-returning comma joins leave `@@warning_count = 0` and a
  following `ROW_COUNT()` returns `-1`.

## Scope

In scope:

- `SELECT column_list FROM left_source, right_source ...`;
- `SELECT * FROM left_source, right_source ...`;
- exactly two source tables;
- persistent and shadowing session temporary readable table descriptors;
- unqualified and schema-qualified table names using the existing selected
  default-schema policy;
- optional `AS alias` and bare source aliases;
- existing validated/no-op source index-hint syntax on each admitted table
  source;
- descriptor projection and wildcard expansion over both sources;
- the existing descriptor `WHERE`, `ORDER BY`, and `LIMIT` subsets over joined
  sources;
- one additional plain joined `SELECT` same-query-block `WHERE left_column =
  right_column` descriptor predicate over supported same-family integer-family
  columns or supported ASCII string-family columns;
- existing result, warning-count, `ROW_COUNT()`, temporary shadowing,
  persistence, and file-format behavior from descriptor-backed `SELECT`.

Out of scope:

- more than two sources, such as `FROM a, b, c`;
- mixed comma and explicit joins, such as `FROM a, b JOIN c ON ...`;
- outer comma-join nesting or MySQL's full comma-versus-explicit-join
  precedence behavior;
- parenthesized table references, ODBC escape joins, derived tables, lateral
  derived tables, table functions, CTEs, and partitions;
- `USING`, natural joins, right/full outer joins, and `STRAIGHT_JOIN` as a
  table-reference join operator;
- qualified wildcards outside the later limited descriptor-backed
  [baseline qualified wildcard SELECT](../baseline-qualified-wildcard-select/specs.md)
  projection slice;
- column-to-column `WHERE` predicates other than `=`;
- mixed-type column-to-column comparisons and MySQL conversion/warning
  behavior for those comparisons;
- decimal, approximate, temporal, binary-string, bit, enum, set, JSON, or full
  Unicode column-to-column comparison parity;
- general expression projection, expression predicates, arbitrary `ON`,
  column-to-column `WHERE` predicates in grouped aggregate `SELECT`, joined
  `UPDATE`, or joined `DELETE`, grouped comma joins beyond the existing
  explicit-join aggregate envelope, `DISTINCT` joins, `SQL_CALC_FOUND_ROWS`,
  locking behavior changes, or arbitrary SQLite pass-through.

## Ownership Boundary

- The public API remains unchanged. `mylite_execute()` owns public call
  validation, result ownership, misuse behavior, and cleanup.
- Statement context owns diagnostics, warning count, result status,
  `ROW_COUNT()`, and `FOUND_ROWS()` state.
- Lexer/parser/AST own comma table-reference syntax admission and source spans.
  They do not read descriptors.
- Analyzer/planner owns schema resolution, temporary-table shadowing,
  descriptor loading, duplicate source-reference diagnostics, projection
  resolution, same-scope column-equality validation, predicate planning,
  ordering, limiting, and unsupported-shape diagnostics.
- Catalog modules remain authoritative for logical schema, table, column, and
  index metadata. SQLite schema text is not used for user-visible resolution.
- SQLite owns physical cartesian join execution and `WHERE` filtering over
  generated stable physical table names. MyLite generates quoted SQL from
  descriptors and binds scalar predicate and limit values.
- Storage/VFS and the `.mylite` preamble/shifted-payload invariants are
  unchanged. This feature does not mutate the catalog or user data.

## Supported SQL Grammar

The feature admits one top-level statement per `mylite_execute()` call.

Independently authored MyLite Lemon-syntax snippets:

```lemon
select_statement ::=
    SELECT select_modifiers select_item_list FROM comma_table_sources
    where_clause_opt group_clause_opt having_clause_opt select_order_clause_opt
    limit_clause_opt select_locking_clause_opt.

select_statement ::=
    SELECT select_modifiers STAR FROM comma_table_sources
    where_clause_opt group_clause_opt having_clause_opt select_order_clause_opt
    limit_clause_opt select_locking_clause_opt.

comma_table_sources ::= table_source COMMA table_source.

table_source ::= table_name table_alias_opt table_index_hints_opt.

predicate_atom ::= qualified_identifier EQUAL qualified_identifier.
```

The AST represents `comma_table_sources` as the existing two-source
`MYLITE_SQL_AST_FROM_JOIN` node with inner join kind and no `ON` child. Runtime
keeps the source count at two for this slice.

## Name Resolution

Source table resolution reuses the explicit join path:

1. Resolve the effective schema from an explicit qualifier or the selected
   default schema.
2. Reject reserved `_mylite_*` schema or table names before SQL generation.
3. Prefer a session temporary descriptor over a persistent descriptor in the
   same schema.
4. Reject missing schemas, missing tables, and unsupported object kinds before
   physical SQL is generated.

The source reference name is the alias when present, otherwise the table name.
Two source reference names in the same comma scope must be distinct under
MyLite's current ASCII case-insensitive descriptor comparison.

Column resolution for selected columns, `WHERE`, and `ORDER BY` matches the
existing explicit join path:

- one-part `column` resolves only when exactly one joined source has a matching
  descriptor column;
- if both sources have a matching descriptor column, the active clause reports
  ambiguous-column `1052 / 23000`;
- two-part `source.column` resolves through the source alias, or through the
  table name when the source is unaliased;
- aliases hide original table qualifiers;
- three-part `schema.table.column` resolves only for an unaliased matching
  source;
- unknown names report `1054 / 42S22` in the active clause.

The new `WHERE left_column = right_column` resolution uses the same joined
source scope for both sides.

## Semantics

`FROM left_source, right_source` is planned as a descriptor-backed inner
cartesian join. MyLite generates standard SQLite `JOIN` SQL without an `ON`
condition and lets SQLite evaluate the product and any supported `WHERE`
predicate.

The admitted same-scope column equality predicate:

- supports `=` only;
- supports same-family integer-family columns and supported ASCII
  string-family columns;
- uses normal SQL three-valued equality semantics, so `NULL = NULL` is not
  true;
- uses MyLite's registered limited Unicode `utf8mb4_0900_ai_ci` collation for admitted
  string-family values;
- rejects mixed-type, unsupported-type, and non-`=` column-to-column predicates
  with deterministic MyLite unsupported diagnostics.

`ORDER BY`, `LIMIT`, `@@sql_select_limit`, warning count, row count, and result
metadata reuse the existing descriptor-backed joined `SELECT` conventions. For
duplicate sort keys, MyLite claims only the sorted key behavior covered by
tests and does not promise tie order.

## Physical SQLite Handling

Generated SQL uses descriptor-owned stable physical names and internal source
aliases:

```sql
SELECT "_mylite_s0"."id", "_mylite_s1"."w"
FROM "_mylite_user_table_<left_id>" AS "_mylite_s0"
JOIN "_mylite_user_table_<right_id>" AS "_mylite_s1"
WHERE "_mylite_s0"."k" = "_mylite_s1"."k"
ORDER BY "_mylite_s1"."w" ASC
LIMIT ?1 OFFSET ?2
```

All generated identifiers are quoted. Scalar predicate and limit values remain
bound parameters. Column-to-column predicates use quoted descriptor column
references rather than interpolated user text. The implementation uses public
SQLite prepare/bind/step APIs and requires no SQLite fork patch.

## Diagnostics

The supported subset uses the existing MyLite/MySQL-shaped diagnostics:

- syntax errors, unsupported source count, mixed comma/explicit joins,
  parenthesized references, derived tables, `USING`, natural joins, right/full
  joins, `STRAIGHT_JOIN`, unsupported modifiers, unsupported `GROUP BY` /
  `HAVING` / `DISTINCT` combinations, and unsupported locking interactions use
  the current parse or unsupported diagnostics for joined `SELECT`;
- missing default schema reports `1046 / 3D000`;
- unknown schema reports `1049 / 42000`;
- unknown table reports `1146 / 42S02`;
- duplicate source aliases report `1066 / 42000`;
- ambiguous selected, predicate, or order columns report `1052 / 23000`;
- unknown selected, predicate, or order columns report `1054 / 42S22`;
- unsupported same-scope column-to-column operators report a deterministic
  MyLite unsupported diagnostic;
- unsupported same-scope column-to-column type families report a deterministic
  MyLite unsupported diagnostic;
- physical SQLite failures and allocation failures keep the existing internal
  error and out-of-memory behavior.

## Test Plan

Add MySQL-runtime expectations and focused C coverage for:

- `SELECT * FROM lefts, rights WHERE lefts.k = rights.k` column order,
  duplicate labels, row values, warning count, and `ROW_COUNT()`;
- alias projection with `WHERE` equality, existing literal predicates,
  `ORDER BY`, `LIMIT`, `LIMIT 0`, and `@@sql_select_limit`;
- cartesian product with no column-equality predicate;
- string-family column equality using registered collation behavior;
- schema-qualified sources and columns;
- temporary table shadowing, close/reopen persistence, table rename, and drop
  diagnostics;
- independent file-backed handles with independent comma-join row state;
- missing default schema, unknown schema, unknown table, duplicate aliases,
  alias hiding, ambiguous selected/predicate/order columns, and unknown
  selected/predicate/order columns;
- unsupported `a,b,c`, mixed comma/explicit joins, `USING`, non-`=`
  column-to-column predicates, mixed-type column equality, expression table
  sources, and qualified wildcard forms outside the later limited descriptor-backed
  projection slice where currently rejected;
- parser coverage for admitted comma-source AST shape and rejected broader
  table-reference forms;
- existing parser, explicit join, left join, row values, select-where,
  select-order-limit, delete, update, file-format, and workflow checks.

## Performance and Storage Notes

The supported path remains close to SQLite's optimal execution path for the
implemented feature. MyLite does not materialize the cartesian product or
filtered row set in process memory. It resolves descriptors and emits a
standard SQLite query over stable physical tables, then builds the public result
object from SQLite's stepped rows as existing descriptor-backed `SELECT` does.

No catalog generation, descriptor version, table descriptors, SQLite schema
generation, storage preamble, or VFS behavior changes are introduced.
