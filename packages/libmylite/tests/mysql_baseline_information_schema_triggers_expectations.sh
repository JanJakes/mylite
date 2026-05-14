#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_information_schema_triggers_expectations_$$"

fail() {
    printf '%s\n' "mysql_baseline_information_schema_triggers_expectations: $1" >&2
    exit 1
}

run_mysql() {
    sql=$1
    shift
    printf '%s\n' "$sql" \
        | docker exec -i "$MYSQL_CONTAINER" mysql --protocol=TCP -h127.0.0.1 -uroot --batch --raw --skip-column-names "$@"
}

run_mysql_with_headers() {
    sql=$1
    shift
    printf '%s\n' "$sql" \
        | docker exec -i "$MYSQL_CONTAINER" mysql --protocol=TCP -h127.0.0.1 -uroot --batch --raw "$@"
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

version=$(run_mysql 'SELECT VERSION();')
case "$version" in
    8.4.9*) ;;
    *) fail "expected MySQL 8.4.9 runtime, got [$version]" ;;
esac

cleanup
run_mysql "CREATE DATABASE ${DATABASE}; CREATE TABLE ${DATABASE}.t(id INT);" >/dev/null

expected_triggers_columns="TRIGGER_CATALOG	TRIGGER_SCHEMA	TRIGGER_NAME	EVENT_MANIPULATION	EVENT_OBJECT_CATALOG	EVENT_OBJECT_SCHEMA	EVENT_OBJECT_TABLE	ACTION_ORDER	ACTION_CONDITION	ACTION_STATEMENT	ACTION_ORIENTATION	ACTION_TIMING	ACTION_REFERENCE_OLD_TABLE	ACTION_REFERENCE_NEW_TABLE	ACTION_REFERENCE_OLD_ROW	ACTION_REFERENCE_NEW_ROW	CREATED	SQL_MODE	DEFINER	CHARACTER_SET_CLIENT	COLLATION_CONNECTION	DATABASE_COLLATION"
count=$(run_mysql \
    "SELECT COUNT(*) FROM INFORMATION_SCHEMA.TRIGGERS WHERE TRIGGER_SCHEMA = '${DATABASE}';")
expect_value "empty triggers count" "0" "$count"

case_count=$(run_mysql \
    "SELECT COUNT(*) FROM INFORMATION_SCHEMA.triggers WHERE TRIGGER_SCHEMA = '${DATABASE}';")
expect_value "case-insensitive table name count" "0" "$case_count"

status=$(run_mysql \
    "SELECT * FROM INFORMATION_SCHEMA.TRIGGERS WHERE TRIGGER_SCHEMA = '${DATABASE}'; "\
"SELECT @@warning_count, ROW_COUNT();" | tail -n 1)
expect_value "triggers status" "0	-1" "$status"

run_mysql \
    "CREATE TRIGGER ${DATABASE}.trg BEFORE INSERT ON ${DATABASE}.t FOR EACH ROW SET @seen = NEW.id;" \
    >/dev/null

triggers_output=$(run_mysql_with_headers \
    "SELECT * FROM INFORMATION_SCHEMA.TRIGGERS WHERE TRIGGER_SCHEMA = '${DATABASE}';")
expect_value "triggers headers" "$expected_triggers_columns" "$(printf '%s\n' "$triggers_output" | sed -n '1p')"
alias_output=$(run_mysql_with_headers \
    "SELECT tr.TRIGGER_NAME FROM INFORMATION_SCHEMA.TRIGGERS AS tr "\
"WHERE tr.TRIGGER_SCHEMA = '${DATABASE}' ORDER BY tr.TRIGGER_NAME LIMIT 1;")
expect_value "alias headers" "TRIGGER_NAME" "$(printf '%s\n' "$alias_output" | sed -n '1p')"

system_table_row=$(run_mysql \
    "SELECT TABLE_SCHEMA,TABLE_NAME,TABLE_TYPE,ENGINE,VERSION,ROW_FORMAT,TABLE_ROWS,DATA_LENGTH,AUTO_INCREMENT "\
"FROM INFORMATION_SCHEMA.TABLES WHERE TABLE_SCHEMA = 'information_schema' AND TABLE_NAME = 'TRIGGERS';")
expect_value "triggers system table row" \
    "information_schema	TRIGGERS	SYSTEM VIEW	NULL	10	NULL	0	0	NULL" \
    "$system_table_row"

expected_columns_metadata="TRIGGERS	TRIGGER_CATALOG	1	NULL	NO	varchar	64	192	NULL	NULL	NULL	utf8mb3	utf8mb3_bin	varchar(64)	select
TRIGGERS	TRIGGER_SCHEMA	2	NULL	NO	varchar	64	192	NULL	NULL	NULL	utf8mb3	utf8mb3_bin	varchar(64)	select
TRIGGERS	TRIGGER_NAME	3	NULL	NO	varchar	64	192	NULL	NULL	NULL	utf8mb3	utf8mb3_general_ci	varchar(64)	select
TRIGGERS	EVENT_MANIPULATION	4	NULL	NO	enum	6	18	NULL	NULL	NULL	utf8mb3	utf8mb3_bin	enum('INSERT','UPDATE','DELETE')	select
TRIGGERS	EVENT_OBJECT_CATALOG	5	NULL	NO	varchar	64	192	NULL	NULL	NULL	utf8mb3	utf8mb3_bin	varchar(64)	select
TRIGGERS	EVENT_OBJECT_SCHEMA	6	NULL	NO	varchar	64	192	NULL	NULL	NULL	utf8mb3	utf8mb3_bin	varchar(64)	select
TRIGGERS	EVENT_OBJECT_TABLE	7	NULL	NO	varchar	64	192	NULL	NULL	NULL	utf8mb3	utf8mb3_bin	varchar(64)	select
TRIGGERS	ACTION_ORDER	8	NULL	NO	int	NULL	NULL	10	0	NULL	NULL	NULL	int unsigned	select
TRIGGERS	ACTION_CONDITION	9	NULL	YES	varbinary	0	0	NULL	NULL	NULL	NULL	NULL	varbinary(0)	select
TRIGGERS	ACTION_STATEMENT	10	NULL	NO	longtext	4294967295	4294967295	NULL	NULL	NULL	utf8mb3	utf8mb3_bin	longtext	select
TRIGGERS	ACTION_ORIENTATION	11		NO	varchar	3	9	NULL	NULL	NULL	utf8mb3	utf8mb3_general_ci	varchar(3)	select
TRIGGERS	ACTION_TIMING	12	NULL	NO	enum	6	18	NULL	NULL	NULL	utf8mb3	utf8mb3_bin	enum('BEFORE','AFTER')	select
TRIGGERS	ACTION_REFERENCE_OLD_TABLE	13	NULL	YES	varbinary	0	0	NULL	NULL	NULL	NULL	NULL	varbinary(0)	select
TRIGGERS	ACTION_REFERENCE_NEW_TABLE	14	NULL	YES	varbinary	0	0	NULL	NULL	NULL	NULL	NULL	varbinary(0)	select
TRIGGERS	ACTION_REFERENCE_OLD_ROW	15		NO	varchar	3	9	NULL	NULL	NULL	utf8mb3	utf8mb3_general_ci	varchar(3)	select
TRIGGERS	ACTION_REFERENCE_NEW_ROW	16		NO	varchar	3	9	NULL	NULL	NULL	utf8mb3	utf8mb3_general_ci	varchar(3)	select
TRIGGERS	CREATED	17	NULL	NO	timestamp	NULL	NULL	NULL	NULL	2	NULL	NULL	timestamp(2)	select
TRIGGERS	SQL_MODE	18	NULL	NO	set	520	1560	NULL	NULL	NULL	utf8mb3	utf8mb3_bin	set('REAL_AS_FLOAT','PIPES_AS_CONCAT','ANSI_QUOTES','IGNORE_SPACE','NOT_USED','ONLY_FULL_GROUP_BY','NO_UNSIGNED_SUBTRACTION','NO_DIR_IN_CREATE','NOT_USED_9','NOT_USED_10','NOT_USED_11','NOT_USED_12','NOT_USED_13','NOT_USED_14','NOT_USED_15','NOT_USED_16','NOT_USED_17','NOT_USED_18','ANSI','NO_AUTO_VALUE_ON_ZERO','NO_BACKSLASH_ESCAPES','STRICT_TRANS_TABLES','STRICT_ALL_TABLES','NO_ZERO_IN_DATE','NO_ZERO_DATE','ALLOW_INVALID_DATES','ERROR_FOR_DIVISION_BY_ZERO','TRADITIONAL','NOT_USED_29','HIGH_NOT_PRECEDENCE','NO_ENGINE_SUBSTITUTION','PAD_CHAR_TO_FULL_LENGTH','TIME_TRUNCATE_FRACTIONAL')	select
TRIGGERS	DEFINER	19	NULL	NO	varchar	288	864	NULL	NULL	NULL	utf8mb3	utf8mb3_bin	varchar(288)	select
TRIGGERS	CHARACTER_SET_CLIENT	20	NULL	NO	varchar	64	192	NULL	NULL	NULL	utf8mb3	utf8mb3_general_ci	varchar(64)	select
TRIGGERS	COLLATION_CONNECTION	21	NULL	NO	varchar	64	192	NULL	NULL	NULL	utf8mb3	utf8mb3_general_ci	varchar(64)	select
TRIGGERS	DATABASE_COLLATION	22	NULL	NO	varchar	64	192	NULL	NULL	NULL	utf8mb3	utf8mb3_general_ci	varchar(64)	select"
columns_metadata=$(run_mysql \
    "SELECT TABLE_NAME,COLUMN_NAME,ORDINAL_POSITION,COLUMN_DEFAULT,IS_NULLABLE,DATA_TYPE, "\
"CHARACTER_MAXIMUM_LENGTH,CHARACTER_OCTET_LENGTH,NUMERIC_PRECISION,NUMERIC_SCALE, "\
"DATETIME_PRECISION,CHARACTER_SET_NAME,COLLATION_NAME,COLUMN_TYPE,PRIVILEGES "\
"FROM INFORMATION_SCHEMA.COLUMNS "\
"WHERE TABLE_SCHEMA = 'information_schema' AND TABLE_NAME = 'TRIGGERS' "\
"ORDER BY ORDINAL_POSITION;")
expect_value "triggers columns metadata" "$expected_columns_metadata" "$columns_metadata"

real_trigger_count=$(run_mysql \
    "SELECT COUNT(*) FROM INFORMATION_SCHEMA.TRIGGERS WHERE TRIGGER_SCHEMA = '${DATABASE}';")
expect_value "mysql real trigger observation" "1" "$real_trigger_count"

printf '%s\n' "mysql_baseline_information_schema_triggers_expectations: ok"
