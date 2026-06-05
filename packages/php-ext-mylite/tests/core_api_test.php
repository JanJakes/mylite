<?php

declare(strict_types=1);

function expect_true(bool $condition, string $message): void
{
    if (!$condition) {
        throw new RuntimeException($message);
    }
}

function mylite_test_path(string $name): string
{
    $path = sys_get_temp_dir() . '/mylite-php-core-' . getmypid() . '-' . $name . '.mylite';
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

expect_true(extension_loaded('mylite'), 'mylite extension is not loaded');
expect_true(mylite_version() !== '', 'mylite_version() returned an empty string');
expect_true(MYLITE_OK === 0, 'MYLITE_OK constant mismatch');
expect_true(MYLITE_NOMEM === 7, 'MYLITE_NOMEM constant mismatch');

$memory = mylite_open(':memory:');
expect_true($memory instanceof MyLite\Connection, 'mylite_open(:memory:) did not return a connection');
expect_true($memory->query('SELECT 1 AS value')->fetchAssociative() === ['value' => '1'], 'memory query mismatch');
expect_true($memory->close(), 'memory close failed');

$path = mylite_test_path('native');
$db = mylite_open($path);
expect_true($db instanceof MyLite\Connection, 'mylite_open(path) did not return a connection');
expect_true($db->exec('CREATE DATABASE app') >= 0, 'CREATE DATABASE failed');
expect_true($db->exec('USE app') >= 0, 'USE failed');
expect_true($db->exec('CREATE TABLE people (id INT PRIMARY KEY, name VARCHAR(32))') >= 0, 'CREATE TABLE failed');
expect_true($db->exec("INSERT INTO people VALUES (1, 'Ada')") === 1, 'INSERT affected rows mismatch');
expect_true($db->changes() === 1, 'changes mismatch');

$result = $db->query('SELECT id, name FROM people ORDER BY id');
expect_true($result instanceof MyLite\Result, 'query did not return a result');
expect_true($result->fetchAssociative() === ['id' => '1', 'name' => 'Ada'], 'row mismatch');
expect_true($result->fetchAssociative() === null, 'result should be exhausted');

$stmt = $db->prepare('INSERT INTO people VALUES (?, ?)');
expect_true($stmt instanceof MyLite\Statement, 'prepare did not return a statement');
expect_true($stmt->bindValue(1, 2), 'bind first parameter');
expect_true($stmt->bindValue(2, 'Grace'), 'bind second parameter');
expect_true($stmt->execute(), 'execute insert statement');

$stmt = $db->prepare('SELECT name FROM people WHERE id = ?');
expect_true($stmt->execute([2]), 'execute select statement');
expect_true($stmt->fetchAssociative() === ['name' => 'Grace'], 'prepared row mismatch');
expect_true($stmt->fetchAssociative() === null, 'prepared result should be exhausted');

$stmt = $db->prepare('SELECT ? AS first, ? AS second');
expect_true($stmt->execute([1, 2]), 'execute two-parameter statement');
try {
    $stmt->execute([1]);
    throw new RuntimeException('too few execute parameters did not throw');
} catch (MyLite\Exception $exception) {
    expect_true($exception->getCode() === MYLITE_MISUSE, 'too few parameter exception code');
}

$stmt = $db->prepare('SELECT ? AS only_value');
try {
    $stmt->execute([1, 2]);
    throw new RuntimeException('too many execute parameters did not throw');
} catch (MyLite\Exception $exception) {
    expect_true($exception->getCode() === MYLITE_MISUSE, 'too many parameter exception code');
}

try {
    $db->query('SELECT * FROM missing_table');
    throw new RuntimeException('invalid query did not throw');
} catch (MyLite\Exception $exception) {
    expect_true($exception->getCode() !== MYLITE_OK, 'exception code should report failure');
    expect_true($db->sqlState() !== '00000', 'SQLSTATE should report failure');
    expect_true($db->errorMessage() !== '', 'error message should be populated');
}

expect_true($db->close(), 'close failed');

$path = mylite_test_path('constructor');
$db = new MyLite\Connection($path);
expect_true($db->exec('CREATE DATABASE app') >= 0, 'file CREATE DATABASE failed');
expect_true($db->exec('USE app') >= 0, 'file USE failed');
expect_true($db->exec('CREATE TABLE files (id INT PRIMARY KEY)') >= 0, 'file CREATE TABLE failed');
expect_true($db->close(), 'file close failed');
expect_true(file_exists($path), 'file database was not created');
