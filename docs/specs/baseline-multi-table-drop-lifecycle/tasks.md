# Baseline Multi-Table DROP Lifecycle Tasks

Implement the next narrow table-lifecycle slice: descriptor-driven
`DROP TABLE [IF EXISTS] table_name [, table_name] ...` for persistent base
tables.

## Design And Expectations

1. Verify MySQL 8.4.9 behavior using
   `packages/libmylite/tests/mysql_baseline_multi_table_drop_expectations.sh`.
2. Keep the authoritative specification in
   `docs/specs/baseline-multi-table-drop-lifecycle/specs.md`.
3. Do not start implementation until the grammar, diagnostics, warning, result,
   atomicity, and storage boundaries are covered by the spec and MySQL runtime
   observations.

## Parser And AST

1. Extend the grammar from one `table_name` to a comma-separated table-name
   list for `DROP TABLE`.
2. Represent the target list structurally, preferably with a dedicated table
   name list node rather than the column-oriented identifier list node.
3. Preserve the existing optional `IF EXISTS` marker.
4. Add parser tests for unqualified, schema-qualified, mixed, and `IF EXISTS`
   target lists.
5. Replace the previous syntax-error expectation for `DROP TABLE a, b` with
   success coverage.

## Runtime

1. Replace the single-target drop runtime path with planning plus execution.
2. Resolve all targets before mutation, including selected/default schema
   policy and missing explicit schemas under `IF EXISTS`.
3. Reject reserved `_mylite_*` schema/table names before generated SQLite SQL.
4. Reject duplicate logical targets with MySQL-compatible `1066` diagnostics
   before mutation.
5. For non-`IF EXISTS`, collect all missing targets and fail atomically with a
   deterministic unknown-table diagnostic.
6. For `IF EXISTS`, append one `Note 1051` per missing target in statement
   order and drop all existing persistent base-table targets.
7. Use one catalog mutation for all descriptor deletes and physical table
   drops.
8. Increment `sqlite_schema_generation` once when at least one physical table
   is dropped; do not increment it for all-missing no-ops.
9. Preserve public non-row result conventions: `affected_rows == 0`,
   `ROW_COUNT() == 0`, and warning count equal to stored notes.
10. Keep generated SQLite as per-table `DROP TABLE "<physical_name>"` using
    descriptor-derived names and quoted identifiers.

## Tests

1. Add or extend plain C runtime tests for:
   - successful unqualified multi-table drop;
   - successful schema-qualified multi-table drop without default schema;
   - missing non-`IF EXISTS` targets with no partial mutation;
   - mixed existing/missing `IF EXISTS` target lists;
   - all-missing `IF EXISTS` no-op behavior;
   - missing explicit schema with `IF EXISTS`;
   - no selected schema with mixed qualified/unqualified targets;
   - duplicate target diagnostics;
   - reserved-name diagnostics;
   - row-count state, warning count, warning rows, and remaining tables;
   - reopen persistence, independent handles, and preamble safety;
   - cleanup after planner allocation/failure paths where practical.
2. Update unsupported syntax tests for `TEMPORARY`, `RESTRICT`, and `CASCADE`.
3. Keep existing lexer, parser, diagnostics, table lifecycle, table IF EXISTS,
   schema lifecycle, row-values, select, delete, update, alter, file-backed
   opening, VFS, and public result/API tests green.

## Documentation

1. Update `COMPATIBILITY.md` only after implementation to say `DROP TABLE`
   supports a limited persistent base-table target list with optional
   `IF EXISTS`.
2. Update `docs/compatibility/sql-table-ddl.md` with the same exact subset.
3. Do not overclaim temporary tables, views, triggers, foreign keys, cascades,
   privileges, implicit commits, `RESTRICT`, `CASCADE`, or non-base objects.

## Verification

1. `cmake --build --preset dev`
2. New or updated parser/runtime CTest entries.
3. `./packages/libmylite/tests/mysql_baseline_multi_table_drop_expectations.sh`
4. Existing table lifecycle and table IF EXISTS CTest entries.
5. `cmake --workflow --preset check`
6. Final diff review for MySQL behavior, descriptor authority, atomicity,
   physical SQLite safety, `.mylite` preamble preservation, and scope control.
