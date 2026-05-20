# Baseline INSERT Duplicate Key-Column Assignments

## Summary

This phase extends the descriptor-driven
`INSERT ... ON DUPLICATE KEY UPDATE` path to admit assignments to supported
primary-key or unique-key columns when those columns are not `AUTO_INCREMENT`
columns and are not parent foreign-key columns.

The goal is to remove the current deterministic rejection for common valid
upsert shapes such as:

```sql
INSERT INTO t VALUES (1, 30, 300)
ON DUPLICATE KEY UPDATE key_col = 3;
```

The phase keeps the existing ODKU source and expression envelope: persistent or
shadowing session temporary base tables, `INSERT ... VALUES` and one-row
`INSERT ... SET`, distinct unqualified duplicate assignment targets, supported
descriptor value conversion, and same-target `VALUES(column_name)` references.
MyLite owns duplicate-row selection, second-order unique-conflict detection,
value conversion, affected-row reporting, and diagnostics. SQLite remains the
physical row store and uniqueness enforcer.

## Sources And Evidence

- MyLite architecture and standards:
  - `README.md`
  - `AGENTS.md`
  - `docs/architecture/engineering-standards.md`
- Existing MyLite specs:
  - `docs/specs/baseline-insert-on-duplicate-key-update/specs.md`
  - `docs/specs/baseline-insert-duplicate-multiple-assignments/specs.md`
  - `docs/specs/baseline-insert-duplicate-composite-keys/specs.md`
  - `docs/specs/baseline-insert-duplicate-multiple-enforced-keys/specs.md`
  - `docs/specs/baseline-primary-key-lifecycle/specs.md`
  - `docs/specs/baseline-composite-primary-key-lifecycle/specs.md`
  - `docs/specs/baseline-unique-index-lifecycle/specs.md`
  - `docs/specs/baseline-composite-unique-indexes/specs.md`
  - `docs/specs/baseline-index-prefix-key-parts/specs.md`
- Official MySQL 8.4 Reference Manual:
  - `INSERT ... ON DUPLICATE KEY UPDATE`:
    <https://dev.mysql.com/doc/refman/8.4/en/insert-on-duplicate.html>
  - `CREATE TABLE` primary and unique indexes:
    <https://dev.mysql.com/doc/refman/8.4/en/create-table.html>
- MySQL runtime probes are captured in
  `packages/libmylite/tests/mysql_baseline_insert_duplicate_key_column_assignments_expectations.sh`
  and verified against MySQL 8.4.9.

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior, public
SQLite APIs, and existing MyLite source. It does not copy MySQL, MariaDB,
Percona, SQLite implementation internals, or other restrictively licensed
implementation sources.

## MySQL 8.4.9 Runtime Observations

Runtime probes establish these expectations for the admitted subset:

- MySQL accepts duplicate-update assignments to primary-key and unique-key
  columns when the resulting row satisfies all unique constraints.
- A valid key-column duplicate update reports affected rows `2` when at least
  one stored column changes.
- A same-key `VALUES(key_column)` duplicate update that stores the existing key
  value reports affected rows `0` and records warning `1287` for the admitted
  `VALUES()` reference.
- If a key-column assignment would conflict with another row's primary or
  unique key, MySQL fails the statement with `1062 / 23000` and reports the
  second conflicting key name and key value.
- Composite unique and primary keys use the post-assignment tuple for conflict
  checking. Non-assigned parts of the tuple come from the duplicate row already
  stored in the table.
- Prefix unique keys compare the post-assignment prefix tuple, not necessarily
  the full string value.
- Multi-row statements remain atomic: a later key-column duplicate conflict
  rolls back earlier row effects from the same statement.
- MySQL accepts `AUTO_INCREMENT` primary-key duplicate assignments and advances
  the next generated value when the assigned value is above the current
  sequence. MyLite deliberately defers that sequence interaction in this phase.
- MySQL applies foreign-key referential actions for key updates where relevant.
  MyLite deliberately defers ODKU key-column assignments that would update a
  referenced parent key.

## Scope

Supported:

- existing supported `INSERT ... VALUES` and one-row `INSERT ... SET` sources;
- persistent and shadowing session temporary base-table targets;
- target tables with existing supported primary and unique key descriptors,
  including single-column, composite, and prefix unique keys;
- one or more distinct unqualified duplicate assignment targets, including
  primary-key and unique-key columns that are not `AUTO_INCREMENT`;
- the existing duplicate assignment value subset for each target:
  descriptor-compatible insert literals, `NULL`, `TRUE`, `FALSE`, `DEFAULT`,
  already-admitted current temporal values, and same-target
  `VALUES(column_name)`;
- second-order unique-conflict detection after applying all duplicate
  assignments to the duplicate row;
- MySQL-compatible duplicate-key diagnostics for those second-order conflicts;
- existing affected-row behavior for inserted, changed duplicate, and unchanged
  duplicate rows;
- existing warning behavior for `DELAYED` and admitted `VALUES(column_name)`;
- all-or-nothing statement behavior through the existing statement transaction;
- file-backed persistence, independent handles, and `.mylite` preamble safety.

Deferred:

- duplicate assignments to `AUTO_INCREMENT` columns;
- key-column assignments that modify a parent key referenced by a supported
  foreign-key descriptor;
- `INSERT IGNORE ... ON DUPLICATE KEY UPDATE`;
- `INSERT ... SELECT`, `TABLE`, CTE, aliases, partitions, joined, or subquery
  source forms for ODKU;
- duplicate assignment targets;
- qualified assignment targets or qualified `VALUES()` arguments;
- cross-column `VALUES(column_name)` references;
- expression assignments beyond the existing ODKU literal/default/current-value
  subset;
- column-to-column assignments, arithmetic assignments, functions beyond
  already admitted value forms, variables, parameters, casts, and collation
  expression semantics;
- triggers, generated columns, broad foreign-key referential actions,
  privilege semantics, protocol info strings, optimizer behavior, and SQLite
  fork patches.

## Ownership Boundaries

- Public API: unchanged. `mylite_execute()` remains the statement entry point
  and no public ABI changes are introduced.
- Statement context: unchanged. It owns diagnostics, warnings, affected rows,
  transaction completion, and non-row result shape.
- Parser/AST: unchanged. Existing grammar already admits the supported
  duplicate-key tail and comma-separated assignments.
- Analyzer/planner: resolves the target table, insert columns, duplicate
  assignment targets, `VALUES(column_name)` references, key participation,
  auto-increment participation, and parent foreign-key key participation from
  MyLite descriptors before SQLite SQL is generated.
- Catalog: remains authoritative for schema names, table identity, physical
  names, columns, defaults, primary keys, unique keys, prefix lengths,
  visibility, auto-increment metadata, and foreign-key descriptors. This DML
  feature does not mutate catalog rows, descriptor versions, descriptor caches,
  catalog generation, or `sqlite_schema_generation`.
- Result builder: successful statements return the existing non-row result with
  zero columns, zero result rows, affected rows, and warning count.
- SQLite physical storage: owns row mutation and physical uniqueness. MyLite
  supplies stable physical table names, quoted identifiers, descriptor-built
  key expressions, and bound values.
- Storage/VFS/file format: unchanged. ODKU writes only inside the shifted
  SQLite payload and must not touch the `.mylite` preamble.

## Supported SQL

No grammar expansion is required. This phase reuses the existing grammar:

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
- assignment values are converted through the same descriptor-owned conversion
  used by the existing ODKU path;
- unknown assignment columns fail before mutation;
- assignment to an `AUTO_INCREMENT` column remains unsupported in this phase;
- assignment to a referenced parent foreign-key column remains unsupported in
  this phase.

`VALUES(column_name)` resolution remains narrow:

- it resolves through the target table descriptor;
- it uses the fully planned proposed value for the current insert row after
  column-list mapping, default filling, generated auto-increment values, and
  insert conversion;
- it is supported only when the referenced column is the same descriptor column
  as the assignment target;
- it may explicitly name invisible descriptor columns when unqualified;
- it records warning `1287` once per supported `VALUES()` assignment value;
- unknown or qualified `VALUES()` references fail before mutation.

Duplicate-row selection is unchanged from the multiple-enforced-keys slice:

- proposed unique-key tuples containing any SQL `NULL` part do not conflict;
- when several keys conflict, MyLite chooses the first conflicting descriptor
  in catalog order, matching the current ODKU conflict-selection model.

After the duplicate row is selected and all duplicate assignment values are
converted, MyLite validates post-assignment unique tuples before running the
physical update:

1. Fetch the existing duplicate row by the originally conflicting key
   descriptor.
2. Build one projected row value array from existing stored values plus all
   duplicate assignment values.
3. For every supported primary or unique key descriptor whose tuple includes at
   least one assigned column, skip validation if the projected tuple contains
   SQL `NULL`; otherwise probe the physical table for another row with the same
   projected key tuple while excluding the original duplicate row.
4. If another row exists, return `1062 / 23000` using that key descriptor name
   and the projected key tuple value.
5. If no conflict exists, execute the descriptor-built SQLite `UPDATE` with
   quoted assignment identifiers and bound assignment/key/change-condition
   parameters.

The projected-row fetch materializes only the one duplicate row being updated.
MyLite does not scan or materialize whole tables for this feature.

Affected rows follow the existing ODKU conventions:

- inserted row: `1`;
- duplicate update where at least one assignment changes the row: `2`;
- duplicate update where every assignment is unchanged: `0`.

## Diagnostics

Existing diagnostics are preserved for syntax errors, unsupported source forms,
reserved names, missing default schema, unknown schema, unknown table,
unsupported object kind, unknown assignment columns, duplicate assignment
targets, qualified assignment targets, unsupported assignment values, unknown
or qualified `VALUES()` references, cross-column `VALUES()` references,
conversion failures, `NULL` into `NOT NULL`, allocation failures, public API
misuse, physical SQLite failures, and check-constraint failures.

This phase changes:

- key-column duplicate assignment targets no longer receive MyLite's former
  `does not support key-column assignments` diagnostic when the target is in
  the admitted non-auto, non-parent-FK subset;
- second-order key conflicts return `1062 / 23000` with MySQL's duplicate-key
  diagnostic shape;
- `AUTO_INCREMENT` duplicate assignment targets keep a deterministic MyLite
  unsupported diagnostic;
- referenced parent foreign-key duplicate assignment targets receive a
  deterministic unsupported diagnostic.

## Storage And Performance

No SQLite fork patch is required. The implementation uses public SQLite
prepared statements over MyLite-generated physical tables:

- duplicate row fetch: one descriptor-built `SELECT` by the originally
  conflicting key;
- second-order conflict probes: one keyed `SELECT 1 ... LIMIT 1` only for
  unique descriptors that include an assigned column;
- row update: the existing descriptor-built `UPDATE`.

Every generated SQLite identifier is quoted, and every assignment, key, and
change-condition value is bound as a prepared-statement parameter. Physical
table names continue to use stable generated MyLite names. Logical descriptors
remain authoritative and separate from SQLite schema text.

## Tests

Coverage for this phase must include:

- MySQL 8.4.9 expectation script for valid key-column assignment, same-key
  `VALUES()` no-op assignment, primary-key assignment, composite unique
  assignment, prefix unique assignment, second-order duplicate errors,
  multi-row atomic rollback, and intentionally deferred auto-increment and
  parent-FK assignment shapes;
- runtime C coverage for the same successful and failing behaviors;
- affected rows, warning counts, absence of result rows, and final row state;
- persistence across close/reopen and `.mylite` preamble preservation for
  updated key rows;
- independent file-backed handles;
- regression coverage that non-key ODKU behavior, multiple assignments,
  composite keys, multiple enforced keys, duplicate diagnostics, parser tests,
  runtime lifecycle tests, and full check workflow still pass.
