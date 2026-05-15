#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
MYSQL_ARGS="--protocol=TCP -h127.0.0.1 -uroot --batch --raw --skip-column-names"
DATABASE="mylite_set_transaction_expectations_$$"

fail() {
    printf '%s\n' "mysql_baseline_set_transaction_expectations: $1" >&2
    exit 1
}

run_mysql() {
    sql=$1
    shift
    printf '%s\n' "$sql" | docker exec -i "$MYSQL_CONTAINER" mysql $MYSQL_ARGS "$@"
}

expect_value() {
    label=$1
    expected=$2
    actual=$3

    if [ "$actual" != "$expected" ]; then
        fail "$label: expected [$expected], got [$actual]"
    fi
}

expect_output() {
    label=$1
    expected=$2
    sql=$3
    shift 3

    output=$(run_mysql "$sql" "$@")
    expect_value "$label" "$expected" "$output"
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

version=$(run_mysql "SELECT VERSION();")
case "$version" in
    8.4.9*) ;;
    *) fail "expected MySQL 8.4.9 runtime, got [$version]" ;;
esac

cleanup
run_mysql "CREATE DATABASE ${DATABASE}; USE ${DATABASE}; CREATE TABLE t (id INT PRIMARY KEY, v INT); INSERT INTO t VALUES (1, 10);" >/dev/null

accepted_expected=$(cat <<'EXPECTED'
0	0
0	0
0	0
0	0
0	0
0	0
0	0
0	0
0	0
EXPECTED
)
expect_output \
    "accepted set transaction forms" \
    "$accepted_expected" \
    "SET TRANSACTION ISOLATION LEVEL READ COMMITTED; SELECT ROW_COUNT(), @@warning_count; "\
"SET TRANSACTION ISOLATION LEVEL READ UNCOMMITTED; SELECT ROW_COUNT(), @@warning_count; "\
"SET TRANSACTION ISOLATION LEVEL REPEATABLE READ; SELECT ROW_COUNT(), @@warning_count; "\
"SET TRANSACTION ISOLATION LEVEL SERIALIZABLE; SELECT ROW_COUNT(), @@warning_count; "\
"SET TRANSACTION READ WRITE; SELECT ROW_COUNT(), @@warning_count; "\
"SET TRANSACTION READ ONLY; SELECT ROW_COUNT(), @@warning_count; "\
"SET TRANSACTION ISOLATION LEVEL READ COMMITTED, READ WRITE; SELECT ROW_COUNT(), @@warning_count; "\
"SET TRANSACTION READ WRITE, ISOLATION LEVEL READ COMMITTED; SELECT ROW_COUNT(), @@warning_count; "\
"SET SESSION TRANSACTION ISOLATION LEVEL SERIALIZABLE, READ WRITE; SELECT ROW_COUNT(), @@warning_count;" \
    "$DATABASE"

expect_error \
    "next transaction characteristics fail while active" \
    1568 \
    25001 \
    "Transaction characteristics can't be changed while a transaction is in progress" \
    "START TRANSACTION; SET TRANSACTION ISOLATION LEVEL SERIALIZABLE;" \
    "$DATABASE"

expect_error \
    "next read only blocks autocommit persistent write" \
    1792 \
    25006 \
    "Cannot execute statement in a READ ONLY transaction." \
    "SET TRANSACTION READ ONLY; INSERT INTO t VALUES (2, 20);" \
    "$DATABASE"

expect_output \
    "failed read only write does not mutate rows" \
    "1	10" \
    "SELECT COUNT(*), SUM(v) FROM t;" \
    "$DATABASE"

expect_output \
    "select consumes next read only characteristic" \
    "1
1	0	2	30" \
    "SET TRANSACTION READ ONLY; SELECT COUNT(*) FROM t; "\
"INSERT INTO t VALUES (2, 20); SELECT ROW_COUNT(), @@warning_count, COUNT(*), SUM(v) FROM t;" \
    "$DATABASE"

expect_error \
    "explicit read only transaction blocks persistent write" \
    1792 \
    25006 \
    "Cannot execute statement in a READ ONLY transaction." \
    "SET TRANSACTION READ ONLY; START TRANSACTION; INSERT INTO t VALUES (3, 30);" \
    "$DATABASE"

expect_error \
    "session read only blocks persistent write" \
    1792 \
    25006 \
    "Cannot execute statement in a READ ONLY transaction." \
    "SET SESSION TRANSACTION READ ONLY; INSERT INTO t VALUES (4, 40);" \
    "$DATABASE"

expect_output \
    "next read write overrides session read only once" \
    "1	0	3
1	0	4" \
    "SET SESSION TRANSACTION READ ONLY; SET TRANSACTION READ WRITE; "\
"INSERT INTO t VALUES (3, 30); SELECT ROW_COUNT(), @@warning_count, COUNT(*) FROM t; "\
"SET SESSION TRANSACTION READ WRITE; "\
"INSERT INTO t VALUES (4, 40); SELECT ROW_COUNT(), @@warning_count, COUNT(*) FROM t;" \
    "$DATABASE"

expect_output \
    "session read only inside active transaction does not affect active transaction" \
    "5	1	0
1	0	6" \
    "START TRANSACTION; SET SESSION TRANSACTION READ ONLY; INSERT INTO t VALUES (5, 50); "\
"COMMIT; SELECT COUNT(*), SUM(id = 5), @@warning_count FROM t; "\
"SET SESSION TRANSACTION READ WRITE; INSERT INTO t VALUES (6, 60); "\
"SELECT ROW_COUNT(), @@warning_count, COUNT(*) FROM t;" \
    "$DATABASE"

expect_error \
    "session read only inside active transaction affects later transaction" \
    1792 \
    25006 \
    "Cannot execute statement in a READ ONLY transaction." \
    "START TRANSACTION; SET SESSION TRANSACTION READ ONLY; INSERT INTO t VALUES (7, 70); "\
"COMMIT; INSERT INTO t VALUES (8, 80);" \
    "$DATABASE"
run_mysql "SET SESSION TRANSACTION READ WRITE;" "$DATABASE" >/dev/null

expect_output \
    "read only transaction allows temporary table dml" \
    "1	0	1" \
    "CREATE TEMPORARY TABLE tmp (id INT); SET TRANSACTION READ ONLY; START TRANSACTION; "\
"INSERT INTO tmp VALUES (1); COMMIT; SELECT COUNT(*), @@warning_count, COUNT(*) FROM tmp;" \
    "$DATABASE"

expect_error \
    "duplicate isolation characteristic is syntax error" \
    1064 \
    42000 \
    "near 'ISOLATION LEVEL SERIALIZABLE'" \
    "SET TRANSACTION ISOLATION LEVEL READ COMMITTED, ISOLATION LEVEL SERIALIZABLE;" \
    "$DATABASE"

expect_error \
    "duplicate access mode characteristic is syntax error" \
    1064 \
    42000 \
    "near 'READ ONLY'" \
    "SET TRANSACTION READ WRITE, READ ONLY;" \
    "$DATABASE"
