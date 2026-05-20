# Baseline Mutable SQL Select Limit Tasks

Implement handle-local mutable `@@sql_select_limit` session behavior and
implicit top-level `SELECT` caps.

## Checklist

1. Research and design
   - Verify official MySQL 8.4 metadata for `sql_select_limit`.
   - Probe MySQL 8.4.9 for supported `SET` forms, `DEFAULT`, signed values,
     overflow, unsupported literal diagnostics, explicit `LIMIT` precedence,
     scalar/table/aggregate/grouped/compound caps, and global behavior.
   - Record MyLite's deliberate no-mutable-global limitation.

2. Runtime system-variable state
   - Add `sql_select_limit` to session state with `UINT64_MAX` initialization.
   - Include it in `SET` multi-assignment snapshots and rollback.
   - Return session values for unscoped/session/local reads and fixed defaults
     for global reads.
   - Update `SHOW VARIABLES` value rendering.

3. `SET` handling
   - Admit no-scope, `SESSION`, `LOCAL`, `@@SESSION`, `@@LOCAL`, and direct
     `@@` assignment targets.
   - Parse `DEFAULT`, unsigned decimal integers, parenthesized integers,
     unary `+`, unary `-`, `TRUE`, `FALSE`, and integer user variables.
   - Clamp negative integers to `0` with warning `1292`.
   - Reject strings, decimals, `NULL`, `ON`, `OFF`, and overflow with
     `1232 / 42000`.
   - Preserve atomic rollback on multi-assignment failure.

4. Select execution
   - Apply implicit caps only for top-level `SELECT` statements without an
     explicit `LIMIT`.
   - Push caps into existing SQLite `LIMIT ?` planning where available.
   - Cap final MyLite-owned result sets for scalar, aggregate, metadata, and
     compound paths.
   - Keep internal DML/DDL source selects outside this slice.

5. Tests and docs
   - Extend the MySQL expectation script.
   - Extend fast C runtime coverage.
   - Update compatibility docs for the exact supported subset.
   - Preserve existing public ABI, catalog, storage, VFS, and SQLite fork
     boundaries.

6. Verification
   - `cmake --build --preset dev`
   - New/updated runtime CTest entries.
   - `packages/libmylite/tests/mysql_baseline_sql_select_limit_system_variable_expectations.sh`
   - Relevant parser/runtime lifecycle CTests.
   - `cmake --workflow --preset check`

## Non-Goals

- Mutable server-global `sql_select_limit`.
- Safe-updates client initialization.
- `SHOW` statement caps.
- Persisted variables, `SET_VAR`, privileges, Performance Schema, or SQLite
  fork patches.
