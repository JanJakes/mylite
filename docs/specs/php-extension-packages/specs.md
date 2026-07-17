# PHP Extension Packages

## Scope

This slice adds opt-in PHP integration packages around the current `libmylite`
C API:

- `packages/php-ext-mylite`: a core `mylite` PHP module exposing
  `MyLite\Connection`, `MyLite\Statement`, `MyLite\Result`, `mylite_open()`,
  and `mylite_version()`.
- `packages/php-ext-mysqli-mylite`: a drop-in PHP module named `mysqli` that
  routes standard mysqli-style calls to embedded MyLite.
- `packages/php-ext-pdo-mylite`: a `pdo_mylite` module that registers the PDO
  driver name `mylite`.

The packages are disabled by default and build when
`MYLITE_BUILD_PHP_EXTENSIONS=ON` is set. PHP discovery goes through
`php-config`; the default CMake developer preset remains independent of PHP
headers.

## Semantics

The PHP modules call `mylite_open()`, `mylite_open_memory()`, `mylite_close()`,
and `mylite_execute()` directly. Query results are buffered into PHP objects so
the source `mylite_result` can be released at the extension boundary.
The core `mylite` module owns the linked `libmylite` runtime; the mysqli and
PDO modules require it at load time and use its exported public ABI instead of
embedding separate MyLite runtime copies.

The current `libmylite` public ABI does not expose prepared statements. PHP
statement objects therefore emulate positional parameter execution by rendering
complete SQL strings and passing those strings to `mylite_execute()`. This
keeps PHP callers usable without introducing a private prepared-statement ABI.
It is not a substitute for eventual native prepared statement support, binary
protocol metadata, server-side cursors, or streaming result sets.

The mysqli package intentionally provides global `mysqli`, `mysqli_result`,
`mysqli_stmt`, and related function symbols when PHP is launched without the
stock mysqli module. This is required for unmodified WordPress bootstrap code.
It accepts MyLite file paths through the `mylite:` host prefix, the socket
argument, path-like database arguments, path-like host arguments, and
`localhost:/path/to/file.mylite` socket-style host strings. `SET autocommit`
is handled by core MyLite session state; the mysqli bridge only exposes the
normal no-result statement surface to PHP callers.

The mysqli implementation is organized by extension responsibility rather than
by PHP symbol order. Module entry/globals, Zend registration metadata, API
entry points, and support/runtime helpers live in separate translation units
sharing one package-private header. The split does not create public ABI; it is
only a maintenance boundary for the replacement extension.

The PDO package uses PDO's emulated placeholder path and executes the expanded
SQL string against MyLite. It reports MyLite SQLSTATE, native error code, error
message, affected rows, and insert ids through PDO's normal surfaces where the
current `libmylite` result API exposes those values.

## Tests

CTest package tests cover:

- core module loading, file and memory opens, exec/query, buffered fetches,
  insert counts, error propagation, and emulated statement execution;
- mysqli object/procedural calls, transactional autocommit transitions,
  statement interpolation, insert ids, field metadata, and error reporting;
- PDO driver registration, exec/query, emulated prepared statements,
  transactions, quoting, and silent error metadata.

The tests are package-local PHP scripts registered in the PHP-enabled CMake
configurations, including the `php-ci` preset used by CI and the `php-dev`
preset used for local development.

## Exclusions

This slice does not implement:

- native `libmylite` prepared statements;
- MySQL binary protocol or mysqlnd integration;
- full stock mysqli/PDO API parity;
- streaming/unbuffered results;
- persistent PDO connections;
- PHP extension builds on Windows CI.
