# Baseline Row Temporal Interval SECOND Projection

## Goal

Add the first table-backed projection slice for MySQL interval-second temporal
arithmetic:

```sql
SELECT DATE_ADD(value, INTERVAL signed_integer_or_NULL SECOND) FROM table_name;
SELECT DATE_SUB(value, INTERVAL signed_integer_or_NULL SECOND) FROM table_name;
SELECT ADDDATE(value, INTERVAL signed_integer_or_NULL SECOND) FROM table_name;
SELECT SUBDATE(value, INTERVAL signed_integer_or_NULL SECOND) FROM table_name;
```

This extends the existing no-source, `FROM DUAL`, and `DO` scalar
`DATE_ADD()` / `DATE_SUB()` / `ADDDATE()` / `SUBDATE()` slices into the
descriptor-driven single-table row-scalar `SELECT` path. It is not general
temporal arithmetic. It keeps the existing whole-second interval unit and
literal interval restriction, and it deliberately does not make these functions
available in predicates, ordering/grouping expressions, DML assignment values,
defaults, or generated columns.

## Sources

- Official MySQL 8.4 date and time function documentation:
  <https://dev.mysql.com/doc/refman/8.4/en/date-and-time-functions.html>
- Official MySQL 8.4 expression and temporal interval documentation:
  <https://dev.mysql.com/doc/refman/8.4/en/expressions.html>
- Observed MySQL 8.4.9 runtime behavior, captured by
  `packages/libmylite/tests/mysql_baseline_row_temporal_interval_second_projection_expectations.sh`.

The MyLite grammar and implementation are independently authored from the
official documentation and MySQL 8.4.9 runtime observations. Do not copy MySQL
grammar or implementation sources.

## MySQL 8.4.9 Runtime Observations

Runtime probes against MySQL 8.4.9 establish these expectations for the
admitted row-backed subset:

- `DATE_ADD(date_column, INTERVAL 1 SECOND)` returns a datetime string. A
  `DATE` value is interpreted as midnight because `SECOND` is a time unit.
- `DATE_SUB(datetime_column, INTERVAL 1 SECOND)` subtracts one second.
  `DATE_SUB(..., INTERVAL -1 SECOND)` and `SUBDATE(..., INTERVAL -1 SECOND)`
  add one second. `ADDDATE(..., INTERVAL -1 SECOND)` subtracts one second.
- `DATE`, `DATETIME`, `TIMESTAMP`, `VARCHAR`, and `TEXT` source values are
  accepted by MySQL for the tested canonical input values.
- A `NULL` temporal value or `NULL` interval returns `NULL` without warnings.
- Invalid string temporal values return `NULL` and append warning
  `1292 / 22007` with an `Incorrect datetime value` message.
- Per-row result overflow returns `NULL` and appends warning
  `1441 / HY000` with a datetime field overflow message.
- MySQL accepts interval expressions, interval columns, and string interval
  values for this function family, but those forms remain deferred by MyLite's
  current narrow interval-second slice.
- `WHERE`, one-column descriptor `ORDER BY`, and `LIMIT row_count` compose with
  the projection through the existing row-scalar `SELECT` envelope.

## Supported Surface

MyLite supports this exact table-backed subset:

- single-table row-scalar `SELECT` projection over the current descriptor-backed
  persistent and session temporary base-table source envelope;
- optional existing supported `WHERE`, `ORDER BY`, and `LIMIT` clauses through
  the current row-scalar `SELECT` planner;
- `DATE_ADD(value, INTERVAL interval_value SECOND)`;
- `DATE_SUB(value, INTERVAL interval_value SECOND)`;
- `ADDDATE(value, INTERVAL interval_value SECOND)`;
- `SUBDATE(value, INTERVAL interval_value SECOND)`;
- temporal argument `value` limited to:
  - an unqualified or supported source-qualified descriptor column with
    descriptor type `DATE`, `DATETIME`, `TIMESTAMP`, or a nonbinary string
    family type;
  - a single- or double-quoted canonical date string `YYYY-MM-DD`;
  - a single- or double-quoted canonical datetime string
    `YYYY-MM-DD HH:MM:SS`;
  - `NULL`;
- literal temporal inputs and descriptor string values must be canonical valid
  date or datetime values in the current nonzero MyLite temporal arithmetic
  range `1000-01-01` through `9999-12-31 23:59:59`;
- descriptor `DATE`, `DATETIME`, and `TIMESTAMP` values use their stored
  canonical text. Nonzero in-range values are supported. Zero and partial-zero
  temporal descriptor values return `NULL`; full-zero descriptor values produce
  a warning matching the string form and partial-zero descriptor values remain
  a known limitation of this slice;
- interval argument is limited to a decimal integer literal with optional unary
  `+` or `-`, or `NULL`;
- interval integer values must fit the signed 64-bit range;
- `ADDDATE` adds the interval, matching `DATE_ADD`;
- `DATE_SUB` and `SUBDATE` subtract the interval. Negative intervals therefore
  add seconds;
- successful in-range rows return datetime text in `YYYY-MM-DD HH:MM:SS` form;
- invalid row values and row-specific overflow return `NULL` with MySQL-style
  warnings instead of aborting the statement;
- the statement returns through the existing public result API for row-returning
  `SELECT`; `ROW_COUNT()` remains `-1` after successful selects.

## Deferred Surface

This slice intentionally does not support:

- no-source scalar behavior beyond the already implemented baseline slices;
- interval units other than `SECOND`;
- plural or alternate unit spellings;
- `ADDDATE(date, days)` or non-interval `SUBDATE` forms;
- `TIMESTAMPADD()`, `ADDTIME()`, `SUBTIME()`, or infix temporal arithmetic;
- interval expressions, string intervals, decimal/fractional intervals,
  floating-point intervals, function or column interval operands, parameters,
  user variables, or subqueries;
- `TIME` arguments, numeric temporal coercion, compact temporal numeric forms,
  relaxed temporal strings, fractional seconds, time-zone conversion,
  `TIMESTAMP` session time-zone storage/retrieval semantics, or complete
  zero/partial-zero temporal arithmetic parity;
- nested function arguments beyond the existing row-scalar envelope;
- predicates, ordering keys, grouping expressions, DML assignments, defaults,
  generated columns, check constraints, or arbitrary expression evaluation.

## Grammar

The parser already admits the function-call shape for the scalar slice. This
feature reuses that independently authored shape in row-scalar `SELECT`
projection:

```lemon
expression ::= DATE_ADD LPAREN expression COMMA INTERVAL expression SECOND RPAREN.
expression ::= DATE_SUB LPAREN expression COMMA INTERVAL expression SECOND RPAREN.
expression ::= ADDDATE LPAREN expression COMMA INTERVAL expression SECOND RPAREN.
expression ::= SUBDATE LPAREN expression COMMA INTERVAL expression SECOND RPAREN.
```

The analyzer, not the grammar, enforces this feature's narrower row-backed
argument rules. Existing function-name whitespace and `IGNORE_SPACE` behavior
remain owned by the existing parser/lexer implementation.

## Runtime Semantics

### Planning

1. Route a select list containing one of the four function AST node kinds
   through the existing row-scalar projection planner when the surrounding
   statement otherwise fits the row-scalar source envelope.
2. Resolve descriptor column arguments using the existing source context and
   MyLite catalog descriptors. SQLite metadata is not consulted for type or
   column existence.
3. Reject unknown descriptor columns with the current unknown-column
   diagnostics.
4. Reject unsupported temporal argument kinds with a deterministic
   function-specific unsupported diagnostic.
5. Reject unsupported interval argument kinds and signed-64 overflow during
   planning.

### Evaluation

1. Emit a descriptor-built SQLite `SELECT` over the stable physical MyLite table
   name, with projection values lowered to a private MyLite SQLite scalar
   function.
2. Quote every generated SQLite identifier and bind scalar literals as
   parameters. Literal temporal strings, literal interval values, input-kind
   discriminators, and operation discriminators are never interpolated into SQL
   text.
3. Evaluate one row at a time through the private MyLite scalar callback. The
   callback converts the source value according to the planned input kind,
   applies whole-second arithmetic, and returns text or `NULL`.
4. `NULL` temporal or interval inputs return `NULL`.
5. Invalid row temporal values append warning `1292 / 22007` and return `NULL`.
6. Row result overflow appends warning `1441 / HY000` and returns `NULL`.
7. In-range rows produce no warnings.

## Architecture

- Public API: unchanged. Results use the existing `mylite_execute()` and
  `mylite_result` APIs.
- Statement context: unchanged, except the existing statement diagnostics own
  warnings emitted by row-level SQLite callbacks.
- Lexer/parser/AST: unchanged for grammar; the existing AST node kinds become
  admissible in table-backed row-scalar projection.
- Analyzer/planner: extends only the row-scalar projection planner. Descriptor
  column resolution stays catalog-owned and source-context aware.
- Catalog: read-only. The feature does not mutate descriptors, descriptor
  caches, catalog generation, or `sqlite_schema_generation`.
- Result builder: unchanged. Projection labels continue to come from the
  source expression span or explicit alias.
- Storage/VFS/file format: unchanged. The feature reads existing physical rows
  through SQLite and does not alter `.mylite` preamble or payload layout.
- SQLite integration: uses public SQLite scalar-function registration and
  prepared statement APIs. No SQLite fork patch is needed.

## Diagnostics And Warnings

Supported in-range expressions return successfully with `warning_count == 0`.
Required failure or warning behavior:

- syntax errors: existing parser syntax diagnostics;
- wrong function shape: existing native function arity or parser diagnostics;
- unknown temporal column: current MySQL-compatible unknown-column diagnostic;
- unsupported temporal argument:
  `FUNCTION() supports only string temporal literals, DATE, DATETIME, TIMESTAMP descriptor columns, string descriptor columns, and NULL`;
- unsupported `TIME` descriptor argument:
  `FUNCTION() does not yet support TIME values`;
- temporal string literal contains embedded NUL:
  `FUNCTION() date literals do not support NUL bytes`;
- unsupported interval argument:
  `FUNCTION() INTERVAL SECOND supports only signed integer literals and NULL`;
- interval integer outside signed-64 range:
  `FUNCTION() INTERVAL SECOND literals must fit the signed 64-bit range`;
- invalid row temporal value: warning `1292 / 22007`
  `Incorrect datetime value: 'value'`, and the row result is `NULL`;
- row result overflow: warning `1441 / HY000`
  `Datetime function: datetime field overflow`, and the row result is
  `NULL`;
- physical SQLite failure: existing runtime error propagation;
- allocation failure: existing `MYLITE_NOMEM` diagnostic behavior;
- public API misuse: no public API changes.

## Tests

Add a focused plain C runtime test and a MySQL expectation script covering:

- projection over descriptor `DATE`, `DATETIME`, `TIMESTAMP`, `VARCHAR`, and
  `TEXT` values;
- `DATE_ADD`, `DATE_SUB`, `ADDDATE`, and `SUBDATE`, including negative and
  `NULL` intervals;
- `NULL` source values;
- invalid descriptor string values returning `NULL` with warning `1292`;
- row result overflow returning `NULL` with warning `1441`;
- existing row-scalar `WHERE`, `ORDER BY`, and `LIMIT` composition;
- source-qualified column resolution;
- unknown column and unsupported `TIME` column diagnostics;
- unsupported interval column, expression, string, and out-of-range literal
  diagnostics;
- close/reopen persistence and unchanged file preamble behavior.
