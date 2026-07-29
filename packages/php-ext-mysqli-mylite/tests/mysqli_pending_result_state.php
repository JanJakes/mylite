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
if (PHP_VERSION_ID < 80400) {
    expect_out_of_sync(
        static fn(): array|null|false => $unbuffered->fetch_row(),
        'PHP 8.3 unbuffered fetch reports retained command error'
    );
    mysqli_report(MYSQLI_REPORT_OFF);
    expect_same('3', $unbuffered->fetch_row()[0], 'PHP 8.3 last unbuffered row');
    mysqli_report(MYSQLI_REPORT_ERROR | MYSQLI_REPORT_STRICT);
    expect_out_of_sync(
        static fn(): mysqli_result|bool => $mysqli->query('SELECT 15'),
        'last row without end-of-data'
    );
    expect_out_of_sync(
        static fn(): array|null|false => $unbuffered->fetch_row(),
        'PHP 8.3 end-of-data fetch reports retained command error'
    );
} else {
    expect_same('2', $unbuffered->fetch_row()[0], 'second unbuffered row');
    expect_same('3', $unbuffered->fetch_row()[0], 'last unbuffered row');
    expect_out_of_sync(
        static fn(): mysqli_result|bool => $mysqli->query('SELECT 15'),
        'last row without end-of-data'
    );
    expect_same(null, $unbuffered->fetch_row(), 'unbuffered end-of-data');
}
expect_same('16', $mysqli->query('SELECT 16')->fetch_row()[0], 'exhaustion recovery');

$mysqli->real_query('SELECT id FROM pending_items ORDER BY id');
$unbuffered = $mysqli->use_result();
expect_same('1', $unbuffered->fetch_row()[0], 'object-free first row');
$unbuffered->free();
expect_same('17', $mysqli->query('SELECT 17')->fetch_row()[0], 'object-free recovery');

$unbuffered = $mysqli->query(
    'SELECT id FROM pending_items ORDER BY id',
    MYSQLI_USE_RESULT
);
expect_same('1', $unbuffered->fetch_row()[0], 'destructor first row');
unset($unbuffered);
gc_collect_cycles();
expect_same('18', $mysqli->query('SELECT 18')->fetch_row()[0], 'destructor recovery');

$mysqli->real_query("SHOW TABLES LIKE 'pending_items'");
$unbuffered = $mysqli->use_result();
expect_out_of_sync(
    static fn(): mysqli_result|bool => $mysqli->query('SELECT 19'),
    'materialized utility unread result'
);
if (PHP_VERSION_ID < 80400) {
    mysqli_report(MYSQLI_REPORT_OFF);
    expect_same(
        'pending_items',
        $unbuffered->fetch_row()[0],
        'PHP 8.3 materialized utility row'
    );
    mysqli_report(MYSQLI_REPORT_ERROR | MYSQLI_REPORT_STRICT);
} else {
    expect_same('pending_items', $unbuffered->fetch_row()[0], 'materialized utility row');
}
expect_out_of_sync(
    static fn(): mysqli_result|bool => $mysqli->query('SELECT 20'),
    'materialized utility last row'
);
if (PHP_VERSION_ID < 80400) {
    expect_out_of_sync(
        static fn(): array|null|false => $unbuffered->fetch_row(),
        'PHP 8.3 materialized utility end-of-data reports retained command error'
    );
} else {
    expect_same(null, $unbuffered->fetch_row(), 'materialized utility end-of-data');
}
expect_same(
    '21',
    $mysqli->query('SELECT 21')->fetch_row()[0],
    'materialized utility recovery'
);

$mysqli->real_query('SELECT id FROM pending_items ORDER BY id');
$pendingPrepare = $mysqli->stmt_init();
expect_out_of_sync(
    static fn(): bool => $pendingPrepare->prepare('SELECT 22'),
    'prepare during direct pending result'
);
$mysqli->store_result()->free();
expect_true($pendingPrepare->prepare('SELECT 22'), 'prepare after direct store');
$pendingPrepare->close();

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
    '23',
    mysqli_query($mysqli, 'SELECT 23')->fetch_row()[0],
    'procedural free recovery'
);

$mysqli->begin_transaction();
expect_true(
    $mysqli->query("INSERT INTO pending_items VALUES (4, 'four')"),
    'transactional pending insert'
);
$unbuffered = $mysqli->query(
    'SELECT id FROM pending_items ORDER BY id',
    MYSQLI_USE_RESULT
);
expect_out_of_sync(static fn(): bool => $mysqli->commit(), 'transactional pending commit');
$unbuffered->free();
expect_true($mysqli->rollback(), 'rollback after pending free');
expect_same(
    '0',
    $mysqli->query('SELECT COUNT(*) FROM pending_items WHERE id = 4')->fetch_row()[0],
    'failed pending commit has no side effect'
);

$unbuffered = $mysqli->query(
    'SELECT id FROM pending_items ORDER BY id',
    MYSQLI_USE_RESULT
);
expect_out_of_sync(static fn(): bool => $mysqli->ping(), 'pending ping');
expect_out_of_sync_return($mysqli, static fn(): string|false => mysqli_stat($mysqli), 'pending stat');
expect_out_of_sync_return(
    $mysqli,
    static fn(): bool => $mysqli->refresh(MYSQLI_REFRESH_STATUS),
    'pending refresh'
);
expect_out_of_sync_return(
    $mysqli,
    static fn(): bool => mysqli_dump_debug_info($mysqli),
    'pending debug info'
);
expect_out_of_sync(static fn(): bool => $mysqli->kill($mysqli->thread_id), 'pending kill');
$unbuffered->free();
expect_same('38', $mysqli->query('SELECT 38')->fetch_row()[0], 'command-family recovery');

$statement = $mysqli->prepare('SELECT id FROM pending_items ORDER BY id');
$statement->execute();
expect_out_of_sync(
    static fn(): mysqli_result|bool => $mysqli->query('SELECT 24'),
    'prepared unread result'
);
expect_true($statement->store_result(), 'prepared store result');
expect_same('25', $mysqli->query('SELECT 25')->fetch_row()[0], 'prepared store recovery');
$statement->free_result();
$statement->close();

$statement = $mysqli->prepare('SELECT id FROM pending_items ORDER BY id');
$statement->execute();
$statement->bind_result($id);
expect_true($statement->fetch(), 'prepared partial fetch');
expect_same(1, $id, 'prepared first id');
expect_out_of_sync(
    static fn(): mysqli_result|bool => $mysqli->query('SELECT 26'),
    'prepared partial result'
);
expect_true($statement->reset(), 'prepared reset');
expect_same('27', $mysqli->query('SELECT 27')->fetch_row()[0], 'prepared reset recovery');

$statement->execute();
$statement->bind_result($id);
expect_true($statement->fetch(), 'prepared fetch before free');
$statement->free_result();
expect_same('28', $mysqli->query('SELECT 28')->fetch_row()[0], 'prepared free recovery');

$statement->execute();
expect_true($statement->execute(), 'same prepared statement re-execute');
expect_out_of_sync(
    static fn(): mysqli_result|bool => $mysqli->query('SELECT 29'),
    'same statement replacement remains pending'
);
expect_true($statement->close(), 'prepared close');
expect_same('30', $mysqli->query('SELECT 30')->fetch_row()[0], 'prepared close recovery');

$zeroRows = $mysqli->prepare('SELECT id FROM pending_items WHERE 0');
$zeroRows->execute();
expect_out_of_sync(
    static fn(): mysqli_result|bool => $mysqli->query('SELECT 31'),
    'prepared zero-row result'
);
$zeroRows->bind_result($id);
expect_same(null, $zeroRows->fetch(), 'prepared zero-row end-of-data');
expect_same('32', $mysqli->query('SELECT 32')->fetch_row()[0], 'zero-row recovery');
$zeroRows->close();

$metadataStatement = $mysqli->prepare(
    'SELECT id AS pending_id, value AS pending_value FROM pending_items ORDER BY id'
);
$metadataStatement->execute();
$metadata = $metadataStatement->result_metadata();
expect_same(2, $metadata->field_count, 'prepared metadata field count');
expect_same(0, $metadata->num_rows, 'prepared metadata row count');
expect_out_of_sync(
    static fn(): mysqli_result|bool => $mysqli->query('SELECT 33'),
    'prepared metadata preserves owner'
);
$metadata->free();
expect_out_of_sync(
    static fn(): mysqli_result|bool => $mysqli->query('SELECT 34'),
    'prepared metadata free preserves owner'
);
$metadataStatement->free_result();
expect_same(
    '35',
    $mysqli->query('SELECT 35')->fetch_row()[0],
    'prepared metadata owner recovery'
);
$metadataStatement->close();

$statement = $mysqli->prepare('SELECT id FROM pending_items ORDER BY id');
$statement->execute();
$result = $statement->get_result();
expect_same('36', $mysqli->query('SELECT 36')->fetch_row()[0], 'get-result recovery');
expect_same(3, $result->num_rows, 'get-result buffered rows');
$result->free();
$statement->close();

$first = $mysqli->prepare('SELECT id FROM pending_items ORDER BY id');
$second = $mysqli->prepare('SELECT 37');
$first->execute();
expect_out_of_sync(static fn(): bool => $second->execute(), 'different prepared owner');
$first->free_result();
expect_true($second->execute(), 'different prepared owner recovery');
$second->free_result();
$first->close();
$second->close();

$mysqli->close();

$closeFirst = open_mylite_mysqli();
reset_pending_items($closeFirst);
$closeFirstStatement = $closeFirst->prepare('SELECT id FROM pending_items ORDER BY id');
$closeFirstStatement->execute();
expect_true($closeFirst->close(), 'close connection with prepared result');
expect_true($closeFirstStatement->close(), 'close prepared owner after connection');
