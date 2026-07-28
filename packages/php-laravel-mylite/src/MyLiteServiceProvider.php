<?php

declare(strict_types=1);

namespace MyLite\Laravel;

use Illuminate\Database\DatabaseManager;
use Illuminate\Database\MySqlConnection;
use Illuminate\Support\ServiceProvider;
use InvalidArgumentException;

final class MyLiteServiceProvider extends ServiceProvider
{
    public function register(): void
    {
        $this->app->booting(function (): void {
            $manager = $this->app->make('db');
            if (! $manager instanceof DatabaseManager) {
                throw new InvalidArgumentException('Laravel database manager is unavailable.');
            }

            $manager->extend('mylite', static function (array $config, string $name): MySqlConnection {
                $database = $config['database'] ?? '';
                $prefix = $config['prefix'] ?? '';
                if (! is_string($database)) {
                    throw new InvalidArgumentException('The MyLite logical database must be a string.');
                }
                if (! is_string($prefix)) {
                    throw new InvalidArgumentException('The MyLite table prefix must be a string.');
                }

                $config['name'] = $name;

                return new MySqlConnection(
                    (new MyLiteConnector())->connect($config),
                    $database,
                    $prefix,
                    $config
                );
            });
        });
    }
}
