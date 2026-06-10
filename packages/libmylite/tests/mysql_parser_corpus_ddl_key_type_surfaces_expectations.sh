#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
MYSQL_BIN="${MYLITE_MYSQL_BIN:-mysql}"
MYSQL_SOCKET="${MYLITE_MYSQL_SOCKET:-}"
DATABASE="mylite_parser_ddl_key_type_$$"

fail() {
    printf '%s\n' "mysql_parser_corpus_ddl_key_type_surfaces_expectations: $1" >&2
    exit 1
}

run_mysql() {
    sql=$1
    shift
    if [ -n "$MYSQL_SOCKET" ]; then
        printf '%s\n' "$sql" \
            | "$MYSQL_BIN" --protocol=SOCKET --socket="$MYSQL_SOCKET" -uroot \
                --batch --raw --skip-column-names "$@"
    else
        printf '%s\n' "$sql" \
            | docker exec -i "$MYSQL_CONTAINER" mysql --protocol=TCP -h127.0.0.1 -uroot \
                --batch --raw --skip-column-names "$@"
    fi
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

expect_success() {
    label=$1
    sql=$2
    shift 2

    if ! run_mysql "$sql" "$@" >/dev/null; then
        fail "$label: command failed"
    fi
}

expect_contains() {
    label=$1
    needle=$2
    sql=$3
    shift 3

    output=$(run_mysql "$sql" "$@")
    case "$output" in
        *"$needle"*) ;;
        *) fail "$label: expected output to contain [$needle], got [$output]" ;;
    esac
}

expect_not_contains() {
    label=$1
    needle=$2
    sql=$3
    shift 3

    output=$(run_mysql "$sql" "$@")
    case "$output" in
        *"$needle"*) fail "$label: expected output not to contain [$needle], got [$output]" ;;
    esac
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
run_mysql "CREATE DATABASE ${DATABASE};" >/dev/null

expect_success \
    "inline KEY accepted" \
    "USE ${DATABASE}; CREATE TABLE inline_key (id INT KEY, v INT);"
expect_output \
    "inline KEY column metadata" \
    "PRI" \
    "USE ${DATABASE}; SELECT COLUMN_KEY FROM INFORMATION_SCHEMA.COLUMNS "\
"WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = 'inline_key' AND COLUMN_NAME = 'id';"
expect_contains \
    "inline KEY show create" \
    'PRIMARY KEY (`id`)' \
    "USE ${DATABASE}; SHOW CREATE TABLE inline_key;"

expect_success \
    "primary prefix key accepted" \
    "USE ${DATABASE}; CREATE TABLE prefix_pk "\
"(a VARCHAR(100), b INT, PRIMARY KEY (a(10), b));"
expect_contains \
    "primary prefix show create" \
    'PRIMARY KEY (`a`(10),`b`)' \
    "USE ${DATABASE}; SHOW CREATE TABLE prefix_pk;"

expect_success \
    "index key block size accepted" \
    "USE ${DATABASE}; CREATE TABLE idx_kbs "\
"(a INT, KEY a_idx (a) KEY_BLOCK_SIZE=1024) ENGINE=InnoDB; "\
"CREATE INDEX idx_kbs_create ON idx_kbs (a) KEY_BLOCK_SIZE 512;"
expect_contains \
    "index key block size keeps indexes" \
    'KEY `idx_kbs_create` (`a`)' \
    "USE ${DATABASE}; SHOW CREATE TABLE idx_kbs;"
expect_not_contains \
    "index key block size not rendered for InnoDB probe" \
    'KEY_BLOCK_SIZE' \
    "USE ${DATABASE}; SHOW CREATE TABLE idx_kbs;"

expect_output \
    "zerofill accepted with warnings" \
    "4" \
    "USE ${DATABASE}; CREATE TABLE zint "\
"(a INT(4) ZEROFILL, b DECIMAL(5,2) ZEROFILL, c FLOAT ZEROFILL); "\
"SELECT @@warning_count;"
expect_contains \
    "zerofill integer rendering" \
    'int(4) unsigned zerofill' \
    "USE ${DATABASE}; SHOW CREATE TABLE zint;"
expect_contains \
    "zerofill decimal rendering" \
    'decimal(5,2) unsigned zerofill' \
    "USE ${DATABASE}; SHOW CREATE TABLE zint;"
expect_contains \
    "zerofill approximate rendering" \
    'float unsigned zerofill' \
    "USE ${DATABASE}; SHOW CREATE TABLE zint;"

cleanup

printf '%s\n' "mysql_parser_corpus_ddl_key_type_surfaces_expectations: ok"
