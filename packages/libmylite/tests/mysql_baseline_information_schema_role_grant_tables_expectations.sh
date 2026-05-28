#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"

fail() {
    printf '%s\n' \
        "mysql_baseline_information_schema_role_grant_tables_expectations: $1" >&2
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
    "SELECT 'ROLE_COLUMN_GRANTS', COUNT(*) FROM INFORMATION_SCHEMA.ROLE_COLUMN_GRANTS; "\
"SELECT 'ROLE_ROUTINE_GRANTS', COUNT(*) FROM INFORMATION_SCHEMA.ROLE_ROUTINE_GRANTS; "\
"SELECT 'ROLE_TABLE_GRANTS', COUNT(*) FROM INFORMATION_SCHEMA.ROLE_TABLE_GRANTS;")
expected_counts="ROLE_COLUMN_GRANTS	0
ROLE_ROUTINE_GRANTS	0
ROLE_TABLE_GRANTS	0"
expect_value "role grant baseline counts" "$expected_counts" "$counts"

status=$(run_mysql \
    "SELECT * FROM INFORMATION_SCHEMA.ROLE_TABLE_GRANTS WHERE GRANTEE = 'mylite_role'; "\
"SELECT @@warning_count, ROW_COUNT();" | tail -n 1)
expect_value "role grant status" "0	-1" "$status"

expect_error \
    "role grant unknown predicate column" \
    1054 \
    "42S22" \
    "Unknown column 'nope' in 'where clause'" \
    "SELECT GRANTEE FROM INFORMATION_SCHEMA.ROLE_TABLE_GRANTS WHERE nope = 'x';"

system_table_rows=$(run_mysql \
    "SELECT TABLE_NAME,TABLE_TYPE,ENGINE,VERSION,ROW_FORMAT,TABLE_ROWS,DATA_LENGTH "\
"FROM INFORMATION_SCHEMA.TABLES "\
"WHERE TABLE_SCHEMA = 'information_schema' "\
"AND TABLE_NAME IN ('ROLE_COLUMN_GRANTS','ROLE_ROUTINE_GRANTS','ROLE_TABLE_GRANTS') "\
"ORDER BY TABLE_NAME;")
expected_system_table_rows="ROLE_COLUMN_GRANTS	SYSTEM VIEW	NULL	10	NULL	0	0
ROLE_ROUTINE_GRANTS	SYSTEM VIEW	NULL	10	NULL	0	0
ROLE_TABLE_GRANTS	SYSTEM VIEW	NULL	10	NULL	0	0"
expect_value "role grant system table rows" \
    "$expected_system_table_rows" \
    "$system_table_rows"

columns_metadata=$(run_mysql \
    "SELECT TABLE_NAME,COLUMN_NAME,ORDINAL_POSITION,COLUMN_DEFAULT,IS_NULLABLE,DATA_TYPE, "\
"CHARACTER_MAXIMUM_LENGTH,CHARACTER_OCTET_LENGTH,NUMERIC_PRECISION,NUMERIC_SCALE, "\
"DATETIME_PRECISION,CHARACTER_SET_NAME,COLLATION_NAME,COLUMN_TYPE,PRIVILEGES "\
"FROM INFORMATION_SCHEMA.COLUMNS "\
"WHERE TABLE_SCHEMA = 'information_schema' "\
"AND TABLE_NAME IN ('ROLE_COLUMN_GRANTS','ROLE_ROUTINE_GRANTS','ROLE_TABLE_GRANTS') "\
"ORDER BY TABLE_NAME, ORDINAL_POSITION;")
expected_columns_metadata="ROLE_COLUMN_GRANTS	GRANTOR	1	NULL	YES	varchar	97	291	NULL	NULL	NULL	utf8mb3	utf8mb3_general_ci	varchar(97)	select
ROLE_COLUMN_GRANTS	GRANTOR_HOST	2	NULL	YES	varchar	256	768	NULL	NULL	NULL	utf8mb3	utf8mb3_general_ci	varchar(256)	select
ROLE_COLUMN_GRANTS	GRANTEE	3		NO	char	32	96	NULL	NULL	NULL	utf8mb3	utf8mb3_bin	char(32)	select
ROLE_COLUMN_GRANTS	GRANTEE_HOST	4		NO	char	255	255	NULL	NULL	NULL	ascii	ascii_general_ci	char(255)	select
ROLE_COLUMN_GRANTS	TABLE_CATALOG	5		NO	varchar	3	9	NULL	NULL	NULL	utf8mb3	utf8mb3_general_ci	varchar(3)	select
ROLE_COLUMN_GRANTS	TABLE_SCHEMA	6		NO	char	64	192	NULL	NULL	NULL	utf8mb3	utf8mb3_bin	char(64)	select
ROLE_COLUMN_GRANTS	TABLE_NAME	7		NO	char	64	192	NULL	NULL	NULL	utf8mb3	utf8mb3_bin	char(64)	select
ROLE_COLUMN_GRANTS	COLUMN_NAME	8		NO	char	64	192	NULL	NULL	NULL	utf8mb3	utf8mb3_bin	char(64)	select
ROLE_COLUMN_GRANTS	PRIVILEGE_TYPE	9		NO	set	31	93	NULL	NULL	NULL	utf8mb3	utf8mb3_general_ci	set('Select','Insert','Update','References')	select
ROLE_COLUMN_GRANTS	IS_GRANTABLE	10		NO	varchar	3	9	NULL	NULL	NULL	utf8mb3	utf8mb3_general_ci	varchar(3)	select
ROLE_ROUTINE_GRANTS	GRANTOR	1	NULL	YES	varchar	97	291	NULL	NULL	NULL	utf8mb3	utf8mb3_general_ci	varchar(97)	select
ROLE_ROUTINE_GRANTS	GRANTOR_HOST	2	NULL	YES	varchar	256	768	NULL	NULL	NULL	utf8mb3	utf8mb3_general_ci	varchar(256)	select
ROLE_ROUTINE_GRANTS	GRANTEE	3		NO	char	32	96	NULL	NULL	NULL	utf8mb3	utf8mb3_bin	char(32)	select
ROLE_ROUTINE_GRANTS	GRANTEE_HOST	4		NO	char	255	255	NULL	NULL	NULL	ascii	ascii_general_ci	char(255)	select
ROLE_ROUTINE_GRANTS	SPECIFIC_CATALOG	5		NO	varchar	3	9	NULL	NULL	NULL	utf8mb3	utf8mb3_general_ci	varchar(3)	select
ROLE_ROUTINE_GRANTS	SPECIFIC_SCHEMA	6		NO	char	64	192	NULL	NULL	NULL	utf8mb3	utf8mb3_bin	char(64)	select
ROLE_ROUTINE_GRANTS	SPECIFIC_NAME	7		NO	char	64	192	NULL	NULL	NULL	utf8mb3	utf8mb3_general_ci	char(64)	select
ROLE_ROUTINE_GRANTS	ROUTINE_CATALOG	8		NO	varchar	3	9	NULL	NULL	NULL	utf8mb3	utf8mb3_general_ci	varchar(3)	select
ROLE_ROUTINE_GRANTS	ROUTINE_SCHEMA	9		NO	char	64	192	NULL	NULL	NULL	utf8mb3	utf8mb3_bin	char(64)	select
ROLE_ROUTINE_GRANTS	ROUTINE_NAME	10		NO	char	64	192	NULL	NULL	NULL	utf8mb3	utf8mb3_general_ci	char(64)	select
ROLE_ROUTINE_GRANTS	PRIVILEGE_TYPE	11		NO	set	27	81	NULL	NULL	NULL	utf8mb3	utf8mb3_general_ci	set('Execute','Alter Routine','Grant')	select
ROLE_ROUTINE_GRANTS	IS_GRANTABLE	12		NO	varchar	3	9	NULL	NULL	NULL	utf8mb3	utf8mb3_general_ci	varchar(3)	select
ROLE_TABLE_GRANTS	GRANTOR	1	NULL	YES	varchar	97	291	NULL	NULL	NULL	utf8mb3	utf8mb3_general_ci	varchar(97)	select
ROLE_TABLE_GRANTS	GRANTOR_HOST	2	NULL	YES	varchar	256	768	NULL	NULL	NULL	utf8mb3	utf8mb3_general_ci	varchar(256)	select
ROLE_TABLE_GRANTS	GRANTEE	3		NO	char	32	96	NULL	NULL	NULL	utf8mb3	utf8mb3_bin	char(32)	select
ROLE_TABLE_GRANTS	GRANTEE_HOST	4		NO	char	255	255	NULL	NULL	NULL	ascii	ascii_general_ci	char(255)	select
ROLE_TABLE_GRANTS	TABLE_CATALOG	5		NO	varchar	3	9	NULL	NULL	NULL	utf8mb3	utf8mb3_general_ci	varchar(3)	select
ROLE_TABLE_GRANTS	TABLE_SCHEMA	6		NO	char	64	192	NULL	NULL	NULL	utf8mb3	utf8mb3_bin	char(64)	select
ROLE_TABLE_GRANTS	TABLE_NAME	7		NO	char	64	192	NULL	NULL	NULL	utf8mb3	utf8mb3_bin	char(64)	select
ROLE_TABLE_GRANTS	PRIVILEGE_TYPE	8		NO	set	98	294	NULL	NULL	NULL	utf8mb3	utf8mb3_general_ci	set('Select','Insert','Update','Delete','Create','Drop','Grant','References','Index','Alter','Create View','Show view','Trigger')	select
ROLE_TABLE_GRANTS	IS_GRANTABLE	9		NO	varchar	3	9	NULL	NULL	NULL	utf8mb3	utf8mb3_general_ci	varchar(3)	select"
expect_value "role grant columns metadata" \
    "$expected_columns_metadata" \
    "$columns_metadata"

printf '%s\n' \
    "mysql_baseline_information_schema_role_grant_tables_expectations: ok"
