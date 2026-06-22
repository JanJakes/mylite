# Baseline SQL Quote SHOW CREATE System Variable Tasks

Add the session system-variable slice for MyLite's embedded SHOW CREATE
quote-control baseline: `@@sql_quote_show_create`.

## Checklist

1. MySQL research and design
   - Verify MySQL 8.4.9 values, scopes, labels, session mutability, quoted-name
     behavior, diagnostics, `SHOW CREATE` interaction, and statement
     diagnostics.
   - Specify fixed global value `1`, session-local mutability, `SHOW`
     readback, and structured database/table SHOW CREATE quote-control
     behavior.
   - Record supported and intentionally unsupported behavior in `specs.md`.

2. Runtime resolver
   - Add `sql_quote_show_create` to the existing system-variable resolver.
   - Return fixed global `1` and mutable session/local/unscoped values.
   - Preserve existing unknown-variable, unsupported-expression, and
     quoted-scope diagnostics.
   - Do not change parser grammar, public ABI, storage, VFS, catalog, or
     SQLite integration.

3. Runtime tests
   - Add a focused runtime system-variable test.
   - Cover values, labels, scopes, quoted final names, `FROM DUAL`, selected
     schema behavior, mixed scalar reads, warning/error clearing, unknown
     names, quoted-scope rejection, persistence, preamble preservation,
     unchanged generations, independent handles, `SHOW VARIABLES`, quoted SHOW
     CREATE output, unquoted simple identifiers while disabled, and
     still-quoted required identifiers.

4. MySQL expectation artifact
   - Add a shell script that checks MySQL 8.4.9 result shapes, values,
     diagnostics, upstream session mutability, default quoted SHOW CREATE
     output, disabled simple-identifier quote output, and wider forms relevant
     to this slice.

5. Compatibility docs
   - Update `COMPATIBILITY.md`.
   - Update `docs/compatibility/runtime-system-variables.md`.
   - Update `docs/compatibility/sql-show-statements.md`.
   - Refresh existing SHOW CREATE specs that previously listed
     `sql_quote_show_create` as entirely missing.
   - Do not claim server-global mutation, persisted state, Performance Schema
     variable tables, or quote-control behavior for unsupported SHOW CREATE
     variants.

6. Verification
   - Run focused CTest entries for parser, runtime system variables, and SHOW
     CREATE.
   - Run the MySQL expectation script.
   - Run `cmake --workflow --preset check`.

## Non-Goals

- Do not implement startup options, persisted variables, server-global state,
  full SHOW CREATE variants, Performance Schema variable tables, table-backed
  evaluation, aliases, clauses, arbitrary expressions, SQLite SQL, catalog
  mutations, or SQLite fork patches.
