# Baseline INSERT ON DUPLICATE KEY UPDATE Lifecycle Tasks

## Goal

Add a narrow descriptor-driven upsert slice for supported `INSERT ... VALUES`
and `INSERT ... SET` statements with one `ON DUPLICATE KEY UPDATE` assignment,
authoritative MyLite descriptors, current key metadata, exact affected-row
counts, and MySQL 8.4.9 verified warnings.

## Tasks

1. Design and documentation
   - Create `docs/specs/baseline-insert-on-duplicate-key-update/specs.md`.
   - Record official MySQL 8.4 documentation sources and MySQL 8.4.9 runtime
     observations.
   - Specify grammar, table resolution, assignment resolution,
     `VALUES(column)` resolution, duplicate-key detection, conversion,
     generated SQLite handling, diagnostics, warnings, and unsupported forms.
   - Update `COMPATIBILITY.md` and `docs/compatibility/sql-table-dml.md` only
     for the exact implemented partial subset.

2. MySQL expectations
   - Add a reproducible MySQL 8.4.9 expectation script.
   - Verify changed duplicate, no-op duplicate, normal insert, multi-row,
     no-key table, `VALUES(column)` warning, `DELAYED`, `DEFAULT`, `NULL`,
     unknown columns, and unsupported shape behavior.
   - Treat a missing MySQL 8.4.9 runtime as a blocker.

3. Parser and AST
   - Extend the insert grammar with an optional duplicate-key update tail for
     supported `VALUES` and `SET` forms.
   - Add AST representation for the duplicate-key clause, one assignment, and
     the duplicate-clause `VALUES(column)` expression.
   - Keep parser code independent from runtime, catalog, storage, and SQLite.
   - Add parser tests for supported and rejected syntax.

4. Analyzer/planner/runtime
   - Reuse the existing insert planner for inserted row conversion and default
     filling.
   - Resolve duplicate assignment targets and `VALUES(column)` references from
     descriptors.
   - Reject unsupported duplicate shapes before mutation.
   - Detect conflicts through the current supported single-column primary-key
     or unique-index descriptor subset.
   - Apply the duplicate assignment with generated SQLite prepared statements
     and bound values.
   - Report affected rows as MySQL does for the supported subset.

5. Atomicity and cleanup
   - Execute the full statement inside the existing insert transaction.
   - Roll back if any row's duplicate branch fails.
   - Keep new cleanup functions zero-initialized-state safe.

6. Tests
   - Add a fast C runtime test under `packages/libmylite/tests/`.
   - Cover primary-key and unique-key duplicate updates, no-key inserts,
     multi-row row-by-row behavior, literal/`VALUES()`/`DEFAULT`/`NULL`
     assignments, diagnostics, warnings, persistence, independent handles,
     preamble safety, result metadata, and unsupported syntax.
   - Keep tests deterministic and avoid a new test framework.

7. Build integration
   - Add any new test binary to `packages/libmylite/CMakeLists.txt`.
   - Keep first-party warning and clang-tidy policy enabled.
   - Keep vendored SQLite warning policy unchanged.

8. Verification and review
   - Run `cmake --build --preset dev`.
   - Run the new CTest entry and the existing parser/insert/update/key
     lifecycle entries.
   - Run the new MySQL expectation script.
   - Run `cmake --workflow --preset check`.
   - Use a subagent review before finalizing the implementation commit.

## Out Of Scope

Full `ON DUPLICATE KEY UPDATE`, `INSERT IGNORE`, `INSERT ... SELECT`, `TABLE`,
row constructors, aliases, partitions, multiple duplicate assignments,
assignment expressions, table-qualified names, key-column duplicate
assignments, multiple-key conflict selection, triggers, cascades, privilege
semantics, protocol info strings, and SQLite fork patches.
