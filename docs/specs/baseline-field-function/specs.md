# Baseline FIELD Function

## Goal

Add the first narrow `FIELD()` slice needed by common application SQL such as:

```sql
SELECT FIELD(option_name, 'User 0000018', 'User 0000019', 'User 0000020')
FROM options;
```

This feature extends the existing scalar and row-scalar projection path. It is
not a general expression engine, general collation layer, or full MySQL
coercion implementation.

## Sources

- Official MySQL 8.4 Reference Manual, string functions and operators:
  <https://dev.mysql.com/doc/refman/8.4/en/string-functions.html>
- Official MySQL 8.4 Reference Manual, expression syntax:
  <https://dev.mysql.com/doc/refman/8.4/en/expressions.html>
- Existing row-scalar expression design:
  `docs/specs/baseline-row-scalar-expressions/specs.md`
- Existing string comparison design:
  `docs/specs/baseline-string-equality-predicates/specs.md`
- Observed MySQL 8.4.9 runtime behavior, captured by
  `packages/libmylite/tests/mysql_baseline_field_function_expectations.sh`.

The MyLite grammar and implementation are independently authored from official
documentation, observed MySQL 8.4.9 behavior, public SQLite APIs, and existing
MyLite code. Do not copy MySQL, MariaDB, Percona, SQLite implementation
internals, or restrictively licensed sources.

## MySQL 8.4.9 Runtime Observations

Runtime probes establish the behavior used by this baseline:

- `FIELD(search, value1, value2, ...)` returns the 1-based position of the
  first equal value after the search argument, or `0` when no value matches.
- `FIELD(NULL, ...)` returns `0`; `NULL` candidates do not match non-`NULL`
  search values.
- At least two arguments are required. `FIELD()` and `FIELD('a')` fail with
  `1582 / 42000`.
- String comparisons use the active collation. Under MySQL's default
  `utf8mb4_0900_ai_ci`, ASCII case differences do not prevent a match:
  `FIELD('abc', 'ABC')` returns `1`. The collation is also accent-insensitive
  for broader Unicode values, but non-ASCII collation behavior is deferred by
  this MyLite slice.
- `FIELD()` accepts whitespace between the function name and `(` in default SQL
  mode.
- `SELECT FIELD(...)` and `SELECT FIELD(...) FROM DUAL` return one row.
  Successful `SELECT` makes a following `ROW_COUNT()` return `-1`; successful
  `DO FIELD(...)` makes `ROW_COUNT()` return `0`.
- Table-backed `FIELD(column, literal, ...)` evaluates once per source row and
  preserves the existing single-table row envelope for `WHERE`, `ORDER BY`, and
  `LIMIT`.
- If all arguments are numbers, MySQL compares numerically. If all arguments
  are strings, MySQL compares as strings. Mixed domains are valid MySQL but may
  use double conversion and warnings; this mixed domain is deferred.

## Supported Surface

MyLite supports:

- no-source `SELECT`, `SELECT ... FROM DUAL`, and `DO`;
- single-table row-scalar `SELECT` using the existing row envelope:
  `FROM table [AS alias]`, optional existing `WHERE`, single descriptor-column
  `ORDER BY`, and existing `LIMIT`;
- `FIELD(search, value[, value ...])` with two or more arguments;
- flat `FIELD()` calls only; no nested `FIELD()`, nested `CONCAT()`, or
  arbitrary function nesting inside `FIELD()` arguments;
- all-string comparison domain:
  - string literals;
  - `CHAR`, `VARCHAR`, and bare `TEXT` family descriptor columns in
    table-backed row-scalar `SELECT`;
  - `NULL` arguments;
- all-integer comparison domain:
  - signed 64-bit integer literals with optional unary sign;
  - `TRUE` and `FALSE` as `1` and `0`;
  - integer-family descriptor columns in table-backed row-scalar `SELECT`;
  - `NULL` arguments;
- fixed `utf8mb4_0900_ai_ci` ASCII case-insensitive semantics for admitted
  string comparisons, matching the current string-predicate baseline;
- result values as integer text through existing result APIs;
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
- nested `FIELD()`, `CONCAT(FIELD(...))`, `FIELD(CONCAT(...), ...)`, arithmetic,
  flow-control, temporal, aggregate, subquery, parameter, or variable
  arguments;
- use in predicates, ordering expressions, grouping expressions, DML
  assignments, defaults, generated columns, indexes, constraints, joins, CTEs,
  views, or arbitrary SQLite pass-through.

## Grammar

MyLite adds a single parser production for `FIELD()`:

```lemon
expression(A) ::= FIELD(T) LPAREN function_argument_list(B) RPAREN(R).
```

Analyzer/runtime acceptance for this feature is narrower:

```lemon
field_expr(A) ::= FIELD(T) LPAREN field_arg_list(B) RPAREN(R).

field_arg(A) ::= descriptor_string_column(B).
field_arg(A) ::= descriptor_integer_column(B).
field_arg(A) ::= string_literal(T).
field_arg(A) ::= decimal_integer_literal(T).
field_arg(A) ::= PLUS(P) decimal_integer_literal(T).
field_arg(A) ::= MINUS(M) decimal_integer_literal(T).
field_arg(A) ::= TRUE(T).
field_arg(A) ::= FALSE(T).
field_arg(A) ::= NULL(T).
field_arg(A) ::= LPAREN field_arg(B) RPAREN(R).

field_arg_list(A) ::= field_arg(B) COMMA field_arg(C).
field_arg_list(A) ::= field_arg_list(B) COMMA field_arg(C).
```

`FIELD` remains usable as an unquoted identifier where the parser admits
identifiers; it is not a whitespace-sensitive function name in this slice.

These snippets describe MyLite's supported subset, not MySQL's full grammar.

## Runtime Semantics

Planning:

1. Detect row-scalar projection attempts when a supported select item contains
   a top-level or parenthesized `FIELD()` call.
2. Resolve the optional source table through the existing selected/default
   schema policy.
3. Resolve descriptor column arguments through MyLite catalog descriptors, not
   SQLite schema text.
4. Convert admitted literal arguments to `planned_value` parameters before
   generating SQLite SQL.
5. Classify the non-`NULL` argument domain as all string or all integer. Reject
   mixed domains deterministically.
6. Generate a SQLite `CASE` expression over the planned argument list, using
   quoted identifiers and numbered bound parameters.

Generated SQL shape:

```sql
CASE
  WHEN search_expr IS NULL THEN 0
  WHEN compare(search_expr, value1_expr) THEN 1
  WHEN compare(search_expr, value2_expr) THEN 2
  ...
  ELSE 0
END
```

For string-domain `FIELD()`, each comparison applies MyLite's registered
`utf8mb4_0900_ai_ci` ASCII collation to string operands. For integer-domain
`FIELD()`, each comparison uses ordinary integer equality. `NULL` candidate
values naturally do not match; the explicit first branch preserves MySQL's
`FIELD(NULL, ...) = 0` behavior.

Execution remains SQLite-backed. MyLite does not materialize source rows to
evaluate `FIELD()` in C; it resolves descriptors, builds the projection
expression, binds literal parameters, and lets SQLite scan/filter/order/limit
the selected rows.

## Ownership Boundaries

- Public API: unchanged. Successful statements use the existing
  `mylite_execute()` and result APIs.
- Statement context: unchanged. Successful `SELECT` and `DO` preserve existing
  row-count and warning-count conventions.
- Lexer/parser/AST: add a `FIELD` token and `FIELD()` AST node. Parser source
  spans remain authoritative for default result labels.
- Analyzer/planner: extend the row-scalar planner to resolve `FIELD()`
  arguments, classify domains, reject unsupported shapes, and build generated
  SQL.
- Catalog: read-only descriptor authority. No descriptor rows, descriptor
  versions, descriptor caches, catalog generation, or
  `sqlite_schema_generation` are mutated.
- Result builder: returns integer text through existing scalar/row result
  conventions. Explicit aliases override default source-span labels.
- Storage/VFS/file format: unchanged. `.mylite` preamble and shifted SQLite
  payload invariants are preserved.
- SQLite: use generated standard SQLite `CASE` and registered MyLite collation.
  No SQLite fork patch is required.

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
  `FIELD() supports only string, integer, boolean, and NULL arguments`;
- unsupported mixed domain:
  `FIELD() does not support mixed string and numeric arguments`;
- unsupported non-ASCII or embedded-`NUL` string values with deterministic
  MyLite-specific diagnostics;
- out-of-range integer literals through the existing signed-64 row-scalar
  diagnostic;
- allocation failures through existing `MYLITE_NOMEM` behavior;
- physical SQLite failures through existing runtime diagnostics;
- public API misuse: no public API changes.

## Tests

Add MySQL-runtime expectation coverage for:

- core string, integer, boolean, no-match, first-match, and `NULL` behavior;
- default ASCII case-insensitive string matching;
- default-mode whitespace between `FIELD` and `(`;
- no-source `SELECT`, `FROM DUAL`, aliases, `DO`, `ROW_COUNT()`, and warning
  count;
- table-backed `FIELD(option_name, ...)` over `VARCHAR`, `CHAR`, `TEXT`, and
  integer descriptor columns with `WHERE`, `ORDER BY`, and `LIMIT`;
- argument-count errors for zero and one argument;
- MySQL-accepted but deferred mixed-domain and binary-domain forms.

Add fast C tests under `packages/libmylite/tests/`, preferably
`runtime_field_function`, plus parser coverage in `parser_test.c`.

## Compatibility Updates

Update:

- `COMPATIBILITY.md`
- `docs/compatibility/functions-string.md`
- `docs/compatibility/sql-query-expressions.md`
- `docs/compatibility/type-system-literals-conversion.md`

Use limited wording. Do not claim full `FIELD()`, general expression
evaluation, full collation semantics, mixed coercion domains, binary strings,
predicates, DML assignments, or ordering/grouping expressions.
