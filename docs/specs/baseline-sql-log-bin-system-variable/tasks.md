# Baseline SQL Log Bin System Variable Tasks

Add the session system-variable slice for MyLite's embedded no-op
session binary-logging baseline: `@@sql_log_bin`.

## Checklist

1. MySQL research and design
   - Verify MySQL 8.4.9 values, session-only scope, labels, session
     mutability, quoted-name behavior, diagnostics, and
     statement-diagnostics interactions.
   - Specify session-local mutability, `SHOW` readback, and unchanged
     descriptor-backed DDL/DML behavior.
   - Specify `@@global.sql_log_bin` rejection with MySQL's session-only
     diagnostic.
   - Record supported and intentionally unsupported behavior in `specs.md`.

2. Runtime resolver
   - Add `sql_log_bin` to the existing system-variable resolver.
   - Return mutable session/local/unscoped values that default to `1`.
   - Preserve existing unknown-variable, unsupported-expression,
     quoted-scope, and session-only diagnostics.
   - Do not change parser grammar, descriptor-backed DDL/DML execution,
     public ABI, storage, VFS, catalog, or SQLite integration.

3. Runtime tests
   - Add a focused runtime system-variable test.
   - Cover values, session assignment, `SHOW VARIABLES`, labels, supported
     scopes, rejected global scope, quoted final names, `FROM DUAL`, selected
     schema behavior, mixed scalar reads, warning/error clearing, unknown
     names, quoted-scope rejection, persistence, preamble preservation,
     unchanged generations, descriptor-backed DDL/DML independence, and
     independent handles.

4. MySQL expectation artifact
   - Add a shell script that checks MySQL 8.4.9 result shapes, values,
     diagnostics, upstream session mutability, session-only global-scope
     rejection, and wider forms relevant to this slice.

5. Compatibility docs
   - Update `COMPATIBILITY.md`.
   - Update `docs/compatibility/runtime-system-variables.md`.
   - Update `docs/compatibility/sql-replication.md`.
   - Do not claim binary log files, GTID behavior, replication semantics,
     privilege checks, Performance Schema variable tables, server-global state,
     or global scope support.

6. Verification
   - Run focused CTest entries for parser, runtime system variables, and table
     lifecycle.
   - Run the MySQL expectation script.
   - Run `cmake --workflow --preset check`.

## Non-Goals

- Do not implement startup options, persisted variables, server-global state,
  binary log files, GTID behavior, replication semantics, privilege checks,
  Performance Schema variable tables, table-backed evaluation, aliases,
  clauses, arbitrary expressions, SQLite SQL, catalog mutations, or SQLite
  fork patches.
