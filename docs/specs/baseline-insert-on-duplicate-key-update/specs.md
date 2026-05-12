# Baseline INSERT ON DUPLICATE KEY UPDATE Lifecycle

## Status

This feature specifies a narrow descriptor-driven
`INSERT ... ON DUPLICATE KEY UPDATE` slice on top of the current MyLite insert,
key, update, result, catalog, and file-backed storage baselines.

The goal is not full MySQL duplicate-key update support. The goal is the
smallest coherent upsert path that unlocks common primary-key and unique-key
application patterns without weakening the catalog authority boundary or
turning duplicate handling into SQLite text pass-through.

## Sources

- MyLite README architecture: `README.md`
- MyLite engineering standards:
  `docs/architecture/engineering-standards.md`
- Baseline implementation strategy:
  `docs/specs/baseline-implementation-strategy/specs.md`
- Runtime handles and statement context:
  `docs/specs/runtime-handles-statement-context/specs.md`
- SQLite connection bootstrap policy:
  `docs/specs/sqlite-connection-bootstrap-policy/specs.md`
- File-backed MyLite opening VFS:
  `docs/specs/file-backed-mylite-opening-vfs/specs.md`
- MyLite file-format preamble:
  `docs/specs/mylite-file-format/specs.md`
- Baseline catalog foundation:
  `docs/specs/baseline-catalog-foundation/specs.md`
- Baseline row values lifecycle:
  `docs/specs/baseline-row-values-lifecycle/specs.md`
- Baseline insert values, insert set, primary key, unique index,
  auto-increment, and update lifecycle implementations in
  `packages/libmylite/src/runtime/`
- MySQL 8.4 Reference Manual, `INSERT`:
  https://dev.mysql.com/doc/refman/8.4/en/insert.html
- MySQL 8.4 Reference Manual, `INSERT ... ON DUPLICATE KEY UPDATE`:
  https://dev.mysql.com/doc/refman/8.4/en/insert-on-duplicate.html
- MySQL 8.4 Reference Manual, constraints:
  https://dev.mysql.com/doc/refman/8.4/en/constraint-primary-key.html

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior, public
SQLite APIs, and existing MyLite source code. It does not copy MySQL, MariaDB,
Percona, SQLite implementation internals, or other restrictively licensed
implementation sources.

## MySQL 8.4.9 Runtime Observations

The expectation script for this feature records these probes against a MySQL
8.4.9 Docker runtime.

- A duplicate row update reports affected rows `2` when the assignment changes
  the existing row, and warning count `0` for literal duplicate assignments.
- A duplicate row update reports affected rows `0` when the assignment leaves
  the existing row unchanged.
- A multi-row values statement reports affected rows as the sum of each row:
  inserted rows count `1`, changed duplicate rows count `2`, and unchanged
  duplicate rows count `0`. Later rows in the same statement can duplicate
  rows inserted earlier in the same statement.
- `VALUES(column_name)` in the duplicate assignment copies the value planned
  for that inserted row and records warning `1287` in MySQL 8.4.9 because that
  form is deprecated.
- Tables without a primary or unique key do not take the duplicate path; rows
  are inserted normally, but the duplicate assignment is still parsed and name
  checked.
- `INSERT DELAYED ... ON DUPLICATE KEY UPDATE` is accepted, executes as normal
  insert, and records warning `3005`.
- Unknown duplicate assignment columns and unknown `VALUES(column)` references
  fail with `ERROR 1054 (42S22)`.
- Assigning `NULL` to a `NOT NULL` column in the duplicate branch fails with
  `ERROR 1048 (23000)`.
- Assigning `DEFAULT` to a column with no explicit default fails with
  `ERROR 1364 (HY000)`.
- Updating a key column can itself raise duplicate-key errors. This phase
  defers key-column duplicate assignments rather than partially implementing
  second-order key conflict handling.

## Scope

The implementation must add:

- parser and AST support for a limited duplicate-key tail on existing supported
  `INSERT ... VALUES` and `INSERT ... SET` forms;
- one duplicate assignment only;
- unqualified target columns in the duplicate assignment;
- assignment values limited to supported row-value literals, `DEFAULT`, and
  `VALUES(column_name)`;
- duplicate resolution for the current descriptor-backed single-column primary
  key or single-column unique-index subset when the table has at most one
  enforced key descriptor;
- all-or-nothing execution inside the existing statement transaction;
- descriptor-driven conversion and binding for inserted rows and duplicate
  assignment values;
- MySQL-compatible affected-row counts for inserted, changed duplicate, and
  unchanged duplicate rows;
- warning `1287` for each supported `VALUES(column_name)` duplicate assignment
  expression and existing warning `3005` for `DELAYED`;
- tests and MySQL 8.4.9 expectation artifacts for the supported behavior and
  deliberately rejected wider MySQL forms.

## Non-Goals

This feature must not implement:

- `INSERT ... SELECT ... ON DUPLICATE KEY UPDATE`, `TABLE`, row constructors,
  row aliases, column aliases, `PARTITION`, CTEs, subqueries, joins, or
  arbitrary SQLite SQL pass-through;
- `INSERT IGNORE ... ON DUPLICATE KEY UPDATE`;
- multiple duplicate assignments;
- table-qualified assignment targets or table-qualified `VALUES()` arguments;
- duplicate assignment expressions other than one supported literal, `DEFAULT`,
  `NULL`, or `VALUES(column_name)`;
- column-to-column assignments, arithmetic assignments, functions other than
  the duplicate-clause `VALUES(column_name)` form, variables, parameters,
  casts, collations, or generated expressions;
- duplicate updates on tables with multiple enforced key descriptors, composite
  key descriptors, or unsupported object kinds;
- duplicate assignment to a primary-key or unique-key column;
- storage-engine scheduling, privilege semantics, replication metadata,
  changed-column protocol metadata, triggers, cascades, foreign keys, generated
  columns, or SQLite fork patches.

## Ownership Boundary

- The public API remains unchanged. `mylite_execute()` owns public call
  validation, result-handle ownership, public misuse behavior, and failure
  cleanup.
- Statement context owns diagnostics, warning count, affected rows, transaction
  completion, and the non-row result object. Successful statements return
  `column_count == 0`, `row_count == 0`, and the exact affected-row count.
- Lexer/parser/AST own syntax admission and source spans. They remain
  independent of runtime, catalog, storage, and SQLite.
- Analyzer/planner code resolves target tables, insert columns, duplicate
  assignment columns, and `VALUES(column_name)` references through MyLite
  descriptors. It rejects unsupported shapes before the first row mutation.
- The catalog module owns durable descriptors, key metadata, catalog
  generation, and descriptor-cache invalidation. This DML feature does not
  mutate catalog rows, descriptor versions, descriptor caches, or
  `sqlite_schema_generation`.
- SQLite owns physical b-tree storage for generated internal statements. MyLite
  builds those statements only from descriptor metadata and stable physical
  names such as `_mylite_user_table_<table_id>`.
- Storage/VFS owns the `.mylite` preamble and shifted SQLite payload boundary.
  Duplicate-key updates must only touch the shifted SQLite payload.

## Supported SQL Grammar

The supported duplicate-key tail applies only to existing supported `VALUES`
and `SET` insert forms:

```sql
insert_statement:
    INSERT insert_modifier_opt ignore_absent_opt INTO_opt table_name
        insert_column_list_opt VALUES row_value_list
        on_duplicate_key_update_opt
  | INSERT insert_modifier_opt ignore_absent_opt INTO_opt table_name
        SET insert_assignment_list
        on_duplicate_key_update_opt

on_duplicate_key_update_opt:
    /* empty */
  | ON DUPLICATE KEY UPDATE duplicate_assignment

duplicate_assignment:
    column_name = duplicate_update_value

duplicate_update_value:
    supported_insert_value
  | DEFAULT
  | VALUES ( column_name )
```

`insert_modifier_opt` reuses the current insert modifier subset:
`LOW_PRIORITY`, `HIGH_PRIORITY`, `DELAYED`, or empty. `ignore_absent_opt` is
empty for this feature. The parser may admit `IGNORE` and route it to a
deterministic unsupported diagnostic, but execution must not silently treat it
as either ignore or duplicate-update behavior.

`supported_insert_value` is the exact literal value subset already accepted by
the current descriptor-driven `INSERT ... VALUES` / `INSERT ... SET` path for
the target column family: integer, fixed decimal, `NULL`, `TRUE`, `FALSE`,
ordinary strings for supported string columns, canonical temporal strings for
supported temporal columns, and `DEFAULT`.

## Schema and Table Resolution

Unqualified and schema-qualified target table resolution follows the existing
insert selected-schema policy. Unqualified targets require a selected default
schema. Schema-qualified targets use the named schema even when no default
schema is selected.

Reserved `_mylite_*` schema and table names are rejected before generated
SQLite SQL is built. Unknown schemas, unknown tables, and unsupported object
kinds use the existing deterministic insert diagnostics.

## Assignment and Value Resolution

The duplicate assignment target resolves against the target table descriptor.
The supported semantic subset requires an unqualified descriptor column. Unknown
assignment targets fail with MySQL-compatible unknown-column diagnostics.

`VALUES(column_name)` resolves against the target table descriptor and refers
to the fully planned value for that inserted row after column-list mapping,
omitted-column default filling, generated auto-increment values, and value
conversion. The referenced column may be visible or invisible when explicitly
named. Unknown `VALUES()` columns fail with unknown-column diagnostics.

The duplicate assignment target must not be part of the enforced key descriptor
used by this feature. Key-column assignment is rejected before mutation until
second-order duplicate conflict handling is specified.

Descriptor catalog case handling follows the existing catalog foundation:
names compare with the current MyLite descriptor name matching rules; this
phase does not add collation-aware name resolution.

## Duplicate Key Resolution

For each planned insert row:

1. MyLite attempts the ordinary descriptor-driven insert using prepared SQLite
   statements and bound values.
2. If the insert succeeds, affected rows increase by `1`.
3. If SQLite reports a duplicate-key failure for the current supported key
   subset, MyLite locates the conflicting row through the single enforced key
   descriptor and applies the duplicate assignment to that existing row.
4. If the duplicate assignment changes the stored value, affected rows
   increase by `2`.
5. If the duplicate assignment stores the same value after MyLite
   canonicalization, affected rows increase by `0`.
6. Any unsupported duplicate shape or physical SQLite failure rolls back the
   full statement.

Tables with no primary or unique key descriptors simply insert rows. Tables
with more than one enforced key descriptor, composite key descriptors, or key
descriptors that MyLite cannot map to a single physical row are rejected for
this feature before mutation. That keeps MyLite from guessing MySQL's
multi-unique-index conflict selection.

Later rows in a multi-row statement see earlier successful inserts and
duplicate updates from the same statement, matching the observed MySQL row
processing model.

## Conversion and Nullability

Inserted row conversion reuses the existing insert path exactly. Duplicate
assignment conversion reuses the existing update/insert assignment conversion
for the assignment target descriptor:

- integer families use the current signed/unsigned range checks and binding
  policy;
- fixed decimal values use the current decimal canonicalization and rounding
  warning policy for admitted decimal targets;
- string values use the current admitted string family and length policy;
- canonical temporal strings use the current date, time, datetime, and
  timestamp conversion policy;
- `DEFAULT` uses the descriptor default or raises the existing no-default
  diagnostic;
- `NULL` stores SQL `NULL` for nullable targets and raises the existing
  `NOT NULL` diagnostic for non-nullable targets.

`VALUES(column_name)` does not perform string interpolation or expression
evaluation. It copies the planned bound value object for the referenced insert
column and then validates it against the duplicate assignment target. Same-type
descriptor copies should be fast paths; incompatible descriptor-family copies
are rejected unless an existing insert/update conversion rule explicitly admits
that conversion.

## Generated SQLite Handling

Generated SQLite SQL must be standard SQLite prepared statements built from
descriptors:

- use stable physical table names;
- quote every SQLite identifier;
- bind inserted values, key lookup values, duplicate assignment values, and
  changed-value comparison values;
- do not interpolate SQL literals;
- do not depend on SQLite's `ON CONFLICT DO UPDATE` syntax for user-visible
  semantics;
- do not rely on SQLite schema text as metadata authority.

The duplicate branch may use MyLite-owned `SELECT ... WHERE key = ?` lookup
and `UPDATE ... WHERE key = ?` statements. If MyLite needs to distinguish a
changed value from a no-op value, it should compare canonical planned values
before or during the generated update path rather than reporting SQLite's
matched-row count.

## Diagnostics and Warnings

The implementation must produce deterministic diagnostics for:

- syntax errors and unsupported grammar;
- missing default schema;
- unknown schema or table;
- reserved `_mylite_*` target names;
- unsupported object kind;
- `IGNORE` combined with `ON DUPLICATE KEY UPDATE`;
- unsupported source form such as `INSERT ... SELECT`, `TABLE`, row
  constructors, aliases, or partitions;
- multiple duplicate assignments;
- table-qualified duplicate assignment targets;
- table-qualified `VALUES()` arguments;
- unknown duplicate assignment columns;
- unknown `VALUES()` columns;
- duplicate assignment to an enforced key column;
- unsupported duplicate assignment expressions;
- unsupported literal or conversion for the assignment target;
- integer, decimal, string, or temporal out-of-range values;
- `NULL` into `NOT NULL`;
- `DEFAULT` on a descriptor with no explicit or effective default;
- tables with multiple enforced keys or unsupported key shapes;
- duplicate branch updates that would violate another key once that broader
  key surface exists;
- physical SQLite failures;
- allocation failures.

For supported in-range statements:

- `warning_count == 0` for literal, `NULL`, and `DEFAULT` duplicate
  assignments without `DELAYED`;
- `VALUES(column_name)` records warning `1287` once per supported duplicate
  assignment expression;
- `DELAYED` records warning `3005` using the existing insert warning policy.

## Compatibility Documentation

After implementation, update `COMPATIBILITY.md` and
`docs/compatibility/sql-table-dml.md` from unsupported to a limited partial
entry. The wording must make clear that this is not full MySQL upsert support:
no `IGNORE`, no aliases, no `SELECT` source, no multiple duplicate
assignments, no duplicate assignment expressions, no key-column assignments,
and no multiple-key conflict selection.

## Tests

Add a fast C runtime test, preferably `runtime_insert_on_duplicate_key_update`,
and register it with a dotted CTest name. Add a MySQL 8.4.9 expectation script
under `packages/libmylite/tests/`.

Coverage must include:

- duplicate literal assignment changed row and no-op row;
- duplicate `VALUES(column_name)` assignment and warning count;
- multi-row insert with one new row and one duplicate row;
- duplicate against a primary key and against a single unique index when
  supported;
- no-key table inserts with a duplicate tail;
- `INSERT ... SET ... ON DUPLICATE KEY UPDATE`;
- `LOW_PRIORITY`, `HIGH_PRIORITY`, and `DELAYED`;
- `DEFAULT`, `NULL`, integer, decimal, string, and temporal assignments where
  those target families are already supported by insert/update;
- `NULL` into `NOT NULL`, no-default `DEFAULT`, and range diagnostics;
- unknown assignment and `VALUES()` columns;
- unsupported `IGNORE`, aliases, partitions, source forms, multiple duplicate
  assignments, table-qualified assignment targets, expressions, parameters,
  functions, arithmetic, column-to-column assignments, and key-column
  assignments;
- affected rows, warning count, absence of result rows, and remaining rows;
- rollback on duplicate-branch failure;
- reopen persistence and independent file-backed handles;
- file-format preamble preservation;
- existing parser, insert, update, primary-key, unique-index, auto-increment,
  runtime handle, diagnostics, catalog, VFS, and file-format tests.

## Verification

Before marking the feature done:

1. Run `cmake --build --preset dev`.
2. Run the new CTest entry plus existing parser, insert, update, primary-key,
   unique-index, auto-increment, row-values, select, delete, file-format, and
   VFS lifecycle entries.
3. Run the MySQL 8.4.9 expectation script added for this feature.
4. Run `cmake --workflow --preset check`.
5. Review the final diff for architecture boundaries, public ABI stability,
   independently authored grammar/spec text, MySQL 8.4.9 evidence, catalog
   authority, descriptor-driven physical updates, key conflict correctness,
   conversion correctness, exact affected-row semantics, warning behavior,
   file-format safety, VFS preservation, zero-init safety, cleanup on failure,
   scope control, compatibility-matrix accuracy, and test relevance.
