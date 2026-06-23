# Baseline Mutable Foreign Key Checks

## Status

This feature extends the existing `@@foreign_key_checks` scalar variable and
descriptor-owned foreign-key enforcement with a narrow mutable session mode.
It is intended to support the common import and migration pattern:

```sql
SET foreign_key_checks = 0;
-- load or rearrange rows
SET foreign_key_checks = 1;
```

The scope is deliberately limited to MyLite handles and MyLite-owned
descriptor enforcement. It does not introduce process-global mutable state,
startup options, persisted variables, `SET_VAR` hints, privilege semantics, or
new foreign-key grammar.

## Sources

- MyLite README architecture: `README.md`
- MyLite engineering standards:
  `docs/architecture/engineering-standards.md`
- Existing fixed scalar variable baseline:
  `docs/specs/baseline-foreign-key-checks-system-variable/specs.md`
- Existing descriptor foreign-key support and tests:
  `packages/libmylite/tests/runtime_foreign_key_constraints_test.c`
- MySQL 8.4 Reference Manual, server system variables:
  https://dev.mysql.com/doc/refman/8.4/en/server-system-variables.html
- MySQL 8.4 Reference Manual, dynamic system variables:
  https://dev.mysql.com/doc/refman/8.4/en/dynamic-system-variables.html
- MySQL 8.4 Reference Manual, foreign key constraints:
  https://dev.mysql.com/doc/refman/8.4/en/create-table-foreign-keys.html

This specification is independently authored from project documentation,
official MySQL 8.4 documentation, observed MySQL 8.4.9 runtime behavior,
public SQLite APIs, and existing MyLite source code. It does not copy MySQL,
MariaDB, Percona, SQLite implementation internals, or other restrictively
licensed implementation sources.

## MySQL 8.4.9 Runtime Observations

Runtime probes were executed against MySQL 8.4.9 through the local MySQL client
and socket. The expectation artifact for this feature records the exact checks
used for ongoing verification.

Observed variable behavior:

- `foreign_key_checks` has global and session scope and defaults to `1`.
- `SET foreign_key_checks = 0`, `SET SESSION foreign_key_checks = 1`,
  `SET @@SESSION.foreign_key_checks = 0`, and
  `SET @@LOCAL.foreign_key_checks = 1` mutate the current session.
- After a session assignment, unqualified, `@@session`, and `@@local` reads
  report the session value; `@@global.foreign_key_checks` reports the global
  value.
- Simple numeric expressions such as `@@foreign_key_checks + 1` are accepted
  in MySQL and preserve expression text as the result label.
- Accepted boolean values include `0`, `1`, `OFF`, `ON`, `FALSE`, `TRUE`,
  `+0`, and `+1`.
- `DEFAULT` is accepted by MySQL 8.4.9 and, in the tested runtime, sets the
  session value to `0` while leaving the global value at `1`.
- Rejected values such as `-1`, `2`, string `'0'`, and `NULL` fail with error
  `1231`, SQLSTATE `42000`, and a message saying the variable cannot be set to
  the supplied value.
- `SET GLOBAL foreign_key_checks = 0` mutates the MySQL server global value for
  users with sufficient privileges. MyLite intentionally does not implement a
  mutable process-global value in this slice.

Observed descriptor-enforcement behavior when the session value is `0`:

- Child inserts and updates that would otherwise fail for missing parents are
  allowed with no warnings.
- `INSERT IGNORE` with a missing parent inserts the row with affected rows `1`
  and no warning.
- Re-enabling `foreign_key_checks` does not retrovalidate existing rows.
- Parent `DELETE` and `UPDATE` operations do not run `CASCADE`, `SET NULL`, or
  restrict checks while checks are disabled; child rows remain unchanged and
  may become or remain orphaned.
- MySQL permits a referenced parent table to be dropped while checks are
  disabled. MyLite intentionally defers that DDL implication because the
  current descriptor catalog stores resolved parent table IDs and cannot yet
  preserve a child foreign key that references a temporarily missing parent by
  name.
- Dropping a required foreign-key index remains rejected even while checks are
  disabled.

## Scope

The implementation must add:

- handle-local mutable session state for `foreign_key_checks`;
- scalar `SELECT` and `SHOW VARIABLES` reads that report the session value for
  unqualified, `session`, and `local` scopes while keeping global reads fixed at
  `1`;
- accepted session `SET` forms using existing grammar for identifier targets
  and `@@` system-variable targets;
- MySQL-compatible value conversion for the admitted boolean subset;
- deterministic error `1231` / SQLSTATE `42000` for unsupported assignment
  values;
- enforcement gates in descriptor-owned foreign-key DML checks, parent
  referential actions, and `INSERT IGNORE` missing-parent warnings/skips;
- fast C tests and a MySQL 8.4.9 expectation artifact.

Supported SQL examples:

```sql
SET foreign_key_checks = 0
SET SESSION foreign_key_checks = 1
SET LOCAL foreign_key_checks = OFF
SET @@foreign_key_checks = ON
SET @@session.foreign_key_checks = FALSE
SET @@local.`foreign_key_checks` = TRUE
SET foreign_key_checks = +1
SELECT @@foreign_key_checks, @@global.foreign_key_checks
SELECT @@foreign_key_checks + 1
SHOW VARIABLES WHERE Variable_name = 'foreign_key_checks'
```

## Non-Goals

This feature must not implement:

- mutable global `foreign_key_checks`, process-global defaults, startup
  options, persisted variables, privilege semantics, or `SET_VAR` hints;
- user variables used to save and restore the previous value;
- new foreign-key grammar, cross-schema foreign keys, non-integer foreign keys,
  recursive cascades, triggers, or deferred checks;
- validation scans when checks are re-enabled;
- disabling descriptor validation for newly created or altered malformed
  foreign-key definitions;
- dropping required FK indexes while checks are disabled;
- referenced-parent `DROP TABLE` and `TRUNCATE TABLE` behavior changes unless
  a later feature specifies and tests name-preserving descriptor handling;
- SQLite `PRAGMA foreign_keys`; MyLite descriptors remain authoritative;
- SQLite fork patches.

## Ownership Boundary

- The public API remains unchanged. `mylite_execute()` continues to own public
  validation, parse/execution orchestration, result ownership, diagnostics
  replacement, row-count state, and failure cleanup.
- Statement context owns diagnostics lifecycle. Successful `SET
  foreign_key_checks` statements are non-row statements with affected rows `0`
  and warning count `0`.
- Lexer/parser/AST already admit the required `SET` and system-variable
  syntax. No new grammar is needed.
- Runtime system-variable execution owns target resolution, session/global
  scope handling, value conversion, diagnostics, and updates to the
  handle-local session flag.
- Runtime DML execution owns enforcement gates around descriptor-built
  child-side probes, parent-side probes, and direct parent referential actions.
- Catalog descriptors remain authoritative for foreign-key metadata. Disabling
  checks does not mutate descriptor rows, descriptor versions, catalog
  generation, or SQLite schema generation.
- Storage, VFS, and SQLite physical row storage remain unchanged. Physical rows
  continue to be stored and queried through the existing generated SQLite
  tables; MyLite decides whether to run its descriptor-enforcement SQL around
  those writes.

## Supported SQL Grammar

This slice uses existing MyLite grammar. The relevant independently authored
subset is:

```lemon
set_statement ::= SET set_system_variable_target EQ set_value.
set_system_variable_target ::= identifier.
set_system_variable_target ::= identifier DOT identifier.
set_system_variable_target ::= SYSTEM_VARIABLE.
set_value ::= DEFAULT.
set_value ::= expression.

expression ::= literal.
expression ::= unary_plus_expression.
```

The target must resolve to `foreign_key_checks`. `GLOBAL` targets are parsed
but rejected for this slice. Accepted effective scopes are none, `SESSION`,
`LOCAL`, `@@`, `@@session`, and `@@local`.

## Variable Resolution And Values

The existing system-variable resolver remains case-insensitive for unquoted
names and accepts a quoted final variable-name component. Quoted scopes are
still rejected.

Reads use this table:

| Expression | Value source |
| --- | --- |
| `@@foreign_key_checks` | current handle session flag |
| `@@session.foreign_key_checks` | current handle session flag |
| `@@local.foreign_key_checks` | current handle session flag |
| `@@global.foreign_key_checks` | fixed MyLite global baseline `1` |

`SHOW VARIABLES` follows the same session/global split: session and unqualified
forms render `ON` or `OFF` from the current handle; global forms render `ON`.

Accepted assignment values:

| SQL value | Stored session flag |
| --- | --- |
| `0`, `+0`, `OFF`, `FALSE` | disabled |
| `1`, `+1`, `ON`, `TRUE` | enabled |
| `DEFAULT` | disabled, matching observed MySQL 8.4.9 behavior |

Unsupported values return `1231` / `42000` with deterministic text containing
`Variable 'foreign_key_checks' can't be set to the value of ...`.

## DML And DDL Semantics

When the current handle's session flag is enabled, descriptor-owned foreign-key
behavior remains unchanged:

- child writes must match an existing parent key unless `MATCH SIMPLE` NULL
  tuple behavior applies;
- parent deletes and updates enforce `RESTRICT` and `NO ACTION`;
- direct non-recursive `CASCADE` and `SET NULL` actions run for supported
  parent `DELETE` and `UPDATE`;
- `INSERT IGNORE` skips missing-parent rows and reports warnings through the
  existing baseline behavior;
- dropping a referenced parent table is rejected.

When the session flag is disabled:

- child-side missing-parent checks are skipped for `INSERT`, `INSERT IGNORE`,
  `INSERT ... SELECT`, `REPLACE`, and `UPDATE`;
- parent-side referenced-row checks are skipped for supported `DELETE`,
  `UPDATE`, `REPLACE` conflict deletes, and any other existing path using the
  descriptor parent-validation helper;
- parent `CASCADE` and `SET NULL` actions are skipped, so the parent statement
  changes only the parent rows and child rows remain as stored;
- `INSERT IGNORE` does not downgrade missing-parent rows to warnings because no
  foreign-key check is performed;
- re-enabling checks does not scan or modify existing rows;
- referenced-parent `DROP TABLE` remains rejected in this slice because the
  current descriptor catalog cannot preserve a child foreign key against a
  missing parent descriptor and later rebind it on parent re-creation;
- required foreign-key indexes and malformed new foreign-key definitions remain
  subject to existing descriptor validation.

This is a MyLite-side wrapper/translation feature. It relies on existing public
SQLite prepared statements for physical reads and writes and does not use
SQLite foreign-key enforcement or new SQLite extension points.

## Diagnostics

This slice uses existing diagnostics for:

- syntax errors and unsupported `SET` grammar;
- unknown system variables: `1193` / `HY000`;
- `SET GLOBAL foreign_key_checks = ...`: MyLite-specific unsupported global
  assignment diagnostic;
- unsupported assignment values: `1231` / `42000`;
- malformed quoted scopes and unsupported expression values;
- DDL and DML foreign-key errors when the session flag is enabled;
- allocation failures and public API misuse through existing paths.

Successful supported `SET` statements return no rows, affected rows `0`, and
warning count `0`.

## Storage And Performance

The session flag is handle-local memory. It is not serialized into `.mylite`
files, not written to the catalog, and reset to enabled when a handle is
opened. Disabling checks improves load paths by skipping MyLite's descriptor
validation probes and referential-action statements. User row writes still go
through the same SQLite physical tables, so no additional row materialization
or SQLite fork patch is introduced.

## Test Plan

Fast C tests must cover:

- initial scalar and `SHOW VARIABLES` values;
- session/local/unqualified/`@@` assignment forms and independent handles;
- close/reopen reset to enabled while row data persists;
- valid values `0`, `1`, `OFF`, `ON`, `FALSE`, `TRUE`, `+0`, `+1`, and
  `DEFAULT`;
- invalid values `-1`, `2`, string `'0'`, and `NULL`;
- global assignment rejection and fixed global reads;
- orphan child insert/update, `INSERT IGNORE`, no retrovalidation after
  re-enable, and future failure after re-enable;
- parent `DELETE` and `UPDATE` with `CASCADE` while checks are disabled leave
  child rows unchanged;
- referenced-parent `DROP TABLE` and required FK index drops remain rejected
  while disabled;
- catalog generation, SQLite schema generation, and `.mylite` preamble safety
  except for intentional DDL/DML changes.

MySQL expectation tests must record the runtime behavior for the public SQL
surface and each DML/DDL side effect admitted by this feature.
