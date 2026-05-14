#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_information_schema_check_constraints_expectations_$$"

fail() {
    printf '%s\n' "mysql_baseline_information_schema_check_constraints_expectations: $1" >&2
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
}

trap cleanup EXIT

version=$(run_mysql "SELECT VERSION();")
case "$version" in
    8.4.9*) ;;
    *) fail "expected MySQL 8.4.9 runtime, got [$version]" ;;
esac

cleanup
run_mysql "CREATE DATABASE ${DATABASE}; CREATE TABLE ${DATABASE}.no_check(id INT);" >/dev/null

expect_output \
    "empty check constraints rowset" \
    "" \
    "SELECT * FROM INFORMATION_SCHEMA.CHECK_CONSTRAINTS "\
"WHERE CONSTRAINT_SCHEMA = '${DATABASE}';" \
    "$DATABASE"

expect_output \
    "empty check constraints count" \
    "0" \
    "SELECT COUNT(*) FROM INFORMATION_SCHEMA.CHECK_CONSTRAINTS "\
"WHERE CONSTRAINT_SCHEMA = '${DATABASE}';" \
    "$DATABASE"

expect_output \
    "empty check constraints alias order limit" \
    "" \
    "SELECT cc.CONSTRAINT_NAME FROM INFORMATION_SCHEMA.CHECK_CONSTRAINTS AS cc "\
"WHERE cc.CONSTRAINT_SCHEMA = '${DATABASE}' ORDER BY cc.CONSTRAINT_NAME LIMIT 1;" \
    "$DATABASE"

expect_output \
    "select status" \
    "0	-1" \
    "SELECT * FROM INFORMATION_SCHEMA.CHECK_CONSTRAINTS "\
"WHERE CONSTRAINT_SCHEMA = '${DATABASE}'; SELECT @@warning_count, ROW_COUNT();" \
    "$DATABASE"

system_tables_expected=$(cat <<\EXPECTED
information_schema	CHECK_CONSTRAINTS	SYSTEM VIEW	NULL	10	NULL	0	0	NULL
EXPECTED
)
expect_output \
    "information schema system table row" \
    "$system_tables_expected" \
    "SELECT TABLE_SCHEMA, TABLE_NAME, TABLE_TYPE, ENGINE, VERSION, ROW_FORMAT, "\
"TABLE_ROWS, DATA_LENGTH, AUTO_INCREMENT FROM INFORMATION_SCHEMA.TABLES "\
"WHERE TABLE_SCHEMA = 'information_schema' AND TABLE_NAME = 'CHECK_CONSTRAINTS';" \
    "$DATABASE"

system_columns_expected=$(cat <<\EXPECTED
CONSTRAINT_CATALOG	1	NULL	NO	varchar	64	192	NULL	NULL	NULL	utf8mb3	utf8mb3_bin	varchar(64)	select
CONSTRAINT_SCHEMA	2	NULL	NO	varchar	64	192	NULL	NULL	NULL	utf8mb3	utf8mb3_bin	varchar(64)	select
CONSTRAINT_NAME	3	NULL	NO	varchar	64	192	NULL	NULL	NULL	utf8mb3	utf8mb3_tolower_ci	varchar(64)	select
CHECK_CLAUSE	4	NULL	NO	longtext	4294967295	4294967295	NULL	NULL	NULL	utf8mb3	utf8mb3_bin	longtext	select
EXPECTED
)
expect_output \
    "information schema system column rows" \
    "$system_columns_expected" \
    "SELECT COLUMN_NAME, ORDINAL_POSITION, COLUMN_DEFAULT, IS_NULLABLE, DATA_TYPE, "\
"CHARACTER_MAXIMUM_LENGTH, CHARACTER_OCTET_LENGTH, NUMERIC_PRECISION, "\
"NUMERIC_SCALE, DATETIME_PRECISION, CHARACTER_SET_NAME, COLLATION_NAME, "\
"COLUMN_TYPE, PRIVILEGES FROM INFORMATION_SCHEMA.COLUMNS "\
"WHERE TABLE_SCHEMA = 'information_schema' AND TABLE_NAME = 'CHECK_CONSTRAINTS' "\
"ORDER BY ORDINAL_POSITION;" \
    "$DATABASE"

run_mysql \
    "CREATE TABLE ${DATABASE}.with_check ("\
"id INT, v INT, CONSTRAINT ck_v CHECK (v > 0)"\
");" >/dev/null

real_check_expected=$(printf 'def\t%s\tck_v\t(`v` > 0)' "$DATABASE")
expect_output \
    "real mysql check constraint row" \
    "$real_check_expected" \
    "SELECT CONSTRAINT_CATALOG, CONSTRAINT_SCHEMA, CONSTRAINT_NAME, CHECK_CLAUSE "\
"FROM INFORMATION_SCHEMA.CHECK_CONSTRAINTS WHERE CONSTRAINT_SCHEMA = '${DATABASE}' "\
"ORDER BY CONSTRAINT_NAME;" \
    "$DATABASE"

expect_error \
    "unknown projection column" \
    1054 \
    42S22 \
    "Unknown column 'nope' in 'field list'" \
    "SELECT nope FROM INFORMATION_SCHEMA.CHECK_CONSTRAINTS;"

expect_error \
    "unknown where column" \
    1054 \
    42S22 \
    "Unknown column 'nope' in 'where clause'" \
    "SELECT CONSTRAINT_NAME FROM INFORMATION_SCHEMA.CHECK_CONSTRAINTS WHERE nope = 'x';"

expect_error \
    "unknown order column" \
    1054 \
    42S22 \
    "Unknown column 'nope' in 'order clause'" \
    "SELECT CONSTRAINT_NAME FROM INFORMATION_SCHEMA.CHECK_CONSTRAINTS ORDER BY nope;"

printf '%s\n' "mysql_baseline_information_schema_check_constraints_expectations: ok"
