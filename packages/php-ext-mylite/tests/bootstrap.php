<?php

if (phpversion('mysqli') !== '0.1.0') {
    fwrite(STDERR, "Skipping php-ext-mylite mysqli tests: PHP already provides mysqli.\n");
    exit(77);
}

mysqli_report(MYSQLI_REPORT_OFF);

function expect_true(mixed $value, string $context): void
{
    if ($value !== true) {
        throw new RuntimeException($context . ': expected true');
    }
}

function expect_false(mixed $value, string $context): void
{
    if ($value !== false) {
        throw new RuntimeException($context . ': expected false');
    }
}

function expect_same(mixed $expected, mixed $actual, string $context): void
{
    if ($actual !== $expected) {
        throw new RuntimeException(
            $context . ': expected ' . var_export($expected, true) . ', got ' . var_export($actual, true)
        );
    }
}

function open_mylite_mysqli(): mysqli
{
    $path = tempnam(sys_get_temp_dir(), 'mylite_php_ext_');
    if ($path === false) {
        throw new RuntimeException('create temporary database path');
    }
    unlink($path);

    $mysqli = new mysqli('localhost', '', '', null, 0, $path);
    expect_same(0, $mysqli->connect_errno, 'connect errno');
    expect_true($mysqli->query('CREATE DATABASE app'), 'create database');
    expect_true($mysqli->select_db('app'), 'select database');

    return $mysqli;
}
