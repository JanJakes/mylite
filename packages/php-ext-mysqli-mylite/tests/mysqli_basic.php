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

$mysqli->close();
