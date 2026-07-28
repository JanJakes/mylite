<?php

declare(strict_types=1);

namespace MyLite\Doctrine\DBAL;

use Doctrine\DBAL\Driver\AbstractMySQLDriver;
use Doctrine\DBAL\Driver\PDO\Connection;
use Doctrine\DBAL\Driver\PDO\Exception;
use MyLite\Doctrine\DBAL\Exception\InvalidConfiguration;
use PDO;
use PDOException;
use SensitiveParameter;

final class Driver extends AbstractMySQLDriver
{
    public function connect(#[SensitiveParameter] array $params): Connection
    {
        $path = self::path($params);
        $database = self::database($params);
        $options = self::options($params);
        $username = self::optionalString($params, 'user');
        $password = self::optionalString($params, 'password');

        try {
            $pdo = PHP_VERSION_ID < 80400
                ? new PDO('mylite:'.$path, $username, $password, $options)
                : PDO::connect('mylite:'.$path, $username, $password, $options);
            if ($database !== '') {
                $pdo->exec('CREATE DATABASE IF NOT EXISTS '.self::quoteIdentifier($database));
                $pdo->exec('USE '.self::quoteIdentifier($database));
            }

            return new Connection($pdo);
        } catch (PDOException $exception) {
            throw Exception::new($exception);
        }
    }

    private static function path(array $params): string
    {
        $path = $params['path'] ?? null;
        if (! is_string($path) || $path === '') {
            throw InvalidConfiguration::value(
                'The MyLite connection path must be a nonempty string or :memory:.'
            );
        }
        if (str_contains($path, "\0")) {
            throw InvalidConfiguration::value('The MyLite connection path must not contain NUL bytes.');
        }

        return $path;
    }

    private static function database(array $params): string
    {
        $database = $params['dbname'] ?? '';
        if (! is_string($database)) {
            throw InvalidConfiguration::value('The MyLite logical database must be a string.');
        }

        return $database;
    }

    private static function options(array $params): array
    {
        $options = $params['driverOptions'] ?? [];
        if (! is_array($options)) {
            throw InvalidConfiguration::value('MyLite driverOptions must be an array.');
        }
        if (! empty($params['persistent'])) {
            throw InvalidConfiguration::value('MyLite persistent connections are not supported.');
        }

        $options[PDO::ATTR_ERRMODE] = PDO::ERRMODE_EXCEPTION;
        $options[PDO::ATTR_DEFAULT_FETCH_MODE] ??= PDO::FETCH_ASSOC;

        return $options;
    }

    private static function optionalString(array $params, string $key): string
    {
        $value = $params[$key] ?? '';
        if (! is_string($value)) {
            throw InvalidConfiguration::value("The MyLite {$key} must be a string or null.");
        }

        return $value;
    }

    private static function quoteIdentifier(string $value): string
    {
        return '`'.str_replace('`', '``', $value).'`';
    }
}
