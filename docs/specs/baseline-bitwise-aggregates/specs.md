# Baseline Bitwise Aggregates

## Status

This feature specifies a narrow aggregate-function slice for
`BIT_AND(column)`, `BIT_OR(column)`, and `BIT_XOR(column)`. It builds on
`mylite_execute()`, statement context, the MyLite parser scaffold, durable
catalog descriptors, integer/`NULL` row values, descriptor-driven
single-table `SELECT`, the baseline `WHERE` predicate subset, and the
existing `COUNT`, `MIN`, `MAX`, `SUM`, and `AVG` aggregate paths.

This is not full bit-expression or binary-string aggregate support. It admits
exactly one aggregate select item over one persistent base table with an
optional baseline `WHERE` predicate. It does not add literal/expression
arguments, `DISTINCT`, binary string operands, grouping, having, ordering,
limiting, window functions, joins, CTEs, subqueries, or general bitwise
operator semantics.

## Sources

- MyLite README architecture: `README.md`
- MyLite engineering standards:
  `docs/architecture/engineering-standards.md`
- Baseline implementation strategy:
  `docs/specs/baseline-implementation-strategy/specs.md`
- Runtime handles and statement context:
  `docs/specs/runtime-handles-statement-context/specs.md`
- SQLite connection bootstrap policy:
  `docs/specs/sqlite-connection-bootstrap-policy/specs.md`
- Baseline catalog foundation:
  `docs/specs/baseline-catalog-foundation/specs.md`
- Baseline row values:
  `docs/specs/baseline-row-values-lifecycle/specs.md`
- Baseline select where lifecycle:
  `docs/specs/baseline-select-where-lifecycle/specs.md`
- Baseline aggregate specs under `docs/specs/`
- MySQL lexer:
  `docs/specs/mysql-lexer/specs.md`
- MySQL parser scaffold:
  `docs/specs/mysql-parser-scaffold/specs.md`
- SQLite source snapshot notes: `third_party/sqlite/README.md`
- MySQL 8.4 Reference Manual, aggregate functions:
  https://dev.mysql.com/doc/refman/8.4/en/aggregate-functions.html
- MySQL 8.4 Reference Manual, bit functions and operators:
  https://dev.mysql.com/doc/refman/8.4/en/bit-functions.html
- MySQL 8.4 Reference Manual, `SELECT` and `DUAL`:
  https://dev.mysql.com/doc/refman/8.4/en/select.html
- MySQL 8.4 Reference Manual, function name parsing:
  https://dev.mysql.com/doc/refman/8.4/en/function-resolution.html
- Public SQLite application-defined function APIs:
  https://www.sqlite.org/appfunc.html

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior,
public SQLite APIs, and existing MyLite source code. It does not copy MySQL,
MariaDB, Percona, SQLite implementation internals, or other restrictively
licensed implementation sources.

## MySQL 8.4.9 Runtime Observations

The expectation script
`packages/libmylite/tests/mysql_baseline_bitwise_aggregates_expectations.sh`
records the runtime probes for this feature. Observed behavior:

- `BIT_AND(expr)`, `BIT_OR(expr)`, and `BIT_XOR(expr)` aggregate the argument
  values from rows retrieved by the select.
- With no `GROUP BY`, an aggregate select groups over all matched rows.
- For integer expression values, MySQL evaluates the aggregate in numeric
  unsigned-64 context. Negative signed integers therefore contribute their
  two's-complement unsigned-64 value; for example, a single `-1` contributes
  `18446744073709551615`.
- `NULL` values do not affect the result. If there are no matched rows, or if
  every matched argument value is `NULL`, `BIT_AND()` returns the neutral
  all-ones unsigned-64 value `18446744073709551615`; `BIT_OR()` and
  `BIT_XOR()` return `0`.
- `BIT_AND()`, `BIT_AND(*)`, and `BIT_AND(a, b)` fail with syntax error
  `1064`, SQLSTATE `42000`. `BIT_XOR(DISTINCT column)` also fails with syntax
  error; unlike `AVG()` and `SUM()`, the probed bitwise aggregates do not admit
  `DISTINCT`.
- `BIT_AND(1)`, `BIT_OR(NULL)`, and `BIT_AND(column + 1)` are valid MySQL
  aggregate expressions. This MyLite slice supports descriptor column
  arguments only and defers literal, `NULL`, and general expression arguments.
- `SELECT BIT_AND(column)` and `SELECT BIT_AND(column) FROM DUAL` fail with
  unknown-column error `1054`, SQLSTATE `42S22`, when the column name is not
  otherwise resolvable.
- Function names are case-insensitive. Default result labels preserve the
  source expression spelling, except MySQL inserts a space after a block
  comment before the following token in labels such as
  `BIT_AND(/*x*/ i)`.
- Under the default SQL mode, whitespace or comments between `BIT_AND`,
  `BIT_OR`, or `BIT_XOR` and `(` resolve as stored-function calls. In a test
  schema with no stored function, `BIT_AND (i)` and `BIT_AND/**/(i)` fail
  with error `1630`, SQLSTATE `42000`.
- `ORDER BY` and `LIMIT` on a single aggregate row are valid MySQL syntax,
  with `LIMIT 0` suppressing the row. This slice rejects `ORDER BY` and
  `LIMIT` to preserve the current aggregate cardinality surface.
- A successful bitwise aggregate result set makes the following `ROW_COUNT()`
  return `-1` and leaves warning count `0`.

## Scope

The implementation must add:

- parser and AST support for no-space `BIT_AND(qualified_identifier)`,
  `BIT_OR(qualified_identifier)`, and `BIT_XOR(qualified_identifier)`;
- `BIT_AND`, `BIT_OR`, and `BIT_XOR` as nonreserved identifiers where
  identifier grammar admits them;
- descriptor-driven
  `SELECT BIT_AND(column_name) [AS alias] FROM table_name [WHERE predicate]`;
- equivalent `BIT_OR(column_name)` and `BIT_XOR(column_name)` forms;
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
- one result row with one text column containing the unsigned-64 decimal
  aggregate result;
- MyLite-owned unsigned-64 bit aggregation through registered SQLite aggregate
  callbacks, so SQLite owns row scanning and predicate filtering without
  needing optional SQLite bit aggregate extensions;
- MySQL-compatible result column labels for the selected aggregate expression;
- result-set row-count state matching existing `SELECT` behavior;
- deterministic diagnostics for unsupported aggregate syntax and wider MySQL
  aggregate/select forms;
- tests and a MySQL 8.4.9 expectation artifact for supported behavior and
  deliberately rejected wider forms.

Existing aggregate behavior must remain unchanged.

## Non-Goals

This feature must not implement:

- `BIT_AND(expr)`, `BIT_OR(expr)`, or `BIT_XOR(expr)` for literals, `NULL`,
  arithmetic, functions, parenthesized expression arguments, or general
  expression arguments;
- binary string evaluation or return values wider than 64 bits;
- no-source or `FROM DUAL` aggregate evaluation;
- multiple aggregate select items, mixed projections, aggregate comparisons,
  aggregate arithmetic, or nested aggregates;
- `DISTINCT`, `GROUP BY`, `HAVING`, `ORDER BY`, `LIMIT`, window `OVER`
  clauses, joins, CTEs, subqueries, unions, locking clauses, query modifiers,
  optimizer hints, `INTO`, or arbitrary SQLite SQL pass-through;
- string, decimal, floating, temporal, JSON, enum, set, collation, or charset
  aggregate expression semantics;
- scalar bit operators such as `&`, `|`, `^`, `~`, shifts, or `BIT_COUNT()`;
- aggregate metadata parity, protocol column flags, exact optimizer behavior,
  transaction isolation beyond existing SQLite statement visibility,
  temporary tables, views, privileges, SQL modes such as `IGNORE_SPACE`, or
  SQLite fork patches.

## Ownership Boundary

- The public API remains unchanged. `mylite_execute()` owns public validation,
  result-handle ownership, statement-boundary row-count state, and failure
  cleanup.
- Statement context owns diagnostics reset, warning count, and statement
  completion. Successful bitwise aggregate selects are result-set statements
  and therefore store `-1` as the connection-local previous row count.
- Lexer/parser/AST own syntax admission, the no-space function-call rule,
  keyword classification, and source spans. They remain independent of
  runtime, catalog, storage, and SQLite.
- Analyzer/planner code recognizes the one-item aggregate shape, resolves the
  source table, aggregate argument column, and optional predicate descriptors,
  rejects unsupported shapes, and builds a descriptor-driven aggregate plan.
- The catalog module remains authoritative for schema/table/column
  descriptors. Bitwise aggregates read descriptors for table, aggregate
  argument, and predicate resolution but do not mutate catalog rows,
  descriptor versions, descriptor caches, catalog generation, or
  `sqlite_schema_generation`.
- SQLite bootstrap owns per-connection registration of MyLite internal
  aggregate callbacks. The registered names are reserved internal physical SQL
  names and are never exposed as supported user-callable SQL functions.
- Runtime execution generates SQLite SQL against the descriptor-owned physical
  table and binds only predicate parameters. SQLite owns scanning and
  predicate filtering; the MyLite callback owns unsigned-64 aggregation for
  admitted integer/`NULL` physical values.
- The result builder owns the one-column text/null result.
- Storage/VFS owns the `.mylite` preamble and shifted SQLite payload boundary.
  Aggregate queries do not touch byte range `[0, 4096)`.

## Supported SQL Grammar

Supported subset:

```sql
SELECT BIT_AND(column_name) [AS alias] FROM table_name [WHERE predicate]
SELECT BIT_OR(column_name) [AS alias] FROM table_name [WHERE predicate]
SELECT BIT_XOR(column_name) [AS alias] FROM table_name [WHERE predicate]
```

`table_name` uses the existing table lifecycle subset:

```sql
table_name:
    identifier
  | identifier.identifier
```

The aggregate argument is one descriptor column reference:

```sql
bitwise_aggregate_argument:
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

The aggregate function name must be directly adjacent to `(` under the default
SQL mode. Whitespace and comments are accepted inside the argument list.

MyLite Lemon-syntax grammar snippets:

```lemon
expression ::= BIT_AND LPAREN qualified_identifier RPAREN.
expression ::= BIT_OR LPAREN qualified_identifier RPAREN.
expression ::= BIT_XOR LPAREN qualified_identifier RPAREN.
```

The parser may admit these aggregate expressions anywhere the expression
grammar is currently shared, but the analyzer accepts them only as the sole
select item in the supported aggregate select shape. The function names remain
usable as ordinary unquoted identifiers in identifier positions where the
parser admits nonreserved keywords. Bare `BIT_AND`, `BIT_OR`, and `BIT_XOR`
are not aggregate calls.

Unsupported examples for this slice:

```sql
SELECT BIT_AND(1) FROM t;
SELECT BIT_OR(NULL) FROM t;
SELECT BIT_XOR(n + 1) FROM t;
SELECT BIT_AND(n), BIT_OR(n) FROM t;
SELECT BIT_AND(n) FROM t ORDER BY id;
SELECT BIT_AND(n) FROM t LIMIT 1;
SELECT BIT_AND(n) OVER () FROM t;
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

For this slice, the aggregate argument must be an integer-family descriptor
column currently stored as SQLite integer or `NULL`: `TINYINT`, `SMALLINT`,
`MEDIUMINT`, `INT`, `INTEGER`, `BIGINT`, their supported `UNSIGNED` forms,
and the currently supported integer aliases such as `BOOL` and `BOOLEAN`.

Each non-`NULL` SQLite integer is converted to its unsigned-64 numeric
contribution before aggregation:

- nonnegative signed-64 values contribute the same numeric value;
- negative signed-64 values contribute the two's-complement unsigned-64 value
  obtained by converting the stored `int64_t` bit pattern to `uint64_t`;
- `NULL` values are ignored.

The aggregate state starts at the MySQL neutral value:

- `BIT_AND` starts at `UINT64_MAX`;
- `BIT_OR` starts at `0`;
- `BIT_XOR` starts at `0`.

If the matched row set is empty, or if every matched aggregate argument is
`NULL`, the neutral value is returned. Otherwise:

- `BIT_AND` returns the bitwise AND of all non-`NULL` contributions;
- `BIT_OR` returns the bitwise OR of all non-`NULL` contributions;
- `BIT_XOR` returns the bitwise XOR of all non-`NULL` contributions.

Results are returned as unsigned-64 decimal text, including values above
`INT64_MAX` such as `18446744073709551615`. The result is not bound back to
SQLite as an integer because SQLite's public integer result type is signed
64-bit.

## Physical SQLite Handling

Generated physical SQL must use stable descriptor-owned physical table names
such as `_mylite_user_table_<table_id>`. Generated identifiers must be quoted
through the existing dynamic-string identifier quoting helper. Predicate
values must be bound through prepared statements, reusing the current
predicate binding path.

The select-list physical SQL uses internal MyLite aggregate function names,
not MySQL-visible names. The shape is:

```sql
SELECT _mylite_bit_and("logical_column") FROM "_mylite_user_table_1" WHERE ...
SELECT _mylite_bit_or("logical_column") FROM "_mylite_user_table_1" WHERE ...
SELECT _mylite_bit_xor("logical_column") FROM "_mylite_user_table_1" WHERE ...
```

The callbacks are registered per SQLite connection during MyLite bootstrap
through the existing function-registration surface. They are deterministic for
their input multiset and innocuous/direct internal use, but they are not a
public SQL API promise. The analyzer must continue to admit only the
descriptor-planned MySQL aggregate surface; arbitrary user calls to internal
function names remain unsupported MyLite implementation details.

The callback step function must:

- validate the argument count supplied by SQLite;
- ignore `SQLITE_NULL`;
- accept `SQLITE_INTEGER`;
- convert the signed `sqlite3_int64` value to `uint64_t`;
- update only fixed-size aggregate state;
- set a SQLite error for impossible physical type mismatches.

The callback final function must format the unsigned-64 state into decimal
text and return that text to SQLite. It must not allocate unbounded memory or
depend on SQLite extensions outside the public application-defined function
API.

No SQLite fork changes are needed for this slice.

## Result Semantics

A successful supported statement returns through the existing public result
API conventions for row-result statements:

- `mylite_result_column_count(result) == 1`;
- `mylite_result_row_count(result) == 1`;
- `mylite_result_column_name(result, 0)` is the select-item alias when
  present, otherwise the MySQL-compatible expression label;
- `mylite_result_value_text(result, 0, 0)` is the unsigned-64 decimal text;
- `mylite_result_affected_rows(result) == 0`;
- `mylite_result_warning_count(result) == 0`;
- subsequent `ROW_COUNT()` returns `-1`.

## Diagnostics

Diagnostics use existing MyLite helpers where possible:

- syntax errors and unsupported grammar: existing parse diagnostic `1064` /
  `42000`;
- whitespace/comment between function name and `(`: existing parser behavior
  must reject like no-space `SUM`, `MIN`, and `MAX`;
- unsupported aggregate select shape: MyLite unsupported diagnostic with text
  naming the aggregate family;
- missing default schema: existing `1046` / `3D000`;
- unknown schema: existing `1049` / `42000`;
- unknown table: existing `1146` / `42S02`;
- reserved `_mylite_*` schema/table names: existing incorrect-name
  diagnostics;
- unsupported object kind: deterministic MyLite unsupported diagnostic once
  non-base descriptors exist;
- unknown aggregate column: existing field-list unknown-column diagnostic;
- unknown predicate column: existing where-clause unknown-column diagnostic;
- unsupported aggregate argument type: deterministic unsupported diagnostic
  stating that the aggregate supports only integer descriptor columns;
- unsupported literal, expression, `DISTINCT`, order, limit, group, having,
  join, CTE, subquery, window, and multiple-select-item forms: deterministic
  parse or unsupported diagnostics;
- physical SQLite callback/type failures: existing internal SQLite row
  operation failure diagnostic unless a more specific MyLite callback error is
  available;
- allocation failures: `MYLITE_NOMEM` with cleanup through the existing
  result/planner paths;
- public API misuse: unchanged existing public execution/result misuse
  behavior.

Supported in-range bitwise aggregate statements produce no warnings.

## Tests

Add
`packages/libmylite/tests/mysql_baseline_bitwise_aggregates_expectations.sh`
to record MySQL 8.4.9 behavior and a fast plain C runtime test under
`packages/libmylite/tests/`, registered as
`libmylite.runtime.bitwise_aggregates`.

Coverage must include:

- successful `BIT_AND`, `BIT_OR`, and `BIT_XOR` over descriptor integer
  families and supported aliases, including negative signed stored values,
  `INT UNSIGNED`, `BIGINT UNSIGNED` values within the current physical range,
  and boolean aliases;
- `NULL` ignoring, empty tables, no matched rows, and all-`NULL` rows returning
  the MySQL neutral values;
- unsigned-64 text results above signed-64 range;
- labels, aliases, case-insensitive function names, block-comment label
  spacing, source-qualified aggregate arguments, quoted identifiers, and
  explicit invisible columns;
- baseline `WHERE` predicates, including comparison operators, `<=>`,
  `IS NULL`, and `IS NOT NULL`;
- schema-qualified and unqualified table resolution, missing default schema,
  unknown schema, unknown table, reserved `_mylite_*` names, unknown aggregate
  columns, and unknown predicate columns;
- unsupported syntax: whitespace/comment before `(`, literals, `NULL`,
  expression arguments, `DISTINCT`, no argument, `*`, multiple arguments,
  multiple aggregate items, mixed projection, `ORDER BY`, `LIMIT`, grouping,
  joins, windows, CTEs, subqueries, and binary-string forms;
- result metadata through the existing public result API: row count, affected
  rows, warning count, and absence of extra rows;
- persistence after close/reopen, behavior after table rename/drop, preamble
  preservation, and independent file-backed handles;
- SQLite bootstrap registration and zero-initialized cleanup for any new
  registration descriptors or callback state;
- regression coverage for lexer/parser, existing aggregate tests, row values,
  select where/order/limit, delete/update, storage opening, VFS, catalog,
  diagnostics, client-data, and registration tests through the standard check
  workflow.

## Compatibility Documentation

Update only the exact supported subset:

- `COMPATIBILITY.md` aggregate rows for `BIT_AND()`, `BIT_OR()`, and
  `BIT_XOR()`;
- `docs/compatibility/functions-aggregate.md`;
- `docs/compatibility/sql-query-expressions.md` projection and `SELECT`
  wording.

Do not update scalar bit-operator rows, binary-string type rows, grouping,
window-function, or expression metadata rows because this slice does not
implement those surfaces.
