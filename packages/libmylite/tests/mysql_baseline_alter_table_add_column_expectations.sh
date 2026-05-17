#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_alter_add_column_expectations_$$"
OTHER_DATABASE="${DATABASE}_other"
MISSING_DATABASE="${DATABASE}_missing"

fail() {
    printf '%s\n' "mysql_baseline_alter_table_add_column_expectations: $1" >&2
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

expect_upstream_accepts() {
    label=$1
    sql=$2
    shift 2

    set +e
    output=$(run_mysql "$sql" "$@" 2>&1)
    status_code=$?
    set -e

    if [ "$status_code" -ne 0 ]; then
        fail "$label: expected MySQL to accept deferred syntax, got [$output]"
    fi
}

cleanup() {
    run_mysql "DROP DATABASE IF EXISTS ${DATABASE};" >/dev/null 2>&1 || true
    run_mysql "DROP DATABASE IF EXISTS ${OTHER_DATABASE};" >/dev/null 2>&1 || true
    run_mysql "DROP DATABASE IF EXISTS ${MISSING_DATABASE};" >/dev/null 2>&1 || true
}

trap cleanup EXIT

version=$(run_mysql 'SELECT VERSION();')
case "$version" in
    8.4.9*) ;;
    *) fail "expected MySQL 8.4.9 runtime, got [$version]" ;;
esac

cleanup
run_mysql "CREATE DATABASE ${DATABASE}; CREATE DATABASE ${OTHER_DATABASE};" >/dev/null

expect_error \
    "add column without selected schema" \
    1046 \
    3D000 \
    "No database selected" \
    "ALTER TABLE numbers ADD COLUMN n INT;"

expect_error \
    "add column qualified unknown schema" \
    1049 \
    42000 \
    "Unknown database '${MISSING_DATABASE}'" \
    "ALTER TABLE ${MISSING_DATABASE}.numbers ADD COLUMN n INT;"

expect_error \
    "add column unknown table" \
    1146 \
    42S02 \
    "Table '${DATABASE}.missing_numbers' doesn't exist" \
    "ALTER TABLE missing_numbers ADD COLUMN n INT;" \
    "$DATABASE"

run_mysql \
    "CREATE TABLE ${DATABASE}.qualified_numbers (id INT NOT NULL); "\
"INSERT INTO ${DATABASE}.qualified_numbers VALUES (1); "\
"ALTER TABLE ${DATABASE}.qualified_numbers ADD COLUMN n INT; "\
"SELECT ROW_COUNT(), @@warning_count, GROUP_CONCAT(CONCAT(id, ':', IFNULL(n, 'N')) ORDER BY id) "\
"FROM ${DATABASE}.qualified_numbers;" \
    >/dev/null
expect_output \
    "schema-qualified add column without selected schema" \
    "0	0	1:0" \
    "ALTER TABLE ${DATABASE}.qualified_numbers ADD COLUMN n2 INT NOT NULL; "\
"SELECT ROW_COUNT(), @@warning_count, GROUP_CONCAT(CONCAT(id, ':', n2) ORDER BY id) "\
"FROM ${DATABASE}.qualified_numbers;"

run_mysql \
    "CREATE TABLE numbers (id INT NOT NULL); INSERT INTO numbers VALUES (1), (2);" \
    "$DATABASE" >/dev/null
expect_output \
    "nullable add column backfills null" \
    "0	0	1:N,2:N" \
    "ALTER TABLE numbers ADD COLUMN n INT NULL; "\
"SELECT ROW_COUNT(), @@warning_count, GROUP_CONCAT(CONCAT(id, ':', IFNULL(n, 'N')) ORDER BY id) "\
"FROM numbers;" \
    "$DATABASE"
expect_output \
    "later insert omits nullable added column" \
    "1	0	1:N,2:N,3:N" \
    "INSERT INTO numbers (id) VALUES (3); "\
"SELECT ROW_COUNT(), @@warning_count, GROUP_CONCAT(CONCAT(id, ':', IFNULL(n, 'N')) ORDER BY id) "\
"FROM numbers;" \
    "$DATABASE"

run_mysql \
    "DROP TABLE numbers; CREATE TABLE numbers (id INT NOT NULL); "\
"INSERT INTO numbers VALUES (1), (2);" \
    "$DATABASE" >/dev/null
expect_output \
    "not null add column backfills zero" \
    "0	0	1:0,2:0" \
    "ALTER TABLE numbers ADD COLUMN nn INT NOT NULL; "\
"SELECT ROW_COUNT(), @@warning_count, GROUP_CONCAT(CONCAT(id, ':', nn) ORDER BY id) "\
"FROM numbers;" \
    "$DATABASE"
expect_error \
    "later insert omits not null added column" \
    1364 \
    HY000 \
    "Field 'nn' doesn't have a default value" \
    "INSERT INTO numbers (id) VALUES (3);" \
    "$DATABASE"
expect_error \
    "insert values must include added column" \
    1136 \
    21S01 \
    "Column count doesn't match value count at row 1" \
    "INSERT INTO numbers VALUES (3);" \
    "$DATABASE"
expect_output \
    "later insert supplies not null added column" \
    "1	0	1:0,2:0,3:5" \
    "INSERT INTO numbers (id, nn) VALUES (3, 5); "\
"SELECT ROW_COUNT(), @@warning_count, GROUP_CONCAT(CONCAT(id, ':', nn) ORDER BY id) "\
"FROM numbers;" \
    "$DATABASE"

run_mysql "DROP TABLE IF EXISTS types; CREATE TABLE types (id INT);" "$DATABASE" >/dev/null
expect_output \
    "integer type forms" \
    "0	0	N:0:N:0" \
    "INSERT INTO types VALUES (1); "\
"ALTER TABLE types ADD COLUMN i INTEGER UNSIGNED; "\
"ALTER TABLE types ADD COLUMN b BIGINT UNSIGNED NOT NULL; "\
"ALTER TABLE types ADD c BIGINT NULL; "\
"ALTER TABLE types ADD d INT UNSIGNED NOT NULL; "\
"SELECT ROW_COUNT(), @@warning_count, GROUP_CONCAT(CONCAT(IFNULL(i, 'N'), ':', b, ':', IFNULL(c, 'N'), ':', d) ORDER BY id) "\
"FROM types;" \
    "$DATABASE"

expect_error \
    "duplicate column" \
    1060 \
    42S21 \
    "Duplicate column name 'id'" \
    "ALTER TABLE types ADD COLUMN id INT;" \
    "$DATABASE"

expect_output \
    "not null add column on empty table" \
    "0	0	0" \
    "CREATE TABLE empty_numbers (id INT NOT NULL); "\
"ALTER TABLE empty_numbers ADD COLUMN nn INT NOT NULL; "\
"SELECT ROW_COUNT(), @@warning_count, COUNT(*) FROM empty_numbers;" \
    "$DATABASE"

run_mysql \
    "CREATE TABLE rename_source (id INT NOT NULL); INSERT INTO rename_source VALUES (1); "\
"RENAME TABLE rename_source TO rename_target; ALTER TABLE rename_target ADD COLUMN n INT;" \
    "$DATABASE" >/dev/null
expect_output \
    "add column after rename" \
    "1:N" \
    "SELECT GROUP_CONCAT(CONCAT(id, ':', IFNULL(n, 'N')) ORDER BY id) FROM rename_target;" \
    "$DATABASE"

expect_error \
    "qualified column syntax" \
    1064 \
    42000 \
    ".n INT" \
    "ALTER TABLE types ADD COLUMN types.n INT;" \
    "$DATABASE"

expect_upstream_accepts \
    "default syntax accepted upstream outside mylite slice" \
    "ALTER TABLE types ADD COLUMN defaulted INT DEFAULT 5;" \
    "$DATABASE"
expect_upstream_accepts \
    "parenthesized add list accepted upstream outside mylite slice" \
    "ALTER TABLE types ADD (parenthesized_col INT);" \
    "$DATABASE"
expect_upstream_accepts \
    "multiple add actions accepted upstream outside mylite slice" \
    "ALTER TABLE types ADD COLUMN multi_a INT, ADD COLUMN multi_b INT;" \
    "$DATABASE"
expect_upstream_accepts \
    "string type accepted upstream outside mylite slice" \
    "ALTER TABLE types ADD COLUMN string_col VARCHAR(10);" \
    "$DATABASE"
