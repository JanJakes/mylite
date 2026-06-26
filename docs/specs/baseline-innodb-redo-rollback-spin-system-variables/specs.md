# Baseline InnoDB Redo, Rollback, And Spin System Variables

## Scope

This slice adds MySQL 8.4.9-shaped metadata placeholders for these adjacent
InnoDB system variables:

- `innodb_redo_log_archive_dirs`
- `innodb_redo_log_capacity`
- `innodb_redo_log_encrypt`
- `innodb_replication_delay`
- `innodb_rollback_on_timeout`
- `innodb_rollback_segments`
- `innodb_segment_reserve_factor`
- `innodb_sort_buffer_size`
- `innodb_spin_wait_delay`
- `innodb_spin_wait_pause_multiplier`

The official MySQL 8.4 Reference Manual lists these as InnoDB variables for
redo-log archive configuration and capacity, redo-log encryption, replication
delay, rollback behavior, rollback segment counts, segment page reservation,
sorted index builds, and spin-wait tuning.

Reference:

- <https://dev.mysql.com/doc/refman/8.4/en/innodb-parameters.html>

## Observed MySQL 8.4.9 Behavior

Runtime probes against `mysql:8.4.9` showed these defaults and scopes:

| Variable | Scalar value | SHOW value | Scope | Mutation |
| --- | --- | --- | --- | --- |
| `innodb_redo_log_archive_dirs` | `NULL` | empty string | global | dynamic global |
| `innodb_redo_log_capacity` | `104857600` | `104857600` | global | dynamic global |
| `innodb_redo_log_encrypt` | `0` | `OFF` | global | dynamic global |
| `innodb_replication_delay` | `0` | `0` | global | dynamic global |
| `innodb_rollback_on_timeout` | `0` | `OFF` | global | read-only |
| `innodb_rollback_segments` | `128` | `128` | global | dynamic global |
| `innodb_segment_reserve_factor` | `12.500000` | `12.500000` | global | dynamic global |
| `innodb_sort_buffer_size` | `1048576` | `1048576` | global | read-only |
| `innodb_spin_wait_delay` | `6` | `6` | global | dynamic global |
| `innodb_spin_wait_pause_multiplier` | `50` | `50` | global | dynamic global |

All variables appear in default, global, and session `SHOW VARIABLES` output.
For every variable in this slice, scalar `@@SESSION` and `@@LOCAL` reads fail
with `1238/HY000` and text containing `Variable '<name>' is a GLOBAL variable`.

For dynamic global-only variables, unqualified, `SESSION`, and `LOCAL`
assignments fail with `1229/HY000`. `SET GLOBAL ... = DEFAULT` succeeds.
`innodb_redo_log_archive_dirs` also accepts `SET GLOBAL ... = NULL`; unsupported
strings fail with `1231/42000`. Numeric and decimal variables reject `NULL` and
string values with `1232/42000`. Boolean variables reject `NULL` and unsupported
strings with `1231/42000`.

For read-only variables, every assignment scope fails with `1238/HY000` and
text containing `read only variable`.

## MyLite Semantics

MyLite exposes fixed placeholder values matching the observed defaults through:

- `SELECT @@variable`
- `SELECT @@GLOBAL.variable`
- `SHOW VARIABLES LIKE ...`
- `SHOW GLOBAL VARIABLES LIKE ...`
- `SHOW SESSION VARIABLES LIKE ...`

For all variables in this slice, `@@SESSION.variable` and `@@LOCAL.variable`
return the MySQL-shaped global-variable diagnostic.

For dynamic global-only variables, MyLite accepts `SET GLOBAL variable =
DEFAULT` and exact fixed-value `SET GLOBAL` assignments as no-ops. Alternate
global values are rejected with a MyLite fixed-placeholder unsupported
diagnostic instead of mutating shared server-global state. For
`innodb_redo_log_archive_dirs`, exact fixed-value assignment means `NULL`.

For read-only variables, every direct and user-variable assignment path returns
the MySQL-shaped read-only diagnostic before value validation.

The baseline deliberately does not implement redo-log archiving, physical
redo-log capacity changes, redo-log encryption, replication delay behavior,
timeout rollback policy changes, rollback-segment allocation, segment page
reservation changes, sorted index build buffer allocation, spin-wait tuning,
persisted variables, startup option handling, privilege checks, Performance
Schema variable tables, or cross-handle global mutation.

## Parser And Runtime Design

No new grammar is required. Existing system-variable scalar and `SET` syntax
covers the baseline:

```lemon
set_statement ::= SET set_assignment_list.
set_assignment ::= system_variable_target EQ set_value.
system_variable_target ::= GLOBAL ident.
system_variable_target ::= SESSION ident.
system_variable_target ::= LOCAL ident.
scalar_expression ::= system_variable_reference.
system_variable_reference ::= AT_AT ident.
system_variable_reference ::= AT_AT GLOBAL DOT ident.
system_variable_reference ::= AT_AT SESSION DOT ident.
```

Runtime implementation adds descriptors for the variables, fixed scalar/SHOW
value cases, global-only/read-only scope wiring, fixed no-op global assignment
validation, fixed-null validation for the nullable redo-log archive variable,
and exact decimal text validation for `innodb_segment_reserve_factor`.

This is pure MyLite runtime logic. It does not require a SQLite public
extension API, wrapper translation, file-format change, or targeted SQLite fork
hook.

## Tests

- `packages/libmylite/tests/mysql_baseline_innodb_redo_rollback_spin_system_variables_expectations.sh`
  verifies the observed MySQL 8.4.9 expectations.
- `packages/libmylite/tests/runtime_innodb_redo_rollback_spin_system_variables_test.c`
  verifies MyLite scalar reads, `SHOW VARIABLES`, scope diagnostics,
  read-only diagnostics, accepted fixed global no-ops, rejected alternate
  values, user-variable assignment paths, NULL scalar rendering, and fixed
  decimal assignment forms.
