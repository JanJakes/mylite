#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_function_specific_result_metadata_$$"

fail() {
    printf '%s\n' "mysql_function_specific_result_metadata_expectations: $1" >&2
    exit 1
}

run_mysql() {
    sql=$1
    shift
    printf '%s\n' "$sql" \
        | docker exec -i "$MYSQL_CONTAINER" mysql --protocol=TCP -h127.0.0.1 -uroot \
            --batch --raw --skip-column-names --default-character-set=utf8mb4 "$@"
}

run_mysql_type_info() {
    sql=$1
    shift
    printf '%s\n' "$sql" \
        | docker exec -i "$MYSQL_CONTAINER" mysql --protocol=TCP -h127.0.0.1 -uroot \
            --default-character-set=utf8mb4 --column-type-info -vvv "$@"
}

expect_contains() {
    label=$1
    needle=$2
    haystack=$3

    case "$haystack" in
        *"$needle"*) ;;
        *) fail "$label: expected output to contain [$needle]" ;;
    esac
}

expect_not_contains() {
    label=$1
    needle=$2
    haystack=$3

    case "$haystack" in
        *"$needle"*) fail "$label: output unexpectedly contained [$needle]" ;;
        *) ;;
    esac
}

field_block() {
    field=$1
    output=$2

    printf '%s\n' "$output" | awk -v marker="\`$field\`" '
        /^Field +[0-9]+:/ {
            if (active) {
                exit
            }
            active = index($0, marker) != 0
        }
        active {
            print
        }
    '
}

expect_field_contains() {
    label=$1
    field=$2
    needle=$3
    output=$4
    block=$(field_block "$field" "$output")

    [ -n "$block" ] || fail "$label: field [$field] was not present"
    expect_contains "$label" "$needle" "$block"
}

expect_field_not_contains() {
    label=$1
    field=$2
    needle=$3
    output=$4
    block=$(field_block "$field" "$output")

    [ -n "$block" ] || fail "$label: field [$field] was not present"
    expect_not_contains "$label" "$needle" "$block"
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

cleanup
run_mysql \
    "CREATE DATABASE ${DATABASE}
         DEFAULT CHARACTER SET utf8mb4 COLLATE utf8mb4_0900_ai_ci;
     CREATE TABLE ${DATABASE}.metadata_values (
         tiny_value TINYINT,
         small_value SMALLINT,
         medium_value MEDIUMINT,
         int_value INT NOT NULL,
         uint_value INT UNSIGNED,
         bigint_value BIGINT,
         ubigint_value BIGINT UNSIGNED,
         string_value VARCHAR(10) NOT NULL,
         nullable_string VARCHAR(20),
         datetime_value DATETIME(3),
         temporal_text VARCHAR(30),
         geometry_value GEOMETRY
     );
     INSERT INTO ${DATABASE}.metadata_values VALUES
         (1, 2, 3, 4, 5, 6, 7, 'alpha', 'first',
          '2024-01-01 00:00:00.123', '2024-01-01 00:00:00.1', POINT(1,2)),
         (8, 9, 10, 11, 12, 13, 14, 'beta', NULL,
          '2024-01-02 00:00:00.456', '2024-01-02 00:00:00', POINT(3,4));" \
    >/dev/null

spatial_output=$(run_mysql_type_info \
    "USE ${DATABASE}; SET NAMES utf8mb4;
     SELECT
         ST_AsText(POINT(1,2)) AS spatial_text,
         ST_AsBinary(POINT(1,2)) AS spatial_binary,
         ST_GeometryType(POINT(1,2)) AS geometry_type,
         ST_GeoHash(1,2,10) AS geohash_value,
         ST_AsGeoJSON(POINT(1,2)) AS geojson_value,
         ST_GeomFromText('POINT(1 2)') AS geometry_value,
         ST_Buffer_Strategy('end_round',32) AS buffer_strategy,
         ST_SRID(POINT(1,2)) AS srid_value,
         ST_Intersects(POINT(1,2),POINT(1,2)) AS predicate_value,
         ST_X(POINT(1,2)) AS coordinate_value;" \
    "$DATABASE")

expect_field_contains "spatial text type" spatial_text \
    'Type:       LONG_BLOB' "$spatial_output"
expect_field_contains "spatial text collation" spatial_text \
    'Collation:  utf8mb4_0900_ai_ci (255)' "$spatial_output"
expect_field_contains "spatial text length" spatial_text \
    'Length:     268435456' "$spatial_output"
expect_field_contains "spatial text decimals" spatial_text \
    'Decimals:   31' "$spatial_output"
expect_field_not_contains "spatial text is nonbinary" spatial_text \
    'Flags:      BINARY' "$spatial_output"

expect_field_contains "spatial binary type" spatial_binary \
    'Type:       LONG_BLOB' "$spatial_output"
expect_field_contains "spatial binary collation" spatial_binary \
    'Collation:  binary (63)' "$spatial_output"
expect_field_contains "spatial binary length" spatial_binary \
    'Length:     4294967295' "$spatial_output"
expect_field_contains "spatial binary flags" spatial_binary \
    'Flags:      BINARY ' "$spatial_output"
expect_field_not_contains "spatial binary is not blob-flagged" spatial_binary \
    'Flags:      BLOB' "$spatial_output"

expect_field_contains "geometry type protocol type" geometry_type \
    'Type:       VAR_STRING' "$spatial_output"
expect_field_contains "geometry type length" geometry_type \
    'Length:     60' "$spatial_output"
expect_field_contains "geohash type" geohash_value \
    'Type:       VAR_STRING' "$spatial_output"
expect_field_contains "geohash length" geohash_value \
    'Length:     400' "$spatial_output"
expect_field_contains "geojson type" geojson_value \
    'Type:       JSON' "$spatial_output"
expect_field_contains "geojson length" geojson_value \
    'Length:     4294967292' "$spatial_output"
expect_field_contains "geometry result type" geometry_value \
    'Type:       GEOMETRY' "$spatial_output"
expect_field_contains "geometry result decimals" geometry_value \
    'Decimals:   0' "$spatial_output"
expect_field_contains "geometry result flags" geometry_value \
    'Flags:      BINARY ' "$spatial_output"
expect_field_not_contains "geometry result is not blob-flagged" geometry_value \
    'Flags:      BLOB' "$spatial_output"
expect_field_contains "buffer strategy binary string" buffer_strategy \
    'Collation:  binary (63)' "$spatial_output"
expect_field_contains "buffer strategy length" buffer_strategy \
    'Length:     16' "$spatial_output"
expect_field_contains "spatial property length" srid_value \
    'Length:     10' "$spatial_output"
expect_field_contains "spatial property signed flags" srid_value \
    'Flags:      BINARY NUM ' "$spatial_output"
expect_field_not_contains "spatial property is signed" srid_value \
    'UNSIGNED' "$spatial_output"
expect_field_contains "spatial predicate length" predicate_value \
    'Length:     1' "$spatial_output"
expect_field_contains "spatial predicate flags" predicate_value \
    'Flags:      BINARY NUM ' "$spatial_output"
expect_field_contains "spatial double type" coordinate_value \
    'Type:       DOUBLE' "$spatial_output"
expect_field_contains "spatial double length" coordinate_value \
    'Length:     23' "$spatial_output"

convert_tz_output=$(run_mysql_type_info \
    "USE ${DATABASE}; SET NAMES utf8mb4;
     SELECT
         CONVERT_TZ('2024-01-01 00:00:00','+00:00','+01:00') AS tz_whole,
         CONVERT_TZ('2024-01-01 00:00:00.123','+00:00','+01:00') AS tz_fraction,
         CONVERT_TZ(datetime_value,'+00:00','+01:00') AS tz_datetime,
         CONVERT_TZ(temporal_text,'+00:00','+01:00') AS tz_text
     FROM metadata_values LIMIT 1;" \
    "$DATABASE")

for field in tz_whole tz_fraction tz_datetime tz_text; do
    expect_field_contains "$field type" "$field" \
        'Type:       DATETIME' "$convert_tz_output"
    expect_field_contains "$field binary collation" "$field" \
        'Collation:  binary (63)' "$convert_tz_output"
    expect_field_contains "$field flags" "$field" \
        'Flags:      BINARY ' "$convert_tz_output"
done
expect_field_contains "whole-second convert tz length" tz_whole \
    'Length:     19' "$convert_tz_output"
expect_field_contains "whole-second convert tz decimals" tz_whole \
    'Decimals:   0' "$convert_tz_output"
expect_field_contains "literal convert tz length" tz_fraction \
    'Length:     23' "$convert_tz_output"
expect_field_contains "literal convert tz decimals" tz_fraction \
    'Decimals:   3' "$convert_tz_output"
expect_field_contains "descriptor convert tz length" tz_datetime \
    'Length:     23' "$convert_tz_output"
expect_field_contains "descriptor convert tz decimals" tz_datetime \
    'Decimals:   3' "$convert_tz_output"
expect_field_contains "text convert tz length" tz_text \
    'Length:     26' "$convert_tz_output"
expect_field_contains "text convert tz decimals" tz_text \
    'Decimals:   6' "$convert_tz_output"

aggregate_output=$(run_mysql_type_info \
    "USE ${DATABASE}; SET NAMES utf8mb4;
     SELECT
         COUNT(*) AS count_value,
         SUM(tiny_value) AS sum_tiny,
         SUM(int_value) AS sum_int,
         SUM(ubigint_value) AS sum_ubigint,
         AVG(int_value) AS avg_int,
         MIN(string_value) AS min_string,
         MAX(uint_value) AS max_uint,
         ANY_VALUE(string_value) AS any_string,
         BIT_OR(int_value) AS bit_value,
         STDDEV_POP(int_value) AS stddev_value,
         JSON_ARRAYAGG(int_value) AS json_value,
         ST_Collect(geometry_value) AS collect_value
     FROM metadata_values;" \
    "$DATABASE")

expect_field_contains "count type" count_value \
    'Type:       LONGLONG' "$aggregate_output"
expect_field_contains "count length" count_value \
    'Length:     21' "$aggregate_output"
expect_field_contains "count flags" count_value \
    'Flags:      NOT_NULL BINARY NUM ' "$aggregate_output"
expect_field_contains "tiny sum length" sum_tiny \
    'Length:     26' "$aggregate_output"
expect_field_contains "int sum type" sum_int \
    'Type:       NEWDECIMAL' "$aggregate_output"
expect_field_contains "int sum length" sum_int \
    'Length:     33' "$aggregate_output"
expect_field_contains "unsigned bigint sum length" sum_ubigint \
    'Length:     43' "$aggregate_output"
expect_field_contains "int average length" avg_int \
    'Length:     16' "$aggregate_output"
expect_field_contains "int average decimals" avg_int \
    'Decimals:   4' "$aggregate_output"
expect_field_contains "minimum string type" min_string \
    'Type:       VAR_STRING' "$aggregate_output"
expect_field_contains "minimum string collation" min_string \
    'Collation:  utf8mb4_0900_ai_ci (255)' "$aggregate_output"
expect_field_contains "minimum string length" min_string \
    'Length:     40' "$aggregate_output"
expect_field_contains "minimum string decimals" min_string \
    'Decimals:   31' "$aggregate_output"
expect_field_not_contains "minimum string clears source constraint" min_string \
    'NOT_NULL' "$aggregate_output"
expect_field_contains "maximum uint source type" max_uint \
    'Type:       LONG' "$aggregate_output"
expect_field_contains "maximum uint length" max_uint \
    'Length:     10' "$aggregate_output"
expect_field_contains "maximum uint signedness" max_uint \
    'Flags:      UNSIGNED BINARY NUM ' "$aggregate_output"
expect_field_contains "any value string type" any_string \
    'Type:       VAR_STRING' "$aggregate_output"
expect_field_contains "bit aggregate type" bit_value \
    'Type:       LONGLONG' "$aggregate_output"
expect_field_contains "bit aggregate flags" bit_value \
    'Flags:      NOT_NULL UNSIGNED BINARY NUM ' "$aggregate_output"
expect_field_contains "statistical aggregate type" stddev_value \
    'Type:       DOUBLE' "$aggregate_output"
expect_field_contains "JSON aggregate type" json_value \
    'Type:       JSON' "$aggregate_output"
expect_field_contains "JSON aggregate length" json_value \
    'Length:     4294967292' "$aggregate_output"
expect_field_contains "spatial aggregate type" collect_value \
    'Type:       GEOMETRY' "$aggregate_output"
expect_field_contains "spatial aggregate length" collect_value \
    'Length:     16777216' "$aggregate_output"
expect_field_contains "spatial aggregate flags" collect_value \
    'Flags:      BINARY ' "$aggregate_output"
expect_field_not_contains "spatial aggregate is not blob-flagged" collect_value \
    'Flags:      BLOB' "$aggregate_output"

group_concat_output=$(run_mysql_type_info \
    "USE ${DATABASE}; SET NAMES utf8mb4;
     SET SESSION group_concat_max_len=512;
     SELECT GROUP_CONCAT(string_value) AS concat_512 FROM metadata_values;
     SET SESSION group_concat_max_len=513;
     SELECT GROUP_CONCAT(string_value) AS concat_513 FROM metadata_values;
     SET SESSION group_concat_max_len=1024;
     SELECT GROUP_CONCAT(string_value) AS concat_default FROM metadata_values;" \
    "$DATABASE")

expect_field_contains "small group concat type" concat_512 \
    'Type:       VAR_STRING' "$group_concat_output"
expect_field_contains "small group concat length" concat_512 \
    'Length:     2048' "$group_concat_output"
expect_field_contains "large group concat type" concat_513 \
    'Type:       LONG_BLOB' "$group_concat_output"
expect_field_contains "large group concat length" concat_513 \
    'Length:     32832' "$group_concat_output"
expect_field_contains "default group concat type" concat_default \
    'Type:       LONG_BLOB' "$group_concat_output"
expect_field_contains "default group concat length" concat_default \
    'Length:     65536' "$group_concat_output"

grouped_output=$(run_mysql_type_info \
    "USE ${DATABASE}; SET NAMES utf8mb4;
     SELECT string_value AS group_string, SUM(int_value) AS grouped_sum
     FROM metadata_values GROUP BY string_value ORDER BY string_value LIMIT 1;" \
    "$DATABASE")

expect_field_contains "group key database" group_string \
    "Database:   \`${DATABASE}\`" "$grouped_output"
expect_field_contains "group key selected table" group_string \
    "Table:      \`metadata_values\`" "$grouped_output"
expect_field_contains "group key origin table" group_string \
    "Org_table:  \`metadata_values\`" "$grouped_output"
expect_field_contains "group key length" group_string \
    'Length:     40' "$grouped_output"
expect_field_contains "grouped aggregate type" grouped_sum \
    'Type:       NEWDECIMAL' "$grouped_output"
expect_field_contains "grouped aggregate length" grouped_sum \
    'Length:     33' "$grouped_output"
expect_field_contains "grouped aggregate empty origin table" grouped_sum \
    'Org_table:  ``' "$grouped_output"

window_output=$(run_mysql_type_info \
    "USE ${DATABASE}; SET NAMES utf8mb4;
     SELECT
         ROW_NUMBER() OVER window_spec AS row_number_value,
         CUME_DIST() OVER window_spec AS distribution_value,
         COUNT(*) OVER window_spec AS count_window,
         SUM(int_value) OVER window_spec AS sum_window,
         AVG(int_value) OVER window_spec AS avg_window,
         MIN(int_value) OVER window_spec AS min_int_window,
         MIN(string_value) OVER window_spec AS min_string_window,
         LAG(int_value) OVER window_spec AS lag_int_window,
         LAG(string_value) OVER window_spec AS lag_string_window,
         BIT_OR(int_value) OVER window_spec AS bit_window,
         STDDEV_POP(int_value) OVER window_spec AS stddev_window,
         JSON_ARRAYAGG(int_value) OVER window_spec AS json_window
     FROM metadata_values
     WINDOW window_spec AS (
         ORDER BY int_value ROWS BETWEEN UNBOUNDED PRECEDING AND CURRENT ROW
     )
     LIMIT 1;" \
    "$DATABASE")

expect_field_contains "row number type" row_number_value \
    'Type:       LONGLONG' "$window_output"
expect_field_contains "row number flags" row_number_value \
    'Flags:      NOT_NULL UNSIGNED NUM ' "$window_output"
expect_field_not_contains "row number is not binary" row_number_value \
    'BINARY' "$window_output"
expect_field_contains "distribution type" distribution_value \
    'Type:       DOUBLE' "$window_output"
expect_field_contains "distribution flags" distribution_value \
    'Flags:      NOT_NULL NUM ' "$window_output"
expect_field_contains "window count flags" count_window \
    'Flags:      NOT_NULL NUM ' "$window_output"
expect_field_contains "window sum type" sum_window \
    'Type:       NEWDECIMAL' "$window_output"
expect_field_contains "window sum length" sum_window \
    'Length:     33' "$window_output"
expect_field_contains "window sum flags" sum_window \
    'Flags:      NUM ' "$window_output"
expect_field_not_contains "window sum is not binary" sum_window \
    'BINARY' "$window_output"
expect_field_contains "window average length" avg_window \
    'Length:     16' "$window_output"
expect_field_contains "window average decimals" avg_window \
    'Decimals:   4' "$window_output"
expect_field_contains "window integer minimum type" min_int_window \
    'Type:       LONGLONG' "$window_output"
expect_field_contains "window integer minimum length" min_int_window \
    'Length:     11' "$window_output"
expect_field_contains "window string minimum type" min_string_window \
    'Type:       VAR_STRING' "$window_output"
expect_field_contains "window string minimum length" min_string_window \
    'Length:     40' "$window_output"
expect_field_contains "window string minimum decimals" min_string_window \
    'Decimals:   0' "$window_output"
expect_field_contains "window integer navigation type" lag_int_window \
    'Type:       LONGLONG' "$window_output"
expect_field_contains "window integer navigation length" lag_int_window \
    'Length:     11' "$window_output"
expect_field_contains "window string navigation type" lag_string_window \
    'Type:       VAR_STRING' "$window_output"
expect_field_contains "window bit flags" bit_window \
    'Flags:      NOT_NULL UNSIGNED NUM ' "$window_output"
expect_field_contains "window statistics flags" stddev_window \
    'Flags:      NUM ' "$window_output"
expect_field_contains "window JSON type" json_window \
    'Type:       JSON' "$window_output"
expect_field_contains "window JSON collation" json_window \
    'Collation:  binary (63)' "$window_output"
expect_field_contains "window JSON length" json_window \
    'Length:     4294967295' "$window_output"
expect_field_contains "window JSON decimals" json_window \
    'Decimals:   0' "$window_output"
expect_field_contains "window JSON flags" json_window \
    'Flags:      BLOB BINARY ' "$window_output"

command -v php >/dev/null 2>&1 || fail "php is required for API expectations"
MYSQL_EXPECTATION_HOST=$(
    docker inspect -f '{{range .NetworkSettings.Networks}}{{.IPAddress}}{{end}}' \
        "$MYSQL_CONTAINER"
)
[ -n "$MYSQL_EXPECTATION_HOST" ] || fail "could not resolve MySQL container address"
export MYSQL_EXPECTATION_HOST
export MYSQL_EXPECTATION_DATABASE="$DATABASE"

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

function mysqli_field_shape(object $field): array
{
    return [
        'name' => $field->name,
        'orgname' => $field->orgname,
        'table' => $field->table,
        'orgtable' => $field->orgtable,
        'db' => $field->db,
        'type' => $field->type,
        'charsetnr' => $field->charsetnr,
        'length' => $field->length,
        'decimals' => $field->decimals,
        'flags' => $field->flags,
    ];
}

function pdo_field_shape(array $field): array
{
    return [
        'native_type' => $field['native_type'],
        'pdo_type' => $field['pdo_type'],
        'flags' => $field['flags'],
        'table' => $field['table'],
        'name' => $field['name'],
        'len' => $field['len'],
        'precision' => $field['precision'],
    ];
}

$host = getenv('MYSQL_EXPECTATION_HOST');
$database = getenv('MYSQL_EXPECTATION_DATABASE');
if ($host === false || $database === false) {
    throw new RuntimeException('missing MySQL expectation environment');
}

$sql =
    "SELECT " .
    "ST_AsText(POINT(1,2)) AS astext, " .
    "ST_GeometryType(POINT(1,2)) AS geometry_type, " .
    "ST_Intersects(POINT(1,2),POINT(1,2)) AS predicate_value, " .
    "CONVERT_TZ('2024-01-01 00:00:00.123','+00:00','+01:00') AS tz_value, " .
    "SUM(int_value) AS sum_value, MIN(string_value) AS min_value " .
    "FROM metadata_values";

$expectedMysqli = [
    ['name' => 'astext', 'orgname' => '', 'table' => '', 'orgtable' => '',
     'db' => '', 'type' => 251, 'charsetnr' => 255, 'length' => 268435456,
     'decimals' => 31, 'flags' => 0],
    ['name' => 'geometry_type', 'orgname' => '', 'table' => '', 'orgtable' => '',
     'db' => '', 'type' => 253, 'charsetnr' => 255, 'length' => 60,
     'decimals' => 31, 'flags' => 0],
    ['name' => 'predicate_value', 'orgname' => '', 'table' => '', 'orgtable' => '',
     'db' => '', 'type' => 8, 'charsetnr' => 63, 'length' => 1,
     'decimals' => 0, 'flags' => 32896],
    ['name' => 'tz_value', 'orgname' => '', 'table' => '', 'orgtable' => '',
     'db' => '', 'type' => 12, 'charsetnr' => 63, 'length' => 23,
     'decimals' => 3, 'flags' => 128],
    ['name' => 'sum_value', 'orgname' => '', 'table' => '', 'orgtable' => '',
     'db' => '', 'type' => 246, 'charsetnr' => 63, 'length' => 33,
     'decimals' => 0, 'flags' => 128],
    ['name' => 'min_value', 'orgname' => '', 'table' => '', 'orgtable' => '',
     'db' => '', 'type' => 253, 'charsetnr' => 255, 'length' => 40,
     'decimals' => 31, 'flags' => 0],
];

$mysqli = new mysqli($host, 'root', '', $database);
$directFields = array_map(
    'mysqli_field_shape',
    $mysqli->query($sql)->fetch_fields()
);
expect_same($expectedMysqli, $directFields, 'mysqli direct field metadata');

$statement = $mysqli->prepare($sql);
expect_same(true, $statement->execute(), 'mysqli prepared execute');
$preparedFields = array_map(
    'mysqli_field_shape',
    $statement->get_result()->fetch_fields()
);
expect_same($expectedMysqli, $preparedFields, 'mysqli prepared field metadata');
expect_same($directFields, $preparedFields, 'mysqli direct/prepared parity');

$groupSql =
    "SELECT string_value AS group_string, SUM(int_value) AS grouped_sum " .
    "FROM metadata_values GROUP BY string_value ORDER BY string_value LIMIT 1";
$expectedGroupedMysqli = [
    ['name' => 'group_string', 'orgname' => 'string_value',
     'table' => 'metadata_values', 'orgtable' => 'metadata_values',
     'db' => $database, 'type' => 253, 'charsetnr' => 255, 'length' => 40,
     'decimals' => 0, 'flags' => 4097],
    ['name' => 'grouped_sum', 'orgname' => '', 'table' => '', 'orgtable' => '',
     'db' => '', 'type' => 246, 'charsetnr' => 63, 'length' => 33,
     'decimals' => 0, 'flags' => 0],
];
$groupFields = array_map(
    'mysqli_field_shape',
    $mysqli->query($groupSql)->fetch_fields()
);
expect_same($expectedGroupedMysqli, $groupFields, 'mysqli grouped origin metadata');

$pdo = new PDO(
    "mysql:host={$host};dbname={$database};charset=utf8mb4",
    'root',
    '',
    [
        PDO::ATTR_EMULATE_PREPARES => false,
        PDO::ATTR_ERRMODE => PDO::ERRMODE_EXCEPTION,
    ]
);

$expectedPdo = [
    ['native_type' => 'LONG_BLOB', 'pdo_type' => PDO::PARAM_STR, 'flags' => [],
     'table' => '', 'name' => 'astext', 'len' => 268435456, 'precision' => 31],
    ['native_type' => 'VAR_STRING', 'pdo_type' => PDO::PARAM_STR, 'flags' => [],
     'table' => '', 'name' => 'geometry_type', 'len' => 60, 'precision' => 31],
    ['native_type' => 'LONGLONG', 'pdo_type' => PDO::PARAM_INT, 'flags' => [],
     'table' => '', 'name' => 'predicate_value', 'len' => 1, 'precision' => 0],
    ['native_type' => 'DATETIME', 'pdo_type' => PDO::PARAM_STR, 'flags' => [],
     'table' => '', 'name' => 'tz_value', 'len' => 23, 'precision' => 3],
    ['native_type' => 'NEWDECIMAL', 'pdo_type' => PDO::PARAM_STR, 'flags' => [],
     'table' => '', 'name' => 'sum_value', 'len' => 33, 'precision' => 0],
    ['native_type' => 'VAR_STRING', 'pdo_type' => PDO::PARAM_STR, 'flags' => [],
     'table' => '', 'name' => 'min_value', 'len' => 40, 'precision' => 31],
];

$direct = $pdo->query($sql);
$directPdo = [];
for ($index = 0; $index < count($expectedPdo); ++$index) {
    $directPdo[] = pdo_field_shape($direct->getColumnMeta($index));
}
expect_same($expectedPdo, $directPdo, 'PDO direct field metadata');

$prepared = $pdo->prepare($sql);
expect_same(true, $prepared->execute(), 'PDO prepared execute');
$preparedPdo = [];
for ($index = 0; $index < count($expectedPdo); ++$index) {
    $preparedPdo[] = pdo_field_shape($prepared->getColumnMeta($index));
}
expect_same($expectedPdo, $preparedPdo, 'PDO prepared field metadata');
expect_same($directPdo, $preparedPdo, 'PDO direct/prepared parity');

echo "mysql_function_specific_result_metadata_expectations: php ok\n";
PHP

printf '%s\n' "mysql_function_specific_result_metadata_expectations: ok"
