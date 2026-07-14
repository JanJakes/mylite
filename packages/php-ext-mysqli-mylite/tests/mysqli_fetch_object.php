<?php

require __DIR__ . '/bootstrap.php';

final class MyliteFetchObjectRow
{
}

$mysqli = open_mylite_mysqli();
expect_true(
    $mysqli->query('CREATE TABLE object_rows (id INT NOT NULL, label VARCHAR(20) NOT NULL)'),
    'create object rows table'
);
expect_true(
    $mysqli->query("INSERT INTO object_rows VALUES (1, 'first'), (2, 'second')"),
    'insert object rows'
);

$result = $mysqli->query('SELECT id, label FROM object_rows ORDER BY id');
if (!$result instanceof mysqli_result) {
    throw new RuntimeException('method fetch object result type');
}
$row = $result->fetch_object();
expect_same(stdClass::class, $row::class, 'method default object class');
expect_same(['id' => '1', 'label' => 'first'], get_object_vars($row), 'method default properties');
expect_same(
    ['id' => '2', 'label' => 'second'],
    get_object_vars($result->fetch_object()),
    'method second default properties'
);
expect_same(null, $result->fetch_object(), 'method default result exhausted');

$duplicate = $mysqli->query('SELECT id AS value, label AS value FROM object_rows WHERE id = 1');
if (!$duplicate instanceof mysqli_result) {
    throw new RuntimeException('duplicate property result type');
}
expect_same(
    ['value' => 'first'],
    get_object_vars($duplicate->fetch_object()),
    'default object duplicate property uses last column'
);

$custom = $mysqli->query('SELECT id, label FROM object_rows WHERE id = 1');
if (!$custom instanceof mysqli_result) {
    throw new RuntimeException('method custom object result type');
}
$custom_row = $custom->fetch_object(MyliteFetchObjectRow::class);
expect_true($custom_row instanceof MyliteFetchObjectRow, 'method custom object class');
expect_same(['id' => '1', 'label' => 'first'], get_object_vars($custom_row), 'method custom properties');

$procedural = mysqli_query($mysqli, 'SELECT id, label FROM object_rows WHERE id = 2');
if (!$procedural instanceof mysqli_result) {
    throw new RuntimeException('procedural default object result type');
}
$procedural_row = mysqli_fetch_object($procedural);
expect_same(stdClass::class, $procedural_row::class, 'procedural default object class');
expect_same(
    ['id' => '2', 'label' => 'second'],
    get_object_vars($procedural_row),
    'procedural default properties'
);

$procedural_custom = mysqli_query($mysqli, 'SELECT id, label FROM object_rows WHERE id = 2');
if (!$procedural_custom instanceof mysqli_result) {
    throw new RuntimeException('procedural custom object result type');
}
$procedural_custom_row = mysqli_fetch_object($procedural_custom, MyliteFetchObjectRow::class);
expect_true(
    $procedural_custom_row instanceof MyliteFetchObjectRow,
    'procedural custom object class'
);
expect_same(
    ['id' => '2', 'label' => 'second'],
    get_object_vars($procedural_custom_row),
    'procedural custom properties'
);

$stream = $mysqli->query('SELECT id, label FROM object_rows ORDER BY id', MYSQLI_USE_RESULT);
if (!$stream instanceof mysqli_result) {
    throw new RuntimeException('streaming default object result type');
}
expect_same(
    ['id' => '1', 'label' => 'first'],
    get_object_vars($stream->fetch_object()),
    'streaming default first properties'
);
expect_same(
    ['id' => '2', 'label' => 'second'],
    get_object_vars(mysqli_fetch_object($stream)),
    'streaming default second properties'
);
expect_same(null, $stream->fetch_object(), 'streaming default result exhausted');

$mysqli->close();
