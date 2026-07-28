#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_prepared_statement_lazy_prepare_expectations_$$"

fail() {
    printf '%s\n' "mysql_prepared_statement_lazy_prepare_expectations: $1" >&2
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

command -v php >/dev/null 2>&1 || fail "php is required for mysqli/PDO expectations"

run_mysql \
    "DROP DATABASE IF EXISTS ${DATABASE};
     CREATE DATABASE ${DATABASE};
     CREATE TABLE ${DATABASE}.items (
         id INT PRIMARY KEY,
         value VARCHAR(20)
     ) ENGINE=InnoDB;
     INSERT INTO ${DATABASE}.items VALUES (1, 'one'), (2, 'two');" >/dev/null

MYLITE_LAZY_PREPARE_MYSQL_HOST=$(
    docker inspect -f '{{range .NetworkSettings.Networks}}{{.IPAddress}}{{end}}' \
        "$MYSQL_CONTAINER"
)
[ -n "$MYLITE_LAZY_PREPARE_MYSQL_HOST" ] || fail "could not resolve MySQL container address"
export MYLITE_LAZY_PREPARE_MYSQL_HOST
export MYLITE_LAZY_PREPARE_DATABASE="$DATABASE"

php <<'PHP'
<?php

mysqli_report(MYSQLI_REPORT_ERROR | MYSQLI_REPORT_STRICT);

function expect_same(mixed $expected, mixed $actual, string $context): void
{
    if ($expected !== $actual) {
        throw new RuntimeException(
            $context . ': expected [' . var_export($expected, true) .
            '], got [' . var_export($actual, true) . ']'
        );
    }
}

function mysqli_transaction_count(mysqli $connection): int
{
    return (int) $connection
        ->query(
            'SELECT COUNT(*) FROM information_schema.innodb_trx ' .
            'WHERE trx_mysql_thread_id = CONNECTION_ID()'
        )
        ->fetch_row()[0];
}

function pdo_transaction_count(PDO $connection): int
{
    return (int) $connection
        ->query(
            'SELECT COUNT(*) FROM information_schema.innodb_trx ' .
            'WHERE trx_mysql_thread_id = CONNECTION_ID()'
        )
        ->fetchColumn();
}

$host = getenv('MYLITE_LAZY_PREPARE_MYSQL_HOST');
$database = getenv('MYLITE_LAZY_PREPARE_DATABASE');
if ($host === false || $database === false) {
    throw new RuntimeException('missing MySQL expectation environment');
}

$mysqli = new mysqli($host, 'root', '', $database);
$mysqliWriter = new mysqli($host, 'root', '', $database);
$mysqliWriter->query('SET SESSION lock_wait_timeout = 2');
$mysqliWriter->query('SET SESSION innodb_lock_wait_timeout = 2');

$constant = $mysqli->prepare('SELECT 7');
expect_same(0, mysqli_transaction_count($mysqli), 'mysqli constant prepare transaction');
$constant->close();

$table = $mysqli->prepare('SELECT id, value FROM items ORDER BY id');
expect_same(0, mysqli_transaction_count($mysqli), 'mysqli table prepare transaction');
$mysqli->query("SET @intervening = 'ok'");
$mysqliWriter->query("UPDATE items SET value = 'changed' WHERE id = 1");
$mysqliWriter->query('ALTER TABLE items ADD COLUMN extra INT NOT NULL DEFAULT 7');
$table->execute();
$table->bind_result($id, $value);
$rows = [];
while ($table->fetch()) {
    $rows[] = $id . ':' . $value;
}
expect_same(['1:changed', '2:two'], $rows, 'mysqli post-prepare rows');
$table->free_result();
$table->close();

$parameterized = $mysqli->prepare('SELECT value FROM items WHERE id = ?');
expect_same(0, mysqli_transaction_count($mysqli), 'mysqli parameterized prepare transaction');
$parameterId = 1;
$parameterized->bind_param('i', $parameterId);
$parameterized->execute();
$parameterized->bind_result($parameterValue);
$parameterized->fetch();
expect_same('changed', $parameterValue, 'mysqli parameterized value');
$parameterized->free_result();
$parameterized->close();

try {
    $mysqli->prepare('SELECT * FROM missing_table');
    throw new RuntimeException('mysqli missing-table prepare unexpectedly succeeded');
} catch (mysqli_sql_exception $exception) {
    expect_same(1146, $exception->getCode(), 'mysqli missing-table error code');
    expect_same('42S02', $exception->getSqlState(), 'mysqli missing-table SQLSTATE');
}

$mysqli->close();
$mysqliWriter->close();

$dsn = 'mysql:host=' . $host . ';dbname=' . $database . ';charset=utf8mb4';
$pdoOptions = [
    PDO::ATTR_ERRMODE => PDO::ERRMODE_EXCEPTION,
    PDO::ATTR_EMULATE_PREPARES => false,
];
$pdo = new PDO($dsn, 'root', '', $pdoOptions);
$pdoWriter = new PDO($dsn, 'root', '', $pdoOptions);
$pdoWriter->exec('SET SESSION lock_wait_timeout = 2');
$pdoWriter->exec('SET SESSION innodb_lock_wait_timeout = 2');

$constant = $pdo->prepare('SELECT 9');
expect_same(0, pdo_transaction_count($pdo), 'PDO constant prepare transaction');
$constant->closeCursor();

$table = $pdo->prepare('SELECT id, value FROM items ORDER BY id');
expect_same(0, pdo_transaction_count($pdo), 'PDO table prepare transaction');
$pdo->exec("SET @intervening = 'ok'");
$pdoWriter->exec("UPDATE items SET value = 'pdo' WHERE id = 2");
$table->execute();
$rows = array_map(
    static fn(array $row): string => $row['id'] . ':' . $row['value'],
    $table->fetchAll(PDO::FETCH_ASSOC)
);
expect_same(['1:changed', '2:pdo'], $rows, 'PDO post-prepare rows');
$table->closeCursor();

$parameterized = $pdo->prepare('SELECT value FROM items WHERE id = ?');
expect_same(0, pdo_transaction_count($pdo), 'PDO parameterized prepare transaction');
$parameterized->execute([2]);
expect_same('pdo', $parameterized->fetchColumn(), 'PDO parameterized value');
$parameterized->closeCursor();

try {
    $pdo->prepare('SELECT * FROM missing_table');
    throw new RuntimeException('PDO missing-table prepare unexpectedly succeeded');
} catch (PDOException $exception) {
    expect_same('42S02', $exception->errorInfo[0], 'PDO missing-table SQLSTATE');
    expect_same(1146, $exception->errorInfo[1], 'PDO missing-table error code');
}

$pdoWriter->exec('CREATE TABLE dropped_after_prepare (id INT PRIMARY KEY)');
$dropped = $pdo->prepare('SELECT id FROM dropped_after_prepare');
$pdoWriter->exec('DROP TABLE dropped_after_prepare');
try {
    $dropped->execute();
    throw new RuntimeException('PDO dropped-table execution unexpectedly succeeded');
} catch (PDOException $exception) {
    expect_same('42S02', $exception->errorInfo[0], 'PDO dropped-table SQLSTATE');
    expect_same(1146, $exception->errorInfo[1], 'PDO dropped-table error code');
}

printf("prepared-statement-lazy-prepare MySQL 8.4.9 mysqli/PDO expectations verified\n");
PHP
