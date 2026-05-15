# Baseline SHOW GRANTS Tasks

## Goal

Add current-user `SHOW GRANTS` compatibility for MyLite's embedded `root@%`
identity, returning MySQL-shaped synthetic grant text without adding account,
role, privilege-enforcement, catalog, storage, or SQLite fork behavior.

## Tasks

1. Design and documentation
   - Create `docs/specs/baseline-show-grants/specs.md`.
   - Record official MySQL 8.4 documentation sources and MySQL 8.4.9 runtime
     observations.
   - Specify grammar, result shape, diagnostics, ownership boundaries,
     performance behavior, and unsupported account/role behavior.
   - Update compatibility docs only for the exact partial subset.

2. MySQL expectations
   - Add a reproducible MySQL 8.4.9 expectation script.
   - Verify `SHOW GRANTS`, `SHOW GRANTS FOR CURRENT_USER`,
     `SHOW GRANTS FOR CURRENT_USER()`, result column labels, exact grant rows,
     warning count, and row-count behavior.
   - Treat missing MySQL 8.4.9 runtime as a blocker.

3. Parser and AST
   - Add a dedicated `SHOW GRANTS` AST node.
   - Add Lemon grammar for the three admitted current-user forms.
   - Keep named accounts, role `USING`, filters, ordering, and limits outside
     the grammar for this phase.

4. Runtime
   - Add a synthetic result builder with one `Grants for root@%` column.
   - Emit the two MySQL 8.4.9-verified grant rows for MyLite's embedded
     `root@%` identity.
   - Preserve existing diagnostics, affected-row, warning-count, and
     `ROW_COUNT()` conventions for row-returning statements.
   - Avoid catalog mutation, SQLite SQL generation, storage changes, and
     SQLite fork changes.

5. Tests
   - Add fast parser and runtime tests under `packages/libmylite/tests/`.
   - Cover accepted spellings, deterministic rejection of deferred syntax,
     exact result values, no warnings, affected rows, subsequent `ROW_COUNT()`,
     explicit schema selection, independent handles, file reopen, and MyLite
     preamble preservation.
   - Keep tests deterministic and avoid a new framework.

6. Build integration
   - Register any new test binary in `packages/libmylite/CMakeLists.txt`.
   - Keep first-party warning and clang-tidy policy enabled.
   - Keep vendored SQLite warning policy unchanged.

7. Verification and review
   - Run `cmake --build --preset dev`.
   - Run focused parser/runtime `SHOW GRANTS` CTest entries.
   - Run `packages/libmylite/tests/mysql_baseline_show_grants_expectations.sh`.
   - Run `cmake --workflow --preset check`.
   - Review the final diff for MySQL 8.4.9 evidence, scope control,
     architecture boundaries, result semantics, docs accuracy, and no
     overclaiming of privilege support.

## Out Of Scope

Named accounts, role names, `USING`, mandatory roles, proxy grants, partial
revokes, account storage, authentication, grant descriptors, grant/revoke DDL,
`SHOW CREATE USER`, `mysql.*` privilege tables, privilege enforcement, definer
privilege checks, privilege filtering, physical storage changes, and SQLite
fork changes.
