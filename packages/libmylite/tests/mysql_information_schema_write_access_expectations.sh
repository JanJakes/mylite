#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_information_schema_write_access_expectations_$$"

fail() {
    printf '%s\n' "mysql_information_schema_write_access_expectations: $1" >&2
    exit 1
}

run_mysql() {
    sql=$1
    shift
    printf '%s\n' "$sql" \
        | docker exec -i "$MYSQL_CONTAINER" mysql \
            --protocol=TCP -h127.0.0.1 -uroot --batch --raw --skip-column-names "$@"
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

expect_access_denied() {
    label=$1
    sql=$2
    shift 2

    expect_error \
        "$label" \
        1044 \
        42000 \
        "Access denied for user 'root'@'%' to database 'information_schema'" \
        "$sql" \
        "$@"
}

cleanup() {
    run_mysql "DROP DATABASE IF EXISTS ${DATABASE};" >/dev/null 2>&1 || true
}

trap cleanup EXIT

version=$(run_mysql "SELECT VERSION();")
case "$version" in
    8.4.9*) ;;
    *) fail "expected MySQL 8.4.9 runtime, got [$version]" ;;
esac

cleanup
run_mysql "CREATE DATABASE ${DATABASE}; USE ${DATABASE}; CREATE TABLE t (id INT, v INT); INSERT INTO t VALUES (1, 10);" >/dev/null

expect_access_denied \
    "create database information_schema" \
    "CREATE DATABASE information_schema;"
expect_access_denied \
    "create database if not exists information_schema" \
    "CREATE DATABASE IF NOT EXISTS information_schema;"
expect_access_denied \
    "drop database information_schema" \
    "DROP DATABASE information_schema;"
expect_access_denied \
    "drop database if exists information_schema" \
    "DROP DATABASE IF EXISTS information_schema;"

expect_access_denied \
    "create table information_schema target" \
    "CREATE TABLE information_schema.t (id INT);"
expect_access_denied \
    "create temporary table information_schema target" \
    "CREATE TEMPORARY TABLE information_schema.t (id INT);"
expect_access_denied \
    "create table if not exists information_schema target" \
    "CREATE TABLE IF NOT EXISTS information_schema.t (id INT);"
expect_access_denied \
    "create table like information_schema target" \
    "CREATE TABLE information_schema.copy LIKE ${DATABASE}.t;"
expect_access_denied \
    "create table select information_schema target" \
    "CREATE TABLE information_schema.copy AS SELECT id FROM ${DATABASE}.t;"
expect_access_denied \
    "drop table information_schema target" \
    "DROP TABLE information_schema.t;"
expect_access_denied \
    "drop table if exists information_schema target" \
    "DROP TABLE IF EXISTS information_schema.t;"

expect_access_denied \
    "create index information_schema target" \
    "CREATE INDEX idx_info ON information_schema.TABLES (TABLE_NAME);"
expect_access_denied \
    "drop index information_schema target" \
    "DROP INDEX idx_info ON information_schema.TABLES;"
expect_access_denied \
    "alter table information_schema target" \
    "ALTER TABLE information_schema.TABLES ADD COLUMN x INT;"
expect_access_denied \
    "truncate information_schema target" \
    "TRUNCATE TABLE information_schema.TABLES;"

expect_access_denied \
    "insert information_schema target" \
    "INSERT INTO information_schema.SCHEMATA (SCHEMA_NAME) VALUES ('x');"
expect_access_denied \
    "replace information_schema target" \
    "REPLACE INTO information_schema.SCHEMATA (SCHEMA_NAME) VALUES ('x');"
expect_access_denied \
    "update information_schema target" \
    "UPDATE information_schema.SCHEMATA SET SCHEMA_NAME = 'x';"
expect_access_denied \
    "delete information_schema target" \
    "DELETE FROM information_schema.SCHEMATA;"

expect_access_denied \
    "rename table to information_schema target" \
    "RENAME TABLE ${DATABASE}.t TO information_schema.t;"
expect_access_denied \
    "rename table from information_schema target" \
    "RENAME TABLE information_schema.TABLES TO ${DATABASE}.tables_copy;"
expect_access_denied \
    "alter table rename to information_schema target" \
    "ALTER TABLE ${DATABASE}.t RENAME TO information_schema.t;"

expect_output \
    "app table remains after denied writes" \
    "1	10" \
    "SELECT id, v FROM ${DATABASE}.t ORDER BY id;"
expect_output \
    "use information_schema selects synthetic schema" \
    "information_schema" \
    "USE information_schema; SELECT DATABASE();"
expect_output \
    "use mixed-case information_schema normalizes selection" \
    "information_schema" \
    "USE Information_Schema; SELECT DATABASE();"

expect_access_denied \
    "selected information_schema create table" \
    "USE information_schema; CREATE TABLE t (id INT);"
expect_access_denied \
    "selected information_schema create table like" \
    "USE information_schema; CREATE TABLE t LIKE ${DATABASE}.t;"
expect_access_denied \
    "selected information_schema create table select" \
    "USE information_schema; CREATE TABLE t AS SELECT id FROM ${DATABASE}.t;"
expect_access_denied \
    "selected information_schema drop table" \
    "USE information_schema; DROP TABLE IF EXISTS t;"
expect_access_denied \
    "selected information_schema create index" \
    "USE information_schema; CREATE INDEX idx_selected ON SCHEMATA (SCHEMA_NAME);"
expect_access_denied \
    "selected information_schema drop index" \
    "USE information_schema; DROP INDEX idx_selected ON SCHEMATA;"
expect_access_denied \
    "selected information_schema alter table" \
    "USE information_schema; ALTER TABLE SCHEMATA ADD COLUMN x INT;"
expect_access_denied \
    "selected information_schema truncate" \
    "USE information_schema; TRUNCATE TABLE SCHEMATA;"
expect_access_denied \
    "selected information_schema insert" \
    "USE information_schema; INSERT INTO SCHEMATA (SCHEMA_NAME) VALUES ('x');"
expect_access_denied \
    "selected information_schema replace" \
    "USE information_schema; REPLACE INTO SCHEMATA (SCHEMA_NAME) VALUES ('x');"
expect_access_denied \
    "selected information_schema update" \
    "USE information_schema; UPDATE SCHEMATA SET SCHEMA_NAME = 'x';"
expect_access_denied \
    "selected information_schema delete" \
    "USE information_schema; DELETE FROM SCHEMATA;"
expect_access_denied \
    "selected information_schema rename source" \
    "USE information_schema; RENAME TABLE SCHEMATA TO ${DATABASE}.schemata_copy;"
expect_access_denied \
    "selected information_schema alter rename target" \
    "USE information_schema; ALTER TABLE ${DATABASE}.t RENAME TO t;"

expect_output \
    "selected information_schema unqualified read" \
    "information_schema" \
    "USE information_schema; SELECT SCHEMA_NAME FROM SCHEMATA WHERE SCHEMA_NAME = 'information_schema';"

printf '%s\n' "mysql_information_schema_write_access_expectations: ok"
