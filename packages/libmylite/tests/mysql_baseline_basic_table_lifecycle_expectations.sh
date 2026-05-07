#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_lifecycle_expectations_$$"
MISSING_DATABASE="${DATABASE}_missing"

fail() {
    printf '%s\n' "mysql_baseline_basic_table_lifecycle_expectations: $1" >&2
    exit 1
}

run_mysql() {
    sql=$1
    shift
    printf '%s\n' "$sql" \
        | docker exec -i "$MYSQL_CONTAINER" mysql -uroot --batch --raw --skip-column-names "$@"
}

run_mysql_with_headers() {
    sql=$1
    shift
    printf '%s\n' "$sql" \
        | docker exec -i "$MYSQL_CONTAINER" mysql -uroot --batch --raw "$@"
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

expect_output_with_headers() {
    label=$1
    expected=$2
    sql=$3
    shift 3

    output=$(run_mysql_with_headers "$sql" "$@")
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
    "create table without selected schema" \
    1046 \
    3D000 \
    "No database selected" \
    "CREATE TABLE no_default_table (id INT);"

expect_error \
    "qualified create with unknown schema" \
    1049 \
    42000 \
    "Unknown database '${MISSING_DATABASE}'" \
    "CREATE TABLE ${MISSING_DATABASE}.missing_schema (id INT);"

expect_error \
    "use unknown schema" \
    1049 \
    42000 \
    "Unknown database '${MISSING_DATABASE}'" \
    "USE ${MISSING_DATABASE};"

expect_output \
    "use selects schema" \
    "$DATABASE" \
    "USE ${DATABASE}; SELECT DATABASE();"

expect_output \
    "qualified create status" \
    "0	0" \
    "CREATE TABLE ${DATABASE}.qualified_table (id INT NOT NULL); SELECT ROW_COUNT(), @@warning_count;"

expect_output \
    "create table status" \
    "0	0" \
    "CREATE TABLE simple_lifecycle (id INT, amount BIGINT NOT NULL, flags INT UNSIGNED NULL); SELECT ROW_COUNT(), @@warning_count;" \
    "$DATABASE"

expected_columns=$(cat <<'EOF'
id	int	YES	1
amount	bigint	NO	2
flags	int unsigned	YES	3
EOF
)
expect_output \
    "column descriptors" \
    "$expected_columns" \
    "SELECT COLUMN_NAME, COLUMN_TYPE, IS_NULLABLE, ORDINAL_POSITION FROM INFORMATION_SCHEMA.COLUMNS WHERE TABLE_SCHEMA = '${DATABASE}' AND TABLE_NAME = 'simple_lifecycle' ORDER BY ORDINAL_POSITION;"

run_mysql "CREATE TABLE a_table (id INT); CREATE TABLE z_table (id INT);" "$DATABASE" >/dev/null

expected_tables=$(cat <<EOF
Tables_in_${DATABASE}
a_table
qualified_table
simple_lifecycle
z_table
EOF
)
expect_output_with_headers \
    "show tables" \
    "$expected_tables" \
    "SHOW TABLES;" \
    "$DATABASE"

expect_output_with_headers \
    "show tables from schema" \
    "$expected_tables" \
    "SHOW TABLES FROM ${DATABASE};"

expect_output_with_headers \
    "show tables in schema" \
    "$expected_tables" \
    "SHOW TABLES IN ${DATABASE};"

expect_error \
    "duplicate table" \
    1050 \
    42S01 \
    "Table 'qualified_table' already exists" \
    "CREATE TABLE qualified_table (id INT);" \
    "$DATABASE"

expect_error \
    "unknown table" \
    1051 \
    42S02 \
    "Unknown table '${DATABASE}.missing_table'" \
    "DROP TABLE missing_table;" \
    "$DATABASE"

expect_error \
    "duplicate column" \
    1060 \
    42S21 \
    "Duplicate column name 'id'" \
    "CREATE TABLE duplicate_column (id INT, id BIGINT);" \
    "$DATABASE"

expect_error \
    "duplicate case column" \
    1060 \
    42S21 \
    "Duplicate column name 'ID'" \
    "CREATE TABLE duplicate_case_column (id INT, ID BIGINT);" \
    "$DATABASE"

expect_output \
    "drop table status" \
    "0	0" \
    "DROP TABLE simple_lifecycle; SELECT ROW_COUNT(), @@warning_count;" \
    "$DATABASE"

expect_output \
    "drop table side effect" \
    "0" \
    "SELECT COUNT(*) FROM INFORMATION_SCHEMA.TABLES WHERE TABLE_SCHEMA = '${DATABASE}' AND TABLE_NAME = 'simple_lifecycle';"

expect_output \
    "drop if exists warning" \
    "Note	1051	Unknown table '${DATABASE}.missing_if_exists'" \
    "DROP TABLE IF EXISTS missing_if_exists; SHOW WARNINGS;" \
    "$DATABASE"

expect_output \
    "integer display width warning" \
    "Warning	1681	Integer display width is deprecated and will be removed in a future release." \
    "CREATE TABLE display_width (id INT(11)); SHOW WARNINGS;" \
    "$DATABASE"

printf '%s\n' "baseline-basic-table-lifecycle MySQL 8.4.9 expectations verified"
