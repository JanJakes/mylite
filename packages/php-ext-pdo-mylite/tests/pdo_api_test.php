<?php

declare(strict_types=1);

function expect_true(bool $condition, string $message): void
{
    if (!$condition) {
        throw new RuntimeException($message);
    }
}

expect_true(extension_loaded('mylite'), 'mylite extension is not loaded');
expect_true(extension_loaded('pdo_mylite'), 'pdo_mylite extension is not loaded');
expect_true(in_array('mylite', PDO::getAvailableDrivers(), true), 'mylite PDO driver is not registered');

function mylite_pdo_test_path(string $name): string
{
    $path = sys_get_temp_dir() . '/mylite-php-pdo-' . getmypid() . '-' . $name . '.mylite';
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

$pdo = new PDO('mylite:' . mylite_pdo_test_path('api'), null, null, [
    PDO::ATTR_ERRMODE => PDO::ERRMODE_EXCEPTION,
    PDO::ATTR_DEFAULT_FETCH_MODE => PDO::FETCH_ASSOC,
]);

expect_true($pdo->getAttribute(PDO::ATTR_DRIVER_NAME) === 'mylite', 'driver name mismatch');
expect_true($pdo->exec('CREATE DATABASE app') >= 0, 'CREATE DATABASE failed');
expect_true($pdo->exec('USE app') >= 0, 'USE failed');
expect_true($pdo->exec('CREATE TABLE people (id INT PRIMARY KEY, name VARCHAR(32))') >= 0, 'CREATE TABLE failed');
expect_true($pdo->exec("INSERT INTO people VALUES (1, 'Ada')") === 1, 'INSERT failed');

$stmt = $pdo->prepare('INSERT INTO people VALUES (?, ?)');
expect_true($stmt instanceof PDOStatement, 'prepare did not return PDOStatement');
expect_true($stmt->execute([2, 'Grace']), 'prepared INSERT failed');

$stmt = $pdo->prepare('SELECT name FROM people WHERE name = ?');
expect_true($stmt->execute(['Grace']), 'prepared SELECT failed');
expect_true($stmt->fetch() === ['name' => 'Grace'], 'prepared SELECT row mismatch');
expect_true($stmt->fetch() === false, 'prepared SELECT should be exhausted');

expect_true($pdo->beginTransaction(), 'beginTransaction failed');
expect_true($pdo->exec("INSERT INTO people VALUES (3, 'Katherine')") === 1, 'transaction INSERT failed');
expect_true($pdo->rollBack(), 'rollBack failed');
expect_true($pdo->query('SELECT COUNT(*) AS total FROM people')->fetch() === ['total' => '2'], 'rollback count mismatch');

$expectedEscape = 'A' . '\\0' . '\\n' . '\\r' . '\\\\' . "\\'" . '\\"' . '\\Z' . 'B';
expect_true($pdo->quote("A\0\n\r\\'\"\x1aB") === "'" . $expectedEscape . "'", 'PDO quote mismatch');

$pdo->setAttribute(PDO::ATTR_ERRMODE, PDO::ERRMODE_SILENT);
expect_true($pdo->exec('INSERT INTO missing_table VALUES (1)') === false, 'silent invalid query should fail');
$errorInfo = $pdo->errorInfo();
expect_true($errorInfo[0] !== '00000', 'PDO errorInfo SQLSTATE mismatch');
expect_true($errorInfo[1] > 0, 'PDO errorInfo native code mismatch');
expect_true($errorInfo[2] !== '', 'PDO errorInfo message mismatch');
