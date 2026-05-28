#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_innodb_tables_expectations_$$"

fail() {
    printf '%s\n' "mysql_baseline_information_schema_innodb_tables_expectations: $1" >&2
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
       id INT NOT NULL,
       v VARCHAR(10),
       PRIMARY KEY(id)
     ) ENGINE=InnoDB;
     CREATE TABLE t_no_pk(
       a INT,
       b INT
     ) ENGINE=InnoDB;
     CREATE TABLE t_unique(
       a INT NOT NULL,
       b INT NOT NULL,
       UNIQUE KEY uq_a(a)
     ) ENGINE=InnoDB;
     CREATE TABLE t_compact(
       a INT
     ) ENGINE=InnoDB ROW_FORMAT=COMPACT;
     CREATE TABLE t_redundant(
       a INT
     ) ENGINE=InnoDB ROW_FORMAT=REDUNDANT;
     CREATE TABLE t_compressed(
       a INT
     ) ENGINE=InnoDB ROW_FORMAT=COMPRESSED KEY_BLOCK_SIZE=8;" >/dev/null

table_kind=$(run_mysql \
    "SHOW FULL TABLES FROM INFORMATION_SCHEMA WHERE Tables_in_INFORMATION_SCHEMA = "\
"'INNODB_TABLES';")
expect_value "innodb tables table kind" \
    "INNODB_TABLES	SYSTEM VIEW" \
    "$table_kind"

system_table_row=$(run_mysql \
    "SELECT TABLE_NAME,TABLE_TYPE,ENGINE,VERSION,ROW_FORMAT,TABLE_ROWS,DATA_LENGTH,AUTO_INCREMENT "\
"FROM INFORMATION_SCHEMA.TABLES WHERE TABLE_SCHEMA = 'information_schema' "\
"AND TABLE_NAME = 'INNODB_TABLES';")
expect_value "innodb tables system table row" \
    "INNODB_TABLES	SYSTEM VIEW	NULL	10	NULL	0	0	NULL" \
    "$system_table_row"

expected_columns_metadata="INNODB_TABLES	TABLE_ID	1		NO	bigint	NULL	NULL	NULL	NULL	NULL	NULL	NULL	bigint unsigned	select
INNODB_TABLES	NAME	2		NO	varchar	218	655	NULL	NULL	NULL	utf8mb3	utf8mb3_general_ci	varchar(655)	select
INNODB_TABLES	FLAG	3		NO	int	NULL	NULL	NULL	NULL	NULL	NULL	NULL	int	select
INNODB_TABLES	N_COLS	4		NO	int	NULL	NULL	NULL	NULL	NULL	NULL	NULL	int	select
INNODB_TABLES	SPACE	5		NO	bigint	NULL	NULL	NULL	NULL	NULL	NULL	NULL	bigint	select
INNODB_TABLES	ROW_FORMAT	6		YES	varchar	4	12	NULL	NULL	NULL	utf8mb3	utf8mb3_general_ci	varchar(12)	select
INNODB_TABLES	ZIP_PAGE_SIZE	7		NO	int	NULL	NULL	NULL	NULL	NULL	NULL	NULL	int unsigned	select
INNODB_TABLES	SPACE_TYPE	8		YES	varchar	3	10	NULL	NULL	NULL	utf8mb3	utf8mb3_general_ci	varchar(10)	select
INNODB_TABLES	INSTANT_COLS	9		NO	int	NULL	NULL	NULL	NULL	NULL	NULL	NULL	int	select
INNODB_TABLES	TOTAL_ROW_VERSIONS	10		NO	int	NULL	NULL	NULL	NULL	NULL	NULL	NULL	int	select"
columns_metadata=$(run_mysql \
    "SELECT TABLE_NAME,COLUMN_NAME,ORDINAL_POSITION,COLUMN_DEFAULT,IS_NULLABLE,DATA_TYPE, "\
"CHARACTER_MAXIMUM_LENGTH,CHARACTER_OCTET_LENGTH,NUMERIC_PRECISION,NUMERIC_SCALE, "\
"DATETIME_PRECISION,CHARACTER_SET_NAME,COLLATION_NAME,COLUMN_TYPE,PRIVILEGES "\
"FROM INFORMATION_SCHEMA.COLUMNS "\
"WHERE TABLE_SCHEMA = 'information_schema' AND TABLE_NAME = 'INNODB_TABLES' "\
"ORDER BY ORDINAL_POSITION;")
expect_value "innodb tables columns metadata" "$expected_columns_metadata" "$columns_metadata"

expected_table_rows="t_compact	1	4	Compact	0	Single	0	0
t_compressed	41	4	Compressed	8192	Single	0	0
t_no_pk	33	5	Dynamic	0	Single	0	0
t_primary	33	5	Dynamic	0	Single	0	0
t_redundant	0	4	Redundant	0	Single	0	0
t_unique	33	5	Dynamic	0	Single	0	0"
table_rows=$(run_mysql \
    "SELECT SUBSTRING_INDEX(NAME, '/', -1), FLAG, N_COLS, ROW_FORMAT, ZIP_PAGE_SIZE, "\
"SPACE_TYPE, INSTANT_COLS, TOTAL_ROW_VERSIONS "\
"FROM INFORMATION_SCHEMA.INNODB_TABLES "\
"WHERE NAME IN ('${DATABASE}/t_primary', '${DATABASE}/t_no_pk', "\
"'${DATABASE}/t_unique', '${DATABASE}/t_compact', '${DATABASE}/t_redundant', "\
"'${DATABASE}/t_compressed') ORDER BY NAME;")
expect_value "innodb tables descriptor rows" "$expected_table_rows" "$table_rows"

physical_id_count=$(run_mysql \
    "SELECT COUNT(*) FROM INFORMATION_SCHEMA.INNODB_TABLES "\
"WHERE NAME LIKE '${DATABASE}/%' AND TABLE_ID > 0 AND SPACE > 0;")
expect_value "innodb tables physical identifiers" "6" "$physical_id_count"

case_count=$(run_mysql \
    "SELECT COUNT(*) FROM INFORMATION_SCHEMA.innodb_tables "\
"WHERE NAME LIKE '${DATABASE}/%';")
expect_value "case-insensitive innodb tables table name count" "6" "$case_count"

use_count=$(run_mysql \
    "USE information_schema; SELECT COUNT(*) FROM INNODB_TABLES "\
"WHERE NAME LIKE '${DATABASE}/%';")
expect_value "unqualified innodb tables count" "6" "$use_count"

status=$(run_mysql \
    "SELECT COUNT(*) FROM INFORMATION_SCHEMA.INNODB_TABLES WHERE NAME LIKE '${DATABASE}/%'; "\
"SELECT @@warning_count, ROW_COUNT();" | tail -n 1)
expect_value "innodb tables status" "0	-1" "$status"

printf '%s\n' "mysql_baseline_information_schema_innodb_tables_expectations: ok"
