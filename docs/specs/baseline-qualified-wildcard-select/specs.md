# Baseline Qualified Wildcard SELECT

## Summary

This phase extends descriptor-backed `SELECT` projection with qualified
wildcards for supported base-table and current two-source joined reads:

```sql
SELECT table_name.* FROM table_name ...
SELECT schema_name.table_name.* FROM schema_name.table_name ...
SELECT alias.* FROM table_name [AS] alias ...
SELECT alias.*, other_alias.column_name FROM left_table AS alias JOIN right_table AS other_alias ...
```

Qualified wildcards expand to the visible descriptor columns of the named
source in descriptor ordinal order. They may appear with other select-list
items. The feature does not add new source shapes, expression projection,
derived tables, CTEs, subqueries, view expansion, protocol-grade metadata, or
unqualified `*` inside mixed select lists.

## Compatibility Authority

- Official MySQL 8.4 documentation:
  - `SELECT` statement select-list `*` shorthand:
    <https://dev.mysql.com/doc/refman/8.4/en/select.html>
  - Identifier qualifiers:
    <https://dev.mysql.com/doc/refman/8.4/en/identifier-qualifiers.html>
  - Invisible columns:
    <https://dev.mysql.com/doc/refman/8.4/en/invisible-columns.html>
- Observed MySQL 8.4.9 runtime behavior, captured in
  `packages/libmylite/tests/mysql_baseline_qualified_wildcard_select_expectations.sh`.

The MySQL 8.4.9 probe for this phase used:

```sql
SELECT VERSION();
DROP DATABASE IF EXISTS mylite_qualified_wildcard_probe;
CREATE DATABASE mylite_qualified_wildcard_probe;
USE mylite_qualified_wildcard_probe;
CREATE TABLE t(id INT NOT NULL, n INT NULL, hidden INT INVISIBLE);
CREATE TABLE u(id INT NOT NULL, t_id INT NOT NULL, v INT NULL);
INSERT INTO t(id,n,hidden) VALUES (1,10,100),(2,NULL,200);
INSERT INTO u VALUES (7,1,70),(8,2,80);

SELECT t.* FROM t ORDER BY t.id;
SELECT mylite_qualified_wildcard_probe.t.*
  FROM mylite_qualified_wildcard_probe.t
  ORDER BY mylite_qualified_wildcard_probe.t.id;
SELECT a.* FROM t AS a ORDER BY a.id;
SELECT id, t.* FROM t ORDER BY id;
SELECT a.*, u.v FROM t AS a JOIN u ON a.id = u.t_id ORDER BY u.id;
SELECT a.*, b.* FROM t AS a, u AS b WHERE a.id = b.t_id ORDER BY b.id;
SELECT @@warning_count, ROW_COUNT();
```

Observed expectations:

- `table.*`, `schema.table.*`, and `alias.*` expand visible columns only;
- invisible descriptor columns are omitted from wildcard expansion while
  remaining explicitly referenceable by prior qualified-column slices;
- selected column labels are the expanded descriptor column names, allowing
  duplicate labels when another selected item names the same column;
- qualified wildcards may appear before or after ordinary selected columns;
- after a source is aliased, `table.*` and `schema.table.*` no longer name that
  source. MySQL reports `1051 (42S02) Unknown table '<qualifier>'`;
- `qualified.* AS alias` is a syntax error;
- successful supported selects report no warnings and following `ROW_COUNT()`
  is `-1`.

## Ownership Boundaries

- Public API: unchanged. Qualified wildcard selects return ordinary row
  `mylite_result` objects.
- Statement context: unchanged. Successful selects keep existing affected-row,
  warning, and found-row state behavior.
- Parser/AST: add a MyLite-owned qualified wildcard AST node containing the
  source qualifier and the `*` token. The parser admits it only as a select
  item without an alias.
- Analyzer/planner: resolve the source qualifier against MyLite table
  descriptors and table aliases, then expand visible descriptor columns through
  the existing planned-select column list.
- Catalog: descriptors remain authoritative for logical source names, aliases,
  visibility, physical table names, and physical column names. SQLite schema
  text is not consulted.
- Result builder: uses the existing descriptor column metadata and labels for
  every expanded column.
- Storage/VFS/file format: no change. The `.mylite` preamble and shifted SQLite
  payload invariants are unaffected.
- SQLite: generated SQL remains descriptor-built and executes through public
  prepare/step/finalize APIs. No SQLite fork patch or new extension point is
  required.

## Syntax

In scope:

```ebnf
qualified_wildcard:
    table_or_alias_name "." "*"
  | schema_name "." table_name "." "*"

select_item:
    existing_supported_select_item
  | qualified_wildcard
```

`qualified_wildcard` may be one item in a comma-separated select list. It does
not admit `AS alias` or a bare alias. Unqualified `*` remains supported by the
existing whole-select `SELECT *` grammar; mixed forms such as `SELECT *, t.*`
are deferred in this slice.

### MyLite Lemon-Syntax Snippet

The intended grammar shape is:

```lemon
select_item(A) ::= qualified_wildcard(B). {
    A = mylite_sql_parser_make_select_item(state, B, NULL);
}

qualified_wildcard(A) ::= qualified_identifier(Q) DOT STAR(S). {
    A = mylite_sql_parser_make_qualified_wildcard(state, Q, S);
}
```

This snippet is independently authored for MyLite and is not copied from MySQL
grammar.

## Resolution Semantics

Qualified wildcard source matching follows the current descriptor qualifier
policy:

- one-part qualifiers match the source alias when present;
- one-part qualifiers match the resolved table name when the source has no
  alias;
- two-part qualifiers match `schema.table` only when the source has no alias;
- after a source is aliased, the original table name and schema-qualified table
  name are hidden for wildcard qualification;
- source, alias, schema, and table comparisons use the current ASCII
  case-insensitive descriptor-name policy used by qualified column references;
- more than two qualifier parts are unsupported for this slice.

For a single-source `SELECT`, the qualifier is matched against that selected
source. For the current two-source joined envelope, the qualifier must match
exactly one joined source. Duplicate joined source aliases and duplicate
unaliased source references remain rejected by existing joined-source planning.

Expansion appends the matching source's visible descriptor columns in descriptor
ordinal order. Invisible columns are skipped, matching MySQL `*` behavior and
the existing MyLite unqualified wildcard policy.

## Physical SQL

Qualified wildcard planning reuses existing planned-select physical SQL. It
does not interpolate the wildcard text into SQLite SQL. Each expanded logical
column becomes the same physical projection shape already used for descriptor
column references:

```sql
SELECT "_mylite_s0"."_mylite_col_<column_id>", ...
FROM "_mylite_user_table_<table_id>" AS "_mylite_s0"
```

Physical table and column identifiers are stable MyLite-owned names and are
quoted. Predicate, ordering, and limit values keep existing bound-parameter
handling.

## Diagnostics

Expected diagnostics for this slice:

| Case | Diagnostic |
| --- | --- |
| Syntax outside admitted grammar | existing parse error `1064` / `42000` |
| `qualified.* AS alias` | parse error `1064` / `42000` |
| Missing default schema for unqualified source table | existing `1046` / `3D000` |
| Unknown source schema or table | existing table-resolution diagnostics |
| Reserved source schema/table name | existing reserved-name diagnostics |
| Unknown qualified wildcard source | `1051` / `42S02`, `Unknown table '<qualifier>'` |
| Unsupported source kind | existing unsupported diagnostic |
| Qualified wildcard in `DISTINCT` projection | existing `SELECT DISTINCT` single-column unsupported diagnostic |
| Unsupported mixed unqualified `*` select lists | existing parse or unsupported diagnostic |
| Physical SQLite failure | existing SQLite runtime diagnostic |
| Allocation failure | existing allocation diagnostic |

Successful supported statements return result rows, `affected_rows == 0`,
`warning_count == 0`, and following `ROW_COUNT() == -1`.

## Tests

Add MySQL-runtime-verified expectations for:

- `table.*`, `schema.table.*`, and `alias.*`;
- visible-only wildcard expansion with an invisible source column;
- result labels and duplicate labels for `SELECT id, t.*`;
- qualified wildcard mixed with ordinary selected columns;
- inner join, comma join, and left join source expansion;
- alias hiding of original table and schema-qualified table names;
- unknown qualified wildcard source names;
- syntax rejection for `qualified.* AS alias`; and
- warning count and `ROW_COUNT()` after successful selects.

Add fast C coverage by extending existing parser and qualified-column runtime
tests:

- parse representative qualified wildcard items and reject aliasing;
- execute single-source, schema-qualified, aliased, mixed projection, joined,
  and invisible-column expansion cases;
- assert result labels, values, affected rows, warning counts, reopen
  persistence, rename/drop behavior, independent file-backed handles, and
  `.mylite` preamble preservation; and
- preserve existing parser, qualified column, join, result metadata, storage,
  VFS, and workflow tests.
