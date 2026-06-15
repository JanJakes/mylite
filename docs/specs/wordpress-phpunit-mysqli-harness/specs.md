# WordPress PHPUnit mysqli Harness

## Scope

This slice adds `tools/wordpress-phpunit-mysqli-mylite`, a Docker-based runner
for WordPress' upstream PHPUnit suite through MyLite's PHP mysqli replacement.
The runner fetches `WordPress/wordpress-develop`, builds this repository with
`MYLITE_BUILD_PHP_EXTENSIONS=ON`, loads `mylite.so` and the MyLite-backed
`mysqli.so`, writes a `wp-tests-config.php` that points `DB_HOST` at a single
`.mylite` file, and invokes PHPUnit with any command-line arguments passed to
the runner.

CI runs the full upstream WordPress PHPUnit suite at the pinned
`MYLITE_WORDPRESS_REF`. Focused local runs remain available by passing PHPUnit
arguments such as `--filter` to the same tool.

## Compatibility Contract

The harness is an application-level signal. It does not mark all WordPress or
mysqli behavior as supported. A passing CI run means WordPress can load the
MyLite mysqli replacement, create the configured test database, bootstrap the
PHPUnit environment, and pass the pinned full upstream WordPress PHPUnit suite
under the current runner configuration.

Third-party plugins, alternate WordPress refs, non-default PHP/runtime
configuration, and future upstream query changes remain outside this exact
baseline until those surfaces are independently specified and added to the core
compatibility matrix.

## Runtime Inputs

The runner is parameterized by environment variables:

- `MYLITE_WORDPRESS_REPO`: WordPress repository URL.
- `MYLITE_WORDPRESS_REF`: ref or commit to fetch.
- `MYLITE_WORDPRESS_DIR`: checkout directory.
- `MYLITE_WORDPRESS_PHPUNIT_TOOLS_DIR`: composer-installed PHPUnit tools.
- `MYLITE_WORDPRESS_CMAKE_BUILD_DIR`: MyLite PHP build directory.
- `MYLITE_WORDPRESS_DB_PATH`: `.mylite` database path.

## Exclusions

This slice does not vendor WordPress, WordPress' PHPUnit dependencies, or
composer packages. It also does not add MySQL assertion replay for WordPress
SQLite integration tests; that is a separate experiment surface.
