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
argument. Other nonzero network ports, client flags, TLS settings, debug
controls, and connection or statement options return `false` with error `1235`
and SQLSTATE `42000`.

Transaction start, read-only/read-write, consistent-snapshot, chain, no-chain,
and no-release flags are routed through MyLite's transaction state machine.
Release and named transaction requests are rejected explicitly. Query and
store-result mode arguments are validated; asynchronous query mode is not
supported.
