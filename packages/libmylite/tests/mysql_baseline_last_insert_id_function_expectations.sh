#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_last_insert_id_function_expectations_$$"

fail() {
    printf '%s\n' "mysql_baseline_last_insert_id_function_expectations: $1" >&2
    exit 1
}

run_mysql() {
    sql=$1
    shift
    printf '%s\n' "$sql" \
        | docker exec -i "$MYSQL_CONTAINER" mysql --protocol=TCP -h127.0.0.1 -uroot --batch --raw --skip-column-names "$@"
}

run_mysql_with_headers() {
    sql=$1
    shift
    printf '%s\n' "$sql" \
        | docker exec -i "$MYSQL_CONTAINER" mysql --protocol=TCP -h127.0.0.1 -uroot --batch --raw "$@"
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

expect_output \
    "fresh last insert id starts at zero" \
    "1
0	0	-1" \
    "SELECT 1; SELECT LAST_INSERT_ID(), @@warning_count, ROW_COUNT();"

expected_labels=$(cat <<EOF
last_insert_id()	Last_Insert_Id()	LAST_INSERT_ID ()	(LAST_INSERT_ID())
0	0	0	0
EOF
)
expect_output_with_headers \
    "source expression labels are preserved" \
    "$expected_labels" \
    "SELECT last_insert_id(), Last_Insert_Id(), LAST_INSERT_ID (), (LAST_INSERT_ID());"

expect_output \
    "from dual returns last insert id" \
    "0" \
    "SELECT LAST_INSERT_ID() FROM DUAL;"

expect_output \
    "non-auto-increment insert leaves last insert id unchanged" \
    "0	2	0
0" \
    "CREATE DATABASE ${DATABASE}; "\
"USE ${DATABASE}; "\
"CREATE TABLE t (id INT, v INT); "\
"INSERT INTO t VALUES (1, 10), (2, 20); "\
"SELECT LAST_INSERT_ID(), ROW_COUNT(), @@warning_count; "\
"SELECT LAST_INSERT_ID(); "\
"DROP DATABASE ${DATABASE};"

expect_output \
    "mysql one-argument form sets following last insert id" \
    "7	7
NULL	0
18446744073709551615	18446744073709551615" \
    "SELECT LAST_INSERT_ID(7), LAST_INSERT_ID(); "\
"SELECT LAST_INSERT_ID(NULL), LAST_INSERT_ID(); "\
"SELECT LAST_INSERT_ID(-1), LAST_INSERT_ID();"

expect_error \
    "mysql rejects multiple arguments" \
    1582 \
    42000 \
    "Incorrect parameter count" \
    "SELECT LAST_INSERT_ID(1, 2);"

expect_error \
    "bare last insert id is not a function" \
    1054 \
    42S22 \
    "Unknown column 'LAST_INSERT_ID'" \
    "SELECT LAST_INSERT_ID;"

expect_output \
    "mysql accepts limit outside this mylite slice" \
    "0" \
    "SELECT LAST_INSERT_ID() LIMIT 1;"
