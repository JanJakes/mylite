# Baseline TIMEDIFF Function

## Goal

Add a narrow scalar temporal subtraction slice for:

```sql
TIMEDIFF(expr1, expr2)
```

This feature covers no-source `SELECT`, `SELECT ... FROM DUAL`, `DO`, and
single-table row-scalar projection over MyLite-owned temporal descriptors. It
is not a general temporal-expression implementation.

## Sources

- Official MySQL 8.4 date and time function documentation:
  <https://dev.mysql.com/doc/refman/8.4/en/date-and-time-functions.html>
- Official MySQL 8.4 built-in function catalog:
  <https://dev.mysql.com/doc/refman/8.4/en/built-in-function-reference.html>
- Observed MySQL 8.4.9 runtime behavior, captured by
  `packages/libmylite/tests/mysql_baseline_timediff_function_expectations.sh`.

The MyLite grammar, specification, and implementation are independently
authored from official documentation and observed MySQL 8.4.9 behavior. Do not
copy MySQL grammar or implementation sources.

## MySQL 8.4.9 Runtime Observations

Runtime probes against the local MySQL 8.4.9 runtime establish these
expectations for this slice:

- `TIMEDIFF('2008-01-02 13:29:17','2008-01-02 13:29:16')` returns
  `00:00:01`.
- Reversing the operands returns `-00:00:01`.
- Time operands return signed time strings. Examples:
  `TIMEDIFF('01:02:03','00:00:04')` returns `01:01:59`,
  `TIMEDIFF('00:00:04','01:02:03')` returns `-01:01:59`, and
  `TIMEDIFF('-01:02:03','00:00:04')` returns `-01:02:07`.
- Date/datetime operands and time operands are separate domains. Canonical
  datetime-vs-time string operands return `NULL` without warnings.
- `TIME`, `DATE`, `DATETIME`, and `TIMESTAMP` columns are accepted by MySQL.
  `DATE`-vs-`DATE` differences are whole-day differences rendered as time,
  `DATETIME`-vs-`TIMESTAMP` is accepted, and mixed `DATE`-vs-`DATETIME` or
  `TIME`-vs-`DATETIME` columns return `NULL`.
- `NULL` as the first argument returns `NULL` without evaluating invalid second
  string content. Invalid first non-`NULL` string content returns `NULL` with
  warning 1292.
- Results outside MySQL's `TIME` display range clip to `838:59:59` or
  `-838:59:59` with warning 1292.
- `TIMEDIFF` is an ordinary nonreserved function name for the tested parser
  contexts: whitespace before `(` is accepted in default SQL mode and under
  `IGNORE_SPACE`, and unquoted `timediff` table names are accepted.
- With `ANSI_QUOTES`, double-quoted operands are parsed as identifiers and
  produce the usual unknown-column diagnostic in no-source scalar `SELECT`.
- MySQL accepts broader operands such as numeric time values, date-only string
  coercions, fractional seconds, and some relaxed temporal strings. Those are
  deferred by this baseline slice.

## Supported Surface

MyLite supports this exact subset:

- no-source `SELECT`, `SELECT ... FROM DUAL`, and `DO` scalar expressions;
- single-table row-scalar projection in supported descriptor-backed `SELECT`;
- `TIMEDIFF(expr1, expr2)` with exactly two arguments;
- ordinary expression parentheses around operands;
- function-name whitespace before `(` in default SQL mode and under
  `IGNORE_SPACE`;
- no-source/`DUAL`/`DO` operands limited to:
  - a single- or double-quoted canonical datetime string
    `YYYY-MM-DD HH:MM:SS`;
  - a single- or double-quoted canonical time string `HH:MM:SS` or
    `HHH:MM:SS`;
  - the matching negative time forms `-HH:MM:SS` and `-HHH:MM:SS`;
  - `NULL`;
- row-scalar operands limited to:
  - descriptor `DATE`, `TIME`, `DATETIME`, and `TIMESTAMP` columns;
  - the same supported string literals;
  - `NULL`;
- date/datetime operands must be complete, valid Gregorian values in the
  current MyLite nonzero temporal arithmetic range
  `1000-01-01 00:00:00` through `9999-12-31 23:59:59`;
- time operands must be canonical values in MyLite's current `TIME` range
  `-838:59:59` through `838:59:59`;
- `TIME`-vs-`TIME`, `DATE`-vs-`DATE`, and `DATETIME`/`TIMESTAMP`-vs-
  `DATETIME`/`TIMESTAMP` operands return clipped MySQL-style time text or
  `NULL`;
- mixed supported operand domains return `NULL` without warnings;
- invalid supported string-literal content returns `NULL` with warning 1292;
- successful in-range expressions produce no warnings and do not mutate catalog
  state, descriptors, physical tables, or the `.mylite` preamble.

`TIMEDIFF` remains available as an unquoted identifier where the current
identifier grammar admits nonreserved function names, including tested
table-name positions. It is not an `IGNORE_SPACE`-reserved function name in the
current slice.

## Deferred Surface

This slice intentionally does not support:

- predicates, ordering, grouping, DML assignments, defaults, generated columns,
  check constraints, or arbitrary expression evaluation with `TIMEDIFF()`;
- row-scalar string-family descriptor columns;
- numeric time operands, compact temporal numeric forms, decimal/fractional
  seconds, relaxed date-only string coercions, plus signs on time strings,
  zero or partial-zero temporal values, time-zone effects, or protocol-grade
  fractional result metadata;
- parameters, user variables, subqueries, function calls, or arithmetic
  expressions as operands.

## Grammar

The parser admits the independently authored two-argument function form:

```lemon
expression ::= TIMEDIFF LPAREN expression COMMA expression RPAREN.
```

Wrong-arity calls are represented with an explicit AST error node so execution
can return MySQL-compatible native-function parameter-count diagnostics:

```lemon
expression ::= TIMEDIFF LPAREN RPAREN.
expression ::= TIMEDIFF LPAREN expression RPAREN.
expression ::= TIMEDIFF LPAREN expression COMMA expression COMMA function_argument_list RPAREN.
```

Identifier grammar admits the name as an ordinary identifier:

```lemon
identifier ::= TIMEDIFF.
```

The runtime, not the grammar, enforces operand literal kinds, descriptor kinds,
canonical temporal shapes, mixed-domain behavior, warnings, and result range.

## Runtime Semantics

### Evaluation

1. Evaluate the first argument with this feature's MyLite-owned literal or
   descriptor rules. `NULL` short-circuits to `NULL`.
2. Decode string literals through the current statement SQL mode. This reuses
   the same literal-decoding path as existing scalar functions, including
   `ANSI_QUOTES` and `NO_BACKSLASH_ESCAPES` behavior.
3. Classify a non-`NULL` string as either a canonical datetime or canonical
   time. Unsupported string contents return `NULL` with warning 1292.
4. Evaluate the second argument. `NULL` returns `NULL`; otherwise it must
   classify into one of the supported operand domains.
5. Subtract `expr2` from `expr1`.
6. `DATE` operands are midnight date/time values, but only `DATE`-vs-`DATE`
   is claimed in this slice. `DATETIME` and `TIMESTAMP` share the datetime
   domain. `TIME` is a separate signed-seconds domain.
7. Domain mismatches return `NULL` without warnings.
8. Result seconds outside the MySQL `TIME` display range clip to
   `838:59:59` / `-838:59:59` and append warning 1292 using the unclipped
   time text in the warning message.
9. Return text or `NULL` through the existing scalar result path.

### Diagnostics

Supported in-range expressions return successfully with `warning_count == 0`.

Required diagnostics:

- syntax errors: existing parser syntax diagnostics;
- wrong arity: native-function parameter-count diagnostics for `TIMEDIFF`;
- unknown identifiers in no-source operands: existing unknown-column
  diagnostics;
- operand is not a string literal, supported descriptor column, or `NULL`:
  deterministic unsupported diagnostics;
- string operand contains embedded NUL:
  `TIMEDIFF() literals do not support NUL bytes`;
- invalid string temporal content: `NULL` result plus warning 1292;
- clipped time results: clipped result plus warning 1292;
- allocation failure: existing `MYLITE_NOMEM` diagnostic behavior;
- public API misuse: no public API changes.

## Architecture

- Public API: unchanged. Successful `SELECT` and `DO` use existing
  `mylite_execute()` and result APIs.
- Statement context: unchanged except that parser/lexer SQL-mode flags
  determine string literal interpretation.
- Lexer/parser/AST: add a nonreserved function token, one function AST node
  kind, and one argument-count error node kind. Do not introduce general
  function-call parsing.
- Analyzer/planner/runtime: no-source scalar execution calls a MyLite-owned
  value helper directly. Row-scalar projection resolves descriptor columns
  from MyLite descriptors, not SQLite metadata, and emits a stable internal
  SQLite callback expression with bound literal arguments.
- Catalog: untouched. The function does not read or mutate descriptors,
  descriptor caches, catalog generation, or `sqlite_schema_generation`.
- Result builder: returns a text/`NULL` scalar cell through existing scalar and
  row-scalar result paths. Column labels come from the original expression span
  or explicit alias.
- Storage/VFS/file format: untouched. The function is pure scalar evaluation
  and must not modify SQLite payload or the MyLite preamble.
- SQLite: use public SQLite scalar-function registration for row-backed
  projection only. No SQLite fork patch is required.

## Tests

Add fast C coverage under `packages/libmylite/tests/`:

- parser accepts supported `TIMEDIFF()` calls, mixed-case names, aliases,
  `FROM DUAL`, `DO`, and ordinary whitespace before `(`;
- parser preserves `TIMEDIFF` as an ordinary identifier in tested table-name
  positions, including under `IGNORE_SPACE`;
- parser produces explicit wrong-arity AST nodes;
- runtime covers datetime, time, and mixed-domain scalar values, `NULL`
  short-circuiting, aliases, `FROM DUAL`, `DO`, independent handles, and
  `ROW_COUNT()` behavior;
- row-scalar runtime covers `TIME`, `DATE`, `DATETIME`, and `TIMESTAMP`
  descriptor columns, `WHERE`/`ORDER BY`/`LIMIT` composition, reopen
  persistence, and warnings;
- current SQL mode behavior for double-quoted strings, `ANSI_QUOTES`, and
  `NO_BACKSLASH_ESCAPES`;
- deterministic diagnostics for unsupported operands, embedded NUL, unknown
  identifiers, and wrong arity;
- file preamble and catalog generation remain unchanged across scalar
  evaluation.

Add a MySQL 8.4.9 expectation artifact:

```sh
./packages/libmylite/tests/mysql_baseline_timediff_function_expectations.sh
```

## Compatibility Updates

Update:

- `COMPATIBILITY.md`
- `docs/compatibility/functions-temporal.md`
- `docs/compatibility/sql-query-expressions.md`
- `docs/compatibility/type-system-literals-conversion.md`
