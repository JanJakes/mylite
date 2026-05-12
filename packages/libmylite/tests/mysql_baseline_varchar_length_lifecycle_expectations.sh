#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_varchar_length_expectations_$$"

fail() {
    printf '%s\n' "mysql_baseline_varchar_length_lifecycle_expectations: $1" >&2
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

expect_upstream_accepts() {
    label=$1
    sql=$2
    shift 2

    set +e
    output=$(run_mysql "$sql" "$@" 2>&1)
    status_code=$?
    set -e

    if [ "$status_code" -ne 0 ]; then
        fail "$label: expected MySQL to accept behavior, got [$output]"
    fi
}

cleanup() {
    run_mysql "DROP DATABASE IF EXISTS ${DATABASE};" >/dev/null 2>&1 || true
}

trap cleanup EXIT

version=$(run_mysql "SELECT VERSION();")
case "$version" in
    8.4.9*) ;;
    *) fail "expected MySQL 8.4.9 runtime, got [$version]" ;;
esac

case "$(run_mysql "SELECT @@sql_mode;")" in
    *STRICT_TRANS_TABLES*) ;;
    *) fail "expected strict default sql_mode" ;;
esac

cleanup
run_mysql \
    "CREATE DATABASE ${DATABASE} DEFAULT CHARACTER SET utf8mb4 COLLATE utf8mb4_0900_ai_ci;" \
    >/dev/null

show_columns_expected=$(cat <<\EXPECTED
v256	varchar(256)	YES		NULL	
v512	varchar(512)	YES		xy	
alias256	varchar(256)	YES		NULL	
alias512	varchar(512)	YES		NULL	
EXPECTED
)
expect_output \
    "wide varchar descriptors render normalized columns" \
    "$show_columns_expected" \
    "CREATE TABLE wide_strings ("\
"v256 VARCHAR(256), "\
"v512 VARCHAR(512) DEFAULT 'xy', "\
"alias256 CHARACTER VARYING(256), "\
"alias512 CHAR VARYING(512)); "\
"SHOW COLUMNS FROM wide_strings;" \
    "$DATABASE"

metadata_expected=$(cat <<\EXPECTED
alias256	varchar	varchar(256)	256	1024
alias512	varchar	varchar(512)	512	2048
v256	varchar	varchar(256)	256	1024
v512	varchar	varchar(512)	512	2048
EXPECTED
)
expect_output \
    "wide varchar information schema lengths" \
    "$metadata_expected" \
    "SELECT COLUMN_NAME, DATA_TYPE, COLUMN_TYPE, CHARACTER_MAXIMUM_LENGTH, "\
"CHARACTER_OCTET_LENGTH FROM INFORMATION_SCHEMA.COLUMNS "\
"WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = 'wide_strings' ORDER BY COLUMN_NAME;" \
    "$DATABASE"

dml_expected=$(cat <<\EXPECTED
1	0	256	512	xy
1	0	256	512	zz
EXPECTED
)
expect_output \
    "wide varchar insert update and defaults" \
    "$dml_expected" \
    "INSERT INTO wide_strings (v256, alias256, alias512) "\
"VALUES (REPEAT('a', 256), REPEAT('c', 256), REPEAT('d', 512)); "\
"SELECT ROW_COUNT(), @@warning_count, CHAR_LENGTH(v256), CHAR_LENGTH(alias512), v512 "\
"FROM wide_strings; "\
"UPDATE wide_strings SET v256 = REPEAT('b', 256), v512 = 'zz'; "\
"SELECT ROW_COUNT(), @@warning_count, CHAR_LENGTH(v256), CHAR_LENGTH(alias512), v512 "\
"FROM wide_strings;" \
    "$DATABASE"

expect_output \
    "single utf8mb4 maximum varchar descriptor is accepted" \
    "16383	16383" \
    "CREATE TABLE v16383 (v VARCHAR(16383)); "\
"INSERT INTO v16383 VALUES (REPEAT('x', 16383)); "\
"SELECT CHAR_LENGTH(v), LENGTH(v) FROM v16383;" \
    "$DATABASE"

expect_upstream_accepts \
    "mysql accepts row-size boundary at two varchar 8191 columns" \
    "CREATE TABLE two8191 (a VARCHAR(8191), b VARCHAR(8191));" \
    "$DATABASE"

expect_error \
    "mysql rejects row-size overflow at two varchar 8192 columns" \
    1118 \
    42000 \
    "Row size too large" \
    "CREATE TABLE two8192 (a VARCHAR(8192), b VARCHAR(8192));" \
    "$DATABASE"

expect_upstream_accepts \
    "mysql accepts int plus varchar 16382 row-size boundary" \
    "CREATE TABLE int_plus_16382 (id INT, v VARCHAR(16382));" \
    "$DATABASE"

expect_error \
    "mysql rejects int plus varchar 16383 row-size overflow" \
    1118 \
    42000 \
    "Row size too large" \
    "CREATE TABLE int_plus_16383 (id INT, v VARCHAR(16383));" \
    "$DATABASE"

expect_error \
    "mysql rejects varchar above utf8mb4 maximum" \
    1074 \
    42000 \
    "Column length too big for column 'v'" \
    "CREATE TABLE too_wide (v VARCHAR(16384));" \
    "$DATABASE"

expect_upstream_accepts \
    "mysql accepts wider varchar unique key deferred by mylite" \
    "CREATE TABLE key512 (v VARCHAR(512), UNIQUE KEY u_v (v));" \
    "$DATABASE"

expect_error \
    "mysql rejects extreme varchar unique key above innodb key length" \
    1071 \
    42000 \
    "Specified key was too long" \
    "CREATE TABLE key16383 (v VARCHAR(16383), UNIQUE KEY u_v (v));" \
    "$DATABASE"

printf '%s\n' "mysql_baseline_varchar_length_lifecycle_expectations: ok"
