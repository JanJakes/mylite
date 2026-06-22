# Baseline SQL Buffer Result System Variable Tasks

Add the session system-variable slice for MyLite's embedded no-op
result-buffering baseline: `@@sql_buffer_result`.

## Checklist

1. MySQL research and design
   - Verify MySQL 8.4.9 values, scopes, labels, session mutability,
     quoted-name behavior, diagnostics, and statement-diagnostics
     interactions.
   - Specify fixed global value `0`, session-local mutability, `SHOW`
     readback, and unchanged descriptor-backed `SELECT` behavior.
   - Record supported and intentionally unsupported behavior in `specs.md`.

2. Runtime resolver
   - Add `sql_buffer_result` to the existing system-variable resolver.
   - Return fixed global `0` and mutable session/local/unscoped values.
   - Preserve existing unknown-variable, unsupported-expression, and
     quoted-scope diagnostics.
   - Do not change parser grammar, descriptor-backed `SELECT` execution,
     public ABI, storage, VFS, catalog, or SQLite integration.

3. Runtime tests
   - Add a focused runtime system-variable test.
   - Cover values, session assignment, `SHOW VARIABLES`, labels, scopes,
     quoted final names, `FROM DUAL`, selected schema behavior, mixed scalar
     reads, warning/error clearing, unknown names, quoted-scope rejection,
     persistence, preamble preservation, unchanged generations,
     descriptor-backed `SELECT` independence, and independent handles.

4. MySQL expectation artifact
   - Add a shell script that checks MySQL 8.4.9 result shapes, values,
     diagnostics, upstream session mutability, and wider forms relevant to
     this slice.

5. Compatibility docs
   - Update `COMPATIBILITY.md`.
   - Update `docs/compatibility/runtime-system-variables.md`.
   - Update `docs/compatibility/sql-query-expressions.md`.
   - Do not claim server-global mutation, persisted state, temporary result
     tables, lock-release behavior, optimizer effects, changed
     descriptor-backed `SELECT` behavior, or Performance Schema variable
     tables.

6. Verification
   - Run focused CTest entries for parser, runtime system variables, and table
     lifecycle.
   - Run the MySQL expectation script.
   - Run `cmake --workflow --preset check`.

## Non-Goals

- Do not implement startup options, persisted variables, server-global
  mutation, temporary result table materialization, server cursor buffering,
  lock-release behavior, optimizer effects, Performance Schema variable
  tables, table-backed evaluation, aliases, clauses, arbitrary expressions,
  SQLite SQL, catalog mutations, or SQLite fork patches.
