# Baseline INSERT Duplicate Multiple Enforced Keys

## Summary

This phase extends MyLite's existing descriptor-driven
`INSERT ... ON DUPLICATE KEY UPDATE` path from zero or one enforced key
descriptor to tables with multiple enforced primary or unique key descriptors.
It keeps the current duplicate-assignment envelope: persistent and shadowing
session temporary base tables, `INSERT ... VALUES` and one-row `INSERT ... SET`,
distinct unqualified non-key assignment targets, supported descriptor value
conversion, and same-target `VALUES(column_name)`.

The goal is to remove a common application-facing limitation without widening
the expression engine, key reassignment semantics, `IGNORE`, or `INSERT ...
SELECT` duplicate-tail behavior.

## Sources And Evidence

- MyLite architecture and standards:
  - `README.md`
  - `AGENTS.md`
  - `docs/architecture/engineering-standards.md`
- Existing MyLite specs:
  - `docs/specs/baseline-insert-on-duplicate-key-update/specs.md`
  - `docs/specs/baseline-insert-duplicate-multiple-assignments/specs.md`
  - `docs/specs/baseline-insert-duplicate-composite-keys/specs.md`
  - `docs/specs/baseline-composite-unique-indexes/specs.md`
  - `docs/specs/baseline-index-prefix-key-parts/specs.md`
- Official MySQL 8.4 Reference Manual:
  - `INSERT ... ON DUPLICATE KEY UPDATE`:
    <https://dev.mysql.com/doc/refman/8.4/en/insert-on-duplicate.html>
  - `CREATE TABLE` index ordering and unique-key behavior:
    <https://dev.mysql.com/doc/refman/8.4/en/create-table.html>
- MySQL runtime probes are captured in
  `packages/libmylite/tests/mysql_baseline_insert_duplicate_multiple_enforced_keys_expectations.sh`
  and verified against MySQL 8.4.9.

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior, public
SQLite APIs, and existing MyLite source code. It does not copy MySQL, MariaDB,
Percona, SQLite implementation internals, or other restrictively licensed
implementation sources.

## MySQL 8.4.9 Runtime Observations

MySQL 8.4.9 accepts `ON DUPLICATE KEY UPDATE` when the target table has several
primary or unique indexes. The manual describes the multi-key case as an update
against an `OR` of duplicate predicates with a single-row limit and explicitly
warns that this shape can be surprising when multiple unique indexes match.

Runtime probes establish these expectations for MyLite's admitted subset:

- A row conflicting only with the primary key updates the primary-key row.
- A row conflicting only with a secondary unique key updates the secondary-key
  row.
- If the proposed row conflicts with a primary key and a secondary unique key
  that identify different existing rows, MySQL updates the primary-key row.
- If the proposed row conflicts with two secondary unique keys that identify
  different existing rows, MySQL updates the row found through the first unique
  key in table index order.
- Later unique keys added by `ALTER TABLE ... ADD UNIQUE` participate after
  earlier unique keys in table index order for the probed subset.
- Composite primary or unique keys participate in the same selection rule.
- Unique keys with any proposed SQL `NULL` key part do not conflict; the row
  inserts normally.
- Session temporary tables shadow persistent tables with the same unqualified
  name for ODKU targets.
- A changed duplicate branch reports affected rows `2`; a no-op duplicate
  branch reports affected rows `0`; an inserted row reports affected rows `1`.
- `VALUES(column_name)` in the duplicate branch records warning `1287` once per
  admitted occurrence, as in the earlier ODKU slices.
- A generated `AUTO_INCREMENT` value is consumed by a duplicate attempt even
  when the branch updates an existing row through a secondary unique key, while
  `LAST_INSERT_ID()` remains the first generated insert id already visible to
  the connection.

MySQL accepts key-column assignments in some cases and reports duplicate-key
errors when the assignment itself creates a second conflict. MyLite keeps
key-column assignment out of scope for this phase.

## Scope

The implementation supports:

- existing supported `INSERT ... VALUES` and one-row `INSERT ... SET` sources;
- persistent and shadowing session temporary base-table targets;
- target tables with no enforced keys, one enforced primary or unique key, or
  multiple enforced primary or unique keys;
- supported single-column, composite, and prefix primary or unique key
  descriptors;
- duplicate conflict selection by MyLite descriptor index order, which must
  match the current catalog order used for descriptor-backed duplicate
  enforcement: primary key first when present, then unique descriptors in
  catalog/index order;
- distinct unqualified duplicate assignment targets that are not part of any
  enforced primary or unique key descriptor and are not the auto-increment
  column;
- the existing duplicate assignment value subset for each target: compatible
  row-value literals, `NULL`, `TRUE`, `FALSE`, `DEFAULT`, admitted temporal
  current-value forms, and same-target `VALUES(column_name)`;
- MySQL-compatible affected rows for inserted, changed duplicate, and unchanged
  duplicate rows;
- existing warning behavior for `DELAYED` and `VALUES(column_name)`;
- statement atomicity and file-backed persistence through the existing
  statement transaction.

## Non-Goals

This phase does not add:

- `INSERT IGNORE ... ON DUPLICATE KEY UPDATE`;
- `INSERT ... SELECT`, `TABLE`, CTE, alias, partition, joined, or subquery
  source forms for ODKU;
- duplicate assignment targets;
- table-qualified assignment targets or table-qualified `VALUES()` arguments;
- cross-column `VALUES(column_name)` references;
- assignments to primary-key, unique-key, or auto-increment columns;
- expression assignments, column-to-column assignments, arithmetic assignments,
  broad `DEFAULT(column_name)`, functions beyond already admitted value forms,
  variables, parameters, casts, or collation-sensitive expression handling;
- full MySQL conflict side effects for key-column reassignment;
- `IGNORE` demotion for duplicate-branch conflicts;
- triggers, cascades, generated columns, privilege semantics, protocol info
  strings, optimizer/index-use guarantees, or SQLite fork patches.

## Ownership Boundaries

- Public API: unchanged. `mylite_execute()` owns public argument validation,
  result ownership, and public misuse behavior.
- Statement context: unchanged. It owns diagnostics, warnings, affected rows,
  transaction completion, and non-row result shape.
- Parser/AST: unchanged. Existing grammar already admits the supported
  duplicate-key tail and comma-separated assignments.
- Analyzer/planner: resolves the target table, insert columns, duplicate
  assignment targets, `VALUES(column_name)` references, and key/auto-increment
  assignment restrictions from descriptors before generated SQLite SQL exists.
  This phase changes planner eligibility from "at most one enforced key" to
  "all enforced keys with descriptor key parts are eligible".
- Catalog: remains authoritative for schema names, table identity, physical
  names, columns, defaults, primary keys, unique keys, prefix lengths,
  visibility, and auto-increment metadata. ODKU DML does not mutate catalog
  rows, descriptor versions, descriptor caches, catalog generation, or
  `sqlite_schema_generation`.
- Result builder: successful statements return the existing non-row result with
  zero columns, zero result rows, affected rows, and warning count.
- SQLite physical storage: owns row mutation for generated user tables. MyLite
  supplies stable physical table names, quoted column/key expressions, and bound
  assignment/key values.
- Storage/VFS/file format: unchanged. Duplicate updates write only inside the
  shifted SQLite payload and must not touch the `.mylite` preamble.

## Supported SQL

No parser expansion is required. This phase reuses the existing MyLite grammar:

```sql
INSERT [LOW_PRIORITY | HIGH_PRIORITY | DELAYED] [INTO] table_name
    [(column_name[, ...])]
    VALUES row_value[, ...]
ON DUPLICATE KEY UPDATE duplicate_assignment
                        [, duplicate_assignment ...]

INSERT [LOW_PRIORITY | HIGH_PRIORITY | DELAYED] [INTO] table_name
    SET insert_assignment[, ...]
ON DUPLICATE KEY UPDATE duplicate_assignment
                        [, duplicate_assignment ...]
```

Runtime narrows the parsed assignment shape:

```sql
duplicate_assignment:
    column_name = duplicate_update_value

duplicate_update_value:
    supported_insert_value
  | DEFAULT
  | VALUES ( column_name )
```

MyLite Lemon-syntax sketch, matching existing parser support:

```lemon
on_duplicate_key_update_opt ::= .
on_duplicate_key_update_opt ::= ON DUPLICATE KEY UPDATE duplicate_assignment_list.

duplicate_assignment_list ::= duplicate_assignment.
duplicate_assignment_list ::= duplicate_assignment_list COMMA duplicate_assignment.

duplicate_assignment ::= qualified_identifier EQUAL duplicate_update_value.

duplicate_update_value ::= insert_value.
duplicate_update_value ::= VALUES LPAREN qualified_identifier RPAREN.
```

Qualified identifiers remain parsed so runtime can return deterministic
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
- a target column that participates in any enforced primary or unique key is
  rejected for this phase;
- the auto-increment target column is rejected;
- unknown assignment columns fail before mutation.

`VALUES(column_name)` resolution remains the current narrow model:

- it resolves through the target table descriptor;
- it uses the fully planned proposed value for the current insert row after
  column-list mapping, default filling, generated auto-increment values, and
  insert conversion;
- it is supported only when the referenced column is the same descriptor column
  as the assignment target;
- it may explicitly name invisible descriptor columns when unqualified;
- it records warning `1287` once per supported `VALUES()` assignment value;
- unknown or qualified `VALUES()` references fail before mutation.

Duplicate-key eligibility:

- no enforced key descriptors: rows insert normally, but duplicate-tail names
  and `VALUES()` warnings are still validated;
- every enforced key descriptor must have descriptor key parts, otherwise the
  statement is rejected before mutation;
- primary and unique descriptors with supported single-column, composite, and
  prefix key parts are eligible;
- proposed unique-key tuples with any SQL `NULL` part are skipped, matching
  MySQL's multiple-`NULL` unique-key behavior;
- when several keys would conflict, MyLite chooses the first conflicting
  descriptor in catalog index order.

For each planned insert row:

1. MyLite attempts the ordinary insert using the existing prepared SQLite
   statement and bound planned row values.
2. Successful inserts add `1` affected row and update generated
   auto-increment/`LAST_INSERT_ID()` state exactly as the existing insert path.
3. Duplicate-key failures in the supported key subset are mapped to the first
   conflicting descriptor key tuple found by MyLite descriptors.
4. MyLite converts or copies every duplicate assignment value before preparing
   the physical update. Any conversion error aborts the statement before
   partial duplicate-row mutation.
5. The generated update sets all duplicate assignment columns and includes the
   existing changed-row filter. Physical changes add `2` affected rows; no-op
   duplicate branches add `0`.
6. Any physical SQLite failure, duplicate-key failure, allocation failure, or
   foreign-key failure rolls back the full statement.

Later rows in a multi-row statement see earlier successful inserts and
duplicate updates from the same statement.

## Generated SQLite SQL Shape

This feature keeps the existing standard SQLite update shape. For two
assignments and a two-part conflicting key, MyLite generates SQL equivalent to:

```sql
UPDATE "_mylite_user_table_<table_id>"
SET "col_a" = ?1, "col_b" = ?2
WHERE "key_a" = ?3 AND "key_b" = ?4
  AND (("col_a" IS NULL OR "col_a" <> ?5)
       OR ("col_b" IS NULL OR "col_b" <> ?6))
```

For prefix key parts, the key-side expression is the descriptor-built physical
prefix expression already used by unique enforcement and duplicate diagnostics.
For `NULL` assignment values, the changed predicate is `column IS NOT NULL` and
has no comparison parameter.

Every generated identifier is quoted and every data value is bound with a
prepared-statement parameter. The parameter order remains assignment values
first, key tuple values second, then changed-row comparison values. This feature
uses MyLite-side translation and public SQLite prepared-statement APIs. It does
not require a SQLite fork patch.

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
- enforced key descriptors without loaded descriptor key parts: unsupported
  diagnostic;
- key-column or auto-increment assignment targets: unsupported diagnostic;
- `NULL` into `NOT NULL`, missing explicit default, range errors, overlength
  values, invalid temporal/JSON/enum/set values, and unsupported conversions:
  current descriptor conversion diagnostics;
- duplicate-key conflict caused by the physical update: current duplicate-key
  diagnostic if reachable in admitted paths;
- physical SQLite, allocation, and public API misuse failures: existing
  runtime diagnostics.

Supported in-range duplicate updates produce no new warnings other than
existing `DELAYED` warning `3005` and one `VALUES()` deprecation warning `1287`
per admitted `VALUES(column_name)` assignment value.

## Tests

The MySQL expectation script records:

- primary-key-only and secondary-unique-only conflicts on the same table;
- primary-plus-unique conflicts where both keys match different rows;
- two-secondary-unique conflicts where each key can match a different row;
- first-conflicting descriptor behavior after a later `ALTER TABLE ... ADD
  UNIQUE`;
- session temporary table shadowing of a persistent table with the same name;
- composite primary plus secondary unique conflicts;
- unique-key `NULL` part behavior;
- no-op duplicate branch affected rows;
- generated auto-increment consumption when a duplicate branch updates through
  a secondary unique key;
- MySQL acceptance of wider key-column assignment behavior that this phase
  intentionally keeps out of scope.

The C runtime tests must cover:

- successful duplicate updates on tables with primary plus unique keys;
- successful duplicate updates on tables with two secondary unique keys;
- ambiguous multi-key conflicts selecting the first descriptor key;
- multi-key conflict selection after a later `ALTER TABLE ... ADD UNIQUE`;
- session temporary/shadowing multi-key ODKU targets;
- composite primary plus unique key conflict handling;
- unique `NULL` parts inserting normally;
- no-op duplicate affected rows;
- generated auto-increment behavior on multi-key tables;
- key-column assignment rejection across every enforced key, not just one key;
- existing single-key, composite-key, prefix-key, warning, persistence,
  independent-handle, `.mylite` preamble, and diagnostic coverage continuing to
  pass.

Existing parser, insert, insert-set, ODKU, duplicate-multiple-assignment,
primary-key, composite-primary-key, unique-index, composite-unique-index,
prefix-key, replace-key, update, row-values, auto-increment, diagnostics,
file-backed storage, VFS, and full workflow tests must continue to pass.
