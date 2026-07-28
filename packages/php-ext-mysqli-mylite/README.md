# php-ext-mysqli-mylite

`php-ext-mysqli-mylite` builds a PHP module named `mysqli` that routes standard PHP
mysqli APIs to embedded MyLite.

Build it with the monorepo CMake workflow:

```sh
cmake --preset php-dev
cmake --build --preset php-dev --target mylite_php_mysqli_extension
```

Run it without PHP's stock mysqli module:

```sh
php -n \
  -d extension=build/php-dev/packages/php-ext-mylite/mylite.so \
  -d extension=build/php-dev/packages/php-ext-mysqli-mylite/mysqli.so \
  script.php
```

## Embedded contracts

MyLite has no network or authentication layer. Username and password arguments
therefore select the documented fixed embedded identity rather than an account,
while the conventional MySQL port `3306` is accepted as an ignored compatibility
argument. `MYSQLI_CLIENT_FOUND_ROWS` is supported and makes UPDATE affected-row
counts and the following SQL `ROW_COUNT()` report matched rows for direct and
prepared execution. Other nonzero network ports, client flags, TLS settings,
debug controls, and connection or statement options return `false` with error
`1235` and SQLSTATE `42000`.

Transaction start, read-only/read-write, consistent-snapshot, chain, no-chain,
and no-release flags are routed through MyLite's transaction state machine.
Release and named transaction requests are rejected explicitly. Query and
store-result mode arguments are validated; asynchronous query mode is not
supported.

A row-producing `real_query()` owns the connection until `store_result()` or
`use_result()` acquires its result. A direct `MYSQLI_USE_RESULT` result
continues to own the connection until a fetch observes end-of-data or the
result is freed, closed, or destroyed. Row-producing prepared execution is
unbuffered by default and similarly owns the connection until it is exhausted,
buffered, freed, reset, re-executed on the same statement, or closed.
Disallowed commands fail with error `2014`, SQLSTATE `HY000`, and MySQL's
commands-out-of-sync message without consuming the active result. Prepared
`result_metadata()` returns field metadata without releasing that ownership.
The busy check also precedes ping, stat, refresh, server debug-info, and kill
operations.
