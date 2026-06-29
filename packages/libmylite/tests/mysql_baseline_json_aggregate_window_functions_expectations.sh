#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_json_aggregate_window_expectations_$$"

fail() {
    printf '%s\n' "mysql_baseline_json_aggregate_window_functions_expectations: $1" >&2
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
"CREATE TABLE t (g INT, id INT, k VARCHAR(20), s VARCHAR(20), j JSON, b TINYINT); "\
"INSERT INTO t VALUES "\
"(1,1,'a','alpha',JSON_OBJECT('x',1),1),"\
"(1,2,'b','beta',JSON_ARRAY(2),0),"\
"(1,3,'a',NULL,NULL,NULL),"\
"(2,4,'c','carrot',JSON_OBJECT('x',4),1),"\
"(3,5,'n',NULL,NULL,NULL);" \
    >/dev/null

expect_output \
    "source-free JSON aggregate windows" \
    "[null]	{\"a\": 1}" \
    "SELECT JSON_ARRAYAGG(NULL) OVER () AS ja, JSON_OBJECTAGG('a', 1) OVER () AS jo;" \
    "$DATABASE"

expect_output \
    "partitioned JSON aggregate windows" \
    "1	1	[\"alpha\", \"beta\", null]	{\"a\": null, \"b\": \"beta\"}
2	1	[\"alpha\", \"beta\", null]	{\"a\": null, \"b\": \"beta\"}
3	1	[\"alpha\", \"beta\", null]	{\"a\": null, \"b\": \"beta\"}
4	2	[\"carrot\"]	{\"c\": \"carrot\"}
5	3	[null]	{\"n\": null}" \
    "SELECT id, g, JSON_ARRAYAGG(s) OVER (PARTITION BY g) AS ja, "\
"JSON_OBJECTAGG(k, s) OVER (PARTITION BY g) AS jo FROM t ORDER BY g, id;" \
    "$DATABASE"

expect_output \
    "running JSON aggregate windows" \
    "1	[\"alpha\"]	{\"a\": \"alpha\"}
2	[\"alpha\", \"beta\"]	{\"a\": \"alpha\", \"b\": \"beta\"}
3	[\"alpha\", \"beta\", null]	{\"a\": null, \"b\": \"beta\"}
4	[\"carrot\"]	{\"c\": \"carrot\"}
5	[null]	{\"n\": null}" \
    "SELECT id, "\
"JSON_ARRAYAGG(s) OVER (PARTITION BY g ORDER BY id "\
"ROWS BETWEEN UNBOUNDED PRECEDING AND CURRENT ROW) AS ja, "\
"JSON_OBJECTAGG(k, s) OVER (PARTITION BY g ORDER BY id "\
"ROWS BETWEEN UNBOUNDED PRECEDING AND CURRENT ROW) AS jo "\
"FROM t ORDER BY g, id;" \
    "$DATABASE"

expect_output \
    "moving JSON aggregate windows" \
    "1	[\"alpha\"]	{\"a\": \"alpha\"}
2	[\"alpha\", \"beta\"]	{\"a\": \"alpha\", \"b\": \"beta\"}
3	[\"beta\", null]	{\"a\": null, \"b\": \"beta\"}
4	[\"carrot\"]	{\"c\": \"carrot\"}
5	[null]	{\"n\": null}" \
    "SELECT id, "\
"JSON_ARRAYAGG(s) OVER (PARTITION BY g ORDER BY id "\
"ROWS BETWEEN 1 PRECEDING AND CURRENT ROW) AS ja, "\
"JSON_OBJECTAGG(k, s) OVER (PARTITION BY g ORDER BY id "\
"ROWS BETWEEN 1 PRECEDING AND CURRENT ROW) AS jo "\
"FROM t ORDER BY g, id;" \
    "$DATABASE"

expect_output \
    "empty frame JSON aggregate windows" \
    "1	NULL	NULL
2	[\"alpha\"]	{\"a\": \"alpha\"}
3	[\"beta\"]	{\"b\": \"beta\"}
4	NULL	NULL
5	NULL	NULL" \
    "SELECT id, "\
"JSON_ARRAYAGG(s) OVER (PARTITION BY g ORDER BY id "\
"ROWS BETWEEN 1 PRECEDING AND 1 PRECEDING) AS ja, "\
"JSON_OBJECTAGG(k, s) OVER (PARTITION BY g ORDER BY id "\
"ROWS BETWEEN 1 PRECEDING AND 1 PRECEDING) AS jo "\
"FROM t ORDER BY g, id;" \
    "$DATABASE"

expect_output \
    "JSON descriptor aggregate window values" \
    "1	[{\"x\": 1}]
2	[{\"x\": 1}, [2]]
3	[{\"x\": 1}, [2], null]
4	[{\"x\": 4}]
5	[null]" \
    "SELECT id, JSON_ARRAYAGG(j) OVER (PARTITION BY g ORDER BY id "\
"ROWS BETWEEN UNBOUNDED PRECEDING AND CURRENT ROW) AS jj FROM t ORDER BY g, id;" \
    "$DATABASE"

run_mysql \
    "CREATE VIEW v_json_window AS SELECT JSON_ARRAYAGG(s) OVER () AS ja, "\
"JSON_OBJECTAGG(k, s) OVER () AS jo FROM t;" \
    "$DATABASE" >/dev/null
expect_output \
    "JSON aggregate window metadata" \
    "ja	json	YES	<NULL>
jo	json	YES	<NULL>" \
    "SELECT COLUMN_NAME, COLUMN_TYPE, IS_NULLABLE, COALESCE(COLUMN_DEFAULT, '<NULL>') "\
"FROM INFORMATION_SCHEMA.COLUMNS WHERE TABLE_SCHEMA = DATABASE() "\
"AND TABLE_NAME = 'v_json_window' ORDER BY ORDINAL_POSITION;" \
    "$DATABASE"

expect_error \
    "GROUP_CONCAT window unsupported" \
    1235 \
    42000 \
    "group_concat as window function" \
    "SELECT GROUP_CONCAT(s) OVER () FROM t;" \
    "$DATABASE"

expect_error \
    "NULL JSON object aggregate window key" \
    3158 \
    22032 \
    "JSON documents may not contain NULL member names." \
    "SELECT JSON_OBJECTAGG(NULL, s) OVER () FROM t;" \
    "$DATABASE"

printf '%s\n' "mysql_baseline_json_aggregate_window_functions_expectations: ok"
