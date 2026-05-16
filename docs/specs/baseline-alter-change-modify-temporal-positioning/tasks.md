# Baseline ALTER CHANGE/MODIFY Temporal Positioning Tasks

## Goal

Extend descriptor-driven `ALTER TABLE ... CHANGE [COLUMN]` and
`ALTER TABLE ... MODIFY [COLUMN]` with the narrow column-positioning and
temporal replacement surface needed by common schema migrations.

## Tasks

1. Design and evidence
   - Create the feature spec and tasks.
   - Record official MySQL 8.4 documentation sources and MySQL 8.4.9 runtime
     observations.
   - Capture MySQL-runtime expectations for position order, temporal metadata,
     affected rows, warnings, diagnostics, and introspection.

2. Parser and AST
   - Add an optional `FIRST` / `AFTER identifier` position node for
     `CHANGE`/`MODIFY`.
   - Keep existing no-position AST shape stable where possible.
   - Add parser tests for accepted and rejected position syntax.

3. Planner and catalog
   - Resolve `FIRST` / `AFTER` from descriptors, not SQLite metadata.
   - Reorder the planned descriptor column array while preserving all other
     column ids and relative order.
   - Add catalog mutation support for updating descriptor ordinals in the same
     mutation as replacement metadata.
   - Preserve existing unsupported checks for indexed, primary-key, and
     CHECK-constrained tables.

4. Runtime execution
   - Rebuild physical tables when position changes.
   - Support `DATETIME` / `TIMESTAMP` replacement when source and target are
     supported temporal text descriptors.
   - Validate existing temporal and `NULL` values before catalog mutation
     commits.
   - Keep generated SQLite SQL descriptor-built, quoted, and stable-physical-name
     based.

5. Tests and docs
   - Extend fast C parser/runtime tests.
   - Update compatibility docs only for the exact supported subset.
   - Run the MySQL expectation script, focused CTest entries, and full check.
   - Review the final diff for MySQL behavior, catalog authority, rebuild
     safety, affected rows, cleanup, file-format safety, and scope control.
