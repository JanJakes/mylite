<?php

require __DIR__ . '/bootstrap.php';

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

function reset_pending_items(mysqli $connection): void
{
    expect_true($connection->query('DROP TABLE IF EXISTS pending_items'), 'drop pending items');
    expect_true(
        $connection->query(
            'CREATE TABLE pending_items (' .
            'id INT PRIMARY KEY, value VARCHAR(20)' .
            ')'
        ),
        'create pending items'
    );
    expect_true(
        $connection->query(
            "INSERT INTO pending_items VALUES (1, 'one'), (2, 'two'), (3, 'three')"
        ),
        'insert pending items'
    );
}

$mysqli = open_mylite_mysqli();
reset_pending_items($mysqli);

expect_true(
    $mysqli->real_query('SELECT id FROM pending_items ORDER BY id'),
    'report-off real query'
);
expect_false($mysqli->query('SELECT 10'), 'report-off pending command');
expect_same(2014, $mysqli->errno, 'report-off pending errno');
expect_same('HY000', $mysqli->sqlstate, 'report-off pending SQLSTATE');
expect_same(
    "Commands out of sync; you can't run this command now",
    $mysqli->error,
    'report-off pending message'
);
$stored = $mysqli->store_result();
expect_same([['1'], ['2'], ['3']], $stored->fetch_all(MYSQLI_NUM), 'store after error');
expect_same('11', $mysqli->query('SELECT 11')->fetch_row()[0], 'store recovery');
$stored->free();

mysqli_report(MYSQLI_REPORT_ERROR | MYSQLI_REPORT_STRICT);

$buffered = $mysqli->query('SELECT id FROM pending_items ORDER BY id');
expect_same('12', $mysqli->query('SELECT 12')->fetch_row()[0], 'buffered unread command');
expect_same(3, $buffered->num_rows, 'buffered unread rows retained');
$buffered->free();

$mysqli->real_query('SELECT id FROM pending_items ORDER BY id');
expect_out_of_sync(
    static fn(): mysqli_result|bool => $mysqli->query('SELECT 13'),
    'unclaimed direct result'
);
$unbuffered = $mysqli->use_result();
expect_same('1', $unbuffered->fetch_row()[0], 'first unbuffered row');
expect_out_of_sync(
    static fn(): mysqli_result|bool => $mysqli->query('SELECT 14'),
    'partial unbuffered result'
);
expect_out_of_sync(static fn(): bool => $mysqli->commit(), 'unbuffered commit');
expect_out_of_sync(
    static fn(): bool => $mysqli->autocommit(false),
    'unbuffered autocommit'
);
expect_same('2', $unbuffered->fetch_row()[0], 'second unbuffered row');
expect_same('3', $unbuffered->fetch_row()[0], 'last unbuffered row');
expect_out_of_sync(
    static fn(): mysqli_result|bool => $mysqli->query('SELECT 15'),
    'last row without end-of-data'
);
expect_same(null, $unbuffered->fetch_row(), 'unbuffered end-of-data');
expect_same('16', $mysqli->query('SELECT 16')->fetch_row()[0], 'exhaustion recovery');

expect_true(
    mysqli_real_query($mysqli, 'SELECT id FROM pending_items ORDER BY id'),
    'procedural real query'
);
$unbuffered = mysqli_use_result($mysqli);
expect_out_of_sync(
    static fn(): mysqli_result|bool => mysqli_query($mysqli, 'SELECT 17'),
    'procedural unread result'
);
mysqli_free_result($unbuffered);
expect_same(
    '18',
    mysqli_query($mysqli, 'SELECT 18')->fetch_row()[0],
    'procedural free recovery'
);

$statement = $mysqli->prepare('SELECT id FROM pending_items ORDER BY id');
$statement->execute();
expect_out_of_sync(
    static fn(): mysqli_result|bool => $mysqli->query('SELECT 19'),
    'prepared unread result'
);
expect_true($statement->store_result(), 'prepared store result');
expect_same('20', $mysqli->query('SELECT 20')->fetch_row()[0], 'prepared store recovery');
$statement->free_result();
$statement->close();

$statement = $mysqli->prepare('SELECT id FROM pending_items ORDER BY id');
$statement->execute();
$statement->bind_result($id);
expect_true($statement->fetch(), 'prepared partial fetch');
expect_same('1', $id, 'prepared first id');
expect_out_of_sync(
    static fn(): mysqli_result|bool => $mysqli->query('SELECT 21'),
    'prepared partial result'
);
expect_true($statement->reset(), 'prepared reset');
expect_same('22', $mysqli->query('SELECT 22')->fetch_row()[0], 'prepared reset recovery');

$statement->execute();
$statement->bind_result($id);
expect_true($statement->fetch(), 'prepared fetch before free');
$statement->free_result();
expect_same('23', $mysqli->query('SELECT 23')->fetch_row()[0], 'prepared free recovery');

$statement->execute();
expect_true($statement->execute(), 'same prepared statement re-execute');
expect_out_of_sync(
    static fn(): mysqli_result|bool => $mysqli->query('SELECT 24'),
    'same statement replacement remains pending'
);
expect_true($statement->close(), 'prepared close');
expect_same('25', $mysqli->query('SELECT 25')->fetch_row()[0], 'prepared close recovery');

$statement = $mysqli->prepare('SELECT id FROM pending_items ORDER BY id');
$statement->execute();
$result = $statement->get_result();
expect_same('26', $mysqli->query('SELECT 26')->fetch_row()[0], 'get-result recovery');
expect_same(3, $result->num_rows, 'get-result buffered rows');
$result->free();
$statement->close();

$first = $mysqli->prepare('SELECT id FROM pending_items ORDER BY id');
$second = $mysqli->prepare('SELECT 27');
$first->execute();
expect_out_of_sync(static fn(): bool => $second->execute(), 'different prepared owner');
$first->free_result();
expect_true($second->execute(), 'different prepared owner recovery');
$second->free_result();
$first->close();
$second->close();

$mysqli->close();
