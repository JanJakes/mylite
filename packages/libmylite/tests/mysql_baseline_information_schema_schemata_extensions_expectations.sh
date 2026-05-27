#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_schemata_ext_$$"

fail() {
    printf '%s\n' "mysql_baseline_information_schema_schemata_extensions_expectations: $1" >&2
    exit 1
}

run_mysql() {
    sql=$1
    shift
    printf '%s\n' "$sql" \
        | docker exec -i "$MYSQL_CONTAINER" mysql --protocol=TCP -h127.0.0.1 -uroot \
            --batch --raw --skip-column-names "$@"
}

run_mysql_with_headers() {
    sql=$1
    shift
    printf '%s\n' "$sql" \
        | docker exec -i "$MYSQL_CONTAINER" mysql --protocol=TCP -h127.0.0.1 -uroot \
            --batch --raw "$@"
}

expect_value() {
    label=$1
    expected=$2
    actual=$3

    if [ "$actual" != "$expected" ]; then
        fail "$label: expected [$expected], got [$actual]"
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

cleanup
run_mysql "CREATE DATABASE ${DATABASE};" >/dev/null

headers=$(run_mysql_with_headers \
    "SELECT * FROM INFORMATION_SCHEMA.SCHEMATA_EXTENSIONS "\
"WHERE SCHEMA_NAME = '${DATABASE}';" | sed -n '1p')
expect_value "schemata extensions headers" "CATALOG_NAME	SCHEMA_NAME	OPTIONS" "$headers"

rows=$(run_mysql \
    "SELECT CATALOG_NAME, SCHEMA_NAME, OPTIONS "\
"FROM INFORMATION_SCHEMA.SCHEMATA_EXTENSIONS "\
"WHERE SCHEMA_NAME IN ('information_schema', 'mysql', 'performance_schema', 'sys', "\
"'${DATABASE}') ORDER BY SCHEMA_NAME;")
expected_rows=$(
    printf 'def\tinformation_schema\t\n'
    printf 'def\t%s\t\n' "$DATABASE"
    printf 'def\tmysql\t\n'
    printf 'def\tperformance_schema\t\n'
    printf 'def\tsys\t'
)
expect_value "schemata extensions built-in and user rows" "$expected_rows" "$rows"

status=$(run_mysql \
    "SELECT * FROM INFORMATION_SCHEMA.SCHEMATA_EXTENSIONS "\
"WHERE SCHEMA_NAME = '${DATABASE}'; SELECT @@warning_count, ROW_COUNT();" | tail -n 1)
expect_value "schemata extensions status" "0	-1" "$status"

system_table_row=$(run_mysql \
    "SELECT TABLE_SCHEMA,TABLE_NAME,TABLE_TYPE,ENGINE,VERSION,ROW_FORMAT,TABLE_ROWS, "\
"DATA_LENGTH,AUTO_INCREMENT FROM INFORMATION_SCHEMA.TABLES "\
"WHERE TABLE_SCHEMA = 'information_schema' AND TABLE_NAME = 'SCHEMATA_EXTENSIONS';")
expect_value "schemata extensions system table row" \
    "information_schema	SCHEMATA_EXTENSIONS	SYSTEM VIEW	NULL	10	NULL	0	0	NULL" \
    "$system_table_row"

columns_metadata=$(run_mysql \
    "SELECT COLUMN_NAME,ORDINAL_POSITION,COLUMN_DEFAULT,IS_NULLABLE,DATA_TYPE, "\
"CHARACTER_MAXIMUM_LENGTH,CHARACTER_OCTET_LENGTH,NUMERIC_PRECISION,NUMERIC_SCALE, "\
"DATETIME_PRECISION,CHARACTER_SET_NAME,COLLATION_NAME,COLUMN_TYPE,PRIVILEGES "\
"FROM INFORMATION_SCHEMA.COLUMNS "\
"WHERE TABLE_SCHEMA = 'information_schema' AND TABLE_NAME = 'SCHEMATA_EXTENSIONS' "\
"ORDER BY ORDINAL_POSITION;")
expected_columns_metadata="CATALOG_NAME	1	NULL	NO	varchar	64	192	NULL	NULL	NULL	utf8mb3	utf8mb3_bin	varchar(64)	select
SCHEMA_NAME	2	NULL	NO	varchar	64	192	NULL	NULL	NULL	utf8mb3	utf8mb3_bin	varchar(64)	select
OPTIONS	3	NULL	YES	varchar	256	768	NULL	NULL	NULL	utf8mb3	utf8mb3_general_ci	varchar(256)	select"
expect_value "schemata extensions columns metadata" \
    "$expected_columns_metadata" \
    "$columns_metadata"

printf '%s\n' "mysql_baseline_information_schema_schemata_extensions_expectations: ok"
