#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_insert_set_expectations_$$"
OTHER_DATABASE="${DATABASE}_other"
MISSING_DATABASE="${DATABASE}_missing"

fail() {
    printf '%s\n' "mysql_baseline_insert_set_lifecycle_expectations: $1" >&2
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
    status_code=$?
    set -e

    if [ "$status_code" -eq 0 ]; then
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
    "insert set without selected schema" \
    1046 \
    3D000 \
    "No database selected" \
    "INSERT INTO no_default_table SET id = 1;"

expect_error \
    "qualified insert set unknown schema" \
    1049 \
    42000 \
    "Unknown database '${MISSING_DATABASE}'" \
    "INSERT INTO ${MISSING_DATABASE}.missing_table SET id = 1;"

run_mysql \
    "CREATE TABLE ${DATABASE}.qualified_numbers (id INT NOT NULL, nn INT NOT NULL);" >/dev/null
expect_output \
    "schema-qualified insert set without selected schema" \
    "1	0	1:2" \
    "INSERT INTO ${DATABASE}.qualified_numbers SET id = 1, nn = 2; "\
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
    "INSERT INTO missing_table SET id = 1;" \
    "$DATABASE"

expect_output \
    "insert into set full assignment status" \
    "1	0	1:2:3:4:5:6:7:NULL:8" \
    "INSERT INTO numbers SET id = 1, i = 2, ii = 3, iu = 4, integeru = 5, "\
"b = 6, bu = 7, n = NULL, nn = 8; "\
"SELECT ROW_COUNT(), @@warning_count, "\
"GROUP_CONCAT(CONCAT(id, ':', IFNULL(i, 'NULL'), ':', IFNULL(ii, 'NULL'), ':', "\
"IFNULL(iu, 'NULL'), ':', IFNULL(integeru, 'NULL'), ':', IFNULL(b, 'NULL'), ':', "\
"IFNULL(bu, 'NULL'), ':', IFNULL(n, 'NULL'), ':', nn) ORDER BY id) FROM numbers;" \
    "$DATABASE"

expect_output \
    "insert set without into and signed values" \
    "1	0	1:2:8,2:-3:-4" \
    "INSERT numbers SET id = 2, i = -3, nn = -4; "\
"SELECT ROW_COUNT(), @@warning_count, GROUP_CONCAT(CONCAT(id, ':', IFNULL(i, 'NULL'), ':', nn) "\
"ORDER BY id) FROM numbers;" \
    "$DATABASE"

expect_output \
    "omitted nullable columns become null" \
    "1	0	3:NULL:9" \
    "INSERT INTO numbers SET id = 3, nn = +9; "\
"SELECT ROW_COUNT(), @@warning_count, CONCAT(id, ':', IFNULL(n, 'NULL'), ':', nn) "\
"FROM numbers WHERE id = 3;" \
    "$DATABASE"

expect_output \
    "signed and unsigned physical boundaries" \
    "1	0	4:-2147483648:2147483647:0:4294967295:-9223372036854775808:9223372036854775807" \
    "INSERT INTO numbers SET id = 4, i = -2147483648, ii = 2147483647, iu = 0, "\
"integeru = 4294967295, b = -9223372036854775808, bu = 9223372036854775807, nn = 10; "\
"SELECT ROW_COUNT(), @@warning_count, CONCAT(id, ':', i, ':', ii, ':', iu, ':', integeru, "\
"':', b, ':', bu) FROM numbers WHERE id = 4;" \
    "$DATABASE"

expect_error \
    "duplicate assignment target" \
    1110 \
    42000 \
    "Column 'id' specified twice" \
    "INSERT INTO numbers SET id = 5, id = 6, nn = 1;" \
    "$DATABASE"

expect_error \
    "unknown assignment target" \
    1054 \
    42S22 \
    "Unknown column 'missing' in 'field list'" \
    "INSERT INTO numbers SET id = 5, missing = 1, nn = 1;" \
    "$DATABASE"

expect_error \
    "omitted not null column" \
    1364 \
    HY000 \
    "Field 'nn' doesn't have a default value" \
    "INSERT INTO numbers SET id = 5;" \
    "$DATABASE"

expect_error \
    "null into not null column" \
    1048 \
    23000 \
    "Column 'id' cannot be null" \
    "INSERT INTO numbers SET id = NULL, nn = 1;" \
    "$DATABASE"

expect_error \
    "signed int above range" \
    1264 \
    22003 \
    "Out of range value for column 'i' at row 1" \
    "INSERT INTO numbers SET id = 5, i = 2147483648, nn = 1;" \
    "$DATABASE"

expect_error \
    "unsigned int below range" \
    1264 \
    22003 \
    "Out of range value for column 'iu' at row 1" \
    "INSERT INTO numbers SET id = 5, iu = -1, nn = 1;" \
    "$DATABASE"

expect_error \
    "signed bigint below range" \
    1264 \
    22003 \
    "Out of range value for column 'b' at row 1" \
    "INSERT INTO numbers SET id = 5, b = -9223372036854775809, nn = 1;" \
    "$DATABASE"

expect_output \
    "mysql accepts wider insert set syntax upstream" \
    "4	1	0:17,6:14,7:15,8:16" \
    "INSERT INTO numbers SET numbers.id = 6, i = 10 + 4, n = DEFAULT, nn = 1; "\
"INSERT LOW_PRIORITY INTO numbers SET id = 7, i = 15, nn = 1; "\
"INSERT HIGH_PRIORITY INTO numbers SET id = 8, i = 16, nn = 1; "\
"INSERT IGNORE INTO numbers SET id = NULL, i = 17, nn = 1; "\
"SELECT COUNT(*), @@warning_count, GROUP_CONCAT(CONCAT(id, ':', IFNULL(i, 'NULL')) ORDER BY id) "\
"FROM numbers WHERE id IN (0, 6, 7, 8);" \
    "$DATABASE"
