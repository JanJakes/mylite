<?php

require __DIR__ . '/bootstrap.php';

$mysqli = open_mylite_mysqli();
expect_true(
    $mysqli->query(
        'CREATE TABLE typed_values (' .
        'signed_value BIGINT, unsigned_value BIGINT UNSIGNED, tiny_value TINYINT, ' .
        'unsigned_int INT UNSIGNED, bit_value BIT(8), float_value FLOAT, ' .
        'double_value DOUBLE, decimal_value DECIMAL(20,4), binary_value VARBINARY(8), ' .
        'text_value VARCHAR(8), null_value INT NULL)'
    ),
    'create typed table'
);
expect_true(
    $mysqli->query(
        "INSERT INTO typed_values VALUES (" .
        "-9223372036854775808, 9223372036854775807, -128, 4294967295, " .
        "b'10100101', 1.25, -2.5, 1234567890123456.2500, UNHEX('610062'), 'text', NULL)"
    ),
    'insert typed row'
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

$direct = $mysqli->query('SELECT * FROM typed_values');
expect_same(
    [
        'signed_value' => '-9223372036854775808',
        'unsigned_value' => '9223372036854775807',
        'tiny_value' => '-128',
        'unsigned_int' => '4294967295',
        'bit_value' => "\xa5",
        'float_value' => '1.25',
        'double_value' => '-2.5',
        'decimal_value' => '1234567890123456.2500',
        'binary_value' => "a\0b",
        'text_value' => 'text',
        'null_value' => null,
    ],
    $direct->fetch_assoc(),
    'direct scalar policy remains unchanged'
);
$direct->free();

$prepared = $mysqli->prepare('SELECT * FROM typed_values');
expect_true($prepared->execute(), 'prepared execute');
$preparedResult = $prepared->get_result();
expect_same($native, $preparedResult->fetch_assoc(), 'prepared get_result native values');
$preparedResult->free();
$prepared->close();

$prepared = $mysqli->prepare('SELECT * FROM typed_values');
expect_true($prepared->execute(), 'bound execute');
$bound = array_fill(0, count($native), null);
expect_true($prepared->bind_result(...$bound), 'bind prepared results');
expect_true($prepared->fetch(), 'fetch bound results');
expect_same(array_values($native), $bound, 'bound native values');
$prepared->close();

$executeQuery = $mysqli->execute_query(
    'SELECT * FROM typed_values WHERE signed_value = ?',
    [PHP_INT_MIN]
);
expect_same($native, $executeQuery->fetch_assoc(), 'execute_query native values');
$executeQuery->free();

$overflow = $mysqli->prepare('SELECT LAST_INSERT_ID(-1) AS overflow_value');
expect_true($overflow->execute(), 'overflow execute');
expect_same(
    ['overflow_value' => '18446744073709551615'],
    $overflow->get_result()->fetch_assoc(),
    'overflow remains string'
);
$overflow->close();
