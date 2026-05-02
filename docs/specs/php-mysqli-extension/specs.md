# PHP mysqli extension

## Scope

This package provides a PHP extension that exposes the standard `mysqli`
procedural functions and object classes while executing through embedded
MyLite. It is the first PHP integration package and intentionally focuses on
`mysqli`; PDO and mysqlnd wire-protocol adapters remain separate work.

The package builds:

- a dynamic PHP module named `mysqli`, intended to be loaded instead of PHP's
  stock `mysqli` module
- a package-local `mylite` executable linked to the same bundled MyLite runtime

The PHP module links MyLite directly into the extension binary, so applications
do not need a MySQL server or a separate shared `libmylite` installation.

## API Shape

The extension registers these standard PHP classes:

- `mysqli`
- `mysqli_result`
- `mysqli_stmt`
- `mysqli_driver`
- `mysqli_warning`
- `mysqli_sql_exception`

It also registers the standard `mysqli_*` procedural entry points used by PHP
applications. Functions whose server-side equivalent is not meaningful in an
embedded process, such as TLS setup, async polling, or process killing, return a
deterministic embedded-compatible placeholder result while preserving the PHP
API entry point.

The extension is loaded as `mysqli.so`. It must not be loaded together with
PHP's stock `mysqli` module because both provide the same symbols, function
names, and classes.

## Connection Mapping

`mysqli` normally receives a network host, credentials, and a schema name.
MyLite has a single local database file and no server authentication. The
extension maps connection arguments as follows:

| PHP argument shape | MyLite behavior |
| --- | --- |
| `hostname` starts with `mylite:` | Open the text after `mylite:` as the MyLite file path. `mylite::memory:` opens an in-memory database. |
| `socket` is non-empty | Treat `socket` as the MyLite file path and use `database` as the default schema name. |
| `database` is `:memory:`, contains a path separator, or ends with `.mylite` | Treat `database` as the MyLite file path. |
| no local path is provided | Open an in-memory MyLite database. |

When `database` is a schema name rather than a file path, the extension issues
`USE database` after opening the file. The `USE` statement follows MySQL
semantics and fails if the schema is missing.

User name, password, port, and network host are accepted for PHP API
compatibility but do not authenticate or create network connections.

## Query Execution

`mysqli_query()`, `mysqli::query()`, `mysqli_real_query()`,
`mysqli::real_query()`, `mysqli_execute_query()`, and `mysqli::execute_query()`
prepare SQL through `mylite_prepare()`, step the statement through
`mylite_step()`, and buffer complete result sets in `mysqli_result` objects.

For result-producing statements:

- `mysqli_result` buffers all rows.
- Fetch modes `MYSQLI_ASSOC`, `MYSQLI_NUM`, and `MYSQLI_BOTH` are supported.
- Result values are exposed as PHP strings or `null`, matching default
  mysqlnd text-result behavior.
- Field metadata is populated from MyLite's result descriptor API: label,
  schema, table, origin names, type, flags, declared length, decimals, charset,
  and nullability.

For non-result statements:

- the return value is `true`
- `affected_rows`, `insert_id`, `field_count`, and `warning_count` are updated
  from the MyLite public C API

For errors:

- parse errors map to `1064` / `42000`
- unsupported SQL maps to `1235` / `42000`
- execution errors map to `1105` / `HY000`
- allocation and SQLite open errors map to generic client errors

`mysqli_report()` controls whether errors are returned normally, emitted as PHP
warnings, or thrown as `mysqli_sql_exception`. The default report mode matches
modern PHP: `MYSQLI_REPORT_ERROR | MYSQLI_REPORT_STRICT`.

## Prepared Statements

`mysqli_stmt` is implemented as a PHP-side prepared statement wrapper over the
current MyLite text SQL API. MyLite does not yet expose a binary prepared
statement ABI, so the extension stores the SQL text and interpolates bound
values immediately before execution.

Supported behavior:

- `mysqli_prepare()` and `mysqli_stmt_prepare()`
- positional `?` markers outside SQL quoted strings and comments
- `mysqli_stmt_bind_param()` with stored PHP references
- `mysqli_stmt_execute()` with either bound parameters or the PHP 8.1+
  `$params` array
- `mysqli_stmt_bind_result()` and `mysqli_stmt_fetch()`
- `mysqli_stmt_get_result()` for buffered results
- statement error, SQLSTATE, field-count, affected-row, insert-id, and row-count
  accessors

Parameter interpolation is deliberately conservative. It quotes strings with
MySQL-style backslash escaping, renders `null` as `NULL`, booleans as `0`/`1`,
and numeric PHP values as decimal text. This is an integration bridge until
MyLite grows a native bind-parameter ABI.

## Embedded-Compatible Placeholders

These APIs have no direct embedded equivalent yet and return stable placeholder
values:

- TLS and compression options are accepted and ignored.
- async polling/reaping reports no pending async results.
- thread/process APIs report the current embedded connection as thread id `1`
  and do not kill anything.
- `multi_query()` executes a single SQL string through the same path as
  `real_query()`; result chaining reports no additional results.
- client/server statistics return empty arrays or concise MyLite status text.

These placeholders should become stricter only when the corresponding protocol
or diagnostics surface is implemented.

## Tests

Package tests load only this extension with `php -n -d extension=<mysqli.so>`
to avoid conflict with PHP's stock `mysqli` module. They cover:

- object-oriented and procedural connection/query/fetch flows
- result metadata and fetch modes
- statement bind/execute/fetch/get-result flows
- report-mode error handling
- the bundled `mylite` executable

The SQL expectations are inherited from the already MySQL-runtime-verified
MyLite feature specs for schema lifecycle, table creation, inserts, selects,
metadata, transactions, warnings, and expressions. PHP-specific API behavior is
verified against PHP 8.5 reflection from the local development runtime.

## Known Gaps

- This is not a mysqlnd plugin and does not expose MySQL wire protocol packets.
- Prepared statements are text-interpolated until MyLite exposes a native bind
  API.
- `MYSQLI_OPT_INT_AND_FLOAT_NATIVE` is accepted but not yet modeled per
  connection; result values currently follow mysqlnd's default string behavior.
- `mysqli_result` is buffered; unbuffered streaming is deferred until the public
  MyLite statement lifetime and PHP result object ownership model are expanded.
- Exact MySQL client info strings, connection stats, and async behavior are
  placeholders.
