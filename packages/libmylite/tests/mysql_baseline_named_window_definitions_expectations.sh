#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_named_window_definitions_expectations_$$"

fail() {
    printf '%s\n' "mysql_baseline_named_window_definitions_expectations: $1" >&2
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
"CREATE TABLE posts(id INT, author_id INT, created_at INT, title VARCHAR(20)); "\
"INSERT INTO posts VALUES "\
"(1,10,100,'a'),"\
"(2,10,200,'b'),"\
"(3,10,200,'c'),"\
"(4,20,NULL,'d'),"\
"(5,20,50,'e'),"\
"(6,NULL,10,'f'),"\
"(7,NULL,20,'g');" \
    >/dev/null

expect_output \
    "no-source empty named window" \
    "1" \
    "SELECT ROW_NUMBER() OVER w AS rn WINDOW w AS ();" \
    "$DATABASE"

expect_output \
    "direct named window" \
    "7	1
6	2
2	1
3	2
1	3
5	1
4	2" \
    "SELECT id, ROW_NUMBER() OVER w AS rn FROM posts "\
"WINDOW w AS (PARTITION BY author_id ORDER BY created_at DESC) "\
"ORDER BY author_id, created_at DESC, id;" \
    "$DATABASE"

expect_output \
    "extended named window" \
    "7	1
6	2
2	1
3	2
1	3
5	1
4	2" \
    "SELECT id, ROW_NUMBER() OVER (base ORDER BY created_at DESC) AS rn FROM posts "\
"WINDOW base AS (PARTITION BY author_id) ORDER BY author_id, created_at DESC, id;" \
    "$DATABASE"

expect_output \
    "forward referenced named window" \
    "7	1
6	2
2	1
3	2
1	3
5	1
4	2" \
    "SELECT id, ROW_NUMBER() OVER w1 AS rn FROM posts "\
"WINDOW w1 AS (w2), w2 AS (PARTITION BY author_id ORDER BY created_at DESC) "\
"ORDER BY author_id, created_at DESC, id;" \
    "$DATABASE"

expect_output \
    "case-insensitive named window" \
    "7	1
6	2
2	1
3	2
1	3
5	1
4	2" \
    "SELECT id, ROW_NUMBER() OVER w AS rn FROM posts "\
"WINDOW W AS (PARTITION BY author_id ORDER BY created_at DESC) "\
"ORDER BY author_id, created_at DESC, id;" \
    "$DATABASE"

expect_output \
    "named window frame" \
    "4	d
6	d
7	d
5	d
1	d
2	d
3	d" \
    "SELECT id, FIRST_VALUE(title) OVER w AS first_title FROM posts "\
"WINDOW w AS (ORDER BY created_at ROWS BETWEEN UNBOUNDED PRECEDING AND CURRENT ROW) "\
"ORDER BY created_at, id;" \
    "$DATABASE"

expect_error \
    "missing window reference" \
    3579 \
    HY000 \
    "Window name 'missing_window' is not defined." \
    "SELECT id, ROW_NUMBER() OVER missing_window AS rn FROM posts;" \
    "$DATABASE"

expect_error \
    "missing inherited window" \
    3579 \
    HY000 \
    "Window name 'missing_window' is not defined." \
    "SELECT id, ROW_NUMBER() OVER w AS rn FROM posts WINDOW w AS (missing_window);" \
    "$DATABASE"

expect_error \
    "duplicate window name" \
    3591 \
    HY000 \
    "Window 'w' is defined twice." \
    "SELECT id, ROW_NUMBER() OVER w AS rn FROM posts WINDOW w AS (), W AS ();" \
    "$DATABASE"

expect_error \
    "window dependency cycle" \
    3580 \
    HY000 \
    "There is a circularity in the window dependency graph." \
    "SELECT id, ROW_NUMBER() OVER w1 AS rn FROM posts WINDOW w1 AS (w2), w2 AS (w1);" \
    "$DATABASE"

expect_error \
    "unnamed duplicate order inheritance" \
    3583 \
    HY000 \
    "Window '<unnamed window>' cannot inherit 'w' since both contain an ORDER BY clause." \
    "SELECT id, ROW_NUMBER() OVER (w ORDER BY id) AS rn FROM posts "\
"WINDOW w AS (ORDER BY created_at);" \
    "$DATABASE"

expect_error \
    "named duplicate order inheritance" \
    3583 \
    HY000 \
    "Window 'w1' cannot inherit 'w2' since both contain an ORDER BY clause." \
    "SELECT id, ROW_NUMBER() OVER w1 AS rn FROM posts "\
"WINDOW w1 AS (w2 ORDER BY id), w2 AS (ORDER BY created_at);" \
    "$DATABASE"

printf '%s\n' "mysql_baseline_named_window_definitions_expectations: ok"
