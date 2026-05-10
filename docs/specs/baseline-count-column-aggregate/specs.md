# Baseline COUNT Column Aggregate

## Status

This feature specifies a narrow extension of the existing baseline `COUNT`
aggregate: `COUNT(column_name)`. It builds on `mylite_execute()`, statement
context, the MyLite parser scaffold, durable catalog descriptors, schema/table
lifecycle, integer/`NULL` row values, descriptor-driven single-table `SELECT`,
the baseline `WHERE` predicate subset, and the existing `COUNT(*)`,
`MIN(column)`, and `MAX(column)` aggregate paths.

The feature is intentionally not full `COUNT(expr)` support. It admits exactly
one aggregate select item, `COUNT(column_name)`, over one persistent base table
with an optional baseline `WHERE` predicate. It does not add literal,
qualified-column, expression, `DISTINCT`, grouping, having, ordering, limiting,
window, join, subquery, alias, or mixed-projection aggregate support.

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
- Baseline min/max aggregate:
  `docs/specs/baseline-min-max-aggregate/specs.md`
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

- `COUNT(expr)` returns a `BIGINT` count of the non-`NULL` expression values in
  the rows retrieved by the select. `COUNT(*)` counts rows regardless of
  `NULL` values.
- With no `GROUP BY`, an aggregate select groups over all matched rows.
- `COUNT(column)` over supported integer columns returns the number of matched
  rows whose column value is not `NULL`.
- Empty matched sets and matched sets where the aggregate argument is `NULL`
  for every row return `0`.
- `COUNT(column)` and the function name are case-insensitive. Default result
  labels preserve the source expression spelling, except MySQL inserts a space
  after a block comment before the following identifier in labels such as
  `COUNT(/*x*/ n)`.
- `SELECT COUNT(column)` and `SELECT COUNT(column) FROM DUAL` fail with
  unknown-column error `1054`, SQLSTATE `42S22`, when the column name is not
  otherwise resolvable.
- `COUNT()` with no argument and `COUNT(a, b)` fail with syntax error `1064`,
  SQLSTATE `42000`.
- Under the default SQL mode, whitespace or a block comment between `COUNT` and
  `(` for `COUNT (column)` and `COUNT/**/(column)` resolves as a
  stored-function call and fails with error `1630` when no such function
  exists. This slice keeps the existing MyLite no-space aggregate-call rule and
  reports a deterministic syntax diagnostic for both forms.
- `COUNT(1)`, `COUNT(NULL)`, `COUNT(DISTINCT column)`, and `COUNT(table.column)`
  are valid MySQL aggregate expressions, but remain outside this MyLite slice
  because they require general expression, distinct, or qualified-name
  aggregate argument support.
- `ORDER BY` and `LIMIT` on a single aggregate row are valid MySQL syntax, with
  `LIMIT 0` suppressing the row. This slice rejects `ORDER BY` and `LIMIT` for
  aggregate selects to preserve the current aggregate cardinality surface.
- A successful `SELECT COUNT(column)` result set makes the following
  `ROW_COUNT()` return `-1` and leaves warning count `0`.

The reproducible probe lives in
`packages/libmylite/tests/mysql_baseline_count_column_aggregate_expectations.sh`.

## Scope

The implementation must add:

- parser and AST support for no-space `COUNT(identifier)`;
- descriptor-driven
  `SELECT COUNT(column_name) FROM table_name [WHERE predicate]`;
- unqualified and schema-qualified table-name resolution using the existing
  selected/default schema policy;
- one persistent MyLite base-table descriptor source only;
- unqualified aggregate argument column resolution from MyLite descriptors;
- explicit aggregate access to invisible descriptor columns, matching the
  existing explicit projection and min/max behavior;
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

Existing `COUNT(*)` behavior remains unchanged.

## Non-Goals

This feature must not implement:

- `COUNT(expr)` for literals, `NULL`, arithmetic, functions, parenthesized
  expressions, table-qualified columns, qualified wildcards, `DISTINCT`, or
  general expression arguments;
- no-source or `FROM DUAL` evaluation for `COUNT(column)`;
- aliases, mixed projections, multiple aggregate select items, aggregate
  comparisons, aggregate arithmetic, or nested aggregates;
- `GROUP BY`, `HAVING`, `ORDER BY`, `LIMIT`, window `OVER` clauses, joins,
  CTEs, subqueries, unions, locking clauses, query modifiers, optimizer hints,
  `INTO`, or arbitrary SQLite SQL pass-through;
- string, decimal, floating, temporal, JSON, enum, set, collation, or charset
  aggregate expression semantics;
- aggregate metadata parity, protocol column flags, exact optimizer behavior,
  index-only count planning, transaction isolation beyond existing SQLite
  statement visibility, temporary tables, views, privileges, SQL modes such as
  `IGNORE_SPACE`, or SQLite fork patches.

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
  source table, aggregate argument column, and optional predicate descriptors,
  rejects unsupported shapes, and builds a descriptor-driven aggregate plan.
- The catalog module remains authoritative for schema/table/column
  descriptors. `COUNT(column)` reads descriptors for table, aggregate
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
SELECT COUNT(column_name) FROM table_name [WHERE predicate]
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
    column_name comparison_operator signed_integer_literal
  | column_name IS NULL
  | column_name IS NOT NULL
  | ( predicate )
```

The aggregate function name must be directly adjacent to `(` under the default
SQL mode. Whitespace and comments are accepted inside the argument list.

MyLite Lemon-syntax grammar snippets:

```lemon
expression ::= COUNT LPAREN STAR RPAREN.
expression ::= COUNT LPAREN identifier RPAREN.
```

The parser may admit `COUNT(column)` anywhere the expression grammar is
currently shared, but the analyzer accepts it only as the sole select item in
the supported statement shape. `COUNT` remains usable as an ordinary unquoted
identifier in identifier positions where the parser admits nonreserved
keywords. Bare `COUNT` is not an aggregate call.

## Runtime Semantics

`SELECT COUNT(column_name) FROM table_name` returns one result row containing
the number of matched rows whose descriptor column value is not `NULL`. With a
supported `WHERE` predicate, the aggregate is evaluated over only matched rows.

If the matched row set is empty, or if all matched aggregate argument values
are `NULL`, the aggregate result value is `0`.

Successful count-column selects:

- return one result column;
- return one result row;
- use MySQL-compatible default label text for the selected expression,
  including the observed space after a block comment before the following
  token;
- use `affected_rows == 0` under the existing MyLite row-result convention;
- use `warning_count == 0` for supported forms;
- set the connection-local previous row count to `-1` after completion because
  the statement returns a result set.

Values stored in the argument column are represented inside MyLite's current
signed 64-bit physical storage range. `COUNT(column)` itself is represented as
decimal text for the current result API; the count is expected to fit SQLite's
count result range for this baseline.

## Schema, Table, And Column Resolution

Unqualified table names use the selected/default schema policy from the
schema/table lifecycle features. If no schema is selected, MyLite reports
MySQL-compatible no-database-selected diagnostics. Schema-qualified table names
first resolve the schema, then the table descriptor within that schema.

Reserved `_mylite_*` schema or table names are rejected before any SQLite SQL
is generated, using the current reserved-name diagnostics shared by
descriptor-backed table DML/query paths.

The source table must resolve to a MyLite persistent base-table descriptor.
When future non-base object descriptors exist, this aggregate path must reject
them before generating SQLite SQL.

The aggregate argument and optional predicate columns resolve from the MyLite
table descriptor, not SQLite metadata. The current descriptor catalog uses the
same ASCII case-insensitive identifier lookup policy as the preceding table
query and min/max aggregate slices. That is the expected behavior for this
slice until full identifier collation support is specified.

Unknown aggregate argument columns use the MySQL field-list unknown-column
diagnostic. Unknown predicate columns keep the existing predicate planner's
where-clause unknown-column diagnostic.

## Generated SQLite Handling

For a table-backed count-column aggregate, MyLite generates the equivalent
SQLite shape:

```sql
SELECT COUNT("physical_column") FROM "physical_table" [WHERE ...]
```

The physical table name comes from the MyLite table descriptor. The physical
column name comes from the resolved MyLite column descriptor. Every generated
SQLite identifier is quoted through the existing identifier-quoting helper.
Predicate literals are converted through MyLite's descriptor-driven predicate
conversion path and bound as prepared-statement parameters.

The implementation must not use SQLite metadata to discover the logical table
or column set. It must not interpolate predicate literals, user identifiers, or
untrusted text into generated SQL. It must not add SQLite fork patches or
custom SQLite functions for this feature.

SQLite performs the physical scan and aggregate. MyLite does not materialize
table rows in C to count them. Without indexes, SQLite may still scan the
physical table; index-only aggregate planning is outside this baseline.

## Diagnostics

Diagnostics must be deterministic and must preserve existing public result
cleanup behavior:

- syntax errors or unsupported grammar: current parser or analyzer diagnostic;
- missing default schema: MySQL-compatible no-database-selected diagnostic;
- unknown schema: MySQL-compatible unknown-database diagnostic;
- unknown table: MySQL-compatible table-not-found diagnostic;
- reserved schema/table names: current MyLite reserved-name diagnostic;
- unsupported object kind: deterministic unsupported object diagnostic once
  non-base descriptors exist;
- unknown aggregate argument column: unknown column in `field list`;
- unknown predicate column: existing unknown column in `where clause`;
- unsupported aggregate argument expression, table-qualified argument,
  literal argument, `NULL` argument, `DISTINCT`, qualified wildcard, multiple
  aggregate items, mixed projection, aliases, grouping, having, ordering,
  limiting, joins, CTEs, subqueries, and window clauses: deterministic syntax
  or unsupported-scope diagnostic;
- physical SQLite failure: current internal SQLite row-operation diagnostic;
- allocation failure: current allocation failure diagnostic;
- public API misuse: unchanged existing public API diagnostics.

Supported in-range count-column aggregate statements emit no warnings.

## Storage And Catalog Impact

`COUNT(column)` is read-only. It must not mutate user rows, catalog rows,
descriptor versions, descriptor caches, catalog generation,
`sqlite_schema_generation`, or the `.mylite` preamble. It must preserve the
shifted SQLite payload invariant and work after closing and reopening a file.

## Compatibility Notes

This slice moves MyLite from `COUNT(*)` only to `COUNT(*)` plus
`COUNT(column)` for one descriptor-backed base table. MySQL-compatible
`COUNT(1)`, `COUNT(NULL)`, `COUNT(DISTINCT column)`, table-qualified
arguments, expression arguments, aggregate aliases, `ORDER BY`, `LIMIT`,
grouping, and windows remain explicitly unsupported.
