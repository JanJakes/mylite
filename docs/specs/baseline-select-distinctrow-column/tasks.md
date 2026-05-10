# Baseline SELECT DISTINCTROW Column Tasks

## Goal

Accept MySQL's `DISTINCTROW` spelling as a synonym for the existing limited
descriptor-driven `SELECT DISTINCT column` slice.

## Tasks

1. Design and expectations
   - [x] Create `docs/specs/baseline-select-distinctrow-column/specs.md`.
   - [x] Record official MySQL 8.4 documentation sources and MySQL 8.4.9
     runtime observations.
   - [x] Specify parser grammar, AST normalization, descriptor resolution,
     generated SQL reuse, result behavior, diagnostics, and unsupported forms.
   - [x] Add MySQL-runtime-verified expectation coverage.
   - [x] Update compatibility docs for the exact limited `DISTINCTROW` alias
     subset.

2. Parser and AST
   - [ ] Map `DISTINCTROW` to a parser token.
   - [ ] Extend Lemon grammar for table-backed `SELECT DISTINCTROW ...`.
   - [ ] Normalize `DISTINCTROW` to the existing distinct select modifier.
   - [ ] Preserve existing `DISTINCT`, non-distinct, scalar, and aggregate
     parsing.
   - [ ] Add parser tests for supported and unsupported `DISTINCTROW` forms.

3. Runtime execution
   - [ ] Reuse the existing descriptor-backed `SELECT DISTINCT column` planner
     and SQL generation.
   - [ ] Preserve descriptor authority for schema/table, selected column,
     predicate column, ordering column, and limit resolution.
   - [ ] Preserve generated SQLite identifier quoting and parameter binding.
   - [ ] Preserve result rows, column labels, affected rows, warning count, and
     following `ROW_COUNT()` behavior.
   - [ ] Avoid catalog, storage, VFS, or SQLite fork changes.

4. Tests
   - [ ] Extend fast C tests under `packages/libmylite/tests/`.
   - [ ] Cover duplicate integer/`NULL`, `BOOL`, predicate/order/limit
     composition, schema-qualified names, reopen persistence, rename/drop
     behavior, diagnostics, and public result conventions.
   - [ ] Keep tests deterministic and avoid a new framework.

5. Verification and review
   - [ ] Run `cmake --build --preset dev`.
   - [ ] Run relevant parser/select lifecycle CTest entries.
   - [ ] Run
     `./packages/libmylite/tests/mysql_baseline_select_distinctrow_column_expectations.sh`.
   - [ ] Run `cmake --workflow --preset check`.
   - [ ] Review final diff for scope control, grammar independence, descriptor
     authority, SQLite pushdown, compatibility docs, and test relevance.

## Out Of Scope

Explicit `ALL`, full multi-expression `DISTINCTROW`, `DISTINCTROW *`, selected
expressions/literals, aliases, table-qualified selected columns, table-qualified
ordering, non-selected ordering, ordinals, expression ordering, joins, grouping,
subqueries, CTEs, set operations, locking clauses, query modifiers, protocol
metadata, storage changes, and SQLite fork patches.

