#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_truncate_expectations_$$"
OTHER_DATABASE="${DATABASE}_other"
MISSING_DATABASE="${DATABASE}_missing"

fail() {
    printf '%s\n' "mysql_baseline_truncate_table_lifecycle_expectations: $1" >&2
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

reset_table() {
    run_mysql \
        "DROP TABLE IF EXISTS numbers; "\
"CREATE TABLE numbers (id INT NOT NULL, n INT NULL); "\
"INSERT INTO numbers VALUES (1, NULL), (2, 5), (3, 7);" \
        "$DATABASE" >/dev/null
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
    "truncate without selected schema" \
    1046 \
    3D000 \
    "No database selected" \
    "TRUNCATE TABLE numbers;"

expect_error \
    "truncate qualified unknown schema" \
    1146 \
    42S02 \
    "Table '${MISSING_DATABASE}.numbers' doesn't exist" \
    "TRUNCATE TABLE ${MISSING_DATABASE}.numbers;"

expect_error \
    "truncate unknown table" \
    1146 \
    42S02 \
    "Table '${DATABASE}.missing_numbers' doesn't exist" \
    "TRUNCATE TABLE missing_numbers;" \
    "$DATABASE"

run_mysql \
    "CREATE TABLE ${DATABASE}.qualified_numbers (id INT NOT NULL); "\
"INSERT INTO ${DATABASE}.qualified_numbers VALUES (1), (2);" >/dev/null
expect_output \
    "schema-qualified truncate without selected schema" \
    "0	0	0" \
    "TRUNCATE ${DATABASE}.qualified_numbers; "\
"SELECT ROW_COUNT(), @@warning_count, COUNT(*) FROM ${DATABASE}.qualified_numbers;"

reset_table
expect_output \
    "truncate table selected schema" \
    "0	0	0	2" \
    "TRUNCATE TABLE numbers; "\
"SELECT ROW_COUNT(), @@warning_count, COUNT(*), "\
"(SELECT COUNT(*) FROM INFORMATION_SCHEMA.COLUMNS "\
"WHERE TABLE_SCHEMA = '${DATABASE}' AND TABLE_NAME = 'numbers') FROM numbers;" \
    "$DATABASE"

reset_table
expect_output \
    "truncate optional table keyword" \
    "0	0	0" \
    "TRUNCATE numbers; SELECT ROW_COUNT(), @@warning_count, COUNT(*) FROM numbers;" \
    "$DATABASE"

expect_output \
    "truncate empty table" \
    "0	0	0" \
    "TRUNCATE TABLE numbers; SELECT ROW_COUNT(), @@warning_count, COUNT(*) FROM numbers;" \
    "$DATABASE"

run_mysql \
    "CREATE TABLE rename_source (id INT NOT NULL); "\
"INSERT INTO rename_source VALUES (1); "\
"RENAME TABLE rename_source TO rename_target;" \
    "$DATABASE" >/dev/null
expect_output \
    "truncate after rename" \
    "0	0	0" \
    "TRUNCATE TABLE rename_target; "\
"SELECT ROW_COUNT(), @@warning_count, COUNT(*) FROM rename_target;" \
    "$DATABASE"

expect_error \
    "if exists syntax" \
    1064 \
    42000 \
    "near 'IF EXISTS numbers'" \
    "TRUNCATE TABLE IF EXISTS numbers;" \
    "$DATABASE"

expect_error \
    "multiple table syntax" \
    1064 \
    42000 \
    "near ', other_numbers'" \
    "TRUNCATE TABLE numbers, other_numbers;" \
    "$DATABASE"

expect_error \
    "where syntax" \
    1064 \
    42000 \
    "near 'WHERE id = 1'" \
    "TRUNCATE TABLE numbers WHERE id = 1;" \
    "$DATABASE"

expect_error \
    "order by syntax" \
    1064 \
    42000 \
    "near 'ORDER BY id'" \
    "TRUNCATE TABLE numbers ORDER BY id;" \
    "$DATABASE"

expect_error \
    "limit syntax" \
    1064 \
    42000 \
    "near 'LIMIT 1'" \
    "TRUNCATE TABLE numbers LIMIT 1;" \
    "$DATABASE"

expect_error \
    "temporary syntax" \
    1064 \
    42000 \
    "near 'TABLE numbers'" \
    "TRUNCATE TEMPORARY TABLE numbers;" \
    "$DATABASE"

expect_error \
    "alias syntax" \
    1064 \
    42000 \
    "near 'AS n'" \
    "TRUNCATE TABLE numbers AS n;" \
    "$DATABASE"

expect_error \
    "partition syntax" \
    1064 \
    42000 \
    "near 'PARTITION (p0)'" \
    "TRUNCATE TABLE numbers PARTITION (p0);" \
    "$DATABASE"
