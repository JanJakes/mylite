#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"

fail() {
    printf '%s\n' "mysql_baseline_information_schema_resource_groups_expectations: $1" >&2
    exit 1
}

run_mysql() {
    sql=$1
    shift
    printf '%s\n' "$sql" \
        | docker exec -i "$MYSQL_CONTAINER" mysql --protocol=TCP -h127.0.0.1 -uroot --batch --raw --skip-column-names "$@"
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

system_table_row=$(run_mysql \
    "SELECT TABLE_SCHEMA,TABLE_NAME,TABLE_TYPE,ENGINE,VERSION,ROW_FORMAT,TABLE_ROWS,DATA_LENGTH,AUTO_INCREMENT "\
"FROM INFORMATION_SCHEMA.TABLES WHERE TABLE_SCHEMA = 'information_schema' "\
"AND TABLE_NAME = 'RESOURCE_GROUPS';")
expect_value "resource groups system table row" \
    "information_schema	RESOURCE_GROUPS	SYSTEM VIEW	NULL	10	NULL	0	0	NULL" \
    "$system_table_row"

expected_columns_metadata="RESOURCE_GROUPS	RESOURCE_GROUP_NAME	1	NULL	NO	varchar	64	192	NULL	NULL	NULL	utf8mb3	utf8mb3_general_ci	varchar(64)	select
RESOURCE_GROUPS	RESOURCE_GROUP_TYPE	2	NULL	NO	enum	6	18	NULL	NULL	NULL	utf8mb3	utf8mb3_bin	enum('SYSTEM','USER')	select
RESOURCE_GROUPS	RESOURCE_GROUP_ENABLED	3	NULL	NO	tinyint	NULL	NULL	3	0	NULL	NULL	NULL	tinyint(1)	select
RESOURCE_GROUPS	VCPU_IDS	4	NULL	YES	blob	65535	65535	NULL	NULL	NULL	NULL	NULL	blob	select
RESOURCE_GROUPS	THREAD_PRIORITY	5	NULL	NO	int	NULL	NULL	10	0	NULL	NULL	NULL	int	select"
columns_metadata=$(run_mysql \
    "SELECT TABLE_NAME,COLUMN_NAME,ORDINAL_POSITION,COLUMN_DEFAULT,IS_NULLABLE,DATA_TYPE, "\
"CHARACTER_MAXIMUM_LENGTH,CHARACTER_OCTET_LENGTH,NUMERIC_PRECISION,NUMERIC_SCALE, "\
"DATETIME_PRECISION,CHARACTER_SET_NAME,COLLATION_NAME,COLUMN_TYPE,PRIVILEGES "\
"FROM INFORMATION_SCHEMA.COLUMNS "\
"WHERE TABLE_SCHEMA = 'information_schema' AND TABLE_NAME = 'RESOURCE_GROUPS' "\
"ORDER BY ORDINAL_POSITION;")
expect_value "resource groups columns metadata" "$expected_columns_metadata" "$columns_metadata"

vcpu_ids=$(run_mysql \
    "SELECT MIN(VCPU_IDS) FROM INFORMATION_SCHEMA.RESOURCE_GROUPS "\
"WHERE RESOURCE_GROUP_NAME IN ('SYS_default','USR_default');")
case "$vcpu_ids" in
    "" | "NULL") fail "expected non-NULL VCPU_IDS for default groups" ;;
esac

vcpu_distinct_count=$(run_mysql \
    "SELECT COUNT(DISTINCT VCPU_IDS) FROM INFORMATION_SCHEMA.RESOURCE_GROUPS "\
"WHERE RESOURCE_GROUP_NAME IN ('SYS_default','USR_default');")
expect_value "resource groups shared vcpu ids" "1" "$vcpu_distinct_count"

expected_rows="SYS_default	SYSTEM	1	$vcpu_ids	0
USR_default	USER	1	$vcpu_ids	0"
rows=$(run_mysql \
    "SELECT RESOURCE_GROUP_NAME,RESOURCE_GROUP_TYPE,RESOURCE_GROUP_ENABLED,VCPU_IDS,THREAD_PRIORITY "\
"FROM INFORMATION_SCHEMA.RESOURCE_GROUPS ORDER BY RESOURCE_GROUP_NAME;")
expect_value "resource groups default rows" "$expected_rows" "$rows"

case_count=$(run_mysql \
    "SELECT COUNT(*) FROM information_schema.resource_groups WHERE resource_group_type = 'USER';")
expect_value "case-insensitive resource groups count" "1" "$case_count"

alias_row=$(run_mysql \
    "SELECT r.THREAD_PRIORITY FROM INFORMATION_SCHEMA.RESOURCE_GROUPS AS r "\
"WHERE r.RESOURCE_GROUP_NAME = 'USR_default';")
expect_value "resource groups alias row" "0" "$alias_row"

status=$(run_mysql \
    "SELECT COUNT(*) FROM INFORMATION_SCHEMA.RESOURCE_GROUPS; "\
"SELECT @@warning_count, ROW_COUNT();" | tail -n 1)
expect_value "resource groups warning and row count status" "0	-1" "$status"

printf '%s\n' "mysql_baseline_information_schema_resource_groups_expectations: ok"
