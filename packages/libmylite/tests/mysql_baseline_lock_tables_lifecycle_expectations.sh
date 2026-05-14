#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_lock_tables_expectations_$$"

fail() {
    printf '%s\n' "mysql_baseline_lock_tables_lifecycle_expectations: $1" >&2
    exit 1
}

run_mysql() {
    sql=$1
    shift
    printf '%s\n' "$sql" \
        | docker exec -i "$MYSQL_CONTAINER" mysql --protocol=TCP -h127.0.0.1 -uroot \
            --batch --raw --skip-column-names "$@"
}

run_mysql_force() {
    sql=$1
    shift
    printf '%s\n' "$sql" \
        | docker exec -i "$MYSQL_CONTAINER" mysql --protocol=TCP -h127.0.0.1 -uroot \
            --batch --raw --skip-column-names --force "$@" 2>&1
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

expect_force_output() {
    label=$1
    expected=$2
    sql=$3
    shift 3

    output=$(run_mysql_force "$sql" "$@")
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
run_mysql \
    "CREATE DATABASE ${DATABASE}; "\
"CREATE TABLE ${DATABASE}.t (id INT PRIMARY KEY, v INT); "\
"CREATE TABLE ${DATABASE}.u (id INT); "\
"INSERT INTO ${DATABASE}.t VALUES (1, 10);" >/dev/null

expect_output \
    "supported lock and unlock forms" \
    "0	0
0	0
0	0
0	0
0	0
0	0" \
    "LOCK TABLES t READ; SELECT ROW_COUNT(), @@warning_count; "\
"UNLOCK TABLES; SELECT ROW_COUNT(), @@warning_count; "\
"LOCK TABLE t WRITE; SELECT ROW_COUNT(), @@warning_count; "\
"UNLOCK TABLE; SELECT ROW_COUNT(), @@warning_count; "\
"LOCK TABLES t AS reader READ LOCAL, u WRITE; SELECT ROW_COUNT(), @@warning_count; "\
"UNLOCK TABLES; SELECT ROW_COUNT(), @@warning_count;" \
    "$DATABASE"

expect_output \
    "effective lock aliases are case-sensitive" \
    "0	0" \
    "LOCK TABLES t READ, u AS T WRITE; SELECT ROW_COUNT(), @@warning_count; UNLOCK TABLES;" \
    "$DATABASE"

expect_output \
    "temporary table lock is accepted and ignored for enforcement" \
    "0	0
1" \
    "CREATE TEMPORARY TABLE temp_t (id INT); "\
"LOCK TABLES temp_t READ; SELECT ROW_COUNT(), @@warning_count; "\
"INSERT INTO temp_t VALUES (1); SELECT GROUP_CONCAT(id ORDER BY id) FROM temp_t; "\
"UNLOCK TABLES;" \
    "$DATABASE"

expect_output \
    "lock tables commits active transaction before lock" \
    "1,2" \
    "START TRANSACTION; INSERT INTO t VALUES (2, 20); "\
"LOCK TABLES t READ; ROLLBACK; UNLOCK TABLES; "\
"SELECT GROUP_CONCAT(id ORDER BY id) FROM t;" \
    "$DATABASE"

expect_output \
    "start transaction releases table locks before new transaction" \
    "1,2" \
    "LOCK TABLES t READ; START TRANSACTION; INSERT INTO t VALUES (3, 30); "\
"ROLLBACK; SELECT GROUP_CONCAT(id ORDER BY id) FROM t;" \
    "$DATABASE"

expect_force_output \
    "failed lock tables commits active transaction" \
    "ERROR 1146 (42S02) at line 1: Table '${DATABASE}.missing_after_tx' doesn't exist
1,2,4" \
    "START TRANSACTION; INSERT INTO t VALUES (4, 40); "\
"LOCK TABLES missing_after_tx READ; ROLLBACK; "\
"SELECT GROUP_CONCAT(id ORDER BY id) FROM t;" \
    "$DATABASE"

expect_output \
    "lock tables replaces previous lock set" \
    "7	0	0" \
    "LOCK TABLES t READ; LOCK TABLES u WRITE; INSERT INTO u VALUES (7); "\
"UNLOCK TABLES; SELECT GROUP_CONCAT(id ORDER BY id), ROW_COUNT(), @@warning_count FROM u;" \
    "$DATABASE"

expect_force_output \
    "failed lock replacement releases previous lock set" \
    "ERROR 1146 (42S02) at line 1: Table '${DATABASE}.missing_after_lock' doesn't exist
5" \
    "LOCK TABLES t READ; LOCK TABLES missing_after_lock READ; "\
"INSERT INTO t VALUES (5, 50); UNLOCK TABLES; "\
"SELECT GROUP_CONCAT(id ORDER BY id) FROM t WHERE id = 5;" \
    "$DATABASE"

expect_error \
    "lock tables requires selected schema for unqualified target" \
    1046 \
    "3D000" \
    "No database selected" \
    "LOCK TABLES t READ"

expect_error \
    "lock tables rejects unknown schema" \
    1049 \
    "42000" \
    "Unknown database 'missingdb'" \
    "LOCK TABLES missingdb.t READ" \
    "$DATABASE"

expect_error \
    "lock tables rejects unknown table" \
    1146 \
    "42S02" \
    "doesn't exist" \
    "LOCK TABLES missing READ" \
    "$DATABASE"

expect_error \
    "lock tables rejects duplicate effective names" \
    1066 \
    "42000" \
    "Not unique table/alias: 't'" \
    "LOCK TABLES t READ, t WRITE" \
    "$DATABASE"

expect_error \
    "lock tables rejects unsupported low priority write syntax" \
    1064 \
    "42000" \
    "near 'LOW_PRIORITY WRITE'" \
    "LOCK TABLES t LOW_PRIORITY WRITE" \
    "$DATABASE"

expect_error \
    "unlock requires table keyword" \
    1064 \
    "42000" \
    "near ''" \
    "UNLOCK" \
    "$DATABASE"

cleanup

printf '%s\n' "mysql_baseline_lock_tables_lifecycle_expectations: ok"
