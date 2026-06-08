#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
MYSQL_BIN="${MYLITE_MYSQL_BIN:-}"
MYSQL_SOCKET="${MYLITE_MYSQL_SOCKET:-}"
DATABASE="mylite_select_order_by_ordinals_expectations_$$"
DEFAULT_SQL_MODE="ONLY_FULL_GROUP_BY,STRICT_TRANS_TABLES,NO_ZERO_IN_DATE,NO_ZERO_DATE,ERROR_FOR_DIVISION_BY_ZERO,NO_ENGINE_SUBSTITUTION"

fail() {
    printf '%s\n' "mysql_baseline_select_order_by_ordinals_expectations: $1" >&2
    exit 1
}

run_mysql() {
    sql=$1
    shift
    sql="SET SESSION sql_mode = '${DEFAULT_SQL_MODE}';
${sql}"
    if [ -n "$MYSQL_BIN" ]; then
        if [ -n "$MYSQL_SOCKET" ]; then
            printf '%s\n' "$sql" \
                | "$MYSQL_BIN" --protocol=SOCKET --socket="$MYSQL_SOCKET" -uroot \
                    --batch --raw --skip-column-names --default-character-set=utf8mb4 "$@"
        else
            printf '%s\n' "$sql" \
                | "$MYSQL_BIN" --protocol=TCP -h127.0.0.1 -uroot \
                    --batch --raw --skip-column-names --default-character-set=utf8mb4 "$@"
        fi
    else
        printf '%s\n' "$sql" \
            | docker exec -i "$MYSQL_CONTAINER" mysql --protocol=TCP -h127.0.0.1 -uroot \
                --batch --raw --skip-column-names --default-character-set=utf8mb4 "$@"
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

trap cleanup EXIT HUP INT TERM

version=$(run_mysql 'SELECT VERSION();')
case "$version" in
    8.4.9*) ;;
    *) fail "expected MySQL 8.4.9 runtime, got [$version]" ;;
esac

run_mysql "DROP DATABASE IF EXISTS ${DATABASE}; CREATE DATABASE ${DATABASE};" >/dev/null

run_mysql \
    "CREATE TABLE t (id INT, a INT, title VARCHAR(20));
     INSERT INTO t VALUES
       (1, 2, 'foo old'),
       (2, 1, 'bar'),
       (3, NULL, 'food new'),
       (4, 2, 'qux');" \
    "$DATABASE" >/dev/null

expect_output \
    "single descriptor ordinal" \
    "4
3
2
1" \
    "SELECT id FROM t ORDER BY 1 DESC;" \
    "$DATABASE"

expect_output \
    "multiple descriptor ordinals" \
    "3	NULL
2	1
4	2
1	2" \
    "SELECT id, a FROM t ORDER BY 2, 1 DESC;" \
    "$DATABASE"

expect_output \
    "distinct ordinal" \
    "2
1
NULL" \
    "SELECT DISTINCT a FROM t ORDER BY 1 DESC;" \
    "$DATABASE"

expect_output \
    "row scalar ordinal" \
    "4-qux
3-food new
2-bar
1-foo old" \
    "SELECT CONCAT(id, '-', title) FROM t ORDER BY 1 DESC;" \
    "$DATABASE"

expect_output \
    "valid ordinal warning count" \
    "0" \
    "SELECT id FROM t ORDER BY 1 DESC LIMIT 0;
     SELECT @@warning_count;" \
    "$DATABASE"

expect_error \
    "zero ordinal" \
    1054 \
    42S22 \
    "Unknown column '0' in 'order clause'" \
    "SELECT id FROM t ORDER BY 0;" \
    "$DATABASE"

expect_error \
    "descriptor ordinal out of range" \
    1054 \
    42S22 \
    "Unknown column '2' in 'order clause'" \
    "SELECT id FROM t ORDER BY 2;" \
    "$DATABASE"

expect_error \
    "row scalar ordinal out of range" \
    1054 \
    42S22 \
    "Unknown column '2' in 'order clause'" \
    "SELECT CONCAT(id, '-', title) FROM t ORDER BY 2;" \
    "$DATABASE"

printf '%s\n' "mysql_baseline_select_order_by_ordinals_expectations: ok"
