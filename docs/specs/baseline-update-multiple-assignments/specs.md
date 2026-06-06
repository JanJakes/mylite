# Baseline UPDATE Multiple Assignments

## Summary

This phase extends the descriptor-driven single-table `UPDATE` path from one
assignment to a deliberately small multiple-assignment subset:

```sql
UPDATE table_name
SET column_name = value, column_name = value [, ...]
[WHERE ...]
[ORDER BY ...]
[LIMIT row_count]
```

The slice is intended for common application updates that set several ordinary
row columns together. It keeps the current efficient execution model: MyLite
resolves names and converts admitted scalar values from catalog descriptors,
then generates one physical SQLite `UPDATE` with quoted identifiers and bound
parameters. It does not materialize matched rows in MyLite memory.

## Sources And Evidence

- MyLite architecture and engineering standards:
  - `README.md`
  - `docs/architecture/engineering-standards.md`
- Existing feature specs:
  - `docs/specs/baseline-update-lifecycle/specs.md`
  - `docs/specs/baseline-update-scalar-subquery-assignment/specs.md`
  - `docs/specs/baseline-update-arithmetic-assignment/specs.md`
  - `docs/specs/baseline-dml-default-keyword-values/specs.md`
  - `docs/specs/baseline-timestamp-type/specs.md`
- Official MySQL 8.4 Reference Manual:
  - `UPDATE`: <https://dev.mysql.com/doc/refman/8.4/en/update.html>
  - integer type ranges:
    <https://dev.mysql.com/doc/refman/8.4/en/integer-types.html>
  - out-of-range and overflow handling:
    <https://dev.mysql.com/doc/refman/8.4/en/out-of-range-and-overflow.html>
- MySQL runtime probes are captured in
  `packages/libmylite/tests/mysql_baseline_update_multiple_assignments_expectations.sh`
  and verified against MySQL 8.4.9.

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior, public
SQLite APIs, and existing MyLite source code. It does not copy MySQL, MariaDB,
Percona, SQLite implementation internals, or other restrictively licensed
implementation sources.

## MySQL Runtime Observations

MySQL 8.4.9 establishes these expectations for the admitted subset:

- `ROW_COUNT()` reports changed rows, not matched rows. If all assignments are
  no-ops for a row, the row is not counted.
- If at least one assignment changes a row and another assignment is a no-op,
  the row is counted once.
- `NULL` assignment follows target nullability. `NULL` into a nullable column
  changes a non-`NULL` value and is unchanged for an already `NULL` value.
- `DEFAULT` assignment uses the descriptor default and participates in changed
  row detection after value canonicalization.
- `CURRENT_TIMESTAMP` assignment to a `TIMESTAMP` or `DATETIME` target uses the
  statement timestamp and participates in changed-row detection.
- Automatic `ON UPDATE CURRENT_TIMESTAMP` columns change only when another
  explicit assignment changes the row. An explicit assignment to the same
  timestamp column suppresses the automatic update for that column.
- Single-table multiple assignments are evaluated left to right. MySQL accepts
  duplicate assignment targets and the later assignment wins. This phase does
  not implement that left-to-right expression model, so duplicate assignment
  targets are rejected deterministically.
- No-match updates and `LIMIT 0` skip runtime value conversion, including
  `NULL` into `NOT NULL` and oversized numeric literals.
- `ORDER BY ... LIMIT` restricts matched rows before changed-row filtering.
  Rows selected by the limit that receive only no-op values are not counted,
  and rows outside the limit are not changed.
- Supported in-range updates report `@@warning_count = 0`.

## Scope

The implementation supports:

- persistent base tables and shadowing session temporary base tables already
  admitted by the current single-table `UPDATE` resolver;
- unqualified and schema-qualified target table names using the existing
  selected-schema policy;
- two or more assignment list entries;
- distinct unqualified assignment targets only;
- assignment to primary-key and unique-key columns, including supported
  `AUTO_INCREMENT` key columns, with duplicate-key diagnostics for the current
  supported primary-key and unique-index subset;
- assignment values admitted by the existing single-assignment constant
  conversion path: decimal integer, fixed decimal, approximate numeric,
  `TRUE`, `FALSE`, `NULL`, `DEFAULT`, ordinary string literals for supported
  text and temporal targets, ordinary string or hex literals for binary-string
  targets, compatible bit literals, limited `ENUM`, limited `SET`, limited
  `JSON`, and zero-fractional `CURRENT_TIMESTAMP` / `NOW()` synonyms for
  `DATETIME` and `TIMESTAMP` targets;
- existing `WHERE`, `ORDER BY`, and `LIMIT row_count` subsets from the current
  `UPDATE` path;
- changed-row `affected_rows` semantics for any changed explicit assignment;
- automatic `ON UPDATE CURRENT_TIMESTAMP` columns when at least one explicit
  assignment changes a row and the timestamp column is not explicitly assigned;
- descriptor-driven generated SQLite `UPDATE` with one physical statement,
  quoted identifiers, and bound parameters.

## Non-Goals

This feature does not add:

- table-qualified assignment targets, aliases, partitions, modifiers, joined
  updates, multi-table updates, CTEs, correlated subqueries, subquery
  predicates, or derived tables;
- duplicate assignment targets;
- scalar subquery assignments in a multi-assignment list;
- arithmetic assignments in a multi-assignment list;
- column-to-column assignments, general expressions, functions other than the
  existing current-timestamp value forms, variables, parameters, or
  `DEFAULT(column_name)`;
- arithmetic or expression assignment to auto-increment columns inside a
  multi-assignment list;
- automatic timestamp updates for key columns;
- parent foreign-key cascade/set-null behavior beyond the existing direct
  single-assignment parent-update action subset;
- MySQL's left-to-right table-backed expression evaluation;
- triggers, cascades, generated columns, privileges, locks, or SQLite fork
  patches.

Scalar subquery assignment and same-column integer arithmetic remain owned by
their existing phases and continue to use their existing paths.

## Ownership Boundaries

- Public API: unchanged. `mylite_execute()` owns public argument validation,
  result-handle ownership, and public misuse behavior.
- Statement context: unchanged. It owns diagnostics reset, warnings,
  transaction completion, and the public non-row result shape.
- Lexer/parser/AST: already admits comma-separated `UPDATE` assignments and
  stores each assignment independently. It does not resolve names or enforce
  descriptor compatibility.
- Analyzer/planner: resolves target schema/table, assignment targets, predicate
  columns, order columns, key participation, auto-increment attributes, and
  automatic timestamp columns from MyLite descriptors before SQLite SQL exists.
- Catalog: remains authoritative for schemas, table identity, object kind,
  physical names, columns, indexes, defaults, and auto-increment metadata.
  Multiple-assignment `UPDATE` does not mutate catalog rows, descriptor
  versions, descriptor caches, catalog generation, or
  `sqlite_schema_generation`.
- Result builder: successful statements return the existing non-row result with
  zero columns, zero result rows, exact changed-row `affected_rows`, and
  `warning_count == 0`.
- SQLite physical storage: owns b-tree mutation inside generated MyLite user
  tables. MyLite supplies stable physical table names, quoted column names, and
  bound values.
- Storage/VFS/file format: unchanged. Updates write only inside the shifted
  SQLite payload and must not touch the `.mylite` preamble.

## Supported SQL

The outer statement remains the current limited single-table form:

```sql
UPDATE table_name
SET update_assignment, update_assignment [, ...]
[WHERE baseline_predicate]
[ORDER BY order_column [ASC | DESC]]
[LIMIT row_count]
```

The semantic subset for this phase is:

```sql
update_assignment:
    column_name = update_constant_value

update_constant_value:
    numeric_literal
  | string_literal
  | hex_literal
  | bit_literal
  | TRUE
  | FALSE
  | NULL
  | DEFAULT
  | CURRENT_TIMESTAMP
  | CURRENT_TIMESTAMP()
  | NOW()
```

MyLite Lemon-syntax sketch, reflecting existing parser support:

```lemon
update_statement ::=
    UPDATE table_name SET update_assignment_list
    where_clause_opt order_by_clause_opt limit_clause_opt.

update_assignment_list ::= update_assignment.
update_assignment_list ::= update_assignment_list COMMA update_assignment.

update_assignment ::= qualified_identifier EQUAL update_value.
```

Runtime narrows this parsed shape:

- each assignment target must be an unqualified descriptor column;
- each target column may appear at most once;
- `AUTO_INCREMENT` targets use the existing descriptor conversion path, and
  positive changed values advance the descriptor-owned next counter just like
  supported single-assignment `UPDATE`;
- each value must be one of the existing single-assignment constant value
  forms. Multi-assignment scalar subqueries and arithmetic expressions are
  rejected even though their single-assignment forms remain supported.

## Semantics

Schema and object resolution follow the existing `UPDATE` policy:

- unqualified target names use the selected schema;
- schema-qualified target names use the named schema without requiring a
  selected schema;
- reserved `_mylite_*` schema and table names are rejected before SQLite SQL is
  generated;
- unknown schema, unknown table, unsupported object kind, and missing default
  schema diagnostics reuse the existing `UPDATE` behavior.

Column resolution is descriptor-driven:

- assignment, predicate, and ordering columns resolve through MyLite
  descriptors, not SQLite metadata;
- the current descriptor catalog's identifier matching policy remains the
  source of truth for case behavior;
- unknown assignment, predicate, and ordering columns fail before physical
  mutation;
- duplicate assignment target names fail with a deterministic MyLite-specific
  unsupported diagnostic until left-to-right expression semantics are
  implemented.

Assignment conversion:

- values are converted only after the matched-row check confirms at least one
  row can be considered and `LIMIT` is not zero;
- conversion reuses the same descriptor-owned target conversion as
  single-assignment `UPDATE`;
- converted values are bound to SQLite as `NULL`, integer, text, or blob
  according to `struct planned_value`;
- `NULL` into `NOT NULL`, range errors, invalid literals, and unsupported
  conversions abort the statement before any physical mutation;
- successful supported conversions produce no warnings.

Changed-row semantics:

- the generated physical `WHERE` includes the matched-row predicate plus an
  explicit changed-row filter;
- the changed-row filter is an `OR` over all explicit assignments;
- a nullable assignment compares as `column IS NOT NULL`;
- a non-`NULL` assignment compares as `column IS NULL OR column <> ?`;
- automatic `ON UPDATE CURRENT_TIMESTAMP` columns are not part of the changed
  filter, so they do not turn all-no-op explicit assignments into changed rows.

`ORDER BY ... LIMIT` continues to use the existing rowid-subquery shape rather
than SQLite's optional `UPDATE ... ORDER BY ... LIMIT` syntax. The limit applies
to matched rows before the changed-row filter. For duplicate order-key ties,
this phase claims only the updated count and value constraints verified against
MySQL 8.4.9, not a deterministic tied-row choice.

## Generated SQLite SQL Shape

For two explicit assignments and one automatic timestamp column, the generated
shape is:

```sql
UPDATE "_mylite_user_table_<table_id>"
SET "col1" = ?1, "col2" = ?2, "ts" = ?3
WHERE <descriptor predicate or rowid-limited subquery>
  AND (("col1" IS NULL OR "col1" <> ?N)
       OR ("col2" IS NULL OR "col2" <> ?N))
```

All generated identifiers are quoted. All explicit assignment values, automatic
timestamp values, predicate values, limit values, and changed-row comparison
values are bound parameters. Parameter order is deterministic:

1. explicit assignment values in SQL assignment-list order;
2. automatic `ON UPDATE CURRENT_TIMESTAMP` values in descriptor column order;
3. predicate values in existing predicate traversal order;
4. limit row count, if present;
5. changed-row comparison values in SQL assignment-list order for non-`NULL`
   explicit assignments.

No SQLite fork patch is required. This phase uses public SQLite prepare, bind,
step, and transaction APIs plus MyLite-side SQL generation.

## Diagnostics

The feature reuses existing diagnostics for:

- syntax errors and unsupported outer `UPDATE` clauses;
- missing default schema, unknown schema, unknown table, reserved names, and
  unsupported object kinds;
- unknown assignment, predicate, or ordering columns;
- unsupported literal or conversion for the target descriptor;
- integer, decimal, approximate, temporal, string, binary, `BIT`, `ENUM`,
  `SET`, and `JSON` conversion errors;
- `NULL` into `NOT NULL`;
- key, foreign-key, physical SQLite, allocation, and public API failures.

The feature adds deterministic MyLite-specific unsupported diagnostics for:

- duplicate assignment targets in a multi-assignment list;
- scalar subquery assignment inside a multi-assignment list;
- same-column arithmetic assignment inside a multi-assignment list;
- assignment to unsupported primary-key, unique-key, or auto-increment
  descriptor shapes inside a multi-assignment list;
- automatic `ON UPDATE CURRENT_TIMESTAMP` columns that would be updated while
  participating in a primary or unique key.

## Tests

Fast C tests live in
`packages/libmylite/tests/runtime_update_multiple_assignments_test.c` and are
registered as `libmylite.runtime.update_multiple_assignments`.

The MySQL expectation script
`packages/libmylite/tests/mysql_baseline_update_multiple_assignments_expectations.sh`
verifies MySQL 8.4.9 behavior for the user-visible subset before the MyLite
runtime expectations are asserted.

Coverage includes:

- full-table and filtered updates with multiple ordinary target columns;
- mixed no-op and changed assignments;
- all-no-op assignments with `affected_rows == 0`;
- `NULL`, `DEFAULT`, and current-timestamp assignment;
- `NULL` into `NOT NULL`, out-of-range literals, unknown columns, and statement
  atomicity;
- `WHERE`, `ORDER BY`, and `LIMIT` reuse, including `LIMIT 0`;
- duplicate assignment target rejection;
- arithmetic and scalar-subquery multi-assignment rejection;
- non-`AUTO_INCREMENT` key and unique-key multi-assignment plus duplicate-key
  rollback;
- auto-increment primary-key multi-assignment with next-counter advancement;
- automatic `ON UPDATE CURRENT_TIMESTAMP` behavior;
- reopen persistence and `.mylite` preamble preservation;
- independent file-backed handles.

## Compatibility

`COMPATIBILITY.md` and `docs/compatibility/sql-table-dml.md` mark
single-table `UPDATE` as still limited, but no longer one-assignment-only for
ordinary constant assignments. They continue to document the unsupported
left-to-right expression, duplicate target, unsupported auto-increment
descriptor shapes, scalar subquery, and arithmetic multi-assignment gaps.
