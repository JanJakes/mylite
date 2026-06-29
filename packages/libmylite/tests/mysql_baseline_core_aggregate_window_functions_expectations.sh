#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_core_aggregate_window_expectations_$$"

fail() {
    printf '%s\n' "mysql_baseline_core_aggregate_window_functions_expectations: $1" >&2
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
"CREATE TABLE posts(id INT, author_id INT, created_at INT, score INT, title VARCHAR(20)); "\
"INSERT INTO posts VALUES "\
"(1,10,100,5,'a'),"\
"(2,10,200,7,'b'),"\
"(3,10,200,NULL,'c'),"\
"(4,20,NULL,4,'d'),"\
"(5,20,50,9,'e'),"\
"(6,NULL,10,2,'f'),"\
"(7,NULL,20,NULL,'g');" \
    >/dev/null

expect_output \
    "source-free core aggregate windows" \
    "1	1	2.0000" \
    "SELECT COUNT(*) OVER () AS c, SUM(1) OVER () AS s, AVG(2) OVER () AS a;" \
    "$DATABASE"

expect_output \
    "partitioned core aggregate windows" \
    "6	NULL	2	1	2	2.0000	2	2
7	NULL	2	1	2	2.0000	2	2
1	10	3	2	12	6.0000	5	7
2	10	3	2	12	6.0000	5	7
3	10	3	2	12	6.0000	5	7
4	20	2	2	13	6.5000	4	9
5	20	2	2	13	6.5000	4	9" \
    "SELECT id, author_id, COUNT(*) OVER (PARTITION BY author_id) AS c, "\
"COUNT(score) OVER (PARTITION BY author_id) AS cs, "\
"SUM(score) OVER (PARTITION BY author_id) AS s, "\
"AVG(score) OVER (PARTITION BY author_id) AS a, "\
"MIN(score) OVER (PARTITION BY author_id) AS mi, "\
"MAX(score) OVER (PARTITION BY author_id) AS ma "\
"FROM posts ORDER BY author_id, id;" \
    "$DATABASE"

expect_output \
    "ordered frame aggregate windows" \
    "6	2	2.0000
7	2	2.0000
1	5	5.0000
2	12	6.0000
3	12	7.0000
4	4	4.0000
5	13	6.5000" \
    "SELECT id, "\
"SUM(score) OVER (PARTITION BY author_id ORDER BY created_at "\
"ROWS BETWEEN UNBOUNDED PRECEDING AND CURRENT ROW) AS running, "\
"AVG(score) OVER (PARTITION BY author_id ORDER BY created_at "\
"ROWS BETWEEN 1 PRECEDING AND CURRENT ROW) AS moving_avg "\
"FROM posts ORDER BY author_id, created_at, id;" \
    "$DATABASE"

expect_output \
    "named aggregate window" \
    "6	1	2
7	2	2
1	1	5
2	2	12
3	3	12
4	1	4
5	2	13" \
    "SELECT id, COUNT(*) OVER w AS c, SUM(score) OVER w AS s FROM posts "\
"WINDOW w AS (PARTITION BY author_id ORDER BY created_at "\
"ROWS BETWEEN UNBOUNDED PRECEDING AND CURRENT ROW) "\
"ORDER BY author_id, created_at, id;" \
    "$DATABASE"

expect_output \
    "count literal aggregate windows" \
    "1	0	3
2	0	3
3	0	3
4	0	2
5	0	2
6	0	2
7	0	2" \
    "SELECT id, COUNT(NULL) OVER () AS cn, "\
"COUNT(1) OVER (PARTITION BY author_id) AS co "\
"FROM posts ORDER BY id;" \
    "$DATABASE"

expect_error \
    "distinct core aggregate window" \
    1235 \
    42000 \
    "This version of MySQL doesn't yet support '<window function>(DISTINCT ..)'" \
    "SELECT SUM(DISTINCT score) OVER () FROM posts;" \
    "$DATABASE"

expect_error \
    "group concat aggregate window" \
    1235 \
    42000 \
    "This version of MySQL doesn't yet support 'group_concat as window function'" \
    "SELECT GROUP_CONCAT(title) OVER () FROM posts;" \
    "$DATABASE"

printf '%s\n' "mysql_baseline_core_aggregate_window_functions_expectations: ok"
