# Baseline SQL Quote SHOW CREATE System Variable Tasks

Add the narrow scalar system-variable slice for MyLite's fixed enabled
SHOW CREATE quote-control baseline: `@@sql_quote_show_create`.

## Checklist

1. MySQL research and design
   - Verify MySQL 8.4.9 values, scopes, labels, session mutability, quoted-name
     behavior, diagnostics, `SHOW CREATE` interaction, and statement
     diagnostics.
   - Specify fixed MyLite value `1` for no-scope, `session`, `local`, and
     `global` forms.
   - Record supported and intentionally unsupported behavior in `specs.md`.

2. Runtime resolver
   - Add `sql_quote_show_create` to the existing system-variable resolver.
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
     unchanged generations, independent handles, and quoted SHOW CREATE output.

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
   - Do not claim mutable quote-control state, disabled rendering, `SET`,
     `SHOW VARIABLES`, or unsupported SHOW CREATE variants.

6. Verification
   - Run focused CTest entries for parser, runtime system variables, and SHOW
     CREATE.
   - Run the MySQL expectation script.
   - Run `cmake --workflow --preset check`.

## Non-Goals

- Do not implement `SET`, startup options, persisted variables, disabled
  SHOW CREATE quote rendering, full SHOW CREATE variants, `SHOW VARIABLES`,
  Performance Schema variable tables, table-backed evaluation, aliases,
  clauses, arbitrary expressions, SQLite SQL, catalog mutations, or SQLite fork
  patches.
