# Baseline MIN/MAX Aggregate

## Status

This feature specifies a narrow aggregate-function slice for `MIN(column)` and
`MAX(column)`. It builds on `mylite_execute()`, statement context, the MyLite
parser scaffold, durable catalog descriptors, schema/table lifecycle,
integer/`NULL` row values, descriptor-driven single-table `SELECT`, the
baseline `WHERE` predicate subset, and the existing `COUNT(*)` aggregate path.

The feature is intentionally not full aggregate support. It admits exactly one
aggregate select item, either `MIN(column_name)` or `MAX(column_name)`, over one
persistent base table with an optional baseline `WHERE` predicate. It does not
add expression arguments, aliases, multiple aggregate select items, grouping,
having, ordering, limiting, window functions, joins, CTEs, subqueries, or
general aggregate expression evaluation.

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

- `MIN(expr)` and `MAX(expr)` are aggregate functions. MySQL documentation
  lists them as returning the minimum and maximum values, and aggregate
  functions ignore `NULL` values unless documented otherwise.
- With no `GROUP BY`, an aggregate select groups over all matched rows.
- `MIN(column)` and `MAX(column)` over supported integer columns return the
  smallest and largest non-`NULL` values in the matched row set.
- Empty matched sets and matched sets where the aggregate argument is `NULL`
  for every row return `NULL`.
- `MIN()` and `MAX()` with no argument, more than one argument, or `*` fail
  with syntax error `1064`, SQLSTATE `42000`.
- `MIN(1)`, `MAX(1)`, `MIN(NULL)`, `MAX(NULL)`, and `MIN(DISTINCT column)` are
  valid MySQL aggregate expressions, but remain outside this MyLite slice
  because they require expression argument support or distinct aggregation.
- `SELECT MIN(column) FROM DUAL` fails as an unknown column when the name is
  not otherwise resolvable.
- Function names are case-insensitive. Default result labels preserve the
  source spelling for the selected expression, except MySQL inserts a space
  after a block comment before the following token in labels such as
  `MAX(/*x*/ n)`.
- Under the default SQL mode, whitespace or comments between `MIN`/`MAX` and
  `(` resolve as stored-function calls. In a test schema with no stored
  function, `MIN (n)` and `MIN/**/(n)` fail with error `1630`, SQLSTATE
  `42000`.
- `ORDER BY` and `LIMIT` on a single aggregate row are valid MySQL syntax, with
  `LIMIT 0` suppressing the row. `GROUP BY` returns one aggregate row per
  group. This slice rejects `ORDER BY`, `LIMIT`, and `GROUP BY` to avoid
  widening aggregate cardinality and grouping semantics.
- A successful `SELECT MIN(column)` or `SELECT MAX(column)` result set makes
  the following `ROW_COUNT()` return `-1` and leaves warning count `0`.

The reproducible probe lives in
`packages/libmylite/tests/mysql_baseline_min_max_aggregate_expectations.sh`.

## Scope

The implementation must add:

- parser and AST support for no-space `MIN(identifier)` and `MAX(identifier)`;
- `MIN` and `MAX` as nonreserved identifiers where identifier grammar admits
  them;
- descriptor-driven `SELECT MIN(column_name) FROM table_name [WHERE predicate]`
  and `SELECT MAX(column_name) FROM table_name [WHERE predicate]`;
- unqualified and schema-qualified table-name resolution using the existing
  selected/default schema policy;
- one persistent MyLite base-table descriptor source only;
- unqualified aggregate argument column resolution from MyLite descriptors;
- explicit aggregate access to invisible descriptor columns, matching the
  existing explicit projection behavior;
- reuse of the existing baseline `WHERE` predicate subset and conversion rules;
- generated SQLite physical SQL built only from descriptors and stable physical
  table names;
- prepared-statement binding for predicate values;
- one result row with one text column containing either the aggregate integer
  value or `NULL`;
- MySQL-compatible result column labels for the selected aggregate expression;
- result-set row-count state matching existing `SELECT` behavior;
- deterministic diagnostics for unsupported aggregate syntax and wider MySQL
  aggregate/select forms;
- tests and a MySQL 8.4.9 expectation artifact for supported behavior and
  deliberately rejected wider forms.

## Non-Goals

This feature must not implement:

- `MIN(expr)` or `MAX(expr)` for literal, arithmetic, function, parenthesized
  expression, table-qualified, qualified wildcard, `DISTINCT`, or general
  expression arguments;
- no-source or `FROM DUAL` aggregate evaluation;
- aliases, mixed projections, multiple aggregate select items, nested
  aggregates, aggregate comparisons, or aggregate arithmetic;
- `GROUP BY`, `HAVING`, `ORDER BY`, `LIMIT`, window `OVER` clauses, joins,
  CTEs, subqueries, unions, locking clauses, query modifiers, optimizer hints,
  `INTO`, or arbitrary SQLite SQL pass-through;
- string, decimal, floating, temporal, JSON, enum, set, collation, or charset
  aggregate semantics;
- aggregate metadata parity, protocol column flags, exact optimizer behavior,
  index-only min/max planning, transaction isolation beyond existing SQLite
  statement visibility, temporary tables, views, privileges, SQL modes such as
  `IGNORE_SPACE`, or SQLite fork patches.

## Ownership Boundary

- The public API remains unchanged. `mylite_execute()` owns public validation,
  result-handle ownership, statement-boundary row-count state, and failure
  cleanup.
- Statement context owns diagnostics reset, warning count, and statement
  completion. Successful min/max selects are result-set statements and
  therefore store `-1` as the connection-local previous row count.
- Lexer/parser/AST own syntax admission, the no-space function-call rule, and
  source spans. They remain independent of runtime, catalog, storage, and
  SQLite.
- Analyzer/planner code recognizes the one-item aggregate shape, resolves the
  source table, aggregate argument column, and optional predicate descriptors,
  rejects unsupported shapes, and builds a descriptor-driven aggregate plan.
- The catalog module remains authoritative for schema/table/column
  descriptors. Min/max reads descriptors for table, aggregate argument, and
  predicate resolution but does not mutate catalog rows, descriptor versions,
  descriptor caches, catalog generation, or `sqlite_schema_generation`.
- Runtime execution generates SQLite SQL against the descriptor-owned physical
  table and binds only predicate parameters.
- The result builder owns the one-column result and text/null row value.
- Storage/VFS owns the `.mylite` preamble and shifted SQLite payload boundary.
  Aggregate queries do not touch byte range `[0, 4096)`.

## Supported SQL Grammar

Supported subset:

```sql
SELECT MIN(column_name) FROM table_name [WHERE predicate]
SELECT MAX(column_name) FROM table_name [WHERE predicate]
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
expression ::= min_max_aggregate_function.

min_max_aggregate_function ::= MIN LPAREN identifier RPAREN.
min_max_aggregate_function ::= MAX LPAREN identifier RPAREN.
```

The parser may admit `MIN(column)` and `MAX(column)` anywhere the expression
grammar is currently shared, but the analyzer accepts them only as the sole
select item in the supported statement shape. `MIN` and `MAX` remain usable as
ordinary unquoted identifiers in identifier positions where the parser admits
nonreserved keywords. Bare `MIN` and `MAX` are not aggregate calls.

## Runtime Semantics

`SELECT MIN(column_name) FROM table_name` returns one result row containing the
smallest non-`NULL` stored value for the descriptor column among all rows in
the descriptor-owned physical table. `MAX` returns the largest non-`NULL`
stored value. With a supported `WHERE` predicate, the aggregate is evaluated
over only matched rows.

If the matched row set is empty, or if all matched aggregate argument values
are `NULL`, the aggregate result value is `NULL`.

Successful min/max selects:

- return one result column;
- return one result row;
- use MySQL-compatible default label text for the selected expression,
  including the observed space after a block comment before the following
  token;
- use `affected_rows == 0` under the existing MyLite row-result convention;
- use `warning_count == 0` for supported forms;
- set the connection-local previous row count to `-1` after completion because
  the statement returns a result set.

Values are represented inside MyLite's current signed 64-bit physical storage
range. This is sufficient for the current integer row-value baseline,
including `INT UNSIGNED` values through `4294967295` and `BIGINT UNSIGNED`
values only through the currently supported signed-64 maximum
`9223372036854775807`.

## Schema, Table, And Column Resolution

Unqualified table names use the existing selected/default schema policy. If no
schema is selected, MyLite returns the existing missing-default-schema
diagnostic. Schema-qualified names resolve the schema first, then the table.

Names beginning with MyLite's reserved `_mylite_` prefix are rejected before
generated SQLite SQL is built.

The aggregate argument and predicate columns are resolved from MyLite column
descriptors, not from SQLite metadata. Unknown aggregate, predicate, or order
columns are reported deterministically through the existing unknown-column
diagnostic path for supported descriptor-backed selects. Current descriptor
catalog name matching remains the existing binary/case-sensitive baseline.

Unsupported object kinds must be rejected once non-base-table descriptors
exist.

## Physical SQLite Handling

Table-backed min/max selects lower to standard SQLite SQL:

```sql
SELECT MIN("physical_column") FROM "physical_table" [WHERE "physical_column" op ?]
SELECT MAX("physical_column") FROM "physical_table" [WHERE "physical_column" op ?]
```

Rules:

- physical table names come from MyLite table descriptors;
- aggregate and predicate column names come from MyLite column descriptors;
- every generated SQLite identifier is quoted;
- predicate literals are converted by MyLite before execution and bound as
  prepared-statement parameters;
- no user SQL text, user literal text, or SQLite metadata lookup is used as
  authority;
- no SQLite optional aggregate extension, custom function, virtual table, VFS
  change, or fork patch is required.

SQLite's built-in `MIN()` and `MAX()` are sufficient because the admitted
columns store only MyLite-owned integer or `NULL` values in physical SQLite
integer/null cells.

## Diagnostics

Supported `MIN(column)` and `MAX(column)` calls do not produce warnings.

Diagnostics follow existing baseline conventions where MyLite has not yet
implemented full MySQL expression or metadata behavior:

- whitespace/comment-separated calls such as `MIN (column)` and
  `MIN/**/(column)` may fail with MyLite's existing syntax/unsupported
  diagnostic rather than MySQL's stored-function error `1630`;
- unsupported aggregate arguments such as `MIN(1)`, `MAX(NULL)`,
  `MIN(DISTINCT column)`, `MIN()`, `MIN(column, column)`, `MIN(*)`, and
  `MIN(table.column)` fail deterministically, either through parse error
  `1064` or the existing unsupported-statement diagnostic class;
- unsupported aggregate select shapes such as no source, `FROM DUAL`, aliases,
  mixed projections, multiple aggregate items, `ORDER BY`, `LIMIT`, `GROUP BY`,
  `HAVING`, joins, subqueries, CTEs, and windows fail deterministically;
- missing default schema, unknown schema, unknown table, reserved schema/table
  names, unsupported object kinds, unknown aggregate argument column, and
  unknown predicate column use existing descriptor-resolution diagnostics;
- physical SQLite failures are converted through existing physical-row read
  diagnostics;
- allocation failures use existing `MYLITE_NOMEM` and diagnostics behavior;
- public API misuse remains unchanged because this feature adds no public
  surface.

## Test Plan

Fast C tests under `packages/libmylite/tests/` must cover:

- parser acceptance for `MIN(column)` and `MAX(column)`, lower/mixed case,
  whitespace/comments inside the argument list, parenthesized aggregates, and
  preserved source spans;
- parser or runtime rejection for whitespace/comment before `(`, no arguments,
  more than one argument, `*`, literals, `NULL`, `DISTINCT`, qualified
  arguments, aliases, mixed projections, multiple aggregate items, no-source,
  `FROM DUAL`, `ORDER BY`, `LIMIT`, `GROUP BY`, `HAVING`, CTEs, joins,
  subqueries, and windows where currently representable;
- successful min/max over `INT`, `INTEGER`, `BIGINT`, and unsigned integer
  families within the current signed-64 physical range;
- nullable columns, all-`NULL` columns, empty tables, and no-match predicates
  returning `NULL`;
- baseline `WHERE` predicate reuse, including comparisons, `<=>` with
  non-`NULL` integer right operands, `IS NULL`, and `IS NOT NULL`;
- schema-qualified and unqualified target table resolution, missing default
  schema, unknown schema, unknown table, and reserved `_mylite_*` names;
- unknown aggregate argument columns and unknown predicate columns;
- result labels, warning count, affected rows, absence of extra rows, and
  following `ROW_COUNT() == -1`;
- reopen persistence, table rename, truncate, drop behavior where applicable,
  independent file-backed handles, and `.mylite` preamble preservation;
- zero-initialized cleanup for any new planner/result objects;
- existing parser, runtime lifecycle, count aggregate, select where, row
  values, storage, VFS, catalog, diagnostics, statement context, result
  metadata, and registration tests still pass.

MySQL-runtime expectations must be verified by:

```sh
./packages/libmylite/tests/mysql_baseline_min_max_aggregate_expectations.sh
```

Implementation verification must run:

1. `cmake --build --preset dev`
2. The new CTest entry plus relevant parser/count/select lifecycle tests.
3. `./packages/libmylite/tests/mysql_baseline_min_max_aggregate_expectations.sh`
4. `cmake --workflow --preset check`

## Compatibility Documentation

Update `COMPATIBILITY.md` and `docs/compatibility/functions-aggregate.md` to
mark `MIN()` and `MAX()` as limited one-column descriptor-backed aggregates.
Update `docs/compatibility/sql-query-expressions.md` only to describe the
exact admitted aggregate projection shape. Do not claim aliases, grouping,
ordering, limiting, expression arguments, expression metadata, window
functions, string/collation behavior, or optimizer/index behavior.
