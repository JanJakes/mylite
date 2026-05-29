#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_session_temp_tablespaces_expectations_$$"

fail() {
    printf '%s\n' \
        "mysql_baseline_information_schema_innodb_session_temp_tablespaces_expectations: $1" >&2
    exit 1
}

run_mysql() {
    sql=$1
    shift
    printf '%s\n' "$sql" \
        | docker exec -i "$MYSQL_CONTAINER" mysql --protocol=TCP -h127.0.0.1 -uroot \
            --batch --raw --skip-column-names "$@"
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

table_kind=$(run_mysql \
    "SHOW FULL TABLES FROM INFORMATION_SCHEMA WHERE Tables_in_INFORMATION_SCHEMA = "\
"'INNODB_SESSION_TEMP_TABLESPACES';")
expect_value "innodb session temp tablespaces table kind" \
    "INNODB_SESSION_TEMP_TABLESPACES	SYSTEM VIEW" \
    "$table_kind"

system_table_row=$(run_mysql \
    "SELECT TABLE_NAME,TABLE_TYPE,ENGINE,VERSION,ROW_FORMAT,TABLE_ROWS,DATA_LENGTH,AUTO_INCREMENT "\
"FROM INFORMATION_SCHEMA.TABLES WHERE TABLE_SCHEMA = 'information_schema' "\
"AND TABLE_NAME = 'INNODB_SESSION_TEMP_TABLESPACES';")
expect_value "innodb session temp tablespaces system table row" \
    "INNODB_SESSION_TEMP_TABLESPACES	SYSTEM VIEW	NULL	10	NULL	0	0	NULL" \
    "$system_table_row"

expected_columns_metadata="INNODB_SESSION_TEMP_TABLESPACES	ID	1		NO	int	NULL	NULL	NULL	NULL	NULL	NULL	NULL	int unsigned	select
INNODB_SESSION_TEMP_TABLESPACES	SPACE	2		NO	int	NULL	NULL	NULL	NULL	NULL	NULL	NULL	int unsigned	select
INNODB_SESSION_TEMP_TABLESPACES	PATH	3		NO	varchar	1333	4001	NULL	NULL	NULL	utf8mb3	utf8mb3_general_ci	varchar(4001)	select
INNODB_SESSION_TEMP_TABLESPACES	SIZE	4		NO	bigint	NULL	NULL	NULL	NULL	NULL	NULL	NULL	bigint unsigned	select
INNODB_SESSION_TEMP_TABLESPACES	STATE	5		NO	varchar	64	192	NULL	NULL	NULL	utf8mb3	utf8mb3_general_ci	varchar(192)	select
INNODB_SESSION_TEMP_TABLESPACES	PURPOSE	6		NO	varchar	64	192	NULL	NULL	NULL	utf8mb3	utf8mb3_general_ci	varchar(192)	select"
columns_metadata=$(run_mysql \
    "SELECT TABLE_NAME,COLUMN_NAME,ORDINAL_POSITION,COLUMN_DEFAULT,IS_NULLABLE,DATA_TYPE, "\
"CHARACTER_MAXIMUM_LENGTH,CHARACTER_OCTET_LENGTH,NUMERIC_PRECISION,NUMERIC_SCALE, "\
"DATETIME_PRECISION,CHARACTER_SET_NAME,COLLATION_NAME,COLUMN_TYPE,PRIVILEGES "\
"FROM INFORMATION_SCHEMA.COLUMNS "\
"WHERE TABLE_SCHEMA = 'information_schema' "\
"AND TABLE_NAME = 'INNODB_SESSION_TEMP_TABLESPACES' "\
"ORDER BY ORDINAL_POSITION;")
expect_value "innodb session temp tablespaces columns metadata" \
    "$expected_columns_metadata" \
    "$columns_metadata"

count=$(run_mysql "SELECT COUNT(*) FROM INFORMATION_SCHEMA.INNODB_SESSION_TEMP_TABLESPACES;")
expect_value "innodb session temp tablespaces count" "10" "$count"

expected_paths="4243767281	./#innodb_temp/temp_1.ibt	81920
4243767282	./#innodb_temp/temp_2.ibt	81920
4243767283	./#innodb_temp/temp_3.ibt	81920
4243767284	./#innodb_temp/temp_4.ibt	81920
4243767285	./#innodb_temp/temp_5.ibt	81920
4243767286	./#innodb_temp/temp_6.ibt	81920
4243767287	./#innodb_temp/temp_7.ibt	81920
4243767288	./#innodb_temp/temp_8.ibt	81920
4243767289	./#innodb_temp/temp_9.ibt	81920
4243767290	./#innodb_temp/temp_10.ibt	81920"
paths=$(run_mysql \
    "SELECT SPACE, PATH, SIZE FROM INFORMATION_SCHEMA.INNODB_SESSION_TEMP_TABLESPACES "\
"ORDER BY SPACE;")
expect_value "innodb session temp tablespaces baseline paths" "$expected_paths" "$paths"

inactive_count=$(run_mysql \
    "SELECT COUNT(*) FROM INFORMATION_SCHEMA.INNODB_SESSION_TEMP_TABLESPACES "\
"WHERE ID = 0 AND STATE = 'INACTIVE' AND PURPOSE = 'NONE';")
expect_value "innodb session temp tablespaces inactive rows" "9" "$inactive_count"

intrinsic_count=$(run_mysql \
    "SELECT COUNT(*) FROM INFORMATION_SCHEMA.INNODB_SESSION_TEMP_TABLESPACES "\
"WHERE ID = CONNECTION_ID() AND STATE = 'ACTIVE' AND PURPOSE = 'INTRINSIC';")
expect_value "innodb session temp tablespaces active intrinsic row" "1" "$intrinsic_count"

case_count=$(run_mysql \
    "SELECT COUNT(*) FROM INFORMATION_SCHEMA.innodb_session_temp_tablespaces "\
"WHERE PURPOSE = 'NONE';")
expect_value "case-insensitive innodb session temp tablespaces count" "9" "$case_count"

use_count=$(run_mysql \
    "USE information_schema; SELECT COUNT(*) FROM INNODB_SESSION_TEMP_TABLESPACES "\
"WHERE STATE = 'ACTIVE';")
expect_value "unqualified innodb session temp tablespaces active count" "1" "$use_count"

status=$(run_mysql \
    "SELECT COUNT(*) FROM INFORMATION_SCHEMA.INNODB_SESSION_TEMP_TABLESPACES; "\
"SELECT @@warning_count, ROW_COUNT();" | tail -n 1)
expect_value "innodb session temp tablespaces status" "0	-1" "$status"

cleanup
user_active_count=$(run_mysql \
    "CREATE DATABASE ${DATABASE}; USE ${DATABASE}; "\
"CREATE TEMPORARY TABLE t_temp(id INT PRIMARY KEY, v VARCHAR(32)) ENGINE=InnoDB; "\
"INSERT INTO t_temp VALUES (1,'a'),(2,'b'); "\
"SELECT COUNT(*) FROM INFORMATION_SCHEMA.INNODB_SESSION_TEMP_TABLESPACES "\
"WHERE ID = CONNECTION_ID() AND STATE = 'ACTIVE' AND PURPOSE = 'USER'; "\
"DROP DATABASE ${DATABASE};" | sed -n '1p')
expect_value "created temporary table active user row observation" "1" "$user_active_count"

printf '%s\n' \
    "mysql_baseline_information_schema_innodb_session_temp_tablespaces_expectations: ok"
