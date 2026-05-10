<?php

require __DIR__ . '/bootstrap.php';

$path = tempnam(sys_get_temp_dir(), 'mylite_php_ext_');
if ($path === false) {
    throw new RuntimeException('create temporary database path');
}
unlink($path);

$mysqli = mysqli_connect('localhost', '', '', null, 0, $path);
if (!$mysqli instanceof mysqli) {
    throw new RuntimeException('procedural connect');
}

expect_true(mysqli_query($mysqli, 'CREATE DATABASE app'), 'create database');
expect_true(mysqli_select_db($mysqli, 'app'), 'select database');
expect_true(mysqli_query($mysqli, 'CREATE TABLE numbers (n INT NOT NULL, label INT NOT NULL)'), 'create table');
expect_true(mysqli_query($mysqli, 'INSERT INTO numbers VALUES (2, 20), (1, 10)'), 'insert');

$result = mysqli_query($mysqli, 'SELECT n, label FROM numbers ORDER BY n');
expect_same(2, mysqli_num_rows($result), 'num rows');
expect_same(['1', 'n' => '1', '10', 'label' => '10'], mysqli_fetch_array($result, MYSQLI_BOTH), 'both row');
expect_same([['2', '20']], mysqli_fetch_all($result, MYSQLI_NUM), 'fetch all remainder');

mysqli_data_seek($result, 0);
expect_same('1', mysqli_fetch_column($result, 0), 'fetch column');
expect_same(2, mysqli_num_fields($result), 'num fields');

mysqli_free_result($result);
expect_true(mysqli_close($mysqli), 'close');
