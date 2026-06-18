# Baseline ADDTIME and SUBTIME Functions

## Goal

Add a narrow scalar temporal arithmetic slice for:

```sql
ADDTIME(expr1, expr2)
SUBTIME(expr1, expr2)
```

This feature covers the common two-argument function shape for no-source
`SELECT`, `SELECT ... FROM DUAL`, `DO`, and the current single-table row-scalar
query/DML contexts. It is not a general temporal-expression implementation. It
deliberately limits operands to canonical string values, supported descriptor
columns, and `NULL`, reuses the existing MyLite-owned datetime-second
arithmetic for datetime results, and adds MyLite-owned canonical time arithmetic
for time results.

## Sources

- Official MySQL 8.4 date and time function documentation:
  <https://dev.mysql.com/doc/refman/8.4/en/date-and-time-functions.html>
- Official MySQL 8.4 built-in function catalog:
  <https://dev.mysql.com/doc/refman/8.4/en/built-in-function-reference.html>
- Observed MySQL 8.4.9 runtime behavior, captured by
  `packages/libmylite/tests/mysql_baseline_addtime_subtime_functions_expectations.sh`.

The MyLite grammar, specification, and implementation are independently
authored from official documentation and observed MySQL 8.4.9 behavior. Do not
copy MySQL grammar or implementation sources.

## MySQL 8.4.9 Runtime Observations

Runtime probes against the local MySQL 8.4.9 runtime establish these
expectations for this slice:

- `ADDTIME('2008-01-02 13:29:17','00:00:01')` returns
  `2008-01-02 13:29:18`.
- `SUBTIME('2008-01-02 13:29:17','00:00:01')` returns
  `2008-01-02 13:29:16`.
- Negative second operands invert the operation direction, so
  `ADDTIME(datetime,'-00:00:01')` subtracts one second and
  `SUBTIME(datetime,'-00:00:01')` adds one second.
- Time inputs return time strings. Examples:
  `ADDTIME('01:02:03','00:00:04')` returns `01:02:07`,
  `SUBTIME('01:02:03','00:00:04')` returns `01:01:59`,
  `ADDTIME('-01:02:03','00:00:04')` returns `-01:01:59`, and
  `SUBTIME('-01:02:03','00:00:04')` returns `-01:02:07`.
- `NULL` as the first argument returns `NULL` without evaluating invalid second
  string content; a non-`NULL` first argument with a `NULL` second argument also
  returns `NULL`.
- `ADDTIME` and `SUBTIME` are ordinary nonreserved function names for the
  tested parser contexts: whitespace before `(` is accepted in default SQL
  mode, and unquoted `addtime` / `subtime` table names are accepted even under
  `IGNORE_SPACE`.
- With `ANSI_QUOTES`, double-quoted operands are parsed as identifiers and
  produce the usual unknown-column diagnostic in no-source scalar `SELECT`.
- MySQL accepts broader operands such as numeric time values, relaxed time
  strings, day-hour time strings, fractional seconds, and warning-producing
  invalid temporal strings. Those are deferred by this baseline slice.
- MySQL clamps out-of-range `TIME` results to the endpoint with warning 1292
  and has datetime edge behavior outside MyLite's current nonzero temporal
  storage range. This slice supports only in-range results and rejects
  out-of-range results deterministically.

## Supported Surface

MyLite supports this exact subset:

- no-source `SELECT`, `SELECT ... FROM DUAL`, and `DO` scalar expressions;
- single-table row-scalar projection, equality predicate, and `ORDER BY`
  expression keys;
- non-key single-table `UPDATE` assignment targets where the target can store
  the resulting canonical temporal text;
- `ADDTIME(expr1, expr2)` and `SUBTIME(expr1, expr2)` with exactly two
  arguments;
- ordinary expression parentheses around operands;
- function-name whitespace before `(` in default SQL mode and under
  `IGNORE_SPACE`;
- `expr1` limited to:
  - a single- or double-quoted canonical datetime string
    `YYYY-MM-DD HH:MM:SS`;
  - a single- or double-quoted canonical time string `HH:MM:SS` or
    `HHH:MM:SS`;
  - the matching negative time forms `-HH:MM:SS` and `-HHH:MM:SS`;
  - `NULL`;
- datetime first arguments must be complete, valid Gregorian values in the
  current MyLite nonzero temporal arithmetic range
  `1000-01-01 00:00:00` through `9999-12-31 23:59:59`;
- time operands must be canonical values in MyLite's current `TIME` range
  `-838:59:59` through `838:59:59`;
- `expr2` limited to the same canonical time string and `NULL` subset, plus
  supported row-backed `TIME` and nonbinary string-family descriptor columns;
- row-backed `expr1` may reference supported `DATETIME`, `TIMESTAMP`, `TIME`,
  and nonbinary string-family descriptor columns;
- successful datetime results return `YYYY-MM-DD HH:MM:SS`;
- successful time results return `[-]HH:MM:SS` or `[-]HHH:MM:SS`, with at least
  two hour digits and no negative zero;
- successful supported expressions produce no warnings and do not mutate
  catalog state, descriptors, physical tables, or the `.mylite` preamble.

`ADDTIME` and `SUBTIME` remain available as unquoted identifiers where the
current identifier grammar admits nonreserved function names, including tested
table-name positions. They are not `IGNORE_SPACE`-reserved function names in
the current slice.

## Deferred Surface

This slice intentionally does not support:

- grouping, aggregate expressions, defaults, generated columns, check
  constraints, or arbitrary expression evaluation with these functions;
- DML assignments outside the current non-key single-table row-scalar subset;
- numeric time operands, compact temporal numeric forms, decimal/fractional
  seconds, day-hour strings such as `1 02:03:04`, relaxed time strings, plus
  signs on time strings, date-only strings, zero or partial-zero temporal
  values, time-zone effects, or warning-returning invalid temporal conversion;
- result clamping for out-of-range `TIME` values;
- datetime results below year 1000 or above year 9999;
- parameters, user variables, subqueries, nested function calls, or arithmetic
  expressions as operands;
- protocol-grade temporal result metadata. The public result currently exposes
  text/`NULL` scalar values.

## Grammar

The parser admits the independently authored two-argument function forms:

```lemon
expression ::= ADDTIME LPAREN expression COMMA expression RPAREN.
expression ::= SUBTIME LPAREN expression COMMA expression RPAREN.
```

Wrong-arity calls are represented with explicit AST error nodes so execution can
return MySQL-compatible native-function parameter-count diagnostics:

```lemon
expression ::= ADDTIME LPAREN RPAREN.
expression ::= ADDTIME LPAREN expression RPAREN.
expression ::= ADDTIME LPAREN expression COMMA expression COMMA function_argument_list RPAREN.

expression ::= SUBTIME LPAREN RPAREN.
expression ::= SUBTIME LPAREN expression RPAREN.
expression ::= SUBTIME LPAREN expression COMMA expression COMMA function_argument_list RPAREN.
```

The runtime, not the grammar, enforces operand literal kinds, canonical temporal
shapes, and result ranges.

Identifier grammar admits both names as ordinary identifiers:

```lemon
identifier ::= ADDTIME.
identifier ::= SUBTIME.
```

## Runtime Semantics

### Evaluation

1. Evaluate the first argument with this feature's MyLite-owned literal rules.
   `NULL` short-circuits to `NULL`.
2. Decode string literals through the current statement SQL mode. This reuses
   the same literal-decoding path as existing scalar functions, including
   `ANSI_QUOTES` and `NO_BACKSLASH_ESCAPES` behavior.
3. Classify the first non-`NULL` string as either a canonical datetime or a
   canonical time. Unsupported strings fail with a deterministic MyLite
   diagnostic.
4. Evaluate the second argument. `NULL` returns `NULL`; otherwise it must be a
   canonical time string in the supported range.
5. For `ADDTIME`, add the second operand. For `SUBTIME`, subtract the second
   operand. Negative second operands naturally reverse the direction.
6. Datetime first arguments use the existing MyLite-owned whole-second
   Gregorian datetime arithmetic. Time first arguments use MyLite-owned signed
   second arithmetic over the current `TIME` range.
7. Return text or `NULL` through the existing scalar result path.

### Diagnostics

Supported in-range expressions return successfully with `warning_count == 0`.
Unsupported or out-of-scope forms fail deterministically through existing
MyLite diagnostics unless the parser rejects them earlier.

Required diagnostics:

- syntax errors: existing parser syntax diagnostics;
- wrong arity: native-function parameter-count diagnostics for `ADDTIME` or
  `SUBTIME`;
- unknown identifiers in no-source operands: existing unknown-column
  diagnostics;
- first argument is not a string literal or `NULL`:
  `FUNCTION() supports only canonical datetime string literals, canonical time string literals, and NULL`;
- second argument is not a string literal or `NULL`:
  `FUNCTION() time argument supports only canonical time string literals and NULL`;
- string operand contains embedded NUL:
  `FUNCTION() time literals do not support NUL bytes`;
- first string cannot be classified as a supported datetime or time:
  `FUNCTION() supports only canonical YYYY-MM-DD HH:MM:SS datetime or canonical [-]HH:MM:SS time values`;
- second string is not a supported time:
  `FUNCTION() time argument supports only canonical [-]HH:MM:SS time values`;
- result outside the supported time or datetime range:
  `FUNCTION() result is outside the supported time or datetime range`;
- allocation failure: existing `MYLITE_NOMEM` diagnostic behavior;
- public API misuse: no public API changes.

## Architecture

- Public API: unchanged. Successful `SELECT` and `DO` use existing
  `mylite_execute()` and result APIs.
- Statement context: unchanged except that parser/lexer SQL-mode flags
  determine string literal interpretation.
- Lexer/parser/AST: add nonreserved function tokens, two function AST node
  kinds, and two argument-count error node kinds. Do not introduce general
  function-call parsing.
- Analyzer/planner/runtime: scalar contexts evaluate directly through the
  MyLite scalar evaluator, while row-backed contexts resolve descriptor columns
  and lower to registered private SQLite scalar functions. The evaluator uses
  MyLite-owned string decoding, canonical temporal validation, signed time
  conversion, and existing datetime-second arithmetic.
- Catalog: untouched. These functions do not read or mutate descriptors,
  descriptor caches, catalog generation, or `sqlite_schema_generation`.
- Result builder: returns a single text/`NULL` scalar cell through existing
  scalar-result paths. Column labels come from the original expression span or
  explicit alias.
- Storage/VFS/file format: untouched. The functions are pure scalar evaluation
  and must not modify SQLite payload or the MyLite preamble.
- SQLite: row-backed contexts use private SQLite scalar functions registered by
  MyLite on each connection; no public SQLite fork patch is required for this
  slice.

## Tests

Add fast C coverage under `packages/libmylite/tests/`:

- parser accepts supported `ADDTIME()` and `SUBTIME()` calls, mixed-case names,
  aliases, `FROM DUAL`, `DO`, and ordinary whitespace before `(`;
- parser preserves `ADDTIME` and `SUBTIME` as ordinary identifiers in tested
  table-name positions, including under `IGNORE_SPACE`;
- parser produces explicit wrong-arity AST nodes;
- runtime covers datetime add/subtract, negative second operands, time
  add/subtract, negative time first arguments, three-digit-hour values, `NULL`
  short-circuiting, aliases, `FROM DUAL`, `DO`, independent handles, and
  `ROW_COUNT()` behavior;
- runtime covers row-backed projection, equality predicate, `ORDER BY`, and
  non-key single-table `UPDATE` assignment contexts over supported descriptor
  columns;
- current SQL mode behavior for double-quoted strings, `ANSI_QUOTES`, and
  `NO_BACKSLASH_ESCAPES`;
- deterministic diagnostics for unsupported first operands, unsupported second
  operands, invalid datetime/time strings, embedded NUL, result overflow, and
  unknown identifiers;
- file preamble and catalog generation remain unchanged across supported
  expression evaluation.

Add a MySQL 8.4.9 expectation artifact:

```sh
./packages/libmylite/tests/mysql_baseline_addtime_subtime_functions_expectations.sh
```

## Compatibility Updates

Update:

- `COMPATIBILITY.md`
- `docs/compatibility/functions-temporal.md`
- `docs/compatibility/sql-query-expressions.md`
- `docs/compatibility/type-system-literals-conversion.md`
