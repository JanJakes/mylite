<?php

require __DIR__ . '/bootstrap.php';

function mylite_mysqli_path(string $name): string
{
    $path = sys_get_temp_dir() . '/mylite-php-mysqli-' . getmypid() . '-' . $name . '.mylite';
    if (file_exists($path)) {
        unlink($path);
    }
    register_shutdown_function(static function () use ($path): void {
        if (file_exists($path)) {
            unlink($path);
        }
    });
    return $path;
}

function expect_mysqli_path_rejected(
    ?string $host,
    ?string $database,
    ?string $socket,
    string $filesystemPrefix,
    string $context
): void {
    $link = mysqli_init();
    expect_false(
        $link->real_connect($host, '', '', $database, 0, $socket),
        $context . ' real_connect'
    );
    expect_same(2002, $link->connect_errno, $context . ' connect errno');
    expect_same('HY000', $link->sqlstate, $context . ' SQLSTATE');
    expect_same(
        'MyLite database paths do not support NUL bytes',
        $link->connect_error,
        $context . ' connect error'
    );
    expect_false(file_exists($filesystemPrefix), $context . ' filesystem prefix');
    $link->close();
}

$nulAtStartPrefix = mylite_mysqli_path('nul-at-start');
expect_mysqli_path_rejected(
    "mylite:\0" . $nulAtStartPrefix,
    null,
    null,
    $nulAtStartPrefix,
    'mylite host path with leading NUL'
);

$nulInMiddlePrefix = mylite_mysqli_path('nul-in-middle');
expect_mysqli_path_rejected(
    $nulInMiddlePrefix . "\0.bypass",
    null,
    null,
    $nulInMiddlePrefix,
    'direct host path with embedded NUL'
);

$nulAtEndPrefix = mylite_mysqli_path('nul-at-end');
expect_mysqli_path_rejected(
    'localhost:' . $nulAtEndPrefix . "\0",
    null,
    null,
    $nulAtEndPrefix,
    'localhost host path with trailing NUL'
);

$socketPrefix = mylite_mysqli_path('socket');
expect_mysqli_path_rejected(
    'localhost',
    null,
    $socketPrefix . "\0.bypass",
    $socketPrefix,
    'socket path with embedded NUL'
);

$databasePrefix = mylite_mysqli_path('database');
expect_mysqli_path_rejected(
    'localhost',
    $databasePrefix . "\0.bypass",
    null,
    $databasePrefix,
    'database path with embedded NUL'
);

$preservedPrefix = mylite_mysqli_path('preserved-prefix');
$preserved = mysqli_init();
expect_true(
    $preserved->real_connect('localhost', '', '', null, 0, $preservedPrefix),
    'preserved-prefix connect'
);
expect_true($preserved->query('CREATE DATABASE app'), 'preserved-prefix create database');
$preservedHash = hash_file('sha256', $preservedPrefix);
if (!is_string($preservedHash)) {
    throw new RuntimeException('preserved-prefix hash failed');
}
expect_false(
    $preserved->real_connect(
        'localhost',
        '',
        '',
        null,
        0,
        $preservedPrefix . "\0.bypass"
    ),
    'connected-link existing-prefix bypass'
);
expect_true(
    $preserved->query('SELECT 1') instanceof mysqli_result,
    'rejected reconnect changed the published link'
);
$preserved->close();
expect_mysqli_path_rejected(
    'localhost',
    null,
    $preservedPrefix . "\0.bypass",
    $preservedPrefix . '.bypass',
    'existing-prefix bypass'
);
expect_same(
    $preservedHash,
    hash_file('sha256', $preservedPrefix),
    'rejected path changed the existing prefix'
);

$emptyFallback = mysqli_init();
expect_true(
    $emptyFallback->real_connect('mylite:', '', '', null, 0, null),
    'empty path memory fallback connect'
);
$emptyFallback->close();

$memory = mysqli_init();
expect_true(
    $memory->real_connect('mylite::memory:', '', '', null, 0, null),
    'exact memory path connect'
);
$memory->close();

$unicodePath = mylite_mysqli_path('café');
$unicode = mysqli_init();
expect_true(
    $unicode->real_connect('localhost', '', '', null, 0, $unicodePath),
    'non-ASCII socket path connect'
);
$unicode->close();
expect_true(file_exists($unicodePath), 'non-ASCII path was not created');
