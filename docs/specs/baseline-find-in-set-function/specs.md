# Baseline FIND_IN_SET Function

## Goal

Add a narrow MySQL-compatible `FIND_IN_SET()` slice for common comma-list SQL:

```sql
SELECT FIND_IN_SET('green', tags) FROM posts;
SELECT id FROM posts WHERE FIND_IN_SET('green', tags);
```

This phase extends the current scalar, row-scalar projection, and descriptor
predicate paths. It is not a general expression engine, full collation layer,
or full `SET` optimization implementation.

## Sources And Evidence

- Official MySQL 8.4 Reference Manual, string functions and operators:
  <https://dev.mysql.com/doc/refman/8.4/en/string-functions.html>
- Existing row-scalar expression design:
  `docs/specs/baseline-row-scalar-expressions/specs.md`
- Existing string-search function design:
  `docs/specs/baseline-string-search-functions/specs.md`
- Observed MySQL 8.4.9 runtime behavior, captured by
  `packages/libmylite/tests/mysql_baseline_find_in_set_function_expectations.sh`.

The MyLite grammar and implementation are independently authored from official
documentation, observed MySQL 8.4.9 behavior, public SQLite APIs, and existing
MyLite code. Do not copy MySQL, MariaDB, Percona, SQLite implementation
internals, or restrictively licensed sources.

## MySQL 8.4.9 Runtime Observations

Runtime probes establish the behavior used by this baseline:

- `FIND_IN_SET(str, strlist)` returns the 1-based position of `str` in the
  comma-separated `strlist`.
- A missing value returns `0`.
- An empty `strlist` returns `0`.
- Either `NULL` argument returns `NULL`.
- Empty members are real list positions. For example, `FIND_IN_SET('', 'a,')`
  returns `2`, `FIND_IN_SET('', ',a')` returns `1`, and
  `FIND_IN_SET('', 'a,,b')` returns `2`.
- The first duplicate member wins.
- Under MySQL's default `utf8mb4_0900_ai_ci`, ASCII case differences do not
  prevent a match. Spaces inside list members remain significant.
- If the first argument contains a comma, observed MySQL 8.4.9 behavior for
  this baseline returns `0`.
- Numeric and boolean scalar arguments are converted to visible string form.
- Successful supported calls produce `@@warning_count = 0`; a preceding
  scalar `SELECT` makes `ROW_COUNT()` return `-1`, while `DO FIND_IN_SET(...)`
  makes `ROW_COUNT()` return `0`.
- Wrong argument counts fail with `1582 / 42000`.
- `WHERE FIND_IN_SET('x', col)` treats positive positions as true, `0` as
  false, and `NULL` as unknown/filtered out. Comparisons such as
  `FIND_IN_SET('x', col) > 0`, `= 0`, and `<> 0` follow normal SQL
  three-valued logic.

## Supported Surface

MyLite supports:

- no-source `SELECT`, `SELECT ... FROM DUAL`, and `DO`;
- single-table row-scalar `SELECT` projection using the existing row envelope:
  `FROM table [AS alias]`, optional existing `WHERE`, descriptor-column
  `ORDER BY`, and existing `LIMIT`;
- single-table descriptor predicates in `SELECT`, `UPDATE`, and `DELETE`
  where the predicate leaf is one of:
  - `FIND_IN_SET(value, list)`;
  - `FIND_IN_SET(value, list) = integer_or_boolean_literal`;
  - `FIND_IN_SET(value, list) <> integer_or_boolean_literal`;
  - `FIND_IN_SET(value, list) != integer_or_boolean_literal`;
  - `FIND_IN_SET(value, list) > integer_or_boolean_literal`;
  - `FIND_IN_SET(value, list) >= integer_or_boolean_literal`;
  - `FIND_IN_SET(value, list) < integer_or_boolean_literal`;
  - `FIND_IN_SET(value, list) <= integer_or_boolean_literal`;
  - `FIND_IN_SET(value, list) IS NULL`;
  - `FIND_IN_SET(value, list) IS NOT NULL`;
- two-argument `FIND_IN_SET(value, list)` only;
- flat arguments only; no nested row functions inside `FIND_IN_SET()`;
- scalar argument values:
  - string literals;
  - signed 64-bit decimal integer literals with optional unary sign;
  - `TRUE` and `FALSE` as `1` and `0`;
  - `NULL`;
  - currently supported session scalar values and system variables where the
    existing string-search argument path admits them;
- table-backed descriptor argument values:
  - integer-family columns;
  - exact `DECIMAL`;
  - `YEAR`, `DATE`, `TIME`, `DATETIME`, and `TIMESTAMP`;
  - `CHAR`, `VARCHAR`, and baseline `TEXT` family columns;
  - limited `ENUM` columns as their descriptor-owned canonical text;
- fixed ASCII case-insensitive matching for admitted nonbinary text values,
  matching MyLite's current `utf8mb4_0900_ai_ci` approximation;
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
- binary-string case-sensitive matching, explicit `BINARY`, `CAST()`,
  `COLLATE`, introducers, connection collation changes, or binary result
  typing;
- `SET` column arguments. MySQL returns definition-member ordinals for `SET`
  columns, not plain comma-text positions, so this requires a later
  descriptor-aware `SET` path rather than the generic comma-list helper;
- approximate numeric scalar arguments, arbitrary numeric string conversion,
  noninteger rounding, warnings from conversion, blob/spatial/JSON arguments,
  parameters, user variables, stored functions, or scalar subqueries inside
  `FIND_IN_SET()`;
- nested `FIND_IN_SET()`, `CONCAT(FIND_IN_SET(...))`,
  `FIND_IN_SET(CONCAT(...), col)`, arithmetic, aggregate, window, CTE, or
  joined-table expression arguments;
- expression `ORDER BY`, grouping expressions, aggregate arguments, generated
  columns, defaults, indexes, constraints, or arbitrary SQLite pass-through.

## Grammar

MyLite adds a reusable function expression production:

```lemon
find_in_set_expr(A) ::= FIND_IN_SET(T) LPAREN expression(B) COMMA expression(C) RPAREN(R).

expression(A) ::= find_in_set_expr(B).
```

Wrong-arity projection forms produce native-function argument-count AST nodes:

```lemon
expression(A) ::= FIND_IN_SET(T) LPAREN RPAREN(R).
expression(A) ::= FIND_IN_SET(T) LPAREN expression(B) RPAREN(R).
expression(A) ::= FIND_IN_SET(T) LPAREN expression(B) COMMA expression(C)
                  COMMA function_argument_list(D) RPAREN(R).
```

The predicate grammar admits only the scoped forms:

```lemon
predicate_atom(A) ::= find_in_set_expr(B).
predicate_atom(A) ::= find_in_set_expr(B) EQUAL(O) predicate_integer_value(C).
predicate_atom(A) ::= find_in_set_expr(B) NOT_EQUAL(O) predicate_integer_value(C).
predicate_atom(A) ::= find_in_set_expr(B) LESS(O) predicate_integer_value(C).
predicate_atom(A) ::= find_in_set_expr(B) LESS_EQUAL(O) predicate_integer_value(C).
predicate_atom(A) ::= find_in_set_expr(B) GREATER(O) predicate_integer_value(C).
predicate_atom(A) ::= find_in_set_expr(B) GREATER_EQUAL(O) predicate_integer_value(C).
predicate_atom(A) ::= find_in_set_expr(B) IS(I) NULL(N).
predicate_atom(A) ::= find_in_set_expr(B) IS(I) NOT NULL(N).
```

These snippets describe MyLite's admitted subset, not MySQL's full grammar.

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
5. Return `0` if the list is empty or if the search string contains a comma.
6. Split the list on literal comma bytes. Empty members count as positions.
7. Compare members with ASCII case-insensitive equality and return the first
   1-based position, or `0` when none matches.

Table-backed projection and predicate execution stays SQLite-backed. MyLite
resolves descriptors, builds generated SQLite SQL over stable physical table
names, binds literal/session arguments, and lets SQLite scan/filter/order/limit
rows. The generated function call uses a registered MyLite scalar helper:

```sql
_mylite_find_in_set_ascii_ci(<search>, <list>)
```

Predicate truth form lowers to the helper expression itself. SQLite and MySQL
both treat `0` as false and `NULL` as unknown in `WHERE`, which gives the
verified MySQL behavior for this admitted subset. Comparison and `IS NULL`
forms lower to ordinary SQLite comparison syntax around the generated helper
expression with bound integer comparison values.

## Ownership Boundaries

- Public API: unchanged. Successful statements use existing `mylite_execute()`
  and result APIs.
- Statement context: owns diagnostics, warning count, affected-row state, and
  result finalization. Supported calls add no warnings.
- Lexer/parser/AST: add `FIND_IN_SET` token, function AST node, argument-count
  error node, and narrow predicate grammar. Source spans remain authoritative
  for default result labels.
- Analyzer/planner: resolves descriptor columns from MyLite catalog
  descriptors, validates supported argument shapes, rejects unsupported
  predicate forms before SQLite SQL is generated, and preserves existing
  single-source predicate rules.
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
  unsupported object kinds through existing row-scalar or predicate source
  diagnostics;
- unknown descriptor column arguments through MySQL-compatible unknown-column
  diagnostics in field-list context for projection and where-clause context
  for predicates;
- unsupported argument shapes:
  `FIND_IN_SET() supports only string, integer, boolean, NULL, session scalar, system variable, and descriptor column arguments`;
- unsupported predicate comparison values:
  `FIND_IN_SET() predicates support only integer and boolean comparison literals`;
- unsupported non-ASCII or embedded-`NUL` text values:
  `FIND_IN_SET() supports only ASCII text values`;
- unsupported descriptor column families:
  `FIND_IN_SET() supports only integer, DECIMAL, nonbinary string, ENUM, YEAR, and temporal columns`;
- `SET` descriptor column arguments:
  `FIND_IN_SET() does not support SET columns`;
- physical SQLite failures through existing runtime diagnostics;
- allocation failures through existing `MYLITE_NOMEM` diagnostics.

## Performance And Storage

The projection path does not materialize source rows in MyLite. SQLite scans the
physical table and invokes a small scalar helper per evaluated row, which is
the same execution shape as the existing string-search functions. The helper is
linear in the second argument length. This baseline does not add indexes, a
`SET` member-ordinal path, catalog mutation, or file-format changes.

## Tests

Tests must cover:

- no-source, `DUAL`, labels, whitespace before `(`, and `DO`;
- found, missing, duplicate, empty-list, empty-member, comma-in-search, case,
  and space-sensitive behavior;
- `NULL` propagation;
- numeric and boolean scalar argument conversion;
- table-backed projection over `INT`, `DECIMAL`, `VARCHAR`, `TEXT`, `YEAR`,
  `DATE`, `TIME`, `DATETIME`, `TIMESTAMP`, and `ENUM` descriptors;
- deterministic rejection of `SET` descriptor arguments until the dedicated
  MySQL member-ordinal behavior is implemented;
- row envelope preservation with descriptor `WHERE`, `ORDER BY`, and `LIMIT`;
- predicate truth, comparison, `IS NULL`, `IS NOT NULL`, `UPDATE`, and
  `DELETE` behavior;
- unknown columns and unsupported nested/expression arguments;
- wrong arity `1582 / 42000`;
- non-ASCII and embedded-`NUL` rejections;
- reopen persistence and `.mylite` preamble preservation;
- zero-initialized cleanup through normal result/free paths;
- focused parser/runtime tests plus existing parser and runtime lifecycle
  regressions.
