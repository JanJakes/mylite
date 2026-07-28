<?php

require __DIR__ . '/bootstrap.php';

$first = open_mylite_mysqli();
$second = open_mylite_mysqli();

$firstSqlId = (int)$first->query('SELECT CONNECTION_ID()')->fetch_row()[0];
$secondSqlId = (int)$second->query('SELECT CONNECTION_ID()')->fetch_row()[0];

expect_true($firstSqlId > 0, 'first SQL connection ID nonzero');
expect_true($secondSqlId > 0, 'second SQL connection ID nonzero');
expect_same($firstSqlId, $first->thread_id, 'first object thread ID');
expect_same($firstSqlId, mysqli_thread_id($first), 'first procedural thread ID');
expect_same($secondSqlId, $second->thread_id, 'second object thread ID');
expect_true($firstSqlId !== $secondSqlId, 'simultaneous connection IDs differ');

expect_same('stable', $first->query("SELECT 'stable'")->fetch_row()[0], 'intervening command');
expect_same($firstSqlId, $first->thread_id, 'first thread ID remains stable');
expect_same($firstSqlId, mysqli_thread_id($first), 'first procedural ID remains stable');

$first->query('SET SESSION pseudo_thread_id = 12345');
expect_same(
    12345,
    (int)$first->query('SELECT CONNECTION_ID()')->fetch_row()[0],
    'pseudo thread SQL ID'
);
expect_same($firstSqlId, $first->thread_id, 'pseudo thread keeps object thread ID');
expect_same($firstSqlId, mysqli_thread_id($first), 'pseudo thread keeps procedural thread ID');

$first->close();
$second->close();
