# Baseline InnoDB Engine Surface Tasks

## Goal

Add the narrow baseline storage-engine surface that lets applications declare
`ENGINE=InnoDB` on MyLite's existing limited persistent base tables and inspect
the embedded default engine through `SHOW ENGINES`.

## Tasks

1. Design and MySQL expectations
   - Create `docs/specs/baseline-innodb-engine-surface/specs.md`.
   - Record official MySQL 8.4 documentation sources and MySQL 8.4.9 runtime
     observations.
   - Specify the grammar subset, AST additions, engine-name decoding,
     diagnostics, result shape, ownership boundaries, and storage/catalog
     decisions.
   - Add `packages/libmylite/tests/mysql_baseline_innodb_engine_surface_expectations.sh`.
   - Verify the expectation script against MySQL 8.4.9 before implementation.

2. Parser and AST
   - Add parser tokens for `ENGINE`, `ENGINES`, and `STORAGE`.
   - Extend `CREATE TABLE` with one optional trailing `ENGINE [=] engine_name`
     option.
   - Add AST support for table engine options and `SHOW ENGINES`.
   - Add parser tests for supported and rejected syntax.

3. Analyzer/runtime
   - Validate optional create-table engine options before catalog or physical
     SQLite mutation.
   - Accept decoded engine name `InnoDB` case-insensitively.
   - Reject all other engine names with error `1286` / SQLSTATE `42000`.
   - Preserve the existing descriptor-driven create-table catalog and physical
     SQLite execution path.
   - Add descriptor-independent `SHOW ENGINES` result construction.

4. Runtime tests
   - Add a fast C test registered as a dotted CTest entry.
   - Cover explicit InnoDB creation forms, schema-qualified targets,
     persistence, `SHOW CREATE TABLE`, `SHOW ENGINES`, `SHOW STORAGE ENGINES`,
     row-count/warning behavior, unknown engines, unsupported syntax,
     preamble preservation, and independent handles.
   - Keep tests deterministic and avoid a new framework.

5. Compatibility docs
   - Update `COMPATIBILITY.md` for the exact limited InnoDB engine surface.
   - Update `docs/compatibility/sql-table-ddl.md`,
     `docs/compatibility/sql-show-statements.md`, and
     `docs/compatibility/embedded-design-decisions.md`.
   - Do not overclaim full storage-engine, table-option, information-schema, or
     SQL-mode behavior.

6. Verification and review
   - Run the MySQL expectation script.
   - Run `cmake --build --preset dev`.
   - Run the new CTest entry plus relevant parser/runtime lifecycle entries.
   - Run `cmake --workflow --preset check`.
   - Self-review for scope, diagnostics, catalog authority, file-format safety,
     parser independence, and documentation accuracy.

## Out Of Scope

- Multiple storage engines, plugin loading, or engine-specific physical storage.
- `SHOW ENGINE`, `SHOW TABLE STATUS`, or `INFORMATION_SCHEMA.ENGINES`.
- Table options other than explicit `InnoDB`.
- Engine substitution warnings or SQL-mode-dependent behavior.
- SQLite fork patches.
