#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"

fail() {
    printf '%s\n' "mysql_baseline_client_found_rows_expectations: $1" >&2
    exit 1
}

command -v php >/dev/null 2>&1 || fail "php is required"

mysql_host=$(
    docker inspect -f '{{range .NetworkSettings.Networks}}{{.IPAddress}}{{end}}' \
        "$MYSQL_CONTAINER"
)
[ -n "$mysql_host" ] || fail "could not resolve MySQL container address"

MYLITE_MYSQL_EXPECTATION_HOST="$mysql_host" php <<'PHP'
<?php

mysqli_report(MYSQLI_REPORT_ERROR | MYSQLI_REPORT_STRICT);

$host = getenv('MYLITE_MYSQL_EXPECTATION_HOST');
$schema = 'mylite_client_found_rows_expectation';
$admin = new mysqli($host, 'root', '');
$admin->query("DROP DATABASE IF EXISTS {$schema}");
$admin->query("CREATE DATABASE {$schema}");
$admin->select_db($schema);
$admin->query('CREATE TABLE values_table (id BIGINT PRIMARY KEY, value BIGINT NOT NULL)');
$admin->query('INSERT INTO values_table VALUES (1, 10), (2, 10)');

try {
    foreach ([0 => 0, MYSQLI_CLIENT_FOUND_ROWS => 2] as $flags => $expected) {
        $connection = mysqli_init();
        $connection->real_connect($host, 'root', '', $schema, 3306, null, $flags);

        $connection->query('UPDATE values_table SET value = 10 WHERE id IN (1, 2)');
        if ($connection->affected_rows !== $expected) {
            throw new RuntimeException(
                "direct affected rows for flags {$flags}: expected {$expected}, " .
                "got {$connection->affected_rows}"
            );
        }
        $rowCount = $connection->query('SELECT ROW_COUNT()')->fetch_row()[0];
        if ($rowCount !== (string) $expected) {
            throw new RuntimeException(
                "ROW_COUNT() for flags {$flags}: expected {$expected}, got {$rowCount}"
            );
        }

        $statement = $connection->prepare(
            'UPDATE values_table SET value = 10 WHERE id IN (?, ?)'
        );
        $first = 1;
        $second = 2;
        $statement->bind_param('ii', $first, $second);
        $statement->execute();
        if ($statement->affected_rows !== $expected) {
            throw new RuntimeException(
                "statement affected rows for flags {$flags}: expected {$expected}, " .
                "got {$statement->affected_rows}"
            );
        }
        if ($connection->affected_rows !== $expected) {
            throw new RuntimeException(
                "connection affected rows for flags {$flags}: expected {$expected}, " .
                "got {$connection->affected_rows}"
            );
        }
        $rowCount = $connection->query('SELECT ROW_COUNT()')->fetch_row()[0];
        if ($rowCount !== (string) $expected) {
            throw new RuntimeException(
                "prepared ROW_COUNT() for flags {$flags}: expected {$expected}, " .
                "got {$rowCount}"
            );
        }

        $statement->close();
        $connection->close();
    }
} finally {
    $admin->query("DROP DATABASE IF EXISTS {$schema}");
    $admin->close();
}

printf("mysql_baseline_client_found_rows_expectations: ok\n");
PHP
