# Baseline SELECT DISTINCT Column

## Status

This feature specifies a narrow `SELECT DISTINCT` slice:
`SELECT DISTINCT column_name FROM table_name`. It builds on `mylite_execute()`,
statement context, the parser scaffold, durable catalog descriptors,
descriptor-driven single-table `SELECT`, the baseline `WHERE` predicate subset,
and the existing single-column `ORDER BY` and `LIMIT`/`OFFSET` select path.

This is not full query-level duplicate elimination. The supported surface is one
unqualified descriptor column from one persistent base table, with optional
baseline `WHERE`, optional one-column `ORDER BY` on the same selected column,
and optional existing select `LIMIT` forms. Multiple selected expressions,
wildcards, table-qualified selected columns, aliases, expression projection,
`DISTINCTROW`, explicit `ALL`, joins, grouping, and general expression ordering
remain outside this slice.

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
- Baseline select order/limit lifecycle:
  `docs/specs/baseline-select-order-limit-lifecycle/specs.md`
- MySQL lexer: `docs/specs/mysql-lexer/specs.md`
- MySQL parser scaffold: `docs/specs/mysql-parser-scaffold/specs.md`
- SQLite source snapshot notes: `third_party/sqlite/README.md`
- MySQL 8.4 Reference Manual, `SELECT`:
  https://dev.mysql.com/doc/refman/8.4/en/select.html

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior,
public SQLite APIs, and existing MyLite source code. It does not copy MySQL,
MariaDB, Percona, SQLite implementation internals, or other restrictively
licensed implementation sources.

## MySQL 8.4.9 Runtime Observations

Observed against the local `mysql:8.4.9` runtime using TCP:

- The `SELECT` syntax admits `ALL`, `DISTINCT`, and `DISTINCTROW` modifiers.
  `ALL` is the default, and `DISTINCTROW` is a synonym for `DISTINCT`.
- `SELECT DISTINCT n FROM t` over values `NULL, 20, 20, 30, NULL` returns one
  `NULL`, one `20`, and one `30`. Without `ORDER BY`, row order is not part of
  this slice's compatibility contract.
- `ORDER BY n` and `ORDER BY n ASC` sort distinct integer values ascending,
  with `NULL` before non-`NULL` values. `ORDER BY n DESC` sorts descending, with
  `NULL` after non-`NULL` values.
- `LIMIT 0` returns no result rows. `LIMIT row_count`,
  `LIMIT row_count OFFSET offset`, and `LIMIT offset, row_count` are valid
  MySQL select limit forms.
- Baseline predicates are applied before duplicate elimination. For example,
  `WHERE n = 20` and `WHERE n <=> 20` return only `20`; `WHERE n IS NULL`
  returns one `NULL`; `WHERE n IS NOT NULL` returns the distinct non-`NULL`
  values.
- `SELECT DISTINCT n FROM t ORDER BY id` fails with MySQL error `3065`,
  SQLSTATE `HY000`, because the order expression is not in the distinct select
  list. MySQL accepts wider forms such as table-qualified selected columns,
  table-qualified order columns, ordinal ordering, expression ordering when the
  expression is compatible with the distinct list, aliases, multiple select
  items, literal select items, and `DISTINCTROW`; those are deferred here.
- Unknown selected, predicate, and order columns fail with MySQL error `1054`.
  Missing default schema fails with `1046`. Unknown schema-qualified tables fail
  with `1049`; unknown tables in a known schema fail with `1146`.
- Successful `SELECT DISTINCT` result-set statements leave warning count `0` and
  make the following `ROW_COUNT()` return `-1`.

The reproducible probe lives in
`packages/libmylite/tests/mysql_baseline_select_distinct_column_expectations.sh`.

## Scope

The implementation must add:

- parser and AST support for the `DISTINCT` select modifier on descriptor-backed
  table selects;
- descriptor-driven
  `SELECT DISTINCT column_name FROM table_name [WHERE predicate]
  [ORDER BY column_name [ASC | DESC]] [limit_clause]`;
- unqualified and schema-qualified table-name resolution using the existing
  selected/default schema policy;
- one persistent MyLite base-table descriptor source only;
- one unqualified selected descriptor column, including explicitly named
  invisible columns;
- reuse of the existing baseline `WHERE` predicate subset and conversion rules;
- optional `ORDER BY` only for the same selected descriptor column;
- optional `ASC` and `DESC`, with omitted direction meaning ascending;
- existing supported select `LIMIT row_count`,
  `LIMIT row_count OFFSET offset`, and `LIMIT offset, row_count` forms;
- generated SQLite physical SQL built only from descriptors and stable physical
  table names;
- prepared-statement binding for predicate, limit, and offset values;
- duplicate elimination delegated to SQLite for the generated one-column
  integer/`NULL` projection;
- result rows, descriptor column names, `NULL` value behavior, affected rows,
  warning count, and following `ROW_COUNT()` behavior matching the existing
  result API conventions for row-result statements;
- deterministic diagnostics for unsupported distinct query shapes; and
- tests and a MySQL 8.4.9 expectation artifact for supported behavior and
  deliberately rejected or deferred wider forms.

Existing non-distinct `SELECT`, aggregate, DML, DDL, and scalar-function behavior
must remain unchanged.

## Non-Goals

This feature must not implement:

- full `SELECT DISTINCT`, `DISTINCTROW`, explicit `ALL`, `DISTINCT *`, multiple
  select items, duplicate-row semantics across multiple expressions, aliases,
  table-qualified selected columns, expression selected items, literal selected
  items, parameters, functions, or arbitrary expression projection;
- table-qualified order columns, aliases, ordinals, expression order keys,
  multiple sort keys, collations, or non-selected order columns;
- `GROUP BY`, `HAVING`, `WITH ROLLUP`, window clauses, joins, CTEs, subqueries,
  set operations, locking clauses, optimizer hints, `HIGH_PRIORITY`,
  `STRAIGHT_JOIN`, `SQL_SMALL_RESULT`, `SQL_BIG_RESULT`, `SQL_BUFFER_RESULT`,
  `SQL_NO_CACHE`, `SQL_CALC_FOUND_ROWS`, `INTO`, partitions, or arbitrary SQLite
  pass-through;
- string, decimal, floating, temporal, JSON, enum, set, collation, or charset
  distinct semantics;
- result metadata parity for source-spelled labels, origin metadata, protocol
  flags, or expression metadata;
- indexes, helper temporary tables in MyLite, custom SQLite functions, SQLite
  virtual tables, or SQLite fork patches.

## Ownership Boundary

- The public API remains unchanged. `mylite_execute()` owns public validation,
  result-handle ownership, statement-boundary row-count state, and failure
  cleanup.
- Statement context owns diagnostics reset, warning count, and statement
  completion. Successful distinct selects are result-set statements and
  therefore store `-1` as the connection-local previous row count.
- Lexer/parser/AST own syntax admission, the distinct select modifier, and
  source spans. They remain independent of runtime, catalog, storage, and
  SQLite.
- Analyzer/planner code recognizes the distinct select shape, resolves the
  source table, selected column, optional predicate column, optional ordering
  column, and optional limit values from MyLite descriptors, rejects unsupported
  shapes, and builds a descriptor-driven select plan.
- The catalog module remains authoritative for schema/table/column descriptors.
  `SELECT DISTINCT column` reads descriptors but does not mutate catalog rows,
  descriptor versions, descriptor caches, catalog generation, or
  `sqlite_schema_generation`.
- Runtime execution generates SQLite SQL against the descriptor-owned physical
  table and binds predicate, limit, and offset parameters.
- The result builder owns copied result column names and text values. This slice
  preserves the existing descriptor-backed select convention of exposing
  descriptor column names rather than adding full MySQL source-expression label
  metadata.
- SQLite owns physical row storage, scan/filter execution, duplicate
  elimination, sorting, and limiting for generated descriptor-safe SQL.
- Storage/VFS owns the `.mylite` preamble and shifted SQLite payload boundary.
  Distinct reads must not write through byte range `[0, 4096)`.

## Supported SQL Grammar

Supported subset:

```sql
SELECT DISTINCT column_name
  FROM table_name [WHERE predicate]
  [ORDER BY column_name [ASC | DESC]]
  [limit_clause]
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

The supported ordering subset is deliberately narrower than MySQL:

```sql
order_key:
    column_name
```

The order key must resolve to the same descriptor column as the selected
distinct column.

The supported limit subset is the existing select limit subset:

```sql
limit_clause:
    LIMIT row_count
  | LIMIT row_count OFFSET offset
  | LIMIT offset, row_count

row_count:
    unsigned_decimal_integer_literal

offset:
    unsigned_decimal_integer_literal
```

`+1`, `-1`, decimal, float, string, hex, bit, parameter, variable, function, and
expression limit forms are rejected in this phase.

MyLite Lemon-syntax grammar snippets:

```lemon
select_statement ::=
    SELECT DISTINCT select_item_list FROM table_name
    where_clause_opt order_clause_opt limit_clause_opt.
select_statement ::=
    SELECT DISTINCT STAR FROM table_name
    where_clause_opt order_clause_opt limit_clause_opt.
```

The parser may admit `DISTINCT` with a general select item list or wildcard so
the analyzer can reject unsupported distinct shapes deterministically. The
semantic subset accepts only one unqualified descriptor column.

## Schema, Table, And Column Resolution

`SELECT DISTINCT n FROM t` uses the connection's selected schema. If no selected
schema exists, the statement fails with MySQL error `1046`, SQLSTATE `3D000`,
and message `No database selected`. `SELECT DISTINCT n FROM db.t` resolves the
schema explicitly and does not require a selected schema.

Schema and table lookup uses the MyLite catalog, not SQLite schema text. Unknown
schemas and unknown tables reuse existing descriptor-backed table diagnostics.
Reserved `_mylite_*` schema and table names are rejected before generating any
SQLite SQL.

The selected, predicate, and order columns resolve through table descriptors.
Current descriptor matching is ASCII case-insensitive and does not implement
full MySQL identifier collation. Explicit references may name invisible columns,
matching the existing explicit projection and aggregate behavior. `SELECT *`
with `DISTINCT` is outside this slice.

Unknown selected columns fail with an unknown-column-in-field-list diagnostic.
Unknown predicate columns fail with the existing unknown-where-column
diagnostic. Unknown ordering columns fail with the existing unknown-order-column
diagnostic. A known order column that is not the selected distinct column is an
unsupported distinct-order shape for this slice, even though MySQL reports error
`3065`.

## Runtime Semantics

For the supported one-column form, duplicate stored values are removed from the
result set. SQL `NULL` is a distinct value for query-level `DISTINCT`, so a
matched set containing one or more `NULL` values returns exactly one `NULL`.
This differs from `COUNT(DISTINCT column)`, which excludes `NULL`.

The optional supported `WHERE` predicate filters stored rows before duplicate
elimination. Predicate conversion remains MyLite-owned and descriptor-driven:
only currently supported integer and boolean literal right operands are
converted, range-checked, and bound.

If `ORDER BY` is omitted, result order is unspecified. Tests must not claim a
portable row order for unordered distinct results. If `ORDER BY` is present, it
must name the same selected descriptor column. For admitted integer and nullable
integer columns:

- omitted direction and `ASC` sort ascending;
- `DESC` sorts descending;
- `NULL` sorts before non-`NULL` ascending;
- `NULL` sorts after non-`NULL` descending; and
- duplicate sort values are already collapsed by the one-column distinct result,
  so this slice does not define tied-row selection.

If `LIMIT` is present without `ORDER BY`, the limited row subset is
implementation-order dependent, matching MySQL's lack of an ordering guarantee.
Tests should use `ORDER BY` when the exact limited values matter. `LIMIT 0`
returns no rows. Row counts and offsets larger than the distinct set are valid
and return the available rows after offset.

Successful supported distinct selects:

- return one result column;
- return zero or more result rows;
- use descriptor-backed result column naming under the existing public result
  API convention;
- use `affected_rows == 0`;
- use `warning_count == 0` for supported in-range statements; and
- set the connection-local previous row count to `-1`.

## Generated SQLite Handling

The generated physical SQL shape is:

```sql
SELECT DISTINCT "logical_column"
FROM "physical_table"
[WHERE "predicate_column" operator ?N]
[ORDER BY "logical_column" ASC|DESC]
[LIMIT ?N [OFFSET ?N]]
```

The physical table name comes from the MyLite table descriptor, such as
`_mylite_user_table_<table_id>`. Generated MyLite user tables are currently
ordinary SQLite rowid tables, but this feature does not depend on rowid.

Every generated SQLite identifier is quoted. Predicate, limit, and offset
values are bound through prepared statements. The selected descriptor column is
emitted only after descriptor resolution; user SQL text is not interpolated as
raw SQLite SQL.

SQLite and MySQL agree for this slice's integer/`NULL` duplicate elimination and
ascending/descending `NULL` placement, so this feature uses MyLite
wrapper/translation code plus public SQLite prepared-statement APIs. It does
not require public SQLite extension registration or a targeted SQLite fork hook.

## Diagnostics

The feature must preserve existing diagnostics for:

- public API misuse;
- syntax errors and unsupported parser shapes;
- missing default schema;
- unknown schema;
- unknown table;
- reserved `_mylite_*` schema or table names;
- unsupported object kind once non-base descriptors exist;
- unknown selected column;
- unknown predicate column;
- unknown order column;
- unsupported distinct wildcard, multiple select items, selected expression,
  literal selected item, table-qualified selected column, alias, explicit
  `ALL`, `DISTINCTROW`, table-qualified order column, order alias, order
  ordinal, order expression, multiple order keys, non-selected order column,
  unsupported predicate, unsupported limit expression, signed limit literal,
  out-of-range limit literal, joins, grouping, subqueries, CTEs, and query
  modifiers;
- physical SQLite failures;
- allocation failures; and
- file-format or VFS failures surfaced by lower layers.

Supported in-range statements produce no warnings.

## Tests

Add fast plain C tests under `packages/libmylite/tests/`, preferably a new
`runtime_select_distinct_column` test binary if that keeps the scope clearer
than extending the existing select-order-limit lifecycle test.

Coverage must include:

- successful one-column distinct over `INT`, `INTEGER`, `BIGINT`, unsigned
  integer forms within the currently supported physical range, and `BOOL`;
- duplicate non-`NULL` values and duplicate `NULL` values;
- nullable and non-nullable descriptor columns;
- invisible columns explicitly named by the distinct select item;
- `WHERE` predicate reuse, including comparisons, `<=>` with non-`NULL` integer
  right operands, `IS NULL`, and `IS NOT NULL`;
- `ORDER BY` same column default, `ASC`, and `DESC`, including nullable columns
  and `NULL` order;
- `LIMIT 0`, exact row counts, larger-than-result row counts, `OFFSET`, and
  comma offset syntax with deterministic `ORDER BY`;
- schema-qualified and unqualified target resolution, including missing default
  schema, unknown schema, unknown table, and reserved `_mylite_*` target names;
- affected rows, warning count, absence of mutation, following `ROW_COUNT()`,
  and row-result conventions;
- unknown selected, predicate, and order columns;
- unsupported `DISTINCT` shapes listed in diagnostics;
- persistence after close/reopen, table rename behavior, drop behavior,
  independent file-backed handles, and `.mylite` preamble preservation;
- zero-initialized cleanup for any new planner objects; and
- no regressions in existing lexer, parser, runtime handle, diagnostics,
  statement context, result metadata, SQLite bootstrap policy, file-backed
  opening, VFS, catalog foundation, table lifecycle, row values, select where,
  select order limit, delete, update, aggregate, client-data, and registration
  tests.

The MySQL expectation script must be runnable against MySQL 8.4.9 and must be
treated as a blocker if the runtime is unavailable.
