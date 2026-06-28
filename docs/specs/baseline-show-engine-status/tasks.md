# Baseline SHOW ENGINE STATUS And LOGS Tasks

## Goal

Add the narrow `SHOW ENGINE InnoDB STATUS` and `SHOW ENGINE InnoDB LOGS`
compatibility surfaces as descriptor-independent embedded placeholder results.

## Tasks

1. Design and MySQL expectations
   - Create `docs/specs/baseline-show-engine-status/specs.md`.
   - Record official MySQL 8.4 documentation sources and MySQL 8.4.9 runtime
     observations.
   - Specify grammar, AST additions, engine-name decoding, diagnostics, result
     shape, ownership boundaries, and storage/catalog decisions.
   - Add
     `packages/libmylite/tests/mysql_baseline_show_engine_status_expectations.sh`.
   - Verify the expectation script against MySQL 8.4.9 before implementation.

2. Parser and AST
   - Add parser support for `SHOW ENGINE option_name STATUS` and
     `SHOW ENGINE option_name LOGS`.
   - Add AST nodes for `SHOW ENGINE STATUS` and `SHOW ENGINE LOGS`.
   - Preserve existing identifier behavior for `ENGINE`, `STATUS`, `LOGS`, and
     related keywords.
   - Add parser tests for supported syntax and deferred syntax errors.

3. Runtime
   - Decode the engine name using the existing table-option name policy.
   - Accept only `InnoDB`, case-insensitively.
   - Reject other engine names with the existing storage-engine diagnostic.
   - Build the MySQL-shaped `Type`, `Name`, `Status` status result with one
     stable MyLite-owned row.
   - Build the MySQL-shaped `Type`, `Name`, `Status` logs result with zero
     rows.
   - Do not read or mutate catalog rows and do not generate SQLite SQL.

4. Runtime tests
   - Extend the existing InnoDB engine surface C test.
   - Cover successful output, row-count/warning behavior, preamble
     preservation, independent handles, unknown engines, and unsupported forms.
   - Keep tests deterministic and avoid a new framework.

5. Compatibility docs
   - Update `COMPATIBILITY.md` for limited `SHOW ENGINE InnoDB STATUS` and
     `SHOW ENGINE InnoDB LOGS`.
   - Update `docs/compatibility/sql-show-statements.md`.
   - Do not overclaim `MUTEX`, Performance Schema status, live monitor
     output, privileges, filters, or alternate engines.

6. Verification and review
   - Run the MySQL expectation script.
   - Run `cmake --build --preset dev`.
   - Run the relevant parser/runtime CTest entries.
   - Run `cmake --workflow --preset check`.
   - Self-review and subagent-review for scope, diagnostics, docs, tests,
     MySQL evidence, architecture boundaries, and file-format safety.

## Out Of Scope

- `SHOW ENGINE InnoDB MUTEX`.
- `SHOW ENGINE PERFORMANCE_SCHEMA STATUS`.
- Non-`InnoDB` engines and plugin-specific engine internals.
- Privilege enforcement and live monitor output.
- Catalog, storage-format, VFS, or SQLite fork changes.
