# Baseline COUNT Boolean Aggregate

## Status

This feature specifies a narrow `COUNT(expr)` extension for bare boolean
literals: `COUNT(TRUE)` and `COUNT(FALSE)`. It builds on `mylite_execute()`,
statement context, the parser scaffold, durable catalog descriptors,
schema/table lifecycle, integer/`NULL` and boolean row values,
descriptor-driven single-table `SELECT`, the baseline `WHERE` predicate
subset, and the existing `COUNT(*)`, `COUNT(column)`, and
`COUNT(integer/NULL literal)` aggregate paths.

The feature is intentionally not full boolean expression support. It admits
exactly one aggregate select item, with either no table source, `FROM DUAL`, or
one persistent base table with an optional baseline `WHERE` predicate. It does
not add unary boolean expressions, arithmetic expressions, comparison
expressions, truth predicates, aliases, grouping, ordering, limiting, window
forms, or general `COUNT(expr)`.

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
- Baseline count literal aggregate:
  `docs/specs/baseline-count-literal-aggregate/specs.md`
- Baseline boolean literals:
  `docs/specs/baseline-boolean-literals/specs.md`
- MySQL lexer:
  `docs/specs/mysql-lexer/specs.md`
- MySQL parser scaffold:
  `docs/specs/mysql-parser-scaffold/specs.md`
- SQLite source snapshot notes: `third_party/sqlite/README.md`
- MySQL 8.4 Reference Manual, aggregate functions:
  https://dev.mysql.com/doc/refman/8.4/en/aggregate-functions.html
- MySQL 8.4 Reference Manual, boolean literals:
  https://dev.mysql.com/doc/refman/8.4/en/boolean-literals.html
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
- MySQL boolean literals `TRUE` and `FALSE` evaluate as non-`NULL` constants
  `1` and `0`, and their names are case-insensitive.
- `COUNT(TRUE)`, `COUNT(FALSE)`, `COUNT(true)`, and `COUNT(false)` return `1`
  for no-source and `FROM DUAL` forms, and return the matched row count for
  table-backed forms.
- Empty and no-match table-backed forms return `0` for both `COUNT(TRUE)` and
  `COUNT(FALSE)`.
- `COUNT(/*x*/TRUE)` and `COUNT(/*x*/FALSE)` count matched rows. Default
  result labels preserve the comment and insert a space before the following
  boolean token: `COUNT(/*x*/ TRUE)` and `COUNT(/*x*/ FALSE)`.
- `COUNT(+TRUE)`, `COUNT(-FALSE)`, `COUNT(NOT TRUE)`, and `COUNT(TRUE + 1)`
  are valid MySQL aggregate expressions, but remain outside this slice because
  they require expression evaluation beyond bare literal recognition.
- `COUNT (TRUE)` and `COUNT/**/(TRUE)` resolve as stored-function-style calls
  under the default SQL mode. With no selected database they fail with
  no-database error `1046`; with a selected database and no such stored
  function they fail with error `1630`. This slice keeps the existing MyLite
  no-space aggregate call rule and reports deterministic syntax diagnostics.
- `ORDER BY` and `LIMIT` on a single aggregate row are valid MySQL syntax, with
  `LIMIT 0` suppressing the row. This slice rejects `ORDER BY` and `LIMIT` for
  aggregate selects to preserve the current aggregate cardinality surface.
- A successful `SELECT COUNT(boolean_literal)` result set makes the following
  `ROW_COUNT()` return `-1` and leaves warning count `0`.

The reproducible probe lives in
`packages/libmylite/tests/mysql_baseline_count_boolean_aggregate_expectations.sh`.

## Scope

The implementation must add:

- parser support for no-space `COUNT(TRUE)` and `COUNT(FALSE)`;
- lowercase and mixed-case spelling through the existing keyword/literal
  scanner behavior;
- execution of `SELECT COUNT(boolean_literal)` and
  `SELECT COUNT(boolean_literal) FROM DUAL`;
- descriptor-driven
  `SELECT COUNT(boolean_literal) FROM table_name [WHERE predicate]`;
- unqualified and schema-qualified table-name resolution using the existing
  selected/default schema policy;
- one persistent MyLite base-table descriptor source only for table-backed
  forms;
- reuse of the existing baseline `WHERE` predicate subset and conversion
  rules;
- generated SQLite physical SQL built only from descriptors and stable
  physical table names for table-backed forms;
- prepared-statement binding for the boolean count literal and predicate
  values in table-backed forms;
- one result row with one text column containing the decimal count;
- MySQL-compatible result column labels for the selected aggregate expression;
- result-set row-count state matching existing `SELECT` behavior;
- deterministic diagnostics for unsupported boolean aggregate syntax and wider
  MySQL aggregate/select forms;
- tests and a MySQL 8.4.9 expectation artifact for supported behavior and
  deliberately rejected or deferred wider forms.

Existing `COUNT(*)`, `COUNT(column)`, `COUNT(integer/NULL literal)`, and
non-aggregate boolean-literal behavior remain unchanged.

## Non-Goals

This feature must not implement:

- general `COUNT(expr)`, `COUNT(DISTINCT expr)`, table-qualified arguments,
  qualified wildcards, string/decimal/float/hex/bit literal arguments,
  arithmetic expressions, boolean operators, unary boolean expressions,
  function calls, parameters, or subqueries;
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
  and records bare `TRUE` and `FALSE` as non-`NULL` count-literal arguments.
- The catalog module remains authoritative for schema/table/column
  descriptors. Table-backed count-boolean forms read descriptors for table and
  predicate resolution but do not mutate catalog rows, descriptor versions,
  descriptor caches, catalog generation, or `sqlite_schema_generation`.
- Runtime execution returns the implicit no-source/`DUAL` value directly, or
  generates SQLite SQL against the descriptor-owned physical table and binds
  the boolean literal plus any predicate parameters.
- The result builder owns the one-column text result. Counts are formatted as
  non-`NULL` decimal integer text.
- Storage/VFS owns the `.mylite` preamble and shifted SQLite payload boundary.
  Aggregate queries do not touch byte range `[0, 4096)`.

## Supported SQL Grammar

Supported subset:

```sql
SELECT COUNT(boolean_literal)
SELECT COUNT(boolean_literal) FROM DUAL
SELECT COUNT(boolean_literal) FROM table_name [WHERE predicate]
```

`boolean_literal` is:

```sql
boolean_literal:
    TRUE
  | FALSE
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
    column_name comparison_operator signed_integer_or_boolean_literal
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
count_literal ::= TRUE.
count_literal ::= FALSE.
```

The parser may admit `COUNT(count_literal)` anywhere the expression grammar is
currently shared, but the analyzer accepts it only as the sole select item in
the supported statement shape. Bare `COUNT` remains an ordinary identifier.

## Runtime Semantics

No-source and `FROM DUAL` forms return one result row:

- `TRUE` returns `1`;
- `FALSE` returns `1`.

Table-backed forms evaluate over the descriptor-owned physical table, with an
optional baseline `WHERE` predicate:

- both `TRUE` and `FALSE` count matched rows, equivalent to the current
  `COUNT(*)` row-count behavior for the admitted table source;
- empty sources and no-match predicates return `0`.

Successful count-boolean selects:

- return one result column;
- return one result row;
- use MySQL-compatible default label text for the selected aggregate
  expression, including the observed space after a block comment before the
  boolean token;
- use `affected_rows == 0` under the existing MyLite row-result convention;
- use `warning_count == 0` for supported forms;
- set the connection-local previous row count to `-1` after completion because
  the statement returns a result set.

## Generated SQLite Handling

For table-backed count-boolean aggregates, MyLite generates this SQLite shape:

```sql
SELECT COUNT(?) FROM "physical_table" [WHERE ...]
```

The parameter is bound to a non-`NULL` integer value for both `TRUE` and
`FALSE`, because MySQL-visible `COUNT(boolean_literal)` semantics depend only
on whether the expression is `NULL`. Generated table names come from MyLite
descriptors and are quoted. Predicate SQL is reused from the existing
descriptor-built baseline `WHERE` planner, and comparison values are bound
through prepared statements.

The no-source and `DUAL` forms do not need a SQLite statement. MyLite can
construct their one-row result directly because there is no table visibility,
predicate, catalog, or storage dependency.

This feature uses MyLite wrapper/translation code and public SQLite prepared
statement APIs only. It must not add SQLite fork patches or custom SQLite
functions.

## Diagnostics

Supported count-boolean forms reuse the existing count aggregate diagnostics:

| Case | Result |
| --- | --- |
| Syntax errors, including `COUNT (TRUE)` and `COUNT/**/(TRUE)` | `MYLITE_ERROR`, MySQL-style syntax diagnostic |
| More than one aggregate select item | `MYLITE_ERROR`, deterministic unsupported `COUNT(literal)` message |
| Unsupported clauses such as `ORDER BY`, `LIMIT`, `GROUP BY`, `HAVING`, or windows | `MYLITE_ERROR`, deterministic unsupported `COUNT(literal)` message |
| No source with `WHERE` | `MYLITE_ERROR`, `COUNT(literal) WHERE requires a descriptor-backed table` |
| Unknown schema or table | Existing schema/table diagnostics |
| Reserved `_mylite_*` table target | Existing reserved-name diagnostic |
| Unknown predicate column | Existing `WHERE` unknown-column diagnostic |
| Unsupported predicate literal or expression | Existing `WHERE` unsupported diagnostic |
| Physical SQLite failure | Existing internal SQLite failure diagnostic |
| Allocation failure | `MYLITE_NOMEM` with existing allocation diagnostic |
| Public API misuse | Existing public API misuse behavior |

Unsupported aggregate arguments such as `+TRUE`, `-FALSE`, `NOT TRUE`,
`TRUE + 1`, string literals, decimal literals, hex literals, bit literals,
parameters, functions, subqueries, column expressions, and table-qualified
arguments remain syntax errors or deterministic unsupported errors according to
the current parser/analyzer boundary.

## Tests

Add a MySQL-runtime expectation artifact covering:

- MySQL version guard for `8.4.9`;
- no-source, `FROM DUAL`, table-backed, empty-table, filtered, and no-match
  `COUNT(TRUE)` and `COUNT(FALSE)`;
- lower-case spelling;
- warning count `0` and following `ROW_COUNT() == -1`;
- default result labels, including block-comment spacing;
- MySQL-accepted but deferred boolean expression forms;
- whitespace/comment between `COUNT` and `(` stored-function behavior.

Add or extend fast C tests covering:

- parser acceptance for `COUNT(TRUE)`, `COUNT(FALSE)`, lower-case spelling, and
  comments inside the argument list;
- parser rejection for `COUNT (TRUE)`, `COUNT/**/(TRUE)`, `COUNT(+TRUE)`,
  `COUNT(-FALSE)`, `COUNT(NOT TRUE)`, and `COUNT(TRUE + 1)`;
- runtime no-source, `DUAL`, table-backed, empty-table, filtered, no-match,
  label, warning count, affected rows, following `ROW_COUNT()`, reopen
  persistence, physical failure, independent handle, and preamble behavior;
- predicate parameter numbering when the count literal occupies SQLite
  parameter `?1` and the comparison predicate starts at `?2`;
- unchanged behavior for existing `COUNT(*)`, `COUNT(column)`, and
  `COUNT(integer/NULL literal)` tests.

Run:

```sh
cmake --build --preset dev
ctest --preset dev --output-on-failure -R 'libmylite\.(parser|runtime\.count_aggregate)'
./packages/libmylite/tests/mysql_baseline_count_boolean_aggregate_expectations.sh
cmake --workflow --preset check
```

## Compatibility Documentation

Update:

- `COMPATIBILITY.md`;
- `docs/compatibility/functions-aggregate.md`;
- `docs/compatibility/sql-query-expressions.md`;
- `docs/compatibility/type-system-literals-conversion.md`.

Use partial wording such as `COUNT(integer/NULL/boolean literal)` and keep the
documented boolean-literal surface limited to aggregate argument positions plus
the already-supported default, row-value, assignment, and predicate positions.

Do not overclaim full `COUNT(expr)`, aliases, grouping, ordering, limiting,
expression metadata, unary boolean expressions, boolean operators, scalar truth
evaluation, or general expression support.
