# Baseline AUTO_INCREMENT Step System Variables

## Status

This feature adds the first MyLite-owned compatibility slice for
`auto_increment_increment` and `auto_increment_offset`. It builds on the
existing descriptor-owned `AUTO_INCREMENT` lifecycle, `SET` system-variable
grammar, scalar `@@` reads, `SHOW VARIABLES`, session state, and durable
catalog table counters.

The slice is intentionally narrow. It supports session-local reads and writes
for the two variables, fixed global readback, and descriptor-driven allocation
for generated values when `1 <= auto_increment_offset <=
auto_increment_increment <= 65535`. It does not implement mutable process-wide
global values, replication behavior, Group Replication rewriting, `SET_VAR`
optimizer hints, concurrent InnoDB auto-increment lock modes, or MySQL's
surprising observed generated-value behavior when the offset exceeds the
increment.

## Sources

- MyLite README architecture: `README.md`
- MyLite engineering standards:
  `docs/architecture/engineering-standards.md`
- Baseline fixed SET system variables:
  `docs/specs/baseline-set-fixed-system-variables/specs.md`
- Baseline SHOW VARIABLES:
  `docs/specs/baseline-show-variables/specs.md`
- Baseline SHOW VARIABLES WHERE:
  `docs/specs/baseline-show-variables-where/specs.md`
- Baseline AUTO_INCREMENT lifecycle:
  `docs/specs/baseline-auto-increment-lifecycle/specs.md`
- Baseline ALTER TABLE AUTO_INCREMENT option:
  `docs/specs/baseline-alter-table-auto-increment-option/specs.md`
- Runtime session and SQL mode state:
  `docs/specs/baseline-sql-mode-session-state/specs.md`
- MySQL 8.4 Reference Manual, server system variable reference:
  https://dev.mysql.com/doc/refman/8.4/en/server-system-variable-reference.html
- MySQL 8.4 Reference Manual, replication source variables:
  https://dev.mysql.com/doc/refman/8.4/en/replication-options-source.html
- MySQL 8.4 Reference Manual, InnoDB `AUTO_INCREMENT` handling:
  https://dev.mysql.com/doc/refman/8.4/en/innodb-auto-increment-handling.html
- MySQL 8.4 Reference Manual, `SET` variable assignment:
  https://dev.mysql.com/doc/refman/8.4/en/set-variable.html
- MySQL 8.4 Reference Manual, `SHOW VARIABLES`:
  https://dev.mysql.com/doc/refman/8.4/en/show-variables.html

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior, public
SQLite APIs, and existing MyLite source code. It does not copy MySQL, MariaDB,
Percona, SQLite implementation internals, or other restrictively licensed
implementation sources.

## MySQL 8.4.9 Observations

The expectation script
`packages/libmylite/tests/mysql_baseline_auto_increment_step_system_variables_expectations.sh`
records runtime probes for this feature. Observed behavior that shapes this
slice:

- both variables have global and session scope, are dynamic, and default to
  `1`;
- scalar reads return decimal text for `@@auto_increment_increment`,
  `@@SESSION.auto_increment_increment`, `@@LOCAL.auto_increment_offset`, and
  global-qualified variants;
- `SHOW VARIABLES LIKE 'auto_inc%'`, `SHOW SESSION VARIABLES`, `SHOW LOCAL
  VARIABLES`, and `SHOW GLOBAL VARIABLES` expose both rows as decimal text;
- unqualified `SET auto_increment_increment = N`, `SET SESSION ...`,
  `SET LOCAL ...`, `SET @@name = N`, `SET @@SESSION.name = N`, and
  `SET @@LOCAL.name = N` update the current session value;
- `SET @@GLOBAL.auto_increment_increment = N` updates MySQL's global value
  without changing the current session value, but MyLite defers mutable global
  state for this baseline;
- `DEFAULT` restores the session value to `1`;
- unsigned integer assignments in `1..65535` succeed with no warnings;
- `+N` succeeds for in-range values;
- `0` and negative integer assignments clamp to `1` and append warning
  `1292 / HY000`, `Truncated incorrect auto_increment_increment value:
  '<value>'` or the corresponding offset-variable message;
- integer assignments greater than `65535` clamp to `65535` and append the
  same warning form;
- `TRUE` stores `1` without a warning, and `FALSE` clamps to `1` with warning
  `1292`;
- string, decimal, `NULL`, and other non-integer assignment values fail with
  `1232 / 42000`, `Incorrect argument type to variable '<name>'`;
- with `auto_increment_increment = 10` and
  `auto_increment_offset = 5`, generated values for a new table are
  `5, 15, 25, ...`;
- after changing the variables, the next generated value for an existing table
  is the least value in the configured series that is greater than or equal to
  the table's stored lower-bound counter and greater than the current maximum
  row value represented by that counter;
- `CREATE TABLE ... AUTO_INCREMENT = N` and `ALTER TABLE ...
  AUTO_INCREMENT = N` render the raw lower-bound counter `N` until the next
  generated insert; if `N` is not on the configured series, the generated value
  is the next series value greater than or equal to `N`;
- explicit `INSERT` values greater than or equal to the current counter store
  `explicit_value + 1` as the rendered next lower bound, not the next value in
  the configured generated series;
- updating an auto-increment column to a larger positive value stores the next
  generated series value greater than the updated value;
- when the generated series reaches the positive range maximum, MySQL keeps the
  rendered next counter at the maximum so the next generated insert fails
  through the existing duplicate/out-of-range path;
- when `auto_increment_offset > auto_increment_increment`, official MySQL
  documentation says the offset is ignored, but observed MySQL 8.4.9 behavior
  is not the simple documented default-offset series. MyLite deliberately
  rejects generated allocation in that state for this baseline instead of
  approximating an unclear edge case.

## Scope

The implementation must add:

- session fields for `auto_increment_increment` and
  `auto_increment_offset`, initialized to `1` per handle;
- scalar `@@` reads for both variables with no warning;
- fixed global scalar readback exposing `1` for both variables;
- `SHOW VARIABLES`, `SHOW SESSION VARIABLES`, `SHOW LOCAL VARIABLES`, and
  `SHOW GLOBAL VARIABLES` rows for both variables;
- `SHOW VARIABLES WHERE` support through the existing output-row predicate
  evaluator because the rows are added to the shared registry;
- mutable session/local/unqualified `SET` and `@@` assignments for both
  variables;
- exact no-op behavior for `DEFAULT` and in-range values with affected rows
  `0` and warning count `0`;
- MySQL-compatible clamp warnings for integer values outside `1..65535`;
- MySQL-compatible incorrect-argument diagnostics for unsupported assignment
  value types;
- descriptor-driven generated auto-increment allocation using the current
  session values when `offset <= increment`;
- table-option and ALTER-table lower-bound handling that preserves existing
  raw descriptor counters and computes the generated value from the current
  session series at insert time;
- explicit `INSERT` counter advancement using the existing lower-bound rule;
- auto-increment-column `UPDATE` counter advancement using the current session
  generated series;
- support for persistent and session-temporary auto-increment tables because
  both already use the same descriptor-owned allocation path;
- reopen persistence of table counters, while variable values remain
  connection-local and reset to defaults on a new handle;
- independent handles with independent session variable values;
- `.mylite` preamble preservation and no SQLite fork changes;
- MySQL 8.4.9 expectation coverage and fast C runtime tests.

## Non-Goals

This feature must not implement:

- mutable process-wide global variable values;
- `SET PERSIST`, `SET PERSIST_ONLY`, startup option parsing, option files, or
  persisted variables;
- `SET_VAR` optimizer hints;
- replication, source/replica collision planning, Group Replication automatic
  changes, binary logging, or GTID interactions;
- `innodb_autoinc_lock_mode` or concurrent InnoDB reservation behavior;
- allocation when `auto_increment_offset > auto_increment_increment`;
- mixed explicit/generated multi-row auto-increment allocation gaps beyond the
  current MyLite-supported modes;
- table-backed `INSERT ... SELECT` into auto-increment tables beyond the
  existing admitted one-row no-source/`DUAL` shape;
- broader arithmetic/string expression assignment for system variables;
- public API or ABI changes;
- SQLite fork patches.

## Ownership Boundary

- Public API remains unchanged. `mylite_execute()` owns public call
  validation, result-handle ownership, diagnostics, and statement-boundary
  cleanup.
- Statement context owns successful `SET` result conventions, affected rows
  `0`, warning count, previous row count, and diagnostic preservation.
- Session state owns the two mutable values per `mylite_db` handle. New handles
  start from fixed defaults; no process-global mutable store is introduced.
- Lexer/parser/AST already own the admitted `SET` target/value forms and
  scalar `@@` token. This feature should not add grammar beyond documenting the
  existing narrow subset.
- Runtime/analyzer resolves variable names through the existing system-variable
  registry and validates assignments before mutating session state.
- Insert/update planning resolves auto-increment columns from MyLite
  descriptors and computes generated integer values before binding SQLite
  statements.
- Catalog owns durable table counters. The table counter remains a lower bound
  used by generated allocation, not a SQLite rowid or `sqlite_sequence` value.
- Result builders render scalar and `SHOW VARIABLES` values from MyLite session
  and fixed global state.
- SQLite physical storage stores rows and catalog updates through ordinary
  prepared statements. Generated values are bound as integers. No generated SQL
  is based on user text.
- Storage/VFS owns the `.mylite` preamble and shifted SQLite payload boundary;
  this feature writes only user rows and existing catalog counter rows inside
  the shifted SQLite payload.

## Supported SQL Surface

The feature uses the existing MyLite `SET` and scalar-expression grammar:

```ebnf
set_system_variable_statement:
    SET set_system_variable_target = set_system_variable_value

set_system_variable_target:
    identifier
  | SESSION identifier
  | LOCAL identifier
  | GLOBAL identifier
  | system_variable

set_system_variable_value:
    DEFAULT
  | unsigned_decimal_integer_literal
  | + unsigned_decimal_integer_literal
  | - unsigned_decimal_integer_literal
  | TRUE
  | FALSE
  | NULL
  | string_literal
```

Only the integer, signed integer, boolean, and `DEFAULT` values can succeed for
the two variables. `NULL`, string literals, decimal literals, function calls,
parameters, subqueries, assignment lists, `:=`, user variables, and general
expressions fail through existing parse or runtime diagnostics.

### MyLite Lemon-Syntax Snippet

No new grammar production is required. The existing independently authored
grammar shape is:

```lemon
set_system_variable_statement(A) ::=
    SET(S) set_system_variable_target(T) EQUAL set_system_variable_value(V). {
    A = mylite_sql_parser_make_set_system_variable_statement(state, S, T, V);
}

set_system_variable_target(A) ::= identifier(N). {
    A = mylite_sql_parser_make_set_system_variable_target(state, NULL, N);
}
set_system_variable_target(A) ::= SESSION(S) identifier(N). {
    A = mylite_sql_parser_make_set_system_variable_target(
        state,
        mylite_sql_parser_make_identifier(state, S),
        N);
}
set_system_variable_target(A) ::= LOCAL(L) identifier(N). {
    A = mylite_sql_parser_make_set_system_variable_target(
        state,
        mylite_sql_parser_make_identifier(state, L),
        N);
}
set_system_variable_target(A) ::= GLOBAL(G) identifier(N). {
    A = mylite_sql_parser_make_set_system_variable_target(
        state,
        mylite_sql_parser_make_identifier(state, G),
        N);
}
set_system_variable_target(A) ::= SYSTEM_VARIABLE(T). {
    A = mylite_sql_parser_make_set_system_variable_target(
        state,
        NULL,
        mylite_sql_parser_make_system_variable(state, T));
}
```

These snippets describe MyLite's current admitted subset and are not MySQL's
full grammar.

## Variable Semantics

`auto_increment_increment` and `auto_increment_offset` are integer variables.

Scalar session reads:

- `@@auto_increment_increment`;
- `@@SESSION.auto_increment_increment`;
- `@@LOCAL.auto_increment_increment`;
- the same forms for `auto_increment_offset`;
- backtick-quoted variable names inside the existing `@@` token where already
  supported.

Global reads:

- `@@GLOBAL.auto_increment_increment`;
- `@@GLOBAL.auto_increment_offset`.

Global reads return MyLite's fixed default `1`. Session and local reads return
the current connection-local value.

`SHOW VARIABLES` rows use the same decimal text as scalar reads. `SHOW GLOBAL
VARIABLES` exposes the fixed global default `1`; session/local/default `SHOW`
exposes the current session values.

Successful session assignment changes only the current handle:

| Value form | Stored value | Warning |
| --- | --- | --- |
| `DEFAULT` | `1` | none |
| integer `1..65535` | exact value | none |
| `+N` where `N` is `1..65535` | exact value | none |
| `0` | `1` | warning `1292 / HY000` |
| negative integer | `1` | warning `1292 / HY000` |
| integer `> 65535` | `65535` | warning `1292 / HY000` |
| `TRUE` | `1` | none |
| `FALSE` | `1` | warning `1292 / HY000` |

String, decimal, `NULL`, and nonliteral values fail with `1232 / 42000`.

`SET GLOBAL` and `@@GLOBAL` assignments are rejected with a MyLite-specific
unsupported diagnostic before any state changes. This follows the existing
embedded-design policy for avoiding mutable process-global state unless a later
feature introduces a deliberate global configuration owner.

## Allocation Semantics

For generated allocation, let:

- `increment = session.auto_increment_increment`;
- `offset = session.auto_increment_offset`;
- `lower_bound = table.auto_increment_next`;
- `positive_max = maximum positive value representable by the descriptor's
  currently supported integer range`.

If `offset > increment`, generated allocation fails with a deterministic
unsupported diagnostic. Existing rows, descriptors, counters, and
`LAST_INSERT_ID()` remain unchanged.

Otherwise, the generated value is the least member of the series

```text
offset, offset + increment, offset + 2 * increment, ...
```

that is greater than or equal to `lower_bound`. If that generated value exceeds
`positive_max`, execution fails through the current generated-auto-increment
range diagnostic path. If it is within range, MyLite binds it as the ordinary
row value and records it as generated for `LAST_INSERT_ID()`.

After a generated row succeeds:

- if the generated value equals `positive_max`, keep the stored counter at
  `positive_max`, preserving the current duplicate-key behavior for a later
  generated attempt at the maximum;
- else if `generated + increment` would exceed `positive_max`, store
  `positive_max` as the next lower bound;
- otherwise store `generated + increment`.

After an explicit positive `INSERT` value succeeds:

- if `value < current_counter`, keep the counter unchanged;
- if `value >= current_counter`, store `min(value + 1, positive_max)`.

After an `UPDATE` assigns a positive value to an auto-increment column and
affects at least one row:

- if `value < current_counter`, keep the counter unchanged;
- otherwise store the least generated-series value greater than `value`,
  capped to `positive_max` using the same maximum handling as generated rows.

`CREATE TABLE ... AUTO_INCREMENT=N` and `ALTER TABLE ... AUTO_INCREMENT=N`
continue to store raw lower-bound counters as the existing descriptor lifecycle
does. A following generated insert applies the current session series at that
time. Changing the session variables after table creation therefore affects
later generated values for all auto-increment tables used by the current
handle.

Temporary auto-increment tables use the same connection-local variable values
and session-local table counters. Closing the handle drops temporary
descriptors and resets variable values.

## Diagnostics

Successful supported statements:

- return `MYLITE_OK`;
- `SET` returns no result columns or rows;
- `SET` reports affected rows `0`;
- supported in-range `SET` reports warning count `0`;
- supported generated inserts preserve existing insert affected-row and
  `LAST_INSERT_ID()` behavior;
- `SHOW VARIABLES` returns row results through existing result conventions.

Diagnostics for this baseline:

- unknown variables use existing `1193 / HY000`;
- unsupported global assignment uses a deterministic MyLite unsupported
  diagnostic;
- noninteger assignment values use `1232 / 42000`,
  `Incorrect argument type to variable '<name>'`;
- clamped integer and `FALSE` assignments append warning `1292 / HY000`,
  `Truncated incorrect <variable> value: '<text>'`;
- unsupported allocation with `offset > increment` uses a deterministic MyLite
  unsupported diagnostic;
- generated values outside the descriptor's supported positive range use the
  current auto-increment range diagnostics;
- allocation failure returns `MYLITE_NOMEM`;
- physical SQLite failures keep existing physical row diagnostics;
- public API misuse is unchanged because there is no public surface change.

## Tests

Fast C tests must cover:

- scalar reads for session, local, global, case-insensitive, and quoted names;
- `SHOW VARIABLES`, `SHOW SESSION`, `SHOW LOCAL`, `SHOW GLOBAL`, `LIKE`, and
  existing `WHERE` filtering over the new rows;
- successful `SET` forms for unqualified, session, local, and `@@` targets;
- `DEFAULT`, `+N`, `TRUE`, `FALSE`, `0`, negative, and high integer
  assignments including warnings and clamped readback;
- string, decimal, `NULL`, function, and global-assignment diagnostics;
- independent handles with independent session values and fixed global reads;
- generated inserts for `increment=10`, `offset=5`;
- table-option lower-bound handling with off-series values;
- explicit insert counter advancement and following generated inserts;
- update counter advancement and following generated inserts;
- `LAST_INSERT_ID()`, affected rows, warning counts, and remaining rows;
- reopen persistence of table counters and reset of session variable values;
- temporary table generated allocation;
- `offset > increment` readback plus deterministic allocation rejection;
- `.mylite` preamble preservation.

The MySQL expectation script must verify every user-visible behavior admitted
by this feature against MySQL 8.4.9, and must record MySQL's accepted but
deferred `offset > increment` behavior without requiring MyLite to match it in
this baseline.
