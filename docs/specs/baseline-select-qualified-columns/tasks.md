# Baseline SELECT Qualified Columns Tasks

## Goal

Resolve descriptor-backed qualified column references for supported
single-table `SELECT` forms while keeping generated SQLite execution
descriptor-driven.

## Tasks

1. Design and expectations
   - [x] Create `docs/specs/baseline-select-qualified-columns/specs.md`.
   - [x] Record official MySQL 8.4 documentation sources and MySQL 8.4.9
     runtime observations.
   - [x] Specify qualifier matching, alias behavior, grammar, diagnostics,
     descriptor resolution, generated SQL reuse, and unsupported forms.
   - [x] Add MySQL-runtime-verified expectation coverage.
   - [x] Update compatibility docs for the exact qualified-column subset.

2. Parser and AST
   - [x] Extend aggregate argument grammar to admit qualified identifiers where
     this slice needs them.
   - [x] Preserve existing nested qualified identifier AST shape.
   - [x] Preserve current default, `ALL`, `DISTINCT`, `DISTINCTROW`, wildcard,
     aggregate, `DUAL`, and no-source parsing.
   - [x] Add parser tests for selected, predicate, order, and aggregate
     qualified references.

3. Runtime execution
   - [x] Resolve one-, two-, and three-part column references against the
     single descriptor-backed source table.
   - [x] Enforce alias shadowing: when a source alias exists, only that alias
     may qualify a column.
   - [x] Preserve descriptor authority for schema/table, selected columns,
     predicate columns, ordering columns, aggregate arguments, and limits.
   - [x] Preserve generated SQLite identifier quoting and parameter binding.
   - [x] Preserve result rows, column labels, affected rows, warning count, and
     following `ROW_COUNT()` behavior.
   - [x] Avoid catalog, storage, VFS, or SQLite fork changes.

4. Tests
   - [x] Extend fast C tests under `packages/libmylite/tests/`.
   - [x] Cover qualified selected columns, predicates, order keys, `ALL`,
     `DISTINCT`, `DISTINCTROW`, aggregates, schema-qualified sources, aliases,
     wrong qualifiers, unknown columns, reopen, rename/drop, independent
     handles, result conventions, and file-format preservation.
   - [x] Keep tests deterministic and avoid a new framework.

5. Verification and review
   - [x] Run `cmake --build --preset dev`.
   - [x] Run relevant parser/select/aggregate lifecycle CTest entries.
   - [x] Run
     `./packages/libmylite/tests/mysql_baseline_select_qualified_columns_expectations.sh`.
   - [x] Run `cmake --workflow --preset check`.
   - [x] Review final diff for scope control, grammar independence, descriptor
     authority, SQLite pushdown, compatibility docs, and test relevance.

## Out Of Scope

Qualified wildcards, select-item aliases, `ORDER BY` select-item aliases,
joins, multiple source tables, derived tables, CTEs, subqueries, grouping,
having, windows, set operations, locking clauses, index hints, partitions,
general expression evaluation, DML qualified references, protocol metadata
changes, storage changes, and SQLite fork patches.
