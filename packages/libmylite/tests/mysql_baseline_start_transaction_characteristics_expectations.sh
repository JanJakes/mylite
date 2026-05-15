#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
MYSQL_ARGS="--protocol=TCP -h127.0.0.1 -uroot --batch --raw --skip-column-names"
DATABASE="mylite_start_transaction_characteristics_expectations_$$"

fail() {
    printf '%s\n' "mysql_baseline_start_transaction_characteristics_expectations: $1" >&2
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
run_mysql \
    "CREATE DATABASE ${DATABASE}; USE ${DATABASE}; "\
"CREATE TABLE t (id INT PRIMARY KEY, v INT); INSERT INTO t VALUES (1, 10);" \
    >/dev/null

accepted_expected=$(cat <<'EXPECTED'
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
    "accepted start transaction characteristic forms" \
    "$accepted_expected" \
    "START TRANSACTION READ ONLY; SELECT ROW_COUNT(), @@warning_count; ROLLBACK; "\
"START TRANSACTION READ WRITE; SELECT ROW_COUNT(), @@warning_count; ROLLBACK; "\
"START TRANSACTION WITH CONSISTENT SNAPSHOT; SELECT ROW_COUNT(), @@warning_count; ROLLBACK; "\
"START TRANSACTION READ ONLY, WITH CONSISTENT SNAPSHOT; SELECT ROW_COUNT(), @@warning_count; ROLLBACK; "\
"START TRANSACTION WITH CONSISTENT SNAPSHOT, READ WRITE; SELECT ROW_COUNT(), @@warning_count; ROLLBACK; "\
"START TRANSACTION READ ONLY, READ ONLY; SELECT ROW_COUNT(), @@warning_count; ROLLBACK; "\
"START TRANSACTION READ WRITE, READ WRITE; SELECT ROW_COUNT(), @@warning_count; ROLLBACK; "\
"START TRANSACTION WITH CONSISTENT SNAPSHOT, WITH CONSISTENT SNAPSHOT; "\
"SELECT ROW_COUNT(), @@warning_count; ROLLBACK;" \
    "$DATABASE"

expect_output \
    "consistent snapshot warning count under read committed" \
    "1" \
    "SET SESSION TRANSACTION ISOLATION LEVEL READ COMMITTED; "\
"START TRANSACTION WITH CONSISTENT SNAPSHOT; SELECT @@warning_count; ROLLBACK;" \
    "$DATABASE"

expect_output \
    "consistent snapshot warning count under other non-repeatable isolation levels" \
    "1
1" \
    "SET SESSION TRANSACTION ISOLATION LEVEL READ UNCOMMITTED; "\
"START TRANSACTION WITH CONSISTENT SNAPSHOT; SELECT @@warning_count; ROLLBACK; "\
"SET SESSION TRANSACTION ISOLATION LEVEL SERIALIZABLE; "\
"START TRANSACTION WITH CONSISTENT SNAPSHOT; SELECT @@warning_count; ROLLBACK;" \
    "$DATABASE"

expect_output \
    "consistent snapshot warning message under read committed" \
    "Warning	138	InnoDB: WITH CONSISTENT SNAPSHOT was ignored because this phrase can only be used with REPEATABLE READ isolation level." \
    "SET SESSION TRANSACTION ISOLATION LEVEL READ COMMITTED; "\
"START TRANSACTION WITH CONSISTENT SNAPSHOT; SHOW WARNINGS; ROLLBACK;" \
    "$DATABASE"

expect_output \
    "repeatable read consistent snapshot has no warning" \
    "0" \
    "SET SESSION TRANSACTION ISOLATION LEVEL REPEATABLE READ; "\
"START TRANSACTION WITH CONSISTENT SNAPSHOT; SHOW COUNT(*) WARNINGS; ROLLBACK;" \
    "$DATABASE"

expect_error \
    "read only transaction blocks persistent write" \
    1792 \
    25006 \
    "Cannot execute statement in a READ ONLY transaction." \
    "START TRANSACTION READ ONLY; INSERT INTO t VALUES (2, 20);" \
    "$DATABASE"

expect_output \
    "read only transaction allows temporary table dml" \
    "1	0" \
    "CREATE TEMPORARY TABLE tmp (id INT); START TRANSACTION READ ONLY; "\
"INSERT INTO tmp VALUES (1); COMMIT; SELECT COUNT(*), @@warning_count FROM tmp;" \
    "$DATABASE"

expect_output \
    "statement read write overrides pending read only" \
    "3	60	0" \
    "SET TRANSACTION READ ONLY; START TRANSACTION READ WRITE; "\
"INSERT INTO t VALUES (2, 20); COMMIT; INSERT INTO t VALUES (3, 30); "\
"SELECT COUNT(*), SUM(v), @@warning_count FROM t;" \
    "$DATABASE"

expect_error \
    "session read only resumes after statement read write transaction" \
    1792 \
    25006 \
    "Cannot execute statement in a READ ONLY transaction." \
    "SET SESSION TRANSACTION READ ONLY; START TRANSACTION READ WRITE; "\
"INSERT INTO t VALUES (4, 40); COMMIT; INSERT INTO t VALUES (5, 50);" \
    "$DATABASE"
run_mysql "SET SESSION TRANSACTION READ WRITE;" "$DATABASE" >/dev/null
expect_output \
    "statement read write committed before session read only resumed" \
    "4	100" \
    "SELECT COUNT(*), SUM(v) FROM t;" \
    "$DATABASE"

expect_error \
    "pending read only applies when start has only snapshot" \
    1792 \
    25006 \
    "Cannot execute statement in a READ ONLY transaction." \
    "SET TRANSACTION READ ONLY; START TRANSACTION WITH CONSISTENT SNAPSHOT; "\
"INSERT INTO t VALUES (6, 60);" \
    "$DATABASE"
run_mysql "ROLLBACK;" "$DATABASE" >/dev/null

expect_output \
    "pending isolation consumed by snapshot start" \
    "1
0" \
    "SET TRANSACTION ISOLATION LEVEL READ COMMITTED; "\
"START TRANSACTION WITH CONSISTENT SNAPSHOT; SELECT @@warning_count; ROLLBACK; "\
"START TRANSACTION WITH CONSISTENT SNAPSHOT; SELECT @@warning_count; ROLLBACK;" \
    "$DATABASE"

expect_error \
    "contradictory read only read write is syntax error" \
    1064 \
    42000 \
    "near ''" \
    "START TRANSACTION READ ONLY, READ WRITE;" \
    "$DATABASE"

expect_error \
    "contradictory read write read only is syntax error" \
    1064 \
    42000 \
    "near ''" \
    "START TRANSACTION READ WRITE, READ ONLY;" \
    "$DATABASE"

expect_error \
    "start transaction isolation characteristic is syntax error" \
    1064 \
    42000 \
    "near 'ISOLATION LEVEL READ COMMITTED'" \
    "START TRANSACTION ISOLATION LEVEL READ COMMITTED;" \
    "$DATABASE"

expect_error \
    "begin read only is syntax error" \
    1064 \
    42000 \
    "near 'READ ONLY'" \
    "BEGIN READ ONLY;" \
    "$DATABASE"

expect_output \
    "nested start read only commits prior transaction" \
    "1	10
2	20
3	30
4	40
7	70
8	80" \
    "INSERT INTO t VALUES (7, 70); START TRANSACTION; INSERT INTO t VALUES (8, 80); "\
"START TRANSACTION READ ONLY; ROLLBACK; SELECT id, v FROM t ORDER BY id;" \
    "$DATABASE"

expect_output \
    "read only transaction ddl follows implicit commit path" \
    "0	0
ddl_marker" \
    "START TRANSACTION READ ONLY; CREATE TABLE ddl_marker (id INT); "\
"SELECT ROW_COUNT(), @@warning_count; ROLLBACK; SHOW TABLES LIKE 'ddl_marker';" \
    "$DATABASE"
