#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_ext_attr_tables_$$"

fail() {
    printf '%s\n' \
        "mysql_baseline_information_schema_extension_attribute_tables_expectations: $1" >&2
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

cleanup() {
    run_mysql "DROP DATABASE IF EXISTS ${DATABASE};" >/dev/null 2>&1 || true
}

trap cleanup EXIT

version=$(run_mysql "SELECT VERSION();")
case "$version" in
    8.4.9*) ;;
    *) fail "expected MySQL 8.4.9 runtime, got [$version]" ;;
esac

cleanup
run_mysql "CREATE DATABASE ${DATABASE};
CREATE TABLE ${DATABASE}.parent (
    id INT,
    v INT,
    CONSTRAINT pk_parent PRIMARY KEY (id),
    CONSTRAINT uq_parent_v UNIQUE (v),
    CONSTRAINT chk_parent_v CHECK (v > 0)
);
CREATE TABLE ${DATABASE}.child (
    id INT,
    parent_id INT,
    CONSTRAINT pk_child PRIMARY KEY (id),
    CONSTRAINT fk_child_parent FOREIGN KEY (parent_id) REFERENCES ${DATABASE}.parent(id)
);
CREATE VIEW ${DATABASE}.v_parent AS SELECT id, v FROM ${DATABASE}.parent;" >/dev/null

tables_headers=$(run_mysql_with_headers \
    "SELECT * FROM INFORMATION_SCHEMA.TABLES_EXTENSIONS "\
"WHERE TABLE_SCHEMA = '${DATABASE}' AND TABLE_NAME = 'parent';" | sed -n '1p')
expect_value "tables extensions headers" \
    "TABLE_CATALOG	TABLE_SCHEMA	TABLE_NAME	ENGINE_ATTRIBUTE	SECONDARY_ENGINE_ATTRIBUTE" \
    "$tables_headers"

columns_headers=$(run_mysql_with_headers \
    "SELECT * FROM INFORMATION_SCHEMA.COLUMNS_EXTENSIONS "\
"WHERE TABLE_SCHEMA = '${DATABASE}' AND TABLE_NAME = 'parent' AND COLUMN_NAME = 'id';" \
    | sed -n '1p')
expect_value "columns extensions headers" \
    "TABLE_CATALOG	TABLE_SCHEMA	TABLE_NAME	COLUMN_NAME	ENGINE_ATTRIBUTE	SECONDARY_ENGINE_ATTRIBUTE" \
    "$columns_headers"

constraints_headers=$(run_mysql_with_headers \
    "SELECT * FROM INFORMATION_SCHEMA.TABLE_CONSTRAINTS_EXTENSIONS "\
"WHERE CONSTRAINT_SCHEMA = '${DATABASE}' AND TABLE_NAME = 'parent' "\
"AND CONSTRAINT_NAME = 'PRIMARY';" | sed -n '1p')
expect_value "table constraints extensions headers" \
    "CONSTRAINT_CATALOG	CONSTRAINT_SCHEMA	CONSTRAINT_NAME	TABLE_NAME	ENGINE_ATTRIBUTE	SECONDARY_ENGINE_ATTRIBUTE" \
    "$constraints_headers"

tables_rows=$(run_mysql \
    "SELECT TABLE_CATALOG,TABLE_SCHEMA,TABLE_NAME,ENGINE_ATTRIBUTE,SECONDARY_ENGINE_ATTRIBUTE "\
"FROM INFORMATION_SCHEMA.TABLES_EXTENSIONS "\
"WHERE TABLE_SCHEMA = '${DATABASE}' ORDER BY TABLE_NAME;")
expected_tables_rows=$(
    printf 'def\t%s\tchild\tNULL\tNULL\n' "$DATABASE"
    printf 'def\t%s\tparent\tNULL\tNULL\n' "$DATABASE"
    printf 'def\t%s\tv_parent\tNULL\tNULL' "$DATABASE"
)
expect_value "tables extensions user rows" "$expected_tables_rows" "$tables_rows"

columns_rows=$(run_mysql \
    "SELECT TABLE_CATALOG,TABLE_SCHEMA,TABLE_NAME,COLUMN_NAME,ENGINE_ATTRIBUTE, "\
"SECONDARY_ENGINE_ATTRIBUTE FROM INFORMATION_SCHEMA.COLUMNS_EXTENSIONS "\
"WHERE TABLE_SCHEMA = '${DATABASE}' ORDER BY TABLE_NAME, COLUMN_NAME;")
expected_columns_rows=$(
    printf 'def\t%s\tchild\tid\tNULL\tNULL\n' "$DATABASE"
    printf 'def\t%s\tchild\tparent_id\tNULL\tNULL\n' "$DATABASE"
    printf 'def\t%s\tparent\tid\tNULL\tNULL\n' "$DATABASE"
    printf 'def\t%s\tparent\tv\tNULL\tNULL\n' "$DATABASE"
    printf 'def\t%s\tv_parent\tid\tNULL\tNULL\n' "$DATABASE"
    printf 'def\t%s\tv_parent\tv\tNULL\tNULL' "$DATABASE"
)
expect_value "columns extensions user rows" "$expected_columns_rows" "$columns_rows"

constraint_rows=$(run_mysql \
    "SELECT CONSTRAINT_CATALOG,CONSTRAINT_SCHEMA,CONSTRAINT_NAME,TABLE_NAME, "\
"ENGINE_ATTRIBUTE,SECONDARY_ENGINE_ATTRIBUTE "\
"FROM INFORMATION_SCHEMA.TABLE_CONSTRAINTS_EXTENSIONS "\
"WHERE CONSTRAINT_SCHEMA = '${DATABASE}' ORDER BY TABLE_NAME, CONSTRAINT_NAME;")
expected_constraint_rows=$(
    printf 'def\t%s\tfk_child_parent\tchild\tNULL\tNULL\n' "$DATABASE"
    printf 'def\t%s\tPRIMARY\tchild\tNULL\tNULL\n' "$DATABASE"
    printf 'def\t%s\tPRIMARY\tparent\tNULL\tNULL\n' "$DATABASE"
    printf 'def\t%s\tuq_parent_v\tparent\tNULL\tNULL' "$DATABASE"
)
expect_value "table constraints extensions user rows" \
    "$expected_constraint_rows" \
    "$constraint_rows"

check_count=$(run_mysql \
    "SELECT COUNT(*) FROM INFORMATION_SCHEMA.TABLE_CONSTRAINTS_EXTENSIONS "\
"WHERE CONSTRAINT_SCHEMA = '${DATABASE}' AND CONSTRAINT_NAME = 'chk_parent_v';")
expect_value "table constraints extensions omit checks" "0" "$check_count"

status=$(run_mysql \
    "SELECT * FROM INFORMATION_SCHEMA.TABLES_EXTENSIONS "\
"WHERE TABLE_SCHEMA = '${DATABASE}' AND TABLE_NAME = 'parent'; "\
"SELECT @@warning_count, ROW_COUNT();" | tail -n 1)
expect_value "tables extensions status" "0	-1" "$status"

system_table_rows=$(run_mysql \
    "SELECT TABLE_NAME,TABLE_TYPE,ENGINE,VERSION,ROW_FORMAT,TABLE_ROWS,DATA_LENGTH "\
"FROM INFORMATION_SCHEMA.TABLES "\
"WHERE TABLE_SCHEMA = 'information_schema' "\
"AND TABLE_NAME IN ('COLUMNS_EXTENSIONS','TABLES_EXTENSIONS', "\
"'TABLE_CONSTRAINTS_EXTENSIONS') ORDER BY TABLE_NAME;")
expected_system_table_rows="COLUMNS_EXTENSIONS	SYSTEM VIEW	NULL	10	NULL	0	0
TABLES_EXTENSIONS	SYSTEM VIEW	NULL	10	NULL	0	0
TABLE_CONSTRAINTS_EXTENSIONS	SYSTEM VIEW	NULL	10	NULL	0	0"
expect_value "extension attribute system table rows" \
    "$expected_system_table_rows" \
    "$system_table_rows"

columns_metadata=$(run_mysql \
    "SELECT TABLE_NAME,COLUMN_NAME,ORDINAL_POSITION,COLUMN_DEFAULT,IS_NULLABLE,DATA_TYPE, "\
"CHARACTER_MAXIMUM_LENGTH,CHARACTER_OCTET_LENGTH,NUMERIC_PRECISION,NUMERIC_SCALE, "\
"DATETIME_PRECISION,CHARACTER_SET_NAME,COLLATION_NAME,COLUMN_TYPE,PRIVILEGES "\
"FROM INFORMATION_SCHEMA.COLUMNS "\
"WHERE TABLE_SCHEMA = 'information_schema' "\
"AND TABLE_NAME IN ('COLUMNS_EXTENSIONS','TABLES_EXTENSIONS', "\
"'TABLE_CONSTRAINTS_EXTENSIONS') ORDER BY TABLE_NAME, ORDINAL_POSITION;")
expected_columns_metadata="COLUMNS_EXTENSIONS	TABLE_CATALOG	1	NULL	NO	varchar	64	192	NULL	NULL	NULL	utf8mb3	utf8mb3_bin	varchar(64)	select
COLUMNS_EXTENSIONS	TABLE_SCHEMA	2	NULL	NO	varchar	64	192	NULL	NULL	NULL	utf8mb3	utf8mb3_bin	varchar(64)	select
COLUMNS_EXTENSIONS	TABLE_NAME	3	NULL	NO	varchar	64	192	NULL	NULL	NULL	utf8mb3	utf8mb3_bin	varchar(64)	select
COLUMNS_EXTENSIONS	COLUMN_NAME	4	NULL	YES	varchar	64	192	NULL	NULL	NULL	utf8mb3	utf8mb3_tolower_ci	varchar(64)	select
COLUMNS_EXTENSIONS	ENGINE_ATTRIBUTE	5	NULL	YES	json	NULL	NULL	NULL	NULL	NULL	NULL	NULL	json	select
COLUMNS_EXTENSIONS	SECONDARY_ENGINE_ATTRIBUTE	6	NULL	YES	json	NULL	NULL	NULL	NULL	NULL	NULL	NULL	json	select
TABLES_EXTENSIONS	TABLE_CATALOG	1	NULL	NO	varchar	64	192	NULL	NULL	NULL	utf8mb3	utf8mb3_bin	varchar(64)	select
TABLES_EXTENSIONS	TABLE_SCHEMA	2	NULL	NO	varchar	64	192	NULL	NULL	NULL	utf8mb3	utf8mb3_bin	varchar(64)	select
TABLES_EXTENSIONS	TABLE_NAME	3	NULL	NO	varchar	64	192	NULL	NULL	NULL	utf8mb3	utf8mb3_bin	varchar(64)	select
TABLES_EXTENSIONS	ENGINE_ATTRIBUTE	4	NULL	YES	json	NULL	NULL	NULL	NULL	NULL	NULL	NULL	json	select
TABLES_EXTENSIONS	SECONDARY_ENGINE_ATTRIBUTE	5	NULL	YES	json	NULL	NULL	NULL	NULL	NULL	NULL	NULL	json	select
TABLE_CONSTRAINTS_EXTENSIONS	CONSTRAINT_CATALOG	1	NULL	NO	varchar	64	192	NULL	NULL	NULL	utf8mb3	utf8mb3_bin	varchar(64)	select
TABLE_CONSTRAINTS_EXTENSIONS	CONSTRAINT_SCHEMA	2	NULL	NO	varchar	64	192	NULL	NULL	NULL	utf8mb3	utf8mb3_bin	varchar(64)	select
TABLE_CONSTRAINTS_EXTENSIONS	CONSTRAINT_NAME	3	NULL	NO	varchar	64	192	NULL	NULL	NULL	utf8mb3	utf8mb3_tolower_ci	varchar(64)	select
TABLE_CONSTRAINTS_EXTENSIONS	TABLE_NAME	4	NULL	NO	varchar	64	192	NULL	NULL	NULL	utf8mb3	utf8mb3_bin	varchar(64)	select
TABLE_CONSTRAINTS_EXTENSIONS	ENGINE_ATTRIBUTE	5	NULL	YES	json	NULL	NULL	NULL	NULL	NULL	NULL	NULL	json	select
TABLE_CONSTRAINTS_EXTENSIONS	SECONDARY_ENGINE_ATTRIBUTE	6	NULL	YES	json	NULL	NULL	NULL	NULL	NULL	NULL	NULL	json	select"
expect_value "extension attribute columns metadata" \
    "$expected_columns_metadata" \
    "$columns_metadata"

printf '%s\n' \
    "mysql_baseline_information_schema_extension_attribute_tables_expectations: ok"
