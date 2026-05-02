<?php

require __DIR__ . '/bootstrap.php';

foreach (['mysqli_info', 'mysqli_refresh'] as $function) {
    if (!function_exists($function)) {
        throw new RuntimeException($function . ' is missing');
    }
}

foreach (
    [
        'MYSQLI_CLIENT_INTERACTIVE',
        'MYSQLI_OPT_SSL_VERIFY_SERVER_CERT',
        'MYSQLI_REFRESH_TABLES',
        'MYSQLI_TRANS_START_READ_ONLY',
        'MYSQLI_TYPE_JSON',
        'MYSQLI_TYPE_GEOMETRY',
    ] as $constant
) {
    if (!defined($constant)) {
        throw new RuntimeException($constant . ' is missing');
    }
}

$query = new ReflectionMethod(mysqli::class, 'query');
expect_same(1, $query->getNumberOfRequiredParameters(), 'mysqli::query required parameter count');
expect_same(2, $query->getNumberOfParameters(), 'mysqli::query parameter count');

$stmtExecute = new ReflectionMethod(mysqli_stmt::class, 'execute');
expect_same(0, $stmtExecute->getNumberOfRequiredParameters(), 'mysqli_stmt::execute required parameter count');
expect_same(1, $stmtExecute->getNumberOfParameters(), 'mysqli_stmt::execute parameter count');

$resultDataSeek = new ReflectionMethod(mysqli_result::class, 'data_seek');
expect_same(1, $resultDataSeek->getNumberOfRequiredParameters(), 'mysqli_result::data_seek required parameter count');
expect_same(1, $resultDataSeek->getNumberOfParameters(), 'mysqli_result::data_seek parameter count');

$mysqli = open_mylite_mysqli();
expect_same('8.4.9', $mysqli->server_info, 'mysqli server_info property');
expect_same(80409, $mysqli->server_version, 'mysqli server_version property');
expect_same('8.4.9', mysqli_get_server_info($mysqli), 'mysqli_get_server_info');
expect_same(80409, mysqli_get_server_version($mysqli), 'mysqli_get_server_version');
