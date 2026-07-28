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

function expect_contains(string $needle, mixed $actual, string $context): void
{
    if (!is_string($actual) || !str_contains($actual, $needle)) {
        throw new RuntimeException(
            $context . ': expected string containing ' . var_export($needle, true) .
            ', got ' . var_export($actual, true)
        );
    }
}

function diagnostic_test_path(): string
{
    $path = sys_get_temp_dir() . '/mylite-pdo-diagnostics-' . getmypid() . '.mylite';
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

$pdo = new PDO('mylite:' . diagnostic_test_path(), null, null, [
    PDO::ATTR_ERRMODE => PDO::ERRMODE_SILENT,
]);
expect_same(true, $pdo->exec('CREATE DATABASE app') !== false, 'create database');
expect_same(true, $pdo->exec('USE app') !== false, 'select database');
expect_same(
    true,
    $pdo->exec('CREATE TABLE first_rows (id INT PRIMARY KEY)') !== false,
    'create first table'
);
expect_same(
    true,
    $pdo->exec('CREATE TABLE second_rows (id INT PRIMARY KEY)') !== false,
    'create second table'
);
expect_same(1, $pdo->exec('INSERT INTO first_rows VALUES (1)'), 'seed first table');
expect_same(1, $pdo->exec('INSERT INTO second_rows VALUES (1)'), 'seed second table');

$first = $pdo->prepare('INSERT INTO first_rows VALUES (?)');
$second = $pdo->prepare('INSERT INTO second_rows VALUES (?)');
$success = $pdo->prepare('SELECT ?');
expect_same(true, $first instanceof PDOStatement, 'prepare first statement');
expect_same(true, $second instanceof PDOStatement, 'prepare second statement');
expect_same(true, $success instanceof PDOStatement, 'prepare successful statement');

expect_same(false, $first->execute([1]), 'first duplicate failure');
$firstInfo = $first->errorInfo();
expect_same('23000', $firstInfo[0], 'first duplicate SQLSTATE');
expect_same(1062, $firstInfo[1], 'first duplicate code');
expect_contains('first_rows.PRIMARY', $firstInfo[2], 'first duplicate message');
expect_same(['00000', null, null], $pdo->errorInfo(), 'connection after first statement error');

expect_same(false, $second->execute([1]), 'second duplicate failure');
$secondInfo = $second->errorInfo();
expect_same('23000', $secondInfo[0], 'second duplicate SQLSTATE');
expect_same(1062, $secondInfo[1], 'second duplicate code');
expect_contains('second_rows.PRIMARY', $secondInfo[2], 'second duplicate message');
expect_contains('first_rows.PRIMARY', $first->errorInfo()[2], 'first message retained');

expect_same(true, $success->execute([7]), 'intervening successful statement');
expect_same(['00000', null, null], $success->errorInfo(), 'successful statement diagnostics');
expect_contains('first_rows.PRIMARY', $first->errorInfo()[2], 'first message after success');
expect_contains('second_rows.PRIMARY', $second->errorInfo()[2], 'second message after success');
expect_same(['00000', null, null], $pdo->errorInfo(), 'connection remains independent');

expect_same(true, $first->execute([2]), 'successful first re-execution');
expect_same(['00000', null, null], $first->errorInfo(), 'successful re-execution clears first');
expect_contains('second_rows.PRIMARY', $second->errorInfo()[2], 'second survives first re-execution');

unset($first, $second, $success, $pdo);

echo "pdo_statement_diagnostics: ok\n";
