# Baseline INSERT Duplicate Multiple Assignments

## Summary

This phase extends the existing descriptor-driven
`INSERT ... ON DUPLICATE KEY UPDATE` baseline from one duplicate-update
assignment to a deliberately small multiple-assignment subset:

```sql
INSERT ... VALUES ...
ON DUPLICATE KEY UPDATE column_name = duplicate_value,
                        column_name = duplicate_value [, ...]

INSERT ... SET ...
ON DUPLICATE KEY UPDATE column_name = duplicate_value,
                        column_name = duplicate_value [, ...]
```

The intent is to cover common application upsert tails such as updating several
ordinary row columns together while preserving the current single-statement,
descriptor-built SQLite execution path. MyLite still owns name resolution,
value conversion, duplicate-key selection, affected-row accounting, and
diagnostics. SQLite remains the physical row store.

## Sources And Evidence

- MyLite architecture and standards:
  - `README.md`
  - `AGENTS.md`
  - `docs/architecture/engineering-standards.md`
- Existing feature specs:
  - `docs/specs/baseline-insert-on-duplicate-key-update/specs.md`
  - `docs/specs/baseline-update-multiple-assignments/specs.md`
  - `docs/specs/baseline-row-values-lifecycle/specs.md`
  - `docs/specs/baseline-primary-key-lifecycle/specs.md`
  - `docs/specs/baseline-unique-index-lifecycle/specs.md`
  - `docs/specs/baseline-auto-increment-lifecycle/specs.md`
- Official MySQL 8.4 Reference Manual:
  - `INSERT`: <https://dev.mysql.com/doc/refman/8.4/en/insert.html>
  - `INSERT ... ON DUPLICATE KEY UPDATE`:
    <https://dev.mysql.com/doc/refman/8.4/en/insert-on-duplicate.html>
- MySQL runtime probes are captured in
  `packages/libmylite/tests/mysql_baseline_insert_duplicate_multiple_assignments_expectations.sh`
  and verified against MySQL 8.4.9.

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior, public
SQLite APIs, and existing MyLite source code. It does not copy MySQL, MariaDB,
Percona, SQLite implementation internals, or other restrictively licensed
implementation sources.

## MySQL 8.4.9 Runtime Observations

MySQL 8.4.9 establishes these expectations for this expansion:

- A duplicate update with two assignments reports affected rows `2` when at
  least one assignment changes the row.
- If every duplicate assignment stores the current value, affected rows are
  `0`.
- If one assignment is a no-op and another changes the row, affected rows are
  `2`.
- Multiple direct `VALUES(column)` references each record warning `1287`.
  Two `VALUES()` references in one duplicate-update tail produce two warnings.
- `VALUES()` warnings are recorded even when the table has no enforced key and
  the statement inserts normally.
- Multiple duplicate assignments in `INSERT ... SET` follow the same affected
  row and warning-count behavior as `INSERT ... VALUES`.
- `DEFAULT` and `NULL` assignment semantics match the existing
  single-assignment duplicate-update path.
- If any duplicate assignment conversion fails, the duplicate branch is atomic:
  earlier assignments in that branch do not persist.
- MySQL accepts duplicate assignment targets and evaluates assignments
  left-to-right. Later assignments can observe earlier assignment effects.
  This phase does not implement that expression model, so duplicate targets are
  rejected deterministically by MyLite.

## Scope

The implementation supports:

- existing supported `INSERT ... VALUES` and one-row `INSERT ... SET` sources;
- two or more duplicate-update assignments;
- persistent base tables already admitted by the current ODKU path;
- distinct unqualified assignment targets only;
- non-key assignment targets only;
- the existing single-assignment duplicate-update value subset for each
  assignment: descriptor-compatible insert literals, `NULL`, `TRUE`, `FALSE`,
  `DEFAULT`, zero-fractional `CURRENT_TIMESTAMP` / `NOW()` value forms where
  already supported by target conversion, and direct descriptor-compatible
  `VALUES(column_name)` as expanded by
  [baseline ODKU VALUES cross-column references](../baseline-odku-values-cross-column/specs.md);
- the current one enforced single-column primary-key or single unique-index
  conflict subset;
- tables without enforced keys, where rows insert normally but duplicate-tail
  names and `VALUES()` warnings are still planned;
- MySQL-style affected rows for inserted rows, changed duplicate rows, and
  unchanged duplicate rows;
- one warning `1287` for each supported `VALUES(column_name)` duplicate value;
- existing `DELAYED` warning behavior and `LOW_PRIORITY` / `HIGH_PRIORITY`
  no-op behavior;
- all-or-nothing statement behavior inside the existing statement transaction;
- descriptor-built physical SQLite `UPDATE` statements with quoted identifiers
  and bound values.

## Non-Goals

This feature does not add:

- `INSERT IGNORE ... ON DUPLICATE KEY UPDATE`;
- `INSERT ... SELECT`, `TABLE`, CTE, alias, partition, joined, or subquery
  forms;
- duplicate assignment targets;
- table-qualified assignment targets or table-qualified `VALUES()` arguments;
- duplicate assignment to primary-key, unique-key, or auto-increment columns;
- duplicate updates on multiple enforced keys or composite keys;
- expression assignments, column-to-column assignments, arithmetic
  assignments, functions other than the existing current-timestamp value forms,
  variables, parameters, casts, collations, or `DEFAULT(column_name)`;
- MySQL's left-to-right table-backed duplicate assignment expression model;
- triggers, cascades, generated columns, privilege semantics, protocol info
  strings, optimizer/index-use guarantees, or SQLite fork patches.

Single-assignment duplicate-update behavior remains owned by
`baseline-insert-on-duplicate-key-update`.

## Ownership Boundaries

- Public API: unchanged. `mylite_execute()` owns public argument validation,
  result-handle ownership, and public misuse behavior.
- Statement context: unchanged. It owns diagnostics, warnings, affected rows,
  transaction completion, and the non-row result shape.
- Parser/AST: already admits comma-separated duplicate assignments and stores
  assignment targets and value nodes independently. It does not resolve names
  or enforce descriptor compatibility.
- Analyzer/planner: resolves target table, insert columns, duplicate
  assignment targets, `VALUES(column)` references, key participation, and
  duplicate-key eligibility from MyLite descriptors before SQLite SQL exists.
- Catalog: remains authoritative for schemas, table identity, physical names,
  columns, defaults, keys, and auto-increment metadata. This DML feature does
  not mutate catalog rows, descriptor versions, descriptor caches, catalog
  generation, or `sqlite_schema_generation`.
- Result builder: successful statements return the existing non-row result with
  zero columns, zero result rows, MySQL-compatible affected rows for this
  subset, and the accumulated warning count.
- SQLite physical storage: owns b-tree mutation for generated user tables.
  MyLite supplies stable physical table names, quoted column names, and bound
  assignment/key values.
- Storage/VFS/file format: unchanged. Duplicate updates write only inside the
  shifted SQLite payload and must not touch the `.mylite` preamble.

## Supported SQL

The outer statement remains the existing limited ODKU form:

```sql
INSERT [LOW_PRIORITY | HIGH_PRIORITY | DELAYED] [INTO] table_name
    [(column_name[, ...])]
    VALUES row_value[, ...]
ON DUPLICATE KEY UPDATE duplicate_assignment,
                        duplicate_assignment [, ...]

INSERT [LOW_PRIORITY | HIGH_PRIORITY | DELAYED] [INTO] table_name
    SET insert_assignment[, ...]
ON DUPLICATE KEY UPDATE duplicate_assignment,
                        duplicate_assignment [, ...]
```

Runtime narrows the parsed shape:

```sql
duplicate_assignment:
    column_name = duplicate_update_value

duplicate_update_value:
    supported_insert_value
  | DEFAULT
  | VALUES ( column_name )
```

MyLite Lemon-syntax sketch, reflecting existing parser support:

```lemon
on_duplicate_key_update_opt ::= .
on_duplicate_key_update_opt ::= ON DUPLICATE KEY UPDATE duplicate_assignment_list.

duplicate_assignment_list ::= duplicate_assignment.
duplicate_assignment_list ::= duplicate_assignment_list COMMA duplicate_assignment.

duplicate_assignment ::= qualified_identifier EQUAL duplicate_update_value.

duplicate_update_value ::= insert_value.
duplicate_update_value ::= VALUES LPAREN qualified_identifier RPAREN.
```

The parser admits qualified identifiers in order to produce deterministic
runtime diagnostics. This phase supports only unqualified assignment targets and
unqualified direct `VALUES()` references.

## Resolution And Semantics

Schema and target-table resolution follow the current `INSERT` policy:

- unqualified target names use the selected schema;
- schema-qualified target names use the named schema without requiring a
  selected schema;
- reserved `_mylite_*` names are rejected before generated SQLite SQL;
- unknown schema, unknown table, unsupported object kind, and missing default
  schema diagnostics reuse existing `INSERT` behavior.

Duplicate assignment resolution is descriptor-driven:

- every assignment target resolves through the target table descriptor;
- assignment targets may explicitly name invisible descriptor columns;
- assignment targets must be unqualified;
- each target column may appear at most once;
- targets participating in the supported enforced key, and auto-increment
  targets, are rejected for this phase;
- unknown assignment columns fail before mutation.

`VALUES(column_name)` resolution:

- resolves through the target table descriptor;
- uses the fully planned proposed value for the current insert row, after
  column-list mapping, default filling, generated auto-increment values, and
  insert conversion;
- is supported only when the referenced column has the same logical and
  physical storage descriptor as the assignment target; character-string
  descriptors must also have the same character set and collation;
- may explicitly name invisible descriptor columns when unqualified;
- records warning `1287` once per supported `VALUES()` assignment value;
- unknown or qualified `VALUES()` references fail before mutation.

Duplicate-key resolution remains the current narrow model:

- no enforced key descriptors: rows insert normally, but duplicate-tail names
  and `VALUES()` warnings still apply;
- exactly one single-column primary or unique key: duplicate rows update the
  existing conflicting row;
- multiple enforced keys, composite keys, unsupported key descriptors, or key
  assignment targets are rejected before mutation.

For each planned insert row:

1. MyLite attempts the ordinary insert using the existing prepared SQLite
   statement and bound planned row values.
2. Successful inserts add `1` affected row and update generated
   auto-increment/`LAST_INSERT_ID()` state exactly as the existing insert path.
3. Duplicate-key failures in the supported key subset are routed to one
   descriptor-built physical `UPDATE`.
4. MyLite converts or copies every duplicate assignment value before preparing
   that physical update. Any conversion error aborts the statement before
   partial duplicate-row mutation.
5. The generated update sets all duplicate assignment columns and includes a
   changed-row filter that is an `OR` of per-assignment comparisons. If the
   update physically changes the conflicting row, the statement adds `2`
   affected rows. If all assignment values are no-ops after canonicalization,
   it adds `0`.
6. Any physical SQLite failure, duplicate-key failure, allocation failure, or
   foreign-key failure rolls back the full statement.

Later rows in a multi-row statement see earlier successful inserts and
duplicate updates from the same statement.

## Generated SQLite SQL Shape

For two assignments and a one-column key, MyLite generates standard SQLite
update SQL of this shape:

```sql
UPDATE "_mylite_user_table_<table_id>"
SET "col_a" = ?1, "col_b" = ?2
WHERE "key_col" = ?3
  AND (("col_a" IS NULL OR "col_a" <> ?4)
       OR ("col_b" IS NULL OR "col_b" <> ?5))
```

For `NULL` assignment values, the changed predicate is `column IS NOT NULL` and
does not need a comparison parameter. For non-`NULL` values, the comparison
parameter is bound separately from the `SET` parameter so SQLite cannot reuse
or reinterpret SQL literals. Every generated identifier is quoted, and every
data value is bound with prepared-statement parameters.

This feature uses MyLite-side translation and public SQLite prepared-statement
APIs. It does not require a SQLite fork patch.

## Diagnostics

Existing single-assignment diagnostics are preserved where possible:

- syntax errors and unsupported parsed shapes: deterministic parse or
  unsupported diagnostics;
- `INSERT IGNORE ... ON DUPLICATE KEY UPDATE`: existing unsupported diagnostic;
- missing default schema, unknown schema, unknown table, reserved target names,
  and unsupported object kind: existing insert diagnostics;
- unknown assignment or `VALUES()` column: MySQL-compatible unknown-column
  diagnostic;
- qualified assignment target or qualified `VALUES()` argument: unsupported
  diagnostic;
- duplicate assignment targets: MyLite-specific unsupported diagnostic until
  left-to-right expression semantics are implemented;
- multiple/composite enforced keys: existing unsupported diagnostics;
- key-column or auto-increment assignment targets: unsupported diagnostic;
- `NULL` into `NOT NULL`, missing explicit default, range errors, overlength
  values, invalid temporal/JSON/enum/set values, and unsupported conversions:
  current descriptor conversion diagnostics;
- duplicate-key conflict caused by the physical update: current duplicate-key
  diagnostic if reachable in supported single-key paths;
- physical SQLite, allocation, and public API misuse failures: existing
  runtime diagnostics.

Supported in-range duplicate updates produce no new warnings other than
existing `DELAYED` warning `3005` and one `VALUES()` deprecation warning `1287`
per admitted `VALUES(column_name)` assignment value.

## Tests

The MySQL expectation script records:

- changed and no-op multi-assignment duplicate updates;
- mixed no-op plus changed assignments;
- warning count and `SHOW WARNINGS` rows for multiple `VALUES()` references;
- `INSERT ... SET` with multiple duplicate assignments;
- no-key tables inserting normally while still recording `VALUES()` warnings;
- `DEFAULT` plus `NULL` assignment behavior;
- duplicate branch atomicity on a later assignment conversion error;
- MySQL's accepted duplicate-target, left-to-right behavior that MyLite
  intentionally defers.

The C runtime tests must cover:

- primary-key and unique-key duplicate updates with multiple assignments;
- multi-row row-by-row behavior and affected-row totals;
- direct `VALUES()` references with repeated warning `1287`;
- literal, `DEFAULT`, `NULL`, string, temporal, and current-timestamp values
  where the existing single-assignment ODKU conversion already supports them;
- no-key table behavior;
- unknown second assignment columns, unknown second `VALUES()` columns,
  duplicate targets, key/auto-increment target rejection, qualified targets,
  qualified `VALUES()` references, and unsupported expressions;
- conversion failure atomicity;
- persistence after close/reopen, independent file-backed handles, `.mylite`
  preamble preservation, non-row result shape, affected rows, and warning
  counts.

Existing parser, insert, insert-set, ODKU, key, update, row-values,
auto-increment, diagnostics, file-backed storage, VFS, and full workflow tests
must continue to pass.
