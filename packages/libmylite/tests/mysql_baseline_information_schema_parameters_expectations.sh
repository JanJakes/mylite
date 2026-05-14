#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_information_schema_parameters_expectations_$$"

fail() {
    printf '%s\n' "mysql_baseline_information_schema_parameters_expectations: $1" >&2
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

expected_parameters_columns="SPECIFIC_CATALOG	SPECIFIC_SCHEMA	SPECIFIC_NAME	ORDINAL_POSITION	PARAMETER_MODE	PARAMETER_NAME	DATA_TYPE	CHARACTER_MAXIMUM_LENGTH	CHARACTER_OCTET_LENGTH	NUMERIC_PRECISION	NUMERIC_SCALE	DATETIME_PRECISION	CHARACTER_SET_NAME	COLLATION_NAME	DTD_IDENTIFIER	ROUTINE_TYPE"

count=$(run_mysql \
    "SELECT COUNT(*) FROM INFORMATION_SCHEMA.PARAMETERS WHERE SPECIFIC_SCHEMA = '${DATABASE}';")
expect_value "empty parameters count" "0" "$count"

case_count=$(run_mysql \
    "SELECT COUNT(*) FROM INFORMATION_SCHEMA.parameters WHERE SPECIFIC_SCHEMA = '${DATABASE}';")
expect_value "case-insensitive table name count" "0" "$case_count"

status=$(run_mysql \
    "SELECT * FROM INFORMATION_SCHEMA.PARAMETERS WHERE SPECIFIC_SCHEMA = '${DATABASE}'; "\
"SELECT @@warning_count, ROW_COUNT();" | tail -n 1)
expect_value "parameters status" "0	-1" "$status"

expect_error \
    "unknown parameters where column" \
    1054 \
    "42S22" \
    "Unknown column 'nope' in 'where clause'" \
    "SELECT SPECIFIC_NAME FROM INFORMATION_SCHEMA.PARAMETERS WHERE nope = 'x';"

run_mysql \
    "CREATE PROCEDURE ${DATABASE}.routine_proc(IN p_in INT, OUT p_out VARCHAR(10), INOUT p_io DECIMAL(5,2)) SELECT p_in; "\
"CREATE FUNCTION ${DATABASE}.routine_func(p_name VARCHAR(20)) RETURNS INT DETERMINISTIC RETURN 1;" \
    >/dev/null

parameters_output=$(run_mysql_with_headers \
    "SELECT * FROM INFORMATION_SCHEMA.PARAMETERS WHERE SPECIFIC_SCHEMA = '${DATABASE}';")
expect_value \
    "parameters headers" \
    "$expected_parameters_columns" \
    "$(printf '%s\n' "$parameters_output" | sed -n '1p')"

alias_output=$(run_mysql_with_headers \
    "SELECT p.SPECIFIC_NAME FROM INFORMATION_SCHEMA.PARAMETERS AS p "\
"WHERE p.SPECIFIC_SCHEMA = '${DATABASE}' ORDER BY p.SPECIFIC_NAME LIMIT 1;")
expect_value "alias headers" "SPECIFIC_NAME" "$(printf '%s\n' "$alias_output" | sed -n '1p')"

system_table_row=$(run_mysql \
    "SELECT TABLE_SCHEMA,TABLE_NAME,TABLE_TYPE,ENGINE,VERSION,ROW_FORMAT,TABLE_ROWS,DATA_LENGTH,AUTO_INCREMENT "\
"FROM INFORMATION_SCHEMA.TABLES WHERE TABLE_SCHEMA = 'information_schema' AND TABLE_NAME = 'PARAMETERS';")
expect_value "parameters system table row" \
    "information_schema	PARAMETERS	SYSTEM VIEW	NULL	10	NULL	0	0	NULL" \
    "$system_table_row"

expected_columns_metadata="PARAMETERS	SPECIFIC_CATALOG	1	NULL	NO	varchar	64	192	NULL	NULL	NULL	utf8mb3	utf8mb3_bin	varchar(64)	select
PARAMETERS	SPECIFIC_SCHEMA	2	NULL	NO	varchar	64	192	NULL	NULL	NULL	utf8mb3	utf8mb3_bin	varchar(64)	select
PARAMETERS	SPECIFIC_NAME	3	NULL	NO	varchar	64	192	NULL	NULL	NULL	utf8mb3	utf8mb3_general_ci	varchar(64)	select
PARAMETERS	ORDINAL_POSITION	4	0	NO	bigint	NULL	NULL	20	0	NULL	NULL	NULL	bigint unsigned	select
PARAMETERS	PARAMETER_MODE	5	NULL	YES	varchar	5	15	NULL	NULL	NULL	utf8mb3	utf8mb3_bin	varchar(5)	select
PARAMETERS	PARAMETER_NAME	6	NULL	YES	varchar	64	192	NULL	NULL	NULL	utf8mb3	utf8mb3_general_ci	varchar(64)	select
PARAMETERS	DATA_TYPE	7	NULL	YES	longtext	4294967295	4294967295	NULL	NULL	NULL	utf8mb3	utf8mb3_bin	longtext	select
PARAMETERS	CHARACTER_MAXIMUM_LENGTH	8	NULL	YES	bigint	NULL	NULL	19	0	NULL	NULL	NULL	bigint	select
PARAMETERS	CHARACTER_OCTET_LENGTH	9	NULL	YES	bigint	NULL	NULL	19	0	NULL	NULL	NULL	bigint	select
PARAMETERS	NUMERIC_PRECISION	10	NULL	YES	int	NULL	NULL	10	0	NULL	NULL	NULL	int unsigned	select
PARAMETERS	NUMERIC_SCALE	11	NULL	YES	bigint	NULL	NULL	19	0	NULL	NULL	NULL	bigint	select
PARAMETERS	DATETIME_PRECISION	12	NULL	YES	int	NULL	NULL	10	0	NULL	NULL	NULL	int unsigned	select
PARAMETERS	CHARACTER_SET_NAME	13	NULL	YES	varchar	64	192	NULL	NULL	NULL	utf8mb3	utf8mb3_general_ci	varchar(64)	select
PARAMETERS	COLLATION_NAME	14	NULL	YES	varchar	64	192	NULL	NULL	NULL	utf8mb3	utf8mb3_general_ci	varchar(64)	select
PARAMETERS	DTD_IDENTIFIER	15	NULL	NO	mediumtext	16777215	16777215	NULL	NULL	NULL	utf8mb3	utf8mb3_bin	mediumtext	select
PARAMETERS	ROUTINE_TYPE	16	NULL	NO	enum	9	27	NULL	NULL	NULL	utf8mb3	utf8mb3_bin	enum('FUNCTION','PROCEDURE')	select"
columns_metadata=$(run_mysql \
    "SELECT TABLE_NAME,COLUMN_NAME,ORDINAL_POSITION,COLUMN_DEFAULT,IS_NULLABLE,DATA_TYPE, "\
"CHARACTER_MAXIMUM_LENGTH,CHARACTER_OCTET_LENGTH,NUMERIC_PRECISION,NUMERIC_SCALE, "\
"DATETIME_PRECISION,CHARACTER_SET_NAME,COLLATION_NAME,COLUMN_TYPE,PRIVILEGES "\
"FROM INFORMATION_SCHEMA.COLUMNS "\
"WHERE TABLE_SCHEMA = 'information_schema' AND TABLE_NAME = 'PARAMETERS' "\
"ORDER BY ORDINAL_POSITION;")
expect_value "parameters columns metadata" "$expected_columns_metadata" "$columns_metadata"

real_parameter_count=$(run_mysql \
    "SELECT COUNT(*) FROM INFORMATION_SCHEMA.PARAMETERS WHERE SPECIFIC_SCHEMA = '${DATABASE}';")
expect_value "mysql real parameter observation" "5" "$real_parameter_count"

real_parameter_sample=$(run_mysql \
    "SELECT SPECIFIC_NAME,ORDINAL_POSITION,PARAMETER_MODE,PARAMETER_NAME,DATA_TYPE, "\
"CHARACTER_MAXIMUM_LENGTH,CHARACTER_OCTET_LENGTH,NUMERIC_PRECISION,NUMERIC_SCALE, "\
"DATETIME_PRECISION,CHARACTER_SET_NAME,COLLATION_NAME,DTD_IDENTIFIER,ROUTINE_TYPE "\
"FROM INFORMATION_SCHEMA.PARAMETERS WHERE SPECIFIC_SCHEMA = '${DATABASE}' "\
"ORDER BY SPECIFIC_NAME, ORDINAL_POSITION;")
expect_value "mysql real parameter sample" \
    "routine_func	0	NULL	NULL	int	NULL	NULL	10	0	NULL	NULL	NULL	int	FUNCTION
routine_func	1	IN	p_name	varchar	20	80	NULL	NULL	NULL	utf8mb4	utf8mb4_0900_ai_ci	varchar(20)	FUNCTION
routine_proc	1	IN	p_in	int	NULL	NULL	10	0	NULL	NULL	NULL	int	PROCEDURE
routine_proc	2	OUT	p_out	varchar	10	40	NULL	NULL	NULL	utf8mb4	utf8mb4_0900_ai_ci	varchar(10)	PROCEDURE
routine_proc	3	INOUT	p_io	decimal	NULL	NULL	5	2	NULL	NULL	NULL	decimal(5,2)	PROCEDURE" \
    "$real_parameter_sample"

printf '%s\n' "mysql_baseline_information_schema_parameters_expectations: ok"
