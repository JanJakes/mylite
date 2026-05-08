#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_table_if_exists_expectations_$$"
MISSING_DATABASE="${DATABASE}_missing"

fail() {
    printf '%s\n' "mysql_baseline_table_if_exists_expectations: $1" >&2
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
    status=$?
    set -e

    if [ "$status" -eq 0 ]; then
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
    status=$?
    set -e

    if [ "$status" -ne 0 ]; then
        fail "$label: expected upstream MySQL to accept, got [$output]"
    fi
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
    "create if not exists without selected schema" \
    1046 \
    3D000 \
    "No database selected" \
    "CREATE TABLE IF NOT EXISTS no_default_table (id INT);"

expect_error \
    "drop if exists without selected schema" \
    1046 \
    3D000 \
    "No database selected" \
    "DROP TABLE IF EXISTS no_default_table;"

expect_error \
    "qualified create if not exists with unknown schema" \
    1049 \
    42000 \
    "Unknown database '${MISSING_DATABASE}'" \
    "CREATE TABLE IF NOT EXISTS ${MISSING_DATABASE}.missing_schema (id INT);"

expect_output \
    "create missing table status" \
    "0	0" \
    "CREATE TABLE IF NOT EXISTS ${DATABASE}.created_table (id INT, amount BIGINT NOT NULL); SELECT ROW_COUNT(), @@warning_count;"

expected_columns=$(cat <<'EOF'
id	int	YES	1
amount	bigint	NO	2
EOF
)
expect_output \
    "created table columns" \
    "$expected_columns" \
    "SELECT COLUMN_NAME, COLUMN_TYPE, IS_NULLABLE, ORDINAL_POSITION FROM INFORMATION_SCHEMA.COLUMNS WHERE TABLE_SCHEMA = '${DATABASE}' AND TABLE_NAME = 'created_table' ORDER BY ORDINAL_POSITION;"

expect_output \
    "create existing table warning status" \
    "0	1" \
    "CREATE TABLE IF NOT EXISTS ${DATABASE}.created_table (id BIGINT NOT NULL, replacement INT); SELECT ROW_COUNT(), @@warning_count;"

expect_output \
    "create existing table warning row" \
    "Note	1050	Table 'created_table' already exists" \
    "CREATE TABLE IF NOT EXISTS ${DATABASE}.created_table (id BIGINT NOT NULL, replacement INT); SHOW WARNINGS;"

expect_output \
    "create existing leaves definition unchanged" \
    "$expected_columns" \
    "SELECT COLUMN_NAME, COLUMN_TYPE, IS_NULLABLE, ORDINAL_POSITION FROM INFORMATION_SCHEMA.COLUMNS WHERE TABLE_SCHEMA = '${DATABASE}' AND TABLE_NAME = 'created_table' ORDER BY ORDINAL_POSITION;"

expect_output \
    "drop existing table status" \
    "0	0" \
    "DROP TABLE IF EXISTS ${DATABASE}.created_table; SELECT ROW_COUNT(), @@warning_count;"

expect_output \
    "drop missing table warning status" \
    "0	1" \
    "DROP TABLE IF EXISTS ${DATABASE}.created_table; SELECT ROW_COUNT(), @@warning_count;"

expect_output \
    "drop missing table warning row" \
    "Note	1051	Unknown table '${DATABASE}.created_table'" \
    "DROP TABLE IF EXISTS ${DATABASE}.created_table; SHOW WARNINGS;"

expect_output \
    "drop missing explicit schema warning status" \
    "0	1" \
    "DROP TABLE IF EXISTS ${MISSING_DATABASE}.missing_table; SELECT ROW_COUNT(), @@warning_count;"

expect_output \
    "drop missing explicit schema warning row" \
    "Note	1051	Unknown table '${MISSING_DATABASE}.missing_table'" \
    "DROP TABLE IF EXISTS ${MISSING_DATABASE}.missing_table; SHOW WARNINGS;"

expect_upstream_accepts \
    "temporary table if not exists is outside MyLite slice" \
    "CREATE TEMPORARY TABLE IF NOT EXISTS ${DATABASE}.temporary_table (id INT); DROP TEMPORARY TABLE ${DATABASE}.temporary_table;"

expect_upstream_accepts \
    "create table like if not exists is outside MyLite slice" \
    "CREATE TABLE ${DATABASE}.like_source (id INT); CREATE TABLE IF NOT EXISTS ${DATABASE}.like_target LIKE ${DATABASE}.like_source;"

expect_upstream_accepts \
    "create table select if not exists is outside MyLite slice" \
    "CREATE TABLE IF NOT EXISTS ${DATABASE}.select_target AS SELECT 1 AS id;"

expect_upstream_accepts \
    "multi table drop if exists is outside MyLite slice" \
    "CREATE TABLE ${DATABASE}.drop_a (id INT); DROP TABLE IF EXISTS ${DATABASE}.drop_a, ${DATABASE}.drop_b;"

printf '%s\n' "baseline-table-if-exists MySQL 8.4.9 expectations verified"
