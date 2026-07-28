<?php

require __DIR__ . '/bootstrap.php';

$path = tempnam(sys_get_temp_dir(), 'mylite_php_ext_empty_db_');
if ($path === false) {
    throw new RuntimeException('create temporary empty database path');
}
unlink($path);

$server_connection = new mysqli($path, '', '', '', 0, '');
expect_same(0, $server_connection->connect_errno, 'empty database connect errno');
expect_true($server_connection->query('CREATE DATABASE app'), 'empty database create schema');
expect_true($server_connection->select_db('app'), 'empty database select schema');
expect_true(
    $server_connection->query('CREATE TABLE empty_database_connection (id INT NOT NULL)'),
    'empty database create table'
);
$server_connection->close();

$mysqli = open_mylite_mysqli();

expect_true(
    $mysqli->query(
        "CREATE TABLE posts (
            id INT NOT NULL,
            views INT NOT NULL DEFAULT 0
        )"
    ),
    'create table'
);
expect_true($mysqli->query("INSERT INTO posts (id, views) VALUES (1, 10), (2, 20)"), 'insert rows');
expect_same(2, $mysqli->affected_rows, 'affected rows');
expect_same(0, $mysqli->insert_id, 'insert id');

expect_true(
    $mysqli->query('CREATE TABLE transaction_fast_path (id INT NOT NULL, views INT NOT NULL)'),
    'create transaction fast path table'
);
expect_true($mysqli->query(" \tSTART TRANSACTION; \n"), 'fast path start transaction');
expect_same(0, $mysqli->affected_rows, 'fast path start affected rows');
expect_true($mysqli->query('INSERT INTO transaction_fast_path VALUES (1, 10)'), 'fast path insert rollback row');
expect_true($mysqli->query('rollback'), 'fast path rollback');
expect_same(0, $mysqli->affected_rows, 'fast path rollback affected rows');
$transactionCount = $mysqli->query('SELECT COUNT(*) AS row_count FROM transaction_fast_path');
if (!$transactionCount instanceof mysqli_result) {
    throw new RuntimeException('fast path rollback count result type');
}
expect_same(['row_count' => '0'], $transactionCount->fetch_assoc(), 'fast path rollback count');

expect_true($mysqli->query('BEGIN;'), 'fast path begin');
expect_true($mysqli->query('INSERT INTO transaction_fast_path VALUES (2, 20)'), 'fast path insert commit row');
expect_true($mysqli->query('COMMIT'), 'fast path commit');
$transactionRows = $mysqli->query('SELECT id, views FROM transaction_fast_path ORDER BY id');
if (!$transactionRows instanceof mysqli_result) {
    throw new RuntimeException('fast path commit result type');
}
expect_same([['2', '20']], $transactionRows->fetch_all(MYSQLI_NUM), 'fast path commit rows');
expect_true($mysqli->query('ROLLBACK'), 'fast path no-op rollback');
expect_same(0, $mysqli->affected_rows, 'fast path no-op rollback affected rows');
expect_true($mysqli->query('BEGIN WORK'), 'parser begin work still supported');
expect_true($mysqli->query('ROLLBACK WORK'), 'parser rollback work still supported');

expect_true($mysqli->autocommit(false), 'object autocommit off');
expect_true(
    $mysqli->query('INSERT INTO transaction_fast_path VALUES (3, 30)'),
    'object autocommit pending insert'
);
expect_true($mysqli->rollback(), 'object autocommit rollback');
$autocommitRows = $mysqli->query('SELECT id FROM transaction_fast_path ORDER BY id');
if (!$autocommitRows instanceof mysqli_result) {
    throw new RuntimeException('object autocommit rollback result type');
}
expect_same([['2']], $autocommitRows->fetch_all(MYSQLI_NUM), 'object autocommit rollback rows');
expect_true($mysqli->autocommit(true), 'object autocommit on');

$result = $mysqli->query('SELECT id, views FROM posts ORDER BY id');
if (!$result instanceof mysqli_result) {
    throw new RuntimeException('query result type');
}

expect_same(2, $result->field_count, 'field count');
expect_same(2, $result->num_rows, 'row count');
expect_same(['id' => '1', 'views' => '10'], $result->fetch_assoc(), 'first assoc row');
expect_same(['2', '20'], $result->fetch_row(), 'second numeric row');
expect_same(null, $result->fetch_assoc(), 'result exhausted');

$stream = $mysqli->query('SELECT id, views FROM posts ORDER BY id', MYSQLI_USE_RESULT);
if (!$stream instanceof mysqli_result) {
    throw new RuntimeException('streaming query result type');
}
expect_same(MYSQLI_USE_RESULT, $stream->type, 'streaming result type');
expect_same(0, mysqli_num_rows($stream), 'streaming initial row count');
expect_same(['id' => '1', 'views' => '10'], $stream->fetch_assoc(), 'streaming first assoc row');
expect_same([1, 2], mysqli_fetch_lengths($stream), 'streaming first row lengths');
expect_same(0, mysqli_num_rows($stream), 'streaming partial row count');
expect_false($stream->data_seek(0), 'streaming data seek unsupported');
expect_same(['id' => '2', 'views' => '20'], $stream->fetch_assoc(), 'streaming second assoc row');
expect_same(null, $stream->fetch_assoc(), 'streaming result exhausted');
expect_same(2, mysqli_num_rows($stream), 'streaming exhausted row count');
expect_same(2, $stream->num_rows, 'streaming exhausted row count property');

$stream_all = $mysqli->query('SELECT id, views FROM posts ORDER BY id', MYSQLI_USE_RESULT);
if (!$stream_all instanceof mysqli_result) {
    throw new RuntimeException('streaming fetch_all result type');
}
expect_same([['1', '10'], ['2', '20']], $stream_all->fetch_all(MYSQLI_NUM), 'streaming fetch all rows');
expect_same(null, $stream_all->fetch_assoc(), 'streaming fetch all exhausted');
expect_same(2, mysqli_num_rows($stream_all), 'streaming fetch all row count');

expect_true($mysqli->real_query('SELECT id, views FROM posts ORDER BY id'), 'real query select');
$real_stream = $mysqli->use_result();
if (!$real_stream instanceof mysqli_result) {
    throw new RuntimeException('real query streaming result type');
}
expect_same(MYSQLI_USE_RESULT, $real_stream->type, 'real query streaming result type constant');
expect_same(0, mysqli_num_rows($real_stream), 'real query streaming initial row count');
expect_same(['id' => '1', 'views' => '10'], $real_stream->fetch_assoc(), 'real query streaming first row');
expect_same([['2', '20']], $real_stream->fetch_all(MYSQLI_NUM), 'real query streaming remainder');
expect_same(2, mysqli_num_rows($real_stream), 'real query streaming exhausted row count');

expect_true($mysqli->real_query('SELECT id, views FROM posts ORDER BY id'), 'real query buffered select');
$real_buffer = $mysqli->store_result();
if (!$real_buffer instanceof mysqli_result) {
    throw new RuntimeException('real query buffered result type');
}
expect_same(MYSQLI_STORE_RESULT, $real_buffer->type, 'real query buffered result type constant');
expect_same(2, $real_buffer->num_rows, 'real query buffered row count');
expect_same([['1', '10'], ['2', '20']], $real_buffer->fetch_all(MYSQLI_NUM), 'real query buffered rows');

expect_true(
    $mysqli->query(
        "CREATE TABLE auto_posts (
            id INT NOT NULL AUTO_INCREMENT PRIMARY KEY,
            title VARCHAR(20)
        )"
    ),
    'create auto table'
);
expect_true($mysqli->query("INSERT INTO auto_posts (title) VALUES ('a')"), 'insert auto row');
expect_same(1, $mysqli->insert_id, 'auto insert id property');
expect_same(1, mysqli_insert_id($mysqli), 'auto insert id function');
expect_true(
    $mysqli->query("INSERT INTO auto_posts (title) VALUES ('b'), ('c')"),
    'insert multi auto row'
);
expect_same(2, $mysqli->insert_id, 'multi auto insert id property');
expect_true(
    $mysqli->query("INSERT INTO auto_posts (id, title) VALUES (10, 'manual')"),
    'insert explicit auto row'
);
expect_same(10, $mysqli->insert_id, 'explicit auto insert id property');
$last_id_result = $mysqli->query('SELECT LAST_INSERT_ID() AS id');
if (!$last_id_result instanceof mysqli_result) {
    throw new RuntimeException('last insert id result type');
}
expect_same(['id' => '2'], $last_id_result->fetch_assoc(), 'explicit auto preserves sql last insert id');

$result->data_seek(0);
$field = $result->fetch_field();
expect_same('id', $field->name, 'field name');
expect_same('posts', $field->table, 'field table');
expect_same('posts', $field->orgtable, 'origin field table');
expect_same('id', $field->orgname, 'origin field name');
expect_same(MYSQLI_TYPE_LONG, $field->type, 'field type');
expect_same(11, $field->length, 'field length');

$empty_result = $mysqli->query('SELECT id FROM posts WHERE id = 0');
if (!$empty_result instanceof mysqli_result) {
    throw new RuntimeException('empty query result type');
}
$empty_field = $empty_result->fetch_field();
expect_same('posts', $empty_field->table, 'empty field table');
expect_same(MYSQLI_TYPE_LONG, $empty_field->type, 'empty field type');
expect_same(11, $empty_field->length, 'empty field length');

$spatialTemporal = $mysqli->query(
    "SELECT ST_AsText(POINT(1,2)) AS spatial_text, " .
    "ST_AsWKB(POINT(1,2)) AS spatial_binary, " .
    "CONVERT_TZ('2024-01-01 00:00:00.123','+00:00','+01:00') AS timezone_value"
);
if (!$spatialTemporal instanceof mysqli_result) {
    throw new RuntimeException('spatial temporal metadata result type');
}
$spatialTemporalFields = $spatialTemporal->fetch_fields();
expect_same(MYSQLI_TYPE_LONG_BLOB, $spatialTemporalFields[0]->type, 'spatial text field type');
expect_same(255, $spatialTemporalFields[0]->charsetnr, 'spatial text field collation');
expect_same(268435456, $spatialTemporalFields[0]->length, 'spatial text field length');
expect_same(31, $spatialTemporalFields[0]->decimals, 'spatial text field decimals');
expect_same(0, $spatialTemporalFields[0]->flags, 'spatial text field flags');
expect_same(MYSQLI_TYPE_LONG_BLOB, $spatialTemporalFields[1]->type, 'spatial binary field type');
expect_same(63, $spatialTemporalFields[1]->charsetnr, 'spatial binary field collation');
expect_same(4294967295, $spatialTemporalFields[1]->length, 'spatial binary field length');
expect_same(31, $spatialTemporalFields[1]->decimals, 'spatial binary field decimals');
expect_same(MYSQLI_BINARY_FLAG, $spatialTemporalFields[1]->flags, 'spatial binary field flags');
expect_same(MYSQLI_TYPE_DATETIME, $spatialTemporalFields[2]->type, 'timezone field type');
expect_same(63, $spatialTemporalFields[2]->charsetnr, 'timezone field collation');
expect_same(23, $spatialTemporalFields[2]->length, 'timezone field length');
expect_same(3, $spatialTemporalFields[2]->decimals, 'timezone field decimals');
expect_same(MYSQLI_BINARY_FLAG, $spatialTemporalFields[2]->flags, 'timezone field flags');

$aggregateMetadata = $mysqli->query(
    'SELECT COUNT(*) AS count_value, SUM(id) AS sum_value, MIN(views) AS min_value FROM posts'
);
if (!$aggregateMetadata instanceof mysqli_result) {
    throw new RuntimeException('aggregate metadata result type');
}
$aggregateFields = $aggregateMetadata->fetch_fields();
expect_same(MYSQLI_TYPE_LONGLONG, $aggregateFields[0]->type, 'count field type');
expect_same(21, $aggregateFields[0]->length, 'count field length');
expect_same(
    MYSQLI_NOT_NULL_FLAG | MYSQLI_BINARY_FLAG | MYSQLI_NUM_FLAG,
    $aggregateFields[0]->flags,
    'count field flags'
);
expect_same(MYSQLI_TYPE_NEWDECIMAL, $aggregateFields[1]->type, 'sum field type');
expect_same(33, $aggregateFields[1]->length, 'sum field length');
expect_same(0, $aggregateFields[1]->decimals, 'sum field decimals');
expect_same(
    MYSQLI_BINARY_FLAG | MYSQLI_NUM_FLAG,
    $aggregateFields[1]->flags,
    'sum field flags'
);
expect_same(MYSQLI_TYPE_LONG, $aggregateFields[2]->type, 'minimum field type');
expect_same(11, $aggregateFields[2]->length, 'minimum field length');
expect_same(
    MYSQLI_BINARY_FLAG | MYSQLI_NUM_FLAG,
    $aggregateFields[2]->flags,
    'minimum field flags'
);

$preparedAggregate = $mysqli->prepare(
    'SELECT COUNT(*) AS count_value, SUM(id) AS sum_value, MIN(views) AS min_value FROM posts'
);
expect_true($preparedAggregate->execute(), 'prepared aggregate metadata execute');
$preparedAggregateMetadata = $preparedAggregate->result_metadata();
if (!$preparedAggregateMetadata instanceof mysqli_result) {
    throw new RuntimeException('prepared aggregate metadata result type');
}
$preparedAggregateFields = $preparedAggregateMetadata->fetch_fields();
foreach ($aggregateFields as $index => $expectedField) {
    expect_same($expectedField->type, $preparedAggregateFields[$index]->type, "prepared aggregate {$index} type");
    expect_same($expectedField->flags, $preparedAggregateFields[$index]->flags, "prepared aggregate {$index} flags");
    expect_same($expectedField->length, $preparedAggregateFields[$index]->length, "prepared aggregate {$index} length");
    expect_same($expectedField->decimals, $preparedAggregateFields[$index]->decimals, "prepared aggregate {$index} decimals");
}
$preparedAggregateMetadata->free();
$preparedAggregate->free_result();
$preparedAggregate->close();

$windowMetadata = $mysqli->query(
    'SELECT ROW_NUMBER() OVER w AS row_number_value, SUM(id) OVER w AS sum_value, ' .
    'LAG(views) OVER w AS lag_value, JSON_ARRAYAGG(id) OVER w AS json_value ' .
    'FROM posts WINDOW w AS (ORDER BY id) LIMIT 1'
);
if (!$windowMetadata instanceof mysqli_result) {
    throw new RuntimeException('window metadata result type');
}
$windowFields = $windowMetadata->fetch_fields();
expect_same(MYSQLI_TYPE_LONGLONG, $windowFields[0]->type, 'row number field type');
expect_same(
    MYSQLI_NOT_NULL_FLAG | MYSQLI_UNSIGNED_FLAG | MYSQLI_NUM_FLAG,
    $windowFields[0]->flags,
    'row number field flags'
);
expect_same(MYSQLI_TYPE_NEWDECIMAL, $windowFields[1]->type, 'window sum field type');
expect_same(33, $windowFields[1]->length, 'window sum field length');
expect_same(MYSQLI_NUM_FLAG, $windowFields[1]->flags, 'window sum field flags');
expect_same(MYSQLI_TYPE_LONGLONG, $windowFields[2]->type, 'window navigation field type');
expect_same(11, $windowFields[2]->length, 'window navigation field length');
expect_same(MYSQLI_NUM_FLAG, $windowFields[2]->flags, 'window navigation field flags');
expect_same(MYSQLI_TYPE_JSON, $windowFields[3]->type, 'window JSON field type');
expect_same(63, $windowFields[3]->charsetnr, 'window JSON field collation');
expect_same(4294967295, $windowFields[3]->length, 'window JSON field length');
expect_same(0, $windowFields[3]->decimals, 'window JSON field decimals');
expect_same(
    MYSQLI_BLOB_FLAG | MYSQLI_BINARY_FLAG,
    $windowFields[3]->flags,
    'window JSON field flags'
);

$mysqli->close();
