# Baseline Default Storage Engine System Variable Tasks

Add the narrow scalar system-variable slice for MyLite's permanent-table
default engine: `@@default_storage_engine`.

## Checklist

1. MySQL research and design
   - Verify MySQL 8.4.9 values, scopes, labels, mutability, quoted-name
     behavior, diagnostics, and statement-diagnostics interactions.
   - Specify fixed MyLite value `InnoDB` for all supported scopes.
   - Record supported and intentionally unsupported behavior in `specs.md`.

2. Runtime resolver
   - Add `default_storage_engine` to the existing system-variable resolver.
   - Return fixed value `InnoDB` for no-scope, `session`, `local`, and
     `global` forms.
   - Preserve existing unknown-variable, unsupported-expression, and
     quoted-scope diagnostics.
   - Do not change parser grammar, public ABI, storage, VFS, catalog, or
     SQLite integration.

3. Runtime tests
   - Extend the focused InnoDB engine surface test.
   - Cover values, labels, scopes, quoted final names, `FROM DUAL`, selected
     schema behavior, mixed scalar reads, warning/error clearing, unknown
     names, quoted-scope rejection, persistence, preamble preservation,
     unchanged generations, and independent handles.

4. MySQL expectation artifact
   - Add a shell script that checks MySQL 8.4.9 result shapes, values,
     diagnostics, scoped/session mutability evidence, and wider forms relevant
     to this slice.

5. Compatibility docs
   - Update `COMPATIBILITY.md`.
   - Update `docs/compatibility/runtime-system-variables.md`.
   - Update `docs/compatibility/embedded-design-decisions.md`.
   - Do not claim `SET`, mutable engine state, `SHOW VARIABLES`, alternate
     engines, temporary-table engine defaults, or plugin behavior.

6. Verification
   - Run focused CTest entries for parser, InnoDB engine surface, and runtime
     system variables.
   - Run the MySQL expectation script.
   - Run `cmake --workflow --preset check`.

## Non-Goals

- Do not implement `SET`, startup options, persisted variables,
  `default_tmp_storage_engine`, `disabled_storage_engines`, alternate engines,
  engine substitution, `SHOW VARIABLES`, `INFORMATION_SCHEMA.ENGINES`,
  plugins, table-backed evaluation, aliases, clauses, arbitrary expressions,
  SQLite SQL, catalog mutations, or SQLite fork patches.
