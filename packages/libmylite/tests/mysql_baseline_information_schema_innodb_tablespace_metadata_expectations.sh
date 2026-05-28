#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"

fail() {
    printf '%s\n' \
        "mysql_baseline_information_schema_innodb_tablespace_metadata_expectations: $1" >&2
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

version=$(run_mysql 'SELECT VERSION();')
case "$version" in
    8.4.9*) ;;
    *) fail "expected MySQL 8.4.9 runtime, got [$version]" ;;
esac

datafiles_headers=$(run_mysql_with_headers \
    "SELECT * FROM INFORMATION_SCHEMA.INNODB_DATAFILES LIMIT 1;" | sed -n '1p')
expect_value "innodb datafiles headers" "SPACE	PATH" "$datafiles_headers"

brief_headers=$(run_mysql_with_headers \
    "SELECT * FROM INFORMATION_SCHEMA.INNODB_TABLESPACES_BRIEF LIMIT 1;" | sed -n '1p')
expect_value "innodb tablespaces brief headers" \
    "SPACE	NAME	PATH	FLAG	SPACE_TYPE" \
    "$brief_headers"

system_table_rows=$(run_mysql \
    "SELECT TABLE_NAME,TABLE_TYPE,ENGINE,VERSION,ROW_FORMAT,TABLE_ROWS,DATA_LENGTH, "\
"AUTO_INCREMENT FROM INFORMATION_SCHEMA.TABLES "\
"WHERE TABLE_SCHEMA = 'information_schema' "\
"AND TABLE_NAME IN ('INNODB_DATAFILES', 'INNODB_TABLESPACES_BRIEF') "\
"ORDER BY TABLE_NAME;")
expected_system_table_rows="INNODB_DATAFILES	SYSTEM VIEW	NULL	10	NULL	0	0	NULL
INNODB_TABLESPACES_BRIEF	SYSTEM VIEW	NULL	10	NULL	0	0	NULL"
expect_value "innodb tablespace metadata system rows" \
    "$expected_system_table_rows" \
    "$system_table_rows"

columns_metadata=$(run_mysql \
    "SELECT TABLE_NAME,COLUMN_NAME,ORDINAL_POSITION,COLUMN_DEFAULT,IS_NULLABLE,DATA_TYPE, "\
"CHARACTER_MAXIMUM_LENGTH,CHARACTER_OCTET_LENGTH,NUMERIC_PRECISION,NUMERIC_SCALE, "\
"DATETIME_PRECISION,CHARACTER_SET_NAME,COLLATION_NAME,COLUMN_TYPE,PRIVILEGES "\
"FROM INFORMATION_SCHEMA.COLUMNS "\
"WHERE TABLE_SCHEMA = 'information_schema' "\
"AND TABLE_NAME IN ('INNODB_DATAFILES', 'INNODB_TABLESPACES_BRIEF') "\
"ORDER BY TABLE_NAME, ORDINAL_POSITION;")
expected_columns_metadata="INNODB_DATAFILES	SPACE	1	NULL	YES	varbinary	256	256	NULL	NULL	NULL	NULL	NULL	varbinary(256)	select
INNODB_DATAFILES	PATH	2	NULL	NO	varchar	512	1536	NULL	NULL	NULL	utf8mb3	utf8mb3_bin	varchar(512)	select
INNODB_TABLESPACES_BRIEF	SPACE	1	NULL	YES	varbinary	256	256	NULL	NULL	NULL	NULL	NULL	varbinary(256)	select
INNODB_TABLESPACES_BRIEF	NAME	2	NULL	NO	varchar	268	804	NULL	NULL	NULL	utf8mb3	utf8mb3_bin	varchar(268)	select
INNODB_TABLESPACES_BRIEF	PATH	3	NULL	NO	varchar	512	1536	NULL	NULL	NULL	utf8mb3	utf8mb3_bin	varchar(512)	select
INNODB_TABLESPACES_BRIEF	FLAG	4	NULL	YES	varbinary	256	256	NULL	NULL	NULL	NULL	NULL	varbinary(256)	select
INNODB_TABLESPACES_BRIEF	SPACE_TYPE	5		NO	varchar	7	21	NULL	NULL	NULL	utf8mb3	utf8mb3_general_ci	varchar(7)	select"
expect_value "innodb tablespace metadata columns" "$expected_columns_metadata" "$columns_metadata"

datafiles_count=$(run_mysql "SELECT COUNT(*) FROM INFORMATION_SCHEMA.INNODB_DATAFILES;")
expect_value "innodb datafiles count" "4" "$datafiles_count"

brief_count=$(run_mysql "SELECT COUNT(*) FROM INFORMATION_SCHEMA.INNODB_TABLESPACES_BRIEF;")
expect_value "innodb tablespaces brief count" "4" "$brief_count"

expected_datafiles_rows="1	./sys/sys_config.ibd
4294967279	./undo_001
4294967278	./undo_002
0	ibdata1"
datafiles_rows=$(run_mysql \
    "SELECT SPACE,PATH FROM INFORMATION_SCHEMA.INNODB_DATAFILES ORDER BY PATH;")
expect_value "innodb datafiles exact rows" "$expected_datafiles_rows" "$datafiles_rows"

expected_brief_rows="0	innodb_system	ibdata1	18432	System
4294967279	innodb_undo_001	./undo_001	0	Single
4294967278	innodb_undo_002	./undo_002	0	Single
1	sys/sys_config	./sys/sys_config.ibd	16417	Single"
brief_rows=$(run_mysql \
    "SELECT SPACE,NAME,PATH,FLAG,SPACE_TYPE "\
"FROM INFORMATION_SCHEMA.INNODB_TABLESPACES_BRIEF ORDER BY NAME;")
expect_value "innodb tablespaces brief exact rows" "$expected_brief_rows" "$brief_rows"

alias_row=$(run_mysql \
    "SELECT b.NAME FROM INFORMATION_SCHEMA.INNODB_TABLESPACES_BRIEF AS b "\
"WHERE b.SPACE = '1';")
expect_value "innodb tablespaces brief alias row" "sys/sys_config" "$alias_row"

status=$(run_mysql \
    "SELECT COUNT(*) FROM INFORMATION_SCHEMA.INNODB_DATAFILES; "\
"SELECT @@warning_count, ROW_COUNT();" | tail -n 1)
expect_value "innodb datafiles warning and row count status" "0	-1" "$status"

printf '%s\n' \
    "mysql_baseline_information_schema_innodb_tablespace_metadata_expectations: ok"
