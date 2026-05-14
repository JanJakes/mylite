#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_information_schema_routines_expectations_$$"

fail() {
    printf '%s\n' "mysql_baseline_information_schema_routines_expectations: $1" >&2
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
        *) fail "$label: expected error $code/$state containing [$message], got [$output]" ;;
    esac
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

expected_routines_columns="SPECIFIC_NAME	ROUTINE_CATALOG	ROUTINE_SCHEMA	ROUTINE_NAME	ROUTINE_TYPE	DATA_TYPE	CHARACTER_MAXIMUM_LENGTH	CHARACTER_OCTET_LENGTH	NUMERIC_PRECISION	NUMERIC_SCALE	DATETIME_PRECISION	CHARACTER_SET_NAME	COLLATION_NAME	DTD_IDENTIFIER	ROUTINE_BODY	ROUTINE_DEFINITION	EXTERNAL_NAME	EXTERNAL_LANGUAGE	PARAMETER_STYLE	IS_DETERMINISTIC	SQL_DATA_ACCESS	SQL_PATH	SECURITY_TYPE	CREATED	LAST_ALTERED	SQL_MODE	ROUTINE_COMMENT	DEFINER	CHARACTER_SET_CLIENT	COLLATION_CONNECTION	DATABASE_COLLATION"
count=$(run_mysql \
    "SELECT COUNT(*) FROM INFORMATION_SCHEMA.ROUTINES WHERE ROUTINE_SCHEMA = '${DATABASE}';")
expect_value "empty routines count" "0" "$count"

case_count=$(run_mysql \
    "SELECT COUNT(*) FROM INFORMATION_SCHEMA.routines WHERE ROUTINE_SCHEMA = '${DATABASE}';")
expect_value "case-insensitive table name count" "0" "$case_count"

status=$(run_mysql \
    "SELECT * FROM INFORMATION_SCHEMA.ROUTINES WHERE ROUTINE_SCHEMA = '${DATABASE}'; "\
"SELECT @@warning_count, ROW_COUNT();" | tail -n 1)
expect_value "routines status" "0	-1" "$status"

expect_error \
    "unknown routines where column" \
    1054 \
    "42S22" \
    "Unknown column 'nope' in 'where clause'" \
    "SELECT ROUTINE_NAME FROM INFORMATION_SCHEMA.ROUTINES WHERE nope = 'x';"

run_mysql \
    "CREATE PROCEDURE ${DATABASE}.routine_proc() SELECT 1; "\
"CREATE FUNCTION ${DATABASE}.routine_func() RETURNS INT DETERMINISTIC RETURN 1;" \
    >/dev/null

routines_output=$(run_mysql_with_headers \
    "SELECT * FROM INFORMATION_SCHEMA.ROUTINES WHERE ROUTINE_SCHEMA = '${DATABASE}';")
expect_value \
    "routines headers" \
    "$expected_routines_columns" \
    "$(printf '%s\n' "$routines_output" | sed -n '1p')"

alias_output=$(run_mysql_with_headers \
    "SELECT r.ROUTINE_NAME FROM INFORMATION_SCHEMA.ROUTINES AS r "\
"WHERE r.ROUTINE_SCHEMA = '${DATABASE}' ORDER BY r.ROUTINE_NAME LIMIT 1;")
expect_value "alias headers" "ROUTINE_NAME" "$(printf '%s\n' "$alias_output" | sed -n '1p')"

system_table_row=$(run_mysql \
    "SELECT TABLE_SCHEMA,TABLE_NAME,TABLE_TYPE,ENGINE,VERSION,ROW_FORMAT,TABLE_ROWS,DATA_LENGTH,AUTO_INCREMENT "\
"FROM INFORMATION_SCHEMA.TABLES WHERE TABLE_SCHEMA = 'information_schema' AND TABLE_NAME = 'ROUTINES';")
expect_value "routines system table row" \
    "information_schema	ROUTINES	SYSTEM VIEW	NULL	10	NULL	0	0	NULL" \
    "$system_table_row"

expected_columns_metadata="ROUTINES	SPECIFIC_NAME	1	NULL	NO	varchar	64	192	NULL	NULL	NULL	utf8mb3	utf8mb3_general_ci	varchar(64)	select
ROUTINES	ROUTINE_CATALOG	2	NULL	NO	varchar	64	192	NULL	NULL	NULL	utf8mb3	utf8mb3_bin	varchar(64)	select
ROUTINES	ROUTINE_SCHEMA	3	NULL	NO	varchar	64	192	NULL	NULL	NULL	utf8mb3	utf8mb3_bin	varchar(64)	select
ROUTINES	ROUTINE_NAME	4	NULL	NO	varchar	64	192	NULL	NULL	NULL	utf8mb3	utf8mb3_general_ci	varchar(64)	select
ROUTINES	ROUTINE_TYPE	5	NULL	NO	enum	9	27	NULL	NULL	NULL	utf8mb3	utf8mb3_bin	enum('FUNCTION','PROCEDURE')	select
ROUTINES	DATA_TYPE	6	NULL	YES	longtext	4294967295	4294967295	NULL	NULL	NULL	utf8mb3	utf8mb3_bin	longtext	select
ROUTINES	CHARACTER_MAXIMUM_LENGTH	7	NULL	YES	bigint	NULL	NULL	19	0	NULL	NULL	NULL	bigint	select
ROUTINES	CHARACTER_OCTET_LENGTH	8	NULL	YES	bigint	NULL	NULL	19	0	NULL	NULL	NULL	bigint	select
ROUTINES	NUMERIC_PRECISION	9	NULL	YES	int	NULL	NULL	10	0	NULL	NULL	NULL	int unsigned	select
ROUTINES	NUMERIC_SCALE	10	NULL	YES	int	NULL	NULL	10	0	NULL	NULL	NULL	int unsigned	select
ROUTINES	DATETIME_PRECISION	11	NULL	YES	int	NULL	NULL	10	0	NULL	NULL	NULL	int unsigned	select
ROUTINES	CHARACTER_SET_NAME	12	NULL	YES	varchar	64	192	NULL	NULL	NULL	utf8mb3	utf8mb3_general_ci	varchar(64)	select
ROUTINES	COLLATION_NAME	13	NULL	YES	varchar	64	192	NULL	NULL	NULL	utf8mb3	utf8mb3_general_ci	varchar(64)	select
ROUTINES	DTD_IDENTIFIER	14	NULL	YES	longtext	4294967295	4294967295	NULL	NULL	NULL	utf8mb3	utf8mb3_bin	longtext	select
ROUTINES	ROUTINE_BODY	15		NO	varchar	8	24	NULL	NULL	NULL	utf8mb3	utf8mb3_general_ci	varchar(8)	select
ROUTINES	ROUTINE_DEFINITION	16	NULL	YES	longtext	4294967295	4294967295	NULL	NULL	NULL	utf8mb3	utf8mb3_bin	longtext	select
ROUTINES	EXTERNAL_NAME	17	NULL	YES	varbinary	0	0	NULL	NULL	NULL	NULL	NULL	varbinary(0)	select
ROUTINES	EXTERNAL_LANGUAGE	18	SQL	NO	varchar	64	192	NULL	NULL	NULL	utf8mb3	utf8mb3_bin	varchar(64)	select
ROUTINES	PARAMETER_STYLE	19		NO	varchar	3	9	NULL	NULL	NULL	utf8mb3	utf8mb3_general_ci	varchar(3)	select
ROUTINES	IS_DETERMINISTIC	20		NO	varchar	3	9	NULL	NULL	NULL	utf8mb3	utf8mb3_general_ci	varchar(3)	select
ROUTINES	SQL_DATA_ACCESS	21	NULL	NO	enum	17	51	NULL	NULL	NULL	utf8mb3	utf8mb3_bin	enum('CONTAINS SQL','NO SQL','READS SQL DATA','MODIFIES SQL DATA')	select
ROUTINES	SQL_PATH	22	NULL	YES	varbinary	0	0	NULL	NULL	NULL	NULL	NULL	varbinary(0)	select
ROUTINES	SECURITY_TYPE	23	NULL	NO	enum	7	21	NULL	NULL	NULL	utf8mb3	utf8mb3_bin	enum('DEFAULT','INVOKER','DEFINER')	select
ROUTINES	CREATED	24	NULL	NO	timestamp	NULL	NULL	NULL	NULL	0	NULL	NULL	timestamp	select
ROUTINES	LAST_ALTERED	25	NULL	NO	timestamp	NULL	NULL	NULL	NULL	0	NULL	NULL	timestamp	select
ROUTINES	SQL_MODE	26	NULL	NO	set	520	1560	NULL	NULL	NULL	utf8mb3	utf8mb3_bin	set('REAL_AS_FLOAT','PIPES_AS_CONCAT','ANSI_QUOTES','IGNORE_SPACE','NOT_USED','ONLY_FULL_GROUP_BY','NO_UNSIGNED_SUBTRACTION','NO_DIR_IN_CREATE','NOT_USED_9','NOT_USED_10','NOT_USED_11','NOT_USED_12','NOT_USED_13','NOT_USED_14','NOT_USED_15','NOT_USED_16','NOT_USED_17','NOT_USED_18','ANSI','NO_AUTO_VALUE_ON_ZERO','NO_BACKSLASH_ESCAPES','STRICT_TRANS_TABLES','STRICT_ALL_TABLES','NO_ZERO_IN_DATE','NO_ZERO_DATE','ALLOW_INVALID_DATES','ERROR_FOR_DIVISION_BY_ZERO','TRADITIONAL','NOT_USED_29','HIGH_NOT_PRECEDENCE','NO_ENGINE_SUBSTITUTION','PAD_CHAR_TO_FULL_LENGTH','TIME_TRUNCATE_FRACTIONAL')	select
ROUTINES	ROUTINE_COMMENT	27	NULL	NO	text	65535	65535	NULL	NULL	NULL	utf8mb3	utf8mb3_bin	text	select
ROUTINES	DEFINER	28	NULL	NO	varchar	288	864	NULL	NULL	NULL	utf8mb3	utf8mb3_bin	varchar(288)	select
ROUTINES	CHARACTER_SET_CLIENT	29	NULL	NO	varchar	64	192	NULL	NULL	NULL	utf8mb3	utf8mb3_general_ci	varchar(64)	select
ROUTINES	COLLATION_CONNECTION	30	NULL	NO	varchar	64	192	NULL	NULL	NULL	utf8mb3	utf8mb3_general_ci	varchar(64)	select
ROUTINES	DATABASE_COLLATION	31	NULL	NO	varchar	64	192	NULL	NULL	NULL	utf8mb3	utf8mb3_general_ci	varchar(64)	select"
columns_metadata=$(run_mysql \
    "SELECT TABLE_NAME,COLUMN_NAME,ORDINAL_POSITION,COLUMN_DEFAULT,IS_NULLABLE,DATA_TYPE, "\
"CHARACTER_MAXIMUM_LENGTH,CHARACTER_OCTET_LENGTH,NUMERIC_PRECISION,NUMERIC_SCALE, "\
"DATETIME_PRECISION,CHARACTER_SET_NAME,COLLATION_NAME,COLUMN_TYPE,PRIVILEGES "\
"FROM INFORMATION_SCHEMA.COLUMNS "\
"WHERE TABLE_SCHEMA = 'information_schema' AND TABLE_NAME = 'ROUTINES' "\
"ORDER BY ORDINAL_POSITION;")
expect_value "routines columns metadata" "$expected_columns_metadata" "$columns_metadata"

real_routine_count=$(run_mysql \
    "SELECT COUNT(*) FROM INFORMATION_SCHEMA.ROUTINES WHERE ROUTINE_SCHEMA = '${DATABASE}';")
expect_value "mysql real routine observation" "2" "$real_routine_count"

real_routine_sample=$(run_mysql \
    "SELECT ROUTINE_NAME,ROUTINE_TYPE,DATA_TYPE,DTD_IDENTIFIER,ROUTINE_BODY,IS_DETERMINISTIC,SQL_DATA_ACCESS,SECURITY_TYPE "\
"FROM INFORMATION_SCHEMA.ROUTINES WHERE ROUTINE_SCHEMA = '${DATABASE}' ORDER BY ROUTINE_NAME;")
expect_value "mysql real routine sample" \
    "routine_func	FUNCTION	int	int	SQL	YES	CONTAINS SQL	DEFINER
routine_proc	PROCEDURE		NULL	SQL	NO	CONTAINS SQL	DEFINER" \
    "$real_routine_sample"

printf '%s\n' "mysql_baseline_information_schema_routines_expectations: ok"
