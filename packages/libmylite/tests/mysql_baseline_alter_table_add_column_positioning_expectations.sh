#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_alter_add_column_positioning_expectations_$$"
MISSING_DATABASE="${DATABASE}_missing"

fail() {
    printf '%s\n' "mysql_baseline_alter_table_add_column_positioning_expectations: $1" >&2
    exit 1
}

run_mysql() {
    sql=$1
    shift
    printf '%s\n' "$sql" \
        | docker exec -i "$MYSQL_CONTAINER" mysql -uroot --batch --raw --skip-column-names "$@"
}

expect_output() {
    label=$1
    expected=$2
    sql=$3
    shift 3

    output=$(run_mysql "$sql" "$@")
    if [ "$output" != "$expected" ]; then
        fail "$label: expected [$expected], got [$output]"
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

cleanup() {
    run_mysql "DROP DATABASE IF EXISTS ${DATABASE};" >/dev/null 2>&1 || true
    run_mysql "DROP DATABASE IF EXISTS ${MISSING_DATABASE};" >/dev/null 2>&1 || true
}

trap cleanup EXIT

version=$(run_mysql 'SELECT VERSION();')
case "$version" in
    8.4.9*) ;;
    *) fail "expected MySQL 8.4.9 runtime, got [$version]" ;;
esac

cleanup
run_mysql "CREATE DATABASE ${DATABASE};" >/dev/null

expect_error \
    "positioned add column without selected schema" \
    1046 \
    3D000 \
    "No database selected" \
    "ALTER TABLE positioned ADD COLUMN first_col INT FIRST;"

expect_error \
    "positioned add column qualified unknown schema" \
    1049 \
    42000 \
    "Unknown database '${MISSING_DATABASE}'" \
    "ALTER TABLE ${MISSING_DATABASE}.positioned ADD COLUMN first_col INT FIRST;"

run_mysql \
    "CREATE TABLE ${DATABASE}.qualified_positioned (id INT NOT NULL, tail INT); "\
"INSERT INTO ${DATABASE}.qualified_positioned VALUES (1, 2); "\
"ALTER TABLE ${DATABASE}.qualified_positioned ADD COLUMN first_col INT NOT NULL FIRST;" \
    >/dev/null
expect_output \
    "schema-qualified positioned add without selected schema" \
    "0	1	2" \
    "SELECT * FROM ${DATABASE}.qualified_positioned;"

run_mysql \
    "CREATE TABLE positioned (id INT NOT NULL, tail INT NULL); "\
"INSERT INTO positioned VALUES (1, 10), (2, NULL);" \
    "$DATABASE" >/dev/null

expect_output \
    "first positioning row values and diagnostics" \
    "0	0	0:1:10,0:2:N" \
    "ALTER TABLE positioned ADD COLUMN first_col INT NOT NULL FIRST; "\
"SELECT ROW_COUNT(), @@warning_count, "\
"GROUP_CONCAT(CONCAT(first_col, ':', id, ':', IFNULL(tail, 'N')) ORDER BY id) "\
"FROM positioned;" \
    "$DATABASE"

expect_output \
    "after positioning row values and diagnostics" \
    "0	0	0:1:N:10,0:2:N:N" \
    "ALTER TABLE positioned ADD COLUMN after_id INT AFTER id; "\
"SELECT ROW_COUNT(), @@warning_count, "\
"GROUP_CONCAT(CONCAT(first_col, ':', id, ':', IFNULL(after_id, 'N'), ':', IFNULL(tail, 'N')) "\
"ORDER BY id) FROM positioned;" \
    "$DATABASE"

expect_output \
    "positioned show columns" \
    "first_col	int	NO		NULL	
id	int	NO		NULL	
after_id	int	YES		NULL	
tail	int	YES		NULL	" \
    "SHOW COLUMNS FROM positioned;" \
    "$DATABASE"

expect_output \
    "positioned information schema ordinals" \
    "first_col	1
id	2
after_id	3
tail	4" \
    "SELECT COLUMN_NAME, ORDINAL_POSITION FROM INFORMATION_SCHEMA.COLUMNS "\
"WHERE TABLE_SCHEMA = '${DATABASE}' AND TABLE_NAME = 'positioned' "\
"ORDER BY ORDINAL_POSITION;" \
    "$DATABASE"

expect_output \
    "implicit row-value insert follows positioned order" \
    "0:1:5:10,0:2:N:N,8:3:9:30" \
    "UPDATE positioned SET after_id = 5 WHERE id = 1; "\
"INSERT INTO positioned VALUES (8, 3, 9, 30); "\
"SELECT GROUP_CONCAT(CONCAT(first_col, ':', id, ':', IFNULL(after_id, 'N'), ':', "\
"IFNULL(tail, 'N')) ORDER BY id) FROM positioned;" \
    "$DATABASE"

run_mysql \
    "CREATE TABLE after_last (id INT NOT NULL, tail INT); INSERT INTO after_last VALUES (1, 2);" \
    "$DATABASE" >/dev/null
expect_output \
    "after last positioning appends" \
    "1	2	NULL" \
    "ALTER TABLE after_last ADD COLUMN added INT AFTER tail; SELECT * FROM after_last;" \
    "$DATABASE"

expect_error \
    "positioned add column unknown after column" \
    1054 \
    42S22 \
    "Unknown column 'missing' in 'positioned'" \
    "ALTER TABLE positioned ADD COLUMN bad INT AFTER missing;" \
    "$DATABASE"

expect_error \
    "positioned add column after self" \
    1054 \
    42S22 \
    "Unknown column 'self_ref' in 'positioned'" \
    "ALTER TABLE positioned ADD COLUMN self_ref INT AFTER self_ref;" \
    "$DATABASE"

expect_error \
    "positioned add column duplicate before position resolution" \
    1060 \
    42S21 \
    "Duplicate column name 'id'" \
    "ALTER TABLE positioned ADD COLUMN id INT FIRST;" \
    "$DATABASE"

expect_error \
    "positioned add column table-qualified after column" \
    1064 \
    42000 \
    ".id" \
    "ALTER TABLE positioned ADD COLUMN qualified_after INT AFTER positioned.id;" \
    "$DATABASE"
