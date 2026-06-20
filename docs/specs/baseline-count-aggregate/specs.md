# Baseline COUNT Aggregate

## Status

This feature specifies a narrow aggregate-function slice for `COUNT()`. It
builds on `mylite_execute()`, statement context, the MyLite parser scaffold,
file-backed `.mylite` opening, durable catalog descriptors, schema/table
lifecycle, integer/`NULL` row values, descriptor-driven `SELECT`, the current
joined descriptor-source envelope, and the baseline `WHERE` predicate subset.

The feature is intentionally not full aggregate support. It admits supported
`COUNT(*)`, `COUNT(literal)`, `COUNT(column)`, limited row-scalar `COUNT(expr)`,
`COUNT(DISTINCT column)`, and single-expression distinct literal or supported
row-scalar count items in the documented source envelopes.
Bare `COUNT(*)` also uses the current joined descriptor-source envelope.
Grouped count forms are covered by grouped-aggregate specs. This slice does
not add broad aggregate expressions, window functions, or other aggregate
functions.

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
- Baseline schema lifecycle:
  `docs/specs/baseline-schema-lifecycle/specs.md`
- Baseline row values:
  `docs/specs/baseline-row-values-lifecycle/specs.md`
- Baseline select where lifecycle:
  `docs/specs/baseline-select-where-lifecycle/specs.md`
- Baseline select order limit lifecycle:
  `docs/specs/baseline-select-order-limit-lifecycle/specs.md`
- Baseline row count function:
  `docs/specs/baseline-row-count-function/specs.md`
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

- `COUNT(*)` returns the number of rows retrieved, including rows whose columns
  are all `NULL` where the table definition permits that.
- `COUNT(*)` returns `0` for empty tables and no-match predicates.
- `COUNT(*)` returns a `BIGINT` result according to MySQL documentation.
- `SELECT COUNT(*)` and `SELECT COUNT(*) FROM DUAL` return `1`.
- `COUNT(*)` is case-insensitive as a function name and preserves expression
  spelling in the default result label, except that MySQL inserts a space
  between a block comment and the `*` in labels such as `COUNT(/*x*/ *)`.
- Whitespace and comments inside the argument list are accepted, such as
  `COUNT( * )` and `COUNT(/*x*/*)`.
- Under the default SQL mode, whitespace or comments between `COUNT` and `(`
  are rejected as syntax errors. `COUNT (*)` and `COUNT/**/(*)` fail with
  error `1064`, SQLSTATE `42000`.
- `COUNT(t.*)`, `COUNT()`, `COUNT(*, *)`, and `COUNT(* + 1)` fail with syntax
  error `1064`.
- `COUNT(IFNULL(column, literal))` counts non-`NULL` row-scalar results.
  `COUNT(NULLIF(column, literal))` skips row-scalar results that evaluate to
  `NULL`.
- `COUNT(DISTINCT literal_or_expr)` counts unique non-`NULL` values in the
  supported literal and row-scalar expression subset. Non-`NULL` literal
  arguments return `1` for nonempty matched sets, `NULL` literal arguments and
  empty matched sets return `0`.
- `SELECT COUNT(*) FROM table WHERE ...` follows normal MySQL predicate
  semantics. This slice reuses only the already specified descriptor-driven
  integer/`NULL` predicate subset.
- `SELECT COUNT(*) FROM joined_sources WHERE ...` counts matched joined rows,
  including comma joins and null-extended outer-join rows that survive `WHERE`.
- `ORDER BY` without grouping is accepted by MySQL for supported single
  `COUNT()` aggregates, but has no visible effect on the single aggregate row.
  MySQL still validates unknown `ORDER BY` names. `LIMIT 0` suppresses that
  row, `LIMIT 1` returns it, `LIMIT 0, n` returns it when `n > 0`, and positive
  offsets suppress it.
- A successful `SELECT COUNT(*)` result set makes the following `ROW_COUNT()`
  return `-1` and leaves warning count `0`.

## Scope

The implementation must add:

- parser and AST support for no-space `COUNT(*)`;
- `COUNT` as a nonreserved identifier where identifier grammar admits it;
- execution of `SELECT COUNT(*)` and `SELECT COUNT(*) FROM DUAL`;
- descriptor-driven `SELECT COUNT(...) FROM table_name [WHERE predicate]
  [ORDER BY descriptor_order_key] [LIMIT row_count [OFFSET offset]]`;
- limited descriptor-driven row-scalar `COUNT(expr)` over the existing
  aggregate row-scalar expression subset, including grouped `COUNT(expr)`;
- limited single-expression `COUNT(DISTINCT expr)` over the existing aggregate
  row-scalar expression subset, plus `COUNT(DISTINCT literal)`, in current
  count-expression aggregate paths;
- descriptor-driven `SELECT COUNT(*) FROM joined_descriptor_sources
  [WHERE predicate]` within the current inner/cartesian/comma/no-op
  `STRAIGHT_JOIN` and supported outer-join source envelope;
- unqualified and schema-qualified table-name resolution using the existing
  selected/default schema policy;
- one persistent MyLite base-table descriptor source for non-joined count
  forms;
- reuse of the existing baseline `WHERE` predicate subset and conversion rules;
- generated SQLite physical `SELECT COUNT(*)` SQL built only from descriptors
  and stable physical table names;
- prepared-statement binding for predicate values;
- one result row with one text column containing the decimal count, subject to
  the query `LIMIT` envelope;
- MySQL-compatible result column labels for the selected `COUNT(*)`
  expression;
- result-set row-count state matching existing `SELECT` behavior;
- deterministic diagnostics for unsupported aggregate syntax and wider MySQL
  aggregate/select forms;
- tests and a MySQL 8.4.9 expectation artifact for supported behavior and
  deliberately rejected wider forms.

## Non-Goals

This feature must not implement:

- unsupported `COUNT(expr)` shapes outside the aggregate row-scalar subset,
  joined `COUNT(literal)`, joined `COUNT(column)`, joined `COUNT(DISTINCT
  column)`, multi-expression
  `COUNT(DISTINCT expr, expr...)`, `COUNT()` with no argument, `COUNT(table.*)`,
  aggregate arithmetic, aggregate comparisons, table-backed mixed projections,
  or general expression projection;
- `GROUP BY`, `HAVING`, unsupported `ORDER BY` expressions, window `OVER`
  clauses, CTEs, subqueries, unions, locking clauses, query modifiers,
  optimizer hints, `INTO`, or arbitrary SQLite SQL pass-through;
- other aggregate functions, aggregate metadata parity, protocol column flags,
  exact MySQL optimizer behavior, index-only count planning, transaction
  isolation beyond existing SQLite statement visibility, temporary tables,
  views, privileges, collations, SQL modes such as `IGNORE_SPACE`, or SQLite
  fork patches.

## Ownership Boundary

- The public API remains unchanged. `mylite_execute()` owns public validation,
  result-handle ownership, statement-boundary row-count state, and failure
  cleanup.
- Statement context owns diagnostics reset, warning count, and statement
  completion. Successful count selects are result-set statements and therefore
  store `-1` as the connection-local previous row count.
- Lexer/parser/AST own syntax admission, the no-space `COUNT(` rule, and
  source spans. They remain independent of runtime, catalog, storage, and
  SQLite.
- Analyzer/planner code recognizes the supported one-item `COUNT()` aggregate
  shapes, resolves optional table, joined source, and predicate descriptors,
  rejects unsupported shapes, and builds a descriptor-driven count plan.
- The catalog module remains authoritative for schema/table/column descriptors.
  `COUNT(*)` reads descriptors for table and predicate resolution but does not
  mutate catalog rows, descriptor versions, descriptor caches, catalog
  generation, or `sqlite_schema_generation`.
- Runtime execution either returns the implicit one-row count for no-source and
  `DUAL` forms or executes generated SQLite SQL against descriptor-owned
  physical table sources.
- The result builder owns the one-column text result. Counts are formatted as
  non-`NULL` decimal integer text.
- Storage/VFS owns the `.mylite` preamble and shifted SQLite payload boundary.
  Count queries do not touch byte range `[0, 4096)`.

## Supported SQL Grammar

Supported subset:

```sql
SELECT COUNT(*)
SELECT COUNT(*) FROM DUAL
SELECT COUNT(...) FROM table_name [WHERE predicate] [ORDER BY order_key] [LIMIT limit]
SELECT COUNT(row_scalar_expr) FROM table_name [WHERE predicate] [LIMIT limit]
SELECT COUNT(DISTINCT row_scalar_expr) FROM table_name [WHERE predicate] [LIMIT limit]
SELECT COUNT(*) FROM joined_descriptor_sources [WHERE predicate]
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

The count function name must be directly adjacent to `(` under the default SQL
mode. Whitespace and comments are accepted after `(` and before `*`.

MyLite Lemon-syntax grammar snippets:

```lemon
expression ::= count_star_function.
expression ::= count_row_scalar_function.
expression ::= count_literal_function.

count_star_function ::= COUNT LPAREN STAR RPAREN.
count_row_scalar_function ::= COUNT LPAREN count_row_scalar_argument RPAREN.
count_row_scalar_function ::= COUNT LPAREN DISTINCT count_row_scalar_argument RPAREN.
count_literal_function ::= COUNT LPAREN DISTINCT count_literal RPAREN.
```

The parser may admit `COUNT(*)` anywhere the expression grammar is currently
shared, but the analyzer accepts it only as the sole select item in the
supported statement shapes. `COUNT` remains usable as an ordinary unquoted
identifier in identifier positions where the parser admits nonreserved
keywords. Bare `COUNT` is not an aggregate call.

## Runtime Semantics

`SELECT COUNT(*)` and `SELECT COUNT(*) FROM DUAL` return one result row
containing `1`.

`SELECT COUNT(*) FROM table_name` returns the number of rows visible to the
current MyLite handle in the descriptor-owned physical table. With a supported
`WHERE` predicate, it returns the number of rows that satisfy that predicate.
Rows are counted regardless of whether projected table columns contain `NULL`.

`SELECT COUNT(*) FROM joined_descriptor_sources` uses the existing descriptor
joined-source planner and counts rows produced by that source after supported
join conditions and `WHERE` predicates are applied.

Successful count selects:

- return one result column;
- return one result row unless the query `LIMIT` envelope suppresses it;
- use MySQL-compatible default label text for the selected expression, including
  the observed space between a block comment and a following `*`;
- use `affected_rows == 0` under the existing MyLite row-result convention;
- use `warning_count == 0` for supported forms;
- set the connection-local previous row count to `-1` after completion because
  the statement returns a result set.

The count value is represented internally within the signed 64-bit range
returned by SQLite for `COUNT(*)`. That is sufficient for the current embedded
baseline and all supported tests. Full MySQL unsigned/protocol metadata parity
is out of scope.

## Physical SQLite Handling

Table-backed count selects lower to standard SQLite SQL built from descriptor
sources:

```sql
SELECT COUNT(*) FROM "physical_table" [WHERE "physical_column" op ?]
SELECT COUNT(*) FROM "physical_left" AS "_mylite_s0"
JOIN "physical_right" AS "_mylite_s1" ON "_mylite_s0"."id" = "_mylite_s1"."id"
[WHERE "_mylite_s0"."column" op ?]
```

Rules:

- physical table names come from MyLite table descriptors;
- joined-source aliases are generated and never user-controlled;
- predicate column names come from MyLite column descriptors;
- every generated SQLite identifier is quoted;
- predicate literals are converted by MyLite before execution and bound as
  prepared-statement parameters;
- no user SQL text, user literal text, or SQLite metadata lookup is used as
  authority;
- no SQLite optional aggregate extension, custom function, virtual table, VFS
  change, or fork patch is required.

No-source and `FROM DUAL` count forms do not require SQLite execution.

## Diagnostics

Supported `COUNT()` calls do not produce warnings.

Diagnostics follow the existing baseline conventions where MyLite has not yet
implemented full MySQL expression or metadata behavior:

- `COUNT (*)` and `COUNT/**/(*)` fail with syntax error `1064`, SQLSTATE
  `42000`, matching observed MySQL default-mode behavior.
- Unsupported count arguments such as `COUNT()`, `COUNT(table.*)`, joined
  non-star count forms, and aggregate expressions fail deterministically,
  either through parse error `1064` or the existing unsupported-statement
  diagnostic class.
- Missing default schema, unknown schema, unknown table, reserved
  `_mylite_*` schema/table names, unsupported object kinds, unknown predicate
  columns, unsupported predicate shapes, unsupported predicate literals, and
  predicate conversion range failures reuse existing descriptor-driven
  `SELECT ... WHERE` diagnostics.
- Unsupported `ORDER BY` expressions, aliases outside the supported result
  label envelope, mixed projections, multiple count items, grouping, having,
  joined non-star count forms, subqueries, CTEs, and query modifiers fail
  deterministically before arbitrary SQLite SQL is generated. Unknown
  descriptor `ORDER BY` names use the existing `order clause` unknown-column
  diagnostic.
- Allocation failures return `MYLITE_NOMEM` and set the existing out-of-memory
  diagnostic. Physical SQLite failures use the existing physical-row failure
  diagnostic unless a narrower diagnostic has already been set.
- Public API misuse behavior is unchanged.

## Tests

Fast C tests must cover:

- parser acceptance for `COUNT(*)`, lower/mixed case, `COUNT( * )`, comments
  inside the argument list, parenthesized `COUNT(*)`, no-source, `FROM DUAL`,
  and table-backed forms;
- parser/runtime rejection for whitespace or comments between `COUNT` and `(`;
- runtime no-source and `FROM DUAL` counts;
- runtime table-backed count over empty and nonempty persistent base tables;
- runtime joined `COUNT(*)` over comma, explicit inner, and supported outer
  joined descriptor sources, including WordPress-style column-equality and
  literal filters;
- count over nullable rows, proving `COUNT(*)` includes rows with `NULL`
  values;
- schema-qualified and unqualified table resolution, missing default schema,
  unknown schema, unknown table, and reserved `_mylite_*` target names;
- baseline `WHERE` predicate reuse, including comparisons, `<=>` with
  non-`NULL` integer right operands, `IS NULL`, and `IS NOT NULL`;
- integer predicate boundary behavior for `INT`, `INTEGER`, `BIGINT`, and
  their `UNSIGNED` forms within the current physical range;
- result column labels, one-row/one-column result shape, `warning_count == 0`,
  `affected_rows == 0`, and following `ROW_COUNT() == -1`;
- reopen persistence and independent file-backed handles observing their own
  committed row state;
- count after table rename and after truncate/drop where applicable;
- physical `.mylite` preamble preservation;
- zero-initialized cleanup for any new planner/result objects;
- supported descriptor `ORDER BY`, `LIMIT 1`, `LIMIT 0`, `LIMIT 0, n`, and
  positive-offset `LIMIT` behavior for the single aggregate result row;
- unsupported forms: unsupported `COUNT(expr)` shapes outside the row-scalar
  subset, `COUNT()`, `COUNT(table.*)`, joined non-star count forms, mixed
  projections, aliases outside the supported result-label envelope, aggregate
  arithmetic, unsupported `ORDER BY` expressions, `GROUP BY`, `HAVING`, CTEs,
  subqueries, window `OVER`, parameters, and unsupported predicate expressions;
- existing lexer, parser, scalar function, schema lifecycle, table lifecycle,
  row values, select-where, select-order-limit, delete, update, truncate,
  result metadata, statement context, file-format, VFS, and SQLite bootstrap
  tests still pass.

The MySQL expectation artifact must verify the admitted MySQL behavior and
record intentionally deferred MySQL-accepted forms without guessing.

## Compatibility Documentation

After implementation:

- update `COMPATIBILITY.md` to mark `COUNT()` as limited for supported
  `COUNT(*)`, literal, column, and distinct-column forms;
- update `docs/compatibility/functions-aggregate.md`;
- update `docs/compatibility/sql-query-expressions.md` only if the documented
  select surface changes;
- do not overclaim general `COUNT(expr)`, joined non-star count forms,
  grouping, having, aliases, window functions, order/limit aggregate semantics,
  general aggregate
  expressions, protocol metadata, optimizer behavior, temporary tables, views,
  privileges, collations, or SQL modes.

## Verification

Before marking this feature done:

1. `cmake --build --preset dev`
2. Run the new count CTest entry and relevant parser/select lifecycle entries.
3. Run the MySQL 8.4.9 expectation script for this feature.
4. `cmake --workflow --preset check`
5. Review the final diff for parser independence, descriptor authority,
   generated SQL safety, parameter binding, result/row-count semantics,
   file-format safety, scope control, compatibility docs, tests, and cleanup on
   failure.
