# Baseline SQL Generate Invisible Primary Key System Variable Tasks

Add the narrow scalar system-variable slice for MyLite's fixed disabled
generated-invisible-primary-key baseline:
`@@sql_generate_invisible_primary_key`.

## Checklist

1. MySQL research and design
   - Verify MySQL 8.4.9 values, scopes, labels, session mutability, GIPK table
     creation effects, quoted-name behavior, diagnostics, and
     statement-diagnostics interactions.
   - Specify fixed MyLite value `0` for no-scope, `session`, `local`, and
     `global` forms.
   - Record supported and intentionally unsupported behavior in `specs.md`.

2. Runtime resolver
   - Add `sql_generate_invisible_primary_key` to the existing system-variable
     resolver.
   - Return fixed value `0` for all supported scopes.
   - Preserve existing unknown-variable, unsupported-expression, and
     quoted-scope diagnostics.
   - Do not change parser grammar, descriptor-backed DDL, public ABI, storage,
     VFS, catalog, or SQLite integration.

3. Runtime tests
   - Add a focused runtime system-variable test.
   - Cover values, labels, scopes, quoted final names, `FROM DUAL`, selected
     schema behavior, mixed scalar reads, warning/error clearing, unknown
     names, quoted-scope rejection, persistence, preamble preservation,
     unchanged generations, descriptor-backed table creation independence, and
     independent handles.

4. MySQL expectation artifact
   - Add a shell script that checks MySQL 8.4.9 result shapes, values,
     diagnostics, upstream session mutability, GIPK table creation effects,
     and wider forms relevant to this slice.

5. Compatibility docs
   - Update `COMPATIBILITY.md`.
   - Update `docs/compatibility/runtime-system-variables.md`.
   - Update `docs/compatibility/sql-table-ddl.md`.
   - Do not claim mutable `sql_generate_invisible_primary_key` state, `SET`,
     invisible columns, generated primary keys, `my_row_id`, auto-increment,
     changed table creation, or `SHOW VARIABLES`.

6. Verification
   - Run focused CTest entries for parser, runtime system variables, and table
     lifecycle.
   - Run the MySQL expectation script.
   - Run `cmake --workflow --preset check`.

## Non-Goals

- Do not implement `SET`, startup options, persisted variables, invisible
  columns, implicit `my_row_id` columns, generated primary keys, primary-key
  descriptors, auto-increment value generation, GIPK visibility controls,
  replication behavior, `SHOW VARIABLES`, Performance Schema variable tables,
  table-backed evaluation, aliases, clauses, arbitrary expressions, SQLite SQL,
  catalog mutations, or SQLite fork patches.
