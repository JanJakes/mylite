#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_schema_if_exists_expectations_$$"
OTHER_DATABASE="${DATABASE}_other"
MISSING_DATABASE="${DATABASE}_missing"
DROP_DATABASE="${DATABASE}_drop"

fail() {
    printf '%s\n' "mysql_baseline_schema_if_exists_expectations: $1" >&2
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

run_mysql_verbose() {
    sql=$1
    shift
    printf '%s\n' "$sql" \
        | docker exec -i "$MYSQL_CONTAINER" mysql -uroot -vvv "$@"
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

expect_verbose_contains() {
    label=$1
    needle=$2
    sql=$3
    shift 3

    output=$(run_mysql_verbose "$sql" "$@")
    case "$output" in
        *"$needle"*) ;;
        *) fail "$label: expected verbose output containing [$needle], got [$output]" ;;
    esac
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
        *) fail "$label: expected error $code/$state containing [$message], got [$output]" ;;
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
    run_mysql "DROP DATABASE IF EXISTS ${OTHER_DATABASE};" >/dev/null 2>&1 || true
    run_mysql "DROP DATABASE IF EXISTS ${MISSING_DATABASE};" >/dev/null 2>&1 || true
    run_mysql "DROP DATABASE IF EXISTS ${DROP_DATABASE};" >/dev/null 2>&1 || true
}

trap cleanup EXIT

version=$(run_mysql 'SELECT VERSION();')
case "$version" in
    8.4.9*) ;;
    *) fail "expected MySQL 8.4.9 runtime, got [$version]" ;;
esac

cleanup

expect_output \
    "create database if not exists creates missing" \
    "1	0	NULL" \
    "CREATE DATABASE IF NOT EXISTS ${DATABASE}; SELECT ROW_COUNT(), @@warning_count, DATABASE();"

expect_output \
    "create schema if not exists creates missing" \
    "1	0" \
    "CREATE SCHEMA IF NOT EXISTS ${OTHER_DATABASE}; SELECT ROW_COUNT(), @@warning_count;"

expect_output \
    "create existing database warning status" \
    "1	1" \
    "CREATE DATABASE IF NOT EXISTS ${DATABASE}; SELECT ROW_COUNT(), @@warning_count;"

expect_output \
    "create existing database warning row" \
    "Note	1007	Can't create database '${DATABASE}'; database exists" \
    "CREATE DATABASE IF NOT EXISTS ${DATABASE}; SHOW WARNINGS;"

expect_output \
    "create existing schema warning row" \
    "Note	1007	Can't create database '${OTHER_DATABASE}'; database exists" \
    "CREATE SCHEMA IF NOT EXISTS ${OTHER_DATABASE}; SHOW WARNINGS;"

expect_verbose_contains \
    "drop existing database if exists affected rows" \
    "Query OK, 0 rows affected" \
    "DROP DATABASE IF EXISTS ${OTHER_DATABASE};"

expect_output \
    "drop selected if exists clears selected database" \
    "NULL" \
    "USE ${DATABASE}; DROP DATABASE IF EXISTS ${DATABASE}; SELECT DATABASE();"

expect_output \
    "drop missing database if exists diagnostics are not stored" \
    "-1	0" \
    "DROP DATABASE IF EXISTS ${MISSING_DATABASE}; SELECT ROW_COUNT(), @@warning_count;"

expect_output_with_headers \
    "drop missing database if exists show warnings empty" \
    "$(cat <<EOF
@@session.warning_count
0
EOF
)" \
    "DROP DATABASE IF EXISTS ${MISSING_DATABASE}; SHOW COUNT(*) WARNINGS;"

expect_output \
    "drop missing schema if exists diagnostics are not stored" \
    "-1	0" \
    "DROP SCHEMA IF EXISTS ${MISSING_DATABASE}; SELECT ROW_COUNT(), @@warning_count;"

expect_verbose_contains \
    "drop missing database if exists client note" \
    "1 warning" \
    "DROP DATABASE IF EXISTS ${MISSING_DATABASE};"

expect_verbose_contains \
    "drop database if exists reports removed table count" \
    "Query OK, 2 rows affected" \
    "CREATE DATABASE ${DROP_DATABASE}; "\
"USE ${DROP_DATABASE}; "\
"CREATE TABLE a (id INT); "\
"CREATE TABLE b (id INT); "\
"DROP DATABASE IF EXISTS ${DROP_DATABASE};"

expect_output \
    "drop existing database if exists row count state" \
    "-1	0" \
    "CREATE DATABASE ${DROP_DATABASE}; "\
"USE ${DROP_DATABASE}; "\
"CREATE TABLE a (id INT); "\
"CREATE TABLE b (id INT); "\
"DROP DATABASE IF EXISTS ${DROP_DATABASE}; "\
"SELECT ROW_COUNT(), @@warning_count;"

expect_error \
    "qualified schema name syntax" \
    1064 \
    42000 \
    "near '.b'" \
    "CREATE DATABASE IF NOT EXISTS a.b;"

expect_upstream_accepts \
    "schema options are outside MyLite slice" \
    "CREATE DATABASE IF NOT EXISTS ${DATABASE} DEFAULT CHARACTER SET utf8mb4; DROP DATABASE ${DATABASE};"

printf '%s\n' "baseline-schema-if-exists MySQL 8.4.9 expectations verified"
