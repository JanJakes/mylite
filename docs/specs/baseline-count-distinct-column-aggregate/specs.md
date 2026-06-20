# Baseline COUNT DISTINCT Column Aggregate

## Status

This feature specifies a narrow `COUNT(DISTINCT expr)` slice:
`COUNT(DISTINCT column_name)`, including parenthesized descriptor-column
arguments such as `COUNT(DISTINCT(table.column))`. It builds on
`mylite_execute()`, statement context, the parser scaffold, durable catalog
descriptors, schema/table lifecycle, integer/`NULL` row values,
descriptor-driven single-table `SELECT`, the baseline `WHERE` predicate subset,
and the existing `COUNT(*)`, `COUNT(column)`, and
`COUNT(integer/NULL/boolean literal)` aggregate paths.

The feature is intentionally not full `COUNT(DISTINCT expr)` support. It
admits exactly one aggregate select item, one descriptor column, one persistent
base-table source, and an optional baseline `WHERE` predicate. It supports the
existing source-qualified descriptor-column surface and the parenthesized form
used by WordPress. Grouped count-distinct support is specified separately in
`docs/specs/baseline-grouped-count-distinct-aggregate/specs.md`. This ungrouped
slice does not add multiple distinct expressions, literal distinct arguments,
general expression arguments, aliases, ordering, limiting, window forms, or
general distinct query semantics.

## Sources

- MyLite README architecture: `README.md`
- MyLite engineering standards:
  `docs/architecture/engineering-standards.md`
- Baseline implementation strategy:
  `docs/specs/baseline-implementation-strategy/specs.md`
- Runtime handles and statement context:
  `docs/specs/runtime-handles-statement-context/specs.md`
- Baseline catalog foundation:
  `docs/specs/baseline-catalog-foundation/specs.md`
- Baseline row values:
  `docs/specs/baseline-row-values-lifecycle/specs.md`
- Baseline select where lifecycle:
  `docs/specs/baseline-select-where-lifecycle/specs.md`
- Baseline count aggregate:
  `docs/specs/baseline-count-aggregate/specs.md`
- Baseline count column aggregate:
  `docs/specs/baseline-count-column-aggregate/specs.md`
- Baseline count literal aggregate:
  `docs/specs/baseline-count-literal-aggregate/specs.md`
- MySQL lexer:
  `docs/specs/mysql-lexer/specs.md`
- MySQL parser scaffold:
  `docs/specs/mysql-parser-scaffold/specs.md`
- SQLite source snapshot notes: `third_party/sqlite/README.md`
- MySQL 8.4 Reference Manual, aggregate functions:
  https://dev.mysql.com/doc/refman/8.4/en/aggregate-functions.html
- MySQL 8.4 Reference Manual, `SELECT` and `DUAL`:
  https://dev.mysql.com/doc/refman/8.4/en/select.html
- MySQL 8.4 Reference Manual, function name parsing:
  https://dev.mysql.com/doc/refman/8.4/en/function-resolution.html

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior,
public SQLite APIs, and existing MyLite source code. It does not copy MySQL,
MariaDB, Percona, SQLite implementation internals, or other restrictively
licensed implementation sources.

## MySQL 8.4.9 Runtime Observations

Observed against the local `mysql:8.4.9` runtime using TCP:

- `COUNT(DISTINCT expr)` returns a `BIGINT` count of different non-`NULL`
  expression values in the retrieved rows.
- Empty matched sets and matched sets where every distinct argument value is
  `NULL` return `0`.
- Duplicate integer values are counted once. `NULL` is not counted as a
  distinct value.
- `COUNT(DISTINCT n)` over values `NULL, 20, 20, 30, NULL` returns `2`.
- `COUNT(DISTINCT bool_col)` over `TRUE, FALSE, FALSE, NULL, TRUE` returns
  `2`.
- `COUNT(DISTINCT varchar_column)`, `COUNT(DISTINCT char_column)`, and
  `COUNT(DISTINCT text_column)` count unique non-`NULL` values using the
  column comparison collation. In the covered default-collation probes,
  `alice` and `Alice`, `A` and `A   `, and `essay` and `Essay` each count as
  one distinct value.
- `COUNT(DISTINCT column)` and the function name are case-insensitive for the
  admitted identifier names. Default result labels preserve source spelling,
  and MySQL inserts a space after a block comment before the following token in
  labels such as `COUNT(DISTINCT /*x*/ n)`,
  `COUNT(DISTINCT/*x*/ n)`, and `COUNT(/*x*/ DISTINCT n)`.
- `SELECT COUNT(DISTINCT column)` and
  `SELECT COUNT(DISTINCT column) FROM DUAL` fail with unknown-column error
  `1054`, SQLSTATE `42S22`, when the column name is not otherwise resolvable.
- `COUNT (DISTINCT column)` and `COUNT/**/(DISTINCT column)` fail with syntax
  error `1064` under the default SQL mode. They do not follow the same stored
  function resolution path observed for `COUNT (1)` or `COUNT (column)`.
- `COUNT(DISTINCT table.column)`, `COUNT(DISTINCT(table.column))`, and
  `COUNT(DISTINCT (table.column))` are valid MySQL aggregate expressions.
  MySQL preserves the parenthesized argument spelling in result labels such as
  `COUNT(DISTINCT(t.id))`.
- `COUNT(DISTINCT n, nn)`, `COUNT(DISTINCT 1)`,
  `COUNT(DISTINCT TRUE)`, and `COUNT(DISTINCT n + 1)` are valid MySQL
  aggregate expressions, but remain outside this MyLite slice because they
  require multi-expression, literal, or general expression aggregate argument
  support.
- `ORDER BY` and `LIMIT` on a single aggregate row are valid MySQL syntax, with
  `LIMIT 0` suppressing the row. This slice rejects `ORDER BY` and `LIMIT` for
  aggregate selects to preserve the current aggregate cardinality surface.
- A successful `SELECT COUNT(DISTINCT column)` result set makes the following
  `ROW_COUNT()` return `-1` and leaves warning count `0`.

The reproducible probe lives in
`packages/libmylite/tests/mysql_baseline_count_distinct_column_aggregate_expectations.sh`.

## Scope

The implementation must add:

- parser and AST support for no-space `COUNT(DISTINCT identifier)` and
  `COUNT(DISTINCT(qualified_identifier))`;
- descriptor-driven
  `SELECT COUNT(DISTINCT column_name) FROM table_name [WHERE predicate]`;
- unqualified and schema-qualified table-name resolution using the existing
  selected/default schema policy;
- one persistent MyLite base-table descriptor source only;
- unqualified, source-qualified, and parenthesized distinct aggregate argument
  column resolution from MyLite descriptors;
- explicit aggregate access to invisible descriptor columns, matching explicit
  projection, `COUNT(column)`, `MIN(column)`, and `MAX(column)` behavior;
- reuse of the existing baseline `WHERE` predicate subset and conversion
  rules;
- generated SQLite physical SQL built only from descriptors and stable
  physical table names;
- prepared-statement binding for predicate values;
- one result row with one text column containing the decimal count;
- MySQL-compatible result column labels for the selected aggregate expression;
- result-set row-count state matching existing `SELECT` behavior;
- deterministic diagnostics for unsupported aggregate syntax and wider MySQL
  aggregate/select forms;
- tests and a MySQL 8.4.9 expectation artifact for supported behavior and
  deliberately rejected or deferred wider forms.

Existing `COUNT(*)`, `COUNT(column)`, and `COUNT(literal)` behavior remains
unchanged.

## Non-Goals

This feature must not implement:

- general `COUNT(DISTINCT expr)`, multiple distinct expressions, literals,
  arithmetic expressions, functions outside the parenthesized descriptor-column
  form, parameters, subqueries, or qualified wildcards;
- no-source or `FROM DUAL` evaluation for `COUNT(DISTINCT column)`;
- aliases, mixed projections, multiple aggregate select items, aggregate
  comparisons, aggregate arithmetic, or nested aggregates;
- `GROUP BY` and `HAVING` outside the separately specified grouped
  count-distinct slice, `ORDER BY`, `LIMIT`, window `OVER` clauses, joins, CTEs,
  subqueries, unions, locking clauses, query modifiers, optimizer hints, `INTO`,
  or arbitrary SQLite SQL pass-through;
- binary string, decimal, floating, temporal, JSON, enum, set, per-expression
  collation, or charset distinct semantics;
- aggregate metadata parity, protocol column flags, exact optimizer behavior,
  index-only distinct-count planning, transaction isolation beyond existing
  SQLite statement visibility, temporary tables, views, privileges, SQL modes
  such as `IGNORE_SPACE`, or SQLite fork patches.

## Ownership Boundary

- The public API remains unchanged. `mylite_execute()` owns public validation,
  result-handle ownership, statement-boundary row-count state, and failure
  cleanup.
- Statement context owns diagnostics reset, warning count, and statement
  completion. Successful count selects are result-set statements and therefore
  store `-1` as the connection-local previous row count.
- Lexer/parser/AST own syntax admission, the no-space function-call rule, and
  source spans. They remain independent of runtime, catalog, storage, and
  SQLite.
- Analyzer/planner code recognizes the one-item aggregate shape, resolves the
  source table, distinct aggregate argument column, and optional predicate
  descriptors, rejects unsupported shapes, and builds a descriptor-driven
  aggregate plan.
- The catalog module remains authoritative for schema/table/column
  descriptors. `COUNT(DISTINCT column)` reads descriptors for table, aggregate
  argument, and predicate resolution but does not mutate catalog rows,
  descriptor versions, descriptor caches, catalog generation, or
  `sqlite_schema_generation`.
- Runtime execution generates SQLite SQL against the descriptor-owned physical
  table and binds only predicate parameters.
- The result builder owns the one-column text result. Counts are formatted as
  non-`NULL` decimal integer text.
- Storage/VFS owns the `.mylite` preamble and shifted SQLite payload boundary.
  Aggregate queries do not touch byte range `[0, 4096)`.

## Supported SQL Grammar

Supported subset:

```sql
SELECT COUNT(DISTINCT column_name) FROM table_name [WHERE predicate]
```

`table_name` uses the existing table lifecycle subset:

```sql
table_name:
    identifier
  | identifier.identifier
```

`column_name` is one unqualified identifier resolved against the selected table
descriptor. It may name a visible or invisible descriptor column.

The supported predicate subset is exactly the subset from
`baseline-select-where-lifecycle`:

```sql
predicate:
    column_name comparison_operator signed_integer_or_boolean_literal
  | column_name IS NULL
  | column_name IS NOT NULL
  | ( predicate )
```

The aggregate function name must be directly adjacent to `(`. Whitespace and
comments are accepted inside the argument list, including around `DISTINCT`.

MyLite Lemon-syntax grammar snippets:

```lemon
expression ::= COUNT LPAREN DISTINCT identifier RPAREN.
```

The parser may admit `COUNT(DISTINCT column)` anywhere the expression grammar
is currently shared, but the analyzer accepts it only as the sole select item
in the supported statement shape. `COUNT` remains usable as an ordinary
unquoted identifier in identifier positions where the parser admits
nonreserved keywords. Bare `COUNT` is not an aggregate call.

## Runtime Semantics

`SELECT COUNT(DISTINCT column_name) FROM table_name` returns one result row
containing the number of unique non-`NULL` stored values in the descriptor
column among the matched rows. With a supported `WHERE` predicate, the
distinct count is evaluated over only matched rows.

If the matched row set is empty, or if all matched aggregate argument values
are `NULL`, the result is `0`.

Successful count-distinct-column selects:

- return one result column;
- return one result row;
- use MySQL-compatible default label text for the selected aggregate
  expression, including observed block-comment spacing;
- use `affected_rows == 0` under the existing MyLite row-result convention;
- use `warning_count == 0` for supported forms;
- set the connection-local previous row count to `-1` after completion because
  the statement returns a result set.

## Generated SQLite Handling

For table-backed count-distinct-column aggregates, MyLite generates this SQLite
shape:

```sql
SELECT COUNT(DISTINCT "physical_column") FROM "physical_table" [WHERE ...]
```

Generated table and column names come from MyLite descriptors and are quoted.
Predicate SQL is reused from the existing descriptor-built baseline `WHERE`
planner, and comparison values are bound through prepared statements.
For nonbinary string descriptor arguments, the distinct expression adds
MyLite's registered string-key collation:

```sql
SELECT COUNT(DISTINCT "physical_column" COLLATE "utf8mb4_0900_ai_ci")
FROM "physical_table" [WHERE ...]
```

This feature uses MyLite wrapper/translation code and public SQLite prepared
statement APIs only. It must not add SQLite fork patches or custom SQLite
functions.

## Diagnostics

Supported count-distinct-column forms reuse the existing count aggregate
diagnostics:

| Case | Result |
| --- | --- |
| Syntax errors, including `COUNT (DISTINCT n)` and `COUNT/**/(DISTINCT n)` | `MYLITE_ERROR`, MySQL-style syntax diagnostic |
| More than one aggregate select item | `MYLITE_ERROR`, deterministic `COUNT(DISTINCT column)` message |
| Unsupported clauses such as `ORDER BY`, `LIMIT`, `GROUP BY`, `HAVING`, or windows | `MYLITE_ERROR`, deterministic `COUNT(DISTINCT column)` message |
| No source or `FROM DUAL` | `MYLITE_ERROR`, unknown-column or unsupported diagnostic matching the existing count-column boundary |
| Unknown schema or table | Existing schema/table diagnostics |
| Reserved `_mylite_*` table target | Existing reserved-name diagnostic |
| Unknown distinct argument column | Existing unknown column in `field list` diagnostic |
| Unknown predicate column | Existing unknown column in `where clause` diagnostic |
| Unsupported predicate literal or expression | Existing `WHERE` unsupported diagnostic |
| Physical SQLite failure | Existing internal SQLite failure diagnostic |
| Allocation failure | `MYLITE_NOMEM` with existing allocation diagnostic |
| Public API misuse | Existing public API misuse behavior |

Unsupported aggregate arguments such as `COUNT(DISTINCT n, nn)`,
`COUNT(DISTINCT t.n)`, `COUNT(DISTINCT 1)`, `COUNT(DISTINCT TRUE)`,
`COUNT(DISTINCT n + 1)`, string/decimal/float/hex/bit literals, parameters,
functions, subqueries, qualified wildcards, and aliases remain syntax errors or
deterministic unsupported errors according to the current parser/analyzer
boundary.

## Tests

Add a MySQL-runtime expectation artifact covering:

- MySQL version guard for `8.4.9`;
- duplicate integer values, duplicate boolean values, `NULL` exclusion,
  all-`NULL` rows, empty tables, filtered rows, and no-match predicates;
- warning count `0` and following `ROW_COUNT() == -1`;
- default result labels, including block-comment spacing around `DISTINCT` and
  the argument;
- MySQL-accepted but deferred forms: multiple distinct expressions, literals,
  boolean literals, and non-descriptor expression arguments;
- whitespace/comment between `COUNT` and `(` syntax-error behavior.

Add or extend fast C tests covering:

- parser acceptance for `COUNT(DISTINCT column)`, parenthesized descriptor
  columns such as `COUNT(DISTINCT(table.column))`, case variants, quoted
  identifiers, and comments inside the argument list;
- parser rejection for `COUNT (DISTINCT column)`,
  `COUNT/**/(DISTINCT column)`, `COUNT(DISTINCT *)`,
  `COUNT(DISTINCT column, other)`, `COUNT(DISTINCT 1)`,
  `COUNT(DISTINCT TRUE)`, and `COUNT(DISTINCT column + 1)`;
- runtime table-backed, empty-table, all-`NULL`, duplicate-value, filtered,
  no-match, label, warning count, affected rows, following `ROW_COUNT()`,
  reopen persistence, physical failure, independent handle, and preamble
  behavior;
- nonbinary string descriptor arguments for `VARCHAR`, `CHAR`, and baseline
  `TEXT` family columns, plus binary string rejection;
- descriptor resolution for visible and invisible columns;
- unchanged behavior for existing `COUNT(*)`, `COUNT(column)`, and
  `COUNT(literal)` tests.

Run:

```sh
cmake --build --preset dev
ctest --preset dev --output-on-failure -R 'libmylite\.(parser|runtime\.count_aggregate)'
./packages/libmylite/tests/mysql_baseline_count_distinct_column_aggregate_expectations.sh
cmake --workflow --preset check
```

## Compatibility Documentation

Update:

- `COMPATIBILITY.md`;
- `docs/compatibility/functions-aggregate.md`;
- `docs/compatibility/sql-query-expressions.md`.

Use partial wording such as limited one-column
`COUNT(DISTINCT descriptor_column)` and the parenthesized descriptor-column
form. Keep `COUNT()` wording explicit that general `COUNT(expr)` remains
unsupported, and keep `COUNT(DISTINCT)` wording explicit that multiple
expressions and non-descriptor expression arguments remain unsupported.
