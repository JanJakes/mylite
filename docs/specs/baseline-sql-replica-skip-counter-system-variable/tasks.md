# Baseline SQL Replica Skip Counter System Variable Tasks

Add the narrow scalar system-variable slice for MyLite's fixed replica
skip-counter baseline: `@@sql_replica_skip_counter`.

## Checklist

1. MySQL research and design
   - Verify MySQL 8.4.9 values, scopes, labels, upstream global mutability,
     quoted-name behavior, diagnostics, deprecated alias behavior, and
     statement-diagnostics interactions.
   - Specify fixed MyLite value `0` for no-scope and `global` forms.
   - Specify global-only diagnostics for `session` and `local`.
   - Record supported and intentionally unsupported behavior in `specs.md`.

2. Runtime resolver
   - Add `sql_replica_skip_counter` to the existing system-variable resolver.
   - Return fixed value `0` for supported scopes.
   - Reject `session` and `local` scopes as global-only.
   - Preserve existing unknown-variable, unsupported-expression,
     unsupported-`SET`, and quoted-scope diagnostics.
   - Do not change parser grammar, descriptor-backed statement execution,
     replication metadata, public ABI, storage, VFS, catalog, or SQLite
     integration.

3. Runtime tests
   - Add a focused runtime system-variable test.
   - Cover values, labels, scopes, quoted final names, `FROM DUAL`, selected
     schema behavior, mixed scalar reads, warning/error clearing, unknown
     names, global-only diagnostics, quoted-scope rejection, rejected `SET`,
     rejected deprecated alias, persistence, preamble preservation, unchanged
     generations, descriptor-backed DDL/DML independence, and independent
     handles.

4. MySQL expectation artifact
   - Add a shell script that checks MySQL 8.4.9 result shapes, values,
     diagnostics, upstream global mutability, deprecated alias warning, and
     wider forms relevant to this slice.

5. Compatibility docs
   - Update `COMPATIBILITY.md`.
   - Update `docs/compatibility/runtime-system-variables.md`.
   - Update `docs/compatibility/sql-replication.md`.
   - Do not claim mutable `sql_replica_skip_counter` state, `SET`,
     replica-event skipping, `START REPLICA`, channels, GTID behavior, the
     deprecated alias, privileges, `SHOW VARIABLES`, or Performance Schema
     variable tables.

6. Verification
   - Run focused CTest entries for parser, runtime system variables,
     descriptor-backed DDL/DML, and table lifecycle.
   - Run the MySQL expectation script.
   - Run `cmake --workflow --preset check`.

## Non-Goals

- Do not implement `SET`, startup options, persisted variables, mutable global
  state, `START REPLICA`, `STOP REPLICA`, replication channels, relay logs,
  binary logs, event skipping, GTID checks, anonymous-transaction assignment,
  source metadata, applier workers, replication status, the deprecated
  `sql_slave_skip_counter` alias, deprecation warnings, privileges,
  `SHOW VARIABLES`, Performance Schema variable tables, table-backed
  evaluation, aliases, clauses, arbitrary expressions, SQLite SQL, catalog
  mutations, or SQLite fork patches.
