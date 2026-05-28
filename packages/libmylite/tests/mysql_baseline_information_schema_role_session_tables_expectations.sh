#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"

fail() {
    printf '%s\n' \
        "mysql_baseline_information_schema_role_session_tables_expectations: $1" >&2
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
        *)
            fail "$label: expected error $code/$state containing [$message], got [$output]"
            ;;
    esac
}

version=$(run_mysql "SELECT VERSION();")
case "$version" in
    8.4.9*) ;;
    *) fail "expected MySQL 8.4.9 runtime, got [$version]" ;;
esac

counts=$(run_mysql \
    "SELECT 'ADMINISTRABLE_ROLE_AUTHORIZATIONS', COUNT(*) "\
"FROM INFORMATION_SCHEMA.ADMINISTRABLE_ROLE_AUTHORIZATIONS; "\
"SELECT 'APPLICABLE_ROLES', COUNT(*) FROM INFORMATION_SCHEMA.APPLICABLE_ROLES; "\
"SELECT 'ENABLED_ROLES', COUNT(*) FROM INFORMATION_SCHEMA.ENABLED_ROLES;")
expected_counts="ADMINISTRABLE_ROLE_AUTHORIZATIONS	0
APPLICABLE_ROLES	0
ENABLED_ROLES	0"
expect_value "role session baseline counts" "$expected_counts" "$counts"

status=$(run_mysql \
    "SELECT * FROM INFORMATION_SCHEMA.ENABLED_ROLES WHERE ROLE_NAME = 'mylite_role'; "\
"SELECT @@warning_count, ROW_COUNT();" | tail -n 1)
expect_value "role session status" "0	-1" "$status"

expect_error \
    "role session unknown predicate column" \
    1054 \
    "42S22" \
    "Unknown column 'nope' in 'where clause'" \
    "SELECT ROLE_NAME FROM INFORMATION_SCHEMA.ENABLED_ROLES WHERE nope = 'x';"

system_table_rows=$(run_mysql \
    "SELECT TABLE_NAME,TABLE_TYPE,ENGINE,VERSION,ROW_FORMAT,TABLE_ROWS,DATA_LENGTH "\
"FROM INFORMATION_SCHEMA.TABLES "\
"WHERE TABLE_SCHEMA = 'information_schema' "\
"AND TABLE_NAME IN ('ADMINISTRABLE_ROLE_AUTHORIZATIONS','APPLICABLE_ROLES', "\
"'ENABLED_ROLES') "\
"ORDER BY TABLE_NAME;")
expected_system_table_rows="ADMINISTRABLE_ROLE_AUTHORIZATIONS	SYSTEM VIEW	NULL	10	NULL	0	0
APPLICABLE_ROLES	SYSTEM VIEW	NULL	10	NULL	0	0
ENABLED_ROLES	SYSTEM VIEW	NULL	10	NULL	0	0"
expect_value "role session system table rows" \
    "$expected_system_table_rows" \
    "$system_table_rows"

columns_metadata=$(run_mysql \
    "SELECT TABLE_NAME,COLUMN_NAME,ORDINAL_POSITION,COLUMN_DEFAULT,IS_NULLABLE,DATA_TYPE, "\
"CHARACTER_MAXIMUM_LENGTH,CHARACTER_OCTET_LENGTH,NUMERIC_PRECISION,NUMERIC_SCALE, "\
"DATETIME_PRECISION,CHARACTER_SET_NAME,COLLATION_NAME,COLUMN_TYPE,PRIVILEGES "\
"FROM INFORMATION_SCHEMA.COLUMNS "\
"WHERE TABLE_SCHEMA = 'information_schema' "\
"AND TABLE_NAME IN ('ADMINISTRABLE_ROLE_AUTHORIZATIONS','APPLICABLE_ROLES', "\
"'ENABLED_ROLES') "\
"ORDER BY TABLE_NAME, ORDINAL_POSITION;")
expected_columns_metadata="ADMINISTRABLE_ROLE_AUTHORIZATIONS	USER	1	NULL	YES	varchar	97	291	NULL	NULL	NULL	utf8mb3	utf8mb3_general_ci	varchar(97)	select
ADMINISTRABLE_ROLE_AUTHORIZATIONS	HOST	2	NULL	YES	varchar	256	768	NULL	NULL	NULL	utf8mb3	utf8mb3_general_ci	varchar(256)	select
ADMINISTRABLE_ROLE_AUTHORIZATIONS	GRANTEE	3	NULL	YES	varchar	97	388	NULL	NULL	NULL	utf8mb4	utf8mb4_0900_ai_ci	varchar(97)	select
ADMINISTRABLE_ROLE_AUTHORIZATIONS	GRANTEE_HOST	4	NULL	YES	varchar	256	1024	NULL	NULL	NULL	utf8mb4	utf8mb4_0900_ai_ci	varchar(256)	select
ADMINISTRABLE_ROLE_AUTHORIZATIONS	ROLE_NAME	5	NULL	YES	varchar	255	1020	NULL	NULL	NULL	utf8mb4	utf8mb4_0900_ai_ci	varchar(255)	select
ADMINISTRABLE_ROLE_AUTHORIZATIONS	ROLE_HOST	6	NULL	YES	varchar	256	1024	NULL	NULL	NULL	utf8mb4	utf8mb4_0900_ai_ci	varchar(256)	select
ADMINISTRABLE_ROLE_AUTHORIZATIONS	IS_GRANTABLE	7		NO	varchar	3	9	NULL	NULL	NULL	utf8mb3	utf8mb3_general_ci	varchar(3)	select
ADMINISTRABLE_ROLE_AUTHORIZATIONS	IS_DEFAULT	8	NULL	YES	varchar	3	9	NULL	NULL	NULL	utf8mb3	utf8mb3_general_ci	varchar(3)	select
ADMINISTRABLE_ROLE_AUTHORIZATIONS	IS_MANDATORY	9		NO	varchar	3	9	NULL	NULL	NULL	utf8mb3	utf8mb3_general_ci	varchar(3)	select
APPLICABLE_ROLES	USER	1	NULL	YES	varchar	97	291	NULL	NULL	NULL	utf8mb3	utf8mb3_general_ci	varchar(97)	select
APPLICABLE_ROLES	HOST	2	NULL	YES	varchar	256	768	NULL	NULL	NULL	utf8mb3	utf8mb3_general_ci	varchar(256)	select
APPLICABLE_ROLES	GRANTEE	3	NULL	YES	varchar	97	388	NULL	NULL	NULL	utf8mb4	utf8mb4_0900_ai_ci	varchar(97)	select
APPLICABLE_ROLES	GRANTEE_HOST	4	NULL	YES	varchar	256	1024	NULL	NULL	NULL	utf8mb4	utf8mb4_0900_ai_ci	varchar(256)	select
APPLICABLE_ROLES	ROLE_NAME	5	NULL	YES	varchar	255	1020	NULL	NULL	NULL	utf8mb4	utf8mb4_0900_ai_ci	varchar(255)	select
APPLICABLE_ROLES	ROLE_HOST	6	NULL	YES	varchar	256	1024	NULL	NULL	NULL	utf8mb4	utf8mb4_0900_ai_ci	varchar(256)	select
APPLICABLE_ROLES	IS_GRANTABLE	7		NO	varchar	3	9	NULL	NULL	NULL	utf8mb3	utf8mb3_general_ci	varchar(3)	select
APPLICABLE_ROLES	IS_DEFAULT	8	NULL	YES	varchar	3	9	NULL	NULL	NULL	utf8mb3	utf8mb3_general_ci	varchar(3)	select
APPLICABLE_ROLES	IS_MANDATORY	9		NO	varchar	3	9	NULL	NULL	NULL	utf8mb3	utf8mb3_general_ci	varchar(3)	select
ENABLED_ROLES	ROLE_NAME	1	NULL	YES	varchar	255	1020	NULL	NULL	NULL	utf8mb4	utf8mb4_0900_ai_ci	varchar(255)	select
ENABLED_ROLES	ROLE_HOST	2	NULL	YES	varchar	255	1020	NULL	NULL	NULL	utf8mb4	utf8mb4_0900_ai_ci	varchar(255)	select
ENABLED_ROLES	IS_DEFAULT	3	NULL	YES	varchar	3	9	NULL	NULL	NULL	utf8mb3	utf8mb3_general_ci	varchar(3)	select
ENABLED_ROLES	IS_MANDATORY	4		NO	varchar	3	9	NULL	NULL	NULL	utf8mb3	utf8mb3_general_ci	varchar(3)	select"
expect_value "role session columns metadata" \
    "$expected_columns_metadata" \
    "$columns_metadata"

printf '%s\n' \
    "mysql_baseline_information_schema_role_session_tables_expectations: ok"
