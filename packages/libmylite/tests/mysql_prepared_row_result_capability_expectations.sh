#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_prepared_row_result_capability_expectations_$$"

fail() {
    printf '%s\n' "mysql_prepared_row_result_capability_expectations: $1" >&2
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
     CREATE TABLE ${DATABASE}.alpha (
         id INT PRIMARY KEY,
         value INT
     ) ENGINE=InnoDB;
     INSERT INTO ${DATABASE}.alpha VALUES (1, 10), (2, 20);" >/dev/null

MYLITE_PREPARED_ROWS_MYSQL_HOST=$(
    docker inspect -f '{{range .NetworkSettings.Networks}}{{.IPAddress}}{{end}}' \
        "$MYSQL_CONTAINER"
)
[ -n "$MYLITE_PREPARED_ROWS_MYSQL_HOST" ] || fail "could not resolve MySQL container address"
export MYLITE_PREPARED_ROWS_MYSQL_HOST
export MYLITE_PREPARED_ROWS_DATABASE="$DATABASE"

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

function mysqli_rows(mysqli_stmt $statement): array
{
    $statement->execute();
    $result = $statement->get_result();
    $rows = $result->fetch_all(MYSQLI_ASSOC);
    $result->free();
    return $rows;
}

function pdo_rows(PDOStatement $statement): array
{
    $statement->execute();
    $rows = $statement->fetchAll(PDO::FETCH_ASSOC);
    $statement->closeCursor();
    return $rows;
}

function first_values(array $rows): array
{
    return array_map(
        static fn(array $row): mixed => array_values($row)[0],
        $rows
    );
}

$host = getenv('MYLITE_PREPARED_ROWS_MYSQL_HOST');
$database = getenv('MYLITE_PREPARED_ROWS_DATABASE');
if ($host === false || $database === false) {
    throw new RuntimeException('missing MySQL expectation environment');
}

$mysqli = new mysqli($host, 'root', '', $database);

$showTables = $mysqli->prepare('SHOW TABLES');
expect_same(['alpha'], first_values(mysqli_rows($showTables)), 'mysqli first SHOW TABLES');
$mysqli->query('CREATE TABLE beta (id INT PRIMARY KEY) ENGINE=InnoDB');
expect_same(
    ['alpha', 'beta'],
    first_values(mysqli_rows($showTables)),
    'mysqli repeated SHOW TABLES'
);
$showTables->close();

$describe = $mysqli->prepare('DESCRIBE alpha');
expect_same(
    ['id', 'value'],
    array_column(mysqli_rows($describe), 'Field'),
    'mysqli first DESCRIBE'
);
$mysqli->query('ALTER TABLE alpha ADD COLUMN marker VARCHAR(8) NULL');
expect_same(
    ['id', 'value', 'marker'],
    array_column(mysqli_rows($describe), 'Field'),
    'mysqli repeated DESCRIBE'
);
$describe->close();

$explain = $mysqli->prepare('EXPLAIN SELECT * FROM alpha WHERE value = 20');
$firstExplain = mysqli_rows($explain);
expect_same(null, $firstExplain[0]['key'], 'mysqli first EXPLAIN key');
$mysqli->query('CREATE INDEX idx_value ON alpha (value)');
$secondExplain = mysqli_rows($explain);
expect_same('idx_value', $secondExplain[0]['key'], 'mysqli repeated EXPLAIN key');
$explain->close();

$mysqli->query("SET SESSION sql_mode = ''");
$showVariables = $mysqli->prepare("SHOW VARIABLES LIKE 'sql_mode'");
expect_same('', mysqli_rows($showVariables)[0]['Value'], 'mysqli first SHOW VARIABLES');
$mysqli->query("SET SESSION sql_mode = 'ANSI_QUOTES'");
expect_same(
    'ANSI_QUOTES',
    mysqli_rows($showVariables)[0]['Value'],
    'mysqli repeated SHOW VARIABLES'
);
$showVariables->close();
$mysqli->close();

$dsn = 'mysql:host=' . $host . ';dbname=' . $database . ';charset=utf8mb4';
$pdo = new PDO(
    $dsn,
    'root',
    '',
    [
        PDO::ATTR_ERRMODE => PDO::ERRMODE_EXCEPTION,
        PDO::ATTR_EMULATE_PREPARES => false,
    ]
);

$showTables = $pdo->prepare('SHOW TABLES');
expect_same(
    ['alpha', 'beta'],
    first_values(pdo_rows($showTables)),
    'PDO first SHOW TABLES'
);
$pdo->exec('CREATE TABLE gamma (id INT PRIMARY KEY) ENGINE=InnoDB');
expect_same(
    ['alpha', 'beta', 'gamma'],
    first_values(pdo_rows($showTables)),
    'PDO repeated SHOW TABLES'
);

$describe = $pdo->prepare('DESCRIBE alpha');
expect_same(
    ['id', 'value', 'marker'],
    array_column(pdo_rows($describe), 'Field'),
    'PDO first DESCRIBE'
);
$pdo->exec('ALTER TABLE alpha ADD COLUMN pdo_marker INT NULL');
expect_same(
    ['id', 'value', 'marker', 'pdo_marker'],
    array_column(pdo_rows($describe), 'Field'),
    'PDO repeated DESCRIBE'
);

$pdo->exec('UPDATE alpha SET pdo_marker = id');
$explain = $pdo->prepare('EXPLAIN SELECT * FROM alpha WHERE pdo_marker = 2');
$firstExplain = pdo_rows($explain);
expect_same(null, $firstExplain[0]['key'], 'PDO first EXPLAIN key');
$pdo->exec('CREATE INDEX idx_pdo_marker ON alpha (pdo_marker)');
$secondExplain = pdo_rows($explain);
expect_same('idx_pdo_marker', $secondExplain[0]['key'], 'PDO repeated EXPLAIN key');

$pdo->exec("SET SESSION sql_mode = ''");
$showVariables = $pdo->prepare("SHOW VARIABLES LIKE 'sql_mode'");
expect_same('', pdo_rows($showVariables)[0]['Value'], 'PDO first SHOW VARIABLES');
$pdo->exec("SET SESSION sql_mode = 'ONLY_FULL_GROUP_BY'");
expect_same(
    'ONLY_FULL_GROUP_BY',
    pdo_rows($showVariables)[0]['Value'],
    'PDO repeated SHOW VARIABLES'
);

printf("prepared-row-result capability MySQL 8.4.9 mysqli/PDO expectations verified\n");
PHP
