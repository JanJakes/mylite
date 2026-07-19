# Baseline CLIENT_FOUND_ROWS

## Status

This feature specifies the limited `MYSQLI_CLIENT_FOUND_ROWS` connection
policy implemented by MyLite's mysqli replacement. It extends the existing
UPDATE affected-row, SQL `ROW_COUNT()`, and native prepared-statement surfaces.

## Sources

- MyLite engineering standards:
  `docs/architecture/engineering-standards.md`
- Baseline row-count function:
  `docs/specs/baseline-row-count-function/specs.md`
- Native prepared-statement binding:
  `docs/specs/native-prepared-statement-binding/specs.md`
- PHP extension API:
  `docs/api/php-extensions.md`
- MySQL 8.4 C API `mysql_real_connect()`:
  https://dev.mysql.com/doc/c-api/8.4/en/mysql-real-connect.html
- MySQL 8.4 C API `mysql_affected_rows()`:
  https://dev.mysql.com/doc/c-api/8.4/en/mysql-affected-rows.html

This specification is independently authored from official MySQL 8.4
documentation, observed MySQL 8.4.9 runtime behavior, PHP's public mysqli API,
and existing MyLite source code. It does not copy implementation source from
MySQL, PHP, MariaDB, or another compatibility layer.

## MySQL 8.4.9 Runtime Observations

The paired expectation script
`packages/libmylite/tests/mysql_baseline_client_found_rows_expectations.sh`
connects to MySQL 8.4.9 through stock mysqli with and without
`MYSQLI_CLIENT_FOUND_ROWS` and verifies:

- a no-op UPDATE matching two rows reports affected rows `0` on a default
  connection;
- the same UPDATE reports affected rows `2` on a connection opened with
  `MYSQLI_CLIENT_FOUND_ROWS`;
- the behavior is the same for direct queries and native prepared statements;
- statement and connection affected-row properties agree after prepared
  execution; and
- SQL `ROW_COUNT()` follows the client policy and returns `2` after the no-op
  UPDATE when the client flag is enabled.

## Scope

The implementation must:

- accept `MYSQLI_CLIENT_FOUND_ROWS` as the only supported nonzero mysqli
  connection flag;
- retain the policy in MyLite's connection-local session state;
- report matched rows through mysqli `affected_rows` for direct and native
  prepared UPDATE execution when the flag is enabled;
- preserve changed-row reporting for connections without the flag;
- make SQL `ROW_COUNT()` report the same connection-policy value; and
- reject any unsupported client-flag bit, including combinations containing
  one supported and one unsupported bit.

## Public API

```c
int mylite_set_client_found_rows(mylite_db *database, int enabled);
```

The call updates connection-local policy for subsequent statements, does not
allocate, accepts zero as disabled and nonzero as enabled, and returns
`MYLITE_MISUSE` for a null database. Adapters set it immediately after opening
the handle. Existing statements observe the policy in effect when execution
completes.

layout of public opaque handles.
The addition is an ABI-compatible symbol addition. It does not alter the layout
of public opaque handles.
layout of public opaque handles.

## Runtime Semantics

UPDATE planning already counts rows admitted by the UPDATE predicate and
LIMIT before executing physical assignments. Successful UPDATE execution
stores either that count or the changed-row count as the result's affected-row
value according to the connection policy. Failed statements publish neither
value.

The core chooses the reported value only after successful UPDATE completion:

- default connection: changed rows;
- `MYSQLI_CLIENT_FOUND_ROWS` connection: predicate-and-LIMIT matched rows;
- all non-UPDATE statements: the existing affected-row value.

The selected affected-row value is exposed consistently by materialized
results, native statement completion, mysqli connection/statement properties,
and the next SQL `ROW_COUNT()`. UPDATE info text continues to distinguish
matched and changed rows. The policy does not change warnings, insert ID,
result rows, or persistent data.

## Diagnostics

`mysqli_real_connect()` accepts flag value `0` and
`MYSQLI_CLIENT_FOUND_ROWS`. Any other bit causes the existing embedded-driver
unsupported connection diagnostic `1235 / 42000` before opening a database.
No warning is emitted for the supported flag.

## Storage And Performance

Matched rows are derived from the UPDATE count query that MyLite already
performs. The feature adds one boolean to connection-local session state and no
new query, table, catalog field, file-format state, SQLite patch, or third-party
dependency.

## Non-Goals

- MySQL wire-protocol capability negotiation or OK packets;
- support for unrelated mysqli client flags;
- extending found-row semantics beyond the currently supported UPDATE
  execution paths;
- complete affected-row parity for every MySQL statement class;
- changing DELETE, INSERT, or REPLACE affected-row rules; or
- exposing a mutable matched-row policy in the core MyLite connection API.

## Tests

Native C tests cover materialized and cursor UPDATE affected rows plus SQL
`ROW_COUNT()` under both policies. PHP tests cover direct and prepared no-op
UPDATEs, connection and statement properties, SQL row-count state, default
changed-row behavior, the supported flag, and unsupported flag rejection. The
MySQL expectation script provides the paired MySQL 8.4.9 evidence described
above. MediaWiki installation and database PHPUnit tests exercise the
application call path that requires the flag.
