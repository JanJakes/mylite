<?php

declare(strict_types=1);

namespace MyLite\Laravel;

use Illuminate\Database\Connectors\Connector;
use Illuminate\Database\Connectors\ConnectorInterface;
use InvalidArgumentException;
use PDO;
use RuntimeException;

final class MyLiteConnector extends Connector implements ConnectorInterface
{
    public function connect(array $config): PDO
    {
        $path = self::path($config);
        $database = self::database($config);
        if (isset($config['options']) && ! is_array($config['options'])) {
            throw new InvalidArgumentException('MyLite connection options must be an array.');
        }

        $pdo = $this->createConnection('mylite:'.$path, $config, $this->getOptions($config));
        if ($database !== '') {
            self::executeBootstrap($pdo, 'CREATE DATABASE IF NOT EXISTS '.self::quoteIdentifier($database));
            self::executeBootstrap($pdo, 'USE '.self::quoteIdentifier($database));
        }

        return $pdo;
    }

    private static function path(array $config): string
    {
        $path = $config['path'] ?? null;
        if (! is_string($path) || $path === '') {
            throw new InvalidArgumentException(
                'The MyLite connection path must be a nonempty string or :memory:.'
            );
        }
        if (str_contains($path, "\0")) {
            throw new InvalidArgumentException('The MyLite connection path must not contain NUL bytes.');
        }

        return $path;
    }

    private static function database(array $config): string
    {
        $database = $config['database'] ?? '';
        if (! is_string($database)) {
            throw new InvalidArgumentException('The MyLite logical database must be a string.');
        }

        return $database;
    }

    private static function executeBootstrap(PDO $pdo, string $sql): void
    {
        if ($pdo->exec($sql) === false) {
            throw new RuntimeException('Could not initialize the MyLite logical database.');
        }
    }

    private static function quoteIdentifier(string $value): string
    {
        return '`'.str_replace('`', '``', $value).'`';
    }
}
