#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_php_native_scalars_$$"

fail() {
    printf '%s\n' "mysql_php_native_scalar_conversion_server_identity_expectations: $1" >&2
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
     CREATE TABLE ${DATABASE}.typed_values (
         signed_value BIGINT,
         unsigned_value BIGINT UNSIGNED,
         tiny_value TINYINT,
         unsigned_int INT UNSIGNED,
         bit_value BIT(8),
         float_value FLOAT,
         double_value DOUBLE,
         decimal_value DECIMAL(20,4),
         binary_value VARBINARY(8),
         text_value VARCHAR(8),
         null_value INT NULL
     ) ENGINE=InnoDB;
     INSERT INTO ${DATABASE}.typed_values VALUES (
         -9223372036854775808,
         9223372036854775807,
         -128,
         4294967295,
         b'10100101',
         1.25,
         -2.5,
         1234567890123456.2500,
         UNHEX('610062'),
         'text',
         NULL
     );" >/dev/null

MYLITE_SCALAR_MYSQL_HOST=$(
    docker inspect -f '{{range .NetworkSettings.Networks}}{{.IPAddress}}{{end}}' \
        "$MYSQL_CONTAINER"
)
[ -n "$MYLITE_SCALAR_MYSQL_HOST" ] || fail "could not resolve MySQL container address"
export MYLITE_SCALAR_MYSQL_HOST
export MYLITE_SCALAR_DATABASE="$DATABASE"

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

$host = getenv('MYLITE_SCALAR_MYSQL_HOST');
$database = getenv('MYLITE_SCALAR_DATABASE');
if ($host === false || $database === false) {
    throw new RuntimeException('missing MySQL expectation environment');
}

$native = [
    'signed_value' => PHP_INT_MIN,
    'unsigned_value' => PHP_INT_MAX,
    'tiny_value' => -128,
    'unsigned_int' => 4294967295,
    'bit_value' => 165,
    'float_value' => 1.25,
    'double_value' => -2.5,
    'decimal_value' => '1234567890123456.2500',
    'binary_value' => "a\0b",
    'text_value' => 'text',
    'null_value' => null,
];
$strings = [
    'signed_value' => '-9223372036854775808',
    'unsigned_value' => '9223372036854775807',
    'tiny_value' => '-128',
    'unsigned_int' => '4294967295',
    'bit_value' => '165',
    'float_value' => '1.25',
    'double_value' => '-2.5',
    'decimal_value' => '1234567890123456.2500',
    'binary_value' => "a\0b",
    'text_value' => 'text',
    'null_value' => null,
];

mysqli_report(MYSQLI_REPORT_OFF);
$mysqli = new mysqli($host, 'root', '', $database);

$direct = $mysqli->query('SELECT * FROM typed_values');
expect_same($strings, $direct->fetch_assoc(), 'mysqli direct scalar policy');
$direct->free();

$prepared = $mysqli->prepare('SELECT * FROM typed_values');
expect_same(true, $prepared->execute(), 'mysqli prepared execute');
$preparedResult = $prepared->get_result();
expect_same($native, $preparedResult->fetch_assoc(), 'mysqli prepared get_result');
$preparedResult->free();
$prepared->close();

$prepared = $mysqli->prepare('SELECT * FROM typed_values');
expect_same(true, $prepared->execute(), 'mysqli bound execute');
$values = array_fill(0, count($native), null);
expect_same(true, $prepared->bind_result(...$values), 'mysqli bind_result');
expect_same(true, $prepared->fetch(), 'mysqli bound fetch');
expect_same(array_values($native), $values, 'mysqli bound native values');
$prepared->close();

$overflow = $mysqli->prepare('SELECT LAST_INSERT_ID(-1) AS overflow_value');
expect_same(true, $overflow->execute(), 'mysqli overflow execute');
expect_same(
    ['overflow_value' => '18446744073709551615'],
    $overflow->get_result()->fetch_assoc(),
    'mysqli overflowing unsigned value'
);
$overflow->close();
$mysqli->close();

foreach ([false, true] as $stringify) {
    $pdo = new PDO(
        "mysql:host={$host};dbname={$database};charset=utf8mb4",
        'root',
        '',
        [
            PDO::ATTR_EMULATE_PREPARES => false,
            PDO::ATTR_STRINGIFY_FETCHES => $stringify,
        ]
    );
    $statement = $pdo->prepare('SELECT * FROM typed_values');
    expect_same(true, $statement->execute(), 'PDO typed execute');
    expect_same(
        $stringify ? $strings : $native,
        $statement->fetch(PDO::FETCH_ASSOC),
        'PDO stringify policy'
    );
    $overflow = $pdo->prepare('SELECT LAST_INSERT_ID(-1) AS overflow_value');
    expect_same(true, $overflow->execute(), 'PDO overflow execute');
    expect_same('18446744073709551615', $overflow->fetchColumn(), 'PDO unsigned overflow');
    expect_same(
        $pdo->query('SELECT VERSION()')->fetchColumn(),
        $pdo->getAttribute(PDO::ATTR_SERVER_VERSION),
        'PDO server identity'
    );
    expect_same(true, $pdo->getAttribute(PDO::ATTR_CLIENT_VERSION) !== '', 'PDO client identity');
}

echo "mysql_php_native_scalar_conversion_server_identity_expectations: ok\n";
PHP
