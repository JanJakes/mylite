# Baseline Insert Set Lifecycle Tasks

## Goal

Add the next narrow DML slice: descriptor-driven `INSERT [INTO] table SET ...`
through `mylite_execute()`, backed by authoritative MyLite catalog descriptors,
stable SQLite physical table names, and the existing MyLite-owned
integer/`NULL` assignment conversion used by `INSERT ... VALUES`.

## Tasks

1. Design and documentation
   - Create `docs/specs/baseline-insert-set-lifecycle/specs.md`.
   - Record official MySQL 8.4 documentation sources and MySQL 8.4.9 runtime
     observations.
   - Specify parser grammar, schema resolution, assignment target resolution,
     assignment conversion, omitted-column behavior, physical SQLite insert
     generation, diagnostics, result behavior, and unsupported behavior.
   - Update compatibility docs only after implementation, and only for the
     exact supported subset.

2. MySQL expectations
   - Add a reproducible MySQL 8.4.9 expectation script for supported
     `INSERT ... SET` behavior and intentionally rejected wider MySQL forms.
   - Verify affected rows, warning count, `ROW_COUNT()`, inserted rows,
     omitted nullable columns, omitted required columns, duplicate assignment
     targets, unknown columns, range diagnostics, and target resolution.
   - Treat a missing MySQL 8.4.9 runtime as a blocker for changing
     compatibility expectations.

3. Parser and AST
   - Extend Lemon grammar for `INSERT [INTO] table_name SET assignment_list`.
   - Keep existing `INSERT ... VALUES` behavior stable.
   - Add AST node kinds or reuse existing assignment-list nodes when the shape
     is semantically clear and does not blur update vs insert ownership.
   - Map any additional keywords needed by the parser adapter.
   - Keep parser code independent from runtime, catalog, storage, and SQLite.
   - Add parser tests for supported and rejected `INSERT ... SET` syntax.

4. Analyzer/planner/runtime execution
   - Add or extend the insert statement execution path using the existing
     statement context.
   - Resolve unqualified and schema-qualified targets against selected schema
     and MyLite catalog descriptors.
   - Reject reserved `_mylite_*` target schema/table names before generated
     SQLite SQL.
   - Resolve assignment targets from descriptors, not SQLite metadata.
   - Reject unknown schemas, unknown tables, unsupported object kinds, unknown
     assignment columns, duplicate assignments, table-qualified assignment
     targets, omitted required columns, unsupported values, out-of-range
     assignments, and `NULL` into `NOT NULL` with deterministic diagnostics.
   - Return an empty DML result with one affected row and zero warnings.

5. Physical SQLite insert
   - Generate SQLite `INSERT` SQL only from descriptors and stable physical
     table names.
   - Quote every generated SQLite identifier.
   - Bind integer and `NULL` values through prepared statements.
   - Convert all assignment and omitted-column values before writing.
   - Reuse existing insert transaction and cleanup behavior when possible.
   - Keep descriptor rows, catalog generation, descriptor versions, descriptor
     caches, and SQLite schema generation unchanged for row DML.

6. Atomicity and cleanup
   - Execute supported inserts inside one MyLite-owned `BEGIN IMMEDIATE`
     transaction.
   - Plan and convert all assignment values before writing the row.
   - Roll back if conversion, prepare, bind, step, allocation, or SQLite
     execution fails.
   - Make new planner cleanup functions tolerate zero-initialized state.

7. Tests
   - Add a fast C test under `packages/libmylite/tests/` and register it with a
     dotted CTest name.
   - Cover successful `INSERT INTO ... SET` and `INSERT ... SET`, assignment
     ordering, omitted nullable columns, all admitted integer families,
     `NULL` handling, assignment range boundaries, affected rows, warning
     count, `ROW_COUNT()`, diagnostics, unsupported syntax, reopen
     persistence, rename/drop behavior, independent handles, preamble safety,
     and result API behavior.
   - Keep tests deterministic and avoid adding a new test framework.

8. Build integration
   - Add any new tests to `packages/libmylite/CMakeLists.txt`.
   - Keep first-party warning and clang-tidy policy enabled.
   - Keep vendored SQLite warning policy unchanged.

9. Verification and review
   - Run the MySQL expectation script.
   - Run `cmake --build --preset dev`.
   - Run the new CTest entry plus parser and adjacent row-values/update/delete
     lifecycle entries.
   - Run `cmake --workflow --preset check`.
   - Review the final diff for architecture boundaries, public ABI stability,
     independently authored grammar/spec text, MySQL 8.4.9 evidence, catalog
     authority, descriptor-driven physical inserts, integer/`NULL` assignment
     conversion correctness, exact affected-row semantics, file-format safety,
     VFS preservation, zero-init safety, cleanup on failure, scope control,
     compatibility-matrix accuracy, and test relevance.

## Out Of Scope

- Full `INSERT`, `INSERT ... VALUES` expansion, `INSERT ... SELECT`, `INSERT
  ... TABLE`, `VALUES ROW()`, priorities, `DELAYED`, `IGNORE`, partitions,
  aliases, row aliases, `ON DUPLICATE KEY UPDATE`, `RETURNING`, table-qualified
  assignment targets, expression assignments, defaults, generated values,
  warnings-as-demotion behavior, non-strict SQL modes, auto-increment,
  insert-id behavior, constraints, triggers, indexes, temporary tables, views,
  privileges, and SQLite fork patches.
