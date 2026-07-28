# Laravel PHPUnit PDO Harness

## Scope

This slice adds `tools/laravel-phpunit-pdo-mylite`, a Docker-based runner for a
MyLite-owned Laravel database integration suite. The runner fetches a fresh
pinned Laravel application skeleton, builds this repository with
`MYLITE_BUILD_PHP_EXTENSIONS=ON`, installs `mylite/laravel-driver` through
Composer, loads `mylite.so` and `pdo_mylite.so`, and executes migrations and a
generated PHPUnit suite through the application's normal configuration.

CI runs the generated suite against Laravel application commit
`945f4e5a9fd3695dc0ee512f497c650fb82cfbb8` and Laravel Framework `v12.62.0`
with a production MyLite PHP-extension build and one `.mylite` database file.

## Compatibility Contract

The harness is an application-level PDO signal. A passing CI run means the
Laravel framework can discover the installed driver package, resolve a
configured `mylite` connection through the database manager, use its standard
MySQL connection grammar against MyLite, and pass coverage for:

- stock and MyLite-owned Artisan migrations plus migration-repository setup;
- schema builder creation and column/index metadata inspection;
- query builder inserts, `insertGetId()`, filtering, ordering, scalar types,
  and counts;
- duplicate-key conversion to Laravel's unique-constraint exception;
- transaction rollback through Laravel's transaction helper;
- Eloquent creation, hydration, eager loading, and one-to-many relations.

The generated suite must not construct PDO, `MySqlConnection`, or an Eloquent
connection resolver. A reflection assertion also proves that the discovered
service provider resolves from the installed local package.

This is not a full Laravel upstream test-suite claim. Broader Laravel
compatibility remains feature-by-feature work in the core MySQL compatibility
matrix.

## Runtime Inputs

The runner is parameterized by environment variables:

- `MYLITE_LARAVEL_REPO`: Laravel application-skeleton repository URL.
- `MYLITE_LARAVEL_REF`: ref or commit to fetch.
- `MYLITE_LARAVEL_FRAMEWORK_VERSION`: exact framework version to install.
- `MYLITE_LARAVEL_DIR`: checkout directory.
- `MYLITE_LARAVEL_SUITE_DIR`: generated PHPUnit suite directory.
- `MYLITE_LARAVEL_CMAKE_BUILD_DIR`: MyLite PHP build directory.
- `MYLITE_LARAVEL_CMAKE_BUILD_TYPE`: CMake build type.
- `MYLITE_LARAVEL_DB_PATH`: `.mylite` database path.

## Exclusions

This slice does not vendor Laravel or Composer dependencies and does not
replace Laravel's full upstream or MySQL test matrices. Read/write splitting,
replicas, persistent connections, network configuration, mysqlnd, stock PDO
MySQL parity, and unselected application paths remain outside this baseline.
