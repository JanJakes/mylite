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

$host_path = tempnam(sys_get_temp_dir(), 'mylite_php_ext_host_');
if ($host_path === false) {
    throw new RuntimeException('create host-path temporary database path');
}
unlink($host_path);

$host_link = mysqli_connect($host_path);
if (!$host_link instanceof mysqli) {
    throw new RuntimeException('host path connect');
}
expect_true(mysqli_query($host_link, 'CREATE DATABASE IF NOT EXISTS host_app'), 'host path create database');
expect_true(mysqli_select_db($host_link, 'host_app'), 'host path select database');
expect_true(mysqli_query($host_link, 'CREATE TABLE host_values (id INT NOT NULL)'), 'host path create table');
expect_true(mysqli_close($host_link), 'host path close');

$localhost_path = tempnam(sys_get_temp_dir(), 'mylite_php_ext_localhost_');
if ($localhost_path === false) {
    throw new RuntimeException('create localhost-path temporary database path');
}
unlink($localhost_path);

$localhost_link = mysqli_connect('localhost:' . $localhost_path);
if (!$localhost_link instanceof mysqli) {
    throw new RuntimeException('localhost path connect');
}
expect_true(
    mysqli_query($localhost_link, 'CREATE DATABASE IF NOT EXISTS wordpress_tests'),
    'localhost path create database'
);
expect_true(mysqli_select_db($localhost_link, 'wordpress_tests'), 'localhost path select database');
expect_true(
    mysqli_query($localhost_link, 'CREATE TABLE wordpress_values (id INT NOT NULL)'),
    'localhost path create table'
);
expect_true(mysqli_query($localhost_link, 'SET autocommit = 0;'), 'wordpress autocommit off shim');
expect_same(0, mysqli_affected_rows($localhost_link), 'wordpress autocommit affected rows');
expect_true(mysqli_query($localhost_link, 'START TRANSACTION;'), 'wordpress explicit transaction start');
expect_true(mysqli_query($localhost_link, 'ROLLBACK;'), 'wordpress explicit transaction rollback');
expect_true(mysqli_close($localhost_link), 'localhost path close');
