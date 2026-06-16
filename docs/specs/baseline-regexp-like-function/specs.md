# Baseline REGEXP_LIKE Function

## Goal

Add a narrow `REGEXP_LIKE()` slice that reuses MyLite's existing
descriptor-backed regular-expression predicate infrastructure:

```sql
SELECT REGEXP_LIKE('abc', '^a');
SELECT id FROM posts WHERE REGEXP_LIKE(option_name, '^rss_.+$');
```

This phase does not add a full ICU regular-expression engine or a general
expression executor. It admits only the current ASCII baseline regex syntax,
keeps table-backed execution inside generated SQLite SQL, and rejects unsupported
forms before arbitrary user SQL reaches SQLite.

## Sources And Evidence

- Official MySQL 8.4 Reference Manual, regular expressions:
  <https://dev.mysql.com/doc/refman/8.4/en/regexp.html>
- Existing regex predicate design:
  `docs/specs/baseline-regexp-rlike-predicates/specs.md`
- Existing row-scalar function and predicate designs:
  `docs/specs/baseline-row-scalar-expressions/specs.md`
  `docs/specs/baseline-find-in-set-function/specs.md`
  `docs/specs/baseline-json-valid-function/specs.md`
- Observed MySQL 8.4.9 runtime behavior, captured by
  `packages/libmylite/tests/mysql_baseline_regexp_like_function_expectations.sh`.

The MyLite grammar and implementation are independently authored from official
documentation, observed MySQL 8.4.9 behavior, public SQLite APIs, and existing
MyLite code. Do not copy MySQL, MariaDB, Percona, SQLite implementation
internals, or restrictively licensed sources.

## MySQL 8.4.9 Runtime Observations

Runtime probes establish the behavior used by this baseline:

- `REGEXP_LIKE(expr, pat)` returns `1`, `0`, or `NULL`.
- A `NULL` `expr` or `pat` returns `NULL` after non-`NULL`
  `match_type` and pattern syntax have been validated. For example,
  `REGEXP_LIKE(NULL, '[')` errors, while `REGEXP_LIKE('a', '[', NULL)`
  returns `NULL`.
- Under MySQL's default `utf8mb4_0900_ai_ci` collation, ASCII matching is
  case-insensitive by default.
- Optional lowercase `match_type` characters `c` and `i` control case
  sensitivity, and the rightmost contradictory character wins. For example,
  `ci` is case-insensitive and `ic` is case-sensitive.
- Uppercase `C` and `I` are invalid `match_type` characters.
- A `NULL` `match_type` returns `NULL`.
- Wrong argument counts fail with `1582 / 42000`.
- Unsupported or invalid `match_type` values fail with `1210 / HY000`.
- Invalid regular expressions fail with MySQL regex diagnostics such as
  `3696 / HY000` for malformed bracket expressions and `3697 / HY000` for
  invalid character ranges.
- Without `match_type` `n`, `.` does not match line terminators. This baseline
  admits the default single-line behavior only and defers `n`.
- Binary string operands are rejected by MySQL's regular-expression functions;
  this baseline rejects them before execution.
- Successful supported calls produce `@@warning_count = 0`; tableless `SELECT`
  sets `ROW_COUNT()` to `-1`, while `DO REGEXP_LIKE(...)` sets it to `0`.

MySQL accepts additional `match_type` characters (`m`, `n`, and `u`) through its
ICU engine. They are deferred here because MyLite's current baseline regex
engine has only default single-line ASCII byte semantics.

## Supported Surface

MyLite supports:

- no-source `SELECT`, `SELECT ... FROM DUAL`, and `DO`;
- single-table row-scalar `SELECT` projection using the existing row envelope:
  `FROM table [AS alias]`, optional existing `WHERE`, descriptor-column
  `ORDER BY`, and existing `LIMIT`;
- single-table descriptor predicates in `SELECT`, `UPDATE`, and `DELETE`
  where the predicate leaf is one of:
  - `REGEXP_LIKE(value, pattern)`;
  - `REGEXP_LIKE(value, pattern, match_type)`;
  - `REGEXP_LIKE(...) = integer_or_boolean_literal`;
  - `REGEXP_LIKE(...) <=> integer_or_boolean_literal`;
  - `REGEXP_LIKE(...) <> integer_or_boolean_literal`;
  - `REGEXP_LIKE(...) != integer_or_boolean_literal`;
  - `REGEXP_LIKE(...) > integer_or_boolean_literal`;
  - `REGEXP_LIKE(...) >= integer_or_boolean_literal`;
  - `REGEXP_LIKE(...) < integer_or_boolean_literal`;
  - `REGEXP_LIKE(...) <= integer_or_boolean_literal`;
  - `REGEXP_LIKE(...) IS NULL`;
  - `REGEXP_LIKE(...) IS NOT NULL`;
- two- and three-argument `REGEXP_LIKE()` only;
- nested argument support is limited to the supported row-scalar value subset;
- value argument forms:
  - string literals;
  - signed 64-bit decimal integer literals with optional unary sign;
  - `TRUE` and `FALSE` as `1` and `0`;
  - `NULL`;
  - currently supported session scalar values and system variables where the
    existing row-scalar text argument path admits them;
  - table-backed descriptor columns whose logical type is `CHAR`, `VARCHAR`,
    or baseline `TEXT` family;
- pattern argument forms:
  - ordinary string literals;
  - signed 64-bit decimal integer literals with optional unary sign;
  - `TRUE` and `FALSE` as `1` and `0`;
  - `NULL`;
- optional match type forms:
  - ordinary string literals containing only lowercase `c` and/or lowercase
    `i`, including the empty string;
  - `NULL`;
- fixed ASCII case-insensitive default matching and fixed ASCII case-sensitive
  matching when rightmost `c` is the effective case flag;
- result values as integer text or SQL `NULL` through existing result APIs;
- warning count `0` for supported in-range forms.

String and pattern values must decode to ordinary ASCII text without embedded
`NUL`. Pattern syntax is exactly the existing baseline regex subset documented
by `baseline-regexp-rlike-predicates`.

## Deferred Surface

This slice intentionally does not support:

- full Unicode ICU regular expressions, Unicode character classes, Unicode
  case folding, accent-insensitive regex matching, or full collation coercion;
- MySQL `match_type` flags `m`, `n`, or `u`;
- binary string operands, explicit `BINARY`, binary casts/converts, introducers,
  explicit `COLLATE`, or connection collation changes;
- pattern values from columns, functions, parameters, subqueries, or arbitrary
  expressions;
- table-backed descriptor value columns outside nonbinary string families;
- nested `REGEXP_LIKE()`, `REGEXP_LIKE(CONCAT(...), ...)`, arithmetic,
  aggregate, window, CTE, joined-table expression arguments, grouping
  expressions, or expression `ORDER BY`;
- use as a DML assignment value, default expression, generated column
  expression, index expression, constraint expression, or arbitrary SQLite
  pass-through.

## Grammar

MyLite adds a reusable function expression production:

```lemon
regexp_like_expr(A) ::= REGEXP_LIKE(T) LPAREN expression(B) COMMA expression(C) RPAREN(R).
regexp_like_expr(A) ::= REGEXP_LIKE(T) LPAREN expression(B) COMMA expression(C)
                        COMMA expression(D) RPAREN(R).

expression(A) ::= regexp_like_expr(B).
```

Wrong-arity projection forms produce native-function argument-count AST nodes:

```lemon
expression(A) ::= REGEXP_LIKE(T) LPAREN RPAREN(R).
expression(A) ::= REGEXP_LIKE(T) LPAREN expression(B) RPAREN(R).
expression(A) ::= REGEXP_LIKE(T) LPAREN expression(B) COMMA expression(C)
                  COMMA expression(D) COMMA function_argument_list(E) RPAREN(R).
```

The predicate grammar admits only the scoped forms:

```lemon
predicate_atom(A) ::= regexp_like_expr(B).
predicate_atom(A) ::= regexp_like_expr(B) EQUAL(O) predicate_integer_value(C).
predicate_atom(A) ::= regexp_like_expr(B) NULL_SAFE_EQUAL(O) predicate_integer_value(C).
predicate_atom(A) ::= regexp_like_expr(B) NOT_EQUAL(O) predicate_integer_value(C).
predicate_atom(A) ::= regexp_like_expr(B) LESS(O) predicate_integer_value(C).
predicate_atom(A) ::= regexp_like_expr(B) LESS_EQUAL(O) predicate_integer_value(C).
predicate_atom(A) ::= regexp_like_expr(B) GREATER(O) predicate_integer_value(C).
predicate_atom(A) ::= regexp_like_expr(B) GREATER_EQUAL(O) predicate_integer_value(C).
predicate_atom(A) ::= regexp_like_expr(B) IS(I) NULL(N).
predicate_atom(A) ::= regexp_like_expr(B) IS(I) NOT NULL(N).
```

These snippets describe MyLite's admitted subset, not MySQL's full grammar.

## Runtime Semantics

No-source, `DUAL`, and `DO` evaluation is MyLite-owned:

1. Unwrap supported parentheses.
2. Verify exactly two or three arguments.
3. Convert `expr` and `pat`:
   - ordinary string literal: decoded ASCII bytes;
   - integer literal: canonical signed decimal text;
   - `TRUE` / `FALSE`: `1` / `0`;
   - `NULL`: SQL `NULL`;
   - supported session scalar/system variable for the value argument only: its
     existing visible text or SQL `NULL`.
4. Convert optional `match_type` from an ASCII string literal or `NULL`.
5. A `NULL` `match_type` returns SQL `NULL` before pattern validation.
6. A non-`NULL` pattern is validated even when the value is `NULL`.
7. Return SQL `NULL` if `expr` or `pat` is SQL `NULL` after those validations.
8. Reject non-ASCII, embedded `NUL`, unsupported match-type flags, and
   unsupported or invalid baseline regex syntax.
9. Compile the pattern with the effective case flag and return `1` for a match
   or `0` for no match.

Table-backed projection and predicate execution stays SQLite-backed. MyLite
resolves descriptors, builds generated SQLite SQL over stable physical table
names, binds literal/session arguments, and lets SQLite scan/filter/order/limit
rows. The generated helpers use pattern-first argument order for SQLite auxdata
caching:

```sql
_mylite_regexp_ci_ascii(<pattern>, <value>)
_mylite_regexp_cs_ascii(<pattern>, <value>)
```

The helpers validate table-backed runtime values as ASCII text without embedded
`NUL` before matching, so row values follow the same boundary as scalar
literals. Predicate truth form lowers to the helper expression itself. SQLite
and MySQL both treat `0` as false and `NULL` as unknown in `WHERE`, which gives
the verified MySQL behavior for this admitted subset. Comparison and `IS NULL`
forms lower to ordinary SQLite comparison syntax around the generated helper
expression with bound integer comparison values.

## Ownership Boundaries

- Public API: unchanged. Successful statements use existing `mylite_execute()`
  and result APIs.
- Statement context: owns diagnostics, warning count, affected-row state, and
  result finalization. Supported calls add no warnings.
- Lexer/parser/AST: add `REGEXP_LIKE` token, function AST node,
  argument-count error node, and narrow predicate grammar. Source spans remain
  authoritative for default result labels.
- Analyzer/planner: resolves descriptor columns from MyLite catalog
  descriptors, validates supported argument shapes, rejects unsupported forms
  before SQLite SQL is generated, and preserves existing single-source
  predicate rules.
- Catalog: read-only descriptor authority. No descriptor rows, descriptor
  versions, descriptor caches, catalog generation, or `sqlite_schema_generation`
  are mutated.
- Result builder: returns integer text or SQL `NULL` through existing scalar
  and row result conventions. Explicit aliases override default source-span
  labels.
- Storage/VFS/file format: unchanged. `.mylite` preamble and shifted SQLite
  payload invariants are preserved.
- SQLite: use a public scalar-function registration for the MyLite helper. No
  SQLite fork patch is required.

## Diagnostics

- Wrong argument counts: native MySQL-style `1582 / 42000`.
- Unsupported value, pattern, or match-type operands: deterministic
  MyLite-specific unsupported-feature errors.
- Unknown table-backed value column: existing unknown-column diagnostics.
- Unsupported table-backed value column kind: deterministic
  MyLite-specific unsupported-feature errors.
- Non-ASCII or embedded `NUL` text/pattern values: deterministic
  MyLite-specific unsupported-feature errors.
- Unsupported match-type characters: deterministic
  MyLite-specific unsupported-feature error.
- Invalid regex syntax in the admitted subset: MySQL-compatible regex error
  numbers/messages where the existing regex engine can identify them.
- Allocation failures: existing allocation diagnostics.
- Physical SQLite failures: existing physical row error handling.

## Compatibility Documentation

Update:

- `COMPATIBILITY.md`;
- `docs/compatibility/functions-string.md`;
- `docs/compatibility/sql-query-expressions.md`;
- `docs/compatibility/sql-table-dml.md`.

The docs must use partial/limited wording and must not claim full ICU regex,
full collation, arbitrary expression arguments, binary strings, or general
expression support.
