<?php

require __DIR__ . '/bootstrap.php';

$mysqli = open_mylite_mysqli();

expect_false($mysqli->query('SELECT FROM'), 'parse error query');
expect_same(1064, $mysqli->errno, 'parse errno');
expect_same('42000', $mysqli->sqlstate, 'parse sqlstate');

mysqli_report(MYSQLI_REPORT_ERROR | MYSQLI_REPORT_STRICT);

$parseErrorThrown = false;
try {
    $mysqli->query('SELECT FROM');
} catch (mysqli_sql_exception $exception) {
    expect_same('42000', $exception->getSqlState(), 'exception sqlstate');
    $parseErrorThrown = true;
}
expect_true($parseErrorThrown, 'strict parse report did not throw');

$unsupportedErrorThrown = false;
try {
    $mysqli->options(MYSQLI_OPT_CONNECT_TIMEOUT, 1);
} catch (mysqli_sql_exception $exception) {
    expect_same(1235, $exception->getCode(), 'unsupported exception code');
    expect_same('42000', $exception->getSqlState(), 'unsupported exception sqlstate');
    $unsupportedErrorThrown = true;
}
expect_true($unsupportedErrorThrown, 'strict unsupported report did not throw');
