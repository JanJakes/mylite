# PHP Extensions

MyLite has three opt-in PHP extension packages:

| Package | Module | Purpose |
| --- | --- | --- |
| `packages/php-ext-mylite` | `mylite` | Core native MyLite API for PHP code. |
| `packages/php-ext-mysqli-mylite` | `mysqli` | Drop-in mysqli replacement for PHP processes launched without stock mysqli. |
| `packages/php-ext-pdo-mylite` | `pdo_mylite` | PDO driver registered as `mylite`. |

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

Laravel and Doctrine run MyLite-owned bridge suites against upstream framework
dependencies through `pdo_mylite`. MediaWiki runs the selected upstream
database PHPUnit paths through the mysqli replacement.

For mysqli callers, pass the `.mylite` file as a socket path, as a path-like
host, or as `localhost:/path/to/file.mylite` for WordPress-style DB host
parsing. The replacement module must be loaded in a PHP process that has not
already loaded PHP's stock `mysqli` module.

The mysqli replacement accepts `MYSQLI_CLIENT_FOUND_ROWS` during
`mysqli_real_connect()`. Direct and native prepared UPDATEs then expose rows
matched by the predicate through the connection and statement
`affected_rows` properties. Connections without the flag expose changed rows,
and SQL `ROW_COUNT()` follows the same connection policy. Other nonzero client
flags remain unsupported.
