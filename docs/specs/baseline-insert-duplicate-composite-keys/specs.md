# Baseline INSERT Duplicate Composite Keys

## Summary

This phase extends the existing descriptor-driven
`INSERT ... ON DUPLICATE KEY UPDATE` path from one enforced single-column key to
one enforced composite primary or unique key. It keeps the existing duplicate
assignment surface: persistent base tables, `INSERT ... VALUES` and one-row
`INSERT ... SET`, distinct unqualified non-key assignment targets, compatible
literal/default values, and same-target `VALUES(column_name)`.

The goal is the smallest useful expansion for application upserts over
composite primary or unique keys. This phase does not implement MySQL's full
multiple-unique-key conflict selection or key-column reassignment semantics.

## Sources And Evidence

- MyLite architecture and standards:
  - `README.md`
  - `AGENTS.md`
  - `docs/architecture/engineering-standards.md`
- Existing MyLite feature specs:
  - `docs/specs/baseline-insert-on-duplicate-key-update/specs.md`
  - `docs/specs/baseline-insert-duplicate-multiple-assignments/specs.md`
  - `docs/specs/baseline-composite-primary-key-lifecycle/specs.md`
  - `docs/specs/baseline-composite-unique-indexes/specs.md`
  - `docs/specs/baseline-index-prefix-key-parts/specs.md`
  - `docs/specs/baseline-replace-key-lifecycle/specs.md`
- Official MySQL 8.4 Reference Manual:
  - `INSERT`: <https://dev.mysql.com/doc/refman/8.4/en/insert.html>
  - `INSERT ... ON DUPLICATE KEY UPDATE`:
    <https://dev.mysql.com/doc/refman/8.4/en/insert-on-duplicate.html>
  - Primary-key and unique-key constraints:
    <https://dev.mysql.com/doc/refman/8.4/en/constraint-primary-key.html>
- MySQL runtime probes are captured in
  `packages/libmylite/tests/mysql_baseline_insert_duplicate_composite_keys_expectations.sh`
  and verified against MySQL 8.4.9.

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior, public
SQLite APIs, and existing MyLite source code. It does not copy MySQL, MariaDB,
Percona, SQLite implementation internals, or other restrictively licensed
implementation sources.

## MySQL 8.4.9 Runtime Observations

MySQL 8.4.9 establishes these expectations for this expansion:

- A duplicate row matched by a composite primary key reports affected rows `2`
  when the duplicate assignment changes an existing non-key column.
- The same composite-key duplicate reports affected rows `0` when every
  duplicate assignment stores the row's existing values.
- Multi-row inserts process rows left to right. Later rows can duplicate and
  update rows inserted or updated earlier by the same statement.
- `VALUES(column_name)` in a composite-key duplicate branch behaves the same as
  in the current single-column-key slice and records warning `1287` once per
  supported `VALUES()` occurrence in the statement.
- A composite unique key does not conflict when any unique-key part in the
  proposed row is SQL `NULL`; such rows insert normally. A non-`NULL` tuple
  conflict updates the existing row.
- MySQL accepts duplicate updates on tables with multiple enforced keys and
  accepts assignments to key columns when the resulting row does not violate a
  key. MyLite defers those wider behaviors in this phase because they require
  explicit conflict-selection and second-order conflict semantics.

## Scope

The implementation supports:

- existing supported `INSERT ... VALUES` and one-row `INSERT ... SET` sources;
- one enforced key descriptor on the target table, where that key may be:
  - a single-column primary key or unique key already supported today;
  - a composite primary key over supported key-part descriptors;
  - a composite unique key over supported key-part descriptors;
  - a supported prefix unique key, including composite prefix key parts, using
    the descriptor key expression already used for insertion and replacement;
- no enforced key descriptors, preserving current insert-normal behavior while
  still validating duplicate-tail names and warnings;
- distinct unqualified duplicate assignment targets that do not participate in
  the enforced key and are not the auto-increment column;
- the existing duplicate assignment value subset for each target: compatible
  row-value literals, `NULL`, `TRUE`, `FALSE`, `DEFAULT`, admitted
  current-timestamp value forms, and same-target `VALUES(column_name)`;
- MySQL-compatible affected rows for inserted, changed duplicate, and
  unchanged duplicate rows;
- one warning `1287` per admitted `VALUES(column_name)` duplicate assignment
  value, plus existing `DELAYED` warning behavior;
- all-or-nothing statement behavior inside the existing statement transaction;
- descriptor-built physical SQLite statements with quoted identifiers and bound
  values.

## Non-Goals

This feature does not add:

- `INSERT IGNORE ... ON DUPLICATE KEY UPDATE`;
- `INSERT ... SELECT`, `TABLE`, CTE, aliases, partitions, joined, or subquery
  forms;
- duplicate assignment targets;
- table-qualified assignment targets or table-qualified `VALUES()` arguments;
- cross-column `VALUES(column_name)` references;
- duplicate assignment to primary-key, unique-key, or auto-increment columns;
- duplicate updates on tables with more than one enforced key descriptor;
- MySQL's multiple-unique-key conflict-selection behavior;
- expression assignments, column-to-column assignments, arithmetic
  assignments, functions beyond already admitted value forms, variables,
  parameters, casts, collations, or broader `DEFAULT(column_name)` behavior;
- triggers, cascades, generated columns, privilege semantics, protocol info
  strings, optimizer/index-use guarantees, or SQLite fork patches.

## Ownership Boundaries

- Public API: unchanged. `mylite_execute()` owns public argument validation,
  result-handle ownership, and public misuse behavior.
- Statement context: unchanged. It owns diagnostics, warnings, affected rows,
  transaction completion, and non-row result shape.
- Parser/AST: unchanged. The current grammar already admits the supported
  duplicate-key tail and comma-separated assignment list.
- Analyzer/planner: resolves target table, insert columns, duplicate
  assignment targets, `VALUES(column_name)` references, enforced key
  eligibility, key-part participation, and auto-increment participation from
  MyLite descriptors before generated SQLite SQL exists.
- Catalog: remains authoritative for schemas, table identity, physical names,
  columns, defaults, primary keys, unique keys, prefix lengths, visibility, and
  auto-increment metadata. This DML feature does not mutate catalog rows,
  descriptor versions, descriptor caches, catalog generation, or
  `sqlite_schema_generation`.
- Result builder: successful statements return the existing non-row result with
  zero columns, zero result rows, MySQL-compatible affected rows for this
  subset, and the accumulated warning count.
- SQLite physical storage: owns b-tree mutation for generated user tables.
  MyLite supplies stable physical table names, quoted column/key expressions,
  and bound assignment/key values.
- Storage/VFS/file format: unchanged. Duplicate updates write only inside the
  shifted SQLite payload and must not touch the `.mylite` preamble.

## Supported SQL

No new grammar is required. This phase reuses the existing MyLite grammar:

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

The parser admits qualified identifiers so runtime can produce deterministic
unsupported diagnostics. This phase supports only unqualified duplicate
assignment targets and unqualified same-target `VALUES()` references.

## Resolution And Semantics

Schema and target-table resolution follow the current `INSERT` policy:

- unqualified target names use the selected schema;
- schema-qualified targets use the named schema without requiring a selected
  schema;
- reserved `_mylite_*` names are rejected before generated SQLite SQL;
- unknown schema, unknown table, unsupported object kind, and missing default
  schema diagnostics reuse existing `INSERT` behavior.

Duplicate assignment resolution remains descriptor-driven:

- every assignment target resolves through the target table descriptor;
- assignment targets may explicitly name invisible descriptor columns;
- assignment targets must be unqualified;
- each target column may appear at most once;
- targets participating in the enforced key, and auto-increment targets, are
  rejected for this phase;
- unknown assignment columns fail before mutation.

`VALUES(column_name)` resolution remains the current narrow model:

- resolves through the target table descriptor;
- uses the fully planned proposed value for the current insert row, after
  column-list mapping, default filling, generated auto-increment values, and
  insert conversion;
- is supported only when the referenced column is the same descriptor column as
  the assignment target;
- may explicitly name invisible descriptor columns when unqualified;
- records warning `1287` once per supported `VALUES()` assignment value;
- unknown or qualified `VALUES()` references fail before mutation.

Duplicate-key eligibility:

- no enforced key descriptors: rows insert normally, but duplicate-tail names
  and `VALUES()` warnings still apply;
- exactly one enforced primary or unique key descriptor: duplicate rows may
  update the existing conflicting row, regardless of whether that key has one
  part or several parts;
- composite unique keys with any proposed SQL `NULL` key part do not conflict
  and insert normally, matching MySQL's unique-key `NULL` behavior;
- more than one enforced key descriptor is rejected before mutation.

For each planned insert row:

1. MyLite attempts the ordinary insert using the existing prepared SQLite
   statement and bound planned row values.
2. Successful inserts add `1` affected row and update generated
   auto-increment/`LAST_INSERT_ID()` state exactly as the existing insert path.
3. Duplicate-key failures in the supported key subset are routed to one
   descriptor-built physical `UPDATE` identified by the conflicting key tuple.
4. MyLite converts or copies every duplicate assignment value before preparing
   that physical update. Any conversion error aborts the statement before
   partial duplicate-row mutation.
5. The generated update sets all duplicate assignment columns and includes a
   changed-row filter that is an `OR` of per-assignment comparisons. If the
   update physically changes the conflicting row, affected rows add `2`. If all
   assignment values are no-ops after canonicalization, affected rows add `0`.
6. Any physical SQLite failure, duplicate-key failure, allocation failure, or
   foreign-key failure rolls back the full statement.

Later rows in a multi-row statement see earlier successful inserts and
duplicate updates from the same statement.

## Generated SQLite SQL Shape

For two assignments and a two-part key, MyLite generates standard SQLite update
SQL of this shape:

```sql
UPDATE "_mylite_user_table_<table_id>"
SET "col_a" = ?1, "col_b" = ?2
WHERE "key_a" = ?3 AND "key_b" = ?4
  AND (("col_a" IS NULL OR "col_a" <> ?5)
       OR ("col_b" IS NULL OR "col_b" <> ?6))
```

For prefix key parts, the key-side expression is the descriptor-built physical
prefix expression already used by unique enforcement, duplicate-key
diagnostics, and `REPLACE`. For `NULL` assignment values, the changed predicate
is `column IS NOT NULL` and has no comparison parameter.

Every generated identifier is quoted and every data value is bound with a
prepared-statement parameter. The parameter order is assignment values first,
key tuple values second, then changed-row comparison values. This feature uses
MyLite-side translation and public SQLite prepared-statement APIs. It does not
require a SQLite fork patch.

## Diagnostics

Existing ODKU diagnostics are preserved where possible:

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
- cross-column `VALUES()` references: existing unsupported diagnostic;
- more than one enforced key descriptor: existing unsupported diagnostic;
- key-column or auto-increment assignment targets: unsupported diagnostic;
- `NULL` into `NOT NULL`, missing explicit default, range errors, overlength
  values, invalid temporal/JSON/enum/set values, and unsupported conversions:
  current descriptor conversion diagnostics;
- duplicate-key conflict caused by the physical update: current duplicate-key
  diagnostic if reachable in supported single-enforced-key paths;
- physical SQLite, allocation, and public API misuse failures: existing
  runtime diagnostics.

Supported in-range duplicate updates produce no new warnings other than
existing `DELAYED` warning `3005` and one `VALUES()` deprecation warning `1287`
per admitted `VALUES(column_name)` assignment value.

## Tests

The MySQL expectation script records:

- changed and no-op duplicate updates through a composite primary key;
- multi-row row-by-row duplicate handling through a composite primary key;
- duplicate updates through a composite unique key;
- composite unique key `NULL` part behavior;
- multiple duplicate assignments over a composite enforced key;
- MySQL acceptance of wider multiple-enforced-key and key-column-assignment
  behavior that this phase intentionally defers.

The C runtime tests must cover:

- composite primary-key and composite unique-key duplicate updates;
- single-column ODKU regression coverage;
- multi-row row-by-row behavior and affected-row totals;
- same-target `VALUES()` references and warning `1287`;
- literal, `DEFAULT`, `NULL`, string, temporal, and current-timestamp values
  where existing ODKU conversion already supports them;
- composite unique keys with `NULL` parts inserting normally;
- multiple assignment updates through composite keys;
- unsupported multiple enforced keys, key/auto-increment target rejection,
  qualified targets, qualified `VALUES()` references, cross-column
  `VALUES()`, and conversion failures;
- rollback on duplicate-branch failure;
- persistence after close/reopen, independent file-backed handles, `.mylite`
  preamble preservation, non-row result shape, affected rows, and warning
  counts.

Existing parser, insert, insert-set, ODKU, duplicate-multiple-assignment,
primary-key, composite-primary-key, unique-index, composite-unique-index,
prefix-key, replace-key, update, row-values, auto-increment, diagnostics,
file-backed storage, VFS, and full workflow tests must continue to pass.

