#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_bitwise_aggregate_window_expectations_$$"

fail() {
    printf '%s\n' "mysql_baseline_bitwise_aggregate_window_functions_expectations: $1" >&2
    exit 1
}

run_mysql() {
    sql=$1
    shift
    printf '%s\n' "$sql" \
        | docker exec -i "$MYSQL_CONTAINER" mysql --protocol=TCP -h127.0.0.1 -uroot \
            --batch --raw --skip-column-names --default-character-set=utf8mb4 "$@"
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
    "CREATE DATABASE ${DATABASE} CHARACTER SET utf8mb4 COLLATE utf8mb4_0900_ai_ci; "\
"USE ${DATABASE}; SET NAMES utf8mb4; SET SESSION sql_mode = 'NO_ENGINE_SUBSTITUTION'; "\
"CREATE TABLE bits(id INT, group_id INT, value BIGINT, label VARCHAR(10)); "\
"INSERT INTO bits VALUES "\
"(1,10,7,'a'),"\
"(2,10,3,'b'),"\
"(3,10,NULL,'c'),"\
"(4,20,12,'d'),"\
"(5,20,10,'e'),"\
"(6,NULL,NULL,'f');" \
    >/dev/null

expect_output \
    "source-free bitwise aggregate windows" \
    "1	0	3" \
    "SELECT BIT_AND(1) OVER () AS ba, BIT_OR(NULL) OVER () AS bo, "\
"BIT_XOR(3) OVER () AS bx;" \
    "$DATABASE"

expect_output \
    "partitioned bitwise aggregate windows" \
    "6	NULL	18446744073709551615	0	0
1	10	3	7	4
2	10	3	7	4
3	10	3	7	4
4	20	8	14	6
5	20	8	14	6" \
    "SELECT id, group_id, BIT_AND(value) OVER (PARTITION BY group_id) AS ba, "\
"BIT_OR(value) OVER (PARTITION BY group_id) AS bo, "\
"BIT_XOR(value) OVER (PARTITION BY group_id) AS bx "\
"FROM bits ORDER BY group_id, id;" \
    "$DATABASE"

expect_output \
    "ordered moving bitwise aggregate windows" \
    "1	7	7	7
2	3	7	4
3	3	3	3
4	12	12	12
5	8	14	6
6	10	10	10" \
    "SELECT id, BIT_AND(value) OVER (ORDER BY id ROWS BETWEEN 1 PRECEDING AND CURRENT ROW) AS ba, "\
"BIT_OR(value) OVER (ORDER BY id ROWS BETWEEN 1 PRECEDING AND CURRENT ROW) AS bo, "\
"BIT_XOR(value) OVER (ORDER BY id ROWS BETWEEN 1 PRECEDING AND CURRENT ROW) AS bx "\
"FROM bits ORDER BY id;" \
    "$DATABASE"

expect_output \
    "empty frame bitwise aggregate windows" \
    "1	18446744073709551615	0	0
2	7	7	7
3	3	3	3
4	18446744073709551615	0	0
5	12	12	12
6	10	10	10" \
    "SELECT id, "\
"BIT_AND(value) OVER (ORDER BY id ROWS BETWEEN 1 PRECEDING AND 1 PRECEDING) AS ba, "\
"BIT_OR(value) OVER (ORDER BY id ROWS BETWEEN 1 PRECEDING AND 1 PRECEDING) AS bo, "\
"BIT_XOR(value) OVER (ORDER BY id ROWS BETWEEN 1 PRECEDING AND 1 PRECEDING) AS bx "\
"FROM bits ORDER BY id;" \
    "$DATABASE"

expect_output \
    "named bitwise aggregate window" \
    "1	7
2	4
3	4
4	8
5	2
6	2" \
    "SELECT id, BIT_XOR(value) OVER w AS bx FROM bits "\
"WINDOW w AS (ORDER BY id ROWS BETWEEN UNBOUNDED PRECEDING AND CURRENT ROW) "\
"ORDER BY id;" \
    "$DATABASE"

run_mysql \
    "CREATE VIEW v AS SELECT BIT_AND(value) OVER () AS ba, "\
"BIT_OR(value) OVER () AS bo, BIT_XOR(value) OVER () AS bx FROM bits;" \
    "$DATABASE" >/dev/null
expect_output \
    "bitwise aggregate window metadata" \
    "ba	bigint unsigned	NO	0
bo	bigint unsigned	NO	0
bx	bigint unsigned	NO	0" \
    "SELECT COLUMN_NAME, COLUMN_TYPE, IS_NULLABLE, COALESCE(COLUMN_DEFAULT, '<NULL>') "\
"FROM INFORMATION_SCHEMA.COLUMNS "\
"WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = 'v' ORDER BY ORDINAL_POSITION;" \
    "$DATABASE"

expect_error \
    "distinct bitwise aggregate window syntax" \
    1064 \
    42000 \
    "near 'DISTINCT value) OVER () FROM bits'" \
    "SELECT BIT_AND(DISTINCT value) OVER () FROM bits;" \
    "$DATABASE"

printf '%s\n' "mysql_baseline_bitwise_aggregate_window_functions_expectations: ok"
