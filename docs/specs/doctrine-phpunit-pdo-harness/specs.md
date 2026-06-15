# Doctrine PHPUnit PDO Harness

## Scope

This slice adds `tools/doctrine-phpunit-pdo-mylite`, a Docker-based runner for
MyLite-owned Doctrine DBAL and ORM database bridge suites. The runner fetches
Doctrine DBAL and Doctrine ORM, builds this repository with
`MYLITE_BUILD_PHP_EXTENSIONS=ON`, loads `mylite.so` and `pdo_mylite.so`, writes
a PHP wrapper with those extensions enabled, generates small PHPUnit suites,
and executes them through the respective Doctrine PHPUnit installations.

CI runs the generated suites against Doctrine DBAL `4.4.3` and Doctrine ORM
`3.6.7` with a production MyLite PHP-extension build and separate `.mylite`
database files.

## Compatibility Contract

The harness is an application-level PDO signal. A passing CI run means:

- Doctrine DBAL can connect through a MyLite-backed PDO driver derived from
  DBAL's MySQL driver abstraction;
- DBAL bridge coverage passes for table creation, inserts, `lastInsertId()`,
  query builder parameters, ordering, schema-manager column/index
  introspection, explicit transaction rollback, and table drops;
- Doctrine ORM can use a DBAL connection backed by `pdo_mylite` and pass bridge
  coverage for schema creation, entity persist/flush, DQL selection,
  transaction rollback, and entity reload.

This is not a full Doctrine upstream test-suite claim. Broader DBAL and ORM
compatibility remains feature-by-feature work in the core MySQL compatibility
matrix.

## Runtime Inputs

The runner is parameterized by environment variables:

- `MYLITE_DOCTRINE_DBAL_REPO`: Doctrine DBAL repository URL.
- `MYLITE_DOCTRINE_DBAL_REF`: DBAL ref or commit to fetch.
- `MYLITE_DOCTRINE_ORM_REPO`: Doctrine ORM repository URL.
- `MYLITE_DOCTRINE_ORM_REF`: ORM ref or commit to fetch.
- `MYLITE_DOCTRINE_DBAL_DIR`: DBAL checkout directory.
- `MYLITE_DOCTRINE_ORM_DIR`: ORM checkout directory.
- `MYLITE_DOCTRINE_SUITE_DIR`: generated PHPUnit suite directory.
- `MYLITE_DOCTRINE_CMAKE_BUILD_DIR`: MyLite PHP build directory.
- `MYLITE_DOCTRINE_CMAKE_BUILD_TYPE`: CMake build type.
- `MYLITE_DOCTRINE_DBAL_DB_PATH`: DBAL `.mylite` database path.
- `MYLITE_DOCTRINE_ORM_DB_PATH`: ORM `.mylite` database path.

## Exclusions

This slice does not vendor Doctrine or Composer dependencies, does not replace
Doctrine's full DBAL/ORM test matrices, and does not implement native
libmylite prepared statements, mysqlnd, stock PDO MySQL parity, persistent PDO
connections, or streaming result sets.
