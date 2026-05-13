#!/usr/bin/env sh

set -eu

MYSQL_CONTAINER="${MYLITE_MYSQL_CONTAINER:-mylite-mysql-849}"
DATABASE="mylite_field_function_expectations_$$"

fail() {
    printf '%s\n' "mysql_baseline_field_function_expectations: $1" >&2
    exit 1
}

run_mysql() {
    sql=$1
    shift
    printf '%s\n' "$sql" \
        | docker exec -i "$MYSQL_CONTAINER" mysql --protocol=TCP -h127.0.0.1 -uroot \
            --batch --raw --skip-column-names "$@"
}

run_mysql_with_headers() {
    sql=$1
    shift
    printf '%s\n' "$sql" \
        | docker exec -i "$MYSQL_CONTAINER" mysql --protocol=TCP -h127.0.0.1 -uroot \
            --batch --raw "$@"
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

expect_output_with_headers() {
    label=$1
    expected=$2
    sql=$3
    shift 3

    output=$(run_mysql_with_headers "$sql" "$@")
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

expect_upstream_accepts() {
    label=$1
    sql=$2
    shift 2

    set +e
    output=$(run_mysql "$sql" "$@" 2>&1)
    status_code=$?
    set -e

    if [ "$status_code" -ne 0 ]; then
        fail "$label: expected MySQL to accept deferred behavior, got [$output]"
    fi
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
run_mysql "CREATE DATABASE ${DATABASE} DEFAULT CHARACTER SET utf8mb4 "\
"COLLATE utf8mb4_0900_ai_ci;" >/dev/null

core_expected=$(cat <<EXPECTED
1	2	0	0	2	0	0	2	2	1	0
-1	0
EXPECTED
)
expect_output \
    "core FIELD values" \
    "$core_expected" \
    "DO 0; SELECT FIELD('x','x'), FIELD('x','y','x'), FIELD('x','y'), "\
"FIELD(NULL,'a',NULL), FIELD('a',NULL,'a'), FIELD('a',NULL), FIELD(NULL,NULL), "\
"FIELD(2,1,2,3), FIELD(TRUE,FALSE,TRUE), FIELD('abc','ABC'), @@warning_count; "\
"SELECT ROW_COUNT(), @@warning_count;" \
    "$DATABASE"

labels_expected=$(cat <<EXPECTED
FIELD ('b','a','b')	field_alias
2	2
EXPECTED
)
expect_output_with_headers \
    "FIELD labels and whitespace" \
    "$labels_expected" \
    "SELECT FIELD ('b','a','b'), FIELD('b','a','b') AS field_alias FROM DUAL;" \
    "$DATABASE"

table_expected=$(cat <<EXPECTED
1	1	3	1	1
2	2	1	2	2
3	0	0	0	0
4	3	2	1	0
EXPECTED
)
expect_output \
    "FIELD table rows" \
    "$table_expected" \
    "CREATE TABLE t(id INT, code VARCHAR(20), n INT, c CHAR(4), body TEXT); "\
"INSERT INTO t VALUES "\
"(1,'alpha',18,'Aa','first'), "\
"(2,'Beta',19,'bb','second'), "\
"(3,NULL,NULL,NULL,NULL), "\
"(4,'Other',20,'aa','third'); "\
"SELECT id, FIELD(code,'ALPHA','beta','other'), FIELD(n,19,20,18), "\
"FIELD(c,'aa','bb'), FIELD(body,'first','second') FROM t ORDER BY id;" \
    "$DATABASE"

where_order_limit_expected=$(cat <<EXPECTED
4	1
3	0
EXPECTED
)
expect_output \
    "FIELD preserves row envelope" \
    "$where_order_limit_expected" \
    "SELECT id, FIELD(code,'other','beta') FROM t WHERE id > 1 ORDER BY id DESC LIMIT 2;" \
    "$DATABASE"

do_expected=$(cat <<EXPECTED
0	0
EXPECTED
)
expect_output \
    "FIELD DO status" \
    "$do_expected" \
    "DO FIELD('b','a','b'), FIELD(NULL,'a'); SELECT ROW_COUNT(), @@warning_count;" \
    "$DATABASE"

expect_error \
    "FIELD zero arguments" \
    1582 \
    "42000" \
    "Incorrect parameter count" \
    "SELECT FIELD();" \
    "$DATABASE"

expect_error \
    "FIELD one argument" \
    1582 \
    "42000" \
    "Incorrect parameter count" \
    "SELECT FIELD('x');" \
    "$DATABASE"

expect_upstream_accepts \
    "mixed domain accepted by MySQL but deferred by MyLite" \
    "SELECT FIELD('x', 0, 1); SHOW WARNINGS;" \
    "$DATABASE"

expect_upstream_accepts \
    "binary domain accepted by MySQL but deferred by MyLite" \
    "SELECT FIELD(_binary 'a', _binary 'A', _binary 'a');" \
    "$DATABASE"

expect_upstream_accepts \
    "accent-insensitive collation accepted by MySQL but deferred by MyLite" \
    "SELECT FIELD('e','é');" \
    "$DATABASE"

printf '%s\n' "mysql_baseline_field_function_expectations: ok"
