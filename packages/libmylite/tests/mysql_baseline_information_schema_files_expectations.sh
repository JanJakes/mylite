#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"

fail() {
    printf '%s\n' "mysql_baseline_information_schema_files_expectations: $1" >&2
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

version=$(run_mysql 'SELECT VERSION();')
case "$version" in
    8.4.9*) ;;
    *) fail "expected MySQL 8.4.9 runtime, got [$version]" ;;
esac

table_kind=$(run_mysql \
    "SHOW FULL TABLES FROM INFORMATION_SCHEMA WHERE Tables_in_INFORMATION_SCHEMA = 'FILES';")
expect_value "files table kind" "FILES	SYSTEM VIEW" "$table_kind"

system_table_row=$(run_mysql \
    "SELECT TABLE_NAME,TABLE_TYPE,ENGINE,VERSION,ROW_FORMAT,TABLE_ROWS,DATA_LENGTH, "\
"AUTO_INCREMENT FROM INFORMATION_SCHEMA.TABLES "\
"WHERE TABLE_SCHEMA = 'information_schema' AND TABLE_NAME = 'FILES';")
expect_value "files system table row" \
    "FILES	SYSTEM VIEW	NULL	10	NULL	0	0	NULL" \
    "$system_table_row"

columns_metadata=$(run_mysql \
    "SELECT TABLE_NAME,COLUMN_NAME,ORDINAL_POSITION,COLUMN_DEFAULT,IS_NULLABLE,DATA_TYPE, "\
"CHARACTER_MAXIMUM_LENGTH,CHARACTER_OCTET_LENGTH,NUMERIC_PRECISION,NUMERIC_SCALE, "\
"DATETIME_PRECISION,CHARACTER_SET_NAME,COLLATION_NAME,COLUMN_TYPE,PRIVILEGES "\
"FROM INFORMATION_SCHEMA.COLUMNS "\
"WHERE TABLE_SCHEMA = 'information_schema' AND TABLE_NAME = 'FILES' "\
"ORDER BY ORDINAL_POSITION;")
expected_columns_metadata="FILES	FILE_ID	1	NULL	YES	bigint	NULL	NULL	19	0	NULL	NULL	NULL	bigint	select
FILES	FILE_NAME	2	NULL	YES	text	65535	65535	NULL	NULL	NULL	utf8mb3	utf8mb3_bin	text	select
FILES	FILE_TYPE	3	NULL	YES	varchar	256	768	NULL	NULL	NULL	utf8mb3	utf8mb3_general_ci	varchar(256)	select
FILES	TABLESPACE_NAME	4	NULL	NO	varchar	268	804	NULL	NULL	NULL	utf8mb3	utf8mb3_bin	varchar(268)	select
FILES	TABLE_CATALOG	5		NO	varchar	0	0	NULL	NULL	NULL	utf8mb3	utf8mb3_general_ci	varchar(0)	select
FILES	TABLE_SCHEMA	6	NULL	YES	varbinary	0	0	NULL	NULL	NULL	NULL	NULL	varbinary(0)	select
FILES	TABLE_NAME	7	NULL	YES	varbinary	0	0	NULL	NULL	NULL	NULL	NULL	varbinary(0)	select
FILES	LOGFILE_GROUP_NAME	8	NULL	YES	varchar	256	768	NULL	NULL	NULL	utf8mb3	utf8mb3_general_ci	varchar(256)	select
FILES	LOGFILE_GROUP_NUMBER	9	NULL	YES	bigint	NULL	NULL	19	0	NULL	NULL	NULL	bigint	select
FILES	ENGINE	10	NULL	NO	varchar	64	192	NULL	NULL	NULL	utf8mb3	utf8mb3_general_ci	varchar(64)	select
FILES	FULLTEXT_KEYS	11	NULL	YES	varbinary	0	0	NULL	NULL	NULL	NULL	NULL	varbinary(0)	select
FILES	DELETED_ROWS	12	NULL	YES	varbinary	0	0	NULL	NULL	NULL	NULL	NULL	varbinary(0)	select
FILES	UPDATE_COUNT	13	NULL	YES	varbinary	0	0	NULL	NULL	NULL	NULL	NULL	varbinary(0)	select
FILES	FREE_EXTENTS	14	NULL	YES	bigint	NULL	NULL	19	0	NULL	NULL	NULL	bigint	select
FILES	TOTAL_EXTENTS	15	NULL	YES	bigint	NULL	NULL	19	0	NULL	NULL	NULL	bigint	select
FILES	EXTENT_SIZE	16	NULL	YES	bigint	NULL	NULL	19	0	NULL	NULL	NULL	bigint	select
FILES	INITIAL_SIZE	17	NULL	YES	bigint	NULL	NULL	19	0	NULL	NULL	NULL	bigint	select
FILES	MAXIMUM_SIZE	18	NULL	YES	bigint	NULL	NULL	19	0	NULL	NULL	NULL	bigint	select
FILES	AUTOEXTEND_SIZE	19	NULL	YES	bigint	NULL	NULL	19	0	NULL	NULL	NULL	bigint	select
FILES	CREATION_TIME	20	NULL	YES	varbinary	0	0	NULL	NULL	NULL	NULL	NULL	varbinary(0)	select
FILES	LAST_UPDATE_TIME	21	NULL	YES	varbinary	0	0	NULL	NULL	NULL	NULL	NULL	varbinary(0)	select
FILES	LAST_ACCESS_TIME	22	NULL	YES	varbinary	0	0	NULL	NULL	NULL	NULL	NULL	varbinary(0)	select
FILES	RECOVER_TIME	23	NULL	YES	varbinary	0	0	NULL	NULL	NULL	NULL	NULL	varbinary(0)	select
FILES	TRANSACTION_COUNTER	24	NULL	YES	varbinary	0	0	NULL	NULL	NULL	NULL	NULL	varbinary(0)	select
FILES	VERSION	25	NULL	YES	bigint	NULL	NULL	19	0	NULL	NULL	NULL	bigint	select
FILES	ROW_FORMAT	26	NULL	YES	varchar	256	768	NULL	NULL	NULL	utf8mb3	utf8mb3_general_ci	varchar(256)	select
FILES	TABLE_ROWS	27	NULL	YES	varbinary	0	0	NULL	NULL	NULL	NULL	NULL	varbinary(0)	select
FILES	AVG_ROW_LENGTH	28	NULL	YES	varbinary	0	0	NULL	NULL	NULL	NULL	NULL	varbinary(0)	select
FILES	DATA_LENGTH	29	NULL	YES	varbinary	0	0	NULL	NULL	NULL	NULL	NULL	varbinary(0)	select
FILES	MAX_DATA_LENGTH	30	NULL	YES	varbinary	0	0	NULL	NULL	NULL	NULL	NULL	varbinary(0)	select
FILES	INDEX_LENGTH	31	NULL	YES	varbinary	0	0	NULL	NULL	NULL	NULL	NULL	varbinary(0)	select
FILES	DATA_FREE	32	NULL	YES	bigint	NULL	NULL	19	0	NULL	NULL	NULL	bigint	select
FILES	CREATE_TIME	33	NULL	YES	varbinary	0	0	NULL	NULL	NULL	NULL	NULL	varbinary(0)	select
FILES	UPDATE_TIME	34	NULL	YES	varbinary	0	0	NULL	NULL	NULL	NULL	NULL	varbinary(0)	select
FILES	CHECK_TIME	35	NULL	YES	varbinary	0	0	NULL	NULL	NULL	NULL	NULL	varbinary(0)	select
FILES	CHECKSUM	36	NULL	YES	varbinary	0	0	NULL	NULL	NULL	NULL	NULL	varbinary(0)	select
FILES	STATUS	37	NULL	YES	varchar	256	768	NULL	NULL	NULL	utf8mb3	utf8mb3_general_ci	varchar(256)	select
FILES	EXTRA	38	NULL	YES	varchar	256	768	NULL	NULL	NULL	utf8mb3	utf8mb3_general_ci	varchar(256)	select"
expect_value "files columns metadata" "$expected_columns_metadata" "$columns_metadata"

default_file_filter="FILE_NAME IN ('./ibdata1','./ibtmp1','./mysql.ibd','./sys/sys_config.ibd','./undo_001','./undo_002')"

files_count=$(run_mysql "SELECT COUNT(*) FROM INFORMATION_SCHEMA.FILES WHERE ${default_file_filter};")
expect_value "files row count" "6" "$files_count"

expected_files_rows="0	./ibdata1	TABLESPACE	innodb_system	InnoDB	12	1048576	12582912	67108864	NORMAL
4294967293	./ibtmp1	TEMPORARY	innodb_temporary	InnoDB	12	1048576	12582912	67108864	NORMAL
4294967294	./mysql.ibd	TABLESPACE	mysql	InnoDB	31	1048576	0	1048576	NORMAL
1	./sys/sys_config.ibd	TABLESPACE	sys/sys_config	InnoDB	0	1048576	0	1048576	NORMAL
4294967279	./undo_001	UNDO LOG	innodb_undo_001	InnoDB	16	1048576	16777216	16777216	NORMAL
4294967278	./undo_002	UNDO LOG	innodb_undo_002	InnoDB	16	1048576	16777216	16777216	NORMAL"
files_rows=$(run_mysql \
    "SELECT FILE_ID,FILE_NAME,FILE_TYPE,TABLESPACE_NAME,ENGINE,TOTAL_EXTENTS, "\
"EXTENT_SIZE,INITIAL_SIZE,AUTOEXTEND_SIZE,STATUS "\
"FROM INFORMATION_SCHEMA.FILES WHERE ${default_file_filter} ORDER BY FILE_NAME;")
expect_value "files stable rows" "$expected_files_rows" "$files_rows"

space_counters=$(run_mysql \
    "SELECT COUNT(*) FROM INFORMATION_SCHEMA.FILES WHERE ${default_file_filter} "\
"AND FREE_EXTENTS >= 0 AND DATA_FREE >= 0;")
expect_value "files nonnegative volatile space counters" "6" "$space_counters"

use_count=$(run_mysql \
    "USE information_schema; SELECT COUNT(*) FROM FILES "\
"WHERE ${default_file_filter} AND ENGINE = 'InnoDB';")
expect_value "unqualified files count" "6" "$use_count"

status=$(run_mysql \
    "SELECT COUNT(*) FROM INFORMATION_SCHEMA.FILES; "\
"SELECT @@warning_count, ROW_COUNT();" | tail -n 1)
expect_value "files warning and row count status" "0	-1" "$status"

printf '%s\n' "mysql_baseline_information_schema_files_expectations: ok"
