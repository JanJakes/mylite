# PHP Extensions

MyLite has three opt-in PHP extension packages:

| Package | Module | Purpose |
| --- | --- | --- |
| `packages/php-ext-mylite` | `mylite` | Core native MyLite API for PHP code. |
| `packages/php-ext-mysqli-mylite` | `mysqli` | Drop-in mysqli replacement for PHP processes launched without stock mysqli. |
| `packages/php-ext-pdo-mylite` | `pdo_mylite` | PDO driver registered as `mylite`. |

It also provides two installable Composer integrations on top of
`pdo_mylite`:

| Package | Composer name | Purpose |
| --- | --- | --- |
| `packages/php-laravel-mylite` | `mylite/laravel-driver` | Package-discovered Laravel 12 `mylite` database connection. |
| `packages/php-doctrine-mylite` | `mylite/doctrine-dbal-driver` | Doctrine DBAL 4.4 driver selected through `driverClass`. |

Build them with:

```sh
cmake --preset php-dev
cmake --build --preset php-dev
ctest --preset php-dev -R '^php-ext'
```

The normal `dev` preset does not require PHP headers. The `php-dev` preset
requires `php-config` and a PHP CLI executable.

## Source Layout

The `mysqli` replacement keeps its installed ABI unchanged but separates the
extension internals into private source modules:

- `mysqli_extension.c`: module globals, MINIT/MINFO, and the Zend module entry.
- `mysqli_registration.c`: arginfo, function tables, constants, and class
  registration.
- `mysqli_api.c`: procedural mysqli functions and object methods.
- `mysqli_support.c`: object lifecycle, connection execution, native statement
  binding, result buffering, path resolution, errors, and property updates.
- `mysqli_extension.h`: private shared declarations for those translation
  units; it is not an installed public header.

Run WordPress' upstream PHPUnit harness through the mysqli replacement with:

```sh
tools/wordpress-phpunit-mysqli-mylite --filter 'Tests_DB::test_check_connection_returns_true_when_there_is_a_connection'
```

Omit the filter for the full WordPress PHPUnit run.

Run Drupal core's database PHPUnit selection through the mysqli replacement
with:

```sh
tools/drupal-phpunit-mysqli-mylite
```

The default selection covers Drupal core's Database kernel tests, Drupal core's
Database unit tests, and Drupal's `mysqli` driver database tests. Pass PHPUnit
arguments to run a broader or different Drupal selection, for example:

```sh
tools/drupal-phpunit-mysqli-mylite test --testsuite=kernel
```

Run the other CI application baselines with:

```sh
tools/laravel-phpunit-pdo-mylite
tools/doctrine-phpunit-pdo-mylite
tools/mediawiki-phpunit-mysqli-mylite
```

Laravel and Doctrine install their respective local Composer packages into
fresh applications and run configured MyLite-owned suites through
`pdo_mylite`. MediaWiki runs the selected upstream database PHPUnit paths
through the mysqli replacement.

For mysqli callers, pass the `.mylite` file as a socket path, as a path-like
host, or as `localhost:/path/to/file.mylite` for WordPress-style DB host
parsing. The replacement module must be loaded in a PHP process that has not
already loaded PHP's stock `mysqli` module.

## Database path safety

The core `mylite` module, mysqli replacement, and PDO driver preserve PHP
string lengths while routing database filenames. Only the exact eight-byte
`:memory:` token selects an in-memory database. Empty core/PDO paths and any
selected path containing NUL fail before native open or filesystem access;
mysqli retains its documented empty-selection memory fallback.

mysqli applies the same check to `mylite:<path>`, path-like and
`localhost:<path>` hosts, socket paths, and path-like database arguments before
publishing a new link path. PDO applies it to plain and `path=` DSNs through
both construction and PHP 8.4 `PDO::connect()`. Existing visible prefix files
are not opened or changed by a rejected suffix.

See [length-aware database paths](../specs/length-aware-database-paths/specs.md)
for the native ownership contract, test matrix, and platform boundary.

The mysqli replacement accepts `MYSQLI_CLIENT_FOUND_ROWS` during
`mysqli_real_connect()`. Direct and native prepared UPDATEs then expose rows
matched by the predicate through the connection and statement
`affected_rows` properties. Connections without the flag expose changed rows,
and SQL `ROW_COUNT()` follows the same connection policy. Other nonzero client
flags remain unsupported.

mysqli and PDO statement execution resets the prior native execution while
retaining SQL and bound values. Unread or completed buffered rows and their
metadata are discarded. Re-executing the same statement object regenerates
`SHOW`, `DESCRIBE`, `EXPLAIN`, SELECT, and maintenance results from the current
schema, data, and execute-time session state.

## mysqli pending-result ownership

The mysqli replacement models the connection protocol state explicitly.
Row-producing `real_query()` calls leave an unclaimed result that must be
acquired with `store_result()` or `use_result()`. Stored results release the
connection immediately. Results acquired with `use_result()`, including
internally materialized utility results, retain the connection until a fetch
observes end-of-data or the result is freed, closed, or destroyed. Returning
the final row alone does not release it.

Row-producing prepared statements are unbuffered after `execute()`.
`store_result()` and `get_result()` buffer the rows and release the connection;
`free_result()`, `reset()`, `close()`, or same-statement re-execution discard
them and release it. `result_metadata()` returns a separate zero-row metadata
object and leaves the prepared result active. A different statement cannot be
prepared or executed while that owner remains active.

A query, transaction command, autocommit change, or other server operation
attempted while the connection is owned fails without changing the owner:

```text
error code: 2014
SQLSTATE: HY000
message: Commands out of sync; you can't run this command now
```

With strict mysqli reporting this is a `mysqli_sql_exception`; with reporting
disabled the operation returns `false` and publishes the same fields on the
connection or statement. Ordinary buffered `query()` results never own the
connection and may remain unread across later commands.

The same readiness check covers ping, stat, refresh, server debug-info, and
kill operations. Matching mysqli's reporting behavior, busy ping and kill
operations throw in strict mode, while busy stat, refresh, and debug-info
operations return `false` and publish the 2014 fields on the connection.

## Statement diagnostics and warning snapshots

PDO keeps database-handle and statement diagnostics separate. A failed
statement publishes its SQLSTATE, native error code, and copied message only
through that statement's `errorInfo()`. Later connection work or work on
another statement does not replace it. A successful statement execution clears
only that statement's record, while successful direct database work clears only
the database-handle record.

The mysqli replacement copies each completed operation's retained warning
records into adapter-owned state. Direct buffered results, direct unbuffered
results after end-of-data, and prepared statements therefore expose real FIFO
warning chains through both the object-oriented and procedural warning APIs.
The connection and prepared statement retain independent copies. The
connection's `warning_count` reports the total generated count even when
`max_error_count` caps the retained records available for iteration.

Each returned `mysqli_warning` object owns a separate copy of its chain.
Calling `next()` advances the same object's `errno`, `sqlstate`, and `message`
properties. Later queries, statement reset or re-execution, and statement or
connection close do not change or invalidate an already returned warning
object.

The native API provides equivalent statement diagnostic accessors and
caller-owned indexed warning copies. Direct result handles also retain warning
records independently from later connection activity. See
[statement diagnostics and warning snapshots](../specs/statement-diagnostics-warning-snapshots/specs.md)
for the exact ABI, ownership, and lifetime contract.

## Native scalar results and server identity

PDO MyLite and prepared mysqli results use native result metadata when creating
PHP values. Signed and unsigned integral values, including `BIT`, become PHP
integers when representable by `zend_long`; larger exact integers remain
decimal strings. FLOAT and DOUBLE become PHP floats. DECIMAL, text, binary,
temporal, JSON, geometry, and unknown values remain length-aware strings. SQL
NULL remains PHP `null`.

`PDO::ATTR_STRINGIFY_FETCHES` converts numeric PDO results to strings while
leaving SQL NULL unchanged. The setting is read at fetch time. Prepared mysqli
`get_result()`, `execute_query()`, and `bind_result()` use native conversion.
Direct buffered and unbuffered mysqli queries retain their existing
string/NULL result policy.

PDO reports the MyLite package version through `PDO::ATTR_CLIENT_VERSION` and
the MySQL compatibility identity through `PDO::ATTR_SERVER_VERSION`. The
server value is identical to `SELECT VERSION()`. The native
`mylite_server_version()` accessor exposes the same process-lifetime identity,
while `mylite_version()` continues to identify the client library.

See
[PHP native scalar conversion and server identity](../specs/php-native-scalar-conversion-server-identity/specs.md)
for the complete conversion matrix and qualification evidence.

## PDO metadata, buffered row counts, and connection identity

PDO MyLite publishes native result descriptors through `getColumnMeta()`.
Each column reports `native_type`, `pdo_type`, `flags`, `table`, `name`, `len`,
and `precision`. Direct table columns preserve metadata for empty results.
MySQL's PDO flag names `not_null`, `primary_key`, `unique_key`,
`multiple_key`, and `blob` are mapped from the native descriptor; unsigned and
auto-increment flags remain intentionally omitted because PDO MySQL does not
publish them through this API.

PDO statements are buffered. Direct and prepared SELECT `rowCount()` therefore
report the complete selected-row count immediately after execution and retain
it during and after fetching. Non-row statements continue to report affected
rows.

The mysqli replacement publishes each live native handle's stable connection
ID through both the `thread_id` property and `mysqli_thread_id()`. Initially
this value equals SQL `CONNECTION_ID()` and differs across simultaneous
connections. As in MySQL, `SET SESSION pseudo_thread_id` changes the SQL value
without changing the mysqli thread ID.

The native `mylite_stmt_buffered_row_count()` and `mylite_connection_id()`
accessors expose the same read-only values. See
[PHP PDO metadata and connection observables](../specs/php-pdo-metadata-connection-observables/specs.md)
for the exact descriptor, lifetime, and compatibility boundaries.
