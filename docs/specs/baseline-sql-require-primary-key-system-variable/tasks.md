# Baseline SQL Require Primary Key System Variable Tasks

Add the narrow scalar system-variable slice for MyLite's fixed disabled
primary-key-requirement baseline: `@@sql_require_primary_key`.

## Checklist

1. MySQL research and design
   - Verify MySQL 8.4.9 values, scopes, labels, session mutability, upstream
     primary-key enforcement, quoted-name behavior, diagnostics, and
     statement-diagnostics interactions.
   - Specify fixed MyLite value `0` for no-scope, `session`, `local`, and
     `global` forms.
   - Record supported and intentionally unsupported behavior in `specs.md`.

2. Runtime resolver
   - Add `sql_require_primary_key` to the existing system-variable resolver.
   - Return fixed value `0` for all supported scopes.
   - Preserve existing unknown-variable, unsupported-expression,
     unsupported-`SET`, and quoted-scope diagnostics.
   - Do not change parser grammar, descriptor-backed statement execution,
     primary-key metadata, public ABI, storage, VFS, catalog, or SQLite
     integration.

3. Runtime tests
   - Add a focused runtime system-variable test.
   - Cover values, labels, scopes, quoted final names, `FROM DUAL`, selected
     schema behavior, mixed scalar reads, warning/error clearing, unknown
     names, quoted-scope rejection, rejected `SET`, persistence, preamble
     preservation, unchanged generations, descriptor-backed DDL/DML
     independence, and independent handles.

4. MySQL expectation artifact
   - Add a shell script that checks MySQL 8.4.9 result shapes, values,
     diagnostics, upstream session mutability, upstream primary-key
     enforcement, and wider forms relevant to this slice.

5. Compatibility docs
   - Update `COMPATIBILITY.md`.
   - Update `docs/compatibility/runtime-system-variables.md`.
   - Update `docs/compatibility/sql-table-ddl.md`.
   - Update `docs/compatibility/sql-replication.md`.
   - Do not claim mutable `sql_require_primary_key` state, `SET`, primary-key
     constraints, DDL enforcement, generated invisible primary keys, import
     behavior, replication policy, privileges, `SHOW VARIABLES`, or
     Performance Schema variable tables.

6. Verification
   - Run focused CTest entries for parser, runtime system variables, DDL/DML,
     and table lifecycle.
   - Run the MySQL expectation script.
   - Run `cmake --workflow --preset check`.

## Non-Goals

- Do not implement `SET`, startup options, persisted variables, mutable
  global/session state, primary-key constraints, generated invisible primary
  keys, `CREATE TABLE ... LIKE`, `CREATE TABLE ... SELECT`, temporary tables,
  `ALTER TABLE` structure changes, table import, replication applier policy,
  privileges, `SHOW VARIABLES`, Performance Schema variable tables,
  table-backed evaluation, aliases, clauses, arbitrary expressions, SQLite SQL,
  catalog mutations, or SQLite fork patches.
