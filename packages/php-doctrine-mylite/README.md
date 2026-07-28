# MyLite Doctrine DBAL Driver

This Composer package provides a Doctrine DBAL 4 driver for the MyLite PDO
extension.

Configure DBAL through its supported `driverClass` parameter:

```php
use Doctrine\DBAL\DriverManager;
use MyLite\Doctrine\DBAL\Driver;

$connection = DriverManager::getConnection([
    'driverClass' => Driver::class,
    'path' => '/path/to/application.mylite',
    'dbname' => 'application',
]);
```

The `path` selects the embedded file, while `dbname` is the logical MySQL
database created and selected when the connection opens. The exact `:memory:`
path selects a nonpersistent database.

The driver uses Doctrine's MySQL 8.4 platform, standard PDO connection wrapper,
and MySQL exception converter. Network hosts, ports, TLS, replicas, persistent
connections, and asynchronous execution are not supported.
