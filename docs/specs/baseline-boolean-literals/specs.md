# Baseline Boolean Literals

## Status

This feature specifies a narrow `TRUE` / `FALSE` literal slice for the existing
descriptor-backed integer row lifecycle. MySQL 8.4.9 evaluates `TRUE` as `1`
and `FALSE` as `0`. MyLite admits those literals only where the current
baseline already admits integer literals for persistent base-table DML:

- `INSERT ... VALUES` row values;
- `INSERT ... SET` assignment values;
- single-table `UPDATE` assignment values;
- supported filtered `SELECT`, `DELETE`, and `UPDATE` comparison predicate
  right operands.

This slice does not add general truth-expression semantics, boolean operators,
`IS TRUE` / `IS FALSE`, scalar expression evaluation beyond the existing
limited scalar-select path, signed unary operators applied to boolean literals,
or boolean literals in `LIMIT`.

## Sources

- MyLite README architecture: `README.md`
- MyLite engineering standards:
  `docs/architecture/engineering-standards.md`
- Baseline implementation strategy:
  `docs/specs/baseline-implementation-strategy/specs.md`
- Baseline row values:
  `docs/specs/baseline-row-values-lifecycle/specs.md`
- Baseline select where:
  `docs/specs/baseline-select-where-lifecycle/specs.md`
- Baseline delete:
  `docs/specs/baseline-delete-lifecycle/specs.md`
- Baseline update:
  `docs/specs/baseline-update-lifecycle/specs.md`
- Baseline BOOL and BOOLEAN aliases:
  `docs/specs/baseline-bool-boolean-aliases/specs.md`
- MySQL 8.4 Reference Manual, boolean literals:
  https://dev.mysql.com/doc/refman/8.4/en/boolean-literals.html
- MySQL 8.4 Reference Manual, numeric type syntax:
  https://dev.mysql.com/doc/refman/8.4/en/numeric-type-syntax.html

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior, public
SQLite APIs, and existing MyLite source code. It does not copy MySQL, MariaDB,
Percona, SQLite implementation internals, or other restrictively licensed
implementation sources.

## MySQL 8.4.9 Evidence Summary

Observed MySQL 8.4.9 behavior:

- `TRUE`, `true`, `FALSE`, and `false` evaluate as `1`, `1`, `0`, and `0`.
- `INSERT ... VALUES` and `INSERT ... SET` accept `TRUE` and `FALSE` for
  integer and `BOOL` / `BOOLEAN` columns, producing integer stored values.
- `UPDATE ... SET col = TRUE` and `UPDATE ... SET col = FALSE` accept the
  literals and report changed-row affected counts.
- `WHERE col = TRUE`, `WHERE col <=> FALSE`, and the existing comparison
  operators compare against integer `1` and `0`.
- `TRUE` and `FALSE` follow the target descriptor's normal integer assignment
  and nullability rules. `FALSE` is a valid `NOT NULL` integer assignment
  because it stores `0`.
- The literal names are case-insensitive.
- `LIMIT TRUE` and `LIMIT FALSE` are syntax errors in MySQL 8.4.9 for the
  probed `SELECT` form.
- MySQL accepts broader expression forms such as `+TRUE`, `-TRUE`, and
  arithmetic using boolean literals. Those remain outside this baseline slice.

The executable evidence script is
`packages/libmylite/tests/mysql_baseline_boolean_literals_expectations.sh`.

## Scope

The implementation must add:

- parser support for `TRUE` and `FALSE` in the current DML value grammar for
  `INSERT ... VALUES`, `INSERT ... SET`, and single-table `UPDATE`;
- parser support for `TRUE` and `FALSE` as comparison predicate right operands
  in the current one-column `WHERE` subset;
- analyzer/planner conversion of `TRUE` to integer `1` and `FALSE` to integer
  `0` before binding values to SQLite;
- descriptor-driven integer range validation for converted values using the
  same target column rules as decimal integer literals;
- existing nullability, unknown-column, unknown-table, schema-resolution,
  reserved-name, ordering, limit, affected-row, warning-count, persistence, and
  file-format behavior;
- deterministic rejection for boolean literals in unsupported grammar
  positions such as `LIMIT TRUE`, unary `+TRUE`, unary `-FALSE`, boolean
  composition, expression assignment, and `IS TRUE`.

## Non-Goals

This feature must not implement:

- general expression truth semantics;
- `WHERE TRUE`, `WHERE FALSE`, `WHERE col`, `WHERE col IS TRUE`, or
  `WHERE col IS FALSE`;
- unary `+` or `-` applied to boolean literals;
- arithmetic, bitwise, logical, comparison-left, function, cast, parameter, or
  subquery expression evaluation using boolean literals;
- boolean literals in `LIMIT`, `OFFSET`, diagnostics `SHOW ... LIMIT`, or DDL
  display-width syntax;
- defaults, generated columns, check constraints, indexes, triggers, cascades,
  foreign keys, privileges, protocol-grade boolean metadata, compact storage,
  or SQLite fork changes.

## Ownership Boundary

- The public API remains unchanged. `mylite_execute()` continues to expose the
  existing DML and result conventions.
- Statement context owns diagnostics snapshots. Supported boolean literal DML
  produces `warning_count == 0`.
- Lexer/parser/AST own syntax admission and source spans. They represent
  boolean literals as literal AST nodes and do not consult descriptors,
  catalog, storage, or SQLite.
- Analyzer/planner owns descriptor lookup and conversion. It maps `TRUE` /
  `FALSE` to the same planned integer value representation used by decimal
  integer literals before binding.
- Catalog descriptors remain authoritative for column type, nullability,
  range, and physical column names. This feature does not add descriptor
  fields or mutate catalog generations.
- Result builders continue to render stored integer values as text and SQL
  `NULL` values as null public values.
- Storage/VFS remains responsible for the `.mylite` preamble and shifted
  SQLite payload. Boolean literals require no file-format change.
- SQLite remains the physical row storage and execution engine. MyLite
  continues to build descriptor-driven SQLite SQL with quoted identifiers and
  bound parameters; it does not materialize table rows to evaluate boolean
  literal predicates or assignments.

## Supported SQL Grammar

Supported value forms:

```sql
TRUE
FALSE
true
false
```

Supported examples:

```sql
INSERT INTO t VALUES (TRUE, FALSE)
INSERT INTO t SET a = TRUE, b = FALSE
UPDATE t SET a = TRUE
SELECT * FROM t WHERE a = FALSE
DELETE FROM t WHERE a <=> TRUE
UPDATE t SET b = FALSE WHERE a >= TRUE
```

Unsupported examples:

```sql
INSERT INTO t VALUES (+TRUE)
UPDATE t SET a = -FALSE
UPDATE t SET a = TRUE + 1
SELECT * FROM t WHERE TRUE
SELECT * FROM t WHERE a IS TRUE
SELECT * FROM t LIMIT TRUE
UPDATE t SET a = 1 LIMIT FALSE
```

### MyLite Lemon-Syntax Snippet

This snippet describes MyLite's intended grammar extension, not MySQL's full
grammar:

```lemon
insert_value ::= INTEGER.
insert_value ::= PLUS INTEGER.
insert_value ::= MINUS INTEGER.
insert_value ::= NULL.
insert_value ::= boolean_literal.

update_value ::= INTEGER.
update_value ::= PLUS INTEGER.
update_value ::= MINUS INTEGER.
update_value ::= NULL.
update_value ::= boolean_literal.

predicate_integer_value ::= INTEGER.
predicate_integer_value ::= PLUS INTEGER.
predicate_integer_value ::= MINUS INTEGER.
predicate_integer_value ::= boolean_literal.

boolean_literal ::= TRUE.
boolean_literal ::= FALSE.
```

`limit_integer` remains `INTEGER` only.

## Conversion Semantics

For supported DML value and predicate positions:

| Literal | Planned integer value |
| --- | --- |
| `TRUE` | `1` |
| `FALSE` | `0` |

The converted value then follows the same descriptor validation as decimal
integer literals:

| Logical type | `TRUE` | `FALSE` |
| --- | --- | --- |
| signed integer families | accepted as `1` | accepted as `0` |
| unsigned integer families | accepted as `1` | accepted as `0` |
| `BOOL` / `BOOLEAN` descriptors | accepted as signed `TINYINT(1)` value `1` | accepted as signed `TINYINT(1)` value `0` |

Boolean literals never produce `NULL`. Assigning `FALSE` to a `NOT NULL`
integer column succeeds because it is the integer value `0`. Existing `NULL`
handling is unchanged.

Supported comparison predicates convert the right operand first, then reuse the
current descriptor-driven predicate behavior:

| Predicate | Behavior |
| --- | --- |
| `col = TRUE` | same as `col = 1` |
| `col <=> TRUE` | same as `col <=> 1`; `NULL` column values do not match |
| `col <> FALSE`, `col != FALSE` | same as comparison against `0` |
| range comparisons with `TRUE` / `FALSE` | same as range comparison against `1` / `0` |

## Physical SQLite Handling

No SQLite fork patch is required. MyLite converts supported boolean literals to
planned integer values before generating physical SQL. Generated SQL continues
to use stable MyLite physical table names, quoted identifiers, and bound
parameters:

```sql
INSERT INTO "<physical_name>" ("<col>") VALUES (?1)
UPDATE "<physical_name>" SET "<col>" = ?1 ...
SELECT "<col>" FROM "<physical_name>" WHERE "<col>" = ?1
```

SQLite receives `sqlite3_bind_int64(..., 1)` for `TRUE` and
`sqlite3_bind_int64(..., 0)` for `FALSE`.

## Result And Reporting Behavior

Successful supported statements report through existing conventions:

- `INSERT` returns an empty DML result with affected rows equal to inserted
  rows;
- `UPDATE` returns an empty DML result with MySQL-compatible changed-row
  affected rows;
- `DELETE` returns an empty DML result with affected rows equal to deleted
  rows;
- `SELECT` returns the selected stored integer/`NULL` row values;
- `warning_count == 0` for supported in-range boolean-literal statements.

## Diagnostics

Supported boolean literals should not introduce new diagnostics.

Unsupported shapes use existing deterministic parser or unsupported diagnostics:

| Condition | Result |
| --- | --- |
| Boolean literal in unsupported grammar position | syntax error or existing unsupported-shape diagnostic |
| `WHERE TRUE`, `WHERE FALSE`, `IS TRUE`, `IS FALSE` | unsupported predicate syntax |
| `+TRUE`, `-TRUE`, arithmetic or column-to-column assignment | unsupported expression assignment/value syntax |
| Boolean literal in `LIMIT` / `OFFSET` | syntax error or existing `LIMIT` literal diagnostic |
| Unknown assignment, predicate, or ordering column | existing unknown-column diagnostics |
| Unknown schema/table or missing default schema | existing schema/table diagnostics |
| Reserved `_mylite_*` target names | existing reserved-name diagnostics |
| Physical SQLite failure or allocation failure | existing internal/nomem diagnostics |

## Compatibility Documentation

Update:

- `COMPATIBILITY.md`;
- `docs/compatibility/type-system-literals-conversion.md`;
- `docs/compatibility/sql-table-dml.md`;
- `docs/compatibility/sql-query-expressions.md`;
- `docs/compatibility/operators.md`.

Do not overclaim full boolean expression support, `IS TRUE`, boolean operators,
boolean literals in `LIMIT`, scalar expression coverage, protocol-grade
metadata, or broader type conversion.

## Test Plan

Implementation tests must cover:

- parser acceptance for `TRUE` / `FALSE` in `INSERT ... VALUES`,
  `INSERT ... SET`, `UPDATE ... SET`, and comparison predicate right operands;
- parser or runtime rejection for `LIMIT TRUE`, `+TRUE`, `-FALSE`,
  `WHERE TRUE`, `IS TRUE`, expression assignments, and boolean literals in
  unsupported contexts;
- successful `INSERT ... VALUES` and `INSERT ... SET` into integer and
  `BOOL` / `BOOLEAN` descriptors;
- successful `UPDATE` assignments to `TRUE` and `FALSE`, including no-op
  changed-row behavior;
- filtered `SELECT`, `DELETE`, and `UPDATE` predicates using `TRUE` and
  `FALSE` with `=`, `<=>`, `<>`, `!=`, `<`, `<=`, `>`, and `>=` where the
  existing predicate subset admits the operator;
- case-insensitive keyword spelling through the lexer;
- `FALSE` into `NOT NULL` columns;
- `TRUE` / `FALSE` ordering and limited update/delete behavior through stored
  integer values rather than new order semantics;
- reopen persistence and independent file-backed handles;
- no catalog generation, descriptor, SQLite schema generation, or preamble
  mutation beyond expected user-row writes;
- the MySQL expectation script above, focused parser/runtime CTest entries,
  and the full check workflow.
