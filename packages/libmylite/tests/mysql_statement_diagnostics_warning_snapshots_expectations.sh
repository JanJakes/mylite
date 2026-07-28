#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_statement_diagnostics_warning_snapshots_$$"

fail() {
    printf '%s\n' "mysql_statement_diagnostics_warning_snapshots_expectations: $1" >&2
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
     CREATE TABLE ${DATABASE}.unique_rows (
         id INT PRIMARY KEY
     ) ENGINE=InnoDB;
     CREATE TABLE ${DATABASE}.required_rows (
         id INT PRIMARY KEY,
         required_value INT NOT NULL
     ) ENGINE=InnoDB;
     CREATE TABLE ${DATABASE}.warning_rows (
         tiny_value TINYINT,
         short_text VARCHAR(2)
     ) ENGINE=InnoDB;" >/dev/null

MYLITE_DIAGNOSTIC_MYSQL_HOST=$(
    docker inspect -f '{{range .NetworkSettings.Networks}}{{.IPAddress}}{{end}}' \
        "$MYSQL_CONTAINER"
)
[ -n "$MYLITE_DIAGNOSTIC_MYSQL_HOST" ] || fail "could not resolve MySQL container address"
export MYLITE_DIAGNOSTIC_MYSQL_HOST
export MYLITE_DIAGNOSTIC_DATABASE="$DATABASE"

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

function mysql_expectation_environment(): array
{
    $host = getenv('MYLITE_DIAGNOSTIC_MYSQL_HOST');
    $database = getenv('MYLITE_DIAGNOSTIC_DATABASE');
    if ($host === false || $database === false) {
        throw new RuntimeException('missing MySQL expectation environment');
    }
    return [$host, $database];
}

function collect_warnings(mysqli|mysqli_stmt $owner): array
{
    $warning = $owner->get_warnings();
    if ($warning === false) {
        return [];
    }
    $records = [];
    do {
        $records[] = [$warning->errno, $warning->sqlstate, $warning->message];
    } while ($warning->next());
    return $records;
}

[$host, $database] = mysql_expectation_environment();

$pdo = new PDO(
    "mysql:host={$host};dbname={$database};charset=utf8mb4",
    'root',
    '',
    [
        PDO::ATTR_ERRMODE => PDO::ERRMODE_SILENT,
        PDO::ATTR_EMULATE_PREPARES => false,
    ]
);
$duplicate = $pdo->prepare('INSERT INTO unique_rows(id) VALUES (?)');
$required = $pdo->prepare('INSERT INTO required_rows(id, required_value) VALUES (?, ?)');
$success = $pdo->prepare('SELECT ?');

expect_same(true, $duplicate->execute([1]), 'PDO seed execute');
expect_same(false, $duplicate->execute([1]), 'PDO duplicate execute');
expect_same('23000', $duplicate->errorInfo()[0], 'PDO duplicate SQLSTATE');
expect_same(1062, $duplicate->errorInfo()[1], 'PDO duplicate code');
expect_same(true, str_contains($duplicate->errorInfo()[2], 'unique_rows.PRIMARY'), 'PDO duplicate message');
expect_same(['00000', null, null], $pdo->errorInfo(), 'PDO handle after statement error');

expect_same(false, $required->execute([1, null]), 'PDO required execute');
expect_same('23000', $required->errorInfo()[0], 'PDO required SQLSTATE');
expect_same(1048, $required->errorInfo()[1], 'PDO required code');
expect_same(
    true,
    str_contains($required->errorInfo()[2], "Column 'required_value' cannot be null"),
    'PDO required message'
);
expect_same(1062, $duplicate->errorInfo()[1], 'PDO duplicate code retained');
expect_same(
    true,
    str_contains($duplicate->errorInfo()[2], 'unique_rows.PRIMARY'),
    'PDO duplicate message retained'
);

expect_same(true, $success->execute([7]), 'PDO intervening success');
expect_same(['00000', null, null], $success->errorInfo(), 'PDO successful statement info');
expect_same(1048, $required->errorInfo()[1], 'PDO required code after success');
expect_same(1062, $duplicate->errorInfo()[1], 'PDO duplicate code after success');
expect_same(['00000', null, null], $pdo->errorInfo(), 'PDO handle remains independent');

expect_same(true, $duplicate->execute([2]), 'PDO successful re-execution');
expect_same(['00000', null, null], $duplicate->errorInfo(), 'PDO re-execution clears statement');
expect_same(1048, $required->errorInfo()[1], 'PDO other statement still retained');

mysqli_report(MYSQLI_REPORT_OFF);
$mysqli = new mysqli($host, 'root', '', $database);

$direct = $mysqli->query("SELECT 1/0, CAST('NULL' AS UNSIGNED)");
expect_same(true, $direct instanceof mysqli_result, 'mysqli direct warning result');
expect_same(2, $mysqli->warning_count, 'mysqli direct warning count');
$directWarning = $mysqli->get_warnings();
expect_same(true, $directWarning instanceof mysqli_warning, 'mysqli direct warning object');
expect_same([1365, 'HY000', 'Division by 0'], [
    $directWarning->errno,
    $directWarning->sqlstate,
    $directWarning->message,
], 'mysqli first direct warning');
expect_same(true, $directWarning->next(), 'mysqli direct warning next');
expect_same([1292, 'HY000', "Truncated incorrect INTEGER value: 'NULL'"], [
    $directWarning->errno,
    $directWarning->sqlstate,
    $directWarning->message,
], 'mysqli second direct warning');
expect_same(false, $directWarning->next(), 'mysqli direct warning end');
$direct->free();

expect_same(true, $mysqli->query('SELECT 1') instanceof mysqli_result, 'mysqli warning-free query');
expect_same(0, $mysqli->warning_count, 'mysqli warning-free count');
expect_same(false, $mysqli->get_warnings(), 'mysqli warning-free list');

expect_same(true, $mysqli->query("SET SESSION sql_mode = ''"), 'mysqli non-strict mode');
$prepared = $mysqli->prepare(
    'INSERT INTO warning_rows(tiny_value, short_text) VALUES (?, ?), (?, ?)'
);
expect_same(true, $prepared instanceof mysqli_stmt, 'mysqli warning prepare');
$tiny1 = 999;
$text1 = 'long';
$tiny2 = -999;
$text2 = 'wide';
expect_same(
    true,
    $prepared->bind_param('isis', $tiny1, $text1, $tiny2, $text2),
    'mysqli warning bind'
);
expect_same(true, $prepared->execute(), 'mysqli warning execute');
expect_same(4, $mysqli->warning_count, 'mysqli prepared warning count');
$expectedWarnings = [
    [1264, 'HY000', "Out of range value for column 'tiny_value' at row 1"],
    [1265, 'HY000', "Data truncated for column 'short_text' at row 1"],
    [1264, 'HY000', "Out of range value for column 'tiny_value' at row 2"],
    [1265, 'HY000', "Data truncated for column 'short_text' at row 2"],
];
expect_same($expectedWarnings, collect_warnings($prepared), 'mysqli prepared warning chain');
expect_same(
    $expectedWarnings,
    collect_warnings($prepared),
    'mysqli prepared warning chain remains statement-owned'
);

expect_same(true, $prepared->reset(), 'mysqli warning reset');
expect_same([], collect_warnings($prepared), 'mysqli reset clears statement warnings');
expect_same(true, $prepared->execute(), 'mysqli warning re-execute');
$held = $prepared->get_warnings();
expect_same(true, $held instanceof mysqli_warning, 'mysqli held warning object');
$prepared->close();
$mysqli->close();

$heldWarnings = [];
do {
    $heldWarnings[] = [$held->errno, $held->sqlstate, $held->message];
} while ($held->next());
expect_same($expectedWarnings, $heldWarnings, 'mysqli warning chain after close');

echo "mysql_statement_diagnostics_warning_snapshots_expectations: ok\n";
PHP
