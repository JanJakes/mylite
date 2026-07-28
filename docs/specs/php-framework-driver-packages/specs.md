# PHP Framework Driver Packages

## Status

Specified. Implementation and qualification are pending. This feature replaces
the test-local Laravel and Doctrine PDO bridges and closes review finding
API-07.

## Sources

- MyLite architecture and engineering policy:
  `README.md`,
  `docs/architecture/engineering-standards.md`
- Existing application baselines:
  `docs/specs/laravel-phpunit-pdo-harness/specs.md`,
  `docs/specs/doctrine-phpunit-pdo-harness/specs.md`
- PHP PDO adapter contract:
  `docs/specs/php-extension-packages/specs.md`,
  `docs/specs/php-pdo-metadata-connection-observables/specs.md`
- Laravel 12 package discovery and database configuration:
  https://laravel.com/docs/12.x/packages,
  https://laravel.com/docs/12.x/database
- Doctrine DBAL 4.4 custom-driver configuration:
  https://www.doctrine-project.org/projects/doctrine-dbal/en/4.4/reference/configuration.html

The package interfaces are independently authored from official framework
documentation and the public APIs of the pinned MIT-licensed Laravel and
Doctrine releases. No third-party implementation source is copied into MyLite.

## Problem

The existing application runners prove useful SQL compatibility, but both
bypass normal integration:

- the Laravel suite constructs `PDO` and `MySqlConnection` directly, installs
  grammars manually, and replaces Eloquent's resolver;
- the Doctrine DBAL and ORM suites declare separate custom driver classes
  inside generated test files.

Those test-only bridges cannot be installed by an application, do not exercise
Laravel package discovery or configured connection resolution, and duplicate
connection bootstrap logic. Passing them is therefore not evidence that a
fresh application can select MyLite through supported framework configuration.

## Package Layout

The repository adds two independent Composer packages:

| Package directory | Composer package | Runtime dependency |
| --- | --- | --- |
| `packages/php-laravel-mylite` | `mylite/laravel-driver` | Laravel/Illuminate 12 |
| `packages/php-doctrine-mylite` | `mylite/doctrine-dbal-driver` | Doctrine DBAL 4.4 |

Both packages require PHP 8.2 or newer, `ext-pdo`, `ext-mylite`, and
`ext-pdo_mylite`. They are thin PHP adapters and add no native or transitive
runtime library to MyLite itself. Their upstream framework dependencies are
MIT-licensed peer requirements rather than vendored code.

The packages have separate dependency surfaces so a Laravel application does
not install Doctrine and a Doctrine application does not install Laravel.

## Shared Configuration Model

Framework configuration separates the physical embedded file from the logical
MySQL database:

| Key | Meaning |
| --- | --- |
| `path` | Nonempty `.mylite` path or the exact `:memory:` token |
| `database` / `dbname` | Optional logical MySQL database selected after open |
| `options` / `driverOptions` | PDO attributes forwarded to `pdo_mylite` |

The connector constructs the length-aware PHP string DSN `mylite:<path>`.
Username and password values are accepted for conventional framework
configuration but have the PDO MyLite embedded-identity semantics. Host, port,
TLS, replicas, persistent connections, and network failover remain outside the
package contract.

When a logical database is nonempty, the connector quotes it as a MySQL
identifier, executes `CREATE DATABASE IF NOT EXISTS`, and then `USE`. An empty
logical database performs no automatic schema selection. The physical path is
never treated as a logical identifier.

## Laravel Driver

The Laravel package provides:

```php
MyLite\Laravel\MyLiteConnector
MyLite\Laravel\MyLiteServiceProvider
```

Composer's Laravel package-discovery metadata registers the service provider.
Before application providers boot, the provider extends Laravel's database
manager with the `mylite` driver name. The resolver:

1. passes the connection configuration to `MyLiteConnector`;
2. opens PDO MyLite and selects the configured logical database;
3. returns Laravel's standard `MySqlConnection` with the logical database,
   prefix, connection name, and complete configuration.

Using `MySqlConnection` deliberately retains Laravel's supported MySQL query
grammar, schema grammar, postprocessor, unique-constraint classification,
event wiring, reconnector, and transaction behavior. Applications configure
the package normally:

```php
'mylite' => [
    'driver' => 'mylite',
    'path' => database_path('application.mylite'),
    'database' => 'application',
    'prefix' => '',
    'options' => [],
],
```

Application code must not construct a PDO or connection resolver to use this
configuration.

## Doctrine DBAL Driver

The Doctrine package provides:

```php
MyLite\Doctrine\DBAL\Driver
```

The driver extends Doctrine's public MySQL driver abstraction, opens PDO MyLite
from `path`, performs optional logical-database bootstrap, and returns
Doctrine's standard PDO driver connection. PDO connection failures and
bootstrap failures are converted to Doctrine's PDO driver exception so DBAL's
MySQL exception converter can classify native MySQL-compatible codes.

Applications select the installed driver through DBAL's documented
`driverClass` parameter:

```php
$connection = Doctrine\DBAL\DriverManager::getConnection([
    'driverClass' => MyLite\Doctrine\DBAL\Driver::class,
    'path' => '/path/to/application.mylite',
    'dbname' => 'application',
]);
```

Doctrine obtains the MySQL 8.4 platform from PDO MyLite's server-version
attribute. The package does not add a private `DriverManager` mapping or patch
Doctrine.

## Validation And Errors

- Missing, non-string, empty, or embedded-NUL paths fail before PDO open.
- A non-string logical database fails before executing bootstrap SQL.
- `options` and `driverOptions` must be arrays.
- Identifier quoting doubles embedded backticks.
- PDO and driver exceptions retain SQLSTATE and native MySQL-compatible error
  numbers.
- Laravel duplicate-key failures become
  `Illuminate\Database\UniqueConstraintViolationException`.
- Doctrine duplicate-key failures become
  `Doctrine\DBAL\Exception\UniqueConstraintViolationException`.
- No adapter exception is swallowed or replaced with a success value.

SEC-03 owns the underlying length-aware native open API and adapter-wide path
matrix. These packages still validate their public configuration boundary so
an invalid framework value cannot silently select a different file.

## Fresh-Application Gates

The Laravel runner must install the local Composer package into a fresh pinned
Laravel 12 application and use only `config/database.php`, environment values,
package discovery, Artisan, facades, and Eloquent. It must cover:

- package discovery and resolution of a named `mylite` connection;
- a normal Artisan migration and migration repository;
- schema column and index metadata;
- query-builder insert, selection, ordering, and row counts;
- Eloquent persistence, scalar hydration, and relations;
- duplicate-key exception conversion;
- transaction commit and rollback.

The Doctrine runner must install the local Composer package into a fresh
Composer application with pinned DBAL 4.4 and ORM 3.6 dependencies. It must use
the package's public driver class through `DriverManager`, with no driver class
declared in a test file. It must cover:

- DBAL schema creation and introspection;
- prepared query-builder execution and scalar hydration;
- insert identity and affected rows;
- duplicate-key exception conversion;
- explicit transaction rollback;
- ORM `SchemaTool`, entity persistence, generated identifiers, DQL hydration,
  reload, and transaction rollback.

Both runners must fail if no tests execute, PHPUnit reports warnings or risky
tests, the installed class resolves outside its package source directory, or a
generated suite declares an inline connector/driver.

## MySQL Reference And Compatibility Boundary

This feature introduces framework packaging and no new SQL semantics. The
queries, metadata, scalar types, diagnostics, constraint errors, insert IDs,
and transactions used by the fresh-application gates remain covered by their
feature-level MySQL 8.4.9 fixtures. The application gates verify that supported
framework extension points transport those already qualified behaviors
without a test-only bridge.

Qualification is limited to the pinned Laravel, Doctrine DBAL, Doctrine ORM,
PHP, and test selections. It does not claim:

- the complete Laravel, Doctrine DBAL, or Doctrine ORM upstream suite;
- Symfony DoctrineBundle configuration;
- framework read/write splitting, replicas, persistent connections, or async
  execution;
- stock PDO MySQL, mysqlnd, or MySQL wire-protocol compatibility;
- third-party framework packages or application-specific SQL outside the
  MyLite compatibility matrix.

## Qualification

Before this row becomes supported:

- both package Composer manifests must validate;
- both fresh-application runners must pass in their pinned Release containers;
- the existing PHP adapter, Laravel, and Doctrine gates must remain green;
- source scans must prove that generated tests contain no local driver class;
- documentation and application claims must name the package and exact test
  boundary;
- the compatibility-claim validator must pass.
