# Baseline Schema Default Charset And Collation Tasks

## Goal

Add descriptor-owned schema default character set and collation metadata, then
use it for supported `CREATE DATABASE`, `ALTER DATABASE`, `SHOW CREATE
DATABASE`, `INFORMATION_SCHEMA.SCHEMATA`, database charset system variables,
and future `CREATE TABLE` defaults.

## Checklist

1. MySQL research and design
   - Verify MySQL 8.4.9 behavior for supported `CREATE DATABASE` option
     spellings, `ALTER DATABASE` named/current-schema forms, binary defaults,
     duplicate/conflicting options, `IF NOT EXISTS`, diagnostics, system
     variables, information schema, and table inheritance.
   - Record the exact admitted MyLite character-set/collation catalog subset.
   - Keep unsupported conversion, comparison, privilege, encryption, and
     server-global behavior explicit.

2. MySQL expectation artifact
   - Add a reproducible MySQL 8.4.9 shell script for this feature.
   - Cover result rows, errors, warnings, `ROW_COUNT()`, `SHOW CREATE
     DATABASE`, `INFORMATION_SCHEMA.SCHEMATA`, system variables, and table
     inheritance.
   - Treat a missing MySQL 8.4.9 runtime as a blocker.

3. Parser and AST
   - Extend `CREATE DATABASE` / `CREATE SCHEMA` grammar to accept supported
     charset/collation option lists.
   - Add `ALTER DATABASE` / `ALTER SCHEMA` grammar with optional schema name
     and required supported option list.
   - Add a dedicated AST node and constructor for alter-schema defaults.
   - Reuse table-option child node kinds for charset/collation option values
     only where the syntax and runtime validation stay identical.
   - Update parser tests for accepted and rejected schema option syntax.

4. Catalog migration and APIs
   - Bump catalog schema version and minimum reader version.
   - Add `default_charset` and `default_collation` to schema descriptors and
     `_mylite_catalog_schemas`.
   - Add v21-to-v22 migration that backfills existing schemas to
     `utf8mb4` / `utf8mb4_0900_ai_ci`.
   - Update fresh catalog initialization, validation, schema read/iterate,
     create, materialization, and mutation paths.
   - Add catalog API support for creating schemas with defaults and updating
     schema defaults.

5. Analyzer/planner/runtime execution
   - Normalize schema options using descriptor-owned charset/collation
     validation.
   - Reject reserved `_mylite_*` and `information_schema` mutation targets
     before catalog mutation.
   - Implement `ALTER DATABASE` named and selected-schema mutation paths.
   - Preserve `IF NOT EXISTS` no-op semantics without changing existing
     descriptor defaults.
   - Keep successful create/alter as non-row results with MySQL-compatible
     affected rows and warnings.

6. Metadata and system variables
   - Render `SHOW CREATE DATABASE` from schema descriptor defaults.
   - Populate `INFORMATION_SCHEMA.SCHEMATA` default columns from descriptors.
   - Update unscoped/session/local `@@character_set_database` and
     `@@collation_database` to follow the selected schema descriptor, with
     server-default fallback and synthetic `information_schema` values.
   - Preserve global reads as fixed server defaults.

7. Table default inheritance
   - Initialize `CREATE TABLE` defaults from the resolved target schema when
     no explicit table charset/collation option is present.
   - Initialize `CREATE TABLE ... SELECT` defaults from the resolved target
     schema.
   - Preserve `CREATE TABLE ... LIKE` source-table descriptor cloning.
   - Keep explicit table options overriding schema defaults.

8. Tests
   - Add a focused C test if extending existing schema/show/system-variable
     tests would make coverage unclear.
   - Cover successful default/utf8mb4/binary create and alter paths; named and
     selected-schema alter; close/reopen; independent handles; and
     preamble/generation safety.
   - Cover `SHOW CREATE DATABASE`, `INFORMATION_SCHEMA.SCHEMATA`,
     `@@character_set_database`, `@@collation_database`, table inheritance,
     explicit table override, and `CREATE TABLE ... LIKE`.
   - Cover no selected schema, unknown schema, `information_schema`, reserved
     names, unknown charsets/collations, mismatched collations, conflicting
     charsets, unsupported/default option names, and no-op `IF NOT EXISTS`.
   - Preserve existing parser, schema lifecycle, show-create-database,
     information-schema, show-variables, table charset/collation, alter-table
     default charset/collation, catalog migration, file-backed, VFS, and full
     check workflow coverage.

9. Compatibility docs
   - Update `COMPATIBILITY.md`.
   - Update `docs/compatibility/sql-schemas.md`,
     `docs/compatibility/runtime-system-variables.md`,
     `docs/compatibility/sql-show-statements.md`,
     `docs/compatibility/metadata-information-schema.md`,
     `docs/compatibility/character-sets.md`, and
     `docs/compatibility/collations.md` only for the exact partial subset.
   - Do not claim full charset/collation catalogs, conversion, comparison,
     encryption mutation, or mutable server-global charset state.

10. Verification and review
    - Run `cmake --build --preset dev`.
    - Run focused CTest entries for parser, schema lifecycle,
      show-create-database, information schema, show variables, table
      charset/collation, and alter-table default charset/collation.
    - Run the MySQL expectation script for this feature.
    - Run `cmake --workflow --preset check`.
    - Review architecture boundaries, public ABI stability, independent spec
      text, MySQL 8.4.9 evidence, catalog migration safety, descriptor
      authority, result and warning semantics, file-format safety, VFS
      preservation, zero-init cleanup, compatibility docs, and test relevance.

## Out Of Scope

- Schema `ENCRYPTION` mutation.
- Full charset/collation catalogs.
- Server-global charset/collation mutation.
- `SET character_set_database` / `SET collation_database`.
- Character-set conversion, literal introducers, collation coercibility, full
  Unicode comparison/order/group/distinct semantics, or protocol negotiation.
- Filesystem database directories, SQLite attached databases, or SQLite fork
  patches.
