#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
MYSQL_BIN="${MYLITE_MYSQL_BIN:-}"
MYSQL_SOCKET="${MYLITE_MYSQL_SOCKET:-}"
DATABASE="mylite_values_function_non_odku_expectations_$$"

fail() {
    printf '%s\n' "mysql_baseline_values_function_non_odku_expectations: $1" >&2
    exit 1
}

run_mysql() {
    sql=$1
    shift
    if [ -n "$MYSQL_BIN" ]; then
        if [ -n "$MYSQL_SOCKET" ]; then
            printf '%s\n' "$sql" \
                | "$MYSQL_BIN" --protocol=SOCKET --socket="$MYSQL_SOCKET" -uroot \
                    --batch --raw --skip-column-names "$@"
        else
            printf '%s\n' "$sql" \
                | "$MYSQL_BIN" --protocol=TCP -h127.0.0.1 -uroot \
                    --batch --raw --skip-column-names "$@"
        fi
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
"SET SESSION sql_mode = 'STRICT_TRANS_TABLES,NO_ENGINE_SUBSTITUTION'; "\
"CREATE TABLE t(id INT PRIMARY KEY, v INT, s VARCHAR(10), b VARBINARY(10), d DATE); "\
"INSERT INTO t VALUES(1, 11, 'aa', X'61', '2020-01-01'), (2, 22, 'bb', X'62', NULL);" \
    >/dev/null

expect_output \
    "values outside odku inline warning count" \
    "NULL	1
NULL	1
1	0" \
    "SELECT VALUES(v), @@warning_count FROM t ORDER BY id; "\
"SELECT @@warning_count, @@error_count;" \
    "$DATABASE"

expect_output \
    "values outside odku nulls and hidden warnings" \
    "NULL	NULL	NULL	NULL	NULL
NULL	NULL	NULL	NULL	NULL
5	0
0" \
    "SELECT VALUES(v), VALUES(s), VALUES(b), VALUES(t.v), VALUES(${DATABASE}.t.v) "\
"FROM t ORDER BY id; "\
"SELECT @@warning_count, @@error_count; "\
"SHOW COUNT(*) WARNINGS; "\
"SHOW WARNINGS;" \
    "$DATABASE"

expect_output \
    "values outside odku metadata" \
    "vv	varbinary(0)	YES	NULL
ss	varbinary(0)	YES	NULL
bb	varbinary(0)	YES	NULL
dd	varbinary(0)	YES	NULL
0	0
0" \
    "CREATE TABLE out_t AS "\
"SELECT VALUES(v) AS vv, VALUES(s) AS ss, VALUES(b) AS bb, VALUES(d) AS dd FROM t; "\
"SELECT COLUMN_NAME, COLUMN_TYPE, IS_NULLABLE, IFNULL(COLUMN_DEFAULT, 'NULL') "\
"FROM INFORMATION_SCHEMA.COLUMNS "\
"WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = 'out_t' ORDER BY ORDINAL_POSITION; "\
"SELECT @@warning_count, @@error_count; "\
"SHOW COUNT(*) WARNINGS; "\
"SHOW WARNINGS;" \
    "$DATABASE"

expect_error \
    "values no source unknown column" \
    1054 \
    42S22 \
    "Unknown column 'v' in 'field list'" \
    "SELECT VALUES(v);" \
    "$DATABASE"

expect_error \
    "values unknown source column" \
    1054 \
    42S22 \
    "Unknown column 'nope' in 'field list'" \
    "SELECT VALUES(nope) FROM t;" \
    "$DATABASE"

expect_error \
    "values empty argument list syntax" \
    1064 \
    42000 \
    "You have an error in your SQL syntax" \
    "SELECT VALUES() FROM t;" \
    "$DATABASE"

expect_error \
    "values multiple arguments syntax" \
    1064 \
    42000 \
    "You have an error in your SQL syntax" \
    "SELECT VALUES(v, id) FROM t;" \
    "$DATABASE"

expect_error \
    "values literal argument syntax" \
    1064 \
    42000 \
    "You have an error in your SQL syntax" \
    "SELECT VALUES(1) FROM t;" \
    "$DATABASE"

printf '%s\n' "mysql_baseline_values_function_non_odku_expectations: ok"
