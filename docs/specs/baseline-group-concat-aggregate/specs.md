# Baseline GROUP_CONCAT Aggregate

## Status

This feature specifies the first descriptor-driven `GROUP_CONCAT()` slice for
file-backed `.mylite` handles. It builds on the current public execution API,
statement context, parser scaffold, durable schema/table/column descriptors,
single-table DML, string and integer row values, descriptor-driven predicates,
limited ordering, and the existing one-column aggregate and grouped aggregate
execution paths.

The feature is intentionally not full MySQL `GROUP_CONCAT()` support. It
supports one or more descriptor column or supported row-scalar expression
arguments as a single per-row concatenated value, optional one descriptor
ordering key inside the aggregate, optional
string-literal separator, and short untruncated results. Wider MySQL forms are
explicitly deferred.

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
- File-backed MyLite opening VFS:
  `docs/specs/file-backed-mylite-opening-vfs/specs.md`
- MyLite file-format preamble:
  `docs/specs/mylite-file-format/specs.md`
- Baseline catalog foundation:
  `docs/specs/baseline-catalog-foundation/specs.md`
- Baseline row values lifecycle:
  `docs/specs/baseline-row-values-lifecycle/specs.md`
- Baseline VARCHAR type:
  `docs/specs/baseline-varchar-type/specs.md`
- Baseline TEXT type:
  `docs/specs/baseline-text-type/specs.md`
- Baseline SELECT order/limit lifecycle:
  `docs/specs/baseline-select-order-limit-lifecycle/specs.md`
- Baseline GROUP BY single-column aggregate:
  `docs/specs/baseline-group-by-single-column-aggregate/specs.md`
- MySQL lexer: `docs/specs/mysql-lexer/specs.md`
- MySQL parser scaffold:
  `docs/specs/mysql-parser-scaffold/specs.md`
- SQLite source snapshot notes: `third_party/sqlite/README.md`
- MySQL 8.4 Reference Manual, aggregate function descriptions:
  https://dev.mysql.com/doc/refman/8.4/en/aggregate-functions.html
- SQLite aggregate function documentation:
  https://www.sqlite.org/lang_aggfunc.html

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior, public
SQLite APIs, and existing MyLite source code. It does not copy MySQL, MariaDB,
Percona, SQLite implementation internals, or other restrictively licensed
implementation sources.

## MySQL 8.4.9 Observations

The expectation script
`packages/libmylite/tests/mysql_baseline_group_concat_aggregate_expectations.sh`
records runtime probes for this feature. Observed behavior that shapes this
slice:

- `GROUP_CONCAT(expr)` returns one string made from non-`NULL` values in the
  group. It returns `NULL` when there are no non-`NULL` values.
- The default separator is comma. `SEPARATOR 'literal'`, `SEPARATOR "literal"`,
  and `SEPARATOR ''` are accepted. `SEPARATOR NULL`, numeric separators, and
  character-set introducers before the separator literal are syntax errors in
  the probed subset.
- An aggregate-local `ORDER BY column [ASC|DESC]` controls value order inside
  the concatenated string. Omitted direction means ascending.
- MySQL's aggregate-local ordering around nullable sort keys differs from the
  already implemented top-level `ORDER BY` contract in the probed numeric
  cases. This slice therefore admits only `NOT NULL` aggregate-local order
  columns. Ties without a further sort key are not a deterministic
  compatibility contract for this slice.
- `GROUP_CONCAT (column)` is a syntax error under the default SQL mode and is
  accepted when `IGNORE_SPACE` is enabled.
- After a successful aggregate `SELECT`, a subsequent `ROW_COUNT()` returns
  `-1` and `@@warning_count` remains `0` for the short, untruncated outputs
  covered by this feature.
- MySQL supports wider forms such as multi-expression `DISTINCT`, ordinal and
  expression order keys, multiple order keys, binary result typing, window use,
  and `group_concat_max_len` truncation with warnings. MyLite defers these
  forms in this baseline slice.

## Scope

The implementation must add:

- parser and AST support for `GROUP_CONCAT` as a no-space-sensitive aggregate
  function name;
- one or more descriptor column or supported row-scalar expression arguments,
  optionally source-qualified where the existing aggregate source envelope
  admits source qualifiers; multiple arguments are lowered through the existing
  `CONCAT()` row-scalar value semantics for each row before aggregation;
- optional one aggregate-local `ORDER BY` descriptor column with optional
  `ASC` or `DESC`;
- optional `SEPARATOR` followed by an ordinary SQL string literal;
- one-item table-backed aggregate `SELECT GROUP_CONCAT(...) FROM table` with
  optional source alias and optional baseline `WHERE`;
- grouped form `SELECT group_column, GROUP_CONCAT(...) FROM table
  [WHERE ...] GROUP BY group_column [HAVING group_column_predicate]
  [ORDER BY group_column] [LIMIT ...]`;
- integer and nonbinary string-family descriptor columns, plus supported
  row-scalar expressions, as concatenated values;
- aggregate-local order keys limited to `NOT NULL` columns in the current
  descriptor `ORDER BY` storage envelope: integer, `BIT`, `YEAR`, `DATE`,
  `TIME`, `DATETIME`, and `TIMESTAMP` descriptor columns;
- MyLite-owned decoding and UTF-8/NUL validation for the admitted separator
  literal before SQLite binding;
- generated SQLite SQL built only from descriptors, stable physical table
  names, quoted identifiers, and bound separator/predicate/limit parameters;
- result labels from the SQL source span unless an explicit select-item alias
  is present;
- success reporting through the existing row-result conventions: one row for
  ungrouped aggregate queries, zero or more rows for grouped queries, no
  affected rows beyond existing `SELECT` behavior, and warning count `0` for
  supported short outputs;
- tests and MySQL 8.4.9 expectation artifacts for supported behavior and
  deliberately rejected wider MySQL forms.

## Non-Goals

This feature must not implement:

- `GROUP_CONCAT(DISTINCT expr, expr...)` tuple-distinct semantics;
- row-scalar expression arguments outside the existing aggregate row-scalar
  subset;
- aggregate-local expression, ordinal, alias, or multiple-column ordering;
- nullable aggregate-local order keys, string order keys, and
  collation-sensitive aggregate-local ordering;
- binary string, `BLOB`, `BIT`, `JSON`, `ENUM`, `SET`, approximate numeric, or
  exact decimal value concatenation unless a later spec proves their exact
  MySQL text conversion and result typing;
- aggregate use with joins, scalar subqueries, CTEs, window functions,
  `WITH ROLLUP`, full grouping, or aggregate-only grouped projection;
- `GROUP_CONCAT()` in `HAVING` predicates, selected aggregate aliases in
  `HAVING` when the selected aggregate is `GROUP_CONCAT`, or comparisons of
  concatenated strings in `HAVING`;
- `group_concat_max_len`, truncation warnings, binary/nonbinary result metadata
  fidelity, protocol-grade type metadata, or result-length caps beyond ordinary
  allocation failure handling;
- arbitrary SQLite pass-through or SQLite fork patches.

## Ownership Boundary

- The public API remains unchanged. `mylite_execute()` owns public call
  validation, result-handle ownership, public misuse behavior, and failure
  cleanup.
- Statement context owns statement diagnostics, warning count, affected rows,
  previous diagnostics snapshots, and transaction completion. Supported
  aggregate `SELECT` statements use existing row-result conventions and do not
  mutate statement transaction state.
- Lexer/parser/AST own syntax admission, no-space-sensitive function parsing,
  optional aggregate-local order/separator child nodes, and source spans. They
  do not inspect catalog descriptors, storage, or SQLite.
- Analyzer/planner code resolves source tables, aggregate value expressions,
  aggregate-local order columns, grouped columns, predicates, and result aliases
  against MyLite descriptors; rejects unsupported shapes; decodes separators;
  and lowers the supported subset to descriptor-safe SQLite.
- The catalog module owns durable descriptor rows, descriptor-cache generation,
  and logical metadata authority. `GROUP_CONCAT()` reads descriptors and rows
  but does not mutate catalog rows, descriptor versions, descriptor caches,
  catalog generation, or `sqlite_schema_generation`.
- The result builder owns copied result column labels and result cell values.
  SQL `NULL` remains represented as a `NULL` pointer returned by
  `mylite_result_value_text()`.
- SQLite owns physical row scanning, grouping, aggregate-local sorting, and
  string concatenation for generated SQL. MyLite uses SQLite's public aggregate
  invocation syntax and does not add a fork patch for this slice.
- Storage/VFS owns the `.mylite` preamble and shifted SQLite payload boundary.
  Supported aggregate reads must not write through byte range `[0, 4096)`.

## Supported SQL Grammar

Supported ungrouped form:

```sql
SELECT GROUP_CONCAT(value_expr [group_concat_order] [group_concat_separator]) [AS alias]
  FROM table_name [WHERE predicate]
```

The single-value `DISTINCT` variant is supported in the same envelope:

```sql
SELECT GROUP_CONCAT(DISTINCT value_expr [group_concat_order] [group_concat_separator]) [AS alias]
  FROM table_name [WHERE predicate]
```

Supported grouped form:

```sql
SELECT group_column,
       GROUP_CONCAT(value_expr [group_concat_order] [group_concat_separator]) [AS alias]
  FROM table_name [WHERE predicate]
  GROUP BY group_column
  [HAVING group_column_predicate]
  [ORDER BY group_column [ASC | DESC]]
  [LIMIT limit_clause]
```

Aggregate-local ordering and separators:

```sql
group_concat_order:
    ORDER BY order_column
  | ORDER BY order_column ASC
  | ORDER BY order_column DESC

group_concat_separator:
    SEPARATOR 'string_literal'
  | SEPARATOR "string_literal"
```

`table_name`, source aliases, `WHERE`, grouped-column `HAVING`, grouped-result
`ORDER BY`, and grouped-result `LIMIT` use the already specified baseline
subsets. The aggregate argument and aggregate-local order key may parse as
qualified identifiers so the analyzer can apply existing descriptor source
resolution.

### MyLite Lemon-Syntax Snippet

This snippet describes the intended MyLite grammar extension, not MySQL's full
grammar:

```lemon
expression ::=
    GROUP_CONCAT LPAREN group_concat_value
        group_concat_order_opt group_concat_separator_opt RPAREN.
expression ::=
    GROUP_CONCAT LPAREN DISTINCT group_concat_single_value
        group_concat_order_opt group_concat_separator_opt RPAREN.

group_concat_value ::= qualified_identifier.
group_concat_value ::= supported_row_scalar_expression.
group_concat_value ::= group_concat_value COMMA supported_row_scalar_expression.
group_concat_single_value ::= qualified_identifier.
group_concat_single_value ::= supported_row_scalar_expression.

group_concat_order_opt ::= .
group_concat_order_opt ::= ORDER BY qualified_identifier order_direction_opt.

group_concat_separator_opt ::= .
group_concat_separator_opt ::= SEPARATOR STRING.

order_direction_opt ::= .
order_direction_opt ::= ASC.
order_direction_opt ::= DESC.
```

`GROUP_CONCAT` follows the same no-space-sensitive function-name rule as
`COUNT`, `MIN`, `MAX`, and `SUM`: whitespace between the function token and
`(` is rejected unless `IGNORE_SPACE` is active in the parser SQL mode.

## Name Resolution And Types

Unqualified source tables use the selected schema. Schema-qualified table names
resolve against MyLite schema descriptors regardless of the selected schema.
Missing default schema, unknown schema, unknown table, reserved `_mylite_*`
schema/table names, and unsupported object kinds reuse existing descriptor
diagnostics before any SQLite SQL is generated.

Descriptor-column value arguments are resolved through MyLite descriptors, not
SQLite metadata. This slice admits integer-family columns, nonbinary
`CHAR`/`VARCHAR`/`TEXT` family columns, and supported row-scalar expressions.
`NULL` value cells are skipped by the aggregate. All-`NULL` groups and empty
inputs return SQL `NULL`.

The aggregate-local order column is resolved through MyLite descriptors and
uses `NOT NULL` columns in the current descriptor `ORDER BY` storage envelope:
integer, `BIT`, `YEAR`, `DATE`, `TIME`, `DATETIME`, and `TIMESTAMP`. Omitted
direction means ascending. Nullable aggregate-local order keys are deferred
because MySQL's probed aggregate-local `NULL` placement differs from the
already implemented top-level `ORDER BY` contract. Duplicate order-key values
without another key have no deterministic compatibility guarantee.

Column descriptor matching continues to use the current catalog identifier
policy: ASCII case-insensitive identifier comparison with the existing
descriptor collation expectations. This feature does not add string collation
ordering for aggregate-local order keys.

## Separator Conversion

When no separator is specified, MySQL and SQLite both use comma for the
admitted single-expression form.

When `SEPARATOR` is specified, the parser admits only ordinary string literals.
The runtime decodes the literal using the session's current string-literal
escape mode, rejects decoded NUL bytes, rejects invalid UTF-8, stores the
decoded bytes in the plan, and binds the separator into the generated SQLite
statement as text. Empty string separators are supported.

`SEPARATOR NULL`, numeric separators, parameters, variables, functions, hex
literals, bit literals, national string literals, and character-set introducers
are outside this baseline slice.

## SQLite Lowering

The generated SQL shape for an ungrouped aggregate is:

```sql
SELECT group_concat("value_column" [ , ?N ] [ ORDER BY "order_column" ASC|DESC ])
FROM "_mylite_user_table_<table_id>"
[WHERE descriptor_predicate]
```

The grouped form adds descriptor group projection and grouping:

```sql
SELECT "group_column",
       group_concat("value_column" [ , ?N ] [ ORDER BY "order_column" ASC|DESC ])
FROM "_mylite_user_table_<table_id>"
[WHERE descriptor_predicate]
GROUP BY "group_column"
[HAVING grouped_column_predicate]
[ORDER BY "group_column" ASC|DESC]
[LIMIT ?M [OFFSET ?K]]
```

Every generated SQLite identifier is quoted. The physical table name is the
stable descriptor-owned name, such as `_mylite_user_table_<table_id>`. The
separator, predicates, and grouped limit/offset values are bound parameters.
SQL literals from user input are not interpolated into generated SQLite SQL.

This approach uses public SQLite aggregate invocation syntax over the pinned
SQLite build. It does not require optional SQLite `UPDATE/DELETE ORDER BY
LIMIT` support, custom virtual tables, or SQLite fork patches.

## Diagnostics

Supported successful statements produce warning count `0` for the short,
untruncated outputs covered by this feature.

Diagnostics to cover:

- syntax errors and unsupported grammar;
- missing default schema, unknown schema, unknown table, reserved target names,
  and unsupported object kind;
- unknown concatenated value column, unknown aggregate-local order column, and
  ambiguous source-qualified columns;
- unsupported value column type;
- unsupported aggregate-local order column type or nullable aggregate-local
  order column;
- unsupported separator literal or decoded separator bytes;
- unsupported `DISTINCT`, unsupported row-scalar expression arguments,
  expression/ordinal/multiple order keys, window use, scalar-subquery use, and
  `HAVING` aggregate predicates over `GROUP_CONCAT`;
- physical SQLite failures and allocation failures;
- public API misuse only if a public surface changes, which this feature does
  not do.

Unsupported wider MySQL forms use deterministic MyLite diagnostics when MySQL
would otherwise accept syntax that this baseline slice deliberately defers.

## Tests

Add fast plain C coverage under `packages/libmylite/tests/`, preferably in a
new `runtime_group_concat_aggregate` test binary, and register it with a dotted
CTest name. Coverage must include:

- `GROUP_CONCAT(integer_column ORDER BY integer_column)` over a persistent base
  table;
- `GROUP_CONCAT(varchar_column ORDER BY integer_column SEPARATOR '|')`;
- `SEPARATOR ''` and double-quoted separator literals;
- `WHERE` filtering, empty input, all-`NULL` input, and `NULL` value skipping;
- aggregate-local default direction, explicit `ASC`, and explicit `DESC` over
  `NOT NULL` order keys without duplicate order-key ties;
- grouped `GROUP_CONCAT` by one integer descriptor column, including an
  all-`NULL` group;
- grouped-column `HAVING`, grouped-result `ORDER BY`, and grouped-result
  `LIMIT` interactions that remain inside existing grouped aggregate limits;
- source aliases and qualified argument/order references where admitted by
  existing source resolution;
- result labels, explicit aliases, warning count, and `ROW_COUNT()` after
  aggregate `SELECT`;
- persistence across close/reopen and after table rename;
- table drop behavior and independent file-backed handles;
- `.mylite` preamble preservation and no catalog generation or
  `sqlite_schema_generation` mutation for reads;
- zero-initialized cleanup for new plan fields;
- deterministic rejection of unsupported value types and unsupported syntax:
  `DISTINCT`, unsupported row-scalar expressions, aggregate-local
  ordinal/expression/multiple
  order keys, nullable order keys, string order keys, `SEPARATOR NULL`,
  numeric separators, parameters, window forms, subqueries, joins, and
  aggregate predicates in `HAVING`.

The MySQL expectation script must run against MySQL 8.4.9. If that runtime is
unavailable, changing user-visible expectations is blocked.

## Compatibility Documentation

Update `COMPATIBILITY.md`, `docs/compatibility/functions-aggregate.md`, and
`docs/compatibility/sql-query-expressions.md` only for this exact partial
surface. Do not claim full `GROUP_CONCAT`, multi-expression `DISTINCT`, broad
expression arguments, expression ordering, string collation ordering,
truncation warnings, `group_concat_max_len`, binary result metadata, windows,
or full grouping.

## Verification

Before marking the feature done:

1. Run `packages/libmylite/tests/mysql_baseline_group_concat_aggregate_expectations.sh`.
2. Run `cmake --build --preset dev`.
3. Run the new CTest entry and the existing parser/count/min-max/sum/avg/bitwise
   aggregate/grouped aggregate entries.
4. Run `cmake --workflow --preset check`.
5. Review the final diff for architecture boundaries, public ABI stability,
   independently authored grammar/spec text, MySQL 8.4.9 evidence, descriptor
   authority, generated SQLite SQL shape, bound separator handling,
   `NULL`/ordering behavior, file-format safety, zero-init cleanup,
   compatibility-matrix accuracy, and test relevance.
