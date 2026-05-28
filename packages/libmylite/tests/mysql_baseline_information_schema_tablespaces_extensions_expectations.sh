#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_tablespaces_ext_$$"

fail() {
    printf '%s\n' \
        "mysql_baseline_information_schema_tablespaces_extensions_expectations: $1" >&2
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

headers=$(run_mysql_with_headers \
    "SELECT * FROM INFORMATION_SCHEMA.TABLESPACES_EXTENSIONS LIMIT 1;" | sed -n '1p')
expect_value "tablespaces extensions headers" \
    "TABLESPACE_NAME	ENGINE_ATTRIBUTE" \
    "$headers"

baseline_rows=$(run_mysql \
    "SELECT TABLESPACE_NAME,ENGINE_ATTRIBUTE "\
"FROM INFORMATION_SCHEMA.TABLESPACES_EXTENSIONS "\
"WHERE TABLESPACE_NAME IN ('innodb_system','innodb_temporary','innodb_undo_001', "\
"'innodb_undo_002','mysql','sys/sys_config') ORDER BY TABLESPACE_NAME;")
expected_baseline_rows="innodb_system	NULL
innodb_temporary	NULL
innodb_undo_001	NULL
innodb_undo_002	NULL
mysql	NULL
sys/sys_config	NULL"
expect_value "tablespaces extensions baseline rows" \
    "$expected_baseline_rows" \
    "$baseline_rows"

run_mysql "CREATE DATABASE ${DATABASE};
CREATE TABLE ${DATABASE}.t (id INT PRIMARY KEY);
CREATE VIEW ${DATABASE}.v AS SELECT id FROM ${DATABASE}.t;
CREATE TEMPORARY TABLE ${DATABASE}.tmp (id INT);" >/dev/null

user_rows=$(run_mysql \
    "SELECT TABLESPACE_NAME,ENGINE_ATTRIBUTE "\
"FROM INFORMATION_SCHEMA.TABLESPACES_EXTENSIONS "\
"WHERE TABLESPACE_NAME LIKE '${DATABASE}/%' ORDER BY TABLESPACE_NAME;")
expected_user_rows="${DATABASE}/t	NULL"
expect_value "tablespaces extensions user rows" "$expected_user_rows" "$user_rows"

status=$(run_mysql \
    "SELECT * FROM INFORMATION_SCHEMA.TABLESPACES_EXTENSIONS "\
"WHERE TABLESPACE_NAME = '${DATABASE}/t'; "\
"SELECT @@warning_count, ROW_COUNT();" | tail -n 1)
expect_value "tablespaces extensions status" "0	-1" "$status"

system_table_rows=$(run_mysql \
    "SELECT TABLE_NAME,TABLE_TYPE,ENGINE,VERSION,ROW_FORMAT,TABLE_ROWS,DATA_LENGTH "\
"FROM INFORMATION_SCHEMA.TABLES "\
"WHERE TABLE_SCHEMA = 'information_schema' "\
"AND TABLE_NAME = 'TABLESPACES_EXTENSIONS';")
expected_system_table_rows="TABLESPACES_EXTENSIONS	SYSTEM VIEW	NULL	10	NULL	0	0"
expect_value "tablespaces extensions system table row" \
    "$expected_system_table_rows" \
    "$system_table_rows"

columns_metadata=$(run_mysql \
    "SELECT COLUMN_NAME,ORDINAL_POSITION,COLUMN_DEFAULT,IS_NULLABLE,DATA_TYPE, "\
"CHARACTER_MAXIMUM_LENGTH,CHARACTER_OCTET_LENGTH,NUMERIC_PRECISION,NUMERIC_SCALE, "\
"DATETIME_PRECISION,CHARACTER_SET_NAME,COLLATION_NAME,COLUMN_TYPE,PRIVILEGES "\
"FROM INFORMATION_SCHEMA.COLUMNS "\
"WHERE TABLE_SCHEMA = 'information_schema' "\
"AND TABLE_NAME = 'TABLESPACES_EXTENSIONS' ORDER BY ORDINAL_POSITION;")
expected_columns_metadata="TABLESPACE_NAME	1	NULL	NO	varchar	268	804	NULL	NULL	NULL	utf8mb3	utf8mb3_bin	varchar(268)	select
ENGINE_ATTRIBUTE	2	NULL	YES	json	NULL	NULL	NULL	NULL	NULL	NULL	NULL	json	select"
expect_value "tablespaces extensions columns metadata" \
    "$expected_columns_metadata" \
    "$columns_metadata"

printf '%s\n' \
    "mysql_baseline_information_schema_tablespaces_extensions_expectations: ok"
