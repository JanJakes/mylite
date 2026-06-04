<?php

require __DIR__ . '/bootstrap.php';

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

$result = $mysqli->query('SELECT id, views FROM posts ORDER BY id');
if (!$result instanceof mysqli_result) {
    throw new RuntimeException('query result type');
}

expect_same(2, $result->field_count, 'field count');
expect_same(2, $result->num_rows, 'row count');
expect_same(['id' => '1', 'views' => '10'], $result->fetch_assoc(), 'first assoc row');
expect_same(['2', '20'], $result->fetch_row(), 'second numeric row');
expect_same(null, $result->fetch_assoc(), 'result exhausted');

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
