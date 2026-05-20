# Baseline Mutable SQL Select Limit

## Status

This feature extends the existing `@@sql_select_limit` scalar-read slice with
the smallest useful mutable session behavior:

- session-local `SET sql_select_limit` forms;
- `@@sql_select_limit` / `@@SESSION.sql_select_limit` readback from session
  state;
- implicit row caps for top-level `SELECT` result sets that do not carry an
  explicit `LIMIT`.

MyLite still does not implement mutable server-global system-variable state.
Global reads continue to expose the compiled-in no-limit value, and `SET
GLOBAL sql_select_limit` is rejected with a deterministic MyLite unsupported
diagnostic.

## Sources

- MyLite README architecture: `README.md`
- Engineering standards:
  `docs/architecture/engineering-standards.md`
- Existing fixed scalar slice:
  `docs/specs/baseline-sql-select-limit-system-variable/specs.md`
- Runtime handles and statement context:
  `docs/specs/runtime-handles-statement-context/specs.md`
- Select order/limit lifecycle:
  `docs/specs/baseline-select-order-limit-lifecycle/specs.md`
- Runtime system-variable compatibility:
  `docs/compatibility/runtime-system-variables.md`
- MySQL 8.4 Reference Manual, server system variables:
  https://dev.mysql.com/doc/refman/8.4/en/server-system-variables.html
- MySQL 8.4 Reference Manual, `SET` variable assignment:
  https://dev.mysql.com/doc/refman/8.4/en/set-variable.html

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior,
public SQLite APIs, and existing MyLite source code. It does not copy MySQL,
MariaDB, Percona, SQLite implementation internals, or other restrictively
licensed implementation sources.

## MySQL 8.4.9 Runtime Observations

The expectation script
`packages/libmylite/tests/mysql_baseline_sql_select_limit_system_variable_expectations.sh`
records the MySQL 8.4.9 probes for this feature. Observed behavior:

- The official variable metadata reports global/session scope, dynamic status,
  integer type, default `18446744073709551615`, minimum `0`, and maximum
  `18446744073709551615`.
- Unscoped, `SESSION`, `LOCAL`, `@@SESSION`, `@@LOCAL`, and direct `@@`
  assignment forms mutate the current session value.
- `SET SESSION sql_select_limit=DEFAULT` resets the session value to the
  current global value. MyLite's global value remains the compiled-in default,
  so `DEFAULT` restores `18446744073709551615`.
- `SET GLOBAL sql_select_limit=N` changes MySQL's global runtime value for
  later session defaults, but does not change the current session value.
- A session value of `N` caps top-level `SELECT` result rows when the statement
  has no explicit `LIMIT`; `0` returns a result set with column metadata and no
  rows.
- An explicit `LIMIT` clause takes precedence over `@@sql_select_limit`.
- The cap applies to table-backed selects, scalar selects, aggregate selects,
  grouped selects, and the final row set of `UNION ALL`.
- Accepted assignment values in this slice are `DEFAULT`, unsigned decimal
  integer literals, parenthesized integer literals, `+integer`, `-integer`,
  `TRUE`, `FALSE`, and user variables whose current MyLite session value is
  integer-typed. A negative integer clamps to `0` and emits warning `1292
  HY000` with text `Truncated incorrect sql_select_limit value: '-1'` for the
  verified `-1` case.
- String, decimal, `NULL`, `ON`/`OFF`, and integer literals above
  `18446744073709551615` fail with error `1232 42000` and text
  `Incorrect argument type to variable 'sql_select_limit'`. String-typed user
  variables fail the same way even when their text is numeric.

## Scope

Supported SQL examples:

```sql
SET sql_select_limit = 2
SET SESSION sql_select_limit = DEFAULT
SET LOCAL sql_select_limit = +1
SET @@SESSION.sql_select_limit = 0
SET @@sql_select_limit = TRUE
SET @n = 2
SET sql_select_limit = @n
SELECT @@sql_select_limit, @@GLOBAL.sql_select_limit
SELECT id FROM t ORDER BY id
SELECT id FROM t ORDER BY id LIMIT 3
SELECT 1
SELECT grp, COUNT(*) FROM t GROUP BY grp ORDER BY grp
SELECT id FROM t WHERE id = 1 UNION ALL SELECT id FROM t WHERE id = 2
```

The implementation must:

- store `sql_select_limit` in handle-local session state and initialize it to
  `UINT64_MAX`;
- snapshot and restore it across multi-assignment `SET` failure rollback;
- preserve existing diagnostics clearing and result ownership conventions;
- preserve `warning_count == 0` for supported in-range assignments and selects;
- emit the MySQL-compatible warning for supported negative integer assignment;
- reject unsupported value forms with deterministic diagnostics;
- apply implicit caps only to top-level `SELECT` statements and final compound
  `SELECT` row sets, not to internal `INSERT ... SELECT`, `CREATE TABLE ...
  SELECT`, or scalar subquery planning paths;
- leave catalog rows, descriptor versions, descriptor caches, catalog
  generation, SQLite schema generation, and `.mylite` preamble bytes unchanged.

## Non-Goals

This feature does not implement:

- mutable server-global `sql_select_limit`;
- startup options, persisted variables, `SET_VAR` hints, Performance Schema
  variable tables, or privilege semantics;
- mysql client `--safe-updates` initialization as a bundle with
  `sql_safe_updates` and `max_join_size`;
- implicit caps for `SHOW` statements;
- general system-variable expression arithmetic beyond existing scalar
  expression support;
- row-limit effects for internal DML/DDL source selects unless a future feature
  verifies and specifies MySQL behavior for that statement family;
- SQLite fork patches.

## Ownership Boundary

- The public API remains unchanged. `mylite_execute()` owns statement
  orchestration, result ownership, diagnostics snapshots, and failure cleanup.
- Statement context continues to carry previous diagnostics, row count, found
  rows, and active statement time. No new public statement-context surface is
  needed.
- Lexer/parser/AST already admit `SET` system-variable targets and
  `SYSTEM_VARIABLE` expressions. This feature adds no new grammar tokens.
- Runtime system-variable handling owns value parsing, warnings, session-state
  mutation, scalar readback, and fixed global readback.
- Select planning owns efficient SQLite-side caps for descriptor-backed
  top-level selects that already lower to SQLite `LIMIT ?`.
- Result building owns final-row-set caps for scalar, aggregate, metadata, and
  compound paths where the current implementation already materializes a small
  MyLite-owned result.
- Catalog, storage, VFS, and physical SQLite schema are unaffected.

## Grammar

The existing MyLite grammar shape is sufficient. The admitted assignment forms
are constrained by runtime validation:

```lemon
set_stmt ::= SET set_assignment_list.
set_assignment ::= set_system_variable_target EQ expr_or_default.
set_assignment ::= scope_keyword system_variable_name EQ expr_or_default.

expr_or_default ::= expression.
expr_or_default ::= DEFAULT.

expression ::= INTEGER.
expression ::= PLUS INTEGER.
expression ::= MINUS INTEGER.
expression ::= TRUE.
expression ::= FALSE.
expression ::= USER_VARIABLE.
expression ::= LP expression RP.
expression ::= SYSTEM_VARIABLE.
```

The supported top-level query cap applies after normal grammar resolution:

```lemon
select_statement ::= SELECT select_item_list from_clause_opt select_tail_opt.
compound_select_statement ::= select_statement union_term_list.
```

## Semantics

Session storage:

- New handles start with `sql_select_limit = 18446744073709551615`.
- Close/reopen starts a new session with the default value; the value is not
  stored in `.mylite` files.
- Independent handles have independent session values.
- Global reads always return `18446744073709551615`.

Assignment:

- `DEFAULT` stores `18446744073709551615`.
- `TRUE` stores `1`; `FALSE` stores `0`.
- A nonnegative integer literal stores the exact parsed `uint64_t` value.
- A positive sign is ignored.
- A negative integer literal stores `0` and appends warning `1292 HY000`.
- An integer-typed user variable stores its current integer value using the
  same range and negative-clamp behavior.
- Unsupported literal kinds and unsigned overflow fail before mutation.
- Multi-assignment `SET` remains atomic: if any assignment fails, earlier
  session mutations in the same `SET` statement are rolled back.

Implicit row cap:

- If a top-level `SELECT` has an explicit `LIMIT`, MyLite leaves the existing
  limit unchanged.
- If a top-level descriptor, row-scalar, or grouped select has no explicit
  limit and the session value is between `0` and `INT64_MAX`, MyLite adds a
  bound SQLite `LIMIT ?`.
- If the session value is greater than `INT64_MAX`, MyLite treats it as no
  additional cap for the current physical result-size envelope.
- Scalar, aggregate, information-schema, and compound paths cap the final
  MyLite-owned result before it is returned.
- `FOUND_ROWS()` continues to reflect the row count MySQL reports for normal
  non-`SQL_CALC_FOUND_ROWS` selects: the number of visible rows after the
  implicit cap. Existing `SQL_CALC_FOUND_ROWS` behavior continues to report the
  pre-limit count.

## Diagnostics

Supported in-range assignments and selects report zero warnings. Negative
integer assignment appends:

| Code | SQLSTATE | Message |
| --- | --- | --- |
| `1292` | `HY000` | `Truncated incorrect sql_select_limit value: '<text>'` |

Unsupported assignment values fail with:

| Code | SQLSTATE | Message |
| --- | --- | --- |
| `1232` | `42000` | `Incorrect argument type to variable 'sql_select_limit'` |

`SET GLOBAL sql_select_limit` fails with MyLite's existing unsupported global
assignment diagnostic because mutable global state is intentionally outside
this embedded slice.

## Tests

Fast C tests must cover:

- default scalar/global/session/local readback and `SHOW VARIABLES` readback;
- supported `SET` spellings, `DEFAULT`, `0`, `1`, `2`, `+1`, `-1`, `TRUE`,
  `FALSE`, and integer user variables;
- unsupported string, decimal, `NULL`, `ON`, `OFF`, and overflow assignment;
- atomic rollback across multi-assignment failure;
- implicit caps for descriptor selects, scalar selects, grouped selects,
  aggregate selects, and compound final results;
- explicit `LIMIT` precedence;
- independent handle state and close/reopen default reset;
- preamble and generation preservation.

The MySQL expectation script must verify each user-visible behavior above
against MySQL 8.4.9 before the implementation is accepted.
