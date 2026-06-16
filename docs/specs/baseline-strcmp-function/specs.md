# Baseline STRCMP Function

## Goal

Add a narrow MySQL-compatible `STRCMP()` slice for scalar and descriptor-backed
row projection:

```sql
SELECT STRCMP('text', 'text2');
SELECT id, STRCMP(name, 'admin') FROM users ORDER BY id;
```

This phase is not a general collation engine, full expression evaluator, or
binary-string comparison implementation. It extends the current row-scalar
string-function machinery with a two-argument comparison that returns
`-1`, `0`, `1`, or SQL `NULL`.

## Sources And Evidence

- Official MySQL 8.4 Reference Manual, string comparison functions and
  operators:
  <https://dev.mysql.com/doc/refman/8.4/en/string-comparison-functions.html>
- Existing string-search function design:
  `docs/specs/baseline-string-search-functions/specs.md`
- Existing `FIND_IN_SET()` design:
  `docs/specs/baseline-find-in-set-function/specs.md`
- Observed MySQL 8.4.9 runtime behavior, captured by
  `packages/libmylite/tests/mysql_baseline_strcmp_function_expectations.sh`.

The MyLite grammar and implementation are independently authored from official
documentation, observed MySQL 8.4.9 behavior, public SQLite APIs, and existing
MyLite code. Do not copy MySQL, MariaDB, Percona, SQLite implementation
internals, or restrictively licensed sources.

## MySQL 8.4.9 Runtime Observations

Runtime probes establish the behavior used by this baseline:

- `STRCMP(expr1, expr2)` returns `0` for equal strings, `-1` when the first
  argument sorts before the second, `1` when it sorts after the second, and
  `NULL` if either argument is `NULL`.
- Exactly two arguments are required. Zero-, one-, and three-argument calls
  fail with `1582 / 42000`.
- Under `utf8mb4_0900_ai_ci`, ASCII case differences do not affect the
  comparison, so `STRCMP('abc', 'ABC')` returns `0`.
- With the connection and database set to `utf8mb4_0900_ai_ci`, trailing spaces
  are significant for this function: `STRCMP('a ', 'a')` returns `1` and
  `STRCMP('a', 'a ')` returns `-1`.
- Numeric and boolean scalar arguments are converted to visible string form.
- Row-backed `CHAR`, `VARCHAR`, `TEXT`, integer, `DECIMAL`, `YEAR`, `DATE`,
  `DATETIME`, and `TIMESTAMP` values are converted to their visible value
  before comparison in the observed supported cases.
- Direct row-backed `TIME` column comparison has distinct MySQL coercion from
  `CAST(time_column AS CHAR)`, so this slice defers `TIME` descriptor support
  until that behavior is specified separately.
- Successful supported calls produce `@@warning_count = 0`; a preceding
  scalar `SELECT` makes `ROW_COUNT()` return `-1`, while
  `DO STRCMP(...)` makes `ROW_COUNT()` return `0`.
- MySQL accepts broader cases such as binary strings, non-ASCII collation
  comparisons, and approximate numeric conversion. Those are intentionally
  deferred here.

## Supported Surface

MyLite supports:

- no-source `SELECT`, `SELECT ... FROM DUAL`, and `DO`;
- single-table row-scalar `SELECT` projection using the existing row envelope:
  `FROM table [AS alias]`, optional existing `WHERE`, descriptor-column
  `ORDER BY`, and existing `LIMIT`;
- exactly two-argument `STRCMP(left, right)` calls;
- supported nested row-scalar value functions inside `STRCMP()` arguments,
  including the current string, integer-producing, conversion, JSON, numeric,
  control-flow, session-value, and system-variable subset;
- scalar argument values:
  - ASCII string literals without embedded `NUL`;
  - signed 64-bit decimal integer literals with optional unary sign;
  - `TRUE` and `FALSE` as `1` and `0`;
  - `NULL`;
  - currently supported session scalar values and system variables where the
    existing string-search argument path admits them;
- table-backed descriptor argument values:
  - integer-family columns;
  - exact `DECIMAL`;
  - `YEAR`, `DATE`, `DATETIME`, and `TIMESTAMP`;
  - `CHAR`, `VARCHAR`, and baseline `TEXT` family columns;
- fixed ASCII case-insensitive comparison for admitted nonbinary text values,
  matching MyLite's current `utf8mb4_0900_ai_ci` approximation;
- byte-length-sensitive comparison after ASCII case folding, including
  significant trailing spaces in the admitted scalar/string column cases;
- result values as integer text or SQL `NULL` through existing result APIs;
- warning count `0` for supported in-range forms.

String literal values must decode to ordinary ASCII text without embedded
`NUL`; MyLite claims MySQL collation parity only for ASCII values in this
slice. Integer literals must fit the signed 64-bit scalar envelope used by
current row-scalar expressions.

## Deferred Surface

This slice intentionally does not support:

- full Unicode collation weights, accent folding, contractions, expansions, or
  other full `utf8mb4_0900_ai_ci` behavior;
- binary-string case-sensitive comparison, explicit `BINARY`, `CAST()`,
  `COLLATE`, introducers, connection collation changes, or binary result
  metadata;
- approximate numeric scalar or row-backed arguments, arbitrary numeric string
  conversion, noninteger rounding, warnings from conversion, blob/spatial/JSON
  arguments outside the supported value subset, parameters, user variables, or
  scalar subqueries inside `STRCMP()`;
- row-backed `TIME` columns;
- nested functions outside the supported row-scalar value subset, arithmetic,
  aggregate, window, CTE, or joined-table expression arguments;
- predicates, expression `ORDER BY`, grouping expressions, aggregate
  arguments, DML assignments, defaults, generated columns, indexes,
  constraints, or arbitrary SQLite pass-through.

## Grammar

MyLite adds a two-argument function expression production:

```lemon
expression(A) ::= STRCMP(T) LPAREN expression(B) COMMA expression(C) RPAREN(R).
```

Wrong-arity projection forms produce native-function argument-count AST nodes:

```lemon
expression(A) ::= STRCMP(T) LPAREN RPAREN(R).
expression(A) ::= STRCMP(T) LPAREN expression(B) RPAREN(R).
expression(A) ::= STRCMP(T) LPAREN expression(B) COMMA expression(C)
                  COMMA function_argument_list(D) RPAREN(R).
```

Analyzer/runtime acceptance for this feature is narrower:

```lemon
strcmp_arg(A) ::= descriptor_string_convertible_column(B).
strcmp_arg(A) ::= string_literal(T).
strcmp_arg(A) ::= decimal_integer_literal(T).
strcmp_arg(A) ::= PLUS(P) decimal_integer_literal(T).
strcmp_arg(A) ::= MINUS(M) decimal_integer_literal(T).
strcmp_arg(A) ::= TRUE(T).
strcmp_arg(A) ::= FALSE(T).
strcmp_arg(A) ::= NULL(T).
strcmp_arg(A) ::= session_scalar_value(B).
strcmp_arg(A) ::= system_variable_value(B).
strcmp_arg(A) ::= LPAREN strcmp_arg(B) RPAREN(R).
```

`STRCMP` remains usable as an unquoted identifier where the parser admits
identifiers; it is not a whitespace-sensitive function name in this slice.
These snippets describe MyLite's supported subset, not MySQL's full grammar.

## Runtime Semantics

No-source, `DUAL`, and `DO` evaluation is MyLite-owned:

1. Unwrap supported parentheses.
2. Verify exactly two arguments.
3. Convert each admitted argument to text:
   - ordinary string literal: decoded ASCII bytes;
   - integer literal: canonical signed decimal text;
   - `TRUE` / `FALSE`: `1` / `0`;
   - `NULL`: SQL `NULL`;
   - supported session scalar/system variable: its existing visible text or
     SQL `NULL`.
4. Return SQL `NULL` if either argument is SQL `NULL`.
5. Compare byte-by-byte after ASCII case folding.
6. Return `-1`, `0`, or `1`; do not expose raw byte differences.

Table-backed projection execution stays SQLite-backed. MyLite resolves
descriptors, builds generated SQLite SQL over stable physical table names,
binds literal/session arguments, and lets SQLite scan/filter/order/limit rows.
The generated function call uses a registered MyLite scalar helper:

```sql
_mylite_strcmp_ascii_ci(<left>, <right>)
```

The helper returns SQL `NULL` when either input is SQL `NULL`, otherwise the
normalized comparison result. It rejects non-ASCII or embedded-`NUL` text before
returning a value. All generated SQLite identifiers are descriptor-owned and
quoted; user SQL text is never interpolated into generated physical SQL.

## Ownership Boundaries

- Public API: unchanged. Successful statements use existing `mylite_execute()`
  and result APIs.
- Statement context: owns diagnostics, warning count, affected-row state, and
  result finalization. Supported calls add no warnings.
- Lexer/parser/AST: add `STRCMP` token, function AST node, and argument-count
  error node. Source spans remain authoritative for default result labels.
- Analyzer/planner: resolves descriptor columns from MyLite catalog
  descriptors, validates supported argument shapes, rejects unsupported
  expression contexts before SQLite SQL is generated, and preserves existing
  single-source row projection rules.
- Catalog: read-only descriptor authority. No descriptor rows, descriptor
  versions, descriptor caches, catalog generation, or
  `sqlite_schema_generation` are mutated.
- Result builder: returns integer text or SQL `NULL` through existing scalar
  and row result conventions. Explicit aliases override default source-span
  labels.
- Storage/VFS/file format: unchanged. `.mylite` preamble and shifted SQLite
  payload invariants are preserved.
- SQLite: use a public scalar-function registration for the MyLite helper. No
  SQLite fork patch is required.

## Diagnostics

Required diagnostics:

- parser syntax errors through existing parse diagnostics;
- wrong argument count: MySQL-compatible `1582 / 42000` native-function
  argument-count error;
- missing default schema, unknown schema/table, reserved table names, and
  unsupported object kinds through existing row-scalar source diagnostics;
- unknown descriptor column arguments through MySQL-compatible unknown-column
  diagnostics in field-list context;
- unsupported scalar argument shapes:
  `STRCMP() supports only string, integer, boolean, NULL, session scalar, and system variable arguments`;
- unsupported descriptor argument shapes are reported with the current
  row-scalar argument diagnostics or a column-family-specific `STRCMP()`
  message;
- unsupported non-ASCII or embedded-`NUL` text values:
  `string search functions support only ASCII text values`;
- unsupported `TIME` descriptor columns:
  `STRCMP() does not support TIME columns`;
- unsupported descriptor column families:
  `STRCMP() supports only integer, DECIMAL, nonbinary string, YEAR, DATE, DATETIME, and TIMESTAMP columns`;
- physical SQLite failures through existing runtime diagnostics;
- allocation failures through existing `MYLITE_NOMEM` diagnostics.

## Performance And Storage

The projection path does not materialize source rows in MyLite. SQLite scans the
physical table and invokes a small scalar helper per evaluated row, the same
execution shape as existing string-search functions. The helper is linear in
the shorter difference prefix plus length comparison. This baseline does not
add indexes, catalog mutation, storage changes, or SQLite fork patches.

## Tests

Tests must cover:

- no-source, `DUAL`, labels, whitespace before `(`, and `DO`;
- less-than, greater-than, equality, empty strings, ASCII case folding, trailing
  space significance, integer/boolean conversion, and `NULL` propagation;
- row count, warning count, and no result rows for `DO`;
- zero-, one-, and three-argument native function count errors;
- table-backed descriptor arguments covering nonbinary strings, `TEXT`,
  integer, `DECIMAL`, `YEAR`, `DATE`, `DATETIME`, and `TIMESTAMP` columns,
  including nullable rows;
- deterministic rejection of `TIME` descriptor columns, with MySQL 8.4.9
  evidence for why this behavior is deferred;
- row envelope preservation with existing `WHERE`, descriptor `ORDER BY`, and
  `LIMIT`;
- unknown column diagnostics in row-scalar projection;
- deterministic rejection for binary columns, approximate columns, non-ASCII
  text, unsupported hex/binary literals, parameters, nested calls, predicate
  use, DML assignment use, and ordering expression use;
- reopen/file-format safety indirectly through a table-backed projection
  reopening the same file and checking the `.mylite` preamble;
- existing parser, runtime scalar, row-scalar, statement context, catalog,
  storage, and VFS tests still passing.

## Compatibility Documentation

Update:

- `COMPATIBILITY.md`;
- `docs/compatibility/functions-string.md`;
- `docs/compatibility/sql-query-expressions.md` if the row-scalar projection
  surface changes need an explicit mention;
- `docs/compatibility/type-system-literals-conversion.md` only if literal
  conversion wording needs to include `STRCMP()`.

Do not document broad `STRCMP()` collation, binary, predicate, DML assignment,
ordering-expression, grouping, generated-column, default-expression, or nested
expression support.
