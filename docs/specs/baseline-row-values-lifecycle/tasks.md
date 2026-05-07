# Baseline Row Values Lifecycle Tasks

## Goal

Add the next narrow row lifecycle slice: limited integer/null
`INSERT ... VALUES` writes and descriptor-driven table `SELECT` reads through
`mylite_execute()`, backed by authoritative MyLite catalog descriptors and
stable SQLite physical table names.

## Tasks

1. Design and documentation
   - Create `docs/specs/baseline-row-values-lifecycle/specs.md`.
   - Record official MySQL 8.4 documentation sources and MySQL 8.4.9 runtime
     observations.
   - Specify parser grammar, schema resolution, column resolution, physical
     SQLite DML/query generation, assignment conversion, transactions,
     diagnostics, result behavior, and unsupported behavior.
   - Update `COMPATIBILITY.md` and relevant detailed compatibility docs only
     for the exact partial subset.

2. MySQL expectations
   - Add a reproducible MySQL 8.4.9 expectation script for the supported
     user-visible SQL behavior and intentionally rejected wider MySQL forms.
   - Verify success status, affected rows, warnings, result column labels,
     result rows, `NULL` values, diagnostics, side effects, and failure
     atomicity.
   - Treat a missing MySQL 8.4.9 runtime as a blocker for changing
     compatibility expectations.

3. Parser and AST
   - Extend Lemon grammar for limited `INSERT INTO table VALUES (...)` and
     `INSERT INTO table (columns) VALUES (...)`.
   - Include multi-row `VALUES (...), (...)` grammar.
   - Extend table `SELECT` grammar for `SELECT * FROM table` and
     `SELECT column[, column ...] FROM table`.
   - Add AST node kinds for insert statements, insert column lists, insert row
     lists, insert rows, identifier lists, and table `FROM` nodes.
   - Map `INSERT`, `INTO`, and `VALUES` keywords in the parser adapter.
   - Keep parser code independent from runtime, catalog, storage, and SQLite.
   - Add parser tests for supported and rejected row lifecycle syntax.

4. Analyzer/planner/runtime execution
   - Add statement execution paths for insert and table select using the
     existing statement context.
   - Resolve unqualified and schema-qualified table names against selected
     schema and MyLite catalog descriptors.
   - Reject reserved `_mylite_*` source schema/table names before generating
     SQLite SQL.
   - Reject unknown schemas, unknown tables, unsupported object kinds, unknown
     columns, duplicate insert target columns, value-count mismatches, omitted
     required columns, unsupported value expressions, `NULL` into `NOT NULL`,
     and integer out-of-range assignments with deterministic diagnostics.
   - Return empty DML results for insert and descriptor-driven text results for
     table select.

5. Physical SQLite row storage
   - Generate SQLite DML/query SQL only from descriptors and stable physical
     table names.
   - Quote every generated SQLite identifier.
   - Bind integer and `NULL` values through prepared statements.
   - Convert values before binding.
   - Keep descriptor rows, catalog generation, descriptor versions, descriptor
     caches, and SQLite schema generation unchanged for row DML and select.

6. Atomicity and cleanup
   - Execute every supported insert inside one MyLite-owned `BEGIN IMMEDIATE`
     transaction.
   - Preflight every row conversion before writing the first row.
   - Roll back if conversion, binding, stepping, allocation, or SQLite
     execution fails.
   - Make new planner/result cleanup functions tolerate zero-initialized state.

7. Tests
   - Add a fast C test under `packages/libmylite/tests/` and register it with a
     dotted CTest name.
   - Cover successful insert/select, multi-row affected rows, all admitted
     integer families, `NULL` handling, explicit column-list ordering,
     full-row ordinal inserts, failure diagnostics, failure unwinding,
     schema resolution, reserved names, reopen persistence, rename visibility,
     drop behavior, independent handles, preamble safety, and result API
     behavior.
   - Keep tests deterministic and avoid adding a new test framework.

8. Build integration
   - Add new sources or tests to `packages/libmylite/CMakeLists.txt`.
   - Keep first-party warning and clang-tidy policy enabled.
   - Keep vendored SQLite warning policy unchanged.

9. Verification and review
   - Run `cmake --build --preset dev`.
   - Run the new CTest entry and existing parser/basic/rename lifecycle
     entries.
   - Run the MySQL expectation script.
   - Run `cmake --workflow --preset check`.
   - Review the final diff for architecture boundaries, public ABI stability,
     independently authored grammar/spec text, MySQL 8.4.9 evidence, catalog
     authority, descriptor-driven physical DML, assignment conversion,
     atomicity, file-format safety, VFS preservation, zero-init safety, cleanup
     on failure, scope control, compatibility-matrix accuracy, and test
     relevance.

## Out Of Scope

- General `SELECT`, expression evaluation, aliases, table-qualified column
  references, predicates, sorting, limiting, joins, grouping, subqueries, CTEs,
  set operations, aggregate/window functions, locks, and arbitrary SQLite
  pass-through.
- `UPDATE`, `DELETE`, `REPLACE`, `LOAD DATA`, `INSERT ... SET`,
  `INSERT ... SELECT`, `INSERT IGNORE`, `ON DUPLICATE KEY UPDATE`,
  priorities, partitions, aliases, row aliases, `RETURNING`, and `VALUES ROW()`.
- `DEFAULT`, auto-increment, generated IDs, defaults, constraints, indexes,
  triggers, temporary tables, views, privileges, full type conversion, warning
  demotion, unsigned 64-bit storage beyond the signed-64-bit physical encoding,
  and SQLite fork patches.
