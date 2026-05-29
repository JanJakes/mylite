#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_innodb_virtual_expectations_$$"

fail() {
    printf '%s\n' "mysql_baseline_information_schema_innodb_virtual_expectations: $1" >&2
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
run_mysql \
    "CREATE DATABASE ${DATABASE}; USE ${DATABASE}; "\
"CREATE TABLE generated_values ("\
"id INT PRIMARY KEY, "\
"a INT, "\
"b INT, "\
"c INT GENERATED ALWAYS AS (a + b) VIRTUAL, "\
"d INT GENERATED ALWAYS AS (a * 2) STORED, "\
"e INT GENERATED ALWAYS AS (5) VIRTUAL, "\
"f INT GENERATED ALWAYS AS (a) VIRTUAL, "\
"g INT GENERATED ALWAYS AS (NULL) VIRTUAL, "\
"h INT GENERATED ALWAYS AS (-a + b) VIRTUAL"\
") ENGINE=InnoDB; "\
"CREATE TABLE generated_order ("\
"id INT PRIMARY KEY, "\
"a INT, "\
"b INT, "\
"c INT GENERATED ALWAYS AS (b + a + b) VIRTUAL, "\
"d INT GENERATED ALWAYS AS (a + a) VIRTUAL, "\
"e INT GENERATED ALWAYS AS ((id + b) * (a - id)) VIRTUAL, "\
"f INT GENERATED ALWAYS AS (5) VIRTUAL, "\
"g INT GENERATED ALWAYS AS (NULL) VIRTUAL"\
") ENGINE=InnoDB; "\
"CREATE TABLE first_virtual ("\
"id INT, a INT, b INT GENERATED ALWAYS AS (a + id) VIRTUAL"\
") ENGINE=InnoDB; "\
"CREATE TABLE second_virtual ("\
"id INT, a INT, b INT GENERATED ALWAYS AS (a + id) VIRTUAL, "\
"c INT GENERATED ALWAYS AS (a) VIRTUAL"\
") ENGINE=InnoDB;" >/dev/null

table_kind=$(run_mysql \
    "SHOW FULL TABLES FROM INFORMATION_SCHEMA WHERE Tables_in_INFORMATION_SCHEMA = "\
"'INNODB_VIRTUAL';")
expect_value "innodb virtual table kind" \
    "INNODB_VIRTUAL	SYSTEM VIEW" \
    "$table_kind"

system_table_row=$(run_mysql \
    "SELECT TABLE_NAME,TABLE_TYPE,ENGINE,VERSION,ROW_FORMAT,TABLE_ROWS,DATA_LENGTH,AUTO_INCREMENT "\
"FROM INFORMATION_SCHEMA.TABLES WHERE TABLE_SCHEMA = 'information_schema' "\
"AND TABLE_NAME = 'INNODB_VIRTUAL';")
expect_value "innodb virtual system table row" \
    "INNODB_VIRTUAL	SYSTEM VIEW	NULL	10	NULL	0	0	NULL" \
    "$system_table_row"

expected_columns_metadata="INNODB_VIRTUAL	TABLE_ID	1		NO	bigint	NULL	NULL	NULL	NULL	NULL	NULL	NULL	bigint unsigned	select
INNODB_VIRTUAL	POS	2		NO	int	NULL	NULL	NULL	NULL	NULL	NULL	NULL	int unsigned	select
INNODB_VIRTUAL	BASE_POS	3		NO	int	NULL	NULL	NULL	NULL	NULL	NULL	NULL	int unsigned	select"
columns_metadata=$(run_mysql \
    "SELECT TABLE_NAME,COLUMN_NAME,ORDINAL_POSITION,COLUMN_DEFAULT,IS_NULLABLE,DATA_TYPE, "\
"CHARACTER_MAXIMUM_LENGTH,CHARACTER_OCTET_LENGTH,NUMERIC_PRECISION,NUMERIC_SCALE, "\
"DATETIME_PRECISION,CHARACTER_SET_NAME,COLLATION_NAME,COLUMN_TYPE,PRIVILEGES "\
"FROM INFORMATION_SCHEMA.COLUMNS "\
"WHERE TABLE_SCHEMA = 'information_schema' AND TABLE_NAME = 'INNODB_VIRTUAL' "\
"ORDER BY ORDINAL_POSITION;")
expect_value "innodb virtual columns metadata" "$expected_columns_metadata" "$columns_metadata"

expected_generated_values="generated_values	65539	1
generated_values	65539	2
generated_values	196614	1
generated_values	327688	1
generated_values	327688	2"
generated_values=$(run_mysql \
    "SELECT SUBSTRING_INDEX(t.NAME,'/',-1), v.POS, v.BASE_POS "\
"FROM INFORMATION_SCHEMA.INNODB_VIRTUAL AS v "\
"JOIN INFORMATION_SCHEMA.INNODB_TABLES AS t ON t.TABLE_ID = v.TABLE_ID "\
"WHERE t.NAME = '${DATABASE}/generated_values' ORDER BY v.POS, v.BASE_POS;")
expect_value "innodb virtual generated_values rows" \
    "$expected_generated_values" \
    "$generated_values"

expected_generated_order="generated_order	65539	1
generated_order	65539	2
generated_order	131076	1
generated_order	196613	0
generated_order	196613	1
generated_order	196613	2"
generated_order=$(run_mysql \
    "SELECT SUBSTRING_INDEX(t.NAME,'/',-1), v.POS, v.BASE_POS "\
"FROM INFORMATION_SCHEMA.INNODB_VIRTUAL AS v "\
"JOIN INFORMATION_SCHEMA.INNODB_TABLES AS t ON t.TABLE_ID = v.TABLE_ID "\
"WHERE t.NAME = '${DATABASE}/generated_order' ORDER BY v.POS, v.BASE_POS;")
expect_value "innodb virtual duplicate and ordering rows" \
    "$expected_generated_order" \
    "$generated_order"

expected_sequence_rows="first_virtual	65538	0
first_virtual	65538	1
second_virtual	65538	0
second_virtual	65538	1
second_virtual	131075	1"
sequence_rows=$(run_mysql \
    "SELECT SUBSTRING_INDEX(t.NAME,'/',-1), v.POS, v.BASE_POS "\
"FROM INFORMATION_SCHEMA.INNODB_VIRTUAL AS v "\
"JOIN INFORMATION_SCHEMA.INNODB_TABLES AS t ON t.TABLE_ID = v.TABLE_ID "\
"WHERE t.NAME IN ('${DATABASE}/first_virtual','${DATABASE}/second_virtual') "\
"ORDER BY t.NAME, v.POS, v.BASE_POS;")
expect_value "innodb virtual per-table ordinal rows" "$expected_sequence_rows" "$sequence_rows"

case_count=$(run_mysql \
    "SELECT COUNT(*) FROM INFORMATION_SCHEMA.innodb_virtual AS v "\
"JOIN INFORMATION_SCHEMA.INNODB_TABLES AS t ON t.TABLE_ID = v.TABLE_ID "\
"WHERE t.NAME LIKE '${DATABASE}/%';")
expect_value "case-insensitive innodb virtual count" "16" "$case_count"

use_count=$(run_mysql \
    "USE information_schema; SELECT COUNT(*) FROM INNODB_VIRTUAL AS v "\
"JOIN INNODB_TABLES AS t ON t.TABLE_ID = v.TABLE_ID "\
"WHERE t.NAME = '${DATABASE}/generated_values' AND v.BASE_POS = 1;")
expect_value "unqualified innodb virtual base position count" "3" "$use_count"

status=$(run_mysql \
    "SELECT COUNT(*) FROM INFORMATION_SCHEMA.INNODB_VIRTUAL; "\
"SELECT @@warning_count, ROW_COUNT();" | tail -n 1)
expect_value "innodb virtual status" "0	-1" "$status"

printf '%s\n' "mysql_baseline_information_schema_innodb_virtual_expectations: ok"
