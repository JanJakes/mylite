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

For mysqli callers, pass the `.mylite` file as a socket path, as a path-like
host, or as `localhost:/path/to/file.mylite` for WordPress-style DB host
parsing. The replacement module must be loaded in a PHP process that has not
already loaded PHP's stock `mysqli` module.
