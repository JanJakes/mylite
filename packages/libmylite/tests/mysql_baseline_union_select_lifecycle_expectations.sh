#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_union_select_expectations_$$"

fail() {
    printf '%s\n' "mysql_baseline_union_select_lifecycle_expectations: $1" >&2
    exit 1
}

run_mysql() {
    sql=$1
    shift
    printf '%s\n' "$sql" \
        | docker exec -i "$MYSQL_CONTAINER" mysql -uroot --batch --raw --skip-column-names "$@"
}

run_mysql_with_names() {
    sql=$1
    shift
    printf '%s\n' "$sql" \
        | docker exec -i "$MYSQL_CONTAINER" mysql -uroot --batch --raw "$@"
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

expect_output_with_names() {
    label=$1
    expected=$2
    sql=$3
    shift 3

    output=$(run_mysql_with_names "$sql" "$@")
    output=$(printf '%s\n' "$output" | tr '\t' '|')
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
        *)
            fail "$label: expected error $code/$state containing [$message], got [$output]"
            ;;
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
run_mysql "CREATE DATABASE ${DATABASE};" >/dev/null

expect_output \
    "scalar union distinct and row count state" \
    "1
2
-1|0" \
    "SELECT 1 UNION SELECT 1 UNION SELECT 2; SELECT ROW_COUNT(), @@warning_count;" \
    "$DATABASE"

expect_output \
    "scalar union all preserves duplicates" \
    "1
1
2
-1|0" \
    "SELECT 1 UNION ALL SELECT 1 UNION ALL SELECT 2; SELECT ROW_COUNT(), @@warning_count;" \
    "$DATABASE"

expect_output \
    "explicit distinct and null duplicate handling" \
    "NULL
1" \
    "SELECT NULL UNION DISTINCT SELECT NULL UNION SELECT 1;" \
    "$DATABASE"

expect_output \
    "mixed all and distinct chain" \
    "1
1" \
    "SELECT 1 UNION ALL SELECT 1 UNION SELECT 1 UNION ALL SELECT 1;" \
    "$DATABASE"

expect_output_with_names \
    "first branch result labels" \
    "a|b
1|2
3|4" \
    "SELECT 1 AS a, 2 AS b UNION SELECT 3 AS c, 4 AS d;" \
    "$DATABASE"

run_mysql \
    "CREATE TABLE t1 (id INT, v VARCHAR(10)); "\
"CREATE TABLE t2 (id INT, v VARCHAR(10)); "\
"INSERT INTO t1 VALUES (1, 'a'), (2, 'b'), (NULL, NULL); "\
"INSERT INTO t2 VALUES (2, 'b'), (3, 'c'), (NULL, NULL);" \
    "$DATABASE" >/dev/null

expect_output_with_names \
    "descriptor-backed table union" \
    "id|v
1|a
2|b
NULL|NULL
3|c" \
    "SELECT id, v FROM t1 UNION SELECT id, v FROM t2;" \
    "$DATABASE"

expect_output_with_names \
    "descriptor-backed table union all" \
    "id|v
1|a
2|b
NULL|NULL
2|b
3|c
NULL|NULL" \
    "SELECT id, v FROM t1 UNION ALL SELECT id, v FROM t2;" \
    "$DATABASE"

expect_error \
    "column count mismatch" \
    1222 \
    21000 \
    "The used SELECT statements have a different number of columns" \
    "SELECT 1 UNION SELECT 1, 2;" \
    "$DATABASE"

expect_error \
    "unparenthesized branch order by is syntax error" \
    1064 \
    42000 \
    "near 'UNION SELECT 2'" \
    "SELECT 1 ORDER BY 1 UNION SELECT 2;" \
    "$DATABASE"

expect_output_with_names \
    "global order by is a broader deferred MySQL surface" \
    "a
1
2" \
    "SELECT 2 AS a UNION SELECT 1 ORDER BY a;" \
    "$DATABASE"

expect_output \
    "global limit is a broader deferred MySQL surface" \
    "1" \
    "SELECT 1 UNION ALL SELECT 2 LIMIT 1;" \
    "$DATABASE"
