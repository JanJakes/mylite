# PHP PDO Metadata And Connection Observables

## Status

Specified and covered by failing adapter regressions. Implementation and
qualification are pending. This feature closes the metadata, buffered row
count, and connection-identity portions of review finding API-06.

## Sources

- MyLite architecture and engineering policy:
  `README.md`,
  `docs/architecture/engineering-standards.md`
- Existing native result metadata:
  `docs/specs/baseline-result-column-metadata/specs.md`,
  `docs/compatibility/error-warning-result-semantics.md`
- Existing connection identity:
  `docs/specs/baseline-connection-id-function/specs.md`
- PHP PDO metadata and row-count contracts:
  https://www.php.net/manual/en/pdostatement.getcolumnmeta.php,
  https://www.php.net/manual/en/pdostatement.rowcount.php
- PHP mysqli connection identity:
  https://www.php.net/manual/en/mysqli.thread-id.php

This specification is independently authored from project documentation,
official PHP documentation, observed MySQL 8.4.9 behavior, and existing MyLite
source. It does not copy MySQL, PHP, mysqlnd, MariaDB, Percona, or other
implementation internals.

## MySQL 8.4.9 Observations

The expectation script
`packages/libmylite/tests/mysql_php_pdo_metadata_connection_observables_expectations.sh`
records the runtime behavior of PHP 8.4 PDO MySQL and mysqli against MySQL
8.4.9.

For a buffered PDO MySQL SELECT:

- `rowCount()` reports the complete selected-row count immediately after
  execution and retains it after fetch;
- an empty SELECT reports zero rows while retaining its column metadata;
- `getColumnMeta()` returns `native_type`, `pdo_type`, `flags`, `table`,
  `name`, `len`, and `precision`;
- direct table columns retain metadata even when the result is empty;
- expressions and aggregates have an empty table name.

The observed PDO MySQL flag vocabulary is `not_null`, `primary_key`,
`unique_key`, `multiple_key`, and `blob`. Although native protocol flags also
carry unsigned and auto-increment information, PHP 8.4 PDO MySQL does not
publish those two flags through `getColumnMeta()`.

Each live mysqli connection has a stable, nonzero thread ID. The object
property and procedural accessor equal that connection's SQL
`CONNECTION_ID()`. Simultaneously live connections have different IDs.
`SET SESSION pseudo_thread_id` changes SQL `CONNECTION_ID()` but does not
change the mysqli thread ID.

## Scope

The implementation must:

- populate PDO's per-column describe storage from native statement metadata;
- implement PDO `getColumnMeta()` using the same native descriptors;
- preserve metadata for empty buffered result sets;
- return the total buffered SELECT row count immediately after execute;
- add public native accessors for the buffered statement row count and stable
  connection ID;
- use the native connection ID for both the mysqli `thread_id` property and
  `mysqli_thread_id()`;
- keep identifiers stable for the lifetime of a database handle and distinct
  for simultaneously live handles;
- preserve that stable identity when `pseudo_thread_id` changes the SQL-visible
  session value.

## Non-Goals

This feature does not:

- change native result descriptors whose expression or function metadata is
  already known to differ from MySQL; SEM-03 owns those descriptor corrections;
- add unbuffered PDO queries or define a nonzero SELECT row count for an
  unbuffered driver mode;
- expose unsigned or auto-increment flags that PHP 8.4 PDO MySQL itself omits;
- add server threads, process scheduling, a wire-protocol thread lifecycle, or
  remote kill semantics;
- make a database handle safe for concurrent use.

## Public Native ABI

The installed header adds:

```c
uint64_t mylite_connection_id(const mylite_db *database);
size_t mylite_stmt_buffered_row_count(const mylite_stmt *stmt);
```

`mylite_connection_id()` returns the nonzero ID assigned when the handle is
created. It remains unchanged until the handle is closed. It returns zero for
a null handle. The value owns no storage and calling the accessor changes no
diagnostics or session state.

`mylite_stmt_buffered_row_count()` returns the total number of rows owned by
the current buffered result. It is valid after execution has started and
before reset or finalize. It returns zero for a null statement, a non-row
statement, a streaming statement, an unexecuted statement, or an empty
buffered result. The accessor does not advance the cursor or alter diagnostics.

## PDO Describe Contract

After successful execute, each PDO column description uses:

| PDO field | Native source |
| --- | --- |
| `name` | `mylite_stmt_column_name()` |
| `len` | `mylite_stmt_column_display_length()` |
| `precision` | `mylite_stmt_column_decimals()` |
| `native_type` | stable uppercase name for `mylite_result_column_type` |
| `pdo_type` | `PDO::PARAM_INT` for integral, YEAR, and BIT descriptors; otherwise `PDO::PARAM_STR` |
| `flags` | mapped `not_null`, `primary_key`, `unique_key`, `multiple_key`, and `blob` flags |
| `table` | `mylite_stmt_column_table_name()`, or an empty string |

The stable native type names are `DECIMAL`, `TINY`, `SHORT`, `LONG`, `FLOAT`,
`DOUBLE`, `NULL`, `TIMESTAMP`, `LONGLONG`, `INT24`, `DATE`, `TIME`,
`DATETIME`, `YEAR`, `VARCHAR`, `BIT`, `JSON`, `NEWDECIMAL`, `BLOB`,
`VAR_STRING`, `STRING`, `GEOMETRY`, and `UNKNOWN`.

String values copied into metadata arrays are owned by the PHP array. No PDO
metadata value borrows storage beyond the call.

## Buffered Row Count

PDO MyLite already prepares statements with `mylite_prepare_buffered()`.
Successful row-producing execution therefore owns the complete materialized
row set before returning to PDO. `stmt->row_count` is set from
`mylite_stmt_buffered_row_count()` for row-producing statements and from
`mylite_stmt_affected_rows()` for non-row statements.

Fetching, exhaustion, and `getColumnMeta()` do not change the buffered SELECT
count. Reset and re-execution replace it with the new result count.

## mysqli Connection Identity

When a mysqli link is opened, the adapter reads `mylite_connection_id()` once
for publication through the public `thread_id` property. The procedural
`mysqli_thread_id()` accessor reads the same live native handle value.

The value must equal:

```sql
SELECT CONNECTION_ID();
```

for that link. A second simultaneously live link must not reuse the first
link's ID. This equality describes the initially assigned connection identity:
like MySQL, a later `SET SESSION pseudo_thread_id` changes SQL
`CONNECTION_ID()` without changing the mysqli thread ID.

## Error And Lifetime Semantics

- Invalid PDO column indexes continue to use PDO core's existing validation.
- Metadata allocation failure follows ordinary Zend allocation failure
  handling and cannot leave borrowed pointers in the returned array.
- Native accessors are read-only and do not replace connection or statement
  diagnostics.
- Reset invalidates the prior buffered row count and column descriptors.
- Closing a native handle ends the connection-ID lifetime.

## Compatibility Boundary

Direct table-column descriptors in the test matrix match the observed MySQL
8.4.9 PDO metadata. Expression and aggregate columns are still exposed
faithfully from MyLite's native descriptors, including current differences in
type and display length. Those two tested expressions currently publish
`UNKNOWN`, zero display length, and zero precision. SEM-03 will correct those
native descriptors once and thereby update native, mysqli, and PDO consumers
together.

## Test Matrix

| Layer | Required coverage |
| --- | --- |
| MySQL 8.4.9 fixture | direct and prepared buffered row counts; post-fetch stability; empty result metadata; integer/unsigned, decimal, VARCHAR, TEXT, BLOB, DATETIME, geometry, expression, and aggregate metadata; two mysqli IDs; `pseudo_thread_id` separation |
| Native C API | null/accessor behavior; nonzero stable ID; two-handle distinction; initial SQL equality; stability across `pseudo_thread_id`; buffered nonempty/empty count; fetch stability; reset and re-execution |
| PDO MyLite | exact table metadata arrays; empty results; expression/aggregate descriptor transport; immediate and post-fetch `rowCount()`; DML affected count |
| mysqli MyLite | object/procedural identity equality; SQL equality; multiple handles; stability across commands |
| Qualification | Release/Debug, ASan/UBSan, ABI manifests, formatters, static analysis, PHP adapters, MySQL fixture, artifact-size gates |
