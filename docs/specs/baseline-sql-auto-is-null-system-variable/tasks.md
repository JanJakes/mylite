# Baseline SQL Auto Is Null System Variable Tasks

Add the session system-variable and predicate lookup slice for MyLite's
auto-is-null baseline: `@@sql_auto_is_null`.

## Checklist

1. MySQL research and design
   - Verify MySQL 8.4.9 values, scopes, labels, session mutability,
     quoted-name behavior, diagnostics, statement-diagnostics interactions,
     and auto-increment `IS NULL` behavior.
   - Specify fixed global value `0`, session-local mutability, and statement
     lookup behavior based on `LAST_INSERT_ID()`.
   - Record supported and intentionally unsupported behavior in `specs.md`.

2. Runtime resolver
   - Add `sql_auto_is_null` to the existing system-variable resolver.
   - Return fixed global `0` and mutable session/local/unscoped values.
   - Preserve existing unknown-variable, unsupported-expression, and
     quoted-scope diagnostics.
   - Do not change parser grammar, public ABI, storage, VFS, catalog, or
     SQLite integration.
   - Rewrite supported `AUTO_INCREMENT` column `IS NULL` predicates
     only when session `sql_auto_is_null` is enabled.

3. Runtime tests
   - Add a focused runtime system-variable test.
   - Cover values, labels, scopes, quoted final names, `FROM DUAL`, selected
     schema behavior, mixed scalar reads, warning/error clearing, unknown
     names, quoted-scope rejection, persistence, preamble preservation,
     unchanged generations, non-auto `IS NULL` independence,
     auto-increment lookup, `UPDATE`, `DELETE`, disabled lookup,
     `IS NOT NULL`, and independent handles.

4. MySQL expectation artifact
   - Add a shell script that checks MySQL 8.4.9 result shapes, values,
     diagnostics, upstream session mutability, auto-increment `IS NULL`
     behavior, and wider forms relevant to this slice.

5. Compatibility docs
   - Update `COMPATIBILITY.md`.
   - Update `docs/compatibility/runtime-system-variables.md`.
   - Update `docs/compatibility/sql-query-expressions.md`.
   - Do not claim server-global mutation, persisted state, Performance Schema
     variable tables, or unsupported predicate contexts.

6. Verification
   - Run focused CTest entries for parser, runtime system variables, and table
     lifecycle.
   - Run the MySQL expectation script.
   - Run `cmake --workflow --preset check`.

## Non-Goals

- Do not implement startup options, persisted variables, server-global
  mutation, ODBC behavior outside SQL semantics, Performance Schema variable
  tables, table-backed variable evaluation, arbitrary expressions, SQLite SQL,
  catalog mutations, or SQLite fork patches.
