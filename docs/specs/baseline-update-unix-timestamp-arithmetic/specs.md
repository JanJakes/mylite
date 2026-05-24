# Baseline UPDATE UNIX_TIMESTAMP Arithmetic

## Goal

Support the WordPress-style timeout value shape in descriptor-driven single-table
updates:

```sql
UPDATE wp_options
SET option_value = UNIX_TIMESTAMP() + 3600
WHERE option_name = '_site_transient_timeout_key'
```

This is a narrow extension of the existing `UPDATE` assignment path and the
existing `INSERT` / `REPLACE` / duplicate-update `UNIX_TIMESTAMP()` arithmetic
value support. It is not a general DML expression engine.

## Sources

- Official MySQL 8.4 Reference Manual, `UPDATE` statement:
  <https://dev.mysql.com/doc/refman/8.4/en/update.html>
- Official MySQL 8.4 Reference Manual, date and time functions:
  <https://dev.mysql.com/doc/refman/8.4/en/date-and-time-functions.html>
- Official MySQL 8.4 Reference Manual, arithmetic operators:
  <https://dev.mysql.com/doc/refman/8.4/en/arithmetic-functions.html>
- Existing MyLite designs:
  - `docs/specs/baseline-update-lifecycle/specs.md`
  - `docs/specs/baseline-update-constant-arithmetic-assignment/specs.md`
  - `docs/specs/baseline-insert-values-unix-timestamp-arithmetic/specs.md`
  - `docs/specs/baseline-unix-timestamp-function/specs.md`
  - `docs/specs/baseline-dml-string-numeric-coercion/specs.md`
- Observed MySQL 8.4.9 runtime behavior captured by
  `packages/libmylite/tests/mysql_baseline_update_unix_timestamp_arithmetic_expectations.sh`.

The MyLite grammar and implementation are independently authored from official
documentation, observed MySQL 8.4.9 behavior, public SQLite APIs, and existing
MyLite code. Do not copy MySQL, MariaDB, Percona, SQLite implementation
internals, or restrictively licensed sources.

## MySQL 8.4.9 Runtime Observations

Runtime probes establish the behavior used by this baseline:

- `UPDATE ... SET c = UNIX_TIMESTAMP()` uses the statement timestamp. With
  `SET timestamp = 1704067200`, the stored value is `1704067200`.
- `UNIX_TIMESTAMP() + signed_integer_literal` and `UNIX_TIMESTAMP() -
  signed_integer_literal` produce signed integer arithmetic results.
- `UNIX_TIMESTAMP() + NULL` and `UNIX_TIMESTAMP() - NULL` produce `NULL`.
- Assigning a `NULL` arithmetic result to a nullable target succeeds and reports
  one changed row when the old value was non-`NULL`.
- Assigning a `NULL` arithmetic result to a `NOT NULL` target fails under
  strict mode with `1048 / 23000`; with `sql_mode = ''`, it stores the
  descriptor implicit value and records warning `1048`.
- Assigning the arithmetic result to an integer column uses normal column range
  conversion. A value outside `INT` range fails with `1264 / 22003` under the
  current strict baseline.
- Assigning the arithmetic result to `CHAR`, `VARCHAR`, and `TEXT`-family
  targets stores the visible decimal integer string. A WordPress `LONGTEXT`
  `wp_options.option_value` column stores `UNIX_TIMESTAMP() + -90` as text such
  as `1704067110`.
- String target length checks use normal MySQL row-value conversion. Updating a
  `VARCHAR(4)` with a ten-digit timestamp fails with `1406 / 22001` in strict
  mode.
- Arithmetic overflow before assignment fails with `1690 / 22003`.
- Successful supported updates produce no row result set, changed-row
  `affected_rows`, and `warning_count == 0`.

MySQL accepts much broader update expressions, including function arguments,
column-to-column arithmetic, multiplication, division, parameters, variables,
subqueries, and implicit conversion into more target families. Those remain
deferred in this slice.

## Supported Surface

MyLite supports one single-table assignment:

```sql
UPDATE table_name
SET column_name = unix_timestamp_update_value
[WHERE baseline_predicate]
[ORDER BY order_column [ASC | DESC]]
[LIMIT row_count]
```

Supported assignment values:

```sql
UNIX_TIMESTAMP()
UNIX_TIMESTAMP() + integer_literal
UNIX_TIMESTAMP() + +integer_literal
UNIX_TIMESTAMP() + -integer_literal
UNIX_TIMESTAMP() + NULL
UNIX_TIMESTAMP() - integer_literal
UNIX_TIMESTAMP() - +integer_literal
UNIX_TIMESTAMP() - -integer_literal
UNIX_TIMESTAMP() - NULL
```

Supported target columns:

- non-`AUTO_INCREMENT` integer-family descriptor targets whose physical storage
  is currently MyLite's integer path, including signed and unsigned `INT`,
  `INTEGER`, and `BIGINT` within MyLite's signed-64 physical storage envelope;
- nonbinary string targets in the current `CHAR`, `VARCHAR`, and baseline
  `TEXT` family, including `TINYTEXT`, `TEXT`, `MEDIUMTEXT`, and `LONGTEXT`.

The existing single-table `UPDATE` schema resolution, descriptor target
resolution, `WHERE`, `ORDER BY`, `LIMIT`, changed-row accounting, duplicate-key
checks, foreign-key checks, auto-update timestamp handling, file-backed
persistence, and `.mylite` preamble invariants remain in force.

## Deferred Surface

This slice intentionally does not support:

- `UNIX_TIMESTAMP(value)` with arguments in update assignments;
- `1 + UNIX_TIMESTAMP()`, multiplication, division, modulo, bitwise operators,
  comparisons, or nested general scalar arithmetic around `UNIX_TIMESTAMP()`;
- multiple assignments containing a `UNIX_TIMESTAMP()` arithmetic value;
- joined updates with `UNIX_TIMESTAMP()` arithmetic assignments;
- string, decimal, float, bit, hex, parameter, variable, subquery, column,
  aggregate, or arbitrary function operands;
- decimal, approximate, temporal, `YEAR`, `BIT`, binary string, `ENUM`, `SET`,
  `JSON`, or spatial target conversion for these expressions;
- explicit `AUTO_INCREMENT` target assignment with these expressions;
- predicates, `ORDER BY`, `GROUP BY`, `HAVING`, defaults, generated columns,
  indexes, constraints, triggers, privilege semantics, or arbitrary SQLite
  pass-through beyond the existing update slice.

## Grammar

The existing expression grammar already parses the supported syntax. This phase
does not need a new AST node. Runtime recognizes only this MyLite subset:

```lemon
update_value(A) ::= update_unix_timestamp_value(B).

update_unix_timestamp_value(A) ::= update_unix_timestamp_now(B).
update_unix_timestamp_value(A) ::= update_unix_timestamp_now(B) PLUS(T) update_unix_timestamp_delta(C).
update_unix_timestamp_value(A) ::= update_unix_timestamp_now(B) MINUS(T) update_unix_timestamp_delta(C).

update_unix_timestamp_now(A) ::= UNIX_TIMESTAMP(T) LPAREN RPAREN(R).

update_unix_timestamp_delta(A) ::= INTEGER(T).
update_unix_timestamp_delta(A) ::= PLUS(P) INTEGER(T).
update_unix_timestamp_delta(A) ::= MINUS(M) INTEGER(T).
update_unix_timestamp_delta(A) ::= NULL(T).
```

These snippets describe MyLite's supported subset, not MySQL's full grammar.

## Runtime Semantics

Planning/conversion:

1. Resolve the update target table and assignment target using the existing
   descriptor-driven single-table `UPDATE` policy.
2. Detect the admitted `UNIX_TIMESTAMP()` value expression before the ordinary
   same-column arithmetic planner rejects binary update expressions.
3. Reject target descriptors outside the admitted integer and nonbinary string
   set with a MyLite-specific unsupported diagnostic.
4. Evaluate no-argument `UNIX_TIMESTAMP()` using the existing statement-context
   timestamp helper.
5. Evaluate the optional signed integer or `NULL` delta through MyLite-owned
   signed-64 arithmetic. If the delta is `NULL`, the expression result is
   `NULL`.
6. Convert the resulting signed integer or `NULL` through the target descriptor:
   - integer-family targets use the existing strict update integer conversion;
   - nonbinary string targets format the signed integer as decimal text, then
     use the existing `CHAR` / `VARCHAR` / `TEXT` conversion and length checking
     path, including current non-strict string truncation adjustment;
   - `NULL` uses the existing nullable, strict, and ordinary non-strict update
     adjustment path.
7. Bind the resulting planned value with the existing descriptor-built SQLite
   update statement.

The expression is evaluated only if the statement matches rows, matching the
current update assignment materialization model for values that can fail at
conversion time. Descriptor target-family validation happens during planning.

## Ownership Boundaries

- Public API: unchanged. Callers use `mylite_execute()` and existing result
  accessors.
- Statement context: remains the source of no-argument `UNIX_TIMESTAMP()` time.
- Lexer/parser/AST: no new public syntax object; the existing expression AST is
  reused and narrowed by runtime planning.
- Analyzer/planner: descriptor-driven table and column resolution remains
  authoritative. The update expression is reduced to a planned integer, planned
  text, or `NULL` before SQLite SQL generation.
- Catalog: read-only for value conversion. This feature does not mutate table
  descriptors, descriptor versions, catalog generation, or
  `sqlite_schema_generation`.
- Result builder: successful statements continue to return non-row results with
  affected rows and warning counts through existing conventions.
- Storage/VFS/file format: unchanged. The `.mylite` preamble and shifted SQLite
  payload invariants are preserved.
- SQLite: receives stable physical table names, quoted identifiers, and bound
  values. No SQLite fork patch is required.

## Diagnostics

Required diagnostics:

- unsupported target descriptor:
  `UPDATE UNIX_TIMESTAMP arithmetic supports only integer and nonbinary string
  targets`;
- explicit `AUTO_INCREMENT` target descriptor:
  `UPDATE UNIX_TIMESTAMP arithmetic does not yet support AUTO_INCREMENT
  targets`;
- unsupported operand, out-of-range delta, overflow, or syntax outside the
  admitted grammar: deterministic parse or unsupported-expression diagnostics;
- wrong target conversion, including integer out of range:
  existing descriptor conversion diagnostics such as `1264 / 22003`;
- `NULL` into `NOT NULL`: existing `1048 / 23000`, with current ordinary
  non-strict warning adjustment where already supported;
- arithmetic overflow: existing MyLite scalar arithmetic `1690 / 22003`
  diagnostic;
- allocation failure: existing `MYLITE_NOMEM` behavior;
- physical SQLite failure: existing wrapped SQLite diagnostics;
- public API misuse: no public API changes.

Supported in-range statements return `warning_count == 0`.

## Performance And Storage

The expression is statement-local and source-free, so MyLite evaluates it once
for the matched update statement and binds the resulting value. It does not
materialize table rows in MyLite memory for expression evaluation, add
temporary SQLite tables, or ask SQLite to parse MySQL expression syntax. SQLite
continues to execute row filtering, ordering, limiting, changed-row checks, and
physical mutation.

## Tests

Add fast C tests and a MySQL-runtime expectation artifact covering:

- WordPress `wp_options.option_value LONGTEXT` update shape with
  `UNIX_TIMESTAMP() + -90`;
- integer target updates with no-argument, plus, minus, signed deltas, nullable
  `NULL`, affected rows, and zero warnings;
- string target updates for `CHAR`, `VARCHAR`, `TEXT`, and `LONGTEXT`;
- strict and non-strict `NULL` into `NOT NULL` string targets;
- `WHERE`, `ORDER BY`, and `LIMIT` interaction through existing update
  execution;
- string length errors, unsupported target descriptors, `AUTO_INCREMENT`
  targets, unsupported expression shapes, signed-64 delta limits, and
  arithmetic overflow;
- no-match and `LIMIT 0` behavior;
- reopen persistence and `.mylite` preamble preservation.

Update:

- `COMPATIBILITY.md`;
- `docs/compatibility/sql-table-dml.md`;
- `docs/compatibility/functions-temporal.md`;
- `docs/compatibility/sql-query-expressions.md` and
  `docs/compatibility/type-system-literals-conversion.md` only for the exact
  DML expression/literal surface.
