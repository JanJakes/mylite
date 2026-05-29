#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"

fail() {
    printf '%s\n' \
        "mysql_baseline_information_schema_connection_control_failed_login_attempts_expectations: $1" >&2
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

ensure_plugin_active() {
    plugin_name=$1
    plugin_library=$2

    plugin_row=$(run_mysql \
        "SELECT COALESCE(MAX(PLUGIN_STATUS), '') FROM INFORMATION_SCHEMA.PLUGINS "\
"WHERE PLUGIN_NAME = '${plugin_name}';")
    case "$plugin_row" in
        ACTIVE) return 0 ;;
        "") ;;
        *) fail "${plugin_name} plugin status is ${plugin_row}" ;;
    esac

    run_mysql "INSTALL PLUGIN ${plugin_name} SONAME '${plugin_library}';" >/dev/null

    plugin_row=$(run_mysql \
        "SELECT COALESCE(MAX(PLUGIN_STATUS), '') FROM INFORMATION_SCHEMA.PLUGINS "\
"WHERE PLUGIN_NAME = '${plugin_name}';")
    expect_value "${plugin_name} plugin active after install" "ACTIVE" "$plugin_row"
}

version=$(run_mysql 'SELECT VERSION();')
case "$version" in
    8.4.9*) ;;
    *) fail "expected MySQL 8.4.9 runtime, got [$version]" ;;
esac

ensure_plugin_active "CONNECTION_CONTROL" "connection_control.so"
ensure_plugin_active "CONNECTION_CONTROL_FAILED_LOGIN_ATTEMPTS" "connection_control.so"

plugin_rows=$(run_mysql \
    "SELECT PLUGIN_NAME,PLUGIN_STATUS,PLUGIN_TYPE,PLUGIN_LIBRARY "\
"FROM INFORMATION_SCHEMA.PLUGINS "\
"WHERE PLUGIN_NAME LIKE 'CONNECTION_CONTROL%' ORDER BY PLUGIN_NAME;")
expected_plugin_rows="CONNECTION_CONTROL	ACTIVE	AUDIT	connection_control.so
CONNECTION_CONTROL_FAILED_LOGIN_ATTEMPTS	ACTIVE	INFORMATION SCHEMA	connection_control.so"
expect_value "connection-control plugin rows" "$expected_plugin_rows" "$plugin_rows"

table_kind=$(run_mysql \
    "SHOW FULL TABLES FROM INFORMATION_SCHEMA WHERE Tables_in_INFORMATION_SCHEMA = "\
"'CONNECTION_CONTROL_FAILED_LOGIN_ATTEMPTS';")
expect_value "connection-control failed login attempts table kind" \
    "CONNECTION_CONTROL_FAILED_LOGIN_ATTEMPTS	SYSTEM VIEW" \
    "$table_kind"

system_table_row=$(run_mysql \
    "SELECT TABLE_SCHEMA,TABLE_NAME,TABLE_TYPE,ENGINE,VERSION,ROW_FORMAT,TABLE_ROWS, "\
"DATA_LENGTH,AUTO_INCREMENT FROM INFORMATION_SCHEMA.TABLES "\
"WHERE TABLE_SCHEMA = 'information_schema' "\
"AND TABLE_NAME = 'CONNECTION_CONTROL_FAILED_LOGIN_ATTEMPTS';")
expect_value "connection-control failed login attempts system table row" \
    "information_schema	CONNECTION_CONTROL_FAILED_LOGIN_ATTEMPTS	SYSTEM VIEW	NULL	10	NULL	0	0	NULL" \
    "$system_table_row"

columns_metadata=$(run_mysql \
    "SELECT TABLE_NAME,COLUMN_NAME,ORDINAL_POSITION,COLUMN_DEFAULT,IS_NULLABLE,DATA_TYPE, "\
"CHARACTER_MAXIMUM_LENGTH,CHARACTER_OCTET_LENGTH,NUMERIC_PRECISION,NUMERIC_SCALE, "\
"DATETIME_PRECISION,CHARACTER_SET_NAME,COLLATION_NAME,COLUMN_TYPE,PRIVILEGES "\
"FROM INFORMATION_SCHEMA.COLUMNS "\
"WHERE TABLE_SCHEMA = 'information_schema' "\
"AND TABLE_NAME = 'CONNECTION_CONTROL_FAILED_LOGIN_ATTEMPTS' ORDER BY ORDINAL_POSITION;")
expected_columns_metadata="CONNECTION_CONTROL_FAILED_LOGIN_ATTEMPTS	USERHOST	1		NO	varchar	119	357	NULL	NULL	NULL	utf8mb3	utf8mb3_general_ci	varchar(357)	select
CONNECTION_CONTROL_FAILED_LOGIN_ATTEMPTS	FAILED_ATTEMPTS	2		NO	int	NULL	NULL	NULL	NULL	NULL	NULL	NULL	int unsigned	select"
expect_value "connection-control failed login attempts columns metadata" \
    "$expected_columns_metadata" \
    "$columns_metadata"

attempt_count=$(run_mysql \
    "SELECT COUNT(*) FROM INFORMATION_SCHEMA.CONNECTION_CONTROL_FAILED_LOGIN_ATTEMPTS;")
expect_value "connection-control failed login attempts row count" "0" "$attempt_count"

unqualified_count=$(run_mysql \
    "USE information_schema; "\
"SELECT COUNT(*) FROM CONNECTION_CONTROL_FAILED_LOGIN_ATTEMPTS WHERE FAILED_ATTEMPTS > 0;")
expect_value "connection-control failed login attempts unqualified count" \
    "0" \
    "$unqualified_count"

status=$(run_mysql \
    "SELECT COUNT(*) FROM INFORMATION_SCHEMA.CONNECTION_CONTROL_FAILED_LOGIN_ATTEMPTS; "\
"SELECT @@warning_count, ROW_COUNT();" | tail -n 1)
expect_value "connection-control failed login attempts warning and row count status" \
    "0	-1" \
    "$status"

printf '%s\n' \
    "mysql_baseline_information_schema_connection_control_failed_login_attempts_expectations: ok"
