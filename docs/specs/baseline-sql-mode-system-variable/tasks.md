# Baseline SQL Mode System Variable Tasks

Add the narrow scalar system-variable slice for MyLite's fixed MySQL 8.4.9
default SQL-mode baseline: `@@sql_mode`.

## Checklist

1. MySQL research and design
   - Verify MySQL 8.4.9 values, scopes, labels, session mutability,
     quoted-name behavior, diagnostics, and statement-diagnostics
     interactions.
   - Specify the fixed MySQL 8.4.9 default mode string for no-scope,
     `session`, `local`, and `global` forms.
   - Record supported scalar behavior and intentionally unsupported SQL-mode
     effects in `specs.md`.

2. Runtime resolver
   - Add `sql_mode` to the existing system-variable resolver.
   - Return the fixed default mode string for all supported scopes.
   - Preserve existing unknown-variable, unsupported-expression, and
     quoted-scope diagnostics.
   - Do not change parser grammar, descriptor-backed statement execution,
     public ABI, storage, VFS, catalog, SQLite integration, or internal
     placeholder SQL-mode state.

3. Runtime tests
   - Add a focused runtime system-variable test.
   - Cover values, labels, scopes, quoted final names, `FROM DUAL`, selected
     schema behavior, mixed scalar reads, warning/error clearing, unknown
     names, quoted-scope rejection, persistence, preamble preservation,
     unchanged generations, unchanged internal placeholder SQL-mode state,
     descriptor-backed DDL/DML independence, and independent handles.

4. MySQL expectation artifact
   - Add a shell script that checks MySQL 8.4.9 result shapes, values,
     diagnostics, upstream session mutability, and wider forms relevant to
     this slice.

5. Compatibility docs
   - Update `COMPATIBILITY.md`.
   - Update `docs/compatibility/runtime-system-variables.md`.
   - Update `docs/compatibility/runtime-session-sql-modes.md`.
   - Do not claim mutable `sql_mode` state, `SET`, mode-dependent parsing,
     mode-dependent conversion behavior, `SHOW VARIABLES`, or Performance
     Schema variable tables.

6. Verification
   - Run focused CTest entries for parser, runtime system variables, and table
     lifecycle.
   - Run the MySQL expectation script.
   - Run `cmake --workflow --preset check`.

## Non-Goals

- Do not implement `SET`, startup options, persisted variables, mutable
  global/session state, `ANSI_QUOTES`, `IGNORE_SPACE`, `PIPES_AS_CONCAT`,
  strict/non-strict conversion behavior, date SQL modes, division-by-zero
  behavior, auto-increment mode behavior, engine substitution behavior,
  `SHOW VARIABLES`, Performance Schema variable tables, table-backed
  evaluation, aliases, clauses, arbitrary expressions, SQLite SQL, catalog
  mutations, or SQLite fork patches.
