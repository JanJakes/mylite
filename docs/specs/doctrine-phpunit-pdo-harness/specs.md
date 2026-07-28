# Doctrine PHPUnit PDO Harness

## Scope

This slice adds `tools/doctrine-phpunit-pdo-mylite`, a Docker-based runner for
MyLite-owned Doctrine DBAL and ORM application suites. The runner creates a
fresh Composer project, installs exact Doctrine DBAL and ORM versions plus
`mylite/doctrine-dbal-driver`, builds this repository with
`MYLITE_BUILD_PHP_EXTENSIONS=ON`, loads `mylite.so` and `pdo_mylite.so`, and
executes generated PHPUnit suites from the application dependency tree.

CI runs the generated suites against Doctrine DBAL `4.4.3` and Doctrine ORM
`3.6.7` with a production MyLite PHP-extension build and separate `.mylite`
database files. Those releases resolve to upstream commits
`61e730f1658814821a85f2402c945f3883407dec` and
`bc217c0e19c3a9eadfa67697143b87c9ba01272c`, respectively.

## Compatibility Contract

The harness is an application-level PDO signal. A passing CI run means:

- Doctrine DBAL can connect through a MyLite-backed PDO driver derived from
  DBAL's MySQL driver abstraction and selected through the documented
  `driverClass` configuration;
- DBAL selects its MySQL 8.4 platform and passes table creation, inserts,
  `lastInsertId()`, query-builder parameters, scalar hydration, ordering,
  schema-manager column/index introspection, duplicate-key exception
  conversion, and explicit transaction rollback;
- Doctrine ORM can use a DBAL connection backed by `pdo_mylite` and pass
  coverage for `SchemaTool`, entity persist/flush, generated identifiers, DQL
  hydration, transaction rollback, and entity reload.

The generated suites import only `MyLite\Doctrine\DBAL\Driver`; they must not
declare an inline DBAL driver or construct PDO directly. A reflection assertion
also proves that the driver resolves from the installed local package.

This is not a full Doctrine upstream test-suite claim. Broader DBAL and ORM
compatibility remains feature-by-feature work in the core MySQL compatibility
matrix.

## Runtime Inputs

The runner is parameterized by environment variables:

- `MYLITE_DOCTRINE_DBAL_VERSION`: exact DBAL version to install.
- `MYLITE_DOCTRINE_ORM_VERSION`: exact ORM version to install.
- `MYLITE_DOCTRINE_APP_DIR`: fresh Composer application directory.
- `MYLITE_DOCTRINE_SUITE_DIR`: generated PHPUnit suite directory.
- `MYLITE_DOCTRINE_CMAKE_BUILD_DIR`: MyLite PHP build directory.
- `MYLITE_DOCTRINE_CMAKE_BUILD_TYPE`: CMake build type.
- `MYLITE_DOCTRINE_DBAL_DB_PATH`: DBAL `.mylite` database path.
- `MYLITE_DOCTRINE_ORM_DB_PATH`: ORM `.mylite` database path.

## Exclusions

This slice does not vendor Doctrine or Composer dependencies and does not
replace Doctrine's full DBAL/ORM test matrices. DoctrineBundle, persistent
connections, replicas, asynchronous execution, mysqlnd, stock PDO MySQL parity,
and unselected application paths remain outside this baseline.
