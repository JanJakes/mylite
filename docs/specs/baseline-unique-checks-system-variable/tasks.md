# Baseline Unique Checks System Variable Tasks

Add the narrow scalar system-variable slice for MyLite's fixed enabled
unique-checking baseline: `@@unique_checks`.

## Checklist

1. MySQL research and design
   - Verify MySQL 8.4.9 values, scopes, labels, session mutability,
     quoted-name behavior, diagnostics, and statement-diagnostics
     interactions.
   - Specify fixed MyLite value `1` for no-scope, `session`, `local`, and
     `global` forms.
   - Record supported and intentionally unsupported behavior in `specs.md`.

2. Runtime resolver
   - Add `unique_checks` to the existing system-variable resolver.
   - Return fixed value `1` for all supported scopes.
   - Preserve existing unknown-variable, unsupported-expression, and
     quoted-scope diagnostics.
   - Do not change parser grammar, public ABI, storage, VFS, catalog, or
     SQLite integration.

3. Runtime tests
   - Add a focused runtime system-variable test.
   - Cover values, labels, scopes, quoted final names, `FROM DUAL`, selected
     schema behavior, mixed scalar reads, warning/error clearing, unknown
     names, quoted-scope rejection, persistence, preamble preservation,
     unchanged generations, table DDL independence, and independent handles.

4. MySQL expectation artifact
   - Add a shell script that checks MySQL 8.4.9 result shapes, values,
     diagnostics, upstream session mutability, and wider forms relevant to
     this slice.

5. Compatibility docs
   - Update `COMPATIBILITY.md`.
   - Update `docs/compatibility/runtime-system-variables.md`.
   - Update `docs/compatibility/sql-indexes-constraints.md`.
   - Do not claim mutable unique-checking state, `SET`, unique index DDL,
     duplicate-key enforcement, index metadata, optimizer effects, import
     optimizations, or `SHOW VARIABLES`.

6. Verification
   - Run focused CTest entries for parser, runtime system variables, and table
     lifecycle.
   - Run the MySQL expectation script.
   - Run `cmake --workflow --preset check`.

## Non-Goals

- Do not implement `SET`, startup options, persisted variables, unique index
  syntax, index descriptors, duplicate-key checks, index metadata, optimizer
  effects, import optimizations, `SHOW VARIABLES`, Performance Schema variable
  tables, table-backed evaluation, aliases, clauses, arbitrary expressions,
  SQLite SQL, catalog mutations, or SQLite fork patches.
