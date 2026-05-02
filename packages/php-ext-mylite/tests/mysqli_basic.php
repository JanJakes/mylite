<?php

require __DIR__ . '/bootstrap.php';

$mysqli = open_mylite_mysqli();

expect_true(
    $mysqli->query(
        "CREATE TABLE posts (
            id BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
            title VARCHAR(64) NOT NULL,
            views INT NOT NULL DEFAULT 0,
            PRIMARY KEY (id)
        )"
    ),
    'create table'
);
expect_true($mysqli->query("INSERT INTO posts (title, views) VALUES ('Alpha', 10), ('Beta', 20)"), 'insert rows');
expect_same(2, $mysqli->affected_rows, 'affected rows');
expect_same(1, $mysqli->insert_id, 'insert id');

$result = $mysqli->query('SELECT id, title, views FROM posts ORDER BY id');
if (!$result instanceof mysqli_result) {
    throw new RuntimeException('query result type');
}

expect_same(3, $result->field_count, 'field count');
expect_same(2, $result->num_rows, 'row count');
expect_same(['id' => '1', 'title' => 'Alpha', 'views' => '10'], $result->fetch_assoc(), 'first assoc row');
expect_same(['2', 'Beta', '20'], $result->fetch_row(), 'second numeric row');
expect_same(null, $result->fetch_assoc(), 'result exhausted');

$result->data_seek(0);
$field = $result->fetch_field();
expect_same('id', $field->name, 'field name');
expect_same('posts', $field->table, 'field table');
expect_same(MYSQLI_TYPE_LONGLONG, $field->type, 'field type');

$mysqli->close();
