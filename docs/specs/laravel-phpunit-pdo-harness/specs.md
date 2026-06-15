# Laravel PHPUnit PDO Harness

## Scope

This slice adds `tools/laravel-phpunit-pdo-mylite`, a Docker-based runner for a
MyLite-owned Laravel database integration suite. The runner fetches Laravel
Framework, builds this repository with `MYLITE_BUILD_PHP_EXTENSIONS=ON`, loads
`mylite.so` and `pdo_mylite.so`, writes a PHP wrapper with those extensions
enabled, generates a small PHPUnit suite, and executes it through Laravel's
installed PHPUnit runner.

CI runs the generated suite against Laravel `v12.62.0` with a production
MyLite PHP-extension build and one `.mylite` database file.

## Compatibility Contract

The harness is an application-level PDO signal. A passing CI run means the
Laravel framework can load through Composer, connect through `pdo_mylite`, use
the MySQL connection grammar against MyLite, and pass the generated database
bridge coverage for:

- schema builder creation of auto-increment, string, unique, integer,
  timestamp, and index metadata;
- query builder inserts, `insertGetId()`, filtering, ordering, and counts;
- transaction rollback through Laravel's transaction helper;
- Eloquent creation, eager loading, and one-to-many relations.

This is not a full Laravel upstream test-suite claim. Broader Laravel
compatibility remains feature-by-feature work in the core MySQL compatibility
matrix.

## Runtime Inputs

The runner is parameterized by environment variables:

- `MYLITE_LARAVEL_REPO`: Laravel Framework repository URL.
- `MYLITE_LARAVEL_REF`: ref or commit to fetch.
- `MYLITE_LARAVEL_DIR`: checkout directory.
- `MYLITE_LARAVEL_SUITE_DIR`: generated PHPUnit suite directory.
- `MYLITE_LARAVEL_CMAKE_BUILD_DIR`: MyLite PHP build directory.
- `MYLITE_LARAVEL_CMAKE_BUILD_TYPE`: CMake build type.
- `MYLITE_LARAVEL_DB_PATH`: `.mylite` database path.

## Exclusions

This slice does not vendor Laravel or Composer dependencies, does not replace
Laravel's full MySQL test matrix, and does not implement native libmylite
prepared statements, mysqlnd, stock PDO MySQL parity, or streaming result sets.
