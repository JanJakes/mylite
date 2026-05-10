# Baseline SELECT ALL Modifier Tasks

## Goal

Accept a single explicit `ALL` modifier after `SELECT` as the duplicate-
preserving default for currently supported select forms.

## Tasks

1. Design and expectations
   - [x] Create `docs/specs/baseline-select-all-modifier/specs.md`.
   - [x] Record official MySQL 8.4 documentation sources and MySQL 8.4.9
     runtime observations.
   - [x] Specify parser grammar, AST normalization, descriptor resolution,
     generated SQL reuse, result behavior, diagnostics, and unsupported forms.
   - [x] Add MySQL-runtime-verified expectation coverage.
   - [x] Update compatibility docs for the exact limited `SELECT ALL` subset.

2. Parser and AST
   - [x] Map `ALL` to a parser token.
   - [x] Extend Lemon grammar for supported `SELECT ALL ...` forms.
   - [x] Normalize `ALL` to the existing default select modifier.
   - [x] Preserve existing default, `DISTINCT`, `DISTINCTROW`, scalar, and
     aggregate parsing.
   - [x] Add parser tests for supported and unsupported `ALL` forms.

3. Runtime execution
   - [x] Reuse existing non-distinct select execution without new runtime state.
   - [x] Preserve descriptor authority for schema/table, selected column,
     predicate column, ordering column, aggregate argument, and limit
     resolution.
   - [x] Preserve generated SQLite identifier quoting and parameter binding.
   - [x] Preserve result rows, column labels, affected rows, warning count, and
     following `ROW_COUNT()` behavior.
   - [x] Avoid catalog, storage, VFS, or SQLite fork changes.

4. Tests
   - [x] Extend fast C tests under `packages/libmylite/tests/`.
   - [x] Cover duplicate preservation, wildcard, scalar, `DUAL`, aggregate,
     predicate/order/limit composition, schema-qualified names, reopen
     persistence, rename/drop behavior, diagnostics, and public result
     conventions.
   - [x] Keep tests deterministic and avoid a new framework.

5. Verification and review
   - [x] Run `cmake --build --preset dev`.
   - [x] Run relevant parser/select/aggregate lifecycle CTest entries.
   - [x] Run
     `./packages/libmylite/tests/mysql_baseline_select_all_modifier_expectations.sh`.
   - [x] Run `cmake --workflow --preset check`.
   - [x] Review final diff for scope control, grammar independence, descriptor
     authority, SQLite pushdown, compatibility docs, and test relevance.

## Out Of Scope

Repeated `ALL`, mixed `ALL` with `DISTINCT` or `DISTINCTROW`, exact MySQL
modifier-mixing error `1221`, other select modifiers, unsupported projection
expressions, joins, grouping, set operations, locking clauses, protocol
metadata changes, storage changes, and SQLite fork patches.
