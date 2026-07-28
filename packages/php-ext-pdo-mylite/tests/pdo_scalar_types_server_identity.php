<?php

declare(strict_types=1);

function expect_same(mixed $expected, mixed $actual, string $context): void
{
    if ($expected !== $actual) {
        throw new RuntimeException(
            $context . ': expected ' . var_export($expected, true) .
            ', got ' . var_export($actual, true)
        );
    }
}

function scalar_test_path(): string
{
    $path = sys_get_temp_dir() . '/mylite-pdo-scalars-' . getmypid() . '.mylite';
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

$pdo = new PDO('mylite:' . scalar_test_path(), null, null, [
    PDO::ATTR_ERRMODE => PDO::ERRMODE_EXCEPTION,
]);

expect_same(mylite_version(), $pdo->getAttribute(PDO::ATTR_CLIENT_VERSION), 'client version');
expect_same('8.4.9', $pdo->getAttribute(PDO::ATTR_SERVER_VERSION), 'server version');
expect_same(
    $pdo->query('SELECT VERSION()')->fetchColumn(),
    $pdo->getAttribute(PDO::ATTR_SERVER_VERSION),
    'server version equals VERSION()'
);

$pdo->exec('CREATE DATABASE app');
$pdo->exec('USE app');
$pdo->exec(
    'CREATE TABLE typed_values (' .
    'signed_value BIGINT, unsigned_value BIGINT UNSIGNED, tiny_value TINYINT, ' .
    'unsigned_int INT UNSIGNED, bit_value BIT(8), float_value FLOAT, ' .
    'double_value DOUBLE, decimal_value DECIMAL(20,4), binary_value VARBINARY(8), ' .
    'text_value VARCHAR(8), null_value INT NULL)'
);
$pdo->exec(
    "INSERT INTO typed_values VALUES (" .
    "-9223372036854775808, 9223372036854775807, -128, 4294967295, " .
    "b'10100101', 1.25, -2.5, 1234567890123456.2500, UNHEX('610062'), 'text', NULL)"
);

$native = [
    'signed_value' => PHP_INT_MIN,
    'unsigned_value' => PHP_INT_MAX,
    'tiny_value' => -128,
    'unsigned_int' => 4294967295,
    'bit_value' => 165,
    'float_value' => 1.25,
    'double_value' => -2.5,
    'decimal_value' => '1234567890123456.2500',
    'binary_value' => "a\0b",
    'text_value' => 'text',
    'null_value' => null,
];
$strings = [
    'signed_value' => '-9223372036854775808',
    'unsigned_value' => '9223372036854775807',
    'tiny_value' => '-128',
    'unsigned_int' => '4294967295',
    'bit_value' => '165',
    'float_value' => '1.25',
    'double_value' => '-2.5',
    'decimal_value' => '1234567890123456.2500',
    'binary_value' => "a\0b",
    'text_value' => 'text',
    'null_value' => null,
];

$statement = $pdo->prepare('SELECT * FROM typed_values');
expect_same(true, $statement->execute(), 'native execute');
expect_same($native, $statement->fetch(PDO::FETCH_ASSOC), 'native scalar conversion');

expect_same(true, $pdo->setAttribute(PDO::ATTR_STRINGIFY_FETCHES, true), 'enable stringify');
expect_same(true, $statement->execute(), 'stringified execute');
expect_same($strings, $statement->fetch(PDO::FETCH_ASSOC), 'stringified scalar conversion');

expect_same(true, $pdo->setAttribute(PDO::ATTR_STRINGIFY_FETCHES, false), 'disable stringify');
expect_same(true, $statement->execute(), 'restored native execute');
expect_same(PHP_INT_MIN, $statement->fetchColumn(), 'runtime stringify toggle');

$overflow = $pdo->prepare('SELECT LAST_INSERT_ID(-1) AS overflow_value');
expect_same(true, $overflow->execute(), 'overflow execute');
expect_same('18446744073709551615', $overflow->fetchColumn(), 'overflow remains string');
