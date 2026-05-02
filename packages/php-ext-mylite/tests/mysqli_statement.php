<?php

require __DIR__ . '/bootstrap.php';

$mysqli = open_mylite_mysqli();
expect_true($mysqli->query('CREATE TABLE items (id INT NOT NULL, name VARCHAR(32) NOT NULL, PRIMARY KEY (id))'), 'create table');

$insert = $mysqli->prepare('INSERT INTO items VALUES (?, ?)');
if (!$insert instanceof mysqli_stmt) {
    throw new RuntimeException('insert prepare');
}

$id = 1;
$name = 'Alpha';
expect_true($insert->bind_param('is', $id, $name), 'bind first insert');
expect_true($insert->execute(), 'execute first insert');

$id = 2;
$name = 'Beta';
expect_true($insert->execute(), 'execute second insert');

$select = mysqli_prepare($mysqli, 'SELECT id, name FROM items WHERE id >= ? ORDER BY id');
if (!$select instanceof mysqli_stmt) {
    throw new RuntimeException('select prepare');
}

$min = 1;
expect_same(1, $select->param_count, 'param count');
expect_true(mysqli_stmt_bind_param($select, 'i', $min), 'bind select');
expect_true(mysqli_stmt_execute($select), 'execute select');

$outId = null;
$outName = null;
expect_true(mysqli_stmt_bind_result($select, $outId, $outName), 'bind result');
expect_true(mysqli_stmt_fetch($select), 'fetch first');
expect_same('1', $outId, 'first id');
expect_same('Alpha', $outName, 'first name');

$result = $mysqli->execute_query('SELECT name FROM items WHERE id = ?', [2]);
expect_same(['name' => 'Beta'], $result->fetch_assoc(), 'execute_query result');

$commented = $mysqli->prepare("SELECT ? AS value /* ignored ? marker */ -- ignored ? marker\n");
expect_same(1, $commented->param_count, 'commented marker count');
expect_true($commented->execute(['Gamma']), 'commented execute');
expect_same(['value' => 'Gamma'], $commented->get_result()->fetch_assoc(), 'commented result');

$dashControl = $mysqli->prepare("SELECT ? AS value --\v ignored ? marker\n");
expect_same(1, $dashControl->param_count, 'dash control marker count');
