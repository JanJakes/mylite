# Baseline GROUP BY Multiple Keys

## Status

This phase expands the descriptor-driven grouped aggregate `SELECT` path from
one group key to a small ordered group-key prefix:

```sql
SELECT group_column_1 [, group_column_2 ...], aggregate [, aggregate ...]
FROM source
[WHERE predicate]
GROUP BY group_column_1 [, group_column_2 ...]
[HAVING grouped_or_selected_aggregate_predicate]
[ORDER BY grouped_column_or_selected_aggregate_alias [ASC | DESC]]
[LIMIT select_limit_form]
```

It keeps the existing grouped aggregate source and aggregate limits. SQLite
continues to perform source scanning, filtering, grouping, aggregate stepping,
ordering, and limiting through generated physical SQL. MyLite owns parser
admission, descriptor resolution, supported-surface validation, generated SQL
shape, result metadata, value readback, and diagnostics.

## Sources

- MyLite README architecture: `README.md`
- MyLite engineering standards:
  `docs/architecture/engineering-standards.md`
- Existing grouped aggregate specs:
  `docs/specs/baseline-group-by-single-column-aggregate/specs.md`,
  `docs/specs/baseline-group-by-multiple-aggregates/specs.md`, and
  `docs/specs/baseline-group-by-string-column/specs.md`
- MySQL 8.4 Reference Manual, `SELECT`:
  https://dev.mysql.com/doc/refman/8.4/en/select.html
- MySQL 8.4 Reference Manual, aggregate functions:
  https://dev.mysql.com/doc/refman/8.4/en/aggregate-functions.html
- MySQL 8.4 Reference Manual, MySQL handling of `GROUP BY`:
  https://dev.mysql.com/doc/refman/8.4/en/group-by-handling.html
- MySQL 8.4.9 runtime probes recorded in
  `packages/libmylite/tests/mysql_baseline_group_by_multiple_keys_expectations.sh`

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior,
public SQLite APIs, and existing MyLite source code. It does not copy MySQL,
MariaDB, Percona, SQLite implementation internals, or restrictively licensed
implementation sources.

## MySQL 8.4.9 Runtime Observations

Runtime probes against MySQL 8.4.9 verify the following behavior for the
admitted subset:

- `GROUP BY a, b` forms one group per distinct tuple.
- `NULL` group-key values compare equal for grouping, so all rows with the
  same non-`NULL` keys and `NULL` in another grouped key share one group.
- With the default `utf8mb4_0900_ai_ci` collation, verified ASCII string case
  variants group together. `CHAR` trailing spaces do not create distinct
  groups.
- `WHERE` filters source rows before grouping. `HAVING` filters grouped rows
  after aggregation. `ORDER BY` and `LIMIT` apply after `HAVING`.
- `SELECT a, b, COUNT(*) FROM t GROUP BY a, b` is valid under the default
  `ONLY_FULL_GROUP_BY` mode. Selecting a nonaggregate descriptor column that is
  not one of the grouped columns fails with `1055 / 42000`.
- `HAVING` can refer to selected aggregate aliases and aggregate expressions.
  MySQL also accepts broader grouped-column and alias forms than this phase.
- `ORDER BY` can sort by grouped columns or selected aggregate aliases. With
  `ONLY_FULL_GROUP_BY` disabled, MySQL also accepts ordering by a
  nonaggregated descriptor column that is neither grouped nor selected; the
  chosen row value is nondeterministic when a group contains multiple values.
  `ASC` is the default, and `NULL` sorts before non-`NULL` ascending and after
  non-`NULL` descending.
- With `sql_mode=''`, WordPress-style archive queries that select
  `YEAR(post_date)`, `MONTH(post_date)`, and `COUNT(ID)`, group by the same
  `YEAR()` / `MONTH()` expressions, and order by `post_date` are accepted.
- Successful grouped result statements leave `@@warning_count = 0` and make
  `ROW_COUNT()` return `-1`.

MySQL supports broader multiple-key grouping, including unselected group keys,
aliases, ordinals, expressions, functional-dependence analysis, and richer
`HAVING`/`ORDER BY` expressions. Those remain outside this slice.

## Supported Surface

MyLite supports:

- two to four descriptor group keys in the grouped aggregate `SELECT` path;
- the existing one-key grouped aggregate behavior unchanged;
- selected group columns as an ordered prefix that must match the `GROUP BY`
  key list exactly by descriptor and source;
- selected `YEAR(descriptor_temporal_column)` and
  `MONTH(descriptor_temporal_column)` expression group keys as an ordered
  prefix that must match the `GROUP BY` key list exactly by temporal function
  and descriptor source;
- selected `YEAR()` / `MONTH()` expression aliases as group keys when no source
  descriptor column shadows the alias name;
- at least one and at most sixteen selected aggregate results after the group
  key prefix;
- group-key descriptor families already admitted by the grouped path:
  integer-family columns and ASCII nonbinary string columns (`CHAR`,
  `VARCHAR`, and baseline `TEXT` family);
- one persistent base table or the current two-source inner/cartesian/comma/
  left-outer grouped source envelope;
- source-qualified group keys using the current source alias and schema/table
  resolution rules;
- optional `WHERE` using the existing grouped source predicate subset;
- optional `HAVING` on any selected grouped integer column or its alias using
  the existing integer comparison/`IS NULL` subset, any selected grouped
  nonbinary string column or its alias using `IS NULL` / `IS NOT NULL`, or one
  selected supported aggregate result using the existing aggregate `HAVING`
  subset;
- optional `ORDER BY` on one selected grouped descriptor column, one unique
  selected grouped descriptor-column alias, or one unique selected `COUNT`,
  `MIN`, `MAX`, or `SUM` aggregate alias; when `ONLY_FULL_GROUP_BY` is disabled,
  one descriptor column order key outside those strict forms is also admitted;
- optional `ASC` and `DESC`, with omitted direction meaning ascending;
- optional `LIMIT` and `OFFSET` using the existing grouped `SELECT` limit
  subset;
- no catalog mutation, descriptor cache mutation, SQLite schema generation
  change, user-row mutation, or `.mylite` preamble change.

The four-key limit is a MyLite baseline guard. It is not a MySQL limit and can
be widened when broader grouped query coverage needs it.

## Deferred Surface

This phase does not add:

- unselected grouping keys;
- group-key order independent from selected group-column order;
- grouping aliases outside the selected descriptor-column and selected
  `YEAR()` / `MONTH()` expression alias subsets, ordinals, literals,
  general expressions, function calls outside the `YEAR()` / `MONTH()` temporal
  archive subset, or parenthesized expression keys;
- expression or multi-key `ORDER BY` for grouped results;
- aggregate expression `ORDER BY`, selected `AVG`, bitwise aggregate, or
  `GROUP_CONCAT()` alias ordering;
- grouped `COUNT(DISTINCT)`;
- mixed grouped `GROUP_CONCAT()` projections;
- full Unicode collation parity, explicit `COLLATE`, collation coercibility, or
  non-ASCII grouping guarantees;
- functional-dependence analysis, rollup, grouping sets, windows, derived
  tables, CTEs, subqueries, or arbitrary SQLite SQL pass-through.

## Ownership Boundary

- The public API remains unchanged. `mylite_execute()` and result ownership
  conventions are unchanged.
- Statement context owns diagnostics reset, warnings, `ROW_COUNT()` state, and
  successful result publication.
- Lexer/parser/AST own comma-separated `GROUP BY` key syntax and source spans.
  They remain independent of runtime, catalog, storage, and SQLite.
- Analyzer/planner code resolves each selected group key and each `GROUP BY`
  key from MyLite descriptors, validates supported descriptor families, and
  rejects unsupported grouped shapes before physical SQL is generated.
- The catalog remains authoritative for schemas, table sources, aliases, and
  columns. SQLite metadata is not used for logical name resolution.
- Runtime SQL generation lowers only validated descriptor columns to quoted
  physical SQLite identifiers and bound parameters.
- SQLite owns physical scanning, filtering, grouping, aggregate execution,
  ordering, and limiting. MyLite materializes only final grouped result rows in
  the public result object.
- Storage/VFS and the `.mylite` preamble are not touched by this read-only
  query path.

## Grammar

The independently authored MyLite subset is:

```lemon
group_clause_opt(A) ::= . { A = NULL; }
group_clause_opt(A) ::= GROUP(G) BY group_key_list(K). {
    A = mylite_sql_parser_make_group_by_clause(parser, G, K);
}

group_key_list(A) ::= group_key(K). {
    A = mylite_sql_parser_make_group_by_key_list(parser, K);
}
group_key_list(A) ::= group_key_list(L) COMMA group_key(K). {
    A = mylite_sql_parser_append_group_by_key(parser, L, K);
}

group_key(A) ::= qualified_identifier(K). { A = K; }
group_key(A) ::= YEAR(T) LPAREN expression(E) RPAREN(R). {
    A = mylite_sql_parser_make_one_argument_function(
        parser, T, MYLITE_SQL_AST_YEAR_FUNCTION, E, R);
}
group_key(A) ::= MONTH(T) LPAREN expression(E) RPAREN(R). {
    A = mylite_sql_parser_make_one_argument_function(
        parser, T, MYLITE_SQL_AST_MONTH_FUNCTION, E, R);
}
```

Runtime validation, not grammar, limits descriptor families, key count, and
select-list shape.

## Resolution And Semantics

Group keys resolve through the existing descriptor source context. For the
supported multiple-key path, MyLite requires the first `N` select items to be
descriptor columns or selected `YEAR()` / `MONTH()` temporal expressions that
match the `N` `GROUP BY` keys in order. Each selected group column or selected
temporal group expression may have an alias. Aggregate select items start after
the selected group-key prefix.

String group keys use MyLite's registered ASCII `utf8mb4_0900_ai_ci`
collation in generated grouping and grouped ordering expressions. Integer keys
use stored integer values. `NULL` values form one group per equal key tuple.
No result row order is promised without an admitted `ORDER BY`.

For `HAVING`, unqualified grouped descriptor names are checked before selected
group aliases, preserving the existing grouped path's descriptor-first
behavior. Aggregate aliases keep the existing unique-alias requirement.

For grouped `ORDER BY`, a grouped descriptor-column alias must be unique among
selected group aliases. Aggregate alias ordering keeps the existing unique
selected aggregate alias requirement and remains limited to `COUNT`, `MIN`,
`MAX`, and `SUM`. When `ONLY_FULL_GROUP_BY` is disabled, MyLite accepts one
descriptor order key outside those strict forms to match MySQL's relaxed mode
for application query shapes such as WordPress archives.

## SQLite Handling

MyLite builds physical SQL shaped like:

```sql
SELECT "group_column_1", "group_column_2", COUNT(*), SUM("n")
FROM "_mylite_user_table_<table_id>"
[WHERE ...]
GROUP BY "group_column_1", "group_column_2"
[HAVING ...]
[ORDER BY "group_column_1" ASC | SUM("n") DESC]
[LIMIT ? [OFFSET ?]]
```

For the admitted temporal archive subset, selected and grouped `YEAR()` /
`MONTH()` expressions lower through MyLite's temporal extraction SQL function
with bound discriminator parameters:

```sql
SELECT _mylite_temporal_extract("post_date", ?, ?, ?), COUNT("ID")
FROM "_mylite_user_table_<table_id>"
GROUP BY _mylite_temporal_extract("post_date", ?, ?, ?)
```

For string group keys, the `GROUP BY`, grouped-column `HAVING`, and grouped
`ORDER BY` expressions use the registered MyLite ASCII collation expression.
Every generated SQLite identifier is quoted. Predicate, `HAVING`, aggregate
separator, temporal extraction discriminators, limit, and offset values remain
bound parameters.

No SQLite fork patch is required.

## Diagnostics

Required diagnostics include:

- syntax errors: existing parser diagnostics;
- missing/default schema, unknown schema, unknown table, reserved names, and
  unsupported object kind: existing grouped source diagnostics;
- unsupported select-list shape:
  `GROUP BY supports selected descriptor group columns followed by aggregate results`;
- too many group keys:
  `GROUP BY supports at most four descriptor group columns`;
- too many aggregate results:
  `GROUP BY supports at most sixteen aggregate results`;
- selected nonaggregate descriptor column not in the exact grouped prefix:
  existing `ONLY_FULL_GROUP_BY` `1055 / 42000` diagnostic with the matching
  select-list expression index;
- unknown grouped, aggregate, predicate, order, or having columns: existing
  MySQL-compatible column diagnostics for the matching clause;
- unsupported group-key descriptor type:
  `GROUP BY supports only integer and nonbinary string descriptor group columns`;
- selected `YEAR()` / `MONTH()` expression that is not also a grouped key:
  `GROUP BY supports selected YEAR() and MONTH() expressions only when grouped`;
- unsupported `HAVING` operand shape, unsupported grouped string comparison,
  unselected aggregate predicate, bitwise aggregate predicate, or
  `GROUP_CONCAT()` predicate: existing grouped `HAVING` diagnostics;
- unsupported grouped `ORDER BY` multiple keys, nonselected group column,
  duplicate group alias, duplicate aggregate alias, selected `AVG`, bitwise
  aggregate, or `GROUP_CONCAT()` alias order key: deterministic grouped
  `ORDER BY` diagnostics;
- physical SQLite failures: existing physical row-operation diagnostic;
- allocation failure: existing `MYLITE_NOMEM` diagnostic behavior.

No public API misuse behavior changes.

## Tests

Add MySQL-runtime expectation coverage and fast C tests for:

- `GROUP BY a, b` integer keys with `COUNT(*)`, `COUNT(column)`, `SUM`,
  `MIN`, `MAX`, and `AVG` where already supported;
- nullable grouped keys and nullable aggregate arguments;
- ASCII nonbinary string plus `CHAR` grouped keys, including case-insensitive
  and trailing-space grouping;
- `WHERE` before grouping;
- `HAVING` on selected grouped keys and selected aggregate aliases;
- grouped `ORDER BY` on grouped keys and selected aggregate aliases, including
  `ASC`, `DESC`, `NULL` placement, and `LIMIT`;
- source-qualified and aliased table references;
- result labels, warning count, row count, no catalog generation changes, no
  SQLite schema generation changes, file preamble preservation, reopen
  persistence, table rename/drop behavior, independent file-backed handles, and
  zero-initialized cleanup through existing result/plan cleanup paths;
- diagnostics for unknown group keys, unknown selected keys, non-grouped
  selected columns, unselected group keys, unsupported key types, too many
  keys, duplicate aliases in grouped ordering, unsupported grouped aliases,
  ordinals, expressions, and broader `HAVING`/`ORDER BY` forms.
