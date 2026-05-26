# Baseline Default Temporary Storage Engine System Variable Tasks

Add the narrow system-variable read slice for MyLite's temporary-table default
engine: `@@default_tmp_storage_engine`.

## Checklist

1. MySQL research and design
   - Verify MySQL 8.4.9 values, scopes, labels, mutability, temporary table
     effect, invalid engine diagnostics, quoted-name behavior, and diagnostics
     interactions.
   - Specify fixed MyLite value `InnoDB` for all supported read scopes.
   - Record mutable alternate temporary engines as a deliberate non-goal.

2. Runtime resolver
   - Add `default_tmp_storage_engine` to the existing system-variable resolver.
   - Return fixed value `InnoDB` for no-scope, `session`, `local`, and
     `global` scalar reads.
   - Include the variable in `SHOW VARIABLES` and `SHOW GLOBAL VARIABLES`.
   - Preserve existing unknown-variable, unsupported-expression, and
     quoted-scope diagnostics.
   - Do not change parser grammar, public ABI, storage, VFS, catalog,
     temporary table creation, or SQLite integration.

3. Runtime tests
   - Extend the focused InnoDB engine surface test for scalar reads,
     labels/scopes, diagnostics clearing, selected schema, reopen behavior,
     preamble preservation, unchanged generations, independent handles, and
     implicit temporary table rendering.
   - Extend SHOW VARIABLES coverage and row counts.

4. MySQL expectation artifact
   - Add a shell script that checks MySQL 8.4.9 result shapes, values, SHOW
     rows, mutability evidence, implicit temporary table effects, invalid
     engine diagnostics, and wider behavior relevant to this slice.

5. Compatibility docs
   - Update `COMPATIBILITY.md`.
   - Update `docs/compatibility/runtime-system-variables.md`.
   - Update `docs/compatibility/embedded-design-decisions.md`.
   - Update table-DDL wording only if needed to clarify the temporary table
     default remains InnoDB.

6. Verification
   - Run focused CTest entries for parser, show variables, InnoDB engine
     surface, SQL mode, and temporary table lifecycle.
   - Run the MySQL expectation script.
   - Run `cmake --workflow --preset check`.

## Non-Goals

- Do not implement `SET default_tmp_storage_engine`, alternate temporary
  engines, mutable global/session engine state, startup options, persisted
  variables, `SET_VAR` hints, `disabled_storage_engines`, Performance Schema
  variable tables, arbitrary expression use, SQLite SQL, catalog mutations, or
  SQLite fork patches.
