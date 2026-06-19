# Baseline AVG Aggregate

## Status

This feature specifies a narrow aggregate-function slice for `AVG(column)`.
It builds on `mylite_execute()`, statement context, the MyLite parser
scaffold, durable catalog descriptors, integer/`NULL` row values,
descriptor-driven single-table `SELECT`, the baseline `WHERE` predicate
subset, and the existing `COUNT`, `MIN`, `MAX`, and `SUM` aggregate paths.

This is not full numeric expression or decimal aggregate support. It admits
exactly one aggregate select item, `AVG(column_name)`, over one persistent
base table with an optional baseline `WHERE` predicate. It does not add
literal/expression arguments, grouping, having, ordering, limiting, window
functions, joins, CTEs, subqueries, or MySQL's full exact decimal widening
behavior beyond the current MyLite signed-64 aggregate-sum envelope.
Executable `DISTINCT` support for the current aggregate envelope is specified
by `docs/specs/baseline-distinct-numeric-aggregates/specs.md`.

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
- Baseline count, min/max, and sum aggregate specs under `docs/specs/`
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

The expectation script
`packages/libmylite/tests/mysql_baseline_avg_aggregate_expectations.sh`
records the runtime probes for this feature. Observed behavior:

- `AVG(expr)` returns the average of non-`NULL` expression values in the rows
  retrieved by the select. Aggregate functions ignore `NULL` values unless
  otherwise documented.
- With no `GROUP BY`, an aggregate select groups over all matched rows.
- `AVG(column)` over supported integer columns returns an exact decimal result
  rendered by the command-line client with four fractional digits for the
  probed integer-family values.
- Empty matched sets and matched sets where the aggregate argument is `NULL`
  for every row return `NULL`.
- `AVG()` with no argument, `AVG(*)`, or `AVG(a, b)` fail with syntax error
  `1064`, SQLSTATE `42000`.
- `SELECT AVG(column)` and `SELECT AVG(column) FROM DUAL` fail with
  unknown-column error `1054`, SQLSTATE `42S22`, when the column name is not
  otherwise resolvable.
- `AVG(1)`, `AVG(NULL)`, `AVG(DISTINCT column)`, and `AVG(table.column)` are
  valid MySQL aggregate expressions. This original MyLite slice supports
  descriptor column arguments, including supported source-qualified descriptor
  columns. The current implementation also supports `AVG(DISTINCT expr)` where
  `expr` is in the documented aggregate argument envelope.
- MySQL returns exact decimal results even when the intermediate exact sum
  exceeds signed 64 bits; for example, averaging `9223372036854775807` and
  `1` as `BIGINT` returns `4611686018427387904.0000`. MyLite currently
  rejects aggregate execution when SQLite's signed-64 `SUM` overflows.
- Function names are case-insensitive. Default result labels preserve the
  source expression spelling, except MySQL inserts a space after a block
  comment before the following token in labels such as `AVG(/*x*/ n)` and
  `AVG/**/ (n)`.
- Under the default SQL mode, whitespace and comments between `AVG` and `(`
  are accepted as the built-in aggregate, unlike `SUM`.
- `ORDER BY` and `LIMIT` on a single aggregate row are valid MySQL syntax,
  with `LIMIT 0` suppressing the row. This slice rejects `ORDER BY` and
  `LIMIT` to preserve the current aggregate cardinality surface.
- A successful `SELECT AVG(column)` result set makes the following
  `ROW_COUNT()` return `-1` and leaves warning count `0`.

## Scope

The implementation must add:

- parser and AST support for `AVG(qualified_identifier)`;
- `AVG` as a nonreserved identifier where identifier grammar admits it;
- whitespace/comment-tolerant `AVG` function-call parsing matching observed
  MySQL 8.4.9 default behavior;
- descriptor-driven
  `SELECT AVG(column_name) [AS alias] FROM table_name [WHERE predicate]`;
- optional source table aliases matching the existing single-table aggregate
  source policy;
- unqualified and schema-qualified table-name resolution using the existing
  selected/default schema policy;
- one persistent MyLite base-table descriptor source only;
- aggregate argument column resolution from MyLite descriptors, including
  unqualified references and supported source-qualified references;
- explicit aggregate access to invisible descriptor columns, matching existing
  explicit projection and aggregate behavior;
- reuse of the existing baseline `WHERE` predicate subset and conversion
  rules;
- generated SQLite physical SQL built only from descriptors and stable
  physical table names;
- prepared-statement binding for predicate values;
- one result row with one text column containing either the MySQL-style
  four-fractional-digit average or `NULL`;
- a deterministic MyLite unsupported diagnostic when the underlying
  `SUM(column)` needed for `AVG(column)` exceeds the current signed-64
  aggregate-sum envelope;
- MySQL-compatible result column labels for the selected aggregate expression;
- result-set row-count state matching existing `SELECT` behavior;
- deterministic diagnostics for unsupported aggregate syntax and wider MySQL
  aggregate/select forms;
- tests and a MySQL 8.4.9 expectation artifact for supported behavior and
  deliberately rejected wider forms.

Existing `COUNT`, `MIN`, `MAX`, and `SUM` behavior must remain unchanged.

## Non-Goals

This feature must not implement:

- `AVG(expr)` for literals, `NULL`, arithmetic, functions, parenthesized
  expression arguments, or general expression arguments;
- no-source or `FROM DUAL` aggregate evaluation;
- multiple aggregate select items, mixed projections, aggregate comparisons,
  aggregate arithmetic, or nested aggregates;
- MySQL's exact decimal `AVG` result behavior when the required exact sum
  exceeds MyLite's current signed-64 aggregate-sum envelope;
- `GROUP BY`, `HAVING`, `ORDER BY`, `LIMIT`, window `OVER` clauses, joins,
  CTEs, subqueries, unions, locking clauses, query modifiers, optimizer hints,
  `INTO`, or arbitrary SQLite SQL pass-through;
- string, decimal, floating, temporal, JSON, enum, set, collation, or charset
  aggregate expression semantics;
- aggregate metadata parity, protocol column flags, exact optimizer behavior,
  transaction isolation beyond existing SQLite statement visibility,
  temporary tables, views, privileges, SQL mode variants, or SQLite fork
  patches.

## Ownership Boundary

- The public API remains unchanged. `mylite_execute()` owns public validation,
  result-handle ownership, statement-boundary row-count state, and failure
  cleanup.
- Statement context owns diagnostics reset, warning count, and statement
  completion. Successful `AVG` selects are result-set statements and therefore
  store `-1` as the connection-local previous row count.
- Lexer/parser/AST own syntax admission, `AVG` keyword classification, and
  source spans. They remain independent of runtime, catalog, storage, and
  SQLite.
- Analyzer/planner code recognizes the one-item aggregate shape, resolves the
  source table, aggregate argument column, and optional predicate descriptors,
  rejects unsupported shapes, and builds a descriptor-driven aggregate plan.
- The catalog module remains authoritative for schema/table/column
  descriptors. `AVG(column)` reads descriptors for table, aggregate argument,
  and predicate resolution but does not mutate catalog rows, descriptor
  versions, descriptor caches, catalog generation, or
  `sqlite_schema_generation`.
- Runtime execution generates SQLite SQL against the descriptor-owned physical
  table and binds only predicate parameters. SQLite owns scanning and the
  `SUM(column), COUNT(column)` aggregate pass for supported inputs; MyLite owns
  signed-64 overflow mapping and the final MySQL-style decimal text formatting.
- The result builder owns the one-column text/null result.
- Storage/VFS owns the `.mylite` preamble and shifted SQLite payload boundary.
  Aggregate queries do not touch byte range `[0, 4096)`.

## Supported SQL Grammar

Supported subset:

```sql
SELECT AVG(column_name) [AS alias] FROM table_name [WHERE predicate]
```

`table_name` uses the existing table lifecycle subset:

```sql
table_name:
    identifier
  | identifier.identifier
```

The aggregate argument is one descriptor column reference:

```sql
avg_argument:
    column_name
  | table_name.column_name
  | schema_name.table_name.column_name
```

The supported predicate subset is exactly the subset from
`baseline-select-where-lifecycle`:

```sql
predicate:
    column_name comparison_operator signed_integer_literal
  | column_name IS NULL
  | column_name IS NOT NULL
  | ( predicate )
```

Unlike the current `SUM` slice, the aggregate function name may have
whitespace or comments before `(`.

MyLite Lemon-syntax grammar snippets:

```lemon
expression ::= AVG LPAREN qualified_identifier RPAREN.
```

The parser may admit `AVG(column)` anywhere the expression grammar is
currently shared, but the analyzer accepts it only as the sole select item in
the supported aggregate select shape.

Unsupported examples for this slice:

```sql
SELECT AVG(1) FROM t;
SELECT AVG(NULL) FROM t;
SELECT AVG(n + 1) FROM t;
SELECT AVG(n), COUNT(*) FROM t;
SELECT AVG(n) FROM t ORDER BY id;
SELECT AVG(n) FROM t LIMIT 1;
SELECT AVG(n) OVER () FROM t;
```

## Name Resolution and Case Sensitivity

Table resolution follows the existing selected/default schema policy:

- unqualified table names require a selected schema;
- schema-qualified table names resolve in the named schema and do not require
  it to be selected;
- missing selected schema, unknown schemas, unknown tables, and reserved
  `_mylite_*` schema/table names reuse the existing diagnostics.

Aggregate argument and predicate columns resolve from MyLite descriptors, not
SQLite metadata. Column matching follows the current descriptor catalog
case-insensitive lookup behavior used by explicit projections and existing
aggregates. Source-qualified aggregate arguments may use the source alias when
one exists, or table/schema-table qualifiers for unaliased sources. Unknown
aggregate argument columns use field-list unknown-column diagnostics; unknown
predicate columns use where-clause diagnostics.

Only persistent base-table descriptors are supported. Unsupported object kinds
must be rejected once non-base-table descriptors exist.

## Value Semantics

`AVG(column)` ignores `NULL` argument values. If no rows match, or if every
matched row has `NULL` for the argument, the result value is `NULL`.

For this slice, the aggregate argument must be an integer-family descriptor
column currently stored as SQLite integer or `NULL`: `TINYINT`, `SMALLINT`,
`MEDIUMINT`, `INT`, `INTEGER`, `BIGINT`, their supported `UNSIGNED` forms,
and the currently supported integer aliases such as `BOOL` and `BOOLEAN`.

Successful non-`NULL` averages are formatted as exact decimal text with four
fractional digits for this integer subset. The value is computed from the
signed-64 SQLite `SUM(column)` and `COUNT(column)` results:

- integer part is the signed quotient truncated toward zero;
- fractional part is rounded to four digits, half away from zero, matching
  observed MySQL exact-value output for integer averages;
- carry from fractional rounding increments the absolute integer part;
- negative averages use a leading `-` when the rounded absolute value is not
  zero.

If SQLite reports signed integer overflow while computing `SUM(column)`,
MyLite returns a deterministic unsupported diagnostic instead of returning an
approximate value or leaking raw SQLite text. MySQL exact decimal results for
such cases are intentionally deferred.

## Physical SQLite Handling

No SQLite fork patch is required. The implementation uses MyLite
wrapper/translation over public SQLite prepared statements.

Generated SQL shape:

```sql
SELECT SUM("physical_column"), COUNT("physical_column")
FROM "physical_table"
[WHERE "physical_predicate_column" <op> ?]
```

Identifiers are generated only from descriptors and quoted with the existing
SQLite identifier quoting helper. Predicate values are bound parameters.
Table names use stable physical names such as `_mylite_user_table_<table_id>`.

SQLite owns the scan, filtering, aggregate pass, pager, durability, and VFS
interaction. MyLite owns descriptor resolution, generated SQL construction,
predicate binding, signed-64 overflow classification, and decimal result text
formatting.

## Result Behavior

Successful `AVG(column)` returns through the existing row-result API:

- one column;
- one row, unless a future admitted `LIMIT 0` suppresses it;
- `NULL` text pointer for SQL `NULL`;
- `affected_rows == 0`;
- `warning_count == 0`;
- subsequent `ROW_COUNT()` returns `-1`.

Default column labels preserve source text for the accepted aggregate
expression, including case, spacing, qualified argument spelling, and block
comment label spacing. Explicit select-item aliases override the default
label.

## Diagnostics

Supported `AVG(column)` forms do not produce warnings.

Errors that remain errors include syntax errors, missing default schema,
unknown schema, unknown table, reserved target names, unsupported object kind,
unknown aggregate argument column, unknown predicate column, unsupported
aggregate arguments, unsupported aggregate select shapes, unsupported
`ORDER BY`/`LIMIT`/`GROUP BY`/`HAVING`, signed-64 aggregate-sum overflow,
physical SQLite failures, allocation failures, and public API misuse.

MyLite-specific unsupported messages:

| Condition | Message |
| --- | --- |
| multiple aggregate or mixed select items | `AVG(column) supports exactly one aggregate select item` |
| unsupported optional select clauses | `AVG(column) supports only WHERE` |
| no descriptor table source | `AVG(column) supports only descriptor-backed table reads` |
| unsupported aggregate argument after parsing | `AVG(column) supports only descriptor columns` |
| non-integer descriptor argument | `AVG(column) supports only integer descriptor columns` |
| aggregate sum exceeds signed-64 range | `AVG(column) intermediate sum exceeds MyLite signed 64-bit range` |

## Tests

The implementation tests must cover:

- parser and lexer coverage for `AVG` as aggregate and identifier;
- successful `AVG(column)` over `TINYINT`, `SMALLINT`, `MEDIUMINT`, `INT`,
  `INTEGER`, `BIGINT`, supported unsigned forms, `BOOL`, and `BOOLEAN`;
- nullable inputs, all-`NULL` inputs, empty tables, and no-match predicates;
- MySQL-style four-fractional-digit formatting, including negative values and
  fractional rounding;
- source aliases, qualified aggregate arguments, quoted identifiers, invisible
  columns, and aliases;
- baseline `WHERE` predicate reuse, including comparisons, `<=>`, `IS NULL`,
  and `IS NOT NULL`;
- missing default schema, unknown schema, unknown table, reserved target names,
  unknown aggregate columns, and unknown predicate columns;
- unsupported literal, `NULL`, expression, star, multi-argument,
  multiple-select-item, `ORDER BY`, `LIMIT`, grouped, window, join, CTE, and
  subquery forms;
- deterministic unsupported diagnostics for signed-64 aggregate-sum overflow;
- result row count, warning count, absence of affected rows, and subsequent
  `ROW_COUNT()`;
- reopen persistence, rename/drop behavior, independent file-backed handles,
  and `.mylite` preamble preservation;
- regression coverage for existing lexer, parser, count/min/max/sum aggregate,
  row-values, select-where, order/limit, delete, update, and file-backed
  storage tests.

## Compatibility Documentation

Update `COMPATIBILITY.md`, `docs/compatibility/functions-aggregate.md`, and
`docs/compatibility/sql-query-expressions.md` for the exact limited
`AVG(column)` subset. Do not claim full `AVG`, general expression
arguments, exact decimal widening beyond this envelope, grouping, ordering,
limiting, windows, joins, or protocol metadata parity.
