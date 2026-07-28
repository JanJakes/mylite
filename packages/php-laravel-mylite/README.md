# MyLite Laravel Driver

This Composer package registers the `mylite` database driver in Laravel 12.
It requires the MyLite core and PDO PHP extensions.

Install the package and configure a connection in `config/database.php`:

```php
'mylite' => [
    'driver' => 'mylite',
    'path' => database_path('application.mylite'),
    'database' => 'application',
    'prefix' => '',
    'options' => [],
],
```

Laravel package discovery loads the service provider. The `path` selects the
embedded file, while `database` is the logical MySQL database created and
selected when the connection opens. The exact `:memory:` path selects a
nonpersistent database.

The package uses Laravel's standard MySQL connection, query grammar, schema
grammar, postprocessor, exception conversion, and transaction integration.
Network hosts, ports, TLS, replicas, persistent connections, and read/write
splitting are not supported.
