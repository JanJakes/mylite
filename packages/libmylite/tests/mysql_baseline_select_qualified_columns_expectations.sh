#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_select_qualified_columns_expectations_$$"

fail() {
    printf '%s\n' "mysql_baseline_select_qualified_columns_expectations: $1" >&2
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
}

trap cleanup EXIT

version=$(run_mysql 'SELECT VERSION();')
case "$version" in
    8.4.9*) ;;
    *) fail "expected MySQL 8.4.9 runtime, got [$version]" ;;
esac

cleanup
run_mysql "CREATE DATABASE ${DATABASE};" >/dev/null
run_mysql \
    "CREATE TABLE numbers (id INT NOT NULL, n INT NULL, nn INT NOT NULL); "\
"INSERT INTO numbers VALUES (1, 10, 5), (2, NULL, 6), (3, 10, 7);" \
    "$DATABASE" >/dev/null

expect_output_with_headers \
    "table-qualified selected column label" \
    "n
10
NULL
10" \
    "SELECT numbers.n FROM numbers ORDER BY numbers.id;" \
    "$DATABASE"

expect_output_with_headers \
    "schema-qualified selected column label" \
    "n
10
NULL
10" \
    "SELECT ${DATABASE}.numbers.n FROM ${DATABASE}.numbers ORDER BY ${DATABASE}.numbers.id;"

expect_output \
    "schema qualifier against unqualified source" \
    "10
NULL
10
0	-1" \
    "DO 0; SELECT ${DATABASE}.numbers.n FROM numbers ORDER BY ${DATABASE}.numbers.id; "\
"SELECT @@warning_count, ROW_COUNT();" \
    "$DATABASE"

expect_output \
    "alias-qualified where order limit" \
    "10
0	-1" \
    "DO 0; SELECT nums.n FROM numbers AS nums WHERE nums.n IS NOT NULL "\
"ORDER BY nums.id DESC LIMIT 1; SELECT @@warning_count, ROW_COUNT();" \
    "$DATABASE"

expect_output \
    "alias-qualified distinct" \
    "NULL
10
0	-1" \
    "DO 0; SELECT DISTINCT nums.n FROM numbers AS nums ORDER BY nums.n; "\
"SELECT @@warning_count, ROW_COUNT();" \
    "$DATABASE"

expect_output \
    "alias-qualified distinctrow" \
    "NULL
10
0	-1" \
    "DO 0; SELECT DISTINCTROW nums.n FROM numbers nums ORDER BY nums.n; "\
"SELECT @@warning_count, ROW_COUNT();" \
    "$DATABASE"

expect_output_with_headers \
    "alias-qualified aggregates" \
    "COUNT(nums.n)	COUNT(DISTINCT nums.n)	MIN(nums.n)	MAX(nums.n)
2	1	10	10
@@warning_count	ROW_COUNT()
0	-1" \
    "DO 0; SELECT COUNT(nums.n), COUNT(DISTINCT nums.n), MIN(nums.n), MAX(nums.n) "\
"FROM numbers AS nums; SELECT @@warning_count, ROW_COUNT();" \
    "$DATABASE"

expect_error \
    "original qualifier hidden after alias field" \
    1054 \
    42S22 \
    "Unknown column 'numbers.n' in 'field list'" \
    "SELECT numbers.n FROM numbers AS nums;" \
    "$DATABASE"

expect_error \
    "schema qualifier hidden after alias field" \
    1054 \
    42S22 \
    "Unknown column '${DATABASE}.numbers.n' in 'field list'" \
    "SELECT ${DATABASE}.numbers.n FROM numbers AS nums;" \
    "$DATABASE"

expect_error \
    "wrong qualifier field" \
    1054 \
    42S22 \
    "Unknown column 'wrong.n' in 'field list'" \
    "SELECT wrong.n FROM numbers AS nums;" \
    "$DATABASE"

expect_error \
    "wrong qualifier where" \
    1054 \
    42S22 \
    "Unknown column 'wrong.n' in 'where clause'" \
    "SELECT nums.n FROM numbers AS nums WHERE wrong.n = 10;" \
    "$DATABASE"

expect_error \
    "wrong qualifier order" \
    1054 \
    42S22 \
    "Unknown column 'wrong.n' in 'order clause'" \
    "SELECT nums.n FROM numbers AS nums ORDER BY wrong.n;" \
    "$DATABASE"

expect_error \
    "qualified missing column" \
    1054 \
    42S22 \
    "Unknown column 'nums.missing' in 'field list'" \
    "SELECT nums.missing FROM numbers AS nums;" \
    "$DATABASE"

expect_output_with_headers \
    "qualified wildcard upstream" \
    "id	n	nn
1	10	5
2	NULL	6
3	10	7" \
    "SELECT nums.* FROM numbers AS nums ORDER BY nums.id;" \
    "$DATABASE"

expect_error \
    "original qualified wildcard after alias" \
    1051 \
    42S02 \
    "Unknown table 'numbers'" \
    "SELECT numbers.* FROM numbers AS nums;" \
    "$DATABASE"

printf '%s\n' "baseline-select-qualified-columns MySQL 8.4.9 expectations verified"
