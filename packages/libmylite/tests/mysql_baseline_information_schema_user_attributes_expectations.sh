#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"

fail() {
    printf '%s\n' "mysql_baseline_information_schema_user_attributes_expectations: $1" >&2
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

root_row=$(run_mysql \
    "SELECT USER, HOST, ATTRIBUTE FROM INFORMATION_SCHEMA.USER_ATTRIBUTES "\
"WHERE USER = 'root' AND HOST = '%';")
expect_value "root user attribute row" "root	%	NULL" "$root_row"

all_rows=$(run_mysql \
    "SELECT USER, HOST, ATTRIBUTE FROM INFORMATION_SCHEMA.USER_ATTRIBUTES "\
"WHERE (USER = 'mysql.infoschema' AND HOST = 'localhost') "\
"OR (USER = 'mysql.session' AND HOST = 'localhost') "\
"OR (USER = 'mysql.sys' AND HOST = 'localhost') "\
"OR (USER = 'root' AND HOST IN ('%', 'localhost')) "\
"ORDER BY USER, HOST;")
expected_all_rows="mysql.infoschema	localhost	NULL
mysql.session	localhost	NULL
mysql.sys	localhost	NULL
root	%	NULL
root	localhost	NULL"
expect_value "default user attribute rows" "$expected_all_rows" "$all_rows"

status=$(run_mysql \
    "SELECT USER FROM INFORMATION_SCHEMA.USER_ATTRIBUTES "\
"WHERE USER = 'root' AND HOST = '%'; "\
"SELECT @@warning_count, ROW_COUNT();" | tail -n 1)
expect_value "user attributes status" "0	-1" "$status"

expect_error \
    "user attributes unknown predicate column" \
    1054 \
    "42S22" \
    "Unknown column 'nope' in 'where clause'" \
    "SELECT USER FROM INFORMATION_SCHEMA.USER_ATTRIBUTES WHERE nope = 'x';"

system_table_row=$(run_mysql \
    "SELECT TABLE_NAME,TABLE_TYPE,ENGINE,VERSION,ROW_FORMAT,TABLE_ROWS,DATA_LENGTH, "\
"LENGTH(CREATE_OPTIONS),LENGTH(TABLE_COMMENT) FROM INFORMATION_SCHEMA.TABLES "\
"WHERE TABLE_SCHEMA = 'information_schema' AND TABLE_NAME = 'USER_ATTRIBUTES';")
expect_value \
    "user attributes system table row" \
    "USER_ATTRIBUTES	SYSTEM VIEW	NULL	10	NULL	0	0	0	0" \
    "$system_table_row"

columns_metadata=$(run_mysql \
    "SELECT COLUMN_NAME,ORDINAL_POSITION,COLUMN_DEFAULT,IS_NULLABLE,DATA_TYPE, "\
"CHARACTER_MAXIMUM_LENGTH,CHARACTER_OCTET_LENGTH,NUMERIC_PRECISION,NUMERIC_SCALE, "\
"DATETIME_PRECISION,CHARACTER_SET_NAME,COLLATION_NAME,COLUMN_TYPE,PRIVILEGES "\
"FROM INFORMATION_SCHEMA.COLUMNS "\
"WHERE TABLE_SCHEMA = 'information_schema' AND TABLE_NAME = 'USER_ATTRIBUTES' "\
"ORDER BY ORDINAL_POSITION;")
expected_columns_metadata="USER	1		NO	char	32	96	NULL	NULL	NULL	utf8mb3	utf8mb3_bin	char(32)	select
HOST	2		NO	char	255	255	NULL	NULL	NULL	ascii	ascii_general_ci	char(255)	select
ATTRIBUTE	3	NULL	YES	longtext	4294967295	4294967295	NULL	NULL	NULL	utf8mb4	utf8mb4_bin	longtext	select"
expect_value "user attributes columns metadata" \
    "$expected_columns_metadata" \
    "$columns_metadata"

printf '%s\n' "mysql_baseline_information_schema_user_attributes_expectations: ok"
