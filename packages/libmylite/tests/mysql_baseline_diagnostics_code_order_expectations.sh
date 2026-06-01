#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_diagnostics_code_order_expectations_$$"

fail() {
    printf '%s\n' "mysql_baseline_diagnostics_code_order_expectations: $1" >&2
    exit 1
}

run_mysql() {
    sql=$1
    shift
    printf '%s\n' "$sql" \
        | docker exec -i "$MYSQL_CONTAINER" mysql --protocol=TCP -h127.0.0.1 -uroot \
            --batch --raw --skip-column-names "$@"
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
        *) fail "$label: expected error $code/$state containing [$message], got [$output]" ;;
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

expect_error \
    "parse error code and sqlstate" \
    1064 \
    42000 \
    "SQL syntax" \
    "BAD SQL;"

expect_error \
    "session-only warning count error code and sqlstate" \
    1238 \
    HY000 \
    "SESSION variable" \
    "SELECT @@global.warning_count;"

expect_error \
    "unknown system variable error code and sqlstate" \
    1193 \
    HY000 \
    "Unknown system variable 'no_such_mylite_variable'" \
    "SELECT @@no_such_mylite_variable;"

run_mysql \
    "CREATE TABLE strings (id INT NOT NULL, v VARCHAR(3), n VARCHAR(3) NOT NULL, z VARCHAR(0));" \
    "$DATABASE" >/dev/null

expect_error \
    "varchar overlength error code and sqlstate" \
    1406 \
    22001 \
    "Data too long for column 'v' at row 1" \
    "INSERT INTO strings VALUES (1, 'abcd', 'abc', '');" \
    "$DATABASE"

expect_error \
    "not null error code and sqlstate" \
    1048 \
    23000 \
    "Column 'n' cannot be null" \
    "INSERT INTO strings VALUES (1, 'abc', NULL, '');" \
    "$DATABASE"

trailing_space_warnings_expected=$(cat <<\EXPECTED
Note	1265	Data truncated for column 'v' at row 1
Note	1265	Data truncated for column 'n' at row 1
2
2	0	-1
0
EXPECTED
)
expect_output \
    "warning order, count preservation, and scalar clearing" \
    "$trailing_space_warnings_expected" \
    "INSERT INTO strings VALUES (9, 'abc ', 'abc ', ''); "\
"SHOW WARNINGS; "\
"SHOW COUNT(*) WARNINGS; "\
"SELECT @@warning_count, @@error_count, ROW_COUNT(); "\
"SHOW COUNT(*) WARNINGS;" \
    "$DATABASE"

processlist_warning=$(run_mysql \
    "SHOW PROCESSLIST; SHOW WARNINGS;" \
    | tail -n 1)
case "$processlist_warning" in
    "Warning	1287	"*"'INFORMATION_SCHEMA.PROCESSLIST' is deprecated"*) ;;
    *) fail "processlist warning code/order: got [$processlist_warning]" ;;
esac

printf '%s\n' "mysql_baseline_diagnostics_code_order_expectations: ok"
