# Baseline INTERVAL Function

## Goal

Add a narrow `INTERVAL()` comparison-function slice for scalar projections,
`DO`, and descriptor-backed single-table row projection.

This is not a general numeric coercion engine or expression evaluator. It
extends the current scalar and row-scalar function machinery after `FIELD()` and
`GREATEST()` / `LEAST()`.

## Sources

- Official MySQL 8.4 Reference Manual, comparison functions and operators:
  <https://dev.mysql.com/doc/refman/8.4/en/comparison-operators.html>
- Existing row-scalar expression design:
  `docs/specs/baseline-row-scalar-expressions/specs.md`
- Existing `FIELD()` design:
  `docs/specs/baseline-field-function/specs.md`
- Existing `GREATEST()` / `LEAST()` design:
  `docs/specs/baseline-greatest-least-functions/specs.md`
- Observed MySQL 8.4.9 runtime behavior, captured by
  `packages/libmylite/tests/mysql_baseline_interval_function_expectations.sh`.

The MyLite grammar and implementation are independently authored from official
documentation, observed MySQL 8.4.9 behavior, public SQLite APIs, and existing
MyLite code. Do not copy MySQL, MariaDB, Percona, SQLite implementation
internals, or restrictively licensed sources.

## MySQL 8.4.9 Runtime Observations

Runtime probes establish the behavior used by this baseline:

- `INTERVAL(search, threshold[, threshold ...])` returns the count of
  thresholds less than or equal to the first argument when thresholds are sorted
  ascending.
- A `NULL` first argument returns `-1`.
- Equal and duplicate thresholds count independently. For example,
  `INTERVAL(1, 1, 1, 2)` returns `2`.
- Integer and boolean arguments compare numerically, including `TRUE` as `1`,
  `FALSE` as `0`, and optional unary signs on integer literals.
- MySQL parses `INTERVAL()` and `INTERVAL(1)` as syntax errors
  (`1064 / 42000`), not native function argument-count errors.
- MySQL accepts broader coercions, including string, decimal, and `NULL`
  threshold values, and may emit truncation warnings for string-to-number
  conversion. This MyLite slice defers those broader coercions.
- MySQL documents sorted thresholds as the correct input shape. This MyLite
  slice rejects descending threshold pairs deterministically instead of relying
  on undefined or implementation-dependent results.
- `SELECT INTERVAL(...)`, `SELECT INTERVAL(...) FROM DUAL`, and `DO
  INTERVAL(...)` follow existing scalar result status conventions: successful
  `SELECT` makes a following `ROW_COUNT()` return `-1`, successful `DO` makes
  `ROW_COUNT()` return `0`, and supported in-range expressions produce zero
  warnings.

## Supported Surface

MyLite supports:

- no-source `SELECT`, `SELECT ... FROM DUAL`, and `DO`;
- single-table row-scalar `SELECT` using the existing row envelope:
  `FROM table [AS alias]`, optional existing `WHERE`, single descriptor-column
  `ORDER BY`, and existing `LIMIT`;
- flat `INTERVAL(search, threshold[, threshold ...])` calls with one search
  argument and at least one threshold argument;
- search argument:
  - signed 64-bit integer literal with optional unary sign;
  - `TRUE` and `FALSE`;
  - `NULL`;
  - integer-family descriptor column in table-backed row-scalar `SELECT`;
- threshold arguments:
  - signed 64-bit integer literal with optional unary sign;
  - `TRUE` and `FALSE`;
- ascending, nondecreasing threshold lists, including duplicates;
- warning count `0` for supported in-range forms.

Integer literals must fit the signed 64-bit scalar envelope used by current
row-scalar expressions. Descriptor columns are resolved through MyLite catalog
descriptors, not SQLite schema text.

## Deferred Surface

This slice intentionally does not support:

- string, decimal, approximate, hex, bit, binary, temporal, JSON, enum, set, or
  spatial arguments;
- `NULL` threshold arguments;
- unsorted threshold lists;
- threshold descriptor columns;
- nested `INTERVAL()`, nested `FIELD()` / `GREATEST()` / `LEAST()`, arithmetic,
  flow-control, temporal, aggregate, subquery, parameter, or variable arguments
  inside `INTERVAL()`;
- use in predicates, ordering expressions, grouping expressions, DML
  assignments, defaults, generated columns, indexes, constraints, joins, CTEs,
  views, or arbitrary SQLite pass-through.

## Grammar

MyLite adds a keyword-function production that requires at least two arguments:

```lemon
expression(A) ::= INTERVAL(T) LPAREN expression(B) COMMA function_argument_list(C) RPAREN(R).
```

The resulting AST node stores the search argument as its first child and a
function-argument-list node containing thresholds as its second child. There is
no zero-argument or one-argument AST marker because MySQL 8.4.9 reports those
forms as syntax errors.

Analyzer/runtime acceptance for this feature is narrower:

```lemon
interval_expr(A) ::= INTERVAL(T) LPAREN interval_search(B) COMMA interval_threshold_list(C) RPAREN(R).

interval_search(A) ::= descriptor_integer_column(B).
interval_search(A) ::= decimal_integer_literal(T).
interval_search(A) ::= PLUS(P) decimal_integer_literal(T).
interval_search(A) ::= MINUS(M) decimal_integer_literal(T).
interval_search(A) ::= TRUE(T).
interval_search(A) ::= FALSE(T).
interval_search(A) ::= NULL(T).
interval_search(A) ::= LPAREN interval_search(B) RPAREN(R).

interval_threshold(A) ::= decimal_integer_literal(T).
interval_threshold(A) ::= PLUS(P) decimal_integer_literal(T).
interval_threshold(A) ::= MINUS(M) decimal_integer_literal(T).
interval_threshold(A) ::= TRUE(T).
interval_threshold(A) ::= FALSE(T).
interval_threshold(A) ::= LPAREN interval_threshold(B) RPAREN(R).

interval_threshold_list(A) ::= interval_threshold(B).
interval_threshold_list(A) ::= interval_threshold_list(B) COMMA interval_threshold(C).
```

These snippets describe MyLite's supported subset, not MySQL's full grammar.

## Runtime Semantics

Planning:

1. Detect scalar and row-scalar projection attempts when a supported expression
   contains a top-level or parenthesized `INTERVAL()` call.
2. Resolve the optional source table through the existing selected/default
   schema policy.
3. Resolve the optional descriptor-column search argument through MyLite catalog
   descriptors, not SQLite schema text.
4. Convert admitted literal arguments to owned planned values before generating
   SQLite SQL or scalar result cells.
5. Validate that every threshold is non-`NULL`, signed-64 integer-domain, and
   not less than the previous threshold.
6. Generate a descriptor-driven SQLite `CASE` expression using quoted
   identifiers and numbered bound parameters.

For `DO` execution, MyLite evaluates admitted values in C. For no-source,
`DUAL`, and table-backed `SELECT`, `INTERVAL()` uses the existing row-scalar
projection path: MyLite builds the generated SQLite projection with bound
parameters and lets SQLite execute the projection. Table-backed forms also let
SQLite scan, filter, order, and limit rows. MyLite does not materialize source
rows to evaluate the function in C.

Generated row-scalar SQL shape:

```sql
CASE
  WHEN search IS NULL THEN -1
  WHEN search < threshold1 THEN 0
  WHEN search < threshold2 THEN 1
  ...
  ELSE threshold_count
END
```

Using `<` instead of `<=` preserves duplicate-threshold counting: a search value
equal to a threshold advances to the next interval. Result constants are
generated only from the validated threshold count. Literal search and threshold
expressions are bound as parameters. The search expression is either a quoted
descriptor column or a bound literal value.

## Ownership Boundaries

- Public API: unchanged. Successful statements use the existing
  `mylite_execute()` and result APIs.
- Statement context: unchanged. Successful `SELECT` and `DO` preserve existing
  row-count and warning-count conventions.
- Lexer/parser/AST: reuse the existing `INTERVAL` token and add an
  `interval_function` AST node. Parser source spans remain authoritative for
  default result labels.
- Analyzer/planner: extend scalar and row-scalar planning to resolve arguments,
  validate integer domains and sorted thresholds, reject unsupported shapes, and
  build generated SQL.
- Catalog: read-only descriptor authority. No descriptor rows, descriptor
  versions, descriptor caches, catalog generation, or `sqlite_schema_generation`
  are mutated.
- Result builder: returns signed integer text through existing scalar/row result
  conventions. Explicit aliases override default source-span labels.
- Storage/VFS/file format: unchanged. `.mylite` preamble and shifted SQLite
  payload invariants are preserved.
- SQLite: use generated standard SQLite `CASE`, bound parameters, and quoted
  identifiers. No SQLite fork patch is required.

## Diagnostics

Required diagnostics:

- `INTERVAL()` and `INTERVAL(1)` parse as syntax errors through existing
  `1064 / 42000` parse diagnostics;
- missing default schema, unknown schema/table, reserved table names, and
  unsupported object kinds through existing row-scalar source diagnostics;
- unknown descriptor-column search arguments through MySQL-compatible
  unknown-column diagnostics in field-list context;
- unsupported search argument shapes:
  `INTERVAL() supports only integer, boolean, and NULL search arguments`;
- unsupported threshold argument shapes:
  `INTERVAL() supports only integer and boolean threshold arguments`;
- unsupported threshold columns:
  `INTERVAL() supports only literal threshold arguments`;
- `NULL` thresholds:
  `INTERVAL() threshold arguments cannot be NULL`;
- unsorted thresholds:
  `INTERVAL() threshold arguments must be sorted ascending`;
- signed-64 overflow:
  `INTERVAL() integer literals must fit the signed 64-bit range`;
- physical SQLite failures through existing runtime diagnostics;
- allocation failures through existing `MYLITE_NOMEM` behavior.

Unsupported behavior is rejected deterministically with MyLite-specific
diagnostics when MySQL accepts a broader coercion domain.

## Tests

Add fast C tests covering:

- scalar no-source and `DUAL` projections;
- `DO` execution and status;
- integer, boolean, signed boundary, `NULL` search, and duplicate-threshold
  behavior;
- table-backed row-scalar projection over integer-family descriptor columns;
- preservation of existing `WHERE`, descriptor-column `ORDER BY`, and `LIMIT`
  row envelope behavior;
- syntax errors for zero- and one-argument forms;
- deterministic diagnostics for unknown columns, unsupported search arguments,
  unsupported threshold arguments, `NULL` thresholds, unsorted thresholds, and
  out-of-range integer literals.

Add a MySQL 8.4.9 expectation script for supported values and documented
deferred coercions.

## Compatibility Updates

Update `COMPATIBILITY.md`, `docs/compatibility/functions-comparison.md`,
`docs/compatibility/sql-query-expressions.md`, and
`docs/compatibility/type-system-literals-conversion.md` only for the exact
supported subset. Do not claim full MySQL `INTERVAL()` coercion, expression,
warning, collation, or metadata behavior.
