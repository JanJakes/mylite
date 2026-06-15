# MediaWiki PHPUnit mysqli Harness

## Scope

This slice adds `tools/mediawiki-phpunit-mysqli-mylite`, a Docker-based runner
for MediaWiki core database PHPUnit coverage through MyLite's PHP mysqli
replacement. The runner fetches MediaWiki core, builds this repository with
`MYLITE_BUILD_PHP_EXTENSIONS=ON`, loads `mylite.so` and the MyLite-backed
`mysqli.so`, installs MediaWiki dependencies, creates a MyLite database, runs
MediaWiki's installer with `--dbtype mysql`, and invokes PHPUnit.

CI runs the default database selection against MediaWiki `REL1_42` with a
production MyLite PHP-extension build:

- `tests/phpunit/includes/db`
- `tests/phpunit/structure/DatabaseIntegrationTest.php`

## Compatibility Contract

The harness is an application-level mysqli signal. A passing CI run means
MediaWiki can load the MyLite mysqli replacement, install its MySQL schema into
a single `.mylite` database file, bootstrap the test environment, and pass the
selected MediaWiki database PHPUnit paths.

This is not a full MediaWiki upstream test-suite claim. Broader MediaWiki
compatibility remains feature-by-feature work in the core MySQL compatibility
matrix.

The CI job must fail when PHPUnit fails. The runner preserves PHPUnit's process
status through its logging pipeline and rejects non-passing PHPUnit summary
lines even if the PHPUnit process exits successfully.

## Runtime Inputs

The runner is parameterized by environment variables:

- `MYLITE_MEDIAWIKI_REPO`: MediaWiki repository URL.
- `MYLITE_MEDIAWIKI_REF`: ref or commit to fetch.
- `MYLITE_MEDIAWIKI_DIR`: checkout directory.
- `MYLITE_MEDIAWIKI_CMAKE_BUILD_DIR`: MyLite PHP build directory.
- `MYLITE_MEDIAWIKI_CMAKE_BUILD_TYPE`: CMake build type.
- `MYLITE_MEDIAWIKI_DB_PATH`: `.mylite` database path.
- `MYLITE_MEDIAWIKI_DB_NAME`: schema name inside the `.mylite` file.
- `MYLITE_MEDIAWIKI_DB_PREFIX`: PHPUnit table prefix.

## Exclusions

This slice does not vendor MediaWiki or Composer dependencies, does not claim
functional browser tests or the full MediaWiki PHPUnit suite, and does not add
stock mysqli/mysqlnd parity, binary protocol support, or streaming result sets.
