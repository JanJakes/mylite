# Drupal PHPUnit mysqli Harness

## Scope

This slice adds `tools/drupal-phpunit-mysqli-mylite`, a Docker-based runner for
Drupal core's PHPUnit suite through MyLite's PHP mysqli replacement. The runner
fetches Drupal core, builds this repository with `MYLITE_BUILD_PHP_EXTENSIONS`
enabled, loads `mylite.so` and the MyLite-backed `mysqli.so`, writes Drupal's
`core/phpunit.xml` with a `mysqli://...` `SIMPLETEST_DB` URL, creates the
configured test schema inside a single `.mylite` file, and invokes PHPUnit.

CI runs a pinned Drupal 11.3.11 compatibility selection against Drupal's core
database kernel tests. Broader Drupal PHPUnit execution remains available
locally by passing arguments to the same tool.

## Compatibility Contract

The harness is an application-level signal. It does not mark all Drupal,
mysqli, or MySQL behavior as supported. A passing selected run means Drupal can
load the MyLite mysqli replacement, resolve Drupal's own `mysqli` database
driver, connect through a MyLite-backed database URL, bootstrap the PHPUnit
environment, and exercise the selected database kernel test path.

The CI job must fail when PHPUnit fails. The runner preserves PHPUnit's process
status through its logging pipeline and rejects non-passing PHPUnit summary
lines even if the PHPUnit process exits successfully.

The full Drupal suite is expected to expose unimplemented MySQL, mysqli, and
Drupal database surfaces until those features are independently specified and
added to the core compatibility matrix.

## Runtime Inputs

The runner is parameterized by environment variables:

- `MYLITE_DRUPAL_REPO`: Drupal repository URL.
- `MYLITE_DRUPAL_REF`: ref or commit to fetch.
- `MYLITE_DRUPAL_DIR`: checkout directory.
- `MYLITE_DRUPAL_CMAKE_BUILD_DIR`: MyLite PHP build directory.
- `MYLITE_DRUPAL_DB_FILE_NAME`: `.mylite` database file name created under
  Drupal's `core/` directory.
- `MYLITE_DRUPAL_DB_NAME`: schema name inside the `.mylite` file.
- `MYLITE_DRUPAL_DB_PREFIX`: PHPUnit table prefix.

## Exclusions

This slice does not vendor Drupal or composer packages. It also does not add a
PDO MySQL replacement; Drupal is intentionally exercised through its core
`mysqli` database driver for this initial CI harness.
