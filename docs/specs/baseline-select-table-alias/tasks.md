# Baseline SELECT Table Alias Tasks

## Goal

Accept one optional table alias on currently supported descriptor-backed
single-table `SELECT` forms without changing descriptor resolution or generated
SQLite execution.

## Tasks

1. Design and expectations
   - [x] Create `docs/specs/baseline-select-table-alias/specs.md`.
   - [x] Record official MySQL 8.4 documentation sources and MySQL 8.4.9
     runtime observations.
   - [x] Specify parser grammar, AST storage, descriptor resolution,
     generated SQL reuse, result behavior, diagnostics, and unsupported forms.
   - [x] Add MySQL-runtime-verified expectation coverage.
   - [x] Update compatibility docs for the exact limited table-alias subset.

2. Parser and AST
   - [x] Add a table-alias optional grammar node for supported table-backed
     `SELECT` productions.
   - [x] Store the alias as the optional second child of `FROM_TABLE` while
     preserving the table name as child `0`.
   - [x] Preserve current default, `ALL`, `DISTINCT`, `DISTINCTROW`, wildcard,
     aggregate, `DUAL`, and no-source parsing.
   - [x] Add parser tests for `AS`, bare, quoted, schema-qualified, modifier,
     wildcard, and aggregate alias forms.

3. Runtime execution
   - [x] Reuse existing table-backed select planning from the table-name child.
   - [x] Preserve descriptor authority for schema/table, selected columns,
     predicate columns, ordering columns, aggregate arguments, and limits.
   - [x] Preserve generated SQLite identifier quoting and parameter binding.
   - [x] Preserve result rows, column labels, affected rows, warning count, and
     following `ROW_COUNT()` behavior.
   - [x] Avoid catalog, storage, VFS, or SQLite fork changes.

4. Tests
   - [x] Extend fast C tests under `packages/libmylite/tests/`.
   - [x] Cover successful aliases across representative default, `ALL`,
     `DISTINCT`, `DISTINCTROW`, wildcard, aggregate, schema-qualified,
     filtered, ordered, and limited forms.
   - [x] Cover no default schema, unknown schema, unknown table, reserved
     target names, unknown columns, unsupported qualified references, reopen,
     rename/drop, independent handles, result conventions, and file-format
     preservation through representative aliased statements.
   - [x] Keep tests deterministic and avoid a new framework.

5. Verification and review
   - [x] Run `cmake --build --preset dev`.
   - [x] Run relevant parser/select/aggregate lifecycle CTest entries.
   - [x] Run
     `./packages/libmylite/tests/mysql_baseline_select_table_alias_expectations.sh`.
   - [x] Run `cmake --workflow --preset check`.
   - [x] Review final diff for scope control, grammar independence, descriptor
     authority, SQLite pushdown, compatibility docs, and test relevance.

## Out Of Scope

Alias-qualified column references, original-table-qualified semantics after an
alias is supplied, select-item aliases, joins, derived tables, CTEs, duplicate
alias checks for multi-table statements, index hints, partitions, grouping,
having, windows, locking clauses, protocol metadata changes, storage changes,
and SQLite fork patches.
