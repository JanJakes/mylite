# Baseline SUM Aggregate

## Status

This feature specifies a narrow aggregate-function slice for `SUM(column)` and
WordPress-shaped `SUM(string_length(column))`.
It builds on `mylite_execute()`, statement context, the MyLite parser
scaffold, durable catalog descriptors, schema/table lifecycle, integer/`NULL`
row values, descriptor-driven single-table `SELECT`, the baseline `WHERE`
predicate subset, and the existing `COUNT`, `MIN`, and `MAX` aggregate paths.

This is not full numeric expression or decimal aggregate support. It admits
exactly one aggregate select item, `SUM(column_name)` or
`SUM(LENGTH(column_name))` and equivalent string-length aliases, over one
persistent base table with an optional baseline `WHERE` predicate. It does not
add literal/`NULL` arguments, general expression arguments, grouping, having,
ordering, limiting, window functions, joins, CTEs, subqueries, or MySQL's exact
decimal result widening beyond MyLite's current signed-64 result envelope.
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
- Baseline count and min/max aggregate specs under `docs/specs/`
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
`packages/libmylite/tests/mysql_baseline_sum_aggregate_expectations.sh`
records the runtime probes for this feature. Observed behavior:

- `SUM(expr)` returns the sum of non-`NULL` expression values in the rows
  retrieved by the select. Aggregate functions ignore `NULL` values unless
  otherwise documented.
- With no `GROUP BY`, an aggregate select groups over all matched rows.
- `SUM(column)` over supported integer columns returns the exact sum of the
  non-`NULL` matched values.
- Empty matched sets and matched sets where the aggregate argument is `NULL`
  for every row return `NULL`.
- `SUM()` with no argument, `SUM(*)`, or `SUM(a, b)` fail with syntax error
  `1064`, SQLSTATE `42000`.
- `SELECT SUM(column)` and `SELECT SUM(column) FROM DUAL` fail with
  unknown-column error `1054`, SQLSTATE `42S22`, when the column name is not
  otherwise resolvable.
- `SUM(1)`, `SUM(NULL)`, `SUM(DISTINCT column)`, `SUM(table.column)`, and
  `SUM(LENGTH(column))` are valid MySQL aggregate expressions. This original
  MyLite slice supports descriptor column arguments, supported source-qualified
  descriptor columns, and the limited string-length expression form. The
  current implementation also supports `SUM(DISTINCT expr)` where `expr` is in
  the documented aggregate argument envelope.
- MySQL returns exact decimal results for integer `SUM`; for example, summing
  two signed-64-range `BIGINT` values can return `9223372036854775808`, and
  summing two supported-range `BIGINT UNSIGNED` values can return
  `18446744073709551614`.
- Function names are case-insensitive. Default result labels preserve the
  source expression spelling, except MySQL inserts a space after a block
  comment before the following identifier in labels such as `SUM(/*x*/ n)`.
- Under the default SQL mode, whitespace or comments between `SUM` and `(`
  resolve as stored-function calls. In a test schema with no stored function,
  `SUM (n)` and `SUM/**/(n)` fail with error `1630`, SQLSTATE `42000`.
- `ORDER BY` and `LIMIT` on a single aggregate row are valid MySQL syntax,
  with `LIMIT 0` suppressing the row. This slice rejects `ORDER BY` and
  `LIMIT` to preserve the current aggregate cardinality surface.
- A successful `SELECT SUM(column)` result set makes the following
  `ROW_COUNT()` return `-1` and leaves warning count `0`.

## Scope

The implementation must add:

- parser and AST support for no-space `SUM(qualified_identifier)` and
  `SUM(string_length_expression)`;
- `SUM` as a nonreserved identifier where identifier grammar admits it;
- descriptor-driven
  `SELECT SUM(column_name) [AS alias] FROM table_name [WHERE predicate]`;
- descriptor-driven
  `SELECT SUM(LENGTH(column_name)) [AS alias] FROM table_name [WHERE predicate]`
  and equivalent `OCTET_LENGTH`, `BIT_LENGTH`, `CHAR_LENGTH`, and
  `CHARACTER_LENGTH` arguments over the existing row-scalar string-length
  envelope;
- optional source table aliases matching the existing single-table aggregate
  source policy;
- unqualified and schema-qualified table-name resolution using the existing
  selected/default schema policy;
- one persistent MyLite base-table descriptor source only;
- aggregate argument column resolution from MyLite descriptors, including
  unqualified references and supported source-qualified references;
- explicit aggregate access to invisible descriptor columns, matching the
  existing explicit projection and aggregate behavior;
- reuse of the existing baseline `WHERE` predicate subset and conversion
  rules;
- generated SQLite physical SQL built only from descriptors and stable
  physical table names;
- prepared-statement binding for predicate values;
- one result row with one text column containing either the decimal signed-64
  sum or `NULL`;
- a deterministic MyLite unsupported diagnostic when the `SUM(column)` result
  exceeds the current signed-64 result envelope;
- MySQL-compatible result column labels for the selected aggregate expression;
- result-set row-count state matching existing `SELECT` behavior;
- deterministic diagnostics for unsupported aggregate syntax and wider MySQL
  aggregate/select forms;
- tests and a MySQL 8.4.9 expectation artifact for supported behavior and
  deliberately rejected wider forms.

Existing `COUNT`, `MIN`, and `MAX` behavior must remain unchanged.

## Non-Goals

This feature must not implement:

- standalone `SUM(expr)` for literals, `NULL`, arithmetic, functions outside
  the string-length family, parenthesized expression arguments, or
  general expression arguments; the narrow grouped `SUM(column + column)`
  metadata bridge is specified by the grouped aggregate slice;
- no-source or `FROM DUAL` aggregate evaluation;
- multiple aggregate select items, mixed projections, aggregate comparisons,
  aggregate arithmetic, or nested aggregates;
- MySQL's exact decimal `SUM` result type beyond MyLite's current signed-64
  result envelope;
- `GROUP BY`, `HAVING`, `ORDER BY`, `LIMIT`, window `OVER` clauses, joins,
  CTEs, subqueries, unions, locking clauses, query modifiers, optimizer hints,
  `INTO`, or arbitrary SQLite SQL pass-through;
- string, decimal, floating, temporal, JSON, enum, set, collation, or charset
  aggregate expression semantics;
- aggregate metadata parity, protocol column flags, exact optimizer behavior,
  transaction isolation beyond existing SQLite statement visibility,
  temporary tables, views, privileges, SQL modes such as `IGNORE_SPACE`, or
  SQLite fork patches.

## Ownership Boundary

- The public API remains unchanged. `mylite_execute()` owns public validation,
  result-handle ownership, statement-boundary row-count state, and failure
  cleanup.
- Statement context owns diagnostics reset, warning count, and statement
  completion. Successful `SUM` selects are result-set statements and
  therefore store `-1` as the connection-local previous row count.
- Lexer/parser/AST own syntax admission, the no-space function-call rule, and
  source spans. They remain independent of runtime, catalog, storage, and
  SQLite.
- Analyzer/planner code recognizes the one-item aggregate shape, resolves the
  source table, aggregate argument column, and optional predicate descriptors,
  rejects unsupported shapes, and builds a descriptor-driven aggregate plan.
- The catalog module remains authoritative for schema/table/column
  descriptors. `SUM(column)` reads descriptors for table, aggregate argument,
  and predicate resolution but does not mutate catalog rows, descriptor
  versions, descriptor caches, catalog generation, or
  `sqlite_schema_generation`.
- Runtime execution generates SQLite SQL against the descriptor-owned physical
  table and binds only predicate parameters. SQLite owns scanning and in-range
  integer aggregation; MyLite owns mapping signed-64 aggregate overflow to a
  deterministic unsupported diagnostic.
- The result builder owns the one-column text/null result.
- Storage/VFS owns the `.mylite` preamble and shifted SQLite payload boundary.
  Aggregate queries do not touch byte range `[0, 4096)`.

## Supported SQL Grammar

Supported subset:

```sql
SELECT SUM(column_name) [AS alias] FROM table_name [WHERE predicate]
SELECT SUM(string_length_expression) [AS alias] FROM table_name [WHERE predicate]
```

`table_name` uses the existing table lifecycle subset:

```sql
table_name:
    identifier
  | identifier.identifier
```

The aggregate argument is one descriptor column reference or one supported
string-length expression over the existing row-scalar string-length operand
envelope:

```sql
sum_argument:
    column_name
  | table_name.column_name
  | schema_name.table_name.column_name
  | string_length_expression

string_length_expression:
    LENGTH(expression)
  | OCTET_LENGTH(expression)
  | BIT_LENGTH(expression)
  | CHAR_LENGTH(expression)
  | CHARACTER_LENGTH(expression)
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

The aggregate function name must be directly adjacent to `(` under the default
SQL mode. Whitespace and comments are accepted inside the argument list.

MyLite Lemon-syntax grammar snippets:

```lemon
expression ::= SUM LPAREN sum_aggregate_argument RPAREN.
sum_aggregate_argument ::= qualified_identifier.
sum_aggregate_argument ::= string_length_expression.
sum_aggregate_argument ::= qualified_identifier PLUS qualified_identifier.
```

The parser may admit `SUM(column)` and `SUM(string_length_expression)` anywhere
the expression grammar is currently shared, and may admit the grouped metadata
bridge `SUM(column + column)`, but the analyzer accepts only the statement
shapes documented by the standalone and grouped aggregate slices. `SUM`
remains usable as an ordinary unquoted identifier in identifier positions where
the parser admits nonreserved keywords. Bare `SUM` is not an aggregate call.

## Runtime Semantics

`SELECT SUM(column_name) FROM table_name` returns one result row containing the
sum of non-`NULL` stored values for the descriptor column among all rows in
the descriptor-owned physical table. With a supported `WHERE` predicate, the
aggregate is evaluated over only matched rows.

If the matched row set is empty, or if all matched aggregate argument values
are `NULL`, the aggregate result value is `NULL`.

Successful supported `SUM` selects:

- return one result column;
- return one result row;
- use MySQL-compatible default label text for the selected expression,
  including the observed space after a block comment before the following
  token;
- use a select-item alias label when one is specified;
- use `affected_rows == 0` under the existing MyLite row-result convention;
- use `warning_count == 0` for supported forms;
- set the connection-local previous row count to `-1` after completion because
  the statement returns a result set.

Values are represented inside MyLite's current signed-64 physical result
range. In-range sums include supported `INT UNSIGNED` sums such as
`4294967298` when they fit signed 64 bits. MySQL-compatible exact decimal
results outside signed 64 bits are deferred; MyLite returns a deterministic
unsupported diagnostic instead of returning an imprecise value or leaking a
raw SQLite error.

## Schema, Table, And Column Resolution

Unqualified table names use the existing selected/default schema policy. If no
schema is selected, MyLite returns the existing missing-default-schema
diagnostic. Schema-qualified names resolve the schema first, then the table.

Names beginning with MyLite's reserved `_mylite_` prefix are rejected before
generated SQLite SQL is built.

The aggregate argument and predicate columns are resolved from MyLite column
descriptors, not from SQLite metadata. Unknown aggregate and predicate columns
are reported deterministically through the existing unknown-column diagnostic
path for supported descriptor-backed selects. Current descriptor catalog
column name matching follows the existing ASCII case-insensitive baseline
after identifier unquoting.

Unsupported object kinds must be rejected once non-base-table descriptors
exist.

## Physical SQLite Handling

MyLite generates descriptor-built SQL shaped as:

```sql
SELECT SUM("physical_column") FROM "physical_table" [WHERE "physical_column" op ?]
SELECT SUM(<row-scalar-string-length-sql>) FROM "physical_table" [WHERE ...]
```

All generated identifiers are quoted. The physical table name comes from the
MyLite table descriptor, and the aggregate/predicate column names come from
MyLite column descriptors. Predicate literals are converted by MyLite and
bound as parameters.

The implementation must not use arbitrary user SQL pass-through, temporary
tables, custom SQLite forks, or optional SQLite syntax. Public SQLite
prepared-statement APIs are sufficient for this slice.

## Diagnostics

Supported `SUM(column)` forms do not produce warnings.

Errors that remain errors include syntax errors, missing default schema,
unknown schema, unknown table, reserved target names, unsupported object kind,
unknown aggregate argument column, unknown predicate column, unsupported
aggregate arguments, unsupported aggregate select shapes, unsupported
`ORDER BY`/`LIMIT`/`GROUP BY`/`HAVING`, signed-64 result overflow, physical
SQLite failures, allocation failures, and public API misuse.

MyLite-specific unsupported messages:

| Condition | Message |
| --- | --- |
| multiple aggregate or mixed select items | `SUM(column) supports exactly one aggregate select item` |
| unsupported optional select clauses | `SUM(column) supports only WHERE` |
| no descriptor table source | `SUM(column) supports only descriptor-backed table reads` |
| unsupported aggregate argument after parsing | `SUM(column) supports only descriptor columns` |
| unsupported row-scalar division after parsing | `aggregate row-scalar arguments do not support / division` |
| aggregate result exceeds signed-64 range | `SUM(column) result exceeds MyLite signed 64-bit range` |

## Tests

The implementation tests must cover:

- parser acceptance for `SUM(column)`, lower/mixed case, whitespace/comments
  inside the argument list, quoted identifiers, qualified arguments,
  string-length arguments, parenthesized aggregates, and `SUM` as an ordinary
  identifier;
- parser rejection for `SUM (column)`, `SUM/**/(column)`, `SUM()`,
  `SUM(*)`, `SUM(1)`, `SUM(NULL)`, multi-argument `SUM`,
  literal/function/non-column arithmetic arguments, and malformed shapes;
- successful `SUM(column)` over `TINYINT`, `SMALLINT`, `MEDIUMINT`, `INT`,
  `INTEGER`, `BIGINT`, unsigned integer forms that remain within signed 64
  bits, and `BOOL`/`BOOLEAN` aliases;
- successful WordPress-shaped `SUM(LENGTH(text_column))` with a descriptor
  `WHERE ... IN (...)` filter;
- nullable, all-`NULL`, empty-table, no-match predicate, `IS NULL`, and
  `IS NOT NULL` behavior;
- baseline comparison predicate reuse, including equality, inequality, range,
  and null-safe equality over supported integer boundaries;
- aliases, source table aliases, source-qualified arguments, case-insensitive
  descriptor lookup, and invisible descriptor columns;
- unsupported signed-64 result overflow with deterministic diagnostics;
- missing default schema, unknown schema, unknown table, reserved names,
  unknown aggregate argument column, and unknown predicate column;
- unsupported wider aggregate/select forms: no-source, `FROM DUAL`, literals,
  `NULL`, multiple select items, mixed projections, `ORDER BY`,
  `LIMIT`, `GROUP BY`, and `HAVING`;
- result column labels, warning count, affected rows, following
  `ROW_COUNT() == -1`, reopen persistence, rename/drop behavior, independent
  handles, and physical preamble preservation;
- existing parser, runtime lifecycle, count aggregate, min/max aggregate,
  select where, row values, update, delete, catalog, storage, and VFS tests.

## Verification

Before marking done:

1. `cmake --build --preset dev`
2. Run the new `libmylite.runtime.sum_aggregate` CTest entry plus existing
   parser, count aggregate, min/max aggregate, select-where, row-values,
   update, delete, and lifecycle entries.
3. `./packages/libmylite/tests/mysql_baseline_sum_aggregate_expectations.sh`
4. `cmake --workflow --preset check`

## Compatibility Documentation

Update `COMPATIBILITY.md`, `docs/compatibility/functions-aggregate.md`, and
`docs/compatibility/sql-query-expressions.md` to mark only the exact limited
`SUM(column)` and narrow `SUM(string_length(column))` aggregate subsets as
supported. Do not claim full `SUM(expr)`, decimal result widening,
grouping, having, ordering, limiting, windows, joins, other expression
arguments, literals, collations, or protocol-grade metadata.
