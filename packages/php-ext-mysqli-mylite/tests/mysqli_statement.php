<?php

require __DIR__ . '/bootstrap.php';

$mysqli = open_mylite_mysqli();
expect_true($mysqli->query('CREATE TABLE items (id INT NOT NULL, value INT NOT NULL)'), 'create table');

$insert = $mysqli->prepare('INSERT INTO items VALUES (?, ?)');
if (!$insert instanceof mysqli_stmt) {
    throw new RuntimeException('insert prepare');
}

$id = 1;
$value = 100;
expect_true($insert->bind_param('ii', $id, $value), 'bind first insert');
expect_true($insert->execute(), 'execute first insert');

$id = 2;
$value = 200;
expect_true($insert->execute(), 'execute second insert');

$select = mysqli_prepare($mysqli, 'SELECT id, value FROM items WHERE id >= ? ORDER BY id');
if (!$select instanceof mysqli_stmt) {
    throw new RuntimeException('select prepare');
}

$min = 1;
expect_same(1, $select->param_count, 'param count');
expect_true(mysqli_stmt_bind_param($select, 'i', $min), 'bind select');
expect_true(mysqli_stmt_execute($select), 'execute select');

$outId = null;
$outValue = null;
expect_true(mysqli_stmt_bind_result($select, $outId, $outValue), 'bind result');
expect_true(mysqli_stmt_fetch($select), 'fetch first');
expect_same('1', $outId, 'first id');
expect_same('100', $outValue, 'first value');

$min = 2;
expect_true(mysqli_stmt_execute($select), 'execute select again');
expect_same(
    ['id' => '2', 'value' => '200'],
    mysqli_stmt_get_result($select)->fetch_assoc(),
    'second execution result'
);

$result = $mysqli->execute_query('SELECT value FROM items WHERE id = ?', [2]);
expect_same(['value' => '200'], $result->fetch_assoc(), 'execute_query result');

$commented = $mysqli->prepare("SELECT ? AS value /* ignored ? marker */ -- ignored ? marker\n");
expect_same(1, $commented->param_count, 'commented marker count');
expect_true($commented->execute([300]), 'commented execute');
expect_same(['value' => '300'], $commented->get_result()->fetch_assoc(), 'commented result');

$dashControl = $mysqli->prepare("SELECT ? AS value --\v ignored ? marker\n");
expect_same(1, $dashControl->param_count, 'dash control marker count');

$tooFew = $mysqli->prepare('SELECT ? AS first_value, ? AS second_value');
expect_same(2, $tooFew->param_count, 'too few marker count');
expect_false($tooFew->execute([1]), 'too few execute params');
expect_false(
    $mysqli->execute_query('SELECT ? AS first_value, ? AS second_value', [1]),
    'too few execute_query params'
);
