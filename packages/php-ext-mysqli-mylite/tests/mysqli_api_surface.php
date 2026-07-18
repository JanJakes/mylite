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

$portLink = mysqli_init();
expect_false(
    mysqli_real_connect($portLink, 'localhost', '', '', null, 3306),
    'network port must be rejected'
);
expect_same(1235, mysqli_connect_errno(), 'network port connect errno');
$flagLink = mysqli_init();
expect_false(
    $flagLink->real_connect('localhost', '', '', null, 0, null, MYSQLI_CLIENT_FOUND_ROWS),
    'client flags must be rejected'
);
expect_same(1235, $flagLink->connect_errno, 'client flag connect errno');

expect_false($mysqli->query('SELECT 1', MYSQLI_ASYNC), 'async query mode must be rejected');
expect_same(1235, $mysqli->errno, 'async query mode errno');
expect_true($mysqli->real_query('SELECT 1'), 'prepare pending store result');
expect_false($mysqli->store_result(999), 'unknown store-result mode must be rejected');
$copiedResult = $mysqli->store_result(MYSQLI_STORE_RESULT_COPY_DATA);
expect_true($copiedResult instanceof mysqli_result, 'copy-data store-result mode');
expect_same('1', $copiedResult->fetch_column(), 'copy-data stored value');
expect_true(mysqli_real_query($mysqli, 'SELECT 2'), 'prepare procedural pending store result');
expect_false(
    mysqli_store_result($mysqli, 999),
    'procedural unknown store-result mode must be rejected'
);
$proceduralCopiedResult = mysqli_store_result($mysqli, MYSQLI_STORE_RESULT_COPY_DATA);
expect_true($proceduralCopiedResult instanceof mysqli_result, 'procedural copy-data mode');
expect_same('2', $proceduralCopiedResult->fetch_column(), 'procedural copy-data value');

expect_true($mysqli->query('CREATE TABLE transaction_flags (id INT PRIMARY KEY)'), 'create flag table');
expect_true(
    mysqli_begin_transaction($mysqli, MYSQLI_TRANS_START_READ_ONLY),
    'procedural read-only transaction'
);
expect_false(
    mysqli_query($mysqli, 'INSERT INTO transaction_flags VALUES (1)'),
    'read-only flag must reject writes'
);
expect_true(mysqli_rollback($mysqli), 'rollback read-only transaction');

expect_true(
    $mysqli->begin_transaction(MYSQLI_TRANS_START_READ_WRITE),
    'object read-write transaction'
);
expect_true($mysqli->query('INSERT INTO transaction_flags VALUES (1)'), 'insert before chain');
expect_true($mysqli->commit(MYSQLI_TRANS_COR_AND_CHAIN), 'commit and chain');
expect_true($mysqli->query('INSERT INTO transaction_flags VALUES (2)'), 'insert in chained transaction');
expect_true(
    mysqli_rollback($mysqli, MYSQLI_TRANS_COR_AND_NO_CHAIN | MYSQLI_TRANS_COR_NO_RELEASE),
    'rollback chained transaction'
);
expect_same(
    '1',
    $mysqli->query('SELECT COUNT(*) FROM transaction_flags')->fetch_column(),
    'chained rollback must preserve only committed row'
);

expect_false(
    $mysqli->begin_transaction(
        MYSQLI_TRANS_START_READ_ONLY | MYSQLI_TRANS_START_READ_WRITE
    ),
    'conflicting start flags'
);
expect_same(1235, $mysqli->errno, 'conflicting start flag errno');
expect_false($mysqli->begin_transaction(0, 'named'), 'named transaction rejection');
expect_same(1235, $mysqli->errno, 'named transaction errno');
expect_true($mysqli->begin_transaction(), 'begin before release rejection');
expect_false($mysqli->commit(MYSQLI_TRANS_COR_RELEASE), 'release flag rejection');
expect_same(1235, $mysqli->errno, 'release flag errno');
expect_true($mysqli->rollback(), 'rollback after release rejection');

expect_false(
    mysqli_options($mysqli, MYSQLI_OPT_CONNECT_TIMEOUT, 1),
    'procedural unsupported option'
);
expect_same(1235, mysqli_errno($mysqli), 'procedural option errno');
expect_false($mysqli->options(MYSQLI_OPT_READ_TIMEOUT, 1), 'object unsupported option');
expect_false($mysqli->set_opt(MYSQLI_OPT_LOCAL_INFILE, 0), 'object unsupported set_opt');
expect_false(
    mysqli_ssl_set($mysqli, null, null, null, null, null),
    'procedural unsupported TLS'
);
expect_false($mysqli->ssl_set(null, null, null, null, null), 'object unsupported TLS');
expect_false(mysqli_dump_debug_info($mysqli), 'procedural unsupported debug dump');
expect_false($mysqli->dump_debug_info(), 'object unsupported debug dump');
expect_false(mysqli_debug('d:t:o,/tmp/mylite.trace'), 'procedural unsupported debug tracing');
expect_false($mysqli->debug('d:t:o,/tmp/mylite.trace'), 'object unsupported debug tracing');

$attributeStatement = $mysqli->prepare('SELECT 1');
expect_true($attributeStatement instanceof mysqli_stmt, 'attribute statement prepare');
expect_false(
    mysqli_stmt_attr_set($attributeStatement, MYSQLI_STMT_ATTR_CURSOR_TYPE, MYSQLI_CURSOR_TYPE_READ_ONLY),
    'procedural unsupported statement attribute set'
);
expect_same(1235, mysqli_stmt_errno($attributeStatement), 'procedural statement attribute errno');
expect_false(
    mysqli_stmt_attr_get($attributeStatement, MYSQLI_STMT_ATTR_CURSOR_TYPE),
    'procedural unsupported statement attribute get'
);
expect_false(
    $attributeStatement->attr_set(MYSQLI_STMT_ATTR_UPDATE_MAX_LENGTH, 1),
    'object unsupported statement attribute set'
);
expect_false(
    $attributeStatement->attr_get(MYSQLI_STMT_ATTR_UPDATE_MAX_LENGTH),
    'object unsupported statement attribute get'
);
$attributeStatement->close();
