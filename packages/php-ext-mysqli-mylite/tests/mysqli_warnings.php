<?php

require __DIR__ . '/bootstrap.php';

function collect_warning_chain(mysqli_warning|false $warning): array
{
    if ($warning === false) {
        return [];
    }
    $records = [];
    do {
        $records[] = [$warning->errno, $warning->sqlstate, $warning->message];
    } while ($warning->next());
    return $records;
}

$mysqli = open_mylite_mysqli();

$direct = $mysqli->query('DO 5 DIV 0, 6 DIV 0');
expect_true($direct, 'direct warning query');
expect_same(2, $mysqli->warning_count, 'direct warning count');
expect_same([
    [1365, 'HY000', 'Division by 0'],
    [1365, 'HY000', 'Division by 0'],
], collect_warning_chain($mysqli->get_warnings()), 'object direct warning chain');

$procedural = mysqli_query($mysqli, 'DO 5 DIV 0, 6 DIV 0');
expect_true($procedural, 'procedural warning query');
expect_same(2, mysqli_warning_count($mysqli), 'procedural warning count');
expect_same([
    [1365, 'HY000', 'Division by 0'],
    [1365, 'HY000', 'Division by 0'],
], collect_warning_chain(mysqli_get_warnings($mysqli)), 'procedural direct warning chain');

$warningFree = $mysqli->query('SELECT 1');
expect_true($warningFree instanceof mysqli_result, 'warning-free query');
expect_same(0, $mysqli->warning_count, 'warning-free count');
expect_false($mysqli->get_warnings(), 'warning-free object list');
expect_false(mysqli_get_warnings($mysqli), 'warning-free procedural list');
$warningFree->free();

$unbuffered = $mysqli->query('SELECT 5 DIV 0, 6 DIV 0', MYSQLI_USE_RESULT);
expect_true($unbuffered instanceof mysqli_result, 'unbuffered warning query');
expect_same([null, null], $unbuffered->fetch_row(), 'unbuffered warning row');
expect_same(null, $unbuffered->fetch_row(), 'unbuffered warning EOF');
expect_same(2, $mysqli->warning_count, 'unbuffered warning count after EOF');
expect_same([
    [1365, 'HY000', 'Division by 0'],
    [1365, 'HY000', 'Division by 0'],
], collect_warning_chain($mysqli->get_warnings()), 'unbuffered warning chain after EOF');
$unbuffered->free();

expect_true($mysqli->query("SET SESSION sql_mode = ''"), 'set non-strict mode');
expect_true(
    $mysqli->query('CREATE TABLE warning_rows (tiny_value TINYINT, short_text VARCHAR(2))'),
    'create warning table'
);
$prepared = $mysqli->prepare(
    'INSERT INTO warning_rows(tiny_value, short_text) VALUES (?, ?), (?, ?)'
);
expect_true($prepared instanceof mysqli_stmt, 'prepare warning insert');
$tiny1 = 999;
$text1 = 'long';
$tiny2 = -999;
$text2 = 'wide';
expect_true(
    $prepared->bind_param('isis', $tiny1, $text1, $tiny2, $text2),
    'bind warning insert'
);
expect_true($prepared->execute(), 'execute warning insert');

$preparedWarnings = [
    [1264, 'HY000', "Out of range value for column 'tiny_value' at row 1"],
    [1265, 'HY000', "Data truncated for column 'short_text' at row 1"],
    [1264, 'HY000', "Out of range value for column 'tiny_value' at row 2"],
    [1265, 'HY000', "Data truncated for column 'short_text' at row 2"],
];
expect_same(4, $mysqli->warning_count, 'prepared link warning count');
expect_same(
    $preparedWarnings,
    collect_warning_chain($prepared->get_warnings()),
    'prepared object warning chain'
);
expect_same(
    $preparedWarnings,
    collect_warning_chain(mysqli_stmt_get_warnings($prepared)),
    'prepared procedural warning chain'
);

expect_true($prepared->reset(), 'prepared warning reset');
expect_false($prepared->get_warnings(), 'prepared reset warning list');
expect_true($prepared->execute(), 'prepared warning re-execution');
$held = $prepared->get_warnings();
expect_true($held instanceof mysqli_warning, 'held prepared warning');
$prepared->close();
$mysqli->close();
expect_same(
    $preparedWarnings,
    collect_warning_chain($held),
    'held warning chain after statement and connection close'
);

echo "mysqli_warnings: ok\n";
