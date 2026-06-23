# Baseline SQL Safe Updates System Variable Tasks

Implement MyLite's embedded `@@sql_safe_updates` baseline: scalar/expression
readback, handle-local session assignment, `SHOW VARIABLES` reflection, and
safe-update checks for supported single-table `UPDATE` and `DELETE`.

## Checklist

1. MySQL research and design
   - Verify MySQL 8.4.9 values, scopes, labels, session mutability,
     `SHOW VARIABLES`, quoted-name behavior, diagnostics, and
     statement-diagnostics interactions.
   - Verify representative safe-update DML behavior for missing keys,
     primary-key predicates, leading secondary-key predicates, non-leading
     composite-key parts, Boolean `AND`/`OR`, and `LIMIT`.
   - Record supported and intentionally unsupported behavior in `specs.md`.

2. Runtime resolver and session state
   - Recognize `sql_safe_updates` in the system-variable resolver.
   - Return fixed global `0` / `OFF`.
   - Store handle-local session overrides for Boolean/default `SET` forms.
   - Preserve existing unknown-variable and quoted-scope diagnostics.
   - Reflect session/global values through `SHOW VARIABLES`.

3. DML enforcement
   - Reject supported single-table `UPDATE` and `DELETE` under session
     `sql_safe_updates=1` when no `LIMIT` is present and the planned predicate
     is not key-constrained.
   - Treat primary-key and leading secondary-index parts as key columns.
   - Treat `AND` as key-constrained when either side is key-constrained and
     `OR` only when both sides are key-constrained.
   - Return MySQL-style `1175 / HY000` safe-update diagnostics.
   - Do not change joined or multi-table DML yet.

4. Runtime tests
   - Cover values, labels, scopes, quoted final names, `FROM DUAL`, selected
     schema behavior, mixed scalar reads, expression reads, warning/error
     clearing, unknown names, quoted-scope rejection, persistence, preamble
     preservation, unchanged generations, and independent handles.
   - Cover `SET SESSION`, `SET LOCAL`, `SET @@...`, `DEFAULT`, user-variable
     assignment, `SHOW VARIABLES`, and `SHOW GLOBAL VARIABLES`.
   - Cover safe-update DML rejection and success cases for UPDATE and DELETE.

5. MySQL expectation artifact
   - Keep the shell script checking MySQL 8.4.9 result shapes, values,
     diagnostics, session mutability, `SHOW VARIABLES`, expression reads, and
     representative safe-update DML decisions.

6. Compatibility docs
   - Update `COMPATIBILITY.md`.
   - Update `docs/compatibility/runtime-system-variables.md`.
   - Update `docs/compatibility/sql-table-dml.md`.
   - Do not claim shared global mutation, persisted state, mysql client
     `--safe-updates` initialization, `sql_select_limit`, `max_join_size`,
     joined DML enforcement, or exact optimizer access-path equivalence.

7. Verification
   - Run the focused runtime system-variable test.
   - Run relevant UPDATE/DELETE and system-variable regression tests.
   - Run the MySQL expectation script.
   - Run `cmake --workflow --preset check`.

## Non-Goals

- Do not implement startup options, persisted variables, mysql client
  `--safe-updates` initialization, mutable shared globals, `sql_select_limit`,
  `max_join_size`, Performance Schema variable tables, exact optimizer
  safe-update checks, joined/multi-table DML safe-update enforcement, catalog
  mutations, or SQLite fork patches.
