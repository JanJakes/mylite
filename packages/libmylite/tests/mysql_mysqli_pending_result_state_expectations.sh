#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_mysqli_pending_result_state_expectations_$$"

fail() {
    printf '%s\n' "mysql_mysqli_pending_result_state_expectations: $1" >&2
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

command -v php >/dev/null 2>&1 || fail "php is required for mysqli expectations"

run_mysql \
    "DROP DATABASE IF EXISTS ${DATABASE};
     CREATE DATABASE ${DATABASE};
     CREATE TABLE ${DATABASE}.items (
         id INT PRIMARY KEY,
         value VARCHAR(20)
     ) ENGINE=InnoDB;
     INSERT INTO ${DATABASE}.items VALUES (1, 'one'), (2, 'two'), (3, 'three');" >/dev/null

MYLITE_PENDING_MYSQL_HOST=$(
    docker inspect -f '{{range .NetworkSettings.Networks}}{{.IPAddress}}{{end}}' \
        "$MYSQL_CONTAINER"
)
[ -n "$MYLITE_PENDING_MYSQL_HOST" ] || fail "could not resolve MySQL container address"
export MYLITE_PENDING_MYSQL_HOST
export MYLITE_PENDING_DATABASE="$DATABASE"

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

function open_connection(): mysqli
{
    $host = getenv('MYLITE_PENDING_MYSQL_HOST');
    $database = getenv('MYLITE_PENDING_DATABASE');
    if ($host === false || $database === false) {
        throw new RuntimeException('missing MySQL expectation environment');
    }
    return new mysqli($host, 'root', '', $database);
}

function expect_out_of_sync(callable $operation, string $context): void
{
    try {
        $operation();
        throw new RuntimeException($context . ': operation unexpectedly succeeded');
    } catch (mysqli_sql_exception $exception) {
        expect_same(2014, $exception->getCode(), $context . ' code');
        expect_same('HY000', $exception->getSqlState(), $context . ' SQLSTATE');
        expect_same(
            "Commands out of sync; you can't run this command now",
            $exception->getMessage(),
            $context . ' message'
        );
    }
}

function expect_out_of_sync_return(mysqli $connection, callable $operation, string $context): void
{
    expect_same(false, $operation(), $context . ' return');
    expect_same(2014, $connection->errno, $context . ' code');
    expect_same('HY000', $connection->sqlstate, $context . ' SQLSTATE');
    expect_same(
        "Commands out of sync; you can't run this command now",
        $connection->error,
        $context . ' message'
    );
}

mysqli_report(MYSQLI_REPORT_OFF);

$connection = open_connection();
expect_same(
    true,
    $connection->real_query('SELECT id FROM items ORDER BY id'),
    'report-off real query'
);
expect_same(false, $connection->query('SELECT 10'), 'report-off pending command');
expect_same(2014, $connection->errno, 'report-off pending errno');
expect_same('HY000', $connection->sqlstate, 'report-off pending SQLSTATE');
expect_same(
    "Commands out of sync; you can't run this command now",
    $connection->error,
    'report-off pending message'
);
$stored = $connection->store_result();
expect_same([['1'], ['2'], ['3']], $stored->fetch_all(MYSQLI_NUM), 'store after error');
expect_same('11', $connection->query('SELECT 11')->fetch_row()[0], 'store recovery');
$stored->free();
$connection->close();

mysqli_report(MYSQLI_REPORT_ERROR | MYSQLI_REPORT_STRICT);

$connection = open_connection();
$buffered = $connection->query('SELECT id FROM items ORDER BY id');
expect_same('12', $connection->query('SELECT 12')->fetch_row()[0], 'buffered unread command');
expect_same(3, $buffered->num_rows, 'buffered unread rows retained');
$buffered->free();
$connection->close();

$connection = open_connection();
$connection->real_query('SELECT id FROM items ORDER BY id');
expect_out_of_sync(
    static fn(): mysqli_result|bool => $connection->query('SELECT 13'),
    'unclaimed direct result'
);
$unbuffered = $connection->use_result();
expect_same('1', $unbuffered->fetch_row()[0], 'first unbuffered row');
expect_out_of_sync(
    static fn(): mysqli_result|bool => $connection->query('SELECT 14'),
    'partial unbuffered result'
);
expect_out_of_sync(static fn(): bool => $connection->commit(), 'unbuffered commit');
expect_out_of_sync(
    static fn(): bool => $connection->autocommit(false),
    'unbuffered autocommit'
);
expect_same('2', $unbuffered->fetch_row()[0], 'second unbuffered row');
expect_same('3', $unbuffered->fetch_row()[0], 'last unbuffered row');
expect_out_of_sync(
    static fn(): mysqli_result|bool => $connection->query('SELECT 15'),
    'last row without end-of-data'
);
expect_same(null, $unbuffered->fetch_row(), 'unbuffered end-of-data');
expect_same('16', $connection->query('SELECT 16')->fetch_row()[0], 'exhaustion recovery');
$connection->close();

$connection = open_connection();
$connection->real_query('SELECT id FROM items ORDER BY id');
$unbuffered = $connection->use_result();
expect_same('1', $unbuffered->fetch_row()[0], 'object-free first row');
$unbuffered->free();
expect_same('117', $connection->query('SELECT 117')->fetch_row()[0], 'object-free recovery');
$connection->close();

$connection = open_connection();
$unbuffered = $connection->query('SELECT id FROM items ORDER BY id', MYSQLI_USE_RESULT);
expect_same('1', $unbuffered->fetch_row()[0], 'destructor first row');
unset($unbuffered);
gc_collect_cycles();
expect_same('118', $connection->query('SELECT 118')->fetch_row()[0], 'destructor recovery');
$connection->close();

$connection = open_connection();
$connection->real_query("SHOW TABLES LIKE 'items'");
$unbuffered = $connection->use_result();
expect_out_of_sync(
    static fn(): mysqli_result|bool => $connection->query('SELECT 119'),
    'materialized utility unread result'
);
expect_same('items', $unbuffered->fetch_row()[0], 'materialized utility row');
expect_out_of_sync(
    static fn(): mysqli_result|bool => $connection->query('SELECT 120'),
    'materialized utility last row'
);
expect_same(null, $unbuffered->fetch_row(), 'materialized utility end-of-data');
expect_same(
    '121',
    $connection->query('SELECT 121')->fetch_row()[0],
    'materialized utility recovery'
);
$connection->close();

$connection = open_connection();
$connection->real_query('SELECT id FROM items ORDER BY id');
$pendingPrepare = $connection->stmt_init();
expect_out_of_sync(
    static fn(): bool => $pendingPrepare->prepare('SELECT 122'),
    'prepare during direct pending result'
);
$connection->store_result()->free();
expect_same(true, $pendingPrepare->prepare('SELECT 122'), 'prepare after direct store');
$pendingPrepare->close();
$connection->close();

$connection = open_connection();
expect_same(
    true,
    mysqli_real_query($connection, 'SELECT id FROM items ORDER BY id'),
    'procedural real query'
);
$unbuffered = mysqli_use_result($connection);
expect_out_of_sync(
    static fn(): mysqli_result|bool => mysqli_query($connection, 'SELECT 17'),
    'procedural unread result'
);
mysqli_free_result($unbuffered);
expect_same(
    '18',
    mysqli_query($connection, 'SELECT 18')->fetch_row()[0],
    'procedural free recovery'
);
$connection->close();

$connection = open_connection();
$connection->begin_transaction();
expect_same(
    true,
    $connection->query("INSERT INTO items VALUES (4, 'four')"),
    'transactional pending insert'
);
$unbuffered = $connection->query('SELECT id FROM items ORDER BY id', MYSQLI_USE_RESULT);
expect_out_of_sync(static fn(): bool => $connection->commit(), 'transactional pending commit');
$unbuffered->free();
expect_same(true, $connection->rollback(), 'rollback after pending free');
expect_same(
    '0',
    $connection->query('SELECT COUNT(*) FROM items WHERE id = 4')->fetch_row()[0],
    'failed pending commit has no side effect'
);
$connection->close();

$connection = open_connection();
$unbuffered = $connection->query('SELECT id FROM items ORDER BY id', MYSQLI_USE_RESULT);
expect_out_of_sync(static fn(): bool => $connection->ping(), 'pending ping');
expect_out_of_sync_return(
    $connection,
    static fn(): string|false => mysqli_stat($connection),
    'pending stat'
);
expect_out_of_sync_return(
    $connection,
    static fn(): bool => $connection->refresh(MYSQLI_REFRESH_STATUS),
    'pending refresh'
);
expect_out_of_sync_return(
    $connection,
    static fn(): bool => mysqli_dump_debug_info($connection),
    'pending debug info'
);
expect_out_of_sync(
    static fn(): bool => $connection->kill($connection->thread_id),
    'pending kill'
);
$unbuffered->free();
expect_same(
    '136',
    $connection->query('SELECT 136')->fetch_row()[0],
    'command-family recovery'
);
$connection->close();

$connection = open_connection();
$statement = $connection->prepare('SELECT id FROM items ORDER BY id');
$statement->execute();
expect_out_of_sync(
    static fn(): mysqli_result|bool => $connection->query('SELECT 19'),
    'prepared unread result'
);
expect_same(true, $statement->store_result(), 'prepared store result');
expect_same('20', $connection->query('SELECT 20')->fetch_row()[0], 'prepared store recovery');
$statement->free_result();
$statement->close();
$connection->close();

$connection = open_connection();
$statement = $connection->prepare('SELECT id FROM items ORDER BY id');
$statement->execute();
$statement->bind_result($id);
expect_same(true, $statement->fetch(), 'prepared partial fetch');
expect_same(1, $id, 'prepared first id');
expect_out_of_sync(
    static fn(): mysqli_result|bool => $connection->query('SELECT 21'),
    'prepared partial result'
);
expect_same(true, $statement->reset(), 'prepared reset');
expect_same('22', $connection->query('SELECT 22')->fetch_row()[0], 'prepared reset recovery');

$statement->execute();
$statement->bind_result($id);
expect_same(true, $statement->fetch(), 'prepared fetch before free');
$statement->free_result();
expect_same('23', $connection->query('SELECT 23')->fetch_row()[0], 'prepared free recovery');

$statement->execute();
expect_same(true, $statement->execute(), 'same prepared statement re-execute');
expect_out_of_sync(
    static fn(): mysqli_result|bool => $connection->query('SELECT 24'),
    'same statement replacement remains pending'
);
expect_same(true, $statement->close(), 'prepared close');
expect_same('25', $connection->query('SELECT 25')->fetch_row()[0], 'prepared close recovery');
$connection->close();

$connection = open_connection();
$zeroRows = $connection->prepare('SELECT id FROM items WHERE 0');
$zeroRows->execute();
expect_out_of_sync(
    static fn(): mysqli_result|bool => $connection->query('SELECT 131'),
    'prepared zero-row result'
);
$zeroRows->bind_result($id);
expect_same(null, $zeroRows->fetch(), 'prepared zero-row end-of-data');
expect_same('132', $connection->query('SELECT 132')->fetch_row()[0], 'zero-row recovery');
$zeroRows->close();
$connection->close();

$connection = open_connection();
$metadataStatement = $connection->prepare(
    'SELECT id AS pending_id, value AS pending_value FROM items ORDER BY id'
);
$metadataStatement->execute();
$metadata = $metadataStatement->result_metadata();
expect_same(2, $metadata->field_count, 'prepared metadata field count');
expect_same(0, $metadata->num_rows, 'prepared metadata row count');
expect_out_of_sync(
    static fn(): mysqli_result|bool => $connection->query('SELECT 133'),
    'prepared metadata preserves owner'
);
$metadata->free();
expect_out_of_sync(
    static fn(): mysqli_result|bool => $connection->query('SELECT 134'),
    'prepared metadata free preserves owner'
);
$metadataStatement->free_result();
expect_same(
    '135',
    $connection->query('SELECT 135')->fetch_row()[0],
    'prepared metadata owner recovery'
);
$metadataStatement->close();
$connection->close();

$connection = open_connection();
$statement = $connection->prepare('SELECT id FROM items ORDER BY id');
$statement->execute();
$result = $statement->get_result();
expect_same('26', $connection->query('SELECT 26')->fetch_row()[0], 'get-result recovery');
expect_same(3, $result->num_rows, 'get-result buffered rows');
$result->free();
$statement->close();

$first = $connection->prepare('SELECT id FROM items ORDER BY id');
$second = $connection->prepare('SELECT 27');
$first->execute();
expect_out_of_sync(static fn(): bool => $second->execute(), 'different prepared owner');
$first->free_result();
expect_same(true, $second->execute(), 'different prepared owner recovery');
$second->free_result();
$first->close();
$second->close();
$connection->close();

$connection = open_connection();
$closeFirstStatement = $connection->prepare('SELECT id FROM items ORDER BY id');
$closeFirstStatement->execute();
expect_same(true, $connection->close(), 'close connection with prepared result');
expect_same(true, $closeFirstStatement->close(), 'close prepared owner after connection');

printf("mysqli pending-result state MySQL 8.4.9 expectations verified\n");
PHP
