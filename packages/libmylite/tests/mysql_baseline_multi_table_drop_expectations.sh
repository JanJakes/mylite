#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_multi_table_drop_expectations_$$"
OTHER_DATABASE="${DATABASE}_other"
MISSING_DATABASE="${DATABASE}_missing"

fail() {
    printf '%s\n' "mysql_baseline_multi_table_drop_expectations: $1" >&2
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
}

trap cleanup EXIT

version=$(run_mysql 'SELECT VERSION();')
case "$version" in
    8.4.9*) ;;
    *) fail "expected MySQL 8.4.9 runtime, got [$version]" ;;
esac

cleanup

expect_output \
    "drop table list removes all existing unqualified tables" \
    "0	0" \
    "CREATE DATABASE ${DATABASE}; "\
"USE ${DATABASE}; "\
"CREATE TABLE a (id INT); "\
"CREATE TABLE b (id INT); "\
"DROP TABLE a, b; "\
"SELECT ROW_COUNT(), @@warning_count; "\
"SHOW TABLES;"

cleanup

expect_output \
    "drop table list accepts qualified targets without default schema" \
    "0	0" \
    "CREATE DATABASE ${DATABASE}; "\
"CREATE DATABASE ${OTHER_DATABASE}; "\
"CREATE TABLE ${DATABASE}.a (id INT); "\
"CREATE TABLE ${OTHER_DATABASE}.b (id INT); "\
"DROP TABLE ${DATABASE}.a, ${OTHER_DATABASE}.b; "\
"SELECT ROW_COUNT(), @@warning_count; "\
"SHOW TABLES FROM ${DATABASE}; "\
"SHOW TABLES FROM ${OTHER_DATABASE};"

cleanup

expect_error \
    "drop table list missing target is atomic" \
    1051 \
    42S02 \
    "Unknown table '${DATABASE}.missing_table'" \
    "CREATE DATABASE ${DATABASE}; "\
"USE ${DATABASE}; "\
"CREATE TABLE a (id INT); "\
"CREATE TABLE b (id INT); "\
"DROP TABLE a, missing_table, b;"

expect_output \
    "drop table list missing target leaves existing tables" \
    "$(cat <<EOF
a
b
EOF
)" \
    "USE ${DATABASE}; SHOW TABLES;"

cleanup

expect_error \
    "drop table list reports multiple missing targets" \
    1051 \
    42S02 \
    "Unknown table '${DATABASE}.one,${DATABASE}.two'" \
    "CREATE DATABASE ${DATABASE}; "\
"USE ${DATABASE}; "\
"DROP TABLE one, two;"

cleanup

expect_error \
    "drop table list explicit missing schema is unknown table" \
    1051 \
    42S02 \
    "Unknown table '${MISSING_DATABASE}.nope'" \
    "CREATE DATABASE ${DATABASE}; "\
"CREATE TABLE ${DATABASE}.a (id INT); "\
"DROP TABLE ${DATABASE}.a, ${MISSING_DATABASE}.nope;"

expect_output \
    "drop table list explicit missing schema leaves existing table" \
    "a" \
    "SHOW TABLES FROM ${DATABASE};"

cleanup

expect_output \
    "drop table if exists mixed target row count" \
    "0	2" \
    "CREATE DATABASE ${DATABASE}; "\
"USE ${DATABASE}; "\
"CREATE TABLE a (id INT); "\
"DROP TABLE IF EXISTS a, missing_one, missing_two; "\
"SELECT ROW_COUNT(), @@warning_count; "\
"SHOW TABLES;"

cleanup

expect_output \
    "drop table if exists mixed target warnings" \
    "$(cat <<EOF
Note	1051	Unknown table '${DATABASE}.missing_one'
Note	1051	Unknown table '${DATABASE}.missing_two'
EOF
)" \
    "CREATE DATABASE ${DATABASE}; "\
"USE ${DATABASE}; "\
"CREATE TABLE a (id INT); "\
"DROP TABLE IF EXISTS a, missing_one, missing_two; "\
"SHOW WARNINGS;"

cleanup

expect_output \
    "drop table if exists all missing warnings" \
    "$(cat <<EOF
Note	1051	Unknown table '${DATABASE}.one'
Note	1051	Unknown table '${DATABASE}.two'
EOF
)" \
    "CREATE DATABASE ${DATABASE}; "\
"USE ${DATABASE}; "\
"DROP TABLE IF EXISTS one, two; "\
"SHOW WARNINGS;"

cleanup

expect_output \
    "drop table if exists explicit missing schema warning" \
    "Note	1051	Unknown table '${MISSING_DATABASE}.nope'" \
    "CREATE DATABASE ${DATABASE}; "\
"CREATE TABLE ${DATABASE}.a (id INT); "\
"DROP TABLE IF EXISTS ${DATABASE}.a, ${MISSING_DATABASE}.nope; "\
"SHOW WARNINGS;"

cleanup

expect_error \
    "drop table list duplicate target" \
    1066 \
    42000 \
    "Not unique table/alias: 'a'" \
    "CREATE DATABASE ${DATABASE}; "\
"USE ${DATABASE}; "\
"CREATE TABLE a (id INT); "\
"DROP TABLE a, a;"

expect_output \
    "drop table list duplicate target leaves table" \
    "a" \
    "SHOW TABLES FROM ${DATABASE};"

cleanup

expect_error \
    "drop table if exists duplicate target" \
    1066 \
    42000 \
    "Not unique table/alias: 'a'" \
    "CREATE DATABASE ${DATABASE}; "\
"USE ${DATABASE}; "\
"CREATE TABLE a (id INT); "\
"DROP TABLE IF EXISTS a, a;"

expect_output \
    "drop table if exists duplicate target leaves table" \
    "a" \
    "SHOW TABLES FROM ${DATABASE};"

cleanup

expect_error \
    "drop table list duplicate qualified target" \
    1066 \
    42000 \
    "Not unique table/alias: 'a'" \
    "CREATE DATABASE ${DATABASE}; "\
"USE ${DATABASE}; "\
"CREATE TABLE a (id INT); "\
"DROP TABLE a, ${DATABASE}.a;"

cleanup

expect_error \
    "drop table list no default schema" \
    1046 \
    3D000 \
    "No database selected" \
    "CREATE DATABASE ${DATABASE}; "\
"CREATE TABLE ${DATABASE}.a (id INT); "\
"DROP TABLE ${DATABASE}.a, b;"

expect_output \
    "drop table list no default schema leaves table" \
    "a" \
    "SHOW TABLES FROM ${DATABASE};"

cleanup

expect_error \
    "drop table if exists no default schema" \
    1046 \
    3D000 \
    "No database selected" \
    "CREATE DATABASE ${DATABASE}; "\
"CREATE TABLE ${DATABASE}.a (id INT); "\
"DROP TABLE IF EXISTS ${DATABASE}.a, b;"

expect_output \
    "drop table if exists no default schema leaves table" \
    "a" \
    "SHOW TABLES FROM ${DATABASE};"

cleanup

expect_upstream_accepts \
    "drop temporary table is outside MyLite slice" \
    "CREATE DATABASE ${DATABASE}; "\
"USE ${DATABASE}; "\
"CREATE TEMPORARY TABLE temp_a (id INT); "\
"DROP TEMPORARY TABLE temp_a;"

expect_upstream_accepts \
    "drop table restrict is outside MyLite slice" \
    "CREATE TABLE ${DATABASE}.restrict_a (id INT); "\
"DROP TABLE ${DATABASE}.restrict_a RESTRICT;"

expect_upstream_accepts \
    "drop table cascade is outside MyLite slice" \
    "CREATE TABLE ${DATABASE}.cascade_a (id INT); "\
"CREATE TABLE ${DATABASE}.cascade_b (id INT); "\
"DROP TABLE ${DATABASE}.cascade_a, ${DATABASE}.cascade_b CASCADE;"

printf '%s\n' "baseline-multi-table-drop MySQL 8.4.9 expectations verified"
