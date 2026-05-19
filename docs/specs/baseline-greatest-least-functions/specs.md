# Baseline GREATEST and LEAST Functions

## Goal

Add a narrow `GREATEST()` / `LEAST()` slice for common MySQL comparison
expressions in scalar projections, `DO`, and descriptor-backed single-table row
projection.

This is not a general expression engine, general type aggregation layer, or full
MySQL collation implementation. It extends the existing scalar and row-scalar
function machinery after `FIELD()`.

## Sources

- Official MySQL 8.4 Reference Manual, comparison functions and operators:
  <https://dev.mysql.com/doc/refman/8.4/en/comparison-operators.html>
- Official MySQL 8.4 Reference Manual, type conversion in expression
  evaluation:
  <https://dev.mysql.com/doc/refman/8.4/en/type-conversion.html>
- Existing row-scalar expression design:
  `docs/specs/baseline-row-scalar-expressions/specs.md`
- Existing `FIELD()` design:
  `docs/specs/baseline-field-function/specs.md`
- Observed MySQL 8.4.9 runtime behavior, captured by
  `packages/libmylite/tests/mysql_baseline_greatest_least_functions_expectations.sh`.

The MyLite grammar and implementation are independently authored from official
documentation, observed MySQL 8.4.9 behavior, public SQLite APIs, and existing
MyLite code. Do not copy MySQL, MariaDB, Percona, SQLite implementation
internals, or restrictively licensed sources.

## MySQL 8.4.9 Runtime Observations

Runtime probes establish the behavior used by this baseline:

- `GREATEST(value1, value2, ...)` returns the maximum-valued argument and
  `LEAST(value1, value2, ...)` returns the minimum-valued argument.
- Both functions require at least two arguments. Zero- and one-argument calls
  fail with `1582 / 42000`.
- If any argument is `NULL`, the function returns `NULL`.
- All-integer argument lists compare numerically, including `TRUE` as `1`,
  `FALSE` as `0`, and optional unary signs on integer literals.
- All-string argument lists compare with the active nonbinary string collation.
  Under MySQL's default `utf8mb4_0900_ai_ci`, ASCII comparisons are
  case-insensitive. In tied case-insensitive ASCII comparisons, observed MySQL
  8.4.9 returns the later tied argument for `GREATEST()` and the earlier tied
  argument for `LEAST()`.
- Mixed string/numeric argument lists are accepted by MySQL and compared as
  strings. This broader coercion domain is deferred by this MyLite slice.
- `SELECT GREATEST(...)`, `SELECT LEAST(...)`, and the same forms from `DUAL`
  return one row. Successful `SELECT` makes a following `ROW_COUNT()` return
  `-1`; successful `DO` makes `ROW_COUNT()` return `0`.
- Table-backed `GREATEST(column, literal, ...)` and `LEAST(column, literal, ...)`
  evaluate once per source row and preserve the existing single-table row
  envelope for `WHERE`, descriptor-column `ORDER BY`, and `LIMIT`.

## Supported Surface

MyLite supports:

- no-source `SELECT`, `SELECT ... FROM DUAL`, and `DO`;
- single-table row-scalar `SELECT` using the existing row envelope:
  `FROM table [AS alias]`, optional existing `WHERE`, single descriptor-column
  `ORDER BY`, and existing `LIMIT`;
- flat `GREATEST(arg, arg[, arg ...])` and `LEAST(arg, arg[, arg ...])` calls
  with two or more arguments;
- all-string comparison domain:
  - ASCII string literals without embedded `NUL`;
  - `CHAR`, `VARCHAR`, and bare `TEXT` family descriptor columns in
    table-backed row-scalar `SELECT`;
  - `NULL` arguments;
- all-integer comparison domain:
  - signed 64-bit integer literals with optional unary sign;
  - `TRUE` and `FALSE` as `1` and `0`;
  - integer-family descriptor columns in table-backed row-scalar `SELECT`;
  - `NULL` arguments;
- fixed `utf8mb4_0900_ai_ci` ASCII case-insensitive semantics for admitted
  string comparisons, matching the current string-predicate and `FIELD()`
  baselines;
- MySQL-compatible tied ASCII case behavior for admitted row and scalar values:
  `GREATEST()` keeps replacing the current winner on equality, while `LEAST()`
  keeps the first equal winner;
- warning count `0` for supported in-range forms.

String literal values must decode to ordinary UTF-8 text without embedded
`NUL`; MyLite claims MySQL collation parity only for ASCII values in this
slice. Integer literals must fit the signed 64-bit scalar envelope used by
current row-scalar expressions.

## Deferred Surface

This slice intentionally does not support:

- mixed string/numeric comparison domains;
- exact decimal, approximate, temporal, binary, blob, enum, set, JSON, or
  spatial comparison domains;
- non-ASCII collation weights, accent folding, contractions, expansions, or
  other full `utf8mb4_0900_ai_ci` behavior;
- explicit `BINARY`, `CAST()`, `COLLATE`, introducers, connection collation
  changes, or binary-string result typing;
- nested `GREATEST()`, nested `LEAST()`, nested `FIELD()`, arithmetic,
  flow-control, temporal, aggregate, subquery, parameter, or variable
  arguments inside `GREATEST()` / `LEAST()`;
- use in predicates, ordering expressions, grouping expressions, DML
  assignments, defaults, generated columns, indexes, constraints, joins, CTEs,
  views, or arbitrary SQLite pass-through.

## Grammar

MyLite adds list-argument parser productions:

```lemon
expression(A) ::= GREATEST(T) LPAREN function_argument_list(B) RPAREN(R).
expression(A) ::= LEAST(T) LPAREN function_argument_list(B) RPAREN(R).
```

Analyzer/runtime acceptance for this feature is narrower:

```lemon
comparison_extrema_expr(A) ::= GREATEST(T) LPAREN extrema_arg_list(B) RPAREN(R).
comparison_extrema_expr(A) ::= LEAST(T) LPAREN extrema_arg_list(B) RPAREN(R).

extrema_arg(A) ::= descriptor_string_column(B).
extrema_arg(A) ::= descriptor_integer_column(B).
extrema_arg(A) ::= string_literal(T).
extrema_arg(A) ::= decimal_integer_literal(T).
extrema_arg(A) ::= PLUS(P) decimal_integer_literal(T).
extrema_arg(A) ::= MINUS(M) decimal_integer_literal(T).
extrema_arg(A) ::= TRUE(T).
extrema_arg(A) ::= FALSE(T).
extrema_arg(A) ::= NULL(T).
extrema_arg(A) ::= LPAREN extrema_arg(B) RPAREN(R).

extrema_arg_list(A) ::= extrema_arg(B) COMMA extrema_arg(C).
extrema_arg_list(A) ::= extrema_arg_list(B) COMMA extrema_arg(C).
```

`GREATEST` and `LEAST` remain usable as unquoted identifiers where the parser
admits identifiers; they are not whitespace-sensitive function names in this
slice.

These snippets describe MyLite's supported subset, not MySQL's full grammar.

## Runtime Semantics

Planning:

1. Detect scalar and row-scalar projection attempts when a supported expression
   contains a top-level or parenthesized `GREATEST()` or `LEAST()` call.
2. Resolve the optional source table through the existing selected/default
   schema policy.
3. Resolve descriptor column arguments through MyLite catalog descriptors, not
   SQLite schema text.
4. Convert admitted literal arguments to owned planned values before generating
   SQLite SQL or scalar result cells.
5. Classify the non-`NULL` argument domain as all string or all integer. Reject
   mixed domains deterministically.
6. Generate a SQLite scalar `max()` / `min()` expression over the planned
   argument list, using quoted identifiers and numbered bound parameters.

For scalar no-source and `DO` execution, MyLite evaluates the admitted values in
C because there is no SQLite source row. For row-scalar table projections,
MyLite does not materialize source rows to evaluate the function in C; it builds
the descriptor-driven SQLite projection and lets SQLite scan, filter, order,
and limit rows.

Generated row-scalar SQL shape:

```sql
max(argN, ..., arg1) -- GREATEST
min(argN, ..., arg1) -- LEAST
```

SQLite's multi-argument `max()` and `min()` return `NULL` when any argument is
`NULL`, which matches this MySQL slice. MyLite reverses the argument order before
lowering because observed SQLite tie selection is opposite of the observed MySQL
8.4.9 identity for admitted ASCII case-insensitive string ties; the reversed
lowering preserves later-tied `GREATEST()` output and earlier-tied `LEAST()`
output. For string-domain calls, each argument applies MyLite's registered
`utf8mb4_0900_ai_ci` ASCII collation. For integer-domain calls, comparisons use
ordinary integer ordering.

## Ownership Boundaries

- Public API: unchanged. Successful statements use the existing
  `mylite_execute()` and result APIs.
- Statement context: unchanged. Successful `SELECT` and `DO` preserve existing
  row-count and warning-count conventions.
- Lexer/parser/AST: add `GREATEST` and `LEAST` tokens and AST nodes. Parser
  source spans remain authoritative for default result labels.
- Analyzer/planner: extend scalar and row-scalar planning to resolve arguments,
  classify domains, reject unsupported shapes, and build generated SQL.
- Catalog: read-only descriptor authority. No descriptor rows, descriptor
  versions, descriptor caches, catalog generation, or
  `sqlite_schema_generation` are mutated.
- Result builder: returns text/integer/`NULL` through existing scalar/row result
  conventions. Explicit aliases override default source-span labels.
- Storage/VFS/file format: unchanged. `.mylite` preamble and shifted SQLite
  payload invariants are preserved.
- SQLite: use generated standard SQLite `max()` / `min()`, bound parameters, quoted
  identifiers, and the registered MyLite collation. No SQLite fork patch is
  required.

## Diagnostics

Required diagnostics:

- parser syntax errors through existing parse diagnostics;
- wrong argument count: MySQL-compatible `1582 / 42000` native-function
  argument-count error for fewer than two arguments;
- missing default schema, unknown schema/table, reserved table names, and
  unsupported object kinds through existing row-scalar source diagnostics;
- unknown descriptor column arguments through MySQL-compatible unknown-column
  diagnostics in field-list context;
- unsupported argument shapes:
  `GREATEST() and LEAST() support only string, integer, boolean, and NULL arguments`;
- unsupported mixed domain:
  `GREATEST() and LEAST() do not support mixed string and numeric arguments`;
- unsupported non-ASCII or embedded-`NUL` string values with deterministic
  MyLite-specific diagnostics;
- signed-64 integer overflow:
  `GREATEST() and LEAST() integer literals must fit the signed 64-bit range`;
- physical SQLite prepare/step/finalize failures through existing runtime
  diagnostics;
- allocation failures through existing no-memory diagnostics.

## Tests

Add a focused plain C test binary under `packages/libmylite/tests/`, registered
as a dotted CTest entry, plus a MySQL 8.4.9 expectation script.

Coverage must include:

- no-source `SELECT`, `SELECT ... FROM DUAL`, and `DO`;
- labels, whitespace before `(`, explicit aliases, row count, warning count, and
  absence/presence of rows according to statement kind;
- zero- and one-argument native function count errors;
- integer literals, signed integer boundaries, booleans, and `NULL`
  propagation;
- ASCII string literals, default case-insensitive comparison, duplicate and
  tied values, and empty strings;
- table-backed integer and string descriptor columns, nullable columns, row
  order, `WHERE`, descriptor-column `ORDER BY`, and `LIMIT`;
- unknown column diagnostics in row-scalar projection;
- deterministic rejection for mixed domains, unsupported non-ASCII strings,
  unsupported binary/decimal/float/temporal/expression/subquery/parameter
  arguments, nested calls, predicate use, DML assignment use, and ordering
  expression use;
- reopen persistence indirectly through existing row storage tests remaining
  green, since this feature is read-only;
- existing parser, runtime scalar, row-scalar, statement context, catalog,
  storage, and VFS tests still passing.

## Compatibility Documentation

Update:

- `COMPATIBILITY.md`;
- `docs/compatibility/functions-comparison.md`;
- `docs/compatibility/sql-query-expressions.md`;
- `docs/compatibility/type-system-literals-conversion.md` only for the admitted
  literal surfaces.

Do not overclaim full `GREATEST()` / `LEAST()`, mixed type coercion, decimals,
floating point, binary strings, full Unicode collation, nested expressions, DML
use, predicate use, ordering/grouping expression use, or general expression
evaluation.
