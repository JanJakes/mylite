#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_window_rank_navigation_expectations_$$"

fail() {
    printf '%s\n' "mysql_baseline_window_rank_navigation_expectations: $1" >&2
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
    "no-source rank and distribution functions" \
    "1	1	0	1	1
0" \
    "SELECT RANK() OVER (), DENSE_RANK() OVER (), PERCENT_RANK() OVER (), "\
"CUME_DIST() OVER (), NTILE(1) OVER (); SELECT @@warning_count;" \
    "$DATABASE"

expect_output \
    "rank distribution and ntile over nullable order" \
    "4	NULL	1	1	0	0.14285714285714285	1
6	10	2	2	0.16666666666666666	0.2857142857142857	1
7	20	3	3	0.3333333333333333	0.42857142857142855	1
5	50	4	4	0.5	0.5714285714285714	2
1	100	5	5	0.6666666666666666	0.7142857142857143	2
2	200	6	6	0.8333333333333334	1	3
3	200	6	6	0.8333333333333334	1	3" \
    "SELECT id, created_at, RANK() OVER (ORDER BY created_at), "\
"DENSE_RANK() OVER (ORDER BY created_at), PERCENT_RANK() OVER (ORDER BY created_at), "\
"CUME_DIST() OVER (ORDER BY created_at), NTILE(3) OVER (ORDER BY created_at) "\
"FROM posts ORDER BY created_at, id;" \
    "$DATABASE"

expect_output \
    "partitioned rank dense rank and ntile" \
    "7	NULL	20	1	1	1
6	NULL	10	2	2	2
2	10	200	1	1	1
3	10	200	1	1	1
1	10	100	3	2	2
5	20	50	1	1	1
4	20	NULL	2	2	2" \
    "SELECT id, author_id, created_at, "\
"RANK() OVER (PARTITION BY author_id ORDER BY created_at DESC), "\
"DENSE_RANK() OVER (PARTITION BY author_id ORDER BY created_at DESC), "\
"NTILE(2) OVER (PARTITION BY author_id ORDER BY created_at DESC) "\
"FROM posts ORDER BY author_id, created_at DESC, id;" \
    "$DATABASE"

expect_output \
    "navigation and frame-value functions" \
    "1	a	NULL	x	b	c	a	a	NULL
2	b	a	x	c	d	a	b	b
3	c	b	a	d	e	a	c	b
4	d	c	b	e	f	a	d	b
5	e	d	c	f	g	a	e	b
6	f	e	d	g	z	a	f	b
7	g	f	e	NULL	z	a	g	b" \
    "SELECT id, title, LAG(title) OVER (ORDER BY id), "\
"LAG(title,2,'x') OVER (ORDER BY id), LEAD(title) OVER (ORDER BY id), "\
"LEAD(title,2,'z') OVER (ORDER BY id), FIRST_VALUE(title) OVER (ORDER BY id), "\
"LAST_VALUE(title) OVER (ORDER BY id), NTH_VALUE(title,2) OVER (ORDER BY id) "\
"FROM posts ORDER BY id;" \
    "$DATABASE"

expect_output \
    "partitioned frame-value functions" \
    "6	NULL	f	f	f	NULL
7	NULL	g	f	g	g
1	10	a	a	a	NULL
2	10	b	a	b	b
3	10	c	a	c	b
4	20	d	d	d	NULL
5	20	e	d	e	e" \
    "SELECT id, author_id, title, "\
"FIRST_VALUE(title) OVER (PARTITION BY author_id ORDER BY id), "\
"LAST_VALUE(title) OVER (PARTITION BY author_id ORDER BY id), "\
"NTH_VALUE(title,2) OVER (PARTITION BY author_id ORDER BY id) "\
"FROM posts ORDER BY author_id, id;" \
    "$DATABASE"

expect_output \
    "zero offset navigation" \
    "1	a	a
2	b	b
3	c	c
4	d	d
5	e	e
6	f	f
7	g	g" \
    "SELECT id, LAG(title,0,'x') OVER (ORDER BY id), "\
"LEAD(title,0,'z') OVER (ORDER BY id) FROM posts ORDER BY id;" \
    "$DATABASE"

expect_error \
    "ntile rejects zero bucket count" \
    1210 \
    HY000 \
    "Incorrect arguments to ntile" \
    "SELECT NTILE(0) OVER ();" \
    "$DATABASE"

expect_error \
    "ntile rejects null syntax" \
    1064 \
    42000 \
    "near 'NULL) OVER ()' at line 1" \
    "SELECT NTILE(NULL) OVER ();" \
    "$DATABASE"

expect_error \
    "nth_value rejects zero index" \
    1210 \
    HY000 \
    "Incorrect arguments to nth_value" \
    "SELECT NTH_VALUE(title,0) OVER (ORDER BY id) FROM posts;" \
    "$DATABASE"

expect_error \
    "nth_value rejects negative index" \
    1210 \
    HY000 \
    "Incorrect arguments to nth_value" \
    "SELECT NTH_VALUE(title,-1) OVER (ORDER BY id) FROM posts;" \
    "$DATABASE"

expect_error \
    "lag rejects null offset syntax" \
    1064 \
    42000 \
    "near 'NULL) OVER (ORDER BY id) FROM posts' at line 1" \
    "SELECT LAG(title,NULL) OVER (ORDER BY id) FROM posts;" \
    "$DATABASE"

expect_error \
    "lead rejects negative offset syntax" \
    1064 \
    42000 \
    "near '-1) OVER (ORDER BY id) FROM posts' at line 1" \
    "SELECT LEAD(title,-1) OVER (ORDER BY id) FROM posts;" \
    "$DATABASE"

expect_error \
    "rank rejects arguments" \
    1064 \
    42000 \
    "near '1) OVER ()' at line 1" \
    "SELECT RANK(1) OVER ();" \
    "$DATABASE"

expect_error \
    "where context rejected" \
    3593 \
    HY000 \
    "You cannot use the window function 'rank' in this context." \
    "SELECT id FROM posts WHERE RANK() OVER () = 1;" \
    "$DATABASE"

printf '%s\n' "mysql_baseline_window_rank_navigation_expectations: ok"
