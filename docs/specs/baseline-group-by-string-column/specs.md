# Baseline GROUP BY String Column

## Status

This phase widens the existing descriptor-driven grouped aggregate `SELECT`
baseline so the one grouped descriptor column may be a supported nonbinary
string column. It builds on the previous grouped aggregate, multiple aggregate,
`HAVING`, string storage, string predicate, and string ordering slices.

This is not full MySQL grouping. It keeps the existing one-group-key grouped
aggregate shape and only adds `CHAR`, `VARCHAR`, and the baseline `TEXT` family
as admitted group-key descriptor types.

## Sources

- MyLite README architecture: `README.md`
- MyLite engineering standards:
  `docs/architecture/engineering-standards.md`
- Baseline grouped aggregate specs:
  `docs/specs/baseline-group-by-single-column-aggregate/specs.md`,
  `docs/specs/baseline-group-by-multiple-aggregates/specs.md`, and
  `docs/specs/baseline-having-grouped-aggregate/specs.md`
- MySQL 8.4 Reference Manual, `SELECT` syntax:
  https://dev.mysql.com/doc/refman/8.4/en/select.html
- MySQL 8.4 Reference Manual, MySQL handling of `GROUP BY`:
  https://dev.mysql.com/doc/refman/8.4/en/group-by-handling.html
- MySQL 8.4 Reference Manual, aggregate functions:
  https://dev.mysql.com/doc/refman/8.4/en/aggregate-functions.html
- MySQL 8.4 Reference Manual, character sets and collations:
  https://dev.mysql.com/doc/refman/8.4/en/charset-general.html
- MySQL 8.4.9 runtime probes recorded in
  `packages/libmylite/tests/mysql_baseline_group_by_string_column_expectations.sh`

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior,
public SQLite APIs, and existing MyLite source code. It does not copy MySQL,
MariaDB, Percona, SQLite implementation internals, or restrictively licensed
implementation sources.

## MySQL 8.4.9 Runtime Observations

Runtime probes against MySQL 8.4.9 verify the following behavior for the
admitted subset:

- A nullable string grouping key places all `NULL` values into one group.
- With the default `utf8mb4_0900_ai_ci` collation, ASCII case variants such as
  `alice` and `Alice` group together.
- `CHAR` values that differ only by trailing spaces group together and are
  displayed in MySQL's trimmed readback shape.
- Baseline `TEXT` values group with the same default case-insensitive collation
  behavior as `VARCHAR` for the probed ASCII values.
- `ORDER BY group_column` sorts `NULL` before non-`NULL` values ascending and
  after non-`NULL` values descending. ASCII string ordering follows the default
  case-insensitive collation for the probed values.
- `WHERE` filters source rows before grouping. `HAVING` filters groups after
  grouping. `ORDER BY` and `LIMIT` apply after `HAVING`.
- `HAVING` can refer to selected aggregate aliases such as `c` in
  `HAVING c > 1`. `HAVING group_alias IS NULL` is accepted.
- `HAVING` can compare supported string group columns and selected aliases to
  ordinary string literals in the companion
  `baseline-grouped-string-comparison-having` slice.
- `LIMIT 0` returns no result rows. `LIMIT row_count OFFSET offset` and
  `LIMIT offset, row_count` keep the existing grouped SELECT limit behavior.
- Successful statements leave warning count `0`; `ROW_COUNT()` returns `-1`
  after the grouped result statement.

MySQL accepts broader string grouping forms, including grouping aliases,
ordinals, expressions, multiple group keys, broader `HAVING` expressions, and
full collation semantics. Those remain outside this slice.

## Scope

The implementation must add:

- `CHAR`, `VARCHAR`, and baseline `TEXT` family descriptor columns as valid
  grouped keys in the existing grouped aggregate `SELECT` shape;
- MyLite descriptor-driven resolution for selected group column, `GROUP BY`
  column, source-qualified variants, optional aliases, optional `WHERE`,
  optional supported `HAVING`, optional `ORDER BY`, and optional `LIMIT`;
- MyLite's registered limited Unicode `utf8mb4_0900_ai_ci` collation on generated SQLite
  grouping keys and existing grouped-order keys for string group columns;
- result readback for `NULL` and NUL-free valid UTF-8 string group values;
- preservation of existing grouped aggregate behavior for integer group keys;
- deterministic diagnostics for unsupported group-key descriptor types and
  unsupported string group-column `HAVING` comparisons;
- MySQL-runtime expectation coverage and fast C runtime tests.

The existing aggregate argument type limits remain unchanged. `COUNT(*)` and
`COUNT(column)` may be grouped by string keys. `MIN`, `MAX`, `SUM`, `AVG`,
`BIT_AND`, `BIT_OR`, and `BIT_XOR` still require supported integer aggregate
argument columns. Grouped `GROUP_CONCAT()` keeps its existing single-result,
one-base-table limitations but may use an admitted string group key when the
rest of the existing `GROUP_CONCAT()` grouped shape is valid.

The current two-source joined grouped aggregate envelope may use string group
keys through the same descriptor and SQL-building path. This phase focuses
new coverage on the persistent base-table path because that is the common
application shape and the reported failure class.

## Non-Goals

This phase must not add:

- multiple grouping keys;
- grouping aliases, ordinals, literals, expressions, function calls, or
  parenthesized expression keys;
- full Unicode collation parity, explicit `COLLATE`, collation coercibility, or
  non-ASCII comparison guarantees;
- binary string, `ENUM`, `SET`, JSON, temporal, decimal, or approximate numeric
  group keys;
- grouped string comparison predicates outside the companion
  `baseline-grouped-string-comparison-having` literal subset;
- `HAVING` boolean composition, unselected aggregate predicates, subqueries, or
  arbitrary expressions;
- functional-dependence analysis, rollup, grouping sets, windows, CTEs,
  derived tables, or arbitrary SQLite SQL pass-through;
- SQLite fork patches.

## Ownership Boundary

- The public API remains unchanged. `mylite_execute()` and result ownership
  conventions are unchanged.
- Statement context owns diagnostics reset, warning count, and result statement
  row-count state. Successful grouped selects continue to make `ROW_COUNT()`
  return `-1`.
- Lexer/parser/AST already admit `GROUP BY qualified_identifier`; this phase
  does not change grammar shape.
- Analyzer/planner code resolves the selected group key and `GROUP BY` key
  from MyLite catalog descriptors, verifies the type is either an existing
  integer group key or an admitted nonbinary string group key, and rejects
  other descriptor families before SQLite SQL is generated.
- The catalog remains authoritative for MySQL-visible schemas, tables, and
  columns. Grouped selects read descriptors but do not mutate catalog rows,
  descriptor versions, descriptor caches, catalog generation, or
  `sqlite_schema_generation`.
- Runtime execution emits descriptor-built SQLite SQL against stable physical
  table names and quoted physical column names. SQLite owns source scanning,
  filtering, grouping, aggregate calculation, ordering, and limiting.
- MyLite result code materializes only final result groups in the public result
  object. It must not materialize source rows to implement grouping.
- Storage/VFS and the `.mylite` preamble are not touched by this read-only
  query path.

## Supported SQL Shape

The supported shape remains the existing grouped aggregate shape:

```sql
SELECT group_column [AS alias], aggregate [AS alias][, aggregate [AS alias] ...]
FROM source
[WHERE baseline_predicate]
GROUP BY group_column
[HAVING baseline_grouped_having_predicate]
[ORDER BY group_column_or_alias_or_supported_aggregate_alias [ASC | DESC]]
[LIMIT select_limit_form]
```

For this phase, `group_column` may resolve to:

```text
INT / INTEGER / BIGINT and admitted unsigned integer family descriptors
CHAR
VARCHAR
baseline TEXT family
```

The string subset uses existing descriptor reference forms:

```sql
column_name
table_name.column_name
schema_name.table_name.column_name
table_alias.column_name
```

MyLite Lemon-syntax shape is unchanged and shown for the supported query
contract:

```lemon
group_clause_opt(A) ::= . { A = NULL; }
group_clause_opt(A) ::= GROUP(G) BY qualified_identifier(C). {
    A = mylite_sql_parser_make_group_by_clause(parser, &G, C);
}
```

Runtime validation, not grammar, limits the admitted descriptor type.

## String Grouping Semantics

For supported string descriptor group keys:

- `NULL` values form one group.
- Non-`NULL` values are grouped with MyLite's registered limited Unicode
  `utf8mb4_0900_ai_ci` collation.
- ASCII case variants group together. Non-ASCII collation weights are not
  claimed in this slice.
- `CHAR` stored values continue to use the existing MyLite `CHAR` conversion
  and readback policy, including default trailing-space trimming.
- Result rows expose the group value returned by SQLite for the grouped
  expression. Tests use stable source data where the representative grouped
  string is MySQL-runtime verified.
- Without an explicit supported `ORDER BY`, no row order is promised.
- With `ORDER BY group_column`, string keys use the same ASCII collation and
  `NULL` ordering as the existing string `ORDER BY` slice.
- Duplicate string sort values have no tie-order guarantee.

## HAVING Semantics

Existing aggregate-result `HAVING` predicates continue to work with a string
group key:

```sql
HAVING count_alias > 1
HAVING SUM(integer_column) IS NOT NULL
```

The grouped string column itself is admitted for existing `IS NULL` and
`IS NOT NULL` group-column `HAVING` predicates:

```sql
HAVING group_alias IS NULL
HAVING group_column IS NOT NULL
```

The companion `baseline-grouped-string-comparison-having` slice admits
descriptor-driven ordinary string-literal comparisons:

```sql
HAVING group_alias = 'alice'
HAVING group_column <> 'bob'
```

Numeric/string coercions, boolean composition, `BETWEEN`, `IN`, `LIKE`,
`REGEXP`, arbitrary expressions, and full Unicode collation parity remain
outside the grouped string-column baseline.

## SQLite Handling

This feature uses MyLite wrapper/translation over public SQLite APIs. No
SQLite fork patch is required.

Generated SQL for a string group column must use a collation-bearing expression
for the `GROUP BY` key:

```sql
SELECT "name", COUNT(*)
FROM "_mylite_user_table_<table_id>"
GROUP BY "name" COLLATE "utf8mb4_0900_ai_ci"
ORDER BY "name" COLLATE "utf8mb4_0900_ai_ci" ASC
```

Joined grouped selects use the same source aliases as the existing joined
grouped aggregate path.

Every generated SQLite identifier is quoted. Predicate, `HAVING`, `LIMIT`, and
`OFFSET` values remain bound parameters. User literals are not interpolated
into generated SQL.

## Diagnostics

Existing diagnostics are preserved for missing default schema, unknown schema,
unknown table, reserved `_mylite_*` names, unknown selected/group/order/HAVING
columns, unsupported grouping shapes, unsupported aggregate arguments,
unsupported order keys, unsupported limit forms, physical SQLite failures,
allocation failures, and public API misuse.

Type validation changes only the group-key diagnostic:

- integer group keys remain accepted;
- admitted nonbinary string group keys are accepted;
- other descriptor families continue to fail with a deterministic unsupported
  message that says `GROUP BY supports only integer and nonbinary string
  descriptor group columns`.

Unsupported string group-column `HAVING` comparisons use the existing
deterministic unsupported literal/conversion diagnostic for this phase.

## Tests

Fast C tests must cover:

- `VARCHAR`, `CHAR`, and baseline `TEXT` group keys;
- `NULL` grouping;
- ASCII case-insensitive grouping and ordering;
- `WHERE` before grouping;
- aggregate-alias `HAVING` with a string group key;
- group-column `HAVING IS NULL`;
- group-column and selected-alias string comparison `HAVING`;
- `ORDER BY` default/`ASC` and `DESC`;
- `LIMIT 0`, exact limits, and offset forms inherited from grouped SELECT;
- successful result labels, warning count `0`, no affected rows, and
  `ROW_COUNT() = -1`;
- persistence after close/reopen and table rename/drop through the existing
  grouped lifecycle test;
- unsupported non-string/non-integer group keys still rejected.

The MySQL expectation script records the MySQL 8.4.9 behavior used by those
tests. Existing grouped aggregate, `HAVING`, parser, runtime, file-format, and
workflow checks must continue to pass.
