<?php

require __DIR__ . '/bootstrap.php';

$mysqli = open_mylite_mysqli();

expect_false($mysqli->query('SELECT FROM'), 'parse error query');
expect_same(1064, $mysqli->errno, 'parse errno');
expect_same('42000', $mysqli->sqlstate, 'parse sqlstate');
expect_false($mysqli->real_query('SELECT FROM'), 'real_query parse error');
expect_same(1064, $mysqli->errno, 'real_query parse errno');
expect_same('42000', $mysqli->sqlstate, 'real_query parse sqlstate');

$escapePayload = "A\0\n\r\\'\"\x1aB";
$expectedEscape = 'A' . '\\0' . '\\n' . '\\r' . '\\\\' . "\\'" . '\\"' . '\\Z' . 'B';
expect_same($expectedEscape, mysqli_real_escape_string($mysqli, $escapePayload), 'normal escape');
expect_true($mysqli->query("SET SESSION sql_mode = 'NO_BACKSLASH_ESCAPES'"), 'set no-backslash mode');
expect_false(mysqli_real_escape_string($mysqli, "x' OR 1=1 -- "), 'procedural insecure escape');
expect_same(2061, $mysqli->errno, 'procedural insecure escape errno');
expect_same('HY000', $mysqli->sqlstate, 'procedural insecure escape sqlstate');
expect_false(mysqli_escape_string($mysqli, "x' OR 1=1 -- "), 'procedural insecure escape alias');
expect_false($mysqli->real_escape_string("x' OR 1=1 -- "), 'object insecure escape');
expect_false($mysqli->escape_string("x' OR 1=1 -- "), 'object insecure escape alias');
expect_true(str_contains($mysqli->error, 'NO_BACKSLASH_ESCAPES'), 'insecure escape message');
expect_true($mysqli->query("SET SESSION sql_mode = ''"), 'reset no-backslash mode');

mysqli_report(MYSQLI_REPORT_ERROR | MYSQLI_REPORT_STRICT);

$parseErrorThrown = false;
try {
    $mysqli->query('SELECT FROM');
} catch (mysqli_sql_exception $exception) {
    expect_same('42000', $exception->getSqlState(), 'exception sqlstate');
    $parseErrorThrown = true;
}
expect_true($parseErrorThrown, 'strict parse report did not throw');

$realQueryErrorThrown = false;
try {
    $mysqli->real_query('SELECT FROM');
} catch (mysqli_sql_exception $exception) {
    expect_same(1064, $exception->getCode(), 'strict real_query parse code');
    expect_same('42000', $exception->getSqlState(), 'strict real_query parse sqlstate');
    $realQueryErrorThrown = true;
}
expect_true($realQueryErrorThrown, 'strict real_query parse report did not throw');

expect_true($mysqli->query("SET SESSION sql_mode = 'NO_BACKSLASH_ESCAPES'"), 'strict set no-backslash mode');
$insecureEscapeThrown = false;
try {
    $mysqli->real_escape_string("x' OR 1=1 -- ");
} catch (mysqli_sql_exception $exception) {
    expect_same(2061, $exception->getCode(), 'strict insecure escape code');
    expect_same('HY000', $exception->getSqlState(), 'strict insecure escape sqlstate');
    $insecureEscapeThrown = true;
}
expect_true($insecureEscapeThrown, 'strict insecure escape did not throw');
expect_true($mysqli->query("SET SESSION sql_mode = ''"), 'strict reset no-backslash mode');

$unsupportedErrorThrown = false;
try {
    $mysqli->options(MYSQLI_OPT_CONNECT_TIMEOUT, 1);
} catch (mysqli_sql_exception $exception) {
    expect_same(1235, $exception->getCode(), 'unsupported exception code');
    expect_same('42000', $exception->getSqlState(), 'unsupported exception sqlstate');
    $unsupportedErrorThrown = true;
}
expect_true($unsupportedErrorThrown, 'strict unsupported report did not throw');
