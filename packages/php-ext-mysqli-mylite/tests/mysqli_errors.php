<?php

require __DIR__ . '/bootstrap.php';

$mysqli = open_mylite_mysqli();

expect_false($mysqli->query('SELECT FROM'), 'parse error query');
expect_same(1064, $mysqli->errno, 'parse errno');
expect_same('42000', $mysqli->sqlstate, 'parse sqlstate');

mysqli_report(MYSQLI_REPORT_ERROR | MYSQLI_REPORT_STRICT);

try {
    $mysqli->query('SELECT FROM');
} catch (mysqli_sql_exception $exception) {
    expect_same('42000', $exception->getSqlState(), 'exception sqlstate');
    exit(0);
}

throw new RuntimeException('strict report did not throw');
