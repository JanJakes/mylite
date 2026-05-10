#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_replace_values_expectations_$$"
OTHER_DATABASE="${DATABASE}_other"
MISSING_DATABASE="${DATABASE}_missing"

fail() {
    printf '%s\n' "mysql_baseline_replace_values_lifecycle_expectations: $1" >&2
    exit 1
}

run_mysql() {
    sql=$1
    shift
    printf '%s\n' "$sql" \
        | docker exec -i "$MYSQL_CONTAINER" mysql -uroot --batch --raw --skip-column-names "$@"
}

expect_output() {
    label=$1
    expected=$2
    sql=$3
    shift 3

    output=$(run_mysql "$sql" "$@")
    if [ "$output" != "$expected" ]; then
        fail "$label: expected [$expected], got [$output]"
    fi
}

expect_error() {
    label=$1
    code=$2
    state=$3
    message=$4
    sql=$5
    shift 5

    set +e
    output=$(run_mysql "$sql" "$@" 2>&1)
    status=$?
    set -e

    if [ "$status" -eq 0 ]; then
        fail "$label: expected error $code/$state, command succeeded with [$output]"
    fi

    case "$output" in
        *"ERROR $code ($state)"*"$message"*) ;;
        *)
            fail "$label: expected error $code/$state containing [$message], got [$output]"
            ;;
    esac
}

cleanup() {
    run_mysql "DROP DATABASE IF EXISTS ${DATABASE};" >/dev/null 2>&1 || true
    run_mysql "DROP DATABASE IF EXISTS ${OTHER_DATABASE};" >/dev/null 2>&1 || true
    run_mysql "DROP DATABASE IF EXISTS ${MISSING_DATABASE};" >/dev/null 2>&1 || true
}

trap cleanup EXIT

version=$(run_mysql 'SELECT VERSION();')
case "$version" in
    8.4.9*) ;;
    *) fail "expected MySQL 8.4.9 runtime, got [$version]" ;;
esac

cleanup
run_mysql "CREATE DATABASE ${DATABASE}; CREATE DATABASE ${OTHER_DATABASE};" >/dev/null

expect_output \
    "default SQL mode includes strict trans tables" \
    "1" \
    "SELECT @@sql_mode LIKE '%STRICT_TRANS_TABLES%';"

expect_error \
    "replace without selected schema" \
    1046 \
    3D000 \
    "No database selected" \
    "REPLACE INTO no_default_table VALUES (1);"

expect_error \
    "qualified replace unknown schema" \
    1049 \
    42000 \
    "Unknown database '${MISSING_DATABASE}'" \
    "REPLACE INTO ${MISSING_DATABASE}.missing_table VALUES (1);"

run_mysql \
    "CREATE TABLE ${DATABASE}.qualified_numbers (id INT NOT NULL, nn INT NOT NULL);" \
    >/dev/null
expect_output \
    "schema-qualified replace without selected schema" \
    "1	0	1:2" \
    "REPLACE INTO ${DATABASE}.qualified_numbers VALUES (1, 2); "\
"SELECT ROW_COUNT(), @@warning_count, GROUP_CONCAT(CONCAT(id, ':', nn) ORDER BY id) "\
"FROM ${DATABASE}.qualified_numbers;"
run_mysql "DROP TABLE ${DATABASE}.qualified_numbers;" >/dev/null

run_mysql \
    "CREATE TABLE ${DATABASE}.numbers ("\
"id INT NOT NULL, i INT, ii INTEGER, iu INT UNSIGNED, integeru INTEGER UNSIGNED, "\
"b BIGINT, bu BIGINT UNSIGNED, n INT NULL, nn INT NOT NULL);" \
    >/dev/null

expect_error \
    "unknown table" \
    1146 \
    42S02 \
    "Table '${DATABASE}.missing_table' doesn't exist" \
    "REPLACE INTO missing_table VALUES (1);" \
    "$DATABASE"

expect_output \
    "full-row replace status" \
    "1	0	1:2:3:4:5:6:7:NULL:8" \
    "REPLACE INTO numbers VALUES (1, 2, 3, 4, 5, 6, 7, NULL, 8); "\
"SELECT ROW_COUNT(), @@warning_count, "\
"GROUP_CONCAT(CONCAT(id, ':', IFNULL(i, 'NULL'), ':', IFNULL(ii, 'NULL'), ':', "\
"IFNULL(iu, 'NULL'), ':', IFNULL(integeru, 'NULL'), ':', IFNULL(b, 'NULL'), ':', "\
"IFNULL(bu, 'NULL'), ':', IFNULL(n, 'NULL'), ':', nn) ORDER BY id, nn) FROM numbers;" \
    "$DATABASE"

expect_output \
    "replace without into inserts another no-key row" \
    "1	0	2	1:-3:-4,1:2:8" \
    "REPLACE numbers (id, i, nn) VALUES (1, -3, -4); "\
"SELECT ROW_COUNT(), @@warning_count, COUNT(*), "\
"GROUP_CONCAT(CONCAT(id, ':', IFNULL(i, 'NULL'), ':', nn) ORDER BY id, nn) FROM numbers;" \
    "$DATABASE"

expect_output \
    "omitted nullable columns become null" \
    "1	0	3:NULL:9" \
    "REPLACE INTO numbers (id, nn) VALUES (3, +9); "\
"SELECT ROW_COUNT(), @@warning_count, CONCAT(id, ':', IFNULL(n, 'NULL'), ':', nn) "\
"FROM numbers WHERE id = 3;" \
    "$DATABASE"

expect_output \
    "multi-row replace status" \
    "2	0	2	4:10,5:11" \
    "REPLACE INTO numbers (id, nn) VALUES (4, 10), (5, 11); "\
"SELECT ROW_COUNT(), @@warning_count, COUNT(*), "\
"GROUP_CONCAT(CONCAT(id, ':', nn) ORDER BY id) FROM numbers WHERE id IN (4, 5);" \
    "$DATABASE"

expect_output \
    "boolean replace values" \
    "1	0	6:1:0" \
    "REPLACE INTO numbers (id, i, nn) VALUES (6, TRUE, FALSE); "\
"SELECT ROW_COUNT(), @@warning_count, CONCAT(id, ':', i, ':', nn) FROM numbers WHERE id = 6;" \
    "$DATABASE"

expect_output \
    "signed and unsigned physical boundaries" \
    "1	0	7:-2147483648:2147483647:0:4294967295:-9223372036854775808:9223372036854775807" \
    "REPLACE INTO numbers (id, i, ii, iu, integeru, b, bu, nn) VALUES "\
"(7, -2147483648, 2147483647, 0, 4294967295, -9223372036854775808, "\
"9223372036854775807, 12); "\
"SELECT ROW_COUNT(), @@warning_count, CONCAT(id, ':', i, ':', ii, ':', iu, ':', "\
"integeru, ':', b, ':', bu) FROM numbers WHERE id = 7;" \
    "$DATABASE"

expect_error \
    "duplicate column target" \
    1110 \
    42000 \
    "Column 'id' specified twice" \
    "REPLACE INTO numbers (id, id, nn) VALUES (8, 9, 10);" \
    "$DATABASE"

expect_error \
    "unknown column target" \
    1054 \
    42S22 \
    "Unknown column 'missing' in 'field list'" \
    "REPLACE INTO numbers (id, missing, nn) VALUES (8, 9, 10);" \
    "$DATABASE"

expect_error \
    "too few values" \
    1136 \
    21S01 \
    "Column count doesn't match value count at row 1" \
    "REPLACE INTO numbers VALUES (8);" \
    "$DATABASE"

expect_error \
    "too many explicit values" \
    1136 \
    21S01 \
    "Column count doesn't match value count at row 1" \
    "REPLACE INTO numbers (id, nn) VALUES (8, 9, 10);" \
    "$DATABASE"

expect_error \
    "omitted not null column" \
    1364 \
    HY000 \
    "Field 'nn' doesn't have a default value" \
    "REPLACE INTO numbers (id) VALUES (8);" \
    "$DATABASE"

expect_error \
    "null into not null column" \
    1048 \
    23000 \
    "Column 'id' cannot be null" \
    "REPLACE INTO numbers (id, nn) VALUES (NULL, 10);" \
    "$DATABASE"

expect_error \
    "signed int above range" \
    1264 \
    22003 \
    "Out of range value for column 'i' at row 1" \
    "REPLACE INTO numbers (id, i, nn) VALUES (8, 2147483648, 10);" \
    "$DATABASE"

expect_error \
    "unsigned int below range" \
    1264 \
    22003 \
    "Out of range value for column 'iu' at row 1" \
    "REPLACE INTO numbers (id, iu, nn) VALUES (8, -1, 10);" \
    "$DATABASE"

expect_error \
    "signed bigint below range" \
    1264 \
    22003 \
    "Out of range value for column 'b' at row 1" \
    "REPLACE INTO numbers (id, b, nn) VALUES (8, -9223372036854775809, 10);" \
    "$DATABASE"

run_mysql "CREATE TABLE ${DATABASE}.no_key (id INT, v INT);" >/dev/null
expect_output \
    "replace without key remains insert equivalent" \
    "1	0	2	1:10,1:20" \
    "REPLACE INTO no_key VALUES (1, 10); REPLACE INTO no_key VALUES (1, 20); "\
"SELECT ROW_COUNT(), @@warning_count, COUNT(*), GROUP_CONCAT(CONCAT(id, ':', v) ORDER BY v) "\
"FROM no_key;" \
    "$DATABASE"

run_mysql "CREATE TABLE ${DATABASE}.keyed (id INT PRIMARY KEY, v INT);" >/dev/null
expect_output \
    "mysql primary key replace deletes and inserts" \
    "2	0	1	1:20" \
    "REPLACE INTO keyed VALUES (1, 10); REPLACE INTO keyed VALUES (1, 20); "\
"SELECT ROW_COUNT(), @@warning_count, COUNT(*), GROUP_CONCAT(CONCAT(id, ':', v)) FROM keyed;" \
    "$DATABASE"

run_mysql \
    "CREATE TABLE ${DATABASE}.wider (id INT NOT NULL, i INT DEFAULT 99, nn INT NOT NULL DEFAULT 7);" \
    >/dev/null
expect_output \
    "mysql accepts wider replace syntax upstream" \
    "5	1	1:2:3,2:4:5,3:14:7,4:15:16,5:17:18" \
    "REPLACE INTO wider VALUE (1, 2, 3); "\
"REPLACE INTO wider VALUES ROW(2, 4, 5); "\
"REPLACE INTO wider SET id = 3, i = 10 + 4, nn = DEFAULT; "\
"REPLACE LOW_PRIORITY INTO wider VALUES (4, 15, 16); "\
"REPLACE DELAYED INTO wider VALUES (5, 17, 18); "\
"SELECT COUNT(*), @@warning_count, GROUP_CONCAT(CONCAT(id, ':', i, ':', nn) ORDER BY id) "\
"FROM wider;" \
    "$DATABASE"
