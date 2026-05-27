# Baseline UPDATE DATE Interval Assignment

## Goal

Support the common WordPress-style update assignment shape that applies a
single DATE interval function to the same descriptor column being updated:

```sql
UPDATE _dates
SET option_value = DATE_SUB(option_value, INTERVAL '2' YEAR)
WHERE option_name = 'expires';
```

This is a narrow extension of the existing descriptor-driven single-table
`UPDATE` path and the existing scalar / row-scalar `DATE_ADD()` /
`DATE_SUB()` / `ADDDATE()` / `SUBDATE()` core-unit implementation. It is not a
general update expression engine.

## Sources

- Official MySQL 8.4 Reference Manual, `UPDATE` statement:
  <https://dev.mysql.com/doc/refman/8.4/en/update.html>
- Official MySQL 8.4 Reference Manual, date and time functions:
  <https://dev.mysql.com/doc/refman/8.4/en/date-and-time-functions.html>
- Existing MyLite designs:
  - `docs/specs/baseline-update-lifecycle/specs.md`
  - `docs/specs/baseline-update-unix-timestamp-arithmetic/specs.md`
  - `docs/specs/baseline-date-interval-core-units/specs.md`
  - `docs/specs/baseline-temporal-types/specs.md`
- Observed MySQL 8.4.9 runtime behavior captured by
  `packages/libmylite/tests/mysql_baseline_update_date_interval_assignment_expectations.sh`.

The MyLite grammar and implementation are independently authored from official
documentation, observed MySQL 8.4.9 behavior, public SQLite APIs, and existing
MyLite code. Do not copy MySQL, MariaDB, Percona, SQLite implementation
internals, or restrictively licensed sources.

## MySQL 8.4.9 Runtime Observations

Runtime probes establish the behavior used by this baseline:

- `UPDATE t SET dt = DATE_SUB(dt, INTERVAL '2' YEAR)` over a `DATETIME`
  descriptor stores datetime text, reports changed-row `ROW_COUNT()`, and
  produces no warnings for in-range values.
- `DATE_ADD(date_col, INTERVAL 1 MONTH)` over `DATE` storage clamps to the last
  valid day of the target month, such as `2024-01-31` to `2024-02-29`.
- `SUBDATE(text_col, INTERVAL '+2' YEAR)` and `ADDDATE(varchar_col, INTERVAL 1
  DAY)` evaluate canonical temporal strings and store the resulting text in
  string-family targets.
- A `NULL` source column or `INTERVAL NULL unit` produces `NULL`. If the target
  is nullable, changed-row accounting follows the old value; if a matched row
  would store `NULL` into a `NOT NULL` target, MySQL raises `1048 / 23000`.
- `INTERVAL 0 DAY` over an unchanged value reports zero changed rows.
- Existing `WHERE`, `ORDER BY`, and `LIMIT` filtering restrict which rows are
  evaluated. No-match updates and `LIMIT 0` skip invalid source values and
  `NULL` intervals into `NOT NULL` targets, then report zero rows and zero
  warnings.
- A matched invalid string source raises `1292 / 22007` with an incorrect
  datetime value diagnostic.
- A matched arithmetic overflow raises `1441 / 22008` with a datetime-field
  overflow diagnostic.
- MySQL accepts broader update expressions, cross-column function arguments,
  date-target time-unit truncation warnings, expression intervals, relaxed
  interval strings, subqueries, parameters, variables, and multi-table update
  forms. Those remain deferred here.

## Supported Surface

MyLite supports one single-table assignment:

```sql
UPDATE table_name
SET column_name = date_interval_update_value
[WHERE baseline_predicate]
[ORDER BY order_column [ASC | DESC]]
[LIMIT row_count]
```

Supported assignment values:

```sql
DATE_ADD(column_name, INTERVAL interval_value interval_unit)
DATE_SUB(column_name, INTERVAL interval_value interval_unit)
ADDDATE(column_name, INTERVAL interval_value interval_unit)
SUBDATE(column_name, INTERVAL interval_value interval_unit)
```

Supported `column_name` rules:

- the assignment target is one unqualified descriptor column;
- the function temporal argument must be the same unqualified descriptor column;
- schema and target table resolution use the existing single-table `UPDATE`
  policy for unqualified and schema-qualified targets;
- reserved `_mylite_*` schema and table names are rejected before SQLite SQL is
  generated.

Supported target/source descriptor families:

- non-key `DATE`, for date-bearing units only;
- non-key `DATETIME`;
- nonbinary `CHAR` and `VARCHAR` when the declared length can store the current
  19-byte datetime result envelope and the column is not a key part;
- baseline nonbinary `TEXT`, `TINYTEXT`, `MEDIUMTEXT`, and `LONGTEXT` when the
  column is not a key part.

Supported interval units:

- `YEAR`;
- `QUARTER`;
- `MONTH`;
- `WEEK`;
- `DAY`;
- `HOUR`;
- `MINUTE`;
- `SECOND`.

Supported interval values:

- decimal integer literals with optional unary `+` or `-`;
- exact quoted decimal integer strings with optional unary `+` or `-`;
- `NULL`.

The existing descriptor-driven `WHERE`, `ORDER BY`, `LIMIT`, duplicate-key
checks, foreign-key checks, auto-update timestamp handling, file-backed
persistence, and `.mylite` preamble invariants remain in force.

## Deferred Surface

This slice intentionally does not support:

- any function temporal argument other than the same assignment target column;
- table-qualified assignment targets or table-qualified function column
  arguments;
- multiple assignments containing DATE interval expressions;
- joined updates containing DATE interval assignments;
- `UPDATE IGNORE` for DATE interval assignments;
- `TIMESTAMP` targets, because this slice defers fixed-UTC timestamp range and
  timezone conversion for row-dependent DATE interval results;
- `DATE` targets with time-bearing units, because MySQL stores the original
  date and emits a truncation warning; MyLite defers that warning/truncation
  path;
- `TIME`, `YEAR`, numeric, decimal, approximate, binary string, `ENUM`, `SET`,
  `JSON`, spatial, generated, or `AUTO_INCREMENT` targets;
- primary-key or unique-key assignment targets;
- `MICROSECOND`, composite interval units, `SQL_TSI_` aliases, or
  `ADDDATE(date, days)`;
- expression intervals, column intervals, relaxed prefix interval strings,
  booleans, decimals, floats, parameters, variables, subqueries, functions,
  arithmetic wrappers, or arbitrary expressions;
- temporal expression coercion outside the admitted same-column function form;
- arbitrary SQLite pass-through or SQLite fork patches.

## Grammar

MyLite admits this independently authored parser shape:

```lemon
update_value(A) ::=
    DATE_ADD(T) LPAREN(L) arithmetic_update_source_column(C)
    COMMA INTERVAL update_date_interval_interval(I) date_interval_unit(U)
    RPAREN(R).
update_value(A) ::=
    DATE_SUB(T) LPAREN(L) arithmetic_update_source_column(C)
    COMMA INTERVAL update_date_interval_interval(I) date_interval_unit(U)
    RPAREN(R).
update_value(A) ::=
    ADDDATE(T) LPAREN(L) arithmetic_update_source_column(C)
    COMMA INTERVAL update_date_interval_interval(I) date_interval_unit(U)
    RPAREN(R).
update_value(A) ::=
    SUBDATE(T) LPAREN(L) arithmetic_update_source_column(C)
    COMMA INTERVAL update_date_interval_interval(I) date_interval_unit(U)
    RPAREN(R).

update_date_interval_interval(A) ::= INTEGER(T).
update_date_interval_interval(A) ::= PLUS(P) INTEGER(T).
update_date_interval_interval(A) ::= MINUS(M) INTEGER(T).
update_date_interval_interval(A) ::= STRING(T).
update_date_interval_interval(A) ::= NULL(T).
```

These snippets describe MyLite's supported subset, not MySQL's full grammar.
Native function spacing remains governed by the existing lexer and
`IGNORE_SPACE` behavior for built-in function names.

## Runtime Semantics

Planning/conversion:

1. Resolve the update table and assignment column through the existing
   descriptor catalog. SQLite metadata is not authoritative.
2. Recognize the admitted DATE interval function before ordinary update value
   conversion rejects the function expression.
3. Resolve the function column argument from the same descriptor set and require
   it to be the assignment target column.
4. Resolve the interval unit and reject `MICROSECOND` and composite units with
   deterministic unsupported diagnostics.
5. Decode the interval as a signed 64-bit integer, exact quoted signed integer
   string, or `NULL`.
6. Validate target/source descriptor family. `DATE` targets accept only
   date-bearing units. String-family targets must be capable of holding the
   possible datetime result envelope.
7. Defer `INTERVAL NULL unit` into a `NOT NULL` target until after matched-row
   resolution so no-match and `LIMIT 0` statements can complete without error.
   If rows match, return the current MySQL-shaped `1048 / 23000` diagnostic.
8. Lower the update to a descriptor-built SQLite statement using the stable
   physical table name and quoted physical column names.

SQLite execution:

```sql
UPDATE "_mylite_user_table_N"
SET "column_name" = _mylite_date_interval_update(
    "column_name", ?1, ?2, ?3, ?4
)
WHERE ... AND (
    "column_name" IS NOT _mylite_date_interval_update(
        "column_name", ?N, ?N+1, ?N+2, ?N+3
    )
)
```

Bound values are:

1. input-kind discriminator (`date`, `datetime`, or `string`);
2. interval value or `NULL`;
3. interval-unit discriminator;
4. subtract flag.

The changed-row predicate repeats the deterministic function call with its own
bound values so SQLite updates only rows whose stored value would change. The
function is row-dependent, so MyLite does not pre-materialize the updated values
in memory.

Evaluation:

1. `NULL` source values or `NULL` intervals evaluate to `NULL`.
2. Date-only values are interpreted as midnight.
3. Date-bearing units preserve date-only output for `DATE` descriptors and use
   datetime output for datetime/string descriptors.
4. Time-bearing units use datetime output for datetime/string descriptors.
5. Month, quarter, and year arithmetic clamps to the last valid day in the
   target month.
6. Invalid matched source values fail with `1292 / 22007`.
7. Matched arithmetic overflow fails with `1441 / 22008`.
8. Supported in-range updates produce `warning_count == 0`.

The statement does not mutate descriptors, descriptor versions, descriptor
caches, catalog generation, `sqlite_schema_generation`, physical table
definitions, or the `.mylite` preamble.

## Ownership Boundaries

- Public API: unchanged. Callers use `mylite_execute()` and existing result
  accessors.
- Statement context: unchanged; no session timestamp or variable state is
  introduced by this feature.
- Lexer/parser/AST: admits the narrow update-value function shape and reuses
  existing function AST nodes.
- Analyzer/planner: owns schema resolution, descriptor column resolution,
  target-family validation, interval conversion, generated SQL shape, and
  parameter binding.
- Catalog: read-only. Descriptors remain the authority for logical columns and
  physical table names.
- Result builder: successful statements keep existing non-row result behavior,
  affected-row reporting, and warning counts.
- Storage/VFS/file format: unchanged; file-backed handles and shifted SQLite
  payload invariants are preserved.
- SQLite: executes a standard `UPDATE` over quoted physical identifiers and a
  MyLite-registered scalar UDF through the public SQLite function API.

## Diagnostics

Required diagnostics:

- syntax outside the admitted shape: existing parser diagnostics;
- unknown schema/table/assignment/predicate/order columns: existing
  descriptor-driven diagnostics;
- function source column different from the assignment target:
  `UPDATE DATE interval assignment supports only the assigned column as input`;
- unsupported target/source descriptor family:
  `UPDATE DATE interval assignment supports only DATE, DATETIME, and nonbinary string targets`;
- key assignment target:
  `UPDATE DATE interval assignment does not yet support key columns`;
- `UPDATE IGNORE` with DATE interval assignment:
  `UPDATE IGNORE does not yet support DATE interval assignments`;
- unsupported `DATE` target with time-bearing units:
  `UPDATE DATE interval assignment does not yet support time units for DATE targets`;
- unsupported interval unit:
  existing DATE interval unit diagnostic from the scalar feature;
- unsupported interval value:
  existing DATE interval interval-value diagnostic from the scalar feature;
- `NULL` into `NOT NULL`: existing `1048 / 23000`;
- invalid matched source value: `1292 / 22007`;
- arithmetic overflow: `1441 / 22008`;
- allocation failure: existing `MYLITE_NOMEM` behavior;
- physical SQLite failure: existing wrapped SQLite diagnostics, preserving any
  MyLite diagnostic set by the update UDF.

## Tests

Add fast C coverage under `packages/libmylite/tests/`, registered as
`libmylite.runtime.update_date_interval_assignment`, and a MySQL expectation
artifact:

- successful `DATETIME`, `DATE`, `VARCHAR`, and `LONGTEXT` updates;
- `DATE_SUB`, `DATE_ADD`, `ADDDATE`, and `SUBDATE`;
- integer and exact quoted integer intervals, including signed strings;
- `NULL` source and `NULL` interval behavior;
- changed-row accounting, `warning_count == 0`, and absence of result rows;
- `WHERE`, `ORDER BY`, `LIMIT`, and `LIMIT 0` interactions;
- no-match and `LIMIT 0` skipping invalid source values;
- no-match and `LIMIT 0` skipping `INTERVAL NULL` into a `NOT NULL` target;
- invalid matched source and overflow errors;
- `NULL` interval into `NOT NULL`;
- unsupported different source column, table-qualified source, time unit for
  `DATE` target, too-short string target, `TIMESTAMP` target, unsupported target
  family, `UPDATE IGNORE`, expression intervals, and unsupported broader update
  expression forms;
- close/reopen persistence and ordered selects observing updated values;
- no public ABI change and no file-format preamble mutation.
