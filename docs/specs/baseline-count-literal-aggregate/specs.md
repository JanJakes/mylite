# Baseline COUNT Literal Aggregate

## Status

This feature specifies a narrow `COUNT(expr)` extension for literal arguments:
`COUNT(decimal_integer_literal)` and `COUNT(NULL)`. It builds on
`mylite_execute()`, statement context, the parser scaffold, durable catalog
descriptors, schema/table lifecycle, integer/`NULL` row values,
descriptor-driven single-table `SELECT`, the baseline `WHERE` predicate
subset, and the existing `COUNT(*)` / `COUNT(column)` aggregate paths.

The feature is intentionally not full expression aggregate support. It admits
exactly one aggregate select item, with either no table source, `FROM DUAL`, or
one persistent base table with an optional baseline `WHERE` predicate. It does
not add string, decimal, float, hex, bit, column expression,
arithmetic expression, function, `DISTINCT`, grouping, having, ordering,
limiting, window, join, subquery, alias, or mixed-projection aggregate support.
Bare boolean-literal aggregate arguments are specified separately in
`docs/specs/baseline-count-boolean-aggregate/specs.md`.

## Sources

- MyLite README architecture: `README.md`
- MyLite engineering standards:
  `docs/architecture/engineering-standards.md`
- Baseline implementation strategy:
  `docs/specs/baseline-implementation-strategy/specs.md`
- Runtime handles and statement context:
  `docs/specs/runtime-handles-statement-context/specs.md`
- Baseline select where lifecycle:
  `docs/specs/baseline-select-where-lifecycle/specs.md`
- Baseline count aggregate:
  `docs/specs/baseline-count-aggregate/specs.md`
- Baseline count column aggregate:
  `docs/specs/baseline-count-column-aggregate/specs.md`
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

- `COUNT(expr)` returns a `BIGINT` count of non-`NULL` expression values in the
  retrieved rows. With no `GROUP BY`, an aggregate select groups over all
  matched rows.
- `COUNT(1)`, `COUNT(0)`, `COUNT(-1)`, and `COUNT(+1)` return `1` for no-source
  and `FROM DUAL` forms, and return the matched row count for table-backed
  forms.
- `COUNT(NULL)` returns `0` for no-source, `FROM DUAL`, empty, nonempty,
  matched, and no-match table-backed forms.
- `COUNT(999999999999999999999999999999999999999999999999999999999999999)`
  returns `1`; for this slice MyLite does not need to convert decimal integer
  literal text to a numeric value because the only visible distinction is
  non-`NULL` versus `NULL`.
- `COUNT(1.0)` and `COUNT('x')` are valid MySQL aggregate expressions, but
  remain outside this slice because they require wider literal/expression
  support.
- Default result labels preserve source expression spelling. MySQL inserts a
  space after a block comment before the following token in labels such as
  `COUNT(/*x*/ 1)`, `COUNT(/*x*/ -1)`, `COUNT(/*x*/ +1)`, and
  `COUNT(/*x*/ NULL)`.
- `COUNT (1)` and `COUNT/**/(1)` resolve as stored-function-style calls under
  the default SQL mode. With no selected database they fail with no-database
  error `1046`; with a selected database and no such stored function they fail
  with error `1630`. This slice keeps the existing MyLite no-space aggregate
  call rule and reports deterministic syntax diagnostics.
- `ORDER BY` and `LIMIT` on a single aggregate row are valid MySQL syntax, with
  `LIMIT 0` suppressing the row. This slice rejects `ORDER BY` and `LIMIT` for
  aggregate selects to preserve the current aggregate cardinality surface.
- A successful `SELECT COUNT(literal)` result set makes the following
  `ROW_COUNT()` return `-1` and leaves warning count `0`.

The reproducible probe lives in
`packages/libmylite/tests/mysql_baseline_count_literal_aggregate_expectations.sh`.

## Scope

The implementation must add:

- parser and AST support for no-space `COUNT(count_literal)`;
- `count_literal` limited to unsigned decimal integer tokens, `+` or `-`
  followed by unsigned decimal integer tokens, and `NULL`;
- execution of `SELECT COUNT(count_literal)` and
  `SELECT COUNT(count_literal) FROM DUAL`;
- descriptor-driven
  `SELECT COUNT(count_literal) FROM table_name [WHERE predicate]`;
- unqualified and schema-qualified table-name resolution using the existing
  selected/default schema policy;
- one persistent MyLite base-table descriptor source only for table-backed
  forms;
- reuse of the existing baseline `WHERE` predicate subset and conversion
  rules;
- generated SQLite physical SQL built only from descriptors and stable
  physical table names for table-backed forms;
- prepared-statement binding for the count literal and predicate values in
  table-backed forms;
- one result row with one text column containing the decimal count;
- MySQL-compatible result column labels for the selected aggregate expression;
- result-set row-count state matching existing `SELECT` behavior;
- deterministic diagnostics for unsupported aggregate syntax and wider MySQL
  aggregate/select forms;
- tests and a MySQL 8.4.9 expectation artifact for supported behavior and
  deliberately rejected or deferred wider forms.

Existing `COUNT(*)` and `COUNT(column)` behavior remains unchanged.

## Non-Goals

This feature must not implement:

- general `COUNT(expr)`, `COUNT(DISTINCT expr)`, table-qualified arguments,
  qualified wildcards, string/decimal/float/hex/bit literal arguments,
  arithmetic expressions, function calls, parameters, or subqueries;
- aliases, mixed projections, multiple aggregate select items, aggregate
  comparisons, aggregate arithmetic, or nested aggregates;
- `GROUP BY`, `HAVING`, `ORDER BY`, `LIMIT`, window `OVER` clauses, joins,
  CTEs, subqueries, unions, locking clauses, query modifiers, optimizer hints,
  `INTO`, or arbitrary SQLite SQL pass-through;
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
  optional source table and predicate descriptors, rejects unsupported shapes,
  and records whether the literal argument is `NULL` or non-`NULL`.
- The catalog module remains authoritative for schema/table/column descriptors.
  Table-backed count-literal forms read descriptors for table and predicate
  resolution but do not mutate catalog rows, descriptor versions, descriptor
  caches, catalog generation, or `sqlite_schema_generation`.
- Runtime execution returns the implicit no-source/`DUAL` value directly, or
  generates SQLite SQL against the descriptor-owned physical table and binds
  the count literal plus any predicate parameters.
- The result builder owns the one-column text result. Counts are formatted as
  non-`NULL` decimal integer text.
- Storage/VFS owns the `.mylite` preamble and shifted SQLite payload boundary.
  Aggregate queries do not touch byte range `[0, 4096)`.

## Supported SQL Grammar

Supported subset:

```sql
SELECT COUNT(count_literal)
SELECT COUNT(count_literal) FROM DUAL
SELECT COUNT(count_literal) FROM table_name [WHERE predicate]
```

`count_literal` is:

```sql
count_literal:
    integer_literal
  | + integer_literal
  | - integer_literal
  | NULL
```

`table_name` uses the existing table lifecycle subset:

```sql
table_name:
    identifier
  | identifier.identifier
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
expression ::= COUNT LPAREN count_literal RPAREN.

count_literal ::= INTEGER.
count_literal ::= PLUS INTEGER.
count_literal ::= MINUS INTEGER.
count_literal ::= NULL.
```

The parser may admit `COUNT(count_literal)` anywhere the expression grammar is
currently shared, but the analyzer accepts it only as the sole select item in
the supported statement shape. Bare `COUNT` remains an ordinary identifier.

## Runtime Semantics

No-source and `FROM DUAL` forms return one result row:

- non-`NULL` integer literals return `1`;
- `NULL` returns `0`.

Table-backed forms evaluate over the descriptor-owned physical table, with an
optional baseline `WHERE` predicate:

- non-`NULL` integer literals count matched rows, equivalent to the current
  `COUNT(*)` row-count behavior for the admitted table source;
- `NULL` returns `0`, while still resolving the table and predicate and
  executing the physical table read for table-backed forms.

Successful count-literal selects:

- return one result column;
- return one result row;
- use MySQL-compatible default label text for the selected expression,
  including the observed space after a block comment before the following
  token;
- use `affected_rows == 0` under the existing MyLite row-result convention;
- use `warning_count == 0` for supported forms;
- set the connection-local previous row count to `-1` after completion because
  the statement returns a result set.

## Generated SQLite Handling

For table-backed count-literal aggregates, MyLite generates one of these
SQLite shapes:

```sql
SELECT COUNT(?) FROM "physical_table" [WHERE ...]
```

The parameter is bound to a non-`NULL` integer value for integer count
literals and to `NULL` for `COUNT(NULL)`. The exact integer literal text is not
converted into a stored MyLite numeric value for this slice because MySQL's
visible `COUNT(literal)` semantics depend only on whether the expression is
`NULL`. Generated table names come from MyLite descriptors and are quoted
through the existing identifier-quoting helper. Predicate literals continue to
use the descriptor-driven predicate conversion path and prepared-statement
parameters.

SQLite performs the physical scan and aggregate for table-backed forms. MyLite
does not materialize table rows in C to count them. Without indexes, SQLite may
still scan the physical table; index-only aggregate planning is outside this
baseline.

## Diagnostics

Diagnostics must be deterministic and must preserve existing public result
cleanup behavior:

- syntax errors or unsupported grammar: current parser or analyzer diagnostic;
- missing default schema for table-backed forms: MySQL-compatible
  no-database-selected diagnostic;
- unknown schema: MySQL-compatible unknown-database diagnostic;
- unknown table: MySQL-compatible table-not-found diagnostic;
- reserved schema/table names: current MyLite reserved-name diagnostic;
- unsupported object kind: deterministic unsupported object diagnostic once
  non-base descriptors exist;
- unknown predicate column: existing unknown column in `where clause`;
- unsupported aggregate argument expression, string/decimal/float/hex/bit
  literal, table-qualified argument, `DISTINCT`, qualified wildcard, multiple
  aggregate items, mixed projection, aliases, grouping, having, ordering,
  limiting, joins, CTEs, subqueries, and window clauses:
  deterministic syntax or unsupported-scope diagnostic;
- physical SQLite failure: current internal SQLite row-operation diagnostic;
- allocation failure: current allocation failure diagnostic;
- public API misuse: unchanged existing public API diagnostics.

Supported count-literal aggregate statements emit no warnings.

## Storage And Catalog Impact

`COUNT(count_literal)` is read-only. It must not mutate user rows, catalog
rows, descriptor versions, descriptor caches, catalog generation,
`sqlite_schema_generation`, or the `.mylite` preamble. It must preserve the
shifted SQLite payload invariant and work after closing and reopening a file.

## Compatibility Notes

This slice moves MyLite from `COUNT(*)` plus `COUNT(column)` to also support
integer-literal and `NULL` arguments for one-item aggregate selects. MySQL-
compatible string, decimal, float, hex, bit, expression, table-qualified,
`DISTINCT`, alias, `ORDER BY`, `LIMIT`, grouping, and window forms remain
explicitly unsupported. Bare boolean-literal aggregate arguments are covered by
the separate count-boolean aggregate slice.
