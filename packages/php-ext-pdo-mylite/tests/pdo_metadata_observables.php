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
    'indexed_code VARCHAR(16), ' .
    'nullable_decimal DECIMAL(10,2) NULL, body TEXT, payload BLOB, ' .
    'created_at DATETIME, location POINT, KEY idx_metadata_values_indexed_code (indexed_code))'
);
$pdo->exec(
    "INSERT INTO metadata_values " .
    "(unique_code, indexed_code, nullable_decimal, body, payload, created_at, location) VALUES " .
    "('one', 'lookup', 12.30, 'hello', UNHEX('610062'), '2026-07-27 12:34:56', POINT(1,2)), " .
    "('two', NULL, NULL, NULL, NULL, NULL, NULL)"
);

$tableMeta = [
    expected_meta('LONG', PDO::PARAM_INT, ['not_null', 'primary_key'], 'metadata_values', 'id', 10, 0),
    expected_meta('VAR_STRING', PDO::PARAM_STR, ['not_null', 'unique_key'], 'metadata_values', 'unique_code', 64, 0),
    expected_meta('VAR_STRING', PDO::PARAM_STR, ['multiple_key'], 'metadata_values', 'indexed_code', 64, 0),
    expected_meta('NEWDECIMAL', PDO::PARAM_STR, [], 'metadata_values', 'nullable_decimal', 12, 2),
    expected_meta('BLOB', PDO::PARAM_STR, ['blob'], 'metadata_values', 'body', 262140, 0),
    expected_meta('BLOB', PDO::PARAM_STR, ['blob'], 'metadata_values', 'payload', 65535, 0),
    expected_meta('DATETIME', PDO::PARAM_STR, [], 'metadata_values', 'created_at', 19, 0),
    expected_meta('GEOMETRY', PDO::PARAM_STR, ['blob'], 'metadata_values', 'location', 4294967295, 0),
];

$statement = $pdo->prepare(
    'SELECT id, unique_code, indexed_code, nullable_decimal, body, payload, created_at, location ' .
    'FROM metadata_values ORDER BY id'
);
expect_same(true, $statement->execute(), 'table metadata execute');
expect_same(2, $statement->rowCount(), 'buffered row count before fetch');
foreach ($tableMeta as $index => $expected) {
    expect_same($expected, $statement->getColumnMeta($index), "table metadata {$index}");
}
expect_same(2, count($statement->fetchAll()), 'buffered fetch count');
expect_same(2, $statement->rowCount(), 'buffered row count after fetch');

$direct = $pdo->query('SELECT id FROM metadata_values ORDER BY id');
expect_same(2, $direct->rowCount(), 'direct buffered row count before fetch');
expect_same(2, count($direct->fetchAll()), 'direct buffered fetch count');
expect_same(2, $direct->rowCount(), 'direct buffered row count after fetch');

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
    expected_meta('UNKNOWN', PDO::PARAM_STR, [], '', 'expression_value', 0, 0),
    $expressions->getColumnMeta(0),
    'expression native descriptor transport'
);
expect_same(
    expected_meta('LONGLONG', PDO::PARAM_INT, ['not_null'], '', 'aggregate_value', 21, 0),
    $expressions->getColumnMeta(1),
    'aggregate native descriptor transport'
);

$spatialTemporal = $pdo->prepare(
    "SELECT ST_AsText(POINT(1,2)) AS spatial_text, " .
    "ST_AsWKB(POINT(1,2)) AS spatial_binary, " .
    "CONVERT_TZ('2024-01-01 00:00:00.123','+00:00','+01:00') AS timezone_value"
);
expect_same(true, $spatialTemporal->execute(), 'spatial temporal metadata execute');
expect_same(
    expected_meta('LONG_BLOB', PDO::PARAM_STR, [], '', 'spatial_text', 268435456, 31),
    $spatialTemporal->getColumnMeta(0),
    'spatial text metadata transport'
);
expect_same(
    expected_meta('LONG_BLOB', PDO::PARAM_STR, [], '', 'spatial_binary', 4294967295, 31),
    $spatialTemporal->getColumnMeta(1),
    'spatial binary metadata transport'
);
expect_same(
    expected_meta('DATETIME', PDO::PARAM_STR, [], '', 'timezone_value', 23, 3),
    $spatialTemporal->getColumnMeta(2),
    'CONVERT_TZ metadata transport'
);

$aggregateMetadata = $pdo->prepare(
    'SELECT COUNT(*) AS count_value, SUM(id) AS sum_value, ' .
    'MIN(unique_code) AS min_value, GROUP_CONCAT(unique_code) AS concat_value ' .
    'FROM metadata_values'
);
expect_same(true, $aggregateMetadata->execute(), 'aggregate metadata execute');
expect_same(
    expected_meta('LONGLONG', PDO::PARAM_INT, ['not_null'], '', 'count_value', 21, 0),
    $aggregateMetadata->getColumnMeta(0),
    'count metadata transport'
);
expect_same(
    expected_meta('NEWDECIMAL', PDO::PARAM_STR, [], '', 'sum_value', 33, 0),
    $aggregateMetadata->getColumnMeta(1),
    'sum metadata transport'
);
expect_same(
    expected_meta('VAR_STRING', PDO::PARAM_STR, [], '', 'min_value', 64, 31),
    $aggregateMetadata->getColumnMeta(2),
    'minimum metadata transport'
);
expect_same(
    expected_meta('LONG_BLOB', PDO::PARAM_STR, [], '', 'concat_value', 65536, 31),
    $aggregateMetadata->getColumnMeta(3),
    'GROUP_CONCAT metadata transport'
);

$windowMetadata = $pdo->prepare(
    'SELECT ROW_NUMBER() OVER w AS row_number_value, SUM(id) OVER w AS sum_value, ' .
    'LAG(unique_code) OVER w AS lag_value, JSON_ARRAYAGG(id) OVER w AS json_value ' .
    'FROM metadata_values WINDOW w AS (ORDER BY id) LIMIT 1'
);
expect_same(true, $windowMetadata->execute(), 'window metadata execute');
expect_same(
    expected_meta('LONGLONG', PDO::PARAM_INT, ['not_null'], '', 'row_number_value', 21, 0),
    $windowMetadata->getColumnMeta(0),
    'row number metadata transport'
);
expect_same(
    expected_meta('NEWDECIMAL', PDO::PARAM_STR, [], '', 'sum_value', 33, 0),
    $windowMetadata->getColumnMeta(1),
    'window sum metadata transport'
);
expect_same(
    expected_meta('VAR_STRING', PDO::PARAM_STR, [], '', 'lag_value', 64, 0),
    $windowMetadata->getColumnMeta(2),
    'window navigation metadata transport'
);
expect_same(
    expected_meta('JSON', PDO::PARAM_STR, ['blob'], '', 'json_value', 4294967295, 0),
    $windowMetadata->getColumnMeta(3),
    'window JSON metadata transport'
);

$insert = $pdo->prepare('INSERT INTO metadata_values (unique_code) VALUES (?)');
expect_same(true, $insert->execute(['three']), 'prepared insert');
expect_same(1, $insert->rowCount(), 'prepared DML affected row count');
