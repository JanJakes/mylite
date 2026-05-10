#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_select_table_alias_expectations_$$"
MISSING_DATABASE="${DATABASE}_missing"

fail() {
    printf '%s\n' "mysql_baseline_select_table_alias_expectations: $1" >&2
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
run_mysql \
    "CREATE TABLE numbers (id INT NOT NULL, n INT NULL, nn INT NOT NULL); "\
"INSERT INTO numbers VALUES (1, 10, 5), (2, NULL, 6), (3, 10, 7);" \
    "$DATABASE" >/dev/null

expect_error \
    "aliased select without selected schema" \
    1046 \
    3D000 \
    "No database selected" \
    "SELECT n FROM numbers AS nums;"

expect_error \
    "qualified aliased select unknown schema" \
    1049 \
    42000 \
    "Unknown database '${MISSING_DATABASE}'" \
    "SELECT n FROM ${MISSING_DATABASE}.numbers AS nums;"

expect_error \
    "aliased select unknown table" \
    1146 \
    42S02 \
    "Table '${DATABASE}.missing' doesn't exist" \
    "SELECT n FROM missing AS nums;" \
    "$DATABASE"

expect_output \
    "as alias order" \
    "10
NULL
10
0	-1" \
    "DO 0; SELECT n FROM numbers AS nums ORDER BY id; SELECT @@warning_count, ROW_COUNT();" \
    "$DATABASE"

expect_output \
    "bare alias order" \
    "10
NULL
10
0	-1" \
    "DO 0; SELECT n FROM numbers nums ORDER BY id; SELECT @@warning_count, ROW_COUNT();" \
    "$DATABASE"

expect_output \
    "same alias as table" \
    "10
NULL
10" \
    "SELECT n FROM numbers AS numbers ORDER BY id;" \
    "$DATABASE"

expect_output \
    "quoted reserved alias" \
    "10
NULL
10" \
    "SELECT n FROM numbers AS \`select\` ORDER BY id;" \
    "$DATABASE"

expect_error \
    "unquoted reserved alias" \
    1064 \
    42000 \
    "near 'select'" \
    "SELECT n FROM numbers AS select;" \
    "$DATABASE"

expect_output \
    "schema-qualified alias" \
    "10
0	-1" \
    "DO 0; SELECT n FROM ${DATABASE}.numbers AS nums WHERE n IS NOT NULL ORDER BY n DESC LIMIT 1; SELECT @@warning_count, ROW_COUNT();"

expect_output \
    "wildcard alias" \
    "1	10	5
2	NULL	6
0	-1" \
    "DO 0; SELECT * FROM numbers AS nums ORDER BY id LIMIT 2; SELECT @@warning_count, ROW_COUNT();" \
    "$DATABASE"

expect_output \
    "all alias" \
    "10
10
0	-1" \
    "DO 0; SELECT ALL n FROM numbers AS nums WHERE n IS NOT NULL ORDER BY n LIMIT 2; SELECT @@warning_count, ROW_COUNT();" \
    "$DATABASE"

expect_output \
    "distinct alias" \
    "NULL
10
0	-1" \
    "DO 0; SELECT DISTINCT n FROM numbers AS nums ORDER BY n LIMIT 10; SELECT @@warning_count, ROW_COUNT();" \
    "$DATABASE"

expect_output \
    "distinctrow alias" \
    "NULL
10
0	-1" \
    "DO 0; SELECT DISTINCTROW n FROM numbers nums ORDER BY n LIMIT 10; SELECT @@warning_count, ROW_COUNT();" \
    "$DATABASE"

expect_output \
    "count alias" \
    "3
0	-1" \
    "DO 0; SELECT COUNT(*) FROM numbers AS nums; SELECT @@warning_count, ROW_COUNT();" \
    "$DATABASE"

expect_output \
    "min max alias" \
    "10	10
0	-1" \
    "DO 0; SELECT MIN(n), MAX(n) FROM numbers AS nums; SELECT @@warning_count, ROW_COUNT();" \
    "$DATABASE"

expect_output \
    "alias-qualified columns accepted upstream" \
    "10
10" \
    "SELECT nums.n FROM numbers AS nums WHERE nums.n IS NOT NULL ORDER BY nums.id;" \
    "$DATABASE"

expect_error \
    "original qualifier hidden after alias" \
    1054 \
    42S22 \
    "Unknown column 'numbers.n' in 'field list'" \
    "SELECT numbers.n FROM numbers AS nums;" \
    "$DATABASE"

expect_error \
    "duplicate alias in join" \
    1066 \
    42000 \
    "Not unique table/alias: 'nums'" \
    "SELECT nums.n FROM numbers AS nums, numbers AS nums;" \
    "$DATABASE"

printf '%s\n' "baseline-select-table-alias MySQL 8.4.9 expectations verified"
