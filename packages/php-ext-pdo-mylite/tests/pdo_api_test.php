<?php

declare(strict_types=1);

function expect_true(bool $condition, string $message): void
{
    if (!$condition) {
        throw new RuntimeException($message);
    }
}

expect_true(extension_loaded('mylite'), 'mylite extension is not loaded');
expect_true(extension_loaded('pdo_mylite'), 'pdo_mylite extension is not loaded');
expect_true(in_array('mylite', PDO::getAvailableDrivers(), true), 'mylite PDO driver is not registered');

function mylite_pdo_test_path(string $name): string
{
    $path = sys_get_temp_dir() . '/mylite-php-pdo-' . getmypid() . '-' . $name . '.mylite';
    if (file_exists($path)) {
        unlink($path);
    }
    register_shutdown_function(static function () use ($path): void {
        if (file_exists($path)) {
            unlink($path);
        }
    });
    return $path;
}

$pdo = new PDO('mylite:' . mylite_pdo_test_path('api'), null, null, [
    PDO::ATTR_ERRMODE => PDO::ERRMODE_EXCEPTION,
    PDO::ATTR_DEFAULT_FETCH_MODE => PDO::FETCH_ASSOC,
]);

expect_true($pdo->getAttribute(PDO::ATTR_DRIVER_NAME) === 'mylite', 'driver name mismatch');
expect_true($pdo->exec('CREATE DATABASE app') >= 0, 'CREATE DATABASE failed');
expect_true($pdo->exec('USE app') >= 0, 'USE failed');
expect_true($pdo->exec('CREATE TABLE people (id INT PRIMARY KEY, name VARCHAR(32))') >= 0, 'CREATE TABLE failed');
expect_true($pdo->exec("INSERT INTO people VALUES (1, 'Ada')") === 1, 'INSERT failed');

$stmt = $pdo->prepare('INSERT INTO people VALUES (?, ?)');
expect_true($stmt instanceof PDOStatement, 'prepare did not return PDOStatement');
expect_true($stmt->execute([2, 'Grace']), 'prepared INSERT failed');
expect_true($stmt->rowCount() === 1, 'prepared INSERT row count mismatch');
expect_true($pdo->lastInsertId() === '0', 'prepared INSERT last insert ID mismatch');
expect_true($pdo->lastInsertId('id') === '0', 'named last insert ID mismatch');

expect_true($pdo->exec(
    'CREATE TABLE generated_people (' .
    'id INT AUTO_INCREMENT PRIMARY KEY, name VARCHAR(32) NOT NULL)'
) >= 0, 'generated people table creation failed');
$generatedInsert = $pdo->prepare('INSERT INTO generated_people (name) VALUES (?)');
expect_true($generatedInsert->execute(['Lin']), 'generated prepared INSERT failed');
expect_true($pdo->lastInsertId() === '1', 'generated prepared INSERT last insert ID mismatch');
expect_true($pdo->lastInsertId('id') === '1', 'generated named last insert ID mismatch');

$stmt = $pdo->prepare('SELECT name FROM people WHERE name = ?');
expect_true($stmt->execute(['Grace']), 'prepared SELECT failed');
expect_true($stmt->columnCount() === 1, 'prepared SELECT column count mismatch');
expect_true($stmt->fetch() === ['name' => 'Grace'], 'prepared SELECT row mismatch');
expect_true($stmt->fetch() === false, 'prepared SELECT should be exhausted');

$byReference = $pdo->prepare('SELECT name FROM people WHERE id = ?');
$personId = 1;
expect_true($byReference->bindParam(1, $personId, PDO::PARAM_INT), 'bindParam failed');
expect_true($byReference->execute(), 'first bindParam execute failed');
expect_true($byReference->fetchColumn() === 'Ada', 'first bindParam value mismatch');
$personId = 2;
expect_true($byReference->execute(), 'second bindParam execute failed');
expect_true($byReference->fetchColumn() === 'Grace', 'second bindParam value mismatch');

$named = $pdo->prepare('SELECT name FROM people WHERE id = :person_id');
expect_true($named->bindValue(':person_id', 2, PDO::PARAM_INT), 'named bindValue failed');
expect_true($named->execute(), 'named parameter execute failed');
expect_true($named->fetchColumn() === 'Grace', 'named parameter value mismatch');

$typed = $pdo->prepare('SELECT ? AS bool_value, ? AS int_value, ? AS null_value, ? AS text_value');
expect_true($typed->bindValue(1, true, PDO::PARAM_BOOL), 'boolean bindValue failed');
expect_true($typed->bindValue(2, -42, PDO::PARAM_INT), 'integer bindValue failed');
expect_true($typed->bindValue(3, null, PDO::PARAM_NULL), 'NULL bindValue failed');
expect_true($typed->bindValue(4, '', PDO::PARAM_STR), 'empty text bindValue failed');
expect_true($typed->execute(), 'typed parameter execute failed');
expect_true($typed->fetch() === [
    'bool_value' => '1',
    'int_value' => '-42',
    'null_value' => null,
    'text_value' => '',
], 'typed parameter values mismatch');

expect_true($pdo->exec(
    'CREATE TABLE payloads (' .
    'id INT PRIMARY KEY, text_value VARCHAR(255), blob_value BLOB, nullable_value VARCHAR(20))'
) >= 0, 'payload table creation failed');
expect_true($pdo->exec("SET SESSION sql_mode = 'NO_BACKSLASH_ESCAPES'") >= 0, 'sql_mode setup failed');
$hostileText = "x'); DROP TABLE people; -- \\ tail";
$blobBytes = "a\0b'\xff";
$blobStream = fopen('php://memory', 'r+');
expect_true($blobStream !== false, 'LOB stream creation failed');
expect_true(fwrite($blobStream, $blobBytes) === strlen($blobBytes), 'LOB stream write failed');
rewind($blobStream);
$payloadInsert = $pdo->prepare('INSERT INTO payloads VALUES (?, ?, ?, ?)');
expect_true($payloadInsert->bindValue(1, 1, PDO::PARAM_INT), 'payload ID bind failed');
expect_true($payloadInsert->bindValue(2, $hostileText, PDO::PARAM_STR), 'hostile text bind failed');
expect_true($payloadInsert->bindValue(3, $blobStream, PDO::PARAM_LOB), 'LOB bind failed');
expect_true($payloadInsert->bindValue(4, null, PDO::PARAM_NULL), 'payload NULL bind failed');
expect_true($payloadInsert->execute(), 'payload INSERT failed');
fclose($blobStream);
$payload = $pdo->query('SELECT text_value, blob_value, nullable_value FROM payloads WHERE id = 1')->fetch();
expect_true($payload === [
    'text_value' => $hostileText,
    'blob_value' => $blobBytes,
    'nullable_value' => null,
], 'binary-safe payload round trip mismatch');
expect_true($pdo->query('SELECT COUNT(*) FROM people')->fetchColumn() === '2', 'hostile text changed schema');

$tables = $pdo->query('SHOW TABLES')->fetchAll(PDO::FETCH_COLUMN);
expect_true(in_array('people', $tables, true), 'SHOW TABLES omitted people');
expect_true(in_array('payloads', $tables, true), 'SHOW TABLES omitted payloads');
$preparedDdl = $pdo->prepare('CREATE TABLE prepared_table (id INT)');
expect_true($preparedDdl->execute(), 'prepared DDL execution failed');
expect_true(in_array('prepared_table', $pdo->query('SHOW TABLES')->fetchAll(PDO::FETCH_COLUMN), true), 'prepared DDL table missing');

$prepareFailed = false;
$missingStatement = null;
try {
    $missingStatement = $pdo->prepare('SELECT missing_column FROM missing_table');
} catch (PDOException) {
    $prepareFailed = true;
}
expect_true($prepareFailed || $missingStatement === false, 'missing table should fail at prepare time');

$stream = $pdo->prepare('SELECT id, name FROM people ORDER BY id');
expect_true($stream->execute(), 'streaming SELECT failed');
expect_true($stream->columnCount() === 2, 'streaming SELECT column count mismatch');
expect_true($stream->fetch(PDO::FETCH_NUM) === ['1', 'Ada'], 'streaming SELECT first row mismatch');
expect_true($stream->closeCursor(), 'streaming SELECT closeCursor failed');
expect_true($stream->execute(), 'streaming SELECT re-execute failed');
expect_true($stream->fetch(PDO::FETCH_ASSOC) === ['id' => '1', 'name' => 'Ada'], 'streaming SELECT re-execute row mismatch');

$scrollPrepareRejected = false;
try {
    $scrollPrepareRejected = $pdo->prepare(
        'SELECT id FROM people ORDER BY id',
        [PDO::ATTR_CURSOR => PDO::CURSOR_SCROLL]
    ) === false;
} catch (PDOException) {
    $scrollPrepareRejected = true;
}
expect_true($scrollPrepareRejected, 'scrollable cursor option must be rejected');

$orientationStatement = $pdo->prepare('SELECT id FROM people ORDER BY id');
expect_true($orientationStatement->execute(), 'orientation statement execute failed');
$orientationRejected = false;
try {
    $orientationRejected =
        $orientationStatement->fetch(PDO::FETCH_ASSOC, PDO::FETCH_ORI_ABS, 1) === false;
} catch (PDOException) {
    $orientationRejected = true;
}
expect_true($orientationRejected, 'absolute fetch orientation must be rejected');
expect_true(
    $orientationStatement->fetch(PDO::FETCH_ASSOC) === ['id' => '1'],
    'rejected orientation must not consume the pending row'
);

expect_true($pdo->beginTransaction(), 'beginTransaction failed');
expect_true($pdo->exec("INSERT INTO people VALUES (3, 'Katherine')") === 1, 'transaction INSERT failed');
expect_true($pdo->rollBack(), 'rollBack failed');
expect_true($stream->fetch(PDO::FETCH_ASSOC) === ['id' => '2', 'name' => 'Grace'], 'buffered SELECT unread row mismatch');
expect_true($pdo->query('SELECT COUNT(*) AS total FROM people')->fetch() === ['total' => '2'], 'rollback count mismatch');

$quotedPayload = "A\0\n\r\\'\"\x1aB";
$quotedLiteral = $pdo->quote($quotedPayload);
expect_true(
    $quotedLiteral === "'" . str_replace("'", "''", $quotedPayload) . "'",
    'PDO NO_BACKSLASH_ESCAPES quote mismatch'
);
expect_true(
    $pdo->query('SELECT ' . $quotedLiteral)->fetchColumn() === $quotedPayload,
    'PDO quoted binary payload round trip mismatch'
);
$quotedAttack = $pdo->quote("Ada' OR 1=1 -- ");
expect_true(
    $pdo->query('SELECT COUNT(*) FROM people WHERE name = ' . $quotedAttack)->fetchColumn() === '0',
    'PDO quote allowed SQL injection'
);

$pdo->setAttribute(PDO::ATTR_ERRMODE, PDO::ERRMODE_SILENT);
expect_true($pdo->exec('INSERT INTO missing_table VALUES (1)') === false, 'silent invalid query should fail');
$errorInfo = $pdo->errorInfo();
expect_true($errorInfo[0] !== '00000', 'PDO errorInfo SQLSTATE mismatch');
expect_true($errorInfo[1] > 0, 'PDO errorInfo native code mismatch');
expect_true($errorInfo[2] !== '', 'PDO errorInfo message mismatch');

$statementFirstPdo = new PDO(
    'mylite:' . mylite_pdo_test_path('statement-first'),
    null,
    null,
    [PDO::ATTR_ERRMODE => PDO::ERRMODE_EXCEPTION]
);
$releasedStatement = $statementFirstPdo->prepare('SELECT 1');
unset($releasedStatement);
gc_collect_cycles();
expect_true(
    $statementFirstPdo->query('SELECT 2')->fetchColumn() === '2',
    'PDO connection failed after statement-first destruction'
);
unset($statementFirstPdo);

$dbhReferencePdo = new PDO(
    'mylite:' . mylite_pdo_test_path('dbh-reference'),
    null,
    null,
    [PDO::ATTR_ERRMODE => PDO::ERRMODE_EXCEPTION]
);
$dbhReferenceStatement = $dbhReferencePdo->prepare('SELECT ?');
unset($dbhReferencePdo);
gc_collect_cycles();
expect_true(
    $dbhReferenceStatement->execute([3]) && $dbhReferenceStatement->fetchColumn() === '3',
    'PDO statement did not retain its database handle'
);
unset($dbhReferenceStatement);
gc_collect_cycles();
