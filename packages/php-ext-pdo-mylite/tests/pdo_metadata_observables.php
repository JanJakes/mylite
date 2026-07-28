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

function expected_meta(
    string $nativeType,
    int $pdoType,
    array $flags,
    string $table,
    string $name,
    int $length,
    int $precision
): array {
    return [
        'native_type' => $nativeType,
        'pdo_type' => $pdoType,
        'flags' => $flags,
        'table' => $table,
        'name' => $name,
        'len' => $length,
        'precision' => $precision,
    ];
}

$path = sys_get_temp_dir() . '/mylite-pdo-metadata-' . getmypid() . '.mylite';
if (file_exists($path)) {
    unlink($path);
}
register_shutdown_function(static function () use ($path): void {
    if (file_exists($path)) {
        unlink($path);
    }
});

$pdo = new PDO('mylite:' . $path, null, null, [
    PDO::ATTR_ERRMODE => PDO::ERRMODE_EXCEPTION,
]);
$pdo->exec('CREATE DATABASE app');
$pdo->exec('USE app');
$pdo->exec(
    'CREATE TABLE metadata_values (' .
    'id INT UNSIGNED NOT NULL AUTO_INCREMENT PRIMARY KEY, ' .
    'unique_code VARCHAR(16) NOT NULL UNIQUE, ' .
    'nullable_decimal DECIMAL(10,2) NULL, body TEXT, payload BLOB, ' .
    'created_at DATETIME, location POINT)'
);
$pdo->exec(
    "INSERT INTO metadata_values " .
    "(unique_code, nullable_decimal, body, payload, created_at, location) VALUES " .
    "('one', 12.30, 'hello', UNHEX('610062'), '2026-07-27 12:34:56', POINT(1,2)), " .
    "('two', NULL, NULL, NULL, NULL, NULL)"
);

$tableMeta = [
    expected_meta('LONG', PDO::PARAM_INT, ['not_null', 'primary_key'], 'metadata_values', 'id', 10, 0),
    expected_meta('VAR_STRING', PDO::PARAM_STR, ['not_null', 'unique_key'], 'metadata_values', 'unique_code', 64, 0),
    expected_meta('NEWDECIMAL', PDO::PARAM_STR, [], 'metadata_values', 'nullable_decimal', 12, 2),
    expected_meta('BLOB', PDO::PARAM_STR, ['blob'], 'metadata_values', 'body', 262140, 0),
    expected_meta('BLOB', PDO::PARAM_STR, ['blob'], 'metadata_values', 'payload', 65535, 0),
    expected_meta('DATETIME', PDO::PARAM_STR, [], 'metadata_values', 'created_at', 19, 0),
    expected_meta('GEOMETRY', PDO::PARAM_STR, ['blob'], 'metadata_values', 'location', 4294967295, 0),
];

$statement = $pdo->prepare(
    'SELECT id, unique_code, nullable_decimal, body, payload, created_at, location ' .
    'FROM metadata_values ORDER BY id'
);
expect_same(true, $statement->execute(), 'table metadata execute');
expect_same(2, $statement->rowCount(), 'buffered row count before fetch');
foreach ($tableMeta as $index => $expected) {
    expect_same($expected, $statement->getColumnMeta($index), "table metadata {$index}");
}
expect_same(2, count($statement->fetchAll()), 'buffered fetch count');
expect_same(2, $statement->rowCount(), 'buffered row count after fetch');

$empty = $pdo->prepare('SELECT id, unique_code FROM metadata_values WHERE id = 999');
expect_same(true, $empty->execute(), 'empty metadata execute');
expect_same(0, $empty->rowCount(), 'empty buffered row count');
expect_same($tableMeta[0], $empty->getColumnMeta(0), 'empty integer metadata');
expect_same($tableMeta[1], $empty->getColumnMeta(1), 'empty text metadata');

$expressions = $pdo->prepare(
    'SELECT id + 1 AS expression_value, COUNT(*) AS aggregate_value ' .
    'FROM metadata_values GROUP BY id ORDER BY id'
);
expect_same(true, $expressions->execute(), 'expression metadata execute');
expect_same(2, $expressions->rowCount(), 'expression buffered row count');
expect_same(
    expected_meta('VAR_STRING', PDO::PARAM_STR, [], '', 'expression_value', 0, 0),
    $expressions->getColumnMeta(0),
    'expression native descriptor transport'
);
expect_same(
    expected_meta('VAR_STRING', PDO::PARAM_STR, [], '', 'aggregate_value', 0, 0),
    $expressions->getColumnMeta(1),
    'aggregate native descriptor transport'
);

$insert = $pdo->prepare('INSERT INTO metadata_values (unique_code) VALUES (?)');
expect_same(true, $insert->execute(['three']), 'prepared insert');
expect_same(1, $insert->rowCount(), 'prepared DML affected row count');
