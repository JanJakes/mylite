#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_php_pdo_metadata_observables_$$"

fail() {
    printf '%s\n' "mysql_php_pdo_metadata_connection_observables_expectations: $1" >&2
    exit 1
}

run_mysql() {
    sql=$1
    shift
    printf '%s\n' "$sql" \
        | docker exec -i "$MYSQL_CONTAINER" mysql --protocol=TCP -h127.0.0.1 -uroot \
            --batch --raw --skip-column-names --default-character-set=utf8mb4 "$@"
}

cleanup() {
    run_mysql "DROP DATABASE IF EXISTS ${DATABASE};" >/dev/null 2>&1 || true
}

trap cleanup EXIT HUP INT TERM

version=$(run_mysql 'SELECT VERSION();')
case "$version" in
    8.4.9*) ;;
    *) fail "expected MySQL 8.4.9 runtime, got [$version]" ;;
esac

command -v php >/dev/null 2>&1 || fail "php is required for API expectations"

run_mysql \
    "DROP DATABASE IF EXISTS ${DATABASE};
     CREATE DATABASE ${DATABASE};
     CREATE TABLE ${DATABASE}.metadata_values (
         id INT UNSIGNED NOT NULL AUTO_INCREMENT PRIMARY KEY,
         unique_code VARCHAR(16) NOT NULL UNIQUE,
         nullable_decimal DECIMAL(10,2) NULL,
         body TEXT,
         payload BLOB,
         created_at DATETIME,
         location POINT
     ) ENGINE=InnoDB;
     INSERT INTO ${DATABASE}.metadata_values
         (unique_code, nullable_decimal, body, payload, created_at, location)
     VALUES
         ('one', 12.30, 'hello', UNHEX('610062'), '2026-07-27 12:34:56', POINT(1,2)),
         ('two', NULL, NULL, NULL, NULL, NULL);" >/dev/null

MYLITE_API06_MYSQL_HOST=$(
    docker inspect -f '{{range .NetworkSettings.Networks}}{{.IPAddress}}{{end}}' \
        "$MYSQL_CONTAINER"
)
[ -n "$MYLITE_API06_MYSQL_HOST" ] || fail "could not resolve MySQL container address"
export MYLITE_API06_MYSQL_HOST
export MYLITE_API06_DATABASE="$DATABASE"

php <<'PHP'
<?php

function expect_same(mixed $expected, mixed $actual, string $context): void
{
    if ($expected !== $actual) {
        throw new RuntimeException(
            $context . ': expected [' . var_export($expected, true) .
            '], got [' . var_export($actual, true) . ']'
        );
    }
}

function expected_meta(
    string $nativeType,
    int $pdoType,
    array $flags,
    string $table,
    string $name,
    int $length,
    int $precision
): array {
    return [
        'native_type' => $nativeType,
        'pdo_type' => $pdoType,
        'flags' => $flags,
        'table' => $table,
        'name' => $name,
        'len' => $length,
        'precision' => $precision,
    ];
}

$host = getenv('MYLITE_API06_MYSQL_HOST');
$database = getenv('MYLITE_API06_DATABASE');
if ($host === false || $database === false) {
    throw new RuntimeException('missing MySQL expectation environment');
}

$pdo = new PDO(
    "mysql:host={$host};dbname={$database};charset=utf8mb4",
    'root',
    '',
    [
        PDO::ATTR_EMULATE_PREPARES => false,
        PDO::ATTR_ERRMODE => PDO::ERRMODE_EXCEPTION,
    ]
);

$tableMeta = [
    expected_meta('LONG', PDO::PARAM_INT, ['not_null', 'primary_key'], 'metadata_values', 'id', 10, 0),
    expected_meta('VAR_STRING', PDO::PARAM_STR, ['not_null', 'unique_key'], 'metadata_values', 'unique_code', 64, 0),
    expected_meta('NEWDECIMAL', PDO::PARAM_STR, [], 'metadata_values', 'nullable_decimal', 12, 2),
    expected_meta('BLOB', PDO::PARAM_STR, ['blob'], 'metadata_values', 'body', 262140, 0),
    expected_meta('BLOB', PDO::PARAM_STR, ['blob'], 'metadata_values', 'payload', 65535, 0),
    expected_meta('DATETIME', PDO::PARAM_STR, [], 'metadata_values', 'created_at', 19, 0),
    expected_meta('GEOMETRY', PDO::PARAM_STR, ['blob'], 'metadata_values', 'location', 4294967295, 0),
];

$statement = $pdo->prepare(
    'SELECT id, unique_code, nullable_decimal, body, payload, created_at, location ' .
    'FROM metadata_values ORDER BY id'
);
expect_same(true, $statement->execute(), 'table metadata execute');
expect_same(2, $statement->rowCount(), 'buffered row count before fetch');
foreach ($tableMeta as $index => $expected) {
    expect_same($expected, $statement->getColumnMeta($index), "table metadata {$index}");
}
expect_same(2, count($statement->fetchAll()), 'buffered fetch count');
expect_same(2, $statement->rowCount(), 'buffered row count after fetch');

$empty = $pdo->prepare('SELECT id, unique_code FROM metadata_values WHERE id = 999');
expect_same(true, $empty->execute(), 'empty metadata execute');
expect_same(0, $empty->rowCount(), 'empty buffered row count');
expect_same($tableMeta[0], $empty->getColumnMeta(0), 'empty integer metadata');
expect_same($tableMeta[1], $empty->getColumnMeta(1), 'empty text metadata');

$expressions = $pdo->prepare(
    'SELECT id + 1 AS expression_value, COUNT(*) AS aggregate_value ' .
    'FROM metadata_values GROUP BY id ORDER BY id'
);
expect_same(true, $expressions->execute(), 'expression metadata execute');
expect_same(2, $expressions->rowCount(), 'expression buffered row count');
expect_same(
    expected_meta('LONGLONG', PDO::PARAM_INT, ['not_null'], '', 'expression_value', 11, 0),
    $expressions->getColumnMeta(0),
    'expression metadata'
);
expect_same(
    expected_meta('LONGLONG', PDO::PARAM_INT, ['not_null'], '', 'aggregate_value', 21, 0),
    $expressions->getColumnMeta(1),
    'aggregate metadata'
);

$first = new mysqli($host, 'root', '', $database);
$second = new mysqli($host, 'root', '', $database);
$firstSqlId = (int)$first->query('SELECT CONNECTION_ID()')->fetch_row()[0];
$secondSqlId = (int)$second->query('SELECT CONNECTION_ID()')->fetch_row()[0];
expect_same($firstSqlId, $first->thread_id, 'first object thread id');
expect_same($firstSqlId, mysqli_thread_id($first), 'first procedural thread id');
expect_same($secondSqlId, $second->thread_id, 'second object thread id');
expect_same(true, $firstSqlId > 0, 'first thread id nonzero');
expect_same(true, $secondSqlId > 0, 'second thread id nonzero');
expect_same(true, $firstSqlId !== $secondSqlId, 'connection thread ids distinct');
expect_same($firstSqlId, $first->thread_id, 'first thread id stable');
$first->close();
$second->close();

echo "mysql_php_pdo_metadata_connection_observables_expectations: ok\n";
PHP
