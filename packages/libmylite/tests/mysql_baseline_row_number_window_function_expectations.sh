#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_row_number_window_function_expectations_$$"

fail() {
    printf '%s\n' "mysql_baseline_row_number_window_function_expectations: $1" >&2
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
"CREATE TABLE posts("\
"id INT, author_id INT, created_at INT, category VARCHAR(20), title VARCHAR(20)"\
"); "\
"INSERT INTO posts VALUES "\
"(1,10,100,'alpha','a'),"\
"(2,10,200,'alpha','b'),"\
"(3,10,200,'Alpha','c'),"\
"(4,20,NULL,NULL,'d'),"\
"(5,20,50,'beta','e'),"\
"(6,NULL,10,NULL,'f'),"\
"(7,NULL,20,'beta','g');" \
    >/dev/null

expect_output \
    "no-source row number" \
    "1
0" \
    "SELECT ROW_NUMBER() OVER () AS rn; SELECT @@warning_count;" \
    "$DATABASE"

expect_output \
    "table row number over empty window" \
    "1	1
2	2
3	3
4	4
5	5
6	6
7	7" \
    "SELECT id, ROW_NUMBER() OVER () AS rn FROM posts ORDER BY id;" \
    "$DATABASE"

expect_output \
    "partition and descending order" \
    "7	NULL	1
6	NULL	2
2	10	1
3	10	2
1	10	3
5	20	1
4	20	2" \
    "SELECT id, author_id, ROW_NUMBER() OVER "\
"(PARTITION BY author_id ORDER BY created_at DESC) AS rn "\
"FROM posts ORDER BY author_id, rn, id;" \
    "$DATABASE"

expect_output \
    "ascending null order" \
    "4	1
6	2
7	3
5	4
1	5
2	6
3	7" \
    "SELECT id, ROW_NUMBER() OVER (ORDER BY created_at ASC) AS rn "\
"FROM posts ORDER BY rn;" \
    "$DATABASE"

expect_output \
    "descending null order" \
    "2	1
3	2
1	3
5	4
7	5
6	6
4	7" \
    "SELECT id, ROW_NUMBER() OVER (ORDER BY created_at DESC) AS rn "\
"FROM posts ORDER BY rn;" \
    "$DATABASE"

expect_output \
    "string partition and qualified order" \
    "4	NULL	1
6	NULL	2
1	alpha	1
2	alpha	2
3	Alpha	3
5	beta	1
7	beta	2" \
    "SELECT id, category, ROW_NUMBER() OVER "\
"(PARTITION BY category ORDER BY posts.id) AS rn "\
"FROM posts ORDER BY category, rn;" \
    "$DATABASE"

expect_output \
    "where and outer limit" \
    "7	1
6	2
2	1
3	2" \
    "SELECT id, ROW_NUMBER() OVER "\
"(PARTITION BY author_id ORDER BY created_at DESC) AS rn "\
"FROM posts WHERE id >= 2 ORDER BY author_id, rn, id LIMIT 4;" \
    "$DATABASE"

expect_error \
    "row_number requires over" \
    1064 \
    42000 \
    "near '' at line 1" \
    "SELECT ROW_NUMBER();" \
    "$DATABASE"

expect_error \
    "row_number rejects arguments" \
    1064 \
    42000 \
    "near '1) OVER ()' at line 1" \
    "SELECT ROW_NUMBER(1) OVER ();" \
    "$DATABASE"

expect_error \
    "where context rejected" \
    3593 \
    HY000 \
    "You cannot use the window function 'row_number' in this context." \
    "SELECT id FROM posts WHERE ROW_NUMBER() OVER () = 1;" \
    "$DATABASE"

expect_error \
    "legacy ordinal order rejected" \
    3592 \
    HY000 \
    "ORDER BY or PARTITION BY uses legacy position indication" \
    "SELECT id, ROW_NUMBER() OVER (ORDER BY 1) AS rn FROM posts;" \
    "$DATABASE"

printf '%s\n' "mysql_baseline_row_number_window_function_expectations: ok"
