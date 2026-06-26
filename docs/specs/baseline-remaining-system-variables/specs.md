# Baseline Remaining System Variables

## Scope

This slice completes the remaining MySQL 8.4.9 runtime system-variable baseline
rows for:

- `insert_id`
- `open_files_limit`
- `pseudo_thread_id`
- `rand_seed1`
- `rand_seed2`
- `statement_id`
- `temptable_max_ram`

The compatibility authority is the MySQL 8.4 Reference Manual server system
variable page, especially the documented system-variable concepts around
runtime, read-only, global, and session variables:
https://dev.mysql.com/doc/refman/8.4/en/server-system-variables.html. Exact
values, warnings, and side effects are verified against a MySQL 8.4.9 runtime.

No new SQL grammar is required. These variables already use MyLite's existing
system-variable scalar and `SET` syntax:

```lemon
expr ::= SYSTEM_VARIABLE.
set_statement ::= SET set_assignment_list.
set_system_variable_statement ::= SET system_variable_target EQ set_value.
system_variable_target ::= system_variable_scope_opt ident.
```

## MySQL-Observed Behavior

`insert_id` is a session variable. Scalar reads and `SHOW VARIABLES` initially
return `0`. `SET insert_id = N` stores the next generated AUTO_INCREMENT value
for the next insert that needs one. After a successful generated
AUTO_INCREMENT insert, MySQL resets `@@insert_id` to `0` and updates
`LAST_INSERT_ID()` to the first generated value. Negative assignments clamp to
`0` and emit warning 1292. `DEFAULT`, `NULL`, non-integer values, and
`SET GLOBAL` return MySQL diagnostics.

`pseudo_thread_id` is a session variable sharing the visible value returned by
`CONNECTION_ID()`. It is mutable by `SET`, `DEFAULT` sets it to `0`, negative
values clamp to `0` with warning 1292, and values above the 32-bit unsigned
range clamp to `4294967295` with warning 1292. `SET GLOBAL` is rejected as a
session-variable assignment.

`rand_seed1` and `rand_seed2` are session variables that always read and show
as `0`, but assignments seed the subsequent unseeded `RAND()` stream. MySQL
uses the assigned integer words directly as RAND state modulo `0x3fffffff`.
Negative values clamp to `0` with warning 1292. `DEFAULT`, `NULL`,
non-integer values, and `SET GLOBAL` return MySQL diagnostics.

`statement_id` is a read-only session variable. It advances once per statement
on the MySQL server, including statements that error. MyLite must expose a
monotonic per-handle counter but does not need to match MySQL's server-global
starting value. `SET statement_id = ...` fails as read-only.

`open_files_limit` is a read-only global variable that also appears in
`SHOW SESSION VARIABLES`. Scalar `@@SESSION.open_files_limit` is rejected as a
global-only read. `SET GLOBAL` and `SET SESSION` both fail as read-only.
MyLite reports the MySQL default-shape value derived from the default
`max_connections` and `table_open_cache` values, matching the observed
`10 + max_connections + (2 * table_open_cache)` baseline.

`temptable_max_ram` is a global variable that also appears in
`SHOW SESSION VARIABLES`. Scalar `@@SESSION.temptable_max_ram` is rejected as a
global-only read. `SET SESSION` fails with the global-variable SET diagnostic.
`SET GLOBAL temptable_max_ram = N` stores a handle-local placeholder value, and
`SET GLOBAL temptable_max_ram = DEFAULT` returns to the computed default. The
default is 3% of physical memory, bounded to the MySQL-observed minimum of 1
GiB and maximum of 4 GiB.

## MyLite Semantics

The implementation belongs in MyLite runtime/session state, not in the SQLite
fork:

- scalar and SHOW values are handled in the system-variable runtime;
- SET behavior is handled by a dedicated remaining-system-variable SET module;
- `insert_id` only affects MyLite AUTO_INCREMENT generation;
- `rand_seed1/2` only affect MyLite's unseeded `RAND()` stream;
- `pseudo_thread_id` mutates the handle-local connection id used by
  `CONNECTION_ID()`;
- `statement_id` is a handle-local monotonic statement counter.

The embedded design does not implement real server file-descriptor governance,
server-global temp-table memory governance shared across connections, or
cross-process statement/thread identities.

## Diagnostics And Warnings

Expected diagnostics are MySQL-runtime verified:

- session-only variable used with `SET GLOBAL`: 1228 / `HY000`
- variable without default: 1230 / `42000`
- incorrect argument type: 1232 / `42000`
- global-only scalar read: 1238 / `HY000`
- read-only variable: 1238 / `HY000`
- global variable assigned without `SET GLOBAL`: 1229 / `HY000`
- truncated negative or out-of-range integer: warning 1292 / `HY000`

## Tests

The MySQL expectation script records the authoritative MySQL 8.4.9 behavior.
The runtime test mirrors it against MyLite and additionally verifies:

- `insert_id` uses the next generated AUTO_INCREMENT value, then resets;
- multi-row inserts allocate a sequence starting from `insert_id`;
- `pseudo_thread_id` and `CONNECTION_ID()` share state;
- `rand_seed1/2` seed unseeded `RAND()` deterministically;
- `statement_id` increases after successful and failed statements;
- SHOW and scalar scope behavior match the documented baseline.
