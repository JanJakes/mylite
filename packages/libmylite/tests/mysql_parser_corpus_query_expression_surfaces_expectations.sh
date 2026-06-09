#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
MYSQL_BIN="${MYLITE_MYSQL_BIN:-mysql}"
MYSQL_SOCKET="${MYLITE_MYSQL_SOCKET:-}"
DATABASE="mylite_parser_query_expr_$$"

fail() {
    printf '%s\n' "mysql_parser_corpus_query_expression_surfaces_expectations: $1" >&2
    exit 1
}

run_mysql() {
    sql=$1
    shift
    if [ -n "$MYSQL_SOCKET" ]; then
        printf '%s\n' "$sql" \
            | "$MYSQL_BIN" --protocol=SOCKET --socket="$MYSQL_SOCKET" -uroot \
                --batch --raw --skip-column-names "$@"
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
    output=$(printf '%s\n' "$output" | tr '\t' '|')
    if [ "$output" != "$expected" ]; then
        fail "$label: expected [$expected], got [$output]"
    fi
}

expect_success() {
    label=$1
    sql=$2
    shift 2

    if ! run_mysql "$sql" "$@" >/dev/null; then
        fail "$label: command failed"
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
run_mysql "CREATE DATABASE ${DATABASE}; USE ${DATABASE}; "\
"CREATE TABLE t1 (id INT, v INT, shared INT); "\
"CREATE TABLE t2 (id INT, w INT, shared INT); "\
"INSERT INTO t1 VALUES (1, 10, 100), (2, 20, 200), (NULL, 50, 400); "\
"INSERT INTO t2 VALUES (1, 30, 100), (3, 40, 300), (NULL, 60, 400);" >/dev/null

expect_output \
    "top-level parenthesized select" \
    "1" \
    "USE ${DATABASE}; (SELECT 1 AS x);"

expect_output \
    "parenthesized union with outer order and limit" \
    "2" \
    "USE ${DATABASE}; (SELECT 1 AS x UNION SELECT 2) ORDER BY x DESC LIMIT 1;"

expect_output \
    "values query block in set operation" \
    "1
2" \
    "USE ${DATABASE}; VALUES ROW(1), ROW(2) UNION SELECT 2 ORDER BY column_0;"

expect_output \
    "derived values query block" \
    "1|10
2|20" \
    "USE ${DATABASE}; SELECT * FROM (VALUES ROW(1, 10), ROW(2, 20)) AS dt(a,b) "\
"ORDER BY a;"

expect_success \
    "compound create view body" \
    "USE ${DATABASE}; CREATE VIEW v_union AS SELECT id FROM t1 UNION SELECT id FROM t2; "\
"DROP VIEW v_union;"

expect_success \
    "table create view body" \
    "USE ${DATABASE}; CREATE VIEW v_table AS TABLE t1; DROP VIEW v_table;"

expect_success \
    "values create view body" \
    "USE ${DATABASE}; CREATE VIEW v_values AS VALUES ROW(1), ROW(2); DROP VIEW v_values;"

expect_success \
    "values alter view body" \
    "USE ${DATABASE}; CREATE VIEW v_alter AS SELECT id FROM t1; "\
"ALTER VIEW v_alter AS VALUES ROW(1); DROP VIEW v_alter;"

expect_output \
    "join using coalesces join column in wildcard output" \
    "100|1|10|1|30
400|NULL|50|NULL|60" \
    "USE ${DATABASE}; SELECT * FROM t1 JOIN t2 USING (shared) ORDER BY shared;"

expect_output \
    "natural join coalesces common columns" \
    "1|100|10|30" \
    "USE ${DATABASE}; SELECT * FROM t1 NATURAL JOIN t2 ORDER BY shared;"

expect_output \
    "respect nulls is accepted on value window functions" \
    "NULL
NULL
NULL" \
    "USE ${DATABASE}; SELECT FIRST_VALUE(id) RESPECT NULLS OVER (ORDER BY id) FROM t1;"

expect_error \
    "ignore nulls parses but is unsupported" \
    1235 \
    42000 \
    "IGNORE NULLS" \
    "USE ${DATABASE}; SELECT LEAD(id) IGNORE NULLS OVER (ORDER BY id) FROM t1;"

expect_error \
    "row number rejects null treatment syntax" \
    1064 \
    42000 \
    "RESPECT NULLS" \
    "USE ${DATABASE}; SELECT ROW_NUMBER() RESPECT NULLS OVER () FROM t1;"

expect_error \
    "nth value from first remains syntax error" \
    1064 \
    42000 \
    "FROM FIRST" \
    "USE ${DATABASE}; SELECT NTH_VALUE(id, 1 FROM FIRST) OVER (ORDER BY id) FROM t1;"

expect_success \
    "straight join using parses" \
    "USE ${DATABASE}; SELECT shared, t1.id, t2.id FROM t1 STRAIGHT_JOIN t2 USING (shared);"

cleanup

printf '%s\n' "mysql_parser_corpus_query_expression_surfaces_expectations: ok"
