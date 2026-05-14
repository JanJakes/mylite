#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"

fail() {
    printf '%s\n' "mysql_baseline_information_schema_collation_applicability_expectations: $1" >&2
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

version=$(run_mysql "SELECT VERSION();")
case "$version" in
    8.4.9*) ;;
    *) fail "expected MySQL 8.4.9 runtime, got [$version]" ;;
esac

applicability_expected=$(cat <<\EXPECTED
utf8mb4_0900_ai_ci	utf8mb4
utf8mb4_bin	utf8mb4
utf8mb4_general_ci	utf8mb4
utf8mb4_unicode_520_ci	utf8mb4
utf8mb4_unicode_ci	utf8mb4
EXPECTED
)
expect_output \
    "supported utf8mb4 collation applicability rows" \
    "$applicability_expected" \
    "SELECT COLLATION_NAME, CHARACTER_SET_NAME "\
"FROM INFORMATION_SCHEMA.COLLATION_CHARACTER_SET_APPLICABILITY "\
"WHERE COLLATION_NAME IN ('utf8mb4_0900_ai_ci','utf8mb4_general_ci',"\
"'utf8mb4_bin','utf8mb4_unicode_ci','utf8mb4_unicode_520_ci') "\
"ORDER BY COLLATION_NAME;"

expect_output \
    "case insensitive applicability predicate" \
    "utf8mb4_bin	utf8mb4" \
    "SELECT COLLATION_NAME, CHARACTER_SET_NAME "\
"FROM INFORMATION_SCHEMA.COLLATION_CHARACTER_SET_APPLICABILITY "\
"WHERE COLLATION_NAME = 'UTF8MB4_BIN';"

expect_output \
    "limited applicability count" \
    "5" \
    "SELECT COUNT(*) FROM INFORMATION_SCHEMA.COLLATION_CHARACTER_SET_APPLICABILITY "\
"WHERE COLLATION_NAME IN ('utf8mb4_0900_ai_ci','utf8mb4_general_ci',"\
"'utf8mb4_bin','utf8mb4_unicode_ci','utf8mb4_unicode_520_ci');"

status_output=$(run_mysql \
    "SELECT COLLATION_NAME FROM INFORMATION_SCHEMA.COLLATION_CHARACTER_SET_APPLICABILITY "\
"WHERE COLLATION_NAME = 'utf8mb4_bin'; SELECT @@warning_count, ROW_COUNT();" | tail -n 1)
status_expected=$(printf '%b' "0\t-1")
if [ "$status_output" != "$status_expected" ]; then
    fail "successful applicability select status: expected [$status_expected], got [$status_output]"
fi

system_tables_expected=$(cat <<\EXPECTED
information_schema	COLLATION_CHARACTER_SET_APPLICABILITY	SYSTEM VIEW	NULL	10	NULL	0	0	NULL
EXPECTED
)
expect_output \
    "information schema applicability system table row" \
    "$system_tables_expected" \
    "SELECT TABLE_SCHEMA, TABLE_NAME, TABLE_TYPE, ENGINE, VERSION, ROW_FORMAT, "\
"TABLE_ROWS, DATA_LENGTH, AUTO_INCREMENT FROM INFORMATION_SCHEMA.TABLES "\
"WHERE TABLE_SCHEMA = 'information_schema' "\
"AND TABLE_NAME = 'COLLATION_CHARACTER_SET_APPLICABILITY';"

system_columns_expected=$(cat <<\EXPECTED
COLLATION_NAME	1	NULL	NO	varchar	64	192	NULL	NULL	NULL	utf8mb3	utf8mb3_general_ci	varchar(64)	select
CHARACTER_SET_NAME	2	NULL	NO	varchar	64	192	NULL	NULL	NULL	utf8mb3	utf8mb3_general_ci	varchar(64)	select
EXPECTED
)
expect_output \
    "information schema applicability system column rows" \
    "$system_columns_expected" \
    "SELECT COLUMN_NAME, ORDINAL_POSITION, COLUMN_DEFAULT, IS_NULLABLE, DATA_TYPE, "\
"CHARACTER_MAXIMUM_LENGTH, CHARACTER_OCTET_LENGTH, NUMERIC_PRECISION, "\
"NUMERIC_SCALE, DATETIME_PRECISION, CHARACTER_SET_NAME, COLLATION_NAME, "\
"COLUMN_TYPE, PRIVILEGES FROM INFORMATION_SCHEMA.COLUMNS "\
"WHERE TABLE_SCHEMA = 'information_schema' "\
"AND TABLE_NAME = 'COLLATION_CHARACTER_SET_APPLICABILITY' "\
"ORDER BY ORDINAL_POSITION;"

expect_error \
    "unknown projection column" \
    1054 \
    42S22 \
    "Unknown column 'nope' in 'field list'" \
    "SELECT nope FROM INFORMATION_SCHEMA.COLLATION_CHARACTER_SET_APPLICABILITY;"

expect_error \
    "unknown where column" \
    1054 \
    42S22 \
    "Unknown column 'nope' in 'where clause'" \
    "SELECT COLLATION_NAME FROM INFORMATION_SCHEMA.COLLATION_CHARACTER_SET_APPLICABILITY "\
"WHERE nope = 'x';"

expect_error \
    "unknown order column" \
    1054 \
    42S22 \
    "Unknown column 'nope' in 'order clause'" \
    "SELECT COLLATION_NAME FROM INFORMATION_SCHEMA.COLLATION_CHARACTER_SET_APPLICABILITY "\
"ORDER BY nope;"

printf '%s\n' "mysql_baseline_information_schema_collation_applicability_expectations: ok"
