# Table-backed SELECT core

## Scope

This feature makes the first user-table `SELECT` form executable. It is the
Task 15 read counterpart to the current `CREATE TABLE`, `DROP TABLE`,
`INSERT ... VALUES`, and `INSERT ... SET` table-storage work.

In scope:

- `SELECT select_list FROM table_name [alias]`
- schema-qualified table names and selected-schema target resolution
- one base table only
- optional table aliases using `AS alias` or a bare identifier alias
- unqualified and qualified column references in the select list
- expression aliases for supported column-reference projections, using `AS`
  or a bare alias where MySQL accepts one
- identifier-quoted and string-quoted projection aliases
- `*`, `table.*`, `schema.table.*`, and alias-qualified wildcard expansion
- output column labels, duplicate labels, table metadata, and origin metadata
  for base-table columns
- invisible-column behavior for wildcard expansion
- deterministic diagnostics for missing schemas, missing tables, unknown
  columns, unknown qualifiers, and alias-hidden base names

Out of scope:

- `SELECT` without `FROM`, except for behavior already supported by existing
  MyLite code
- `FROM DUAL`, except for behavior already supported by existing MyLite code
- multiple table references, comma joins, explicit joins, derived tables,
  table functions, lateral references, and parenthesized table references
- `WHERE`
- `GROUP BY`, `HAVING`, window clauses, and aggregate functions
- `ORDER BY`, `LIMIT`, and `OFFSET`
- `DISTINCT`, `DISTINCTROW`, and `ALL`
- `WITH` and `WITH RECURSIVE`
- subqueries
- locking clauses
- `SELECT ... INTO`
- optimizer hints, index hints, partitions, and SELECT modifiers
- parameter markers and prepared statement marker binding
- expression operator expansion, function calls, aggregate calls, expression
  type inference, and broad expression evaluation
- privilege-sensitive metadata filtering
- warning records

Task 15 execution is intentionally limited to wildcard expansion and column
reference projections over one base table. The existing expression parser may
continue to accept other expression nodes in a select item, but table-backed
execution should reject unsupported projection expressions deterministically
until the expression tasks specify and implement them.

## Sources

- MySQL 8.4 Reference Manual, `SELECT` statement:
  https://dev.mysql.com/doc/refman/8.4/en/select.html
- MySQL 8.4 Reference Manual, Identifier Qualifiers:
  https://dev.mysql.com/doc/refman/8.4/en/identifier-qualifiers.html
- MySQL 8.4 Reference Manual, Problems with Column Aliases:
  https://dev.mysql.com/doc/refman/8.4/en/problems-with-alias.html
- MySQL 8.4 Reference Manual, Invisible Columns:
  https://dev.mysql.com/doc/refman/8.4/en/invisible-columns.html
- Existing MyLite specs:
  - `docs/specs/schema-lifecycle/specs.md`
  - `docs/specs/core-metadata-catalog/specs.md`
  - `docs/specs/column-attributes/specs.md`
  - `docs/specs/create-table-base-execution/specs.md`
  - `docs/specs/insert-values/specs.md`
  - `docs/specs/insert-set/specs.md`
- Observed MySQL 8.4.9 runtime behavior from Docker container
  `mylite-mysql-849`, using `docker exec -i mylite-mysql-849 mysql -uroot`.
  Metadata observations used `mysql --column-type-info -vvv`.

This specification is independently authored from official documentation and
observed MySQL runtime behavior. It does not copy MySQL grammar,
documentation prose, or implementation sources.

## MySQL 8.4.9 behavior summary

### Test fixture used for runtime verification

Runtime probes used this fixture:

```sql
DROP DATABASE IF EXISTS mylite_task15_select_core;
CREATE DATABASE mylite_task15_select_core;
USE mylite_task15_select_core;

CREATE TABLE t (
  a INT,
  b VARCHAR(10),
  hidden INT INVISIBLE,
  CamelCase INT
);

INSERT INTO t (a, b, hidden, CamelCase)
VALUES (1, 'one', 99, 7), (2, 'two', 88, 8);
```

The result order without `ORDER BY` is not a semantic guarantee. These probes
observe MySQL's current simple-table order only to make the examples readable;
implementation tests should compare unordered row sets unless they introduce a
separate ordering feature.

### Schema and table resolution

An unqualified table name uses the selected default schema. If no default
schema is selected, `SELECT * FROM t` fails with:

- 1046 / SQLSTATE `3D000`: `No database selected`

A schema-qualified table name does not require a selected default schema:

```sql
SELECT * FROM mylite_task15_select_core.t;
```

The query returns visible columns `a`, `b`, and `CamelCase`.

Missing targets use these diagnostics:

- missing explicit schema: 1049 / `42000`,
  `Unknown database 'missing_task15_schema'`
- missing table in an existing schema: 1146 / `42S02`,
  `Table 'mylite_task15_select_core.missing_t' doesn't exist`

### Table aliases

MySQL accepts both table alias spellings:

```sql
SELECT alias.* FROM t AS alias;
SELECT alias.* FROM t alias;
```

Quoted identifier aliases are accepted:

```sql
SELECT `quoted alias`.* FROM t AS `quoted alias`;
```

When a table alias is present, the base table name is hidden for column and
wildcard qualification:

- `SELECT t.a FROM t AS alias` fails with 1054 / `42S22`,
  `Unknown column 't.a' in 'field list'`
- `SELECT t.* FROM t alias` fails with 1051 / `42S02`,
  `Unknown table 't'`
- `SELECT mylite_task15_select_core.t.* FROM
  mylite_task15_select_core.t AS alias` fails with 1051 / `42S02`,
  `Unknown table 'mylite_task15_select_core.t'`

The alias itself remains valid:

```sql
SELECT alias.a FROM t AS alias;
SELECT alias.* FROM mylite_task15_select_core.t AS alias;
```

On the verified Linux MySQL runtime, table and alias qualifiers are
case-sensitive while column-name lookup is case-insensitive:

- ``SELECT camelcase, CAMELCASE, `CamelCase` FROM t`` resolves all three
  projections to the `CamelCase` column.
- `SELECT T.a FROM t` and `SELECT ALIAS.a FROM t AS alias` fail with unknown
  column diagnostics.

This matches the existing MyLite catalog direction of byte-preserving,
case-sensitive schema/table names and case-insensitive column matching.

### Wildcard expansion

The unqualified wildcard expands all visible columns from the single table in
catalog ordinal order:

```sql
SELECT * FROM t;
```

Result columns:

1. `a`
2. `b`
3. `CamelCase`

The invisible column `hidden` is omitted.

Qualified wildcards over the visible table identity behave the same:

```sql
SELECT t.* FROM t;
SELECT mylite_task15_select_core.t.* FROM mylite_task15_select_core.t;
SELECT mylite_task15_select_core.t.* FROM t;
SELECT alias.* FROM t AS alias;
```

Each returns `a`, `b`, and `CamelCase`, omitting `hidden`.

An explicitly referenced invisible column is selectable:

```sql
SELECT hidden FROM t;
```

The result contains the `hidden` column values.

MySQL allows an unqualified `*` before other select-list items but not after
another expression:

- `SELECT *, a FROM t` is accepted.
- `SELECT a, * FROM t` is a syntax error.

Qualified wildcards can appear before or after other select-list items:

```sql
SELECT a, t.* FROM t;
SELECT t.*, a FROM t;
SELECT mylite_task15_select_core.t.*, a FROM mylite_task15_select_core.t;
```

Task 15 should support qualified wildcard select-list items. It may either
support MySQL's `SELECT *, a` special case now or defer mixed unqualified `*`
until broader projection-list work, but it must not accept `SELECT a, * FROM t`
as a valid Task 15 shape.

Unknown wildcard qualifiers use table diagnostics, not column diagnostics:

- `SELECT missing_alias.* FROM t` fails with 1051 / `42S02`,
  `Unknown table 'missing_alias'`
- `SELECT missing_schema.t.* FROM t` fails with 1051 / `42S02`,
  `Unknown table 'missing_schema.t'`

### Column references

Column references may be unqualified, table-qualified, or
schema-table-qualified:

```sql
SELECT a FROM t;
SELECT t.a FROM t;
SELECT mylite_task15_select_core.t.a FROM mylite_task15_select_core.t;
SELECT mylite_task15_select_core.t.a FROM t;
```

Column-name lookup is case-insensitive. The output label for an unaliased
explicit column reference uses the final identifier spelling from the select
list, not necessarily the catalog's original spelling:

```sql
SELECT camelcase, CAMELCASE, `CamelCase`, t.CamelCase FROM t;
```

The client-visible labels are `camelcase`, `CAMELCASE`, `CamelCase`, and
`CamelCase`. Origin metadata still points to the same base column.

Unknown column or column qualifier diagnostics:

- `SELECT missing_col FROM t` fails with 1054 / `42S22`,
  `Unknown column 'missing_col' in 'field list'`
- `SELECT missing_alias.a FROM t` fails with 1054 / `42S22`,
  `Unknown column 'missing_alias.a' in 'field list'`
- `SELECT other_schema.t.a FROM t` fails with 1054 / `42S22`,
  `Unknown column 'other_schema.t.a' in 'field list'`
- `SELECT mylite_task15_select_core.t.a FROM
  mylite_task15_select_core.t AS alias` fails with 1054 / `42S22`,
  `Unknown column 'mylite_task15_select_core.t.a' in 'field list'`

There is no ambiguous unqualified column case inside Task 15's one-table scope:
MyLite table DDL rejects duplicate column names case-insensitively, and joins
are deferred. Ambiguous column diagnostics should be covered by the join tasks.

### Projection aliases and duplicate labels

Column projections accept aliases using `AS`:

```sql
SELECT a AS x FROM t;
```

For identifier aliases, `AS` is optional:

```sql
SELECT a x FROM t;
```

MySQL also accepts quoted projection aliases using identifier or string quoting
characters:

```sql
SELECT a AS `backtick alias` FROM t;
SELECT a AS 'single alias' FROM t;
SELECT a 'single alias' FROM t;
SELECT a AS "double alias" FROM t;
```

The observed default SQL mode treats the double-quoted alias as a string-quoted
alias. Future SQL-mode work must revisit double quotes when `ANSI_QUOTES` is
implemented.

Unquoted reserved words are not valid aliases after `AS`; for example,
`SELECT a AS SELECT FROM t` is a syntax error. Without `AS`, a bare alias is
only safe when the following token is an identifier or string alias token, not
a clause keyword such as `FROM`.

Duplicate output labels are accepted and preserved:

```sql
SELECT a AS x, b AS x FROM t;
```

The result has two columns both named `x`.

### Result metadata

The MySQL CLI with `--column-type-info -vvv` verified these metadata fields:

- `Field` is the client-visible output label.
- `Catalog` is `def`.
- `Database` is the resolved schema.
- `Table` is the visible table identity for the result column.
- `Org_table` is the underlying base table.
- Type, collation, length, decimals, and flags reflect the base column.

For `SELECT a AS x, b AS x FROM t`, both output labels are `x`, both
`Database` values are `mylite_task15_select_core`, both `Table` values are
`t`, and both `Org_table` values are `t`.

For `SELECT alias.* FROM t AS alias`, wildcard output labels are the base
column names, `Table` is `alias`, and `Org_table` is `t`.

For ``SELECT a x, b `spaced alias`, hidden AS `hidden alias` FROM t``, output
labels are `x`, `spaced alias`, and `hidden alias`, and the columns retain
their base table metadata.

The CLI output used here does not expose the origin column-name field, but the
MySQL protocol field model has enough origin information for clients. MyLite
should keep statement-owned metadata for both client-visible labels and origin
identity so protocol adapters can expose MySQL-compatible fields later.

## MyLite behavior

### Parser and AST

The current parser accepts a narrow `SELECT` shape, but select items cannot
record aliases, wildcard qualifiers, or table aliases. Task 15 needs explicit
AST representation for those concepts.

Recommended statement shape:

1. select list
2. from table

Recommended `FROM` table shape:

1. table name
2. optional table alias

Recommended select item shape:

1. projection expression or wildcard node
2. optional projection alias

The wildcard node should distinguish:

- unqualified `*`
- one-part qualified `table_or_alias.*`
- two-part qualified `schema.table.*`

The parser must preserve source spelling for explicit column-reference labels.
For example, `SELECT camelcase FROM t` should expose the output label
`camelcase` even when the catalog column is `CamelCase`. Wildcard output labels
come from catalog column names.

### Lemon grammar snippets

These snippets describe MyLite's intended Task 15 grammar. They are not copied
from MySQL grammar.

```lemon
select_statement ::= SELECT select_list FROM single_table_reference.

single_table_reference ::= table_name opt_table_alias.

opt_table_alias ::= .
opt_table_alias ::= table_alias.
opt_table_alias ::= AS table_alias.

table_alias ::= identifier.

table_name ::= qualified_identifier.

select_list ::= select_item.
select_list ::= select_list COMMA select_item.

select_item ::= select_projection.
select_item ::= expression AS projection_alias.
select_item ::= expression identifier.
select_item ::= expression STRING.

select_projection ::= expression.
select_projection ::= STAR.
select_projection ::= qualified_wildcard.

qualified_wildcard ::= identifier DOT STAR.
qualified_wildcard ::= identifier DOT identifier DOT STAR.

projection_alias ::= identifier.
projection_alias ::= STRING.
```

The implementation should avoid treating clause keywords as bare aliases.
Since Task 15 has no clauses after the table reference, this matters most for
the select list: `SELECT a FROM t` must parse `FROM` as the clause boundary,
not as an alias for `a`.

The following MySQL grammar surface is intentionally deferred and should not be
accepted as Task 15 support:

```lemon
/* Deferred: SELECT modifiers and duplicate elimination. */
select_statement ::= SELECT select_modifier_list select_list FROM single_table_reference.

/* Deferred: common table expressions. */
select_statement ::= WITH cte_list SELECT select_list FROM single_table_reference.

/* Deferred: joins and multi-table table references. */
single_table_reference ::= table_reference JOIN table_reference join_condition.
single_table_reference ::= table_reference COMMA table_reference.

/* Deferred: filtering, grouping, sorting, and limiting. */
select_statement ::= SELECT select_list FROM single_table_reference WHERE expression.
select_statement ::= SELECT select_list FROM single_table_reference GROUP BY group_list.
select_statement ::= SELECT select_list FROM single_table_reference ORDER BY order_list.
select_statement ::= SELECT select_list FROM single_table_reference LIMIT limit_value.

/* Deferred: locking and SELECT INTO. */
select_statement ::= SELECT select_list FROM single_table_reference locking_clause.
select_statement ::= SELECT select_list INTO select_into_target FROM single_table_reference.

/* Deferred: partitions and index hints. */
single_table_reference ::= table_name PARTITION LPAREN identifier_list RPAREN opt_table_alias.
single_table_reference ::= table_name opt_table_alias index_hint_list.
```

### Runtime execution

Preparing a scoped table-backed `SELECT` should create a read-only statement
that executes against the SQLite physical table for the resolved MyLite table.
The implementation may either prepare a generated SQLite statement with stable
column aliases and metadata side tables, or use a custom statement wrapper.
The key requirement is that MyLite result values and metadata match the
documented MySQL behavior rather than leaking internal physical table names.

Schema and table resolution:

- `schema.table` targets the written schema.
- Unqualified `table` targets the selected default schema.
- A missing selected schema fails with `No database selected`.
- A missing explicit schema fails with `Unknown database 'schema'`.
- A missing table in an existing schema fails with
  `Table 'schema.table' doesn't exist`.
- User-table execution targets only base tables stored by MyLite's table
  catalog and physical SQLite table mapping.
- `INFORMATION_SCHEMA` system-view selects remain governed by the existing
  core metadata catalog path. Projection support over those system views is not
  part of Task 15 unless the implementation can share the same machinery
  without changing the user-table scope.

Table alias handling:

- Store the table alias after identifier unquoting when one is present.
- Use the alias as the only valid two-part qualifier for column and wildcard
  references when an alias exists.
- When no alias exists, accept the table name as a valid two-part qualifier.
- Accept `schema.table` qualifiers only when no table alias exists and both
  parts match the resolved schema and table identity.
- Match schema/table/alias qualifiers according to MyLite's byte-preserving,
  case-sensitive schema/table policy.

Column resolution:

- Load table column metadata from `__mylite_column_catalog` ordered by
  `ORDINAL_POSITION`.
- Match column names case-insensitively.
- For explicit column references, invisible columns are resolvable.
- For unaliased explicit column references, use the final identifier spelling
  from the query text as the output label.
- For aliased projections, use the alias text after identifier or string
  unquoting as the output label.
- Preserve duplicate output labels.

Wildcard expansion:

- Expand unqualified `*` to all visible columns in catalog ordinal order.
- Expand `table.*`, `schema.table.*`, and `alias.*` to all visible columns from
  the matched table identity in catalog ordinal order.
- Do not include invisible columns in wildcard expansion.
- Use catalog column names as output labels for wildcard-expanded columns.
- Reject unknown wildcard qualifiers with an unknown-table diagnostic.

Value production:

- Read values from the catalog-derived SQLite physical table name.
- Project columns in select-list order after wildcard expansion.
- The query is read-only and must not change affected rows or last insert id.
- Without `ORDER BY`, result row order is not a compatibility guarantee.

Diagnostics:

- Unknown explicit column references use the 1054-style
  `Unknown column 'name' in 'field list'` diagnostic.
- Unknown explicit column qualifiers also use the 1054-style column diagnostic.
- Unknown wildcard qualifiers use the 1051-style `Unknown table 'qualifier'`
  diagnostic.
- Ambiguous-column diagnostics are deferred to join support because the Task 15
  scope has only one base table and duplicate column names are rejected by DDL.

### Result metadata model

For every output column, MyLite should track:

- client-visible column label
- catalog name, currently `def`
- resolved schema name
- visible table name, meaning the table alias when present, otherwise the base
  table name
- origin schema name
- origin table name
- origin column name
- type/nullability/key/visibility details already available in
  `__mylite_column_catalog`

Current public C APIs can verify column count and labels through
`mylite_column_count()` and `mylite_column_name()`. If Task 15 adds public
origin-metadata accessors, they should be narrow, statement-owned, and
documented with explicit lifetime rules. If not, the implementation should
still keep internal result metadata so a future MySQL protocol layer and Task
23's broader result metadata work do not have to reverse-engineer origin data
from generated SQLite SQL.

### Explicit deferred behavior

MyLite intentionally documents these Task 15 boundaries:

- Non-column projection expressions in table-backed selects are deferred unless
  they are already supported without expanding expression semantics.
- Expression labels for arbitrary expressions are deferred to result metadata
  and expression work.
- Type conversion, expression warnings, collation-sensitive expression
  evaluation, function calls, aggregates, and subqueries are deferred.
- Information-schema projections beyond the existing `SELECT *` system-view
  path remain deferred.
- Privileges are not implemented; visibility is based only on MyLite catalog
  state.
- No ordering guarantee is introduced without `ORDER BY`.

## MySQL-runtime-verified expectations

Implementation tests should cover these MySQL 8.4.9 expectations:

| SQL or behavior | Expected MyLite-compatible outcome |
| --- | --- |
| `SELECT * FROM t` with selected schema | Returns visible columns in ordinal order; invisible columns omitted. |
| `SELECT * FROM t` with no selected schema | Fails with no-selected-schema diagnostic. |
| `SELECT * FROM schema.t` with no selected schema | Resolves and reads the qualified table. |
| `SELECT * FROM missing_schema.t` | Fails with unknown-database diagnostic. |
| `SELECT * FROM schema.missing_t` | Fails with table-does-not-exist diagnostic. |
| `SELECT t.* FROM t` | Expands visible columns from `t`. |
| `SELECT schema.t.* FROM schema.t` | Expands visible columns from the schema-qualified table. |
| `SELECT schema.t.* FROM t` while `schema` is selected | Expands visible columns from the resolved table. |
| `SELECT alias.* FROM t AS alias` | Expands visible columns and reports visible table metadata as `alias`. |
| `SELECT alias.* FROM t alias` | Same behavior as the `AS` alias form. |
| `SELECT t.* FROM t AS alias` | Fails with unknown-table diagnostic because the alias hides the base name. |
| `SELECT missing_alias.* FROM t` | Fails with unknown-table diagnostic. |
| `SELECT hidden FROM t` where `hidden` is invisible | Explicitly returns the invisible column. |
| `SELECT a AS x FROM t` | Returns label `x` over column `a`. |
| `SELECT a x FROM t` | Returns label `x`; bare identifier alias accepted. |
| ``SELECT a AS `spaced alias` FROM t`` | Returns label `spaced alias`. |
| `SELECT a AS 'single alias' FROM t` | Returns label `single alias`. |
| `SELECT a 'single alias' FROM t` | Returns label `single alias`. |
| `SELECT a AS x, b AS x FROM t` | Returns two output columns both labeled `x`. |
| ``SELECT camelcase, CAMELCASE, `CamelCase` FROM t`` | Resolves all to the same column but preserves written output labels. |
| `SELECT missing_col FROM t` | Fails with unknown-column diagnostic. |
| `SELECT missing_alias.a FROM t` | Fails with unknown-column diagnostic. |
| `SELECT t.a FROM t AS alias` | Fails with unknown-column diagnostic because the alias hides the base name. |
| single-table unqualified column references | Cannot be ambiguous in Task 15; join tasks cover ambiguous diagnostics. |
| `SELECT a, * FROM t` | MySQL syntax error; Task 15 must not accept it as supported. |
| `WHERE`, `ORDER BY`, `LIMIT`, joins, grouping, aggregates, subqueries, CTEs, and locking clauses | MySQL supports many of these, but MyLite Task 15 intentionally defers them. |

## Test plan

Parser tests:

- base table reference with selected schema
- schema-qualified table reference
- table alias with `AS`
- table alias without `AS`
- quoted identifier table alias
- projection aliases with `AS`
- bare identifier projection aliases
- backtick-quoted and string-quoted projection aliases
- duplicate projection labels
- unqualified `*`
- `table.*`
- `schema.table.*`
- mixed qualified wildcard and column-reference select-list items
- parse rejection or unsupported handling for `SELECT a, * FROM t`
- parse rejection or unsupported handling for deferred clauses:
  `WHERE`, `ORDER BY`, `LIMIT`, joins, grouping, aggregate calls, subqueries,
  CTEs, locking clauses, `SELECT ... INTO`, partitions, and index hints

Runtime tests:

- successful read through selected-schema table resolution
- successful read through schema-qualified table resolution without selected
  schema
- missing selected schema, missing explicit schema, and missing table
  diagnostics
- visible-column wildcard expansion order
- invisible-column omission from `*` and qualified wildcards
- explicit invisible-column selection
- `table.*`, `schema.table.*`, and `alias.*` qualifier resolution
- alias-hidden base table diagnostics for column and wildcard references
- unknown column, unknown column qualifier, and unknown wildcard qualifier
  diagnostics
- case-insensitive column lookup with output-label spelling preserved
- duplicate output labels preserved
- metadata labels, resolved schema, visible table name, origin table name, and
  origin column identity for unaliased, aliased, and wildcard projections
- read-only side effects: affected rows remains read-only/unknown, last insert
  id is unchanged, and no catalog or physical table mutation occurs
