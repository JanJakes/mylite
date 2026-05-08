# Baseline SQL Auto Is Null System Variable Tasks

Add the narrow scalar system-variable slice for MyLite's fixed disabled
auto-is-null baseline: `@@sql_auto_is_null`.

## Checklist

1. MySQL research and design
   - Verify MySQL 8.4.9 values, scopes, labels, session mutability,
     quoted-name behavior, diagnostics, statement-diagnostics interactions,
     and auto-increment `IS NULL` behavior.
   - Specify fixed MyLite value `0` for no-scope, `session`, `local`, and
     `global` forms.
   - Record supported and intentionally unsupported behavior in `specs.md`.

2. Runtime resolver
   - Add `sql_auto_is_null` to the existing system-variable resolver.
   - Return fixed value `0` for all supported scopes.
   - Preserve existing unknown-variable, unsupported-expression, and
     quoted-scope diagnostics.
   - Do not change parser grammar, descriptor-backed predicate execution,
     public ABI, storage, VFS, catalog, or SQLite integration.

3. Runtime tests
   - Add a focused runtime system-variable test.
   - Cover values, labels, scopes, quoted final names, `FROM DUAL`, selected
     schema behavior, mixed scalar reads, warning/error clearing, unknown
     names, quoted-scope rejection, persistence, preamble preservation,
     unchanged generations, descriptor-backed `IS NULL` independence, and
     independent handles.

4. MySQL expectation artifact
   - Add a shell script that checks MySQL 8.4.9 result shapes, values,
     diagnostics, upstream session mutability, auto-increment `IS NULL`
     behavior, and wider forms relevant to this slice.

5. Compatibility docs
   - Update `COMPATIBILITY.md`.
   - Update `docs/compatibility/runtime-system-variables.md`.
   - Update `docs/compatibility/sql-query-expressions.md`.
   - Do not claim mutable `sql_auto_is_null` state, `SET`, auto-increment
     lookup behavior, `LAST_INSERT_ID()`, changed `IS NULL` predicate behavior,
     or `SHOW VARIABLES`.

6. Verification
   - Run focused CTest entries for parser, runtime system variables, and table
     lifecycle.
   - Run the MySQL expectation script.
   - Run `cmake --workflow --preset check`.

## Non-Goals

- Do not implement `SET`, startup options, persisted variables,
  `AUTO_INCREMENT` metadata, generated value assignment, `LAST_INSERT_ID()`,
  ODBC auto-is-null behavior, special `IS NULL` predicate rewriting,
  `SHOW VARIABLES`, Performance Schema variable tables, table-backed
  evaluation, aliases, clauses, arbitrary expressions, SQLite SQL, catalog
  mutations, or SQLite fork patches.
