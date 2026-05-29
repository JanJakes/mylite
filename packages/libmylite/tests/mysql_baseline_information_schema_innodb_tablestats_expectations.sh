#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_innodb_tablestats_expectations_$$"

fail() {
    printf '%s\n' "mysql_baseline_information_schema_innodb_tablestats_expectations: $1" >&2
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

cleanup

run_mysql \
    "CREATE DATABASE ${DATABASE}; USE ${DATABASE};
     CREATE TABLE t_primary(
       id INT PRIMARY KEY,
       v INT,
       KEY ix_v(v),
       KEY ix_v_id(v,id)
     ) ENGINE=InnoDB;
     CREATE TABLE t_no_pk(
       a INT,
       b INT
     ) ENGINE=InnoDB;
     CREATE TABLE t_auto(
       id INT AUTO_INCREMENT PRIMARY KEY,
       v INT
     ) ENGINE=InnoDB;
     INSERT INTO t_primary VALUES (1,10),(2,20),(3,20);
     INSERT INTO t_no_pk VALUES (1,2),(3,4);
     INSERT INTO t_auto(v) VALUES (7),(8);" >/dev/null

table_kind=$(run_mysql \
    "SHOW FULL TABLES FROM INFORMATION_SCHEMA WHERE Tables_in_INFORMATION_SCHEMA = "\
"'INNODB_TABLESTATS';")
expect_value "innodb tablestats table kind" \
    "INNODB_TABLESTATS	SYSTEM VIEW" \
    "$table_kind"

system_table_row=$(run_mysql \
    "SELECT TABLE_NAME,TABLE_TYPE,ENGINE,VERSION,ROW_FORMAT,TABLE_ROWS,DATA_LENGTH,AUTO_INCREMENT "\
"FROM INFORMATION_SCHEMA.TABLES WHERE TABLE_SCHEMA = 'information_schema' "\
"AND TABLE_NAME = 'INNODB_TABLESTATS';")
expect_value "innodb tablestats system table row" \
    "INNODB_TABLESTATS	SYSTEM VIEW	NULL	10	NULL	0	0	NULL" \
    "$system_table_row"

expected_columns_metadata="INNODB_TABLESTATS	TABLE_ID	1		NO	bigint	NULL	NULL	NULL	NULL	NULL	NULL	NULL	bigint unsigned	select
INNODB_TABLESTATS	NAME	2		NO	varchar	64	193	NULL	NULL	NULL	utf8mb3	utf8mb3_general_ci	varchar(193)	select
INNODB_TABLESTATS	STATS_INITIALIZED	3		NO	varchar	64	193	NULL	NULL	NULL	utf8mb3	utf8mb3_general_ci	varchar(193)	select
INNODB_TABLESTATS	NUM_ROWS	4		NO	bigint	NULL	NULL	NULL	NULL	NULL	NULL	NULL	bigint unsigned	select
INNODB_TABLESTATS	CLUST_INDEX_SIZE	5		NO	bigint	NULL	NULL	NULL	NULL	NULL	NULL	NULL	bigint unsigned	select
INNODB_TABLESTATS	OTHER_INDEX_SIZE	6		NO	bigint	NULL	NULL	NULL	NULL	NULL	NULL	NULL	bigint unsigned	select
INNODB_TABLESTATS	MODIFIED_COUNTER	7		NO	bigint	NULL	NULL	NULL	NULL	NULL	NULL	NULL	bigint unsigned	select
INNODB_TABLESTATS	AUTOINC	8		NO	bigint	NULL	NULL	NULL	NULL	NULL	NULL	NULL	bigint unsigned	select
INNODB_TABLESTATS	REF_COUNT	9		NO	int	NULL	NULL	NULL	NULL	NULL	NULL	NULL	int	select"
columns_metadata=$(run_mysql \
    "SELECT TABLE_NAME,COLUMN_NAME,ORDINAL_POSITION,COLUMN_DEFAULT,IS_NULLABLE,DATA_TYPE, "\
"CHARACTER_MAXIMUM_LENGTH,CHARACTER_OCTET_LENGTH,NUMERIC_PRECISION,NUMERIC_SCALE, "\
"DATETIME_PRECISION,CHARACTER_SET_NAME,COLLATION_NAME,COLUMN_TYPE,PRIVILEGES "\
"FROM INFORMATION_SCHEMA.COLUMNS "\
"WHERE TABLE_SCHEMA = 'information_schema' AND TABLE_NAME = 'INNODB_TABLESTATS' "\
"ORDER BY ORDINAL_POSITION;")
expect_value "innodb tablestats columns metadata" "$expected_columns_metadata" "$columns_metadata"

expected_stats_rows="t_auto	Initialized	2	1	0	3	1
t_no_pk	Initialized	2	1	0	0	1
t_primary	Initialized	3	1	2	0	1"
stats_rows=$(run_mysql \
    "SELECT SUBSTRING_INDEX(NAME, '/', -1), STATS_INITIALIZED, NUM_ROWS, "\
"CLUST_INDEX_SIZE, OTHER_INDEX_SIZE, AUTOINC, TABLE_ID > 0 "\
"FROM INFORMATION_SCHEMA.INNODB_TABLESTATS "\
"WHERE NAME IN ('${DATABASE}/t_primary', '${DATABASE}/t_no_pk', '${DATABASE}/t_auto') "\
"ORDER BY NAME;")
expect_value "innodb tablestats descriptor rows" "$expected_stats_rows" "$stats_rows"

case_count=$(run_mysql \
    "SELECT COUNT(*) FROM INFORMATION_SCHEMA.innodb_tablestats "\
"WHERE NAME LIKE '${DATABASE}/%';")
expect_value "case-insensitive innodb tablestats table name count" "3" "$case_count"

use_count=$(run_mysql \
    "USE information_schema; SELECT COUNT(*) FROM INNODB_TABLESTATS "\
"WHERE NAME LIKE '${DATABASE}/%';")
expect_value "unqualified innodb tablestats count" "3" "$use_count"

status=$(run_mysql \
    "SELECT COUNT(*) FROM INFORMATION_SCHEMA.INNODB_TABLESTATS "\
"WHERE NAME LIKE '${DATABASE}/%'; SELECT @@warning_count, ROW_COUNT();" | tail -n 1)
expect_value "innodb tablestats status" "0	-1" "$status"

printf '%s\n' "mysql_baseline_information_schema_innodb_tablestats_expectations: ok"
