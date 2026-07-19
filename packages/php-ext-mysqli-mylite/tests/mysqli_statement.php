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

expect_true(
    $mysqli->query('CREATE TABLE generated_items (id INT AUTO_INCREMENT PRIMARY KEY, name VARCHAR(20))'),
    'create generated table'
);
$generatedInsert = $mysqli->prepare('INSERT INTO generated_items (name) VALUES (?)');
expect_true($generatedInsert->execute(['first']), 'execute generated insert');
expect_same(1, $mysqli->insert_id, 'prepared insert link property insert id');
expect_same(1, mysqli_insert_id($mysqli), 'prepared insert link function insert id');
expect_same(1, $generatedInsert->insert_id, 'prepared insert statement property insert id');
expect_same(1, mysqli_stmt_insert_id($generatedInsert), 'prepared insert statement function insert id');

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

$update = $mysqli->prepare('UPDATE items SET value = ? WHERE id >= ?');
$updatedValue = 100;
$minimumId = 1;
expect_true($update->bind_param('ii', $updatedValue, $minimumId), 'bind prepared update');
expect_true($update->execute(), 'execute prepared update');
expect_same(1, $update->affected_rows, 'prepared update changed rows');
expect_same(1, $mysqli->affected_rows, 'prepared update link changed rows');
expect_same(
    'Rows matched: 2  Changed: 1  Warnings: 0',
    $mysqli->info,
    'prepared update matched-row info'
);

expect_true($mysqli->query('SET SESSION sql_mode = \'NO_BACKSLASH_ESCAPES\''), 'set SQL mode');
$hostile = "Grace\\'); DROP TABLE items; --";
$hostileStatement = $mysqli->prepare('SELECT ? AS value');
expect_true($hostileStatement->execute([$hostile]), 'execute hostile statement');
expect_same(
    ['value' => $hostile],
    $hostileStatement->get_result()->fetch_assoc(),
    'hostile statement value'
);
expect_same(
    ['value' => $hostile],
    $mysqli->execute_query('SELECT ? AS value', [$hostile])->fetch_assoc(),
    'hostile execute_query value'
);
expect_same(
    ['row_count' => '2'],
    $mysqli->query('SELECT COUNT(*) AS row_count FROM items')->fetch_assoc(),
    'bound text changed the database'
);

$typed = $mysqli->prepare('SELECT ? AS null_value, ? AS empty_value, ? AS binary_value');
expect_true($typed->execute([null, '', "a\0b"]), 'execute typed values');
expect_same(
    ['null_value' => null, 'empty_value' => '', 'binary_value' => "a\0b"],
    $typed->get_result()->fetch_assoc(),
    'typed values'
);

expect_true($mysqli->query('CREATE TABLE payloads (id INT PRIMARY KEY, data BLOB)'), 'create blobs');
$blobStatement = $mysqli->prepare('INSERT INTO payloads VALUES (?, ?)');
$blobId = 1;
$blobValue = null;
expect_true($blobStatement->bind_param('ib', $blobId, $blobValue), 'bind blob');
expect_true($blobStatement->send_long_data(1, "a\0"), 'send first blob chunk');
expect_true($blobStatement->send_long_data(1, 'b'), 'send second blob chunk');
expect_true($blobStatement->execute(), 'execute blob insert');
expect_same(
    ['data' => "a\0b"],
    $mysqli->query('SELECT data FROM payloads WHERE id = 1')->fetch_assoc(),
    'blob chunks'
);

$commented = $mysqli->prepare("SELECT ? AS value /* ignored ? marker */ -- ignored ? marker\n");
expect_same(1, $commented->param_count, 'commented marker count');
expect_true($commented->execute([300]), 'commented execute');
expect_same(['value' => '300'], $commented->get_result()->fetch_assoc(), 'commented result');

$dashControl = $mysqli->prepare("SELECT ? AS value --\v ignored ? marker\n");
expect_same(1, $dashControl->param_count, 'dash control marker count');

$tooFew = $mysqli->prepare('SELECT ? AS a_value, ? AS b_value');
expect_same(2, $tooFew->param_count, 'too few marker count');
expect_false($tooFew->execute([1]), 'too few execute params');
expect_false(
    $mysqli->execute_query('SELECT ? AS a_value, ? AS b_value', [1]),
    'too few execute_query params'
);

$oversized = $mysqli->prepare('SELECT ? AS value');
$oversizedPayload = str_repeat('x', 64 * 1024 * 1024 + 1);
expect_false($oversized->execute([$oversizedPayload]), 'reject oversized prepared payload');
expect_same(1153, $oversized->errno, 'oversized prepared payload statement errno');
expect_same(1153, $mysqli->errno, 'oversized prepared payload link errno');
unset($oversizedPayload);

$reset = $mysqli->prepare('SELECT ? AS value');
expect_true($reset->execute([1]), 'execute reset statement');
expect_true($reset->reset(), 'reset statement');
expect_true($reset->execute([2]), 'execute reset statement again');
expect_same(['value' => '2'], $reset->get_result()->fetch_assoc(), 'reset result');
expect_true($reset->close(), 'close statement');
expect_false($reset->execute([3]), 'execute closed statement');

expect_false($mysqli->prepare('SELECT * FROM missing_table'), 'prepare missing table');

$closeFirstLink = open_mylite_mysqli();
$closeFirstStatement = $closeFirstLink->prepare('SELECT ? AS value');
expect_true($closeFirstLink->close(), 'close link before statement');
expect_false($closeFirstStatement->execute([1]), 'execute statement after link close');
expect_true($closeFirstStatement->close(), 'close statement after link close');

$statementFirstLink = open_mylite_mysqli();
$statementFirst = $statementFirstLink->prepare('SELECT 1 AS value');
expect_true($statementFirst->close(), 'close statement before link');
expect_same(
    ['value' => '2'],
    $statementFirstLink->query('SELECT 2 AS value')->fetch_assoc(),
    'query after statement-first close'
);
expect_true($statementFirstLink->close(), 'close link after statement');
