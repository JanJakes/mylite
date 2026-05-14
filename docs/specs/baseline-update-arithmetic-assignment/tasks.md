# Baseline UPDATE Arithmetic Assignment Tasks

## Goal

Add a narrow, descriptor-driven row-relative arithmetic assignment path for
common counter-style updates:

```sql
UPDATE t SET n = n + 1;
UPDATE t SET n = n - 1 WHERE ... ORDER BY ... LIMIT ...;
```

Keep the first slice limited to same-column integer `+`/`-` with unsigned
decimal literal deltas, preserve changed-row affected counts, and keep mutation
inside SQLite rather than materializing rows in MyLite memory.

## Tasks

- [x] Create `docs/specs/baseline-update-arithmetic-assignment/specs.md` with
      MySQL 8.4 documentation sources, MySQL 8.4.9 runtime observations,
      independently authored grammar, descriptor resolution, range checks,
      changed-row semantics, physical SQL shape, diagnostics, storage impact,
      and deferred key-column behavior.
- [x] Add a reproducible MySQL 8.4.9 expectation script covering supported
      arithmetic updates, changed rows, warnings, `NULL`, no-match and
      `LIMIT 0` evaluation skipping, `ORDER BY ... LIMIT`, and overflow
      diagnostics.
- [x] Extend `update_value` grammar with the narrow `identifier +/- INTEGER`
      form while preserving deterministic rejection for broader assignment
      expressions.
- [x] Add descriptor-driven runtime planning and execution for same-column
      integer arithmetic assignment, including non-key target checks, bound
      deltas, descriptor-built SQLite SQL, matched-row overflow validation, and
      existing statement/file-format invariants.
- [x] Add parser and runtime C tests, register the new dotted CTest entry, and
      keep the tests deterministic without a new framework.
- [x] Update `packages/libmylite/CMakeLists.txt` and compatibility docs for the
      exact supported subset.
- [x] Run `cmake --build --preset dev`, the focused parser/runtime CTests,
      `./packages/libmylite/tests/mysql_baseline_update_arithmetic_assignment_expectations.sh`,
      and `cmake --workflow --preset check`.
- [x] Review MySQL evidence, descriptor authority, generated SQL, range checks,
      changed-row semantics, file-format safety, zero-init cleanup,
      compatibility docs, and tests.

## Out Of Scope

General expression assignments, constant arithmetic assignment, source columns
other than the target column, signed right operands, multiplication, division,
modulo, functions, parameters, casts, strings, decimals, floats, hex, bit
literals, subqueries, multiple assignments, key-column arithmetic updates,
auto-increment arithmetic updates, duplicate-key update ordering, `IGNORE`,
multi-table updates, joined updates, cascades, triggers, generated columns, and
SQLite fork patches.
