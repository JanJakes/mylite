#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
MYSQL_BIN="${MYLITE_MYSQL_BIN:-mysql}"
MYSQL_SOCKET="${MYLITE_MYSQL_SOCKET:-}"
DATABASE="mylite_parser_ddl_default_order_residuals_$$"

fail() {
    printf '%s\n' "mysql_parser_corpus_ddl_default_order_residuals_expectations: $1" >&2
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

    output=$(run_mysql "$sql")
    if [ "$output" != "$expected" ]; then
        fail "$label: expected [$expected], got [$output]"
    fi
}

expect_success() {
    label=$1
    sql=$2

    if ! run_mysql "$sql" >/dev/null 2>&1; then
        fail "$label: expected success"
    fi
}

expect_error_contains() {
    label=$1
    sql=$2
    expected=$3

    set +e
    output=$(run_mysql "$sql" 2>&1)
    status=$?
    set -e
    if [ "$status" -eq 0 ]; then
        fail "$label: expected error"
    fi
    case "$output" in
        *"$expected"*) ;;
        *) fail "$label: expected error containing [$expected], got [$output]" ;;
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
run_mysql "CREATE DATABASE ${DATABASE}; USE ${DATABASE}; "\
"CREATE TABLE t1(payoutid INT, bandid INT, c1 INT, CONSTRAINT t1_chk_1 CHECK (c1 > 0));" \
    >/dev/null

expect_success \
    "repeated defaults" \
    "USE ${DATABASE}; CREATE TABLE duplicate_defaults ("\
"a INT DEFAULT 1 DEFAULT 2, b INT DEFAULT NULL DEFAULT 5, "\
"c INT DEFAULT 6 DEFAULT NULL, d CHAR(4) DEFAULT 'a' DEFAULT 'b');"
expect_output \
    "repeated default metadata" \
    "a	2
b	5
c	<NULL>
d	b" \
    "USE ${DATABASE}; SELECT COLUMN_NAME, IFNULL(COLUMN_DEFAULT, '<NULL>') "\
"FROM INFORMATION_SCHEMA.COLUMNS WHERE TABLE_SCHEMA = '${DATABASE}' "\
"AND TABLE_NAME = 'duplicate_defaults' ORDER BY ORDINAL_POSITION;"
expect_output \
    "repeated default inserted values" \
    "2	5	N	b" \
    "USE ${DATABASE}; INSERT INTO duplicate_defaults () VALUES (); "\
"SELECT IFNULL(a,'N'), IFNULL(b,'N'), IFNULL(c,'N'), d FROM duplicate_defaults;"

expect_success \
    "float decimal-looking precision" \
    "USE ${DATABASE}; CREATE TABLE float_decimal_precision (f FLOAT(10.3));"
expect_output \
    "float decimal-looking precision metadata" \
    "float" \
    "USE ${DATABASE}; SELECT COLUMN_TYPE FROM INFORMATION_SCHEMA.COLUMNS "\
"WHERE TABLE_SCHEMA = '${DATABASE}' AND TABLE_NAME = 'float_decimal_precision';"

expect_success \
    "alter table add column order by action" \
    "USE ${DATABASE}; ALTER TABLE t1 ADD COLUMN new_col INT, ORDER BY payoutid,bandid;"
expect_success \
    "alter table float decimal precision multi action" \
    "USE ${DATABASE}; ALTER TABLE t1 MODIFY COLUMN c1 FLOAT(10.3), "\
"DROP CHECK t1_chk_1, ADD CONSTRAINT CHECK(c1 > 10.1) ENFORCED;"

expect_error_contains \
    "bare add partition reaches runtime" \
    "USE ${DATABASE}; ALTER TABLE t1 ADD PARTITION;" \
    "Partition management on a not partitioned table is not possible"
expect_error_contains \
    "bare reorganize partition reaches runtime" \
    "USE ${DATABASE}; ALTER TABLE t1 REORGANIZE PARTITION;" \
    "Partition management on a not partitioned table is not possible"

cleanup

printf '%s\n' "mysql_parser_corpus_ddl_default_order_residuals_expectations: ok"
